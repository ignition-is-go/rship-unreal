// Rship Sequencer Sync Implementation

#include "RshipSequencerSync.h"
#include "RshipSubsystem.h"
#include "RshipTimecodeSync.h"
#include "Logs.h"
#include "Engine/World.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"

void URshipSequencerSync::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

    // Subscribe to timecode events
    if (Subsystem)
    {
        URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
        if (Timecode)
        {
            Timecode->OnTimecodeChanged.AddDynamic(this, &URshipSequencerSync::OnTimecodeChanged);
            Timecode->OnStateChanged.AddDynamic(this, &URshipSequencerSync::OnTimecodeStateChanged);
            Timecode->OnCuePointReached.AddDynamic(this, &URshipSequencerSync::OnCuePointReached);
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("SequencerSync initialized"));
}

void URshipSequencerSync::Shutdown()
{
    // Stop all active sequences
    Stop();

    // Cleanup players
    for (auto& Pair : ActivePlayers)
    {
        CleanupPlayer(Pair.Key);
    }
    ActivePlayers.Empty();
    SequenceActors.Empty();

    // Unsubscribe
    if (Subsystem)
    {
        URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
        if (Timecode)
        {
            Timecode->OnTimecodeChanged.RemoveDynamic(this, &URshipSequencerSync::OnTimecodeChanged);
            Timecode->OnStateChanged.RemoveDynamic(this, &URshipSequencerSync::OnTimecodeStateChanged);
            Timecode->OnCuePointReached.RemoveDynamic(this, &URshipSequencerSync::OnCuePointReached);
        }
    }

    Mappings.Empty();
    Subsystem = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("SequencerSync shutdown"));
}

void URshipSequencerSync::Tick(float DeltaTime)
{
    if (!bSyncEnabled || !Subsystem) return;

    URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
    if (!Timecode) return;

    FRshipTimecodeStatus Status = Timecode->GetStatus();
    int64 CurrentFrame = Status.TotalFrames;

    if (SyncMode == ERshipSequencerSyncMode::FollowTimecode ||
        SyncMode == ERshipSequencerSyncMode::Bidirectional)
    {
        UpdateSequencerFromTimecode(CurrentFrame, DeltaTime);
    }

    if (SyncMode == ERshipSequencerSyncMode::DriveTimecode ||
        SyncMode == ERshipSequencerSyncMode::Bidirectional)
    {
        UpdateTimecodeFromSequencer();
    }

    LastTimecodeFrame = CurrentFrame;
}

void URshipSequencerSync::SetSyncMode(ERshipSequencerSyncMode Mode)
{
    SyncMode = Mode;
}

void URshipSequencerSync::SetSyncBehavior(ERshipSequencerSyncBehavior Behavior)
{
    SyncBehavior = Behavior;
}

void URshipSequencerSync::SetSyncEnabled(bool bEnabled)
{
    bSyncEnabled = bEnabled;
    if (!bEnabled)
    {
        Pause();
    }
}

void URshipSequencerSync::AddSequenceMapping(const FRshipSequenceMapping& Mapping)
{
    // Remove existing with same ID
    RemoveSequenceMapping(Mapping.MappingId);

    FRshipSequenceMapping NewMapping = Mapping;
    if (NewMapping.MappingId.IsEmpty())
    {
        NewMapping.MappingId = FGuid::NewGuid().ToString();
    }

    Mappings.Add(NewMapping);

    UE_LOG(LogRshipExec, Log, TEXT("SequencerSync: Added mapping %s (frames %lld-%lld)"),
        *NewMapping.MappingId, NewMapping.TimecodeStartFrame, NewMapping.TimecodeEndFrame);
}

void URshipSequencerSync::RemoveSequenceMapping(const FString& MappingId)
{
    StopMappingPlayback(MappingId);
    Mappings.RemoveAll([&](const FRshipSequenceMapping& M) { return M.MappingId == MappingId; });
}

