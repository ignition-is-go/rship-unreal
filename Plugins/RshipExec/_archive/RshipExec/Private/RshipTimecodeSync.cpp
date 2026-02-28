// Rship Timecode Sync Implementation

#include "RshipTimecodeSync.h"
#include "RshipSubsystem.h"
#include "Logs.h"

void URshipTimecodeSync::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    LastTickTime = FPlatformTime::Seconds();
    CurrentStatus.FrameRate = FFrameRate(30, 1);
    CurrentStatus.State = ERshipTimecodeState::Stopped;
    CurrentStatus.Source = ERshipTimecodeSource::Internal;
    CurrentStatus.Mode = ERshipTimecodeMode::Receive;
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync initialized"));
}

void URshipTimecodeSync::Shutdown()
{
    Stop();
    CuePoints.Empty();
    Subsystem = nullptr;
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync shutdown"));
}

void URshipTimecodeSync::Tick(float DeltaTime)
{
    if (CurrentStatus.State != ERshipTimecodeState::Playing) return;

    switch (CurrentStatus.Source)
    {
        case ERshipTimecodeSource::Internal:
            UpdateInternalTimecode(DeltaTime);
            break;
        case ERshipTimecodeSource::Rship:
            UpdateFromRshipTimecode();
            break;
        default:
            UpdateInternalTimecode(DeltaTime);
            break;
    }

    CheckCuePoints();
    UpdateSyncStatus();
    OnTimecodeChanged.Broadcast(CurrentStatus);

    // Publish timecode if in Publish or Bidirectional mode
    if (CurrentStatus.Mode == ERshipTimecodeMode::Publish || CurrentStatus.Mode == ERshipTimecodeMode::Bidirectional)
    {
        TimeSinceLastPublish += DeltaTime;
        float PublishInterval = 1.0f / PublishRateHz;

        // Publish at rate limit, or immediately if frame changed
        if (TimeSinceLastPublish >= PublishInterval || CurrentStatus.TotalFrames != LastPublishedFrame)
        {
            PublishTimecodeToRship();
            TimeSinceLastPublish = 0.0f;
            LastPublishedFrame = CurrentStatus.TotalFrames;
        }
    }
}

void URshipTimecodeSync::Play()
{
    if (CurrentStatus.State == ERshipTimecodeState::Playing) return;
    ERshipTimecodeState OldState = CurrentStatus.State;
    CurrentStatus.State = ERshipTimecodeState::Playing;
    LastTickTime = FPlatformTime::Seconds();
    for (FRshipCuePoint& Cue : CuePoints) Cue.bFired = false;
    OnStateChanged.Broadcast(OldState, CurrentStatus.State);
}

void URshipTimecodeSync::Pause()
{
    if (CurrentStatus.State != ERshipTimecodeState::Playing) return;
    ERshipTimecodeState OldState = CurrentStatus.State;
    CurrentStatus.State = ERshipTimecodeState::Paused;
    OnStateChanged.Broadcast(OldState, CurrentStatus.State);
}

void URshipTimecodeSync::Stop()
{
    ERshipTimecodeState OldState = CurrentStatus.State;
    CurrentStatus.State = ERshipTimecodeState::Stopped;
    CurrentStatus.TotalFrames = 0;
    CurrentStatus.ElapsedSeconds = 0.0;
    CurrentStatus.Timecode = FTimecode(0, 0, 0, 0, false);
    InternalTime = 0.0;
    for (FRshipCuePoint& Cue : CuePoints) Cue.bFired = false;
    OnStateChanged.Broadcast(OldState, CurrentStatus.State);
}

void URshipTimecodeSync::SeekToTimecode(FTimecode TargetTimecode) { SeekToFrame(TimecodeToFrame(TargetTimecode)); }
void URshipTimecodeSync::SeekToTime(double Seconds) { SeekToFrame(SecondsToFrame(Seconds)); }
void URshipTimecodeSync::StepForward(int32 Frames) { SeekToFrame(CurrentStatus.TotalFrames + Frames); }
void URshipTimecodeSync::StepBackward(int32 Frames) { SeekToFrame(CurrentStatus.TotalFrames - Frames); }

