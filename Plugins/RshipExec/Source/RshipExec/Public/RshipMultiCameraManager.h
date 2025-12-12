// Rship Multi-Camera Manager
// Manages multiple camera views for virtual production and previsualization

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "RshipMultiCameraManager.generated.h"

class URshipSubsystem;
class URshipCameraManager;

// ============================================================================
// CAMERA VIEW TYPES
// ============================================================================

UENUM(BlueprintType)
enum class ERshipCameraViewType : uint8
{
    MainOutput      UMETA(DisplayName = "Main Output (Program)"),
    Preview         UMETA(DisplayName = "Preview"),
    Aux             UMETA(DisplayName = "Auxiliary"),
    Recording       UMETA(DisplayName = "Recording"),
    VR              UMETA(DisplayName = "VR/XR"),
    Debug           UMETA(DisplayName = "Debug View")
};

UENUM(BlueprintType)
enum class ERshipTransitionType : uint8
{
    Cut             UMETA(DisplayName = "Cut (Instant)"),
    Dissolve        UMETA(DisplayName = "Dissolve"),
    Fade            UMETA(DisplayName = "Fade through Black"),
    Wipe            UMETA(DisplayName = "Wipe"),
    Push            UMETA(DisplayName = "Push"),
    Slide           UMETA(DisplayName = "Slide")
};

UENUM(BlueprintType)
enum class ERshipCameraTallyState : uint8
{
    Off             UMETA(DisplayName = "Off"),
    Preview         UMETA(DisplayName = "Preview (Green)"),
    Program         UMETA(DisplayName = "Program (Red)"),
    Recording       UMETA(DisplayName = "Recording")
};

/**
 * Camera view configuration
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipCameraView
{
    GENERATED_BODY()

    /** View identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString Id;

    /** Display name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString Name;

    /** View type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    ERshipCameraViewType Type = ERshipCameraViewType::MainOutput;

    /** Camera actor for this view */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    ACameraActor* Camera = nullptr;

    /** Camera ID in rship (for calibration sync) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString RshipCameraId;

    /** Current tally state */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|MultiCamera")
    ERshipCameraTallyState TallyState = ERshipCameraTallyState::Off;

    /** Is this view enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    bool bEnabled = true;

    /** Priority (for auto-switching) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    int32 Priority = 0;
};

/**
 * Transition configuration
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipCameraTransition
{
    GENERATED_BODY()

    /** Transition type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    ERshipTransitionType Type = ERshipTransitionType::Cut;

    /** Duration in seconds (0 for cut) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    float Duration = 0.0f;

    /** Easing curve name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString EasingCurve = TEXT("Linear");

    /** Direction for wipe/push/slide (0-360 degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    float Direction = 0.0f;
};

/**
 * Camera switch preset
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipCameraPreset
{
    GENERATED_BODY()

    /** Preset identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString Id;

    /** Display name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString Name;

    /** Camera view ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString ViewId;

    /** Transition to use */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FRshipCameraTransition Transition;

    /** Keyboard shortcut */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FKey Shortcut;

    /** MIDI note (for control surface) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    int32 MIDINote = -1;

    /** Color for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FLinearColor Color = FLinearColor::White;
};

/**
 * Auto-switching rule
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipAutoSwitchRule
{
    GENERATED_BODY()

    /** Rule identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString Id;

    /** Display name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString Name;

    /** Is rule enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    bool bEnabled = true;

    /** Trigger condition type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString TriggerType;  // "TimeBased", "EmitterValue", "CuePoint", etc.

    /** Trigger parameters (JSON-like) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString TriggerParams;

    /** Camera view to switch to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FString TargetViewId;

    /** Transition to use */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    FRshipCameraTransition Transition;

    /** Priority (higher = evaluated first) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|MultiCamera")
    int32 Priority = 0;
};

/**
 * Recording session info
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipRecordingSession
{
    GENERATED_BODY()

    /** Session identifier */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|MultiCamera")
    FString Id;

    /** Start time */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|MultiCamera")
    FDateTime StartTime;

    /** Recording views (camera IDs being recorded) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|MultiCamera")
    TArray<FString> RecordingViews;

    /** Output path */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|MultiCamera")
    FString OutputPath;

    /** Is recording */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|MultiCamera")
    bool bIsRecording = false;

    /** Recording duration in seconds */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|MultiCamera")
    float DurationSeconds = 0.0f;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCameraSwitched, const FString&, FromViewId, const FString&, ToViewId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransitionStarted, const FRshipCameraTransition&, Transition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTransitionCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTallyStateChanged, const FString&, ViewId, ERshipCameraTallyState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRecordingStateChanged, bool, bIsRecording);

// ============================================================================
// MULTI-CAMERA MANAGER
// ============================================================================