bool URshipSequencerSync::GetMapping(const FString& MappingId, FRshipSequenceMapping& OutMapping) const
{
    for (const FRshipSequenceMapping& M : Mappings)
    {
        if (M.MappingId == MappingId)
        {
            OutMapping = M;
            return true;
        }
    }
    return false;
}

void URshipSequencerSync::ClearMappings()
{
    Stop();
    Mappings.Empty();
}

FString URshipSequencerSync::QuickSyncSequence(ULevelSequence* Sequence)
{
    if (!Sequence || !Subsystem) return TEXT("");

    URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
    if (!Timecode) return TEXT("");

    FRshipSequenceMapping Mapping;
    Mapping.MappingId = FGuid::NewGuid().ToString();
    Mapping.Sequence = Sequence;
    Mapping.TimecodeStartFrame = Timecode->GetStatus().TotalFrames;
    Mapping.TimecodeEndFrame = -1; // Use sequence length
    Mapping.bEnabled = true;

    AddSequenceMapping(Mapping);

    return Mapping.MappingId;
}

FString URshipSequencerSync::QuickSyncFromActor(ALevelSequenceActor* SequenceActor)
{
    if (!SequenceActor) return TEXT("");

    ULevelSequence* Sequence = SequenceActor->GetSequence();
    if (!Sequence) return TEXT("");

    FString MappingId = QuickSyncSequence(Sequence);

    // Store the actor reference
    if (!MappingId.IsEmpty())
    {
        SequenceActors.Add(MappingId, SequenceActor);
    }

    return MappingId;
}

void URshipSequencerSync::Play()
{
    bIsPlaying = true;

    if (!Subsystem) return;

    URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
    if (Timecode)
    {
        int64 CurrentFrame = Timecode->GetStatus().TotalFrames;

        for (const FRshipSequenceMapping& Mapping : Mappings)
        {
            if (!Mapping.bEnabled) continue;

            bool bInRange = CurrentFrame >= Mapping.TimecodeStartFrame;
            if (Mapping.TimecodeEndFrame >= 0)
            {
                bInRange = bInRange && CurrentFrame < Mapping.TimecodeEndFrame;
            }

            if (bInRange && !ActivePlayers.Contains(Mapping.MappingId))
            {
                StartMappingPlayback(Mapping, CurrentFrame);
            }
        }
    }
}

void URshipSequencerSync::Pause()
{
    bIsPlaying = false;

    for (auto& Pair : ActivePlayers)
    {
        if (Pair.Value)
        {
            Pair.Value->Pause();
        }
    }
}

void URshipSequencerSync::Stop()
{
    bIsPlaying = false;

    TArray<FString> ToStop;
    ActivePlayers.GetKeys(ToStop);

    for (const FString& MappingId : ToStop)
    {
        StopMappingPlayback(MappingId);
    }
}

void URshipSequencerSync::ScrubToFrame(int64 Frame)
{
    for (const FRshipSequenceMapping& Mapping : Mappings)
    {
        if (!Mapping.bEnabled) continue;

        bool bInRange = Frame >= Mapping.TimecodeStartFrame;
        if (Mapping.TimecodeEndFrame >= 0)
        {
            bInRange = bInRange && Frame < Mapping.TimecodeEndFrame;
        }

        if (bInRange)
        {
            if (!ActivePlayers.Contains(Mapping.MappingId))
            {
                StartMappingPlayback(Mapping, Frame);
            }
            else
            {
                ULevelSequencePlayer* Player = ActivePlayers[Mapping.MappingId];
                if (Player)
                {
                    float SequenceTime = CalculateSequenceTime(Mapping, Frame);
                    Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
                        FFrameTime::FromDecimal(SequenceTime * Player->GetFrameRate().AsDecimal()),
                        EUpdatePositionMethod::Scrub
                    ));
                }
            }
        }
        else if (ActivePlayers.Contains(Mapping.MappingId))
        {
            StopMappingPlayback(Mapping.MappingId);
        }
    }
}