void URshipTimecodeSync::SeekToFrame(int64 FrameNumber)
{
    ERshipTimecodeState OldState = CurrentStatus.State;
    CurrentStatus.TotalFrames = FMath::Max(0LL, FrameNumber);
    CurrentStatus.ElapsedSeconds = FrameToSeconds(CurrentStatus.TotalFrames);
    CurrentStatus.Timecode = FrameToTimecode(CurrentStatus.TotalFrames);
    InternalTime = CurrentStatus.ElapsedSeconds;
    for (FRshipCuePoint& Cue : CuePoints) Cue.bFired = (Cue.FrameNumber < CurrentStatus.TotalFrames);
    CurrentStatus.State = (OldState == ERshipTimecodeState::Playing) ? ERshipTimecodeState::Playing : ERshipTimecodeState::Paused;
    OnTimecodeChanged.Broadcast(CurrentStatus);
}

void URshipTimecodeSync::SetPlaybackSpeed(float Speed) { CurrentStatus.PlaybackSpeed = FMath::Clamp(Speed, -10.0f, 10.0f); }
void URshipTimecodeSync::SetTimecodeSource(ERshipTimecodeSource Source) { CurrentStatus.Source = Source; CurrentStatus.bIsSynchronized = false; RecentSyncOffsets.Empty(); }
void URshipTimecodeSync::ForceResync() { CurrentStatus.bIsSynchronized = false; RecentSyncOffsets.Empty(); }
void URshipTimecodeSync::SetFrameRate(FFrameRate NewFrameRate) { CurrentStatus.FrameRate = NewFrameRate; }

void URshipTimecodeSync::AddCuePoint(const FRshipCuePoint& CuePoint)
{
    FRshipCuePoint NewCue = CuePoint;
    if (NewCue.Id.IsEmpty()) NewCue.Id = FGuid::NewGuid().ToString();
    if (NewCue.FrameNumber == 0) NewCue.FrameNumber = TimecodeToFrame(NewCue.Timecode);
    CuePoints.Add(NewCue);
    CuePoints.Sort([](const FRshipCuePoint& A, const FRshipCuePoint& B) { return A.FrameNumber < B.FrameNumber; });
}

void URshipTimecodeSync::RemoveCuePoint(const FString& CuePointId) { CuePoints.RemoveAll([&](const FRshipCuePoint& C) { return C.Id == CuePointId; }); }
void URshipTimecodeSync::ClearCuePoints() { CuePoints.Empty(); }

bool URshipTimecodeSync::GetNextCuePoint(FRshipCuePoint& OutCuePoint) const
{
    for (const FRshipCuePoint& Cue : CuePoints)
        if (Cue.bEnabled && !Cue.bFired && Cue.FrameNumber > CurrentStatus.TotalFrames) { OutCuePoint = Cue; return true; }
    return false;
}

