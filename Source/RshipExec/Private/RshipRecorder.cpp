// Rship Recorder Implementation

#include "RshipRecorder.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRshipRecorder, Log, All);

// ============================================================================
// RECORDER SERVICE
// ============================================================================

void URshipRecorder::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    State = ERshipRecorderState::Idle;
    RecordingStartTime = 0.0;
    PlaybackTime = 0.0;
    PlaybackEventIndex = 0;

    UE_LOG(LogRshipRecorder, Log, TEXT("RshipRecorder initialized"));
}

void URshipRecorder::Shutdown()
{
    if (State == ERshipRecorderState::Recording)
    {
        StopRecording();
    }
    if (State == ERshipRecorderState::Playing || State == ERshipRecorderState::Paused)
    {
        StopPlayback();
    }

    UE_LOG(LogRshipRecorder, Log, TEXT("RshipRecorder shut down"));
}

void URshipRecorder::Tick(float DeltaTime)
{
    if (State == ERshipRecorderState::Playing)
    {
        ProcessPlayback(DeltaTime);
    }
}

// ============================================================================
// RECORDING
// ============================================================================

void URshipRecorder::StartRecording(const FString& RecordingName, const FRshipRecordingFilter& Filter)
{
    if (State == ERshipRecorderState::Recording)
    {
        UE_LOG(LogRshipRecorder, Warning, TEXT("Already recording"));
        return;
    }

    // Stop playback if playing
    if (State == ERshipRecorderState::Playing || State == ERshipRecorderState::Paused)
    {
        StopPlayback();
    }

    // Initialize recording
    CurrentRecording = FRshipRecording();
    CurrentRecording.Metadata.Name = RecordingName;
    CurrentRecording.Metadata.CreatedAt = FDateTime::Now();
    CurrentRecording.Metadata.FrameRate = 60.0f;

    CurrentFilter = Filter;
    RecordingStartTime = FPlatformTime::Seconds();

    // Bind to pulse receiver
    BindToRecording();

    State = ERshipRecorderState::Recording;
    OnRecordingStarted.Broadcast();

    UE_LOG(LogRshipRecorder, Log, TEXT("Started recording: %s"), *RecordingName);
}

FRshipRecording URshipRecorder::StopRecording()
{
    if (State != ERshipRecorderState::Recording)
    {
        return FRshipRecording();
    }

    UnbindFromRecording();

    // Finalize metadata
    double EndTime = FPlatformTime::Seconds();
    CurrentRecording.Metadata.Duration = EndTime - RecordingStartTime;
    CurrentRecording.Metadata.EventCount = CurrentRecording.Events.Num();

    // Collect unique emitter IDs
    TSet<FString> UniqueEmitters;
    for (const FRshipRecordedPulse& Pulse : CurrentRecording.Events)
    {
        UniqueEmitters.Add(Pulse.EmitterId);
    }
    CurrentRecording.Metadata.EmitterIds = UniqueEmitters.Array();

    State = ERshipRecorderState::Idle;

    FRshipRecording Result = CurrentRecording;
    OnRecordingStopped.Broadcast(Result);

    UE_LOG(LogRshipRecorder, Log, TEXT("Stopped recording: %s (%.2fs, %d events)"),
        *CurrentRecording.Metadata.Name,
        CurrentRecording.Metadata.Duration,
        CurrentRecording.Metadata.EventCount);

    return Result;
}

double URshipRecorder::GetRecordingDuration() const
{
    if (State == ERshipRecorderState::Recording)
    {
        return FPlatformTime::Seconds() - RecordingStartTime;
    }
    return CurrentRecording.Metadata.Duration;
}

void URshipRecorder::BindToRecording()
{
    if (!Subsystem) return;

    URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
    if (!Receiver) return;

    RecordingPulseHandle = Receiver->OnPulseReceived.AddUObject(this, &URshipRecorder::OnPulseReceived);
}

void URshipRecorder::UnbindFromRecording()
{
    if (!Subsystem) return;

    URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
    if (Receiver && RecordingPulseHandle.IsValid())
    {
        Receiver->OnPulseReceived.Remove(RecordingPulseHandle);
        RecordingPulseHandle.Reset();
    }
}

void URshipRecorder::OnPulseReceived(const FString& EmitterId, TSharedPtr<FJsonObject> Data)
{
    if (State != ERshipRecorderState::Recording) return;

    // Check filter
    if (!MatchesFilter(EmitterId)) return;

    // Create recorded event
    FRshipRecordedPulse Pulse;
    Pulse.TimeOffset = FPlatformTime::Seconds() - RecordingStartTime;
    Pulse.EmitterId = EmitterId;

    // Serialize data to JSON string
    FString DataJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&DataJson);
    if (Data.IsValid() && FJsonSerializer::Serialize(Data.ToSharedRef(), Writer))
    {
        Pulse.DataJson = DataJson;
    }

    CurrentRecording.Events.Add(Pulse);
}