void URshipSequencerSync::ForceSync()
{
    if (!Subsystem) return;

    URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
    if (Timecode)
    {
        ScrubToFrame(Timecode->GetStatus().TotalFrames);
    }
}

TArray<FString> URshipSequencerSync::GetActiveMappings() const
{
    TArray<FString> Result;
    ActivePlayers.GetKeys(Result);
    return Result;
}

bool URshipSequencerSync::IsMappingActive(const FString& MappingId) const
{
    return ActivePlayers.Contains(MappingId);
}

void URshipSequencerSync::UpdateSequencerFromTimecode(int64 CurrentFrame, float DeltaTime)
{
    if (SyncBehavior != ERshipSequencerSyncBehavior::Continuous) return;

    for (const FRshipSequenceMapping& Mapping : Mappings)
    {
        if (!Mapping.bEnabled) continue;

        bool bInRange = CurrentFrame >= Mapping.TimecodeStartFrame;
        if (Mapping.TimecodeEndFrame >= 0)
        {
            bInRange = bInRange && CurrentFrame < Mapping.TimecodeEndFrame;
        }

        if (bInRange)
        {
            if (!ActivePlayers.Contains(Mapping.MappingId))
            {
                StartMappingPlayback(Mapping, CurrentFrame);
            }
            else if (bIsPlaying)
            {
                // Update position if playing
                ULevelSequencePlayer* Player = ActivePlayers[Mapping.MappingId];
                if (Player && Player->IsPlaying())
                {
                    float TargetTime = CalculateSequenceTime(Mapping, CurrentFrame);
                    float CurrentTime = Player->GetCurrentTime().AsSeconds();

                    // Calculate offset
                    CurrentSyncOffsetMs = (CurrentTime - TargetTime) * 1000.0f;

                    // Correct if offset is too large (> 50ms)
                    if (FMath::Abs(CurrentSyncOffsetMs) > 50.0f)
                    {
                        Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
                            FFrameTime::FromDecimal(TargetTime * Player->GetFrameRate().AsDecimal()),
                            EUpdatePositionMethod::Jump
                        ));
                    }
                }
            }
        }
        else if (ActivePlayers.Contains(Mapping.MappingId))
        {
            StopMappingPlayback(Mapping.MappingId);
        }
    }
}

void URshipSequencerSync::UpdateTimecodeFromSequencer()
{
    // Find the primary active player
    for (auto& Pair : ActivePlayers)
    {
        if (Pair.Value && Pair.Value->IsPlaying())
        {
            // This player is driving - would update timecode here
            // For now, just track what we'd send
            break;
        }
    }
}

void URshipSequencerSync::StartMappingPlayback(const FRshipSequenceMapping& Mapping, int64 CurrentFrame)
{
    ULevelSequencePlayer* Player = GetOrCreatePlayer(Mapping);
    if (!Player) return;

    float StartTime = CalculateSequenceTime(Mapping, CurrentFrame);
    Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
        FFrameTime::FromDecimal(StartTime * Player->GetFrameRate().AsDecimal()),
        EUpdatePositionMethod::Jump
    ));

    if (bIsPlaying)
    {
        Player->Play();
    }

    ActivePlayers.Add(Mapping.MappingId, Player);

    ULevelSequence* Sequence = Mapping.Sequence.Get();
    OnSequenceStarted.Broadcast(Mapping.MappingId, Sequence);

    UE_LOG(LogRshipExec, Log, TEXT("SequencerSync: Started mapping %s at time %.2fs"),
        *Mapping.MappingId, StartTime);
}