void URshipTimecodeSync::JumpToNextCue() { FRshipCuePoint Next; if (GetNextCuePoint(Next)) SeekToFrame(Next.FrameNumber); }
void URshipTimecodeSync::JumpToPreviousCue() { for (int i = CuePoints.Num()-1; i >= 0; i--) if (CuePoints[i].FrameNumber < CurrentStatus.TotalFrames) { SeekToFrame(CuePoints[i].FrameNumber); return; } }
void URshipTimecodeSync::LoadEventTrack(const FString& TrackId) { UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: Load track %s"), *TrackId); }

void URshipTimecodeSync::UpdateInternalTimecode(float DeltaTime)
{
    InternalTime += DeltaTime * CurrentStatus.PlaybackSpeed;
    if (InternalTime < 0.0) InternalTime = 0.0;
    CurrentStatus.ElapsedSeconds = InternalTime;
    CurrentStatus.TotalFrames = SecondsToFrame(InternalTime);
    CurrentStatus.Timecode = FrameToTimecode(CurrentStatus.TotalFrames);
    CurrentStatus.bIsSynchronized = true;
}

void URshipTimecodeSync::UpdateFromRshipTimecode()
{
    double Now = FPlatformTime::Seconds();
    float DeltaTime = (float)(Now - LastTickTime);
    LastTickTime = Now;
    if (!CurrentStatus.bIsSynchronized) UpdateInternalTimecode(DeltaTime);
}

void URshipTimecodeSync::CheckCuePoints()
{
    for (FRshipCuePoint& Cue : CuePoints)
    {
        if (!Cue.bEnabled || Cue.bFired) continue;
        int64 TriggerFrame = Cue.FrameNumber - SecondsToFrame(Cue.PreRollSeconds);
        if (CurrentStatus.TotalFrames >= TriggerFrame) { Cue.bFired = true; OnCuePointReached.Broadcast(Cue); }
    }
}

void URshipTimecodeSync::UpdateSyncStatus()
{
    if (CurrentStatus.Source == ERshipTimecodeSource::Internal) { CurrentStatus.bIsSynchronized = true; CurrentStatus.SyncOffsetMs = 0.0f; return; }
    if (RecentSyncOffsets.Num() > 0)
    {
        float Sum = 0.0f; for (float O : RecentSyncOffsets) Sum += O;
        CurrentStatus.SyncOffsetMs = Sum / RecentSyncOffsets.Num();
        bool bWasSynced = CurrentStatus.bIsSynchronized;
        CurrentStatus.bIsSynchronized = FMath::Abs(CurrentStatus.SyncOffsetMs) < SyncLostThresholdMs;
        if (bWasSynced != CurrentStatus.bIsSynchronized) OnSyncStatusChanged.Broadcast(CurrentStatus.bIsSynchronized, CurrentStatus.SyncOffsetMs);
    }
}

FTimecode URshipTimecodeSync::FrameToTimecode(int64 Frame) const
{
    double FR = CurrentStatus.FrameRate.AsDecimal();
    int32 Total = (int32)(Frame / FR), Frames = Frame % (int32)FR;
    return FTimecode(Total/3600, (Total%3600)/60, Total%60, Frames, false);
}

int64 URshipTimecodeSync::TimecodeToFrame(const FTimecode& TC) const { return (int64)((TC.Hours*3600 + TC.Minutes*60 + TC.Seconds) * CurrentStatus.FrameRate.AsDecimal()) + TC.Frames; }
double URshipTimecodeSync::FrameToSeconds(int64 Frame) const { return (double)Frame / CurrentStatus.FrameRate.AsDecimal(); }
int64 URshipTimecodeSync::SecondsToFrame(double Seconds) const { return (int64)(Seconds * CurrentStatus.FrameRate.AsDecimal()); }

void URshipTimecodeSync::ProcessTimecodeEvent(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid()) return;
    int32 H=0,M=0,S=0,F=0;
    Data->TryGetNumberField(TEXT("hours"),H); Data->TryGetNumberField(TEXT("minutes"),M);
    Data->TryGetNumberField(TEXT("seconds"),S); Data->TryGetNumberField(TEXT("frames"),F);
    FTimecode TC(H,M,S,F,false);
    int64 RecvFrame = TimecodeToFrame(TC);
    float OffsetMs = (float)(RecvFrame - CurrentStatus.TotalFrames) / CurrentStatus.FrameRate.AsDecimal() * 1000.0f;
    RecentSyncOffsets.Add(OffsetMs);
    if (RecentSyncOffsets.Num() > MaxSyncSamples) RecentSyncOffsets.RemoveAt(0);
    CurrentStatus.TotalFrames = RecvFrame; CurrentStatus.Timecode = TC;
    CurrentStatus.ElapsedSeconds = FrameToSeconds(RecvFrame);
    CurrentStatus.LastSyncTime = FPlatformTime::Seconds();
    InternalTime = CurrentStatus.ElapsedSeconds;
}

void URshipTimecodeSync::ProcessEventTrackEvent(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid()) return;
    LoadedTrack.Id = Data->GetStringField(TEXT("id"));
    LoadedTrack.Name = Data->GetStringField(TEXT("name"));
    Data->TryGetNumberField(TEXT("durationFrames"), LoadedTrack.DurationFrames);
    const TArray<TSharedPtr<FJsonValue>>* Cues;
    if (Data->TryGetArrayField(TEXT("cuePoints"), Cues))
        for (const auto& V : *Cues) { auto O = V->AsObject(); if (O.IsValid()) { FRshipCuePoint C; C.Id = O->GetStringField(TEXT("id")); C.Name = O->GetStringField(TEXT("name")); O->TryGetNumberField(TEXT("frameNumber"), C.FrameNumber); AddCuePoint(C); } }
    OnEventTrackLoaded.Broadcast(LoadedTrack);
}