bool URshipRecorder::MatchesFilter(const FString& EmitterId) const
{
    // Check excludes first
    for (const FString& Pattern : CurrentFilter.ExcludePatterns)
    {
        if (MatchesPattern(EmitterId, Pattern))
        {
            return false;
        }
    }

    // If no includes specified, allow all
    if (CurrentFilter.IncludePatterns.Num() == 0)
    {
        return true;
    }

    // Check includes
    for (const FString& Pattern : CurrentFilter.IncludePatterns)
    {
        if (MatchesPattern(EmitterId, Pattern))
        {
            return true;
        }
    }

    return false;
}

bool URshipRecorder::MatchesPattern(const FString& EmitterId, const FString& Pattern) const
{
    if (Pattern == TEXT("*")) return true;

    if (Pattern.Contains(TEXT("*")))
    {
        FString Prefix = Pattern.Replace(TEXT("*"), TEXT(""));
        return EmitterId.StartsWith(Prefix);
    }

    return EmitterId == Pattern;
}

// ============================================================================
// PLAYBACK
// ============================================================================

void URshipRecorder::StartPlayback(const FRshipRecording& Recording, const FRshipPlaybackOptions& Options)
{
    if (State == ERshipRecorderState::Recording)
    {
        UE_LOG(LogRshipRecorder, Warning, TEXT("Cannot start playback while recording"));
        return;
    }

    if (Recording.Events.Num() == 0)
    {
        UE_LOG(LogRshipRecorder, Warning, TEXT("Recording has no events"));
        return;
    }

    // Stop current playback if any
    if (State == ERshipRecorderState::Playing || State == ERshipRecorderState::Paused)
    {
        StopPlayback();
    }

    PlaybackRecording = Recording;
    PlaybackOptions = Options;
    PlaybackTime = Options.StartOffset;
    PlaybackEventIndex = 0;

    // Find starting event index
    for (int32 i = 0; i < PlaybackRecording.Events.Num(); i++)
    {
        if (PlaybackRecording.Events[i].TimeOffset >= PlaybackTime)
        {
            PlaybackEventIndex = i;
            break;
        }
    }

    // Parse all event data for fast playback
    for (FRshipRecordedPulse& Pulse : PlaybackRecording.Events)
    {
        if (!Pulse.DataJson.IsEmpty() && !Pulse.ParsedData.IsValid())
        {
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Pulse.DataJson);
            TSharedPtr<FJsonObject> ParsedObj;
            if (FJsonSerializer::Deserialize(Reader, ParsedObj))
            {
                Pulse.ParsedData = ParsedObj;
            }
        }
    }

    State = ERshipRecorderState::Playing;
    OnPlaybackStarted.Broadcast();

    UE_LOG(LogRshipRecorder, Log, TEXT("Started playback: %s (%.2fs)"),
        *PlaybackRecording.Metadata.Name,
        PlaybackRecording.Metadata.Duration);
}

void URshipRecorder::StopPlayback()
{
    if (State != ERshipRecorderState::Playing && State != ERshipRecorderState::Paused)
    {
        return;
    }

    State = ERshipRecorderState::Idle;
    PlaybackTime = 0.0;
    PlaybackEventIndex = 0;

    OnPlaybackStopped.Broadcast();

    UE_LOG(LogRshipRecorder, Log, TEXT("Stopped playback"));
}

void URshipRecorder::PausePlayback()
{
    if (State != ERshipRecorderState::Playing)
    {
        return;
    }

    State = ERshipRecorderState::Paused;
    UE_LOG(LogRshipRecorder, Log, TEXT("Paused playback at %.2fs"), PlaybackTime);
}

void URshipRecorder::ResumePlayback()
{
    if (State != ERshipRecorderState::Paused)
    {
        return;
    }

    State = ERshipRecorderState::Playing;
    UE_LOG(LogRshipRecorder, Log, TEXT("Resumed playback"));
}

float URshipRecorder::GetPlaybackProgress() const
{
    if (PlaybackRecording.Metadata.Duration <= 0.0)
    {
        return 0.0f;
    }

    double EndTime = PlaybackOptions.EndTime > 0.0 ? PlaybackOptions.EndTime : PlaybackRecording.Metadata.Duration;
    double StartTime = PlaybackOptions.StartOffset;
    double Range = EndTime - StartTime;

    if (Range <= 0.0) return 0.0f;

    return (float)FMath::Clamp((PlaybackTime - StartTime) / Range, 0.0, 1.0);
}