void URshipSequencerSync::StopMappingPlayback(const FString& MappingId)
{
    ULevelSequencePlayer** PlayerPtr = ActivePlayers.Find(MappingId);
    if (PlayerPtr && *PlayerPtr)
    {
        (*PlayerPtr)->Stop();

        FRshipSequenceMapping Mapping;
        if (GetMapping(MappingId, Mapping))
        {
            ULevelSequence* Sequence = Mapping.Sequence.Get();
            OnSequenceStopped.Broadcast(MappingId, Sequence);
        }
    }

    ActivePlayers.Remove(MappingId);
    CleanupPlayer(MappingId);

    UE_LOG(LogRshipExec, Log, TEXT("SequencerSync: Stopped mapping %s"), *MappingId);
}

float URshipSequencerSync::CalculateSequenceTime(const FRshipSequenceMapping& Mapping, int64 CurrentFrame)
{
    if (!Subsystem) return 0.0f;

    URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
    if (!Timecode) return 0.0f;

    FFrameRate FrameRate = Timecode->GetStatus().FrameRate;

    int64 OffsetFrame = CurrentFrame - Mapping.TimecodeStartFrame + Mapping.SequenceStartOffset;
    float Time = (float)OffsetFrame / FrameRate.AsDecimal() * Mapping.PlaybackRate;

    // Handle looping
    if (Mapping.bLoop && Mapping.Sequence.IsValid())
    {
        ULevelSequence* Sequence = Mapping.Sequence.Get();
        if (Sequence)
        {
            float SequenceLength = Sequence->GetMovieScene()->GetPlaybackRange().Size<FFrameNumber>().Value / FrameRate.AsDecimal();
            if (SequenceLength > 0.0f)
            {
                Time = FMath::Fmod(Time, SequenceLength);
            }
        }
    }

    return FMath::Max(0.0f, Time);
}

ULevelSequencePlayer* URshipSequencerSync::GetOrCreatePlayer(const FRshipSequenceMapping& Mapping)
{
    // Check if we have a sequence actor
    ALevelSequenceActor** ActorPtr = SequenceActors.Find(Mapping.MappingId);
    if (ActorPtr && *ActorPtr)
    {
        return (*ActorPtr)->GetSequencePlayer();
    }

    // Load the sequence
    ULevelSequence* Sequence = Mapping.Sequence.LoadSynchronous();
    if (!Sequence) return nullptr;

    // Get world
    UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
    if (!World) return nullptr;

    // Create player
    FMovieSceneSequencePlaybackSettings Settings;
    Settings.bAutoPlay = false;
    Settings.LoopCount.Value = Mapping.bLoop ? -1 : 0;
    Settings.PlayRate = Mapping.PlaybackRate;

    ALevelSequenceActor* Actor = nullptr;
    ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(
        World, Sequence, Settings, Actor);

    if (Actor)
    {
        SequenceActors.Add(Mapping.MappingId, Actor);
    }

    return Player;
}

void URshipSequencerSync::CleanupPlayer(const FString& MappingId)
{
    ALevelSequenceActor** ActorPtr = SequenceActors.Find(MappingId);
    if (ActorPtr && *ActorPtr && (*ActorPtr)->IsValidLowLevel())
    {
        // Only destroy if we created it
        // (*ActorPtr)->Destroy();
    }
    SequenceActors.Remove(MappingId);
}

void URshipSequencerSync::OnTimecodeChanged(const FRshipTimecodeStatus& Status)
{
    // Position changes are handled in Tick
}

void URshipSequencerSync::OnTimecodeStateChanged(ERshipTimecodeState OldState, ERshipTimecodeState NewState)
{
    if (NewState == ERshipTimecodeState::Playing)
    {
        Play();
    }
    else if (NewState == ERshipTimecodeState::Paused)
    {
        Pause();
    }
    else if (NewState == ERshipTimecodeState::Stopped)
    {
        Stop();
    }
}

void URshipSequencerSync::OnCuePointReached(const FRshipCuePoint& CuePoint)
{
    if (SyncBehavior == ERshipSequencerSyncBehavior::CueOnly)
    {
        ForceSync();
    }
}
