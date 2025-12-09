// Rship Sequencer Sync
// Synchronize UE Sequencer playback with rship timecode

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "RshipTimecodeSync.h"
#include "RshipSequencerSync.generated.h"

class URshipSubsystem;
class ALevelSequenceActor;

// ============================================================================
// SYNC MODES
// ============================================================================

/** How the sequencer relates to rship timecode */
UENUM(BlueprintType)
enum class ERshipSequencerSyncMode : uint8
{
    Disabled        UMETA(DisplayName = "Disabled"),          // No sync
    FollowTimecode  UMETA(DisplayName = "Follow Timecode"),   // Sequencer follows rship timecode
    DriveTimecode   UMETA(DisplayName = "Drive Timecode"),    // Sequencer drives rship timecode (master)
    Bidirectional   UMETA(DisplayName = "Bidirectional")      // Whichever moves, the other follows
};

/** Sync behavior options */
UENUM(BlueprintType)
enum class ERshipSequencerSyncBehavior : uint8
{
    Continuous      UMETA(DisplayName = "Continuous"),        // Always match position
    CueOnly         UMETA(DisplayName = "Cue Points Only"),   // Only sync on cue points
    ManualTrigger   UMETA(DisplayName = "Manual Trigger")     // Only sync when triggered
};

// ============================================================================
// SEQUENCE MAPPING
// ============================================================================

/** Map a timecode range to a level sequence */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipSequenceMapping
{
    GENERATED_BODY()

    /** Unique ID for this mapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    FString MappingId;

    /** The level sequence asset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    TSoftObjectPtr<ULevelSequence> Sequence;

    /** Timecode start (in frames) - when this sequence should start */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    int64 TimecodeStartFrame = 0;

    /** Timecode end (in frames) - when this sequence should end (-1 = use sequence length) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    int64 TimecodeEndFrame = -1;

    /** Sequence start offset (skip this many frames into the sequence) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    int32 SequenceStartOffset = 0;

    /** Playback rate multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    float PlaybackRate = 1.0f;

    /** Whether to loop the sequence within its timecode range */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    bool bLoop = false;

    /** Whether this mapping is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    bool bEnabled = true;

    /** Tags for grouping/filtering */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Sequencer")
    TArray<FString> Tags;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSequenceSyncStarted, const FString&, MappingId, ULevelSequence*, Sequence);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSequenceSyncStopped, const FString&, MappingId, ULevelSequence*, Sequence);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSequenceSyncPositionChanged, const FString&, MappingId, float, SequenceTime, int64, TimecodeFrame);

// ============================================================================
// SEQUENCER SYNC SERVICE
// ============================================================================

/**
 * Service for synchronizing UE Sequencer with rship timecode.
 * Maps timecode ranges to level sequences and keeps them in sync.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipSequencerSync : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    // ========================================================================
    // SYNC MODE
    // ========================================================================

    /** Set the sync mode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void SetSyncMode(ERshipSequencerSyncMode Mode);

    /** Get current sync mode */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Sequencer")
    ERshipSequencerSyncMode GetSyncMode() const { return SyncMode; }

    /** Set sync behavior */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void SetSyncBehavior(ERshipSequencerSyncBehavior Behavior);

    /** Enable/disable sync */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void SetSyncEnabled(bool bEnabled);

    /** Is sync currently enabled */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Sequencer")
    bool IsSyncEnabled() const { return bSyncEnabled; }

    /** Is currently playing */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Sequencer")
    bool IsPlaying() const { return bIsPlaying; }

    // ========================================================================
    // SEQUENCE MAPPINGS
    // ========================================================================

    /** Add a sequence mapping */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void AddSequenceMapping(const FRshipSequenceMapping& Mapping);

    /** Remove a sequence mapping by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void RemoveSequenceMapping(const FString& MappingId);

    /** Get all sequence mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    TArray<FRshipSequenceMapping> GetAllMappings() const { return Mappings; }

    /** Get a mapping by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    bool GetMapping(const FString& MappingId, FRshipSequenceMapping& OutMapping) const;

    /** Clear all mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void ClearMappings();

    // ========================================================================
    // QUICK SETUP
    // ========================================================================

    /** Quick setup: sync a single sequence starting at current timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    FString QuickSyncSequence(ULevelSequence* Sequence);

    /** Quick setup: sync a sequence from a level sequence actor */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    FString QuickSyncFromActor(ALevelSequenceActor* SequenceActor);

    // ========================================================================
    // PLAYBACK CONTROL
    // ========================================================================

    /** Play all active sequences from current timecode position */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void Play();

    /** Pause all active sequences */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void Pause();

    /** Stop all sequences and return to start */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void Stop();

    /** Scrub to a specific timecode frame */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void ScrubToFrame(int64 Frame);

    /** Force sync all sequences to current timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    void ForceSync();

    // ========================================================================
    // STATE
    // ========================================================================

    /** Get currently active mapping IDs */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    TArray<FString> GetActiveMappings() const;

    /** Check if a specific mapping is currently active */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    bool IsMappingActive(const FString& MappingId) const;

    /** Get sync offset in milliseconds (positive = sequencer ahead) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Sequencer")
    float GetSyncOffsetMs() const { return CurrentSyncOffsetMs; }

    // ========================================================================
    // EVENTS
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Rship|Sequencer")
    FOnSequenceSyncStarted OnSequenceStarted;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Sequencer")
    FOnSequenceSyncStopped OnSequenceStopped;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Sequencer")
    FOnSequenceSyncPositionChanged OnPositionChanged;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    TArray<FRshipSequenceMapping> Mappings;

    // Active sequence players (MappingId -> Player)
    TMap<FString, ULevelSequencePlayer*> ActivePlayers;
    TMap<FString, ALevelSequenceActor*> SequenceActors;

    ERshipSequencerSyncMode SyncMode = ERshipSequencerSyncMode::FollowTimecode;
    ERshipSequencerSyncBehavior SyncBehavior = ERshipSequencerSyncBehavior::Continuous;
    bool bSyncEnabled = true;
    float CurrentSyncOffsetMs = 0.0f;
    int64 LastTimecodeFrame = -1;
    bool bIsPlaying = false;

    void UpdateSequencerFromTimecode(int64 CurrentFrame, float DeltaTime);
    void UpdateTimecodeFromSequencer();
    void StartMappingPlayback(const FRshipSequenceMapping& Mapping, int64 CurrentFrame);
    void StopMappingPlayback(const FString& MappingId);
    float CalculateSequenceTime(const FRshipSequenceMapping& Mapping, int64 CurrentFrame);
    ULevelSequencePlayer* GetOrCreatePlayer(const FRshipSequenceMapping& Mapping);
    void CleanupPlayer(const FString& MappingId);

    FDelegateHandle TimecodeChangedHandle;
    FDelegateHandle TimecodeStateHandle;
    FDelegateHandle CuePointHandle;

    void OnTimecodeChanged(const FRshipTimecodeStatus& Status);
    void OnTimecodeStateChanged(ERshipTimecodeState OldState, ERshipTimecodeState NewState);
    void OnCuePointReached(const FRshipCuePoint& CuePoint);
};
