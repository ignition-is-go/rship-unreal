// Rship Audio Reactive Component
// Analyze audio and generate pulse data for beat detection and frequency response

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundSubmix.h"
#include "DSP/FFTAlgorithm.h"
#include "RshipAudioReactive.generated.h"

class URshipSubsystem;
class USoundSubmix;

// ============================================================================
// AUDIO ANALYSIS TYPES
// ============================================================================

/** Frequency band configuration */
UENUM(BlueprintType)
enum class ERshipFrequencyBand : uint8
{
    SubBass     UMETA(DisplayName = "Sub-Bass (20-60Hz)"),
    Bass        UMETA(DisplayName = "Bass (60-250Hz)"),
    LowMid      UMETA(DisplayName = "Low-Mid (250-500Hz)"),
    Mid         UMETA(DisplayName = "Mid (500-2kHz)"),
    HighMid     UMETA(DisplayName = "High-Mid (2-4kHz)"),
    High        UMETA(DisplayName = "High (4-6kHz)"),
    Presence    UMETA(DisplayName = "Presence (6-20kHz)"),
    Custom      UMETA(DisplayName = "Custom Range")
};

/** Beat detection mode */
UENUM(BlueprintType)
enum class ERshipBeatDetectionMode : uint8
{
    Energy          UMETA(DisplayName = "Energy-Based"),        // Detect energy spikes
    Spectral        UMETA(DisplayName = "Spectral Flux"),       // Detect spectral changes
    Combined        UMETA(DisplayName = "Combined"),            // Both methods
    BPMTracking     UMETA(DisplayName = "BPM Tracking")         // Track consistent tempo
};

/** Output mode for audio data */
UENUM(BlueprintType)
enum class ERshipAudioOutputMode : uint8
{
    Emitter         UMETA(DisplayName = "Emit to Rship"),       // Push as emitter data
    Local           UMETA(DisplayName = "Local Events"),        // Fire local delegates
    Both            UMETA(DisplayName = "Both")                 // Both modes
};

// ============================================================================
// ANALYSIS RESULTS
// ============================================================================

