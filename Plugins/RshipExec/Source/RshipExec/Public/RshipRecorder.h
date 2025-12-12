// Rship Recorder
// Record and playback pulse data for previz and rehearsal

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RshipRecorder.generated.h"

class URshipSubsystem;
class URshipPulseReceiver;

// ============================================================================
// RECORDING DATA STRUCTURES
// ============================================================================

/** Single recorded pulse event */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipRecordedPulse
{
    GENERATED_BODY()

    /** Time offset from recording start (seconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Rship|Recording")
    double TimeOffset = 0.0;

    /** Emitter ID */
    UPROPERTY(BlueprintReadWrite, Category = "Rship|Recording")
    FString EmitterId;

    /** JSON data as string (compact storage) */
    UPROPERTY(BlueprintReadWrite, Category = "Rship|Recording")
    FString DataJson;

    /** Parsed data for fast playback */
    TSharedPtr<FJsonObject> ParsedData;
};

/** Recording metadata */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipRecordingMetadata
{
    GENERATED_BODY()

    /** Recording name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    FString Name;

    /** Description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    FString Description;

    /** Recording duration (seconds) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Recording")
    double Duration = 0.0;

    /** Number of recorded events */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Recording")
    int32 EventCount = 0;

    /** Unique emitter IDs in recording */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Recording")
    TArray<FString> EmitterIds;

    /** Recording creation time */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Recording")
    FDateTime CreatedAt;

    /** Frame rate used during recording */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Recording")
    float FrameRate = 60.0f;
};

/** Full recording data */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipRecording
{
    GENERATED_BODY()

    /** Recording metadata */
    UPROPERTY(BlueprintReadWrite, Category = "Rship|Recording")
    FRshipRecordingMetadata Metadata;

    /** Recorded pulse events */
    UPROPERTY(BlueprintReadWrite, Category = "Rship|Recording")
    TArray<FRshipRecordedPulse> Events;
};

/** Recording filter options */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipRecordingFilter
{
    GENERATED_BODY()

    /** Only record these emitter ID patterns (empty = all) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    TArray<FString> IncludePatterns;

    /** Exclude these emitter ID patterns */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    TArray<FString> ExcludePatterns;

    /** Maximum events per second (0 = unlimited) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording", meta = (ClampMin = "0", ClampMax = "1000"))
    int32 MaxEventsPerSecond = 0;
};

/** Playback options */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipPlaybackOptions
{
    GENERATED_BODY()

    /** Playback speed (1.0 = normal) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float Speed = 1.0f;

    /** Loop playback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    bool bLoop = false;

    /** Start time offset (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    double StartOffset = 0.0;

    /** End time (0 = full duration) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    double EndTime = 0.0;

    /** Emit pulses to rship during playback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    bool bEmitToRship = true;

    /** Fire local events during playback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Recording")
    bool bFireLocalEvents = true;
};

/** Recording state */
UENUM(BlueprintType)
enum class ERshipRecorderState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Recording       UMETA(DisplayName = "Recording"),
    Playing         UMETA(DisplayName = "Playing"),
    Paused          UMETA(DisplayName = "Paused")
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRecordingStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRecordingStopped, const FRshipRecording&, Recording);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaybackStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaybackStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaybackLooped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlaybackPulse, const FString&, EmitterId, const FString&, DataJson, double, Time);

// ============================================================================
// RECORDER SERVICE
// ============================================================================

/**
 * Service for recording and playing back rship pulse data.
 * Useful for previz, rehearsal, and debugging.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipRecorder : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    // ========================================================================
    // RECORDING
    // ========================================================================

    /** Start recording */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    void StartRecording(const FString& RecordingName = TEXT("Recording"), const FRshipRecordingFilter& Filter = FRshipRecordingFilter());

    /** Stop recording and get the recording data */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    FRshipRecording StopRecording();

    /** Is currently recording */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    bool IsRecording() const { return State == ERshipRecorderState::Recording; }

    /** Get current recording duration */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    double GetRecordingDuration() const;

    /** Get number of recorded events */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    int32 GetRecordedEventCount() const { return CurrentRecording.Events.Num(); }

    // ========================================================================
    // PLAYBACK
    // ========================================================================

    /** Start playback of a recording */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    void StartPlayback(const FRshipRecording& Recording, const FRshipPlaybackOptions& Options = FRshipPlaybackOptions());

    /** Stop playback */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    void StopPlayback();

    /** Pause playback */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    void PausePlayback();

    /** Resume playback */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    void ResumePlayback();

    /** Is currently playing */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    bool IsPlaying() const { return State == ERshipRecorderState::Playing; }

    /** Is playback paused */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    bool IsPaused() const { return State == ERshipRecorderState::Paused; }

    /** Get current playback time */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    double GetPlaybackTime() const { return PlaybackTime; }

    /** Get playback progress (0-1) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    float GetPlaybackProgress() const;

    /** Seek to specific time */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    void SeekTo(double Time);

    /** Set playback speed */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    void SetPlaybackSpeed(float Speed);

    // ========================================================================
    // STORAGE
    // ========================================================================

    /** Save recording to file */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    bool SaveRecording(const FRshipRecording& Recording, const FString& FilePath);

    /** Load recording from file */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    bool LoadRecording(const FString& FilePath, FRshipRecording& OutRecording);

    /** Get list of saved recordings in default directory */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    TArray<FString> GetSavedRecordings();

    /** Delete a saved recording */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    bool DeleteRecording(const FString& FilePath);

    // ========================================================================
    // STATE
    // ========================================================================

    /** Get current state */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    ERshipRecorderState GetState() const { return State; }

    /** Get the current recording (while recording) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    FRshipRecording GetCurrentRecording() const { return CurrentRecording; }

    /** Get the loaded playback recording */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Recording")
    FRshipRecording GetPlaybackRecording() const { return PlaybackRecording; }

    // ========================================================================
    // EVENTS
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Rship|Recording")
    FOnRecordingStarted OnRecordingStarted;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Recording")
    FOnRecordingStopped OnRecordingStopped;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Recording")
    FOnPlaybackStarted OnPlaybackStarted;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Recording")
    FOnPlaybackStopped OnPlaybackStopped;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Recording")
    FOnPlaybackLooped OnPlaybackLooped;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Recording")
    FOnPlaybackPulse OnPlaybackPulse;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    ERshipRecorderState State = ERshipRecorderState::Idle;

    // Recording state
    FRshipRecording CurrentRecording;
    FRshipRecordingFilter CurrentFilter;
    double RecordingStartTime;
    FDelegateHandle RecordingPulseHandle;

    // Playback state
    FRshipRecording PlaybackRecording;
    FRshipPlaybackOptions PlaybackOptions;
    double PlaybackTime;
    int32 PlaybackEventIndex;

    void BindToRecording();
    void UnbindFromRecording();
    void OnPulseReceived(const FString& EmitterId, TSharedPtr<FJsonObject> Data);

    bool MatchesFilter(const FString& EmitterId) const;
    bool MatchesPattern(const FString& EmitterId, const FString& Pattern) const;

    void ProcessPlayback(float DeltaTime);
    void EmitPlaybackPulse(const FRshipRecordedPulse& Pulse);

    FString GetDefaultRecordingsPath() const;
};