void URshipRecorder::SeekTo(double Time)
{
    PlaybackTime = FMath::Max(0.0, Time);

    // Find event index for this time
    PlaybackEventIndex = 0;
    for (int32 i = 0; i < PlaybackRecording.Events.Num(); i++)
    {
        if (PlaybackRecording.Events[i].TimeOffset >= PlaybackTime)
        {
            PlaybackEventIndex = i;
            break;
        }
    }

    UE_LOG(LogRshipRecorder, Verbose, TEXT("Seeked to %.2fs (event %d)"), PlaybackTime, PlaybackEventIndex);
}

void URshipRecorder::SetPlaybackSpeed(float Speed)
{
    PlaybackOptions.Speed = FMath::Clamp(Speed, 0.1f, 10.0f);
}

void URshipRecorder::ProcessPlayback(float DeltaTime)
{
    if (PlaybackRecording.Events.Num() == 0) return;

    // Advance time
    PlaybackTime += DeltaTime * PlaybackOptions.Speed;

    double EndTime = PlaybackOptions.EndTime > 0.0 ? PlaybackOptions.EndTime : PlaybackRecording.Metadata.Duration;

    // Process events up to current time
    while (PlaybackEventIndex < PlaybackRecording.Events.Num())
    {
        const FRshipRecordedPulse& Pulse = PlaybackRecording.Events[PlaybackEventIndex];

        if (Pulse.TimeOffset > PlaybackTime)
        {
            break;  // Not yet time for this event
        }

        // Emit this pulse
        EmitPlaybackPulse(Pulse);
        PlaybackEventIndex++;
    }

    // Check for end
    if (PlaybackTime >= EndTime)
    {
        if (PlaybackOptions.bLoop)
        {
            PlaybackTime = PlaybackOptions.StartOffset;
            PlaybackEventIndex = 0;

            // Find starting event index
            for (int32 i = 0; i < PlaybackRecording.Events.Num(); i++)
            {
                if (PlaybackRecording.Events[i].TimeOffset >= PlaybackTime)
                {
                    PlaybackEventIndex = i;
                    break;
                }
            }

            OnPlaybackLooped.Broadcast();
            UE_LOG(LogRshipRecorder, Log, TEXT("Playback looped"));
        }
        else
        {
            StopPlayback();
        }
    }
}

void URshipRecorder::EmitPlaybackPulse(const FRshipRecordedPulse& Pulse)
{
    // Fire local event
    if (PlaybackOptions.bFireLocalEvents)
    {
        OnPlaybackPulse.Broadcast(Pulse.EmitterId, Pulse.DataJson, Pulse.TimeOffset);
    }

    // Emit to rship
    if (PlaybackOptions.bEmitToRship && Subsystem)
    {
        URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
        if (Receiver && Pulse.ParsedData.IsValid())
        {
            // Directly process the pulse as if it came from the server
            Receiver->ProcessPulseEvent(Pulse.EmitterId, Pulse.ParsedData);
        }
    }
}

// ============================================================================
// STORAGE
// ============================================================================

FString URshipRecorder::GetDefaultRecordingsPath() const
{
    return FPaths::ProjectSavedDir() / TEXT("RshipRecordings");
}

bool URshipRecorder::SaveRecording(const FRshipRecording& Recording, const FString& FilePath)
{
    // Create JSON object
    TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();

    // Metadata
    TSharedRef<FJsonObject> MetaObj = MakeShared<FJsonObject>();
    MetaObj->SetStringField(TEXT("name"), Recording.Metadata.Name);
    MetaObj->SetStringField(TEXT("description"), Recording.Metadata.Description);
    MetaObj->SetNumberField(TEXT("duration"), Recording.Metadata.Duration);
    MetaObj->SetNumberField(TEXT("eventCount"), Recording.Metadata.EventCount);
    MetaObj->SetNumberField(TEXT("frameRate"), Recording.Metadata.FrameRate);
    MetaObj->SetStringField(TEXT("createdAt"), Recording.Metadata.CreatedAt.ToString());

    TArray<TSharedPtr<FJsonValue>> EmitterArray;
    for (const FString& EmitterId : Recording.Metadata.EmitterIds)
    {
        EmitterArray.Add(MakeShared<FJsonValueString>(EmitterId));
    }
    MetaObj->SetArrayField(TEXT("emitterIds"), EmitterArray);

    RootObj->SetObjectField(TEXT("metadata"), MetaObj);

    // Events
    TArray<TSharedPtr<FJsonValue>> EventsArray;
    for (const FRshipRecordedPulse& Pulse : Recording.Events)
    {
        TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
        EventObj->SetNumberField(TEXT("t"), Pulse.TimeOffset);
        EventObj->SetStringField(TEXT("e"), Pulse.EmitterId);
        EventObj->SetStringField(TEXT("d"), Pulse.DataJson);
        EventsArray.Add(MakeShared<FJsonValueObject>(EventObj));
    }
    RootObj->SetArrayField(TEXT("events"), EventsArray);

    // Serialize
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (!FJsonSerializer::Serialize(RootObj, Writer))
    {
        UE_LOG(LogRshipRecorder, Error, TEXT("Failed to serialize recording"));
        return false;
    }

    // Ensure directory exists
    FString Directory = FPaths::GetPath(FilePath);
    if (!Directory.IsEmpty())
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        PlatformFile.CreateDirectoryTree(*Directory);
    }

    // Write to file
    if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
    {
        UE_LOG(LogRshipRecorder, Error, TEXT("Failed to save recording to: %s"), *FilePath);
        return false;
    }

    UE_LOG(LogRshipRecorder, Log, TEXT("Saved recording: %s"), *FilePath);
    return true;
}