// ============================================================================
// MODE CONTROL (BIDIRECTIONAL)
// ============================================================================

void URshipTimecodeSync::SetTimecodeMode(ERshipTimecodeMode NewMode)
{
    if (CurrentStatus.Mode == NewMode) return;

    ERshipTimecodeMode OldMode = CurrentStatus.Mode;
    CurrentStatus.Mode = NewMode;

    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: Mode changed from %d to %d"),
           static_cast<int32>(OldMode), static_cast<int32>(NewMode));

    // Reset publish state when entering publish mode
    if (NewMode == ERshipTimecodeMode::Publish || NewMode == ERshipTimecodeMode::Bidirectional)
    {
        TimeSinceLastPublish = 0.0f;
        LastPublishedFrame = -1;

        // Immediately publish current state
        PublishTimecodeToRship();
    }
}

void URshipTimecodeSync::SetTimecodeEmitterId(const FString& NewEmitterId)
{
    TimecodeEmitterId = NewEmitterId;
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: Emitter ID set to %s"), *TimecodeEmitterId);
}

void URshipTimecodeSync::PublishTimecodeToRship()
{
    if (!Subsystem) return;

    // Build the timecode pulse data
    TSharedPtr<FJsonObject> PulseData = MakeShared<FJsonObject>();

    // Timecode fields
    PulseData->SetNumberField(TEXT("hours"), CurrentStatus.Timecode.Hours);
    PulseData->SetNumberField(TEXT("minutes"), CurrentStatus.Timecode.Minutes);
    PulseData->SetNumberField(TEXT("seconds"), CurrentStatus.Timecode.Seconds);
    PulseData->SetNumberField(TEXT("frames"), CurrentStatus.Timecode.Frames);

    // Additional status fields
    PulseData->SetNumberField(TEXT("totalFrames"), CurrentStatus.TotalFrames);
    PulseData->SetNumberField(TEXT("elapsedSeconds"), CurrentStatus.ElapsedSeconds);
    PulseData->SetNumberField(TEXT("playbackSpeed"), CurrentStatus.PlaybackSpeed);
    PulseData->SetStringField(TEXT("state"), StaticEnum<ERshipTimecodeState>()->GetNameStringByValue(static_cast<int64>(CurrentStatus.State)));

    // Frame rate info
    PulseData->SetNumberField(TEXT("frameRateNumerator"), CurrentStatus.FrameRate.Numerator);
    PulseData->SetNumberField(TEXT("frameRateDenominator"), CurrentStatus.FrameRate.Denominator);

    // Publish via subsystem
    Subsystem->PulseEmitter(TimecodeTargetId, TimecodeEmitterId, PulseData);
}

// ============================================================================
// RSHIP ACTIONS
// ============================================================================

void URshipTimecodeSync::RS_SetTimecode(int32 Hours, int32 Minutes, int32 Seconds, int32 Frames)
{
    FTimecode TC(Hours, Minutes, Seconds, Frames, false);
    SeekToTimecode(TC);
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: RS_SetTimecode %02d:%02d:%02d:%02d"), Hours, Minutes, Seconds, Frames);
}

void URshipTimecodeSync::RS_Play()
{
    Play();
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: RS_Play"));
}

void URshipTimecodeSync::RS_Pause()
{
    Pause();
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: RS_Pause"));
}

void URshipTimecodeSync::RS_Stop()
{
    Stop();
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: RS_Stop"));
}

void URshipTimecodeSync::RS_SeekToFrame(int64 Frame)
{
    SeekToFrame(Frame);
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: RS_SeekToFrame %lld"), Frame);
}

void URshipTimecodeSync::RS_SetPlaybackSpeed(float Speed)
{
    SetPlaybackSpeed(Speed);
    UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync: RS_SetPlaybackSpeed %.2f"), Speed);
}