/**
 * Manages multiple camera views for virtual production and live switching.
 * Supports transitions, tally state, auto-switching, and recording.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipMultiCameraManager : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize with subsystem reference */
    void Initialize(URshipSubsystem* InSubsystem);

    /** Shutdown and cleanup */
    void Shutdown();

    /** Tick update for transitions */
    void Tick(float DeltaTime);

    // ========================================================================
    // VIEW MANAGEMENT
    // ========================================================================

    /** Add a camera view */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void AddView(const FRshipCameraView& View);

    /** Remove a camera view */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void RemoveView(const FString& ViewId);

    /** Get all views */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    TArray<FRshipCameraView> GetAllViews() const;

    /** Get view by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    bool GetView(const FString& ViewId, FRshipCameraView& OutView) const;

    /** Update view configuration */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void UpdateView(const FRshipCameraView& View);

    /** Get current program view */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    FRshipCameraView GetProgramView() const { return ProgramView; }

    /** Get current preview view */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    FRshipCameraView GetPreviewView() const { return PreviewView; }

    // ========================================================================
    // SWITCHING
    // ========================================================================

    /** Switch to view (cut) */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void SwitchToView(const FString& ViewId);

    /** Switch with transition */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void SwitchWithTransition(const FString& ViewId, const FRshipCameraTransition& Transition);

    /** Set preview view */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void SetPreviewView(const FString& ViewId);

    /** Execute transition from preview to program */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void Take(const FRshipCameraTransition& Transition);

    /** Quick cut from preview to program */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void Cut();

    /** Auto transition (uses default transition) */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void Auto();

    /** Fade to black */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void FadeToBlack(float Duration = 1.0f);

    /** Fade from black */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void FadeFromBlack(float Duration = 1.0f);

    /** Is transition in progress */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    bool IsTransitioning() const { return bIsTransitioning; }

    // ========================================================================
    // PRESETS
    // ========================================================================

    /** Add camera preset */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void AddPreset(const FRshipCameraPreset& Preset);

    /** Remove preset */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void RemovePreset(const FString& PresetId);

    /** Get all presets */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    TArray<FRshipCameraPreset> GetPresets() const { return Presets; }

    /** Execute preset */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void ExecutePreset(const FString& PresetId);

    // ========================================================================
    // AUTO-SWITCHING
    // ========================================================================

    /** Enable/disable auto-switching */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void SetAutoSwitchEnabled(bool bEnabled);

    /** Is auto-switching enabled */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    bool IsAutoSwitchEnabled() const { return bAutoSwitchEnabled; }

    /** Add auto-switch rule */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void AddAutoSwitchRule(const FRshipAutoSwitchRule& Rule);

    /** Remove auto-switch rule */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void RemoveAutoSwitchRule(const FString& RuleId);

    /** Get all auto-switch rules */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    TArray<FRshipAutoSwitchRule> GetAutoSwitchRules() const { return AutoSwitchRules; }

    // ========================================================================
    // TALLY
    // ========================================================================

    /** Set tally state for a view */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void SetTallyState(const FString& ViewId, ERshipCameraTallyState State);

    /** Get tally state for a view */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    ERshipCameraTallyState GetTallyState(const FString& ViewId) const;

    /** Enable/disable tally output (to physical tally lights) */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void SetTallyOutputEnabled(bool bEnabled);

    // ========================================================================
    // RECORDING
    // ========================================================================

    /** Start recording views */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void StartRecording(const TArray<FString>& ViewIds, const FString& OutputPath);

    /** Stop recording */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void StopRecording();

    /** Is recording */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    bool IsRecording() const { return CurrentRecording.bIsRecording; }

    /** Get current recording session */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    FRshipRecordingSession GetCurrentRecording() const { return CurrentRecording; }

    // ========================================================================
    // DEFAULT TRANSITION
    // ========================================================================

    /** Set default transition */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    void SetDefaultTransition(const FRshipCameraTransition& Transition);

    /** Get default transition */
    UFUNCTION(BlueprintCallable, Category = "Rship|MultiCamera")
    FRshipCameraTransition GetDefaultTransition() const { return DefaultTransition; }

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when camera is switched */
    UPROPERTY(BlueprintAssignable, Category = "Rship|MultiCamera")
    FOnCameraSwitched OnCameraSwitched;

    /** Fired when transition starts */
    UPROPERTY(BlueprintAssignable, Category = "Rship|MultiCamera")
    FOnTransitionStarted OnTransitionStarted;

    /** Fired when transition completes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|MultiCamera")
    FOnTransitionCompleted OnTransitionCompleted;

    /** Fired when tally state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|MultiCamera")
    FOnTallyStateChanged OnTallyStateChanged;

    /** Fired when recording state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|MultiCamera")
    FOnRecordingStateChanged OnRecordingStateChanged;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Views
    UPROPERTY()
    TMap<FString, FRshipCameraView> Views;

    FRshipCameraView ProgramView;
    FRshipCameraView PreviewView;

    // Presets
    TArray<FRshipCameraPreset> Presets;

    // Auto-switching
    bool bAutoSwitchEnabled = false;
    TArray<FRshipAutoSwitchRule> AutoSwitchRules;

    // Transition state
    bool bIsTransitioning = false;
    FRshipCameraTransition ActiveTransition;
    float TransitionProgress = 0.0f;
    FString TransitionFromViewId;
    FString TransitionToViewId;

    // Default transition
    FRshipCameraTransition DefaultTransition;

    // Tally
    bool bTallyOutputEnabled = true;

    // Recording
    FRshipRecordingSession CurrentRecording;

    // Transition methods
    void UpdateTransition(float DeltaTime);
    void CompleteTransition();
    void ApplyTransitionBlend(float Alpha);

    // Tally update
    void UpdateTallyStates();
    void SendTallyToRship(const FString& ViewId, ERshipCameraTallyState State);

    // Auto-switch evaluation
    void EvaluateAutoSwitchRules();

    // Sync with rship
    void ProcessCameraSwitchCommand(const TSharedPtr<FJsonObject>& Data);
};