bool URshipRecorder::LoadRecording(const FString& FilePath, FRshipRecording& OutRecording)
{
    // Read file
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogRshipRecorder, Error, TEXT("Failed to load recording from: %s"), *FilePath);
        return false;
    }

    // Parse JSON
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    TSharedPtr<FJsonObject> RootObj;
    if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
    {
        UE_LOG(LogRshipRecorder, Error, TEXT("Failed to parse recording JSON"));
        return false;
    }

    OutRecording = FRshipRecording();

    // Load metadata
    TSharedPtr<FJsonObject> MetaObj = RootObj->GetObjectField(TEXT("metadata"));
    if (MetaObj.IsValid())
    {
        OutRecording.Metadata.Name = MetaObj->GetStringField(TEXT("name"));
        OutRecording.Metadata.Description = MetaObj->GetStringField(TEXT("description"));
        OutRecording.Metadata.Duration = MetaObj->GetNumberField(TEXT("duration"));
        OutRecording.Metadata.EventCount = MetaObj->GetIntegerField(TEXT("eventCount"));
        OutRecording.Metadata.FrameRate = MetaObj->GetNumberField(TEXT("frameRate"));

        FDateTime::Parse(MetaObj->GetStringField(TEXT("createdAt")), OutRecording.Metadata.CreatedAt);

        const TArray<TSharedPtr<FJsonValue>>* EmitterArray;
        if (MetaObj->TryGetArrayField(TEXT("emitterIds"), EmitterArray))
        {
            for (const TSharedPtr<FJsonValue>& Val : *EmitterArray)
            {
                OutRecording.Metadata.EmitterIds.Add(Val->AsString());
            }
        }
    }

    // Load events
    const TArray<TSharedPtr<FJsonValue>>* EventsArray;
    if (RootObj->TryGetArrayField(TEXT("events"), EventsArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *EventsArray)
        {
            TSharedPtr<FJsonObject> EventObj = Val->AsObject();
            if (EventObj.IsValid())
            {
                FRshipRecordedPulse Pulse;
                Pulse.TimeOffset = EventObj->GetNumberField(TEXT("t"));
                Pulse.EmitterId = EventObj->GetStringField(TEXT("e"));
                Pulse.DataJson = EventObj->GetStringField(TEXT("d"));
                OutRecording.Events.Add(Pulse);
            }
        }
    }

    UE_LOG(LogRshipRecorder, Log, TEXT("Loaded recording: %s (%d events)"), *FilePath, OutRecording.Events.Num());
    return true;
}

TArray<FString> URshipRecorder::GetSavedRecordings()
{
    TArray<FString> Result;

    FString RecordingsPath = GetDefaultRecordingsPath();
    IFileManager& FileManager = IFileManager::Get();

    TArray<FString> Files;
    FileManager.FindFiles(Files, *(RecordingsPath / TEXT("*.json")), true, false);

    for (const FString& File : Files)
    {
        Result.Add(RecordingsPath / File);
    }

    return Result;
}

bool URshipRecorder::DeleteRecording(const FString& FilePath)
{
    IFileManager& FileManager = IFileManager::Get();

    if (FileManager.Delete(*FilePath))
    {
        UE_LOG(LogRshipRecorder, Log, TEXT("Deleted recording: %s"), *FilePath);
        return true;
    }

    UE_LOG(LogRshipRecorder, Error, TEXT("Failed to delete recording: %s"), *FilePath);
    return false;
}
