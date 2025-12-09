// Rship Timecode Synchronization
// Synchronizes UE timeline with rship timecode sources for show playback

#pragma once

#include "CoreMinimal.h"
#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"
#include "RshipTimecodeSync.generated.h"

class URshipSubsystem;

// ============================================================================
// TIMECODE TYPES
// ============================================================================

UENUM(BlueprintType)
enum class ERshipTimecodeSource : uint8
{
    Internal        UMETA(DisplayName = "Internal (UE Clock)"),
    Rship           UMETA(DisplayName = "Rship Server"),
    LTC             UMETA(DisplayName = "LTC Audio Input"),
    MTC             UMETA(DisplayName = "MIDI Timecode"),
    ArtNet          UMETA(DisplayName = "Art-Net Timecode"),
    PTP             UMETA(DisplayName = "PTP/IEEE 1588"),
    NTP             UMETA(DisplayName = "NTP Network Time"),
    Manual          UMETA(DisplayName = "Manual/Triggered")
};

UENUM(BlueprintType)
enum class ERshipTimecodeState : uint8
{
    Stopped,
    Playing,
    Paused,
    Seeking,
    Syncing,
    Lost            // Lost sync with source
};

UENUM(BlueprintType)
enum class ERshipPlaybackMode : uint8
{
    Realtime        UMETA(DisplayName = "Realtime (1x)"),
    SlowMotion      UMETA(DisplayName = "Slow Motion"),
    FastForward     UMETA(DisplayName = "Fast Forward"),
    Reverse         UMETA(DisplayName = "Reverse"),
    Stepped         UMETA(DisplayName = "Stepped (Frame-by-Frame)")
};

/**
 * Current timecode status
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipTimecodeStatus
{
    GENERATED_BODY()

    /** Current timecode value */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    FTimecode Timecode;

    /** Frame rate */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    FFrameRate FrameRate = FFrameRate(30, 1);

    /** Total frames since start */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    int64 TotalFrames = 0;

    /** Elapsed time in seconds */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    double ElapsedSeconds = 0.0;

    /** Current state */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    ERshipTimecodeState State = ERshipTimecodeState::Stopped;

    /** Active source */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    ERshipTimecodeSource Source = ERshipTimecodeSource::Internal;

    /** Playback speed multiplier */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    float PlaybackSpeed = 1.0f;

    /** Sync offset from source (ms) - indicates drift */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    float SyncOffsetMs = 0.0f;

    /** Is synchronized with source */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    bool bIsSynchronized = false;

    /** Last sync timestamp */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    double LastSyncTime = 0.0;
};

/**
 * Cue point for triggering events at specific timecodes
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipCuePoint
{
    GENERATED_BODY()

    /** Unique identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Timecode")
    FString Id;

    /** Display name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Timecode")
    FString Name;

    /** Timecode position */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Timecode")
    FTimecode Timecode;

    /** Frame number (alternative to timecode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Timecode")
    int64 FrameNumber = 0;

    /** Pre-roll time in seconds (fire early) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Timecode")
    float PreRollSeconds = 0.0f;

    /** User data for the cue */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Timecode")
    FString UserData;

    /** Color for UI display */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Timecode")
    FLinearColor Color = FLinearColor::Green;

    /** Is this cue enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Timecode")
    bool bEnabled = true;

    /** Has this cue been fired in current playback */
    bool bFired = false;
};

/**
 * Event track definition (from rship)
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipEventTrack
{
    GENERATED_BODY()

    /** Track ID from rship */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    FString Id;

    /** Track name */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    FString Name;

    /** Track color for UI */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    FLinearColor Color = FLinearColor::White;

    /** Duration in frames */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    int64 DurationFrames = 0;

    /** Frame rate */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    FFrameRate FrameRate = FFrameRate(30, 1);

    /** Is looping */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    bool bLooping = false;

    /** Cue points on this track */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Timecode")
    TArray<FRshipCuePoint> CuePoints;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimecodeChanged, const FRshipTimecodeStatus&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTimecodeStateChanged, ERshipTimecodeState, OldState, ERshipTimecodeState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCuePointReached, const FRshipCuePoint&, CuePoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSyncStatusChanged, bool, bIsSynchronized, float, OffsetMs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventTrackLoaded, const FRshipEventTrack&, Track);

// ============================================================================
// TIMECODE SYNC SERVICE
// ============================================================================