/** Real-time audio analysis results */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipAudioAnalysis
{
    GENERATED_BODY()

    /** Overall audio level (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    float Level = 0.0f;

    /** Peak level (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    float Peak = 0.0f;

    /** RMS level (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    float RMS = 0.0f;

    /** Frequency band values (0-1 each) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    TArray<float> Bands;

    /** Beat detected this frame */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    bool bBeatDetected = false;

    /** Estimated BPM */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    float EstimatedBPM = 0.0f;

    /** Beat confidence (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    float BeatConfidence = 0.0f;

    /** Time since last beat (seconds) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    float TimeSinceLastBeat = 0.0f;

    /** Spectral centroid (brightness indicator) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    float SpectralCentroid = 0.0f;

    /** Spectral flatness (noise vs tonal) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Audio")
    float SpectralFlatness = 0.0f;
};

/** Frequency band definition */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFrequencyBandDef
{
    GENERATED_BODY()

    /** Band type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    ERshipFrequencyBand Band = ERshipFrequencyBand::Bass;

    /** Custom minimum frequency (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio", meta = (EditCondition = "Band == ERshipFrequencyBand::Custom"))
    float CustomMinHz = 20.0f;

    /** Custom maximum frequency (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio", meta = (EditCondition = "Band == ERshipFrequencyBand::Custom"))
    float CustomMaxHz = 200.0f;

    /** Smoothing factor (0 = instant, 0.99 = very slow) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float Smoothing = 0.5f;

    /** Gain multiplier for this band */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float Gain = 1.0f;

    /** Emitter field name to output to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    FString OutputField;

    // Runtime state
    float CurrentValue = 0.0f;
    float TargetValue = 0.0f;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioBeat, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioAnalysis, const FRshipAudioAnalysis&, Analysis);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioBandUpdate, int32, BandIndex, float, Value);

// ============================================================================
// AUDIO REACTIVE COMPONENT
// ============================================================================

/**
 * Component for audio-reactive behavior.
 * Analyzes audio input and generates pulse data for rship integration.
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Audio Reactive"))
class RSHIPEXEC_API URshipAudioReactive : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipAudioReactive();

    // UActorComponent interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** Submix to analyze (null = master) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    USoundSubmix* SubmixToAnalyze;

    /** Target ID for emitter output */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    FString TargetId;

    /** Emitter ID for output */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    FString EmitterId = TEXT("audio");

    /** Output mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    ERshipAudioOutputMode OutputMode = ERshipAudioOutputMode::Both;

    /** Beat detection mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    ERshipBeatDetectionMode BeatMode = ERshipBeatDetectionMode::Combined;

    /** FFT size (power of 2, higher = more frequency resolution but more latency) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio", meta = (ClampMin = "256", ClampMax = "8192"))
    int32 FFTSize = 1024;

    /** Analysis rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio", meta = (ClampMin = "10", ClampMax = "120"))
    float AnalysisRate = 60.0f;

    /** Frequency bands to analyze */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    TArray<FRshipFrequencyBandDef> FrequencyBands;

    // ========================================================================
    // BEAT DETECTION SETTINGS
    // ========================================================================

    /** Beat detection threshold (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Beat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BeatThreshold = 0.5f;

    /** Minimum time between beats (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Beat", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float MinBeatInterval = 0.1f;

    /** Beat decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Beat", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float BeatDecay = 2.0f;

    /** Use only bass frequencies for beat detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Beat")
    bool bUseBassForBeats = true;

    // ========================================================================
    // LEVEL SETTINGS
    // ========================================================================

    /** Input gain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Level", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float InputGain = 1.0f;

    /** Level smoothing (0 = instant, 0.99 = very slow) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Level", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float LevelSmoothing = 0.7f;

    /** Peak hold time (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Level", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float PeakHoldTime = 0.5f;

    /** Noise floor threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Level", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float NoiseFloor = 0.01f;

    // ========================================================================
    // OUTPUT CONTROL
    // ========================================================================

    /** Enable analysis and output */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio")
    bool bEnabled = true;

    /** Emit level data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Output")
    bool bEmitLevel = true;

    /** Emit beat data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Output")
    bool bEmitBeat = true;

    /** Emit frequency band data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Audio|Output")
    bool bEmitBands = true;

    // ========================================================================
    // RUNTIME API
    // ========================================================================

    /** Get current analysis results */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Audio")
    FRshipAudioAnalysis GetAnalysis() const { return CurrentAnalysis; }

    /** Get a specific frequency band value */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Audio")
    float GetBandValue(int32 BandIndex) const;

    /** Get current level */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Audio")
    float GetLevel() const { return CurrentAnalysis.Level; }

    /** Get whether beat was detected this frame */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Audio")
    bool WasBeatDetected() const { return CurrentAnalysis.bBeatDetected; }

    /** Get estimated BPM */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Audio")
    float GetBPM() const { return CurrentAnalysis.EstimatedBPM; }

    /** Manually trigger a beat (for testing or external sync) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Audio")
    void TriggerBeat(float Intensity = 1.0f);

    /** Set up default frequency bands */
    UFUNCTION(BlueprintCallable, Category = "Rship|Audio")
    void SetupDefaultBands();

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when a beat is detected */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Audio")
    FOnAudioBeat OnBeatDetected;

    /** Fired each analysis frame */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Audio")
    FOnAudioAnalysis OnAnalysisUpdate;

    /** Fired when a frequency band value changes significantly */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Audio")
    FOnAudioBandUpdate OnBandUpdate;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    FRshipAudioAnalysis CurrentAnalysis;

    // Audio analysis state
    TArray<float> AudioBuffer;
    TArray<float> FFTMagnitudes;
    TArray<float> EnergyHistory;
    TArray<double> BeatTimes;

    float AnalysisTimer;
    float PeakHoldTimer;
    float CurrentPeak;
    double LastBeatTime;
    float BeatEnergy;

    // Submix analysis
    FDelegateHandle SubmixAnalysisHandle;

    void SetupSubmixAnalysis();
    void CleanupSubmixAnalysis();
    void OnSubmixEnvelope(const TArray<float>& Envelope);

    void ProcessAudioData(const float* Data, int32 NumSamples, int32 NumChannels);
    void PerformFFT();
    void AnalyzeFrequencyBands();
    void DetectBeat();
    void UpdateBPMEstimate();
    void ApplySmoothing(float DeltaTime);
    void EmitToRship();

    float GetBandEnergy(float MinHz, float MaxHz);
    void GetBandFrequencyRange(ERshipFrequencyBand Band, float& OutMinHz, float& OutMaxHz);
};

// ============================================================================
// AUDIO REACTIVE MANAGER
// ============================================================================

/**
 * Manager for coordinating multiple audio reactive components.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipAudioManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();

    /** Get global audio level across all active components */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Audio")
    float GetGlobalLevel() const;

    /** Check if any beat was detected this frame */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Audio")
    bool WasAnyBeatDetected() const;

    /** Get estimated global BPM */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Audio")
    float GetGlobalBPM() const;

    void RegisterComponent(URshipAudioReactive* Component);
    void UnregisterComponent(URshipAudioReactive* Component);

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    TArray<URshipAudioReactive*> ActiveComponents;
};