/**
 * Manages timecode synchronization between UE and rship.
 * Supports multiple timecode sources and provides cue point triggering.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipTimecodeSync : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize with subsystem reference */
    void Initialize(URshipSubsystem* InSubsystem);

    /** Shutdown and cleanup */
    void Shutdown();

    /** Tick update (called from subsystem) */
    void Tick(float DeltaTime);

    // ========================================================================
    // PLAYBACK CONTROL
    // ========================================================================

    /** Start playback */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void Play();

    /** Pause playback */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void Pause();

    /** Stop playback (resets to start) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void Stop();

    /** Seek to specific timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void SeekToTimecode(FTimecode TargetTimecode);

    /** Seek to specific frame */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void SeekToFrame(int64 FrameNumber);

    /** Seek to specific time in seconds */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void SeekToTime(double Seconds);

    /** Step forward by frames */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void StepForward(int32 Frames = 1);

    /** Step backward by frames */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void StepBackward(int32 Frames = 1);

    /** Set playback speed */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void SetPlaybackSpeed(float Speed);

    // ========================================================================
    // SOURCE CONTROL
    // ========================================================================

    /** Set the active timecode source */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void SetTimecodeSource(ERshipTimecodeSource Source);

    /** Get the active timecode source */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    ERshipTimecodeSource GetTimecodeSource() const { return CurrentStatus.Source; }

    /** Force resync with source */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void ForceResync();

    /** Set frame rate */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void SetFrameRate(FFrameRate NewFrameRate);

    // ========================================================================
    // STATUS
    // ========================================================================

    /** Get current timecode status */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    FRshipTimecodeStatus GetStatus() const { return CurrentStatus; }

    /** Get current timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    FTimecode GetCurrentTimecode() const { return CurrentStatus.Timecode; }

    /** Get current frame number */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    int64 GetCurrentFrame() const { return CurrentStatus.TotalFrames; }

    /** Get elapsed time in seconds */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    double GetElapsedSeconds() const { return CurrentStatus.ElapsedSeconds; }

    /** Is currently playing */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    bool IsPlaying() const { return CurrentStatus.State == ERshipTimecodeState::Playing; }

    /** Is synchronized with source */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    bool IsSynchronized() const { return CurrentStatus.bIsSynchronized; }

    // ========================================================================
    // CUE POINTS
    // ========================================================================

    /** Add a cue point */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void AddCuePoint(const FRshipCuePoint& CuePoint);

    /** Remove a cue point by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void RemoveCuePoint(const FString& CuePointId);

    /** Clear all cue points */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void ClearCuePoints();

    /** Get all cue points */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    TArray<FRshipCuePoint> GetCuePoints() const { return CuePoints; }

    /** Get next cue point from current position */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    bool GetNextCuePoint(FRshipCuePoint& OutCuePoint) const;

    /** Jump to next cue point */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void JumpToNextCue();

    /** Jump to previous cue point */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void JumpToPreviousCue();

    // ========================================================================
    // EVENT TRACKS (from rship)
    // ========================================================================

    /** Load event track from rship */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    void LoadEventTrack(const FString& TrackId);

    /** Get loaded event track */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    FRshipEventTrack GetLoadedTrack() const { return LoadedTrack; }

    /** Is a track loaded */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    bool HasLoadedTrack() const { return !LoadedTrack.Id.IsEmpty(); }

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired every frame with current timecode */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Timecode")
    FOnTimecodeChanged OnTimecodeChanged;

    /** Fired when playback state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Timecode")
    FOnTimecodeStateChanged OnStateChanged;

    /** Fired when a cue point is reached */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Timecode")
    FOnCuePointReached OnCuePointReached;

    /** Fired when sync status changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Timecode")
    FOnSyncStatusChanged OnSyncStatusChanged;

    /** Fired when an event track is loaded */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Timecode")
    FOnEventTrackLoaded OnEventTrackLoaded;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Current status
    FRshipTimecodeStatus CurrentStatus;

    // Cue points
    TArray<FRshipCuePoint> CuePoints;

    // Loaded event track
    FRshipEventTrack LoadedTrack;

    // Internal timing
    double InternalTime = 0.0;
    double LastTickTime = 0.0;
    double SyncReferenceTime = 0.0;

    // Sync tracking
    TArray<float> RecentSyncOffsets;
    int32 MaxSyncSamples = 10;
    float SyncLostThresholdMs = 100.0f;

    // Update methods
    void UpdateInternalTimecode(float DeltaTime);
    void UpdateFromRshipTimecode();
    void CheckCuePoints();
    void UpdateSyncStatus();

    // Conversion helpers
    FTimecode FrameToTimecode(int64 Frame) const;
    int64 TimecodeToFrame(const FTimecode& TC) const;
    double FrameToSeconds(int64 Frame) const;
    int64 SecondsToFrame(double Seconds) const;

    // Process rship timecode message
    void ProcessTimecodeEvent(const TSharedPtr<FJsonObject>& Data);
    void ProcessEventTrackEvent(const TSharedPtr<FJsonObject>& Data);
};
