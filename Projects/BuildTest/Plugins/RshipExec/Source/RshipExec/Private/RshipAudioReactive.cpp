// Rship Audio Reactive Implementation

#include "RshipAudioReactive.h"
#include "RshipSubsystem.h"
#include "Engine/Engine.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Sound/SoundSubmix.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRshipAudio, Log, All);

// ============================================================================
// AUDIO REACTIVE COMPONENT
// ============================================================================

URshipAudioReactive::URshipAudioReactive()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Default frequency bands
    SetupDefaultBands();
}

void URshipAudioReactive::BeginPlay()
{
    Super::BeginPlay();

    // Get subsystem
    if (GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    }

    // Initialize buffers
    AudioBuffer.SetNumZeroed(FFTSize);
    FFTMagnitudes.SetNumZeroed(FFTSize / 2);
    EnergyHistory.SetNumZeroed(32);  // ~0.5 seconds at 60Hz

    CurrentAnalysis.Bands.SetNum(FrequencyBands.Num());

    AnalysisTimer = 0.0f;
    PeakHoldTimer = 0.0f;
    CurrentPeak = 0.0f;
    LastBeatTime = 0.0;
    BeatEnergy = 0.0f;

    // Setup submix analysis
    SetupSubmixAnalysis();

    UE_LOG(LogRshipAudio, Log, TEXT("Audio Reactive component started on %s"), *GetOwner()->GetName());
}

void URshipAudioReactive::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CleanupSubmixAnalysis();
    Super::EndPlay(EndPlayReason);
}

void URshipAudioReactive::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnabled) return;

    // Update analysis at configured rate
    AnalysisTimer += DeltaTime;
    float AnalysisInterval = 1.0f / AnalysisRate;

    if (AnalysisTimer >= AnalysisInterval)
    {
        AnalysisTimer = 0.0f;

        // Perform FFT and analysis
        PerformFFT();
        AnalyzeFrequencyBands();
        DetectBeat();
        UpdateBPMEstimate();
    }

    // Apply smoothing every frame
    ApplySmoothing(DeltaTime);

    // Update peak hold
    PeakHoldTimer += DeltaTime;
    if (PeakHoldTimer > PeakHoldTime)
    {
        CurrentPeak *= 0.95f;  // Slow decay
    }

    // Emit to rship
    if (OutputMode == ERshipAudioOutputMode::Emitter || OutputMode == ERshipAudioOutputMode::Both)
    {
        EmitToRship();
    }

    // Fire events
    if (OutputMode == ERshipAudioOutputMode::Local || OutputMode == ERshipAudioOutputMode::Both)
    {
        OnAnalysisUpdate.Broadcast(CurrentAnalysis);

        if (CurrentAnalysis.bBeatDetected)
        {
            OnBeatDetected.Broadcast(BeatEnergy);
        }
    }

    // Update time since last beat
    CurrentAnalysis.TimeSinceLastBeat = (float)(FPlatformTime::Seconds() - LastBeatTime);
    CurrentAnalysis.bBeatDetected = false;  // Reset for next frame
}

void URshipAudioReactive::SetupDefaultBands()
{
    FrequencyBands.Empty();

    // Sub-bass
    FRshipFrequencyBandDef SubBass;
    SubBass.Band = ERshipFrequencyBand::SubBass;
    SubBass.Smoothing = 0.6f;
    SubBass.OutputField = TEXT("subBass");
    FrequencyBands.Add(SubBass);

    // Bass
    FRshipFrequencyBandDef Bass;
    Bass.Band = ERshipFrequencyBand::Bass;
    Bass.Smoothing = 0.5f;
    Bass.OutputField = TEXT("bass");
    FrequencyBands.Add(Bass);

    // Low-mid
    FRshipFrequencyBandDef LowMid;
    LowMid.Band = ERshipFrequencyBand::LowMid;
    LowMid.Smoothing = 0.4f;
    LowMid.OutputField = TEXT("lowMid");
    FrequencyBands.Add(LowMid);

    // Mid
    FRshipFrequencyBandDef Mid;
    Mid.Band = ERshipFrequencyBand::Mid;
    Mid.Smoothing = 0.3f;
    Mid.OutputField = TEXT("mid");
    FrequencyBands.Add(Mid);

    // High-mid
    FRshipFrequencyBandDef HighMid;
    HighMid.Band = ERshipFrequencyBand::HighMid;
    HighMid.Smoothing = 0.3f;
    HighMid.OutputField = TEXT("highMid");
    FrequencyBands.Add(HighMid);

    // High
    FRshipFrequencyBandDef High;
    High.Band = ERshipFrequencyBand::High;
    High.Smoothing = 0.2f;
    High.OutputField = TEXT("high");
    FrequencyBands.Add(High);
}

void URshipAudioReactive::SetupSubmixAnalysis()
{
    // Get world for audio device
    UWorld* World = GetWorld();
    if (!World) return;

    // Note: Submix real-time analysis requires AudioSynesthesia plugin
    // This component provides basic analysis via ProcessAudioData callback
    if (SubmixToAnalyze)
    {
        UE_LOG(LogRshipAudio, Log, TEXT("Submix configured: %s"), *SubmixToAnalyze->GetName());
    }
}

void URshipAudioReactive::CleanupSubmixAnalysis()
{
    // No dynamic delegate bindings to clean up
}

void URshipAudioReactive::OnSubmixEnvelope(const TArray<float>& Envelope)
{
    // This would be called with envelope data from the submix
    // For now, we'll use a simpler approach in PerformFFT
}

void URshipAudioReactive::ProcessAudioData(const float* Data, int32 NumSamples, int32 NumChannels)
{
    if (NumSamples == 0) return;

    // Mix down to mono and store in buffer
    int32 MonoSamples = NumSamples / NumChannels;
    for (int32 i = 0; i < MonoSamples && i < FFTSize; i++)
    {
        float Sample = 0.0f;
        for (int32 c = 0; c < NumChannels; c++)
        {
            Sample += Data[i * NumChannels + c];
        }
        Sample /= NumChannels;
        AudioBuffer[i] = Sample * InputGain;
    }
}

void URshipAudioReactive::PerformFFT()
{
    // Calculate RMS and peak from the buffer
    float SumSquares = 0.0f;
    float Peak = 0.0f;

    for (int32 i = 0; i < AudioBuffer.Num(); i++)
    {
        float Sample = FMath::Abs(AudioBuffer[i]);
        SumSquares += Sample * Sample;
        Peak = FMath::Max(Peak, Sample);
    }

    float RMS = FMath::Sqrt(SumSquares / AudioBuffer.Num());

    // Apply noise floor
    if (RMS < NoiseFloor)
    {
        RMS = 0.0f;
    }
    else
    {
        RMS = (RMS - NoiseFloor) / (1.0f - NoiseFloor);
    }

    CurrentAnalysis.RMS = FMath::Clamp(RMS, 0.0f, 1.0f);

    // Update peak
    if (Peak > CurrentPeak)
    {
        CurrentPeak = Peak;
        PeakHoldTimer = 0.0f;
    }
    CurrentAnalysis.Peak = FMath::Clamp(CurrentPeak, 0.0f, 1.0f);

    // Use simple band-pass energy estimation instead of FFT
    // (Audio::FFFTAlgorithm API changed in UE 5.6, using fallback)
    float SampleRate = 48000.0f;  // Assume 48kHz

    // Simple spectral estimation using band-pass energy
    // This is a fallback - for proper FFT, use AudioSynesthesia plugin
    int32 NumBins = FFTSize / 2;

    // Zero out magnitudes
    for (int32 i = 0; i < FFTMagnitudes.Num(); i++)
    {
        FFTMagnitudes[i] = 0.0f;
    }

    // Estimate energy in frequency bands using simple filtering
    // Low frequencies: running average (low-pass approximation)
    // High frequencies: difference from average (high-pass approximation)
    float Sum = 0.0f;
    float SumSq = 0.0f;
    for (int32 i = 0; i < AudioBuffer.Num(); i++)
    {
        Sum += AudioBuffer[i];
        SumSq += AudioBuffer[i] * AudioBuffer[i];
    }

    float Avg = Sum / FMath::Max(1, AudioBuffer.Num());
    float Variance = (SumSq / FMath::Max(1, AudioBuffer.Num())) - (Avg * Avg);
    float StdDev = FMath::Sqrt(FMath::Max(0.0f, Variance));

    // Distribute energy estimate across bins (simplified)
    // Low bins get more of the average (bass), high bins get more of the variance (treble)
    for (int32 i = 0; i < NumBins && i < FFTMagnitudes.Num(); i++)
    {
        float BinPosition = (float)i / (float)NumBins;
        // Mix between low-frequency (average) and high-frequency (variance) components
        FFTMagnitudes[i] = FMath::Lerp(FMath::Abs(Avg), StdDev, BinPosition) * 0.1f;
    }

    // Calculate spectral features
    float WeightedSum = 0.0f;
    float MagnitudeSum = 0.0f;
    float LogSum = 0.0f;
    float ArithSum = 0.0f;
    int32 ValidBins = 0;

    for (int32 i = 1; i < FFTMagnitudes.Num(); i++)
    {
        float Freq = (float)i * SampleRate / (float)FFTSize;
        float Mag = FFTMagnitudes[i];

        WeightedSum += Freq * Mag;
        MagnitudeSum += Mag;

        if (Mag > 0.0001f)
        {
            LogSum += FMath::Loge(Mag);
            ArithSum += Mag;
            ValidBins++;
        }
    }

    // Spectral centroid (brightness)
    if (MagnitudeSum > 0.0f)
    {
        CurrentAnalysis.SpectralCentroid = WeightedSum / MagnitudeSum / 10000.0f;  // Normalize
    }

    // Spectral flatness (tonal vs noise)
    if (ValidBins > 0 && ArithSum > 0.0f)
    {
        float GeometricMean = FMath::Exp(LogSum / ValidBins);
        float ArithmeticMean = ArithSum / ValidBins;
        CurrentAnalysis.SpectralFlatness = GeometricMean / ArithmeticMean;
    }
}

void URshipAudioReactive::AnalyzeFrequencyBands()
{
    float SampleRate = 48000.0f;

    for (int32 i = 0; i < FrequencyBands.Num(); i++)
    {
        FRshipFrequencyBandDef& Band = FrequencyBands[i];

        float MinHz, MaxHz;
        GetBandFrequencyRange(Band.Band, MinHz, MaxHz);

        if (Band.Band == ERshipFrequencyBand::Custom)
        {
            MinHz = Band.CustomMinHz;
            MaxHz = Band.CustomMaxHz;
        }

        Band.TargetValue = GetBandEnergy(MinHz, MaxHz) * Band.Gain;
        Band.TargetValue = FMath::Clamp(Band.TargetValue, 0.0f, 1.0f);
    }
}

float URshipAudioReactive::GetBandEnergy(float MinHz, float MaxHz)
{
    float SampleRate = 48000.0f;
    int32 MinBin = FMath::Max(1, FMath::FloorToInt(MinHz * FFTSize / SampleRate));
    int32 MaxBin = FMath::Min(FFTMagnitudes.Num() - 1, FMath::CeilToInt(MaxHz * FFTSize / SampleRate));

    float Energy = 0.0f;
    int32 Count = 0;

    for (int32 i = MinBin; i <= MaxBin && i < FFTMagnitudes.Num(); i++)
    {
        Energy += FFTMagnitudes[i];
        Count++;
    }

    if (Count > 0)
    {
        Energy /= Count;
    }

    // Scale to 0-1 range (adjust scaling factor as needed)
    return FMath::Min(Energy * 10.0f, 1.0f);
}

void URshipAudioReactive::GetBandFrequencyRange(ERshipFrequencyBand Band, float& OutMinHz, float& OutMaxHz)
{
    switch (Band)
    {
        case ERshipFrequencyBand::SubBass:
            OutMinHz = 20.0f;
            OutMaxHz = 60.0f;
            break;
        case ERshipFrequencyBand::Bass:
            OutMinHz = 60.0f;
            OutMaxHz = 250.0f;
            break;
        case ERshipFrequencyBand::LowMid:
            OutMinHz = 250.0f;
            OutMaxHz = 500.0f;
            break;
        case ERshipFrequencyBand::Mid:
            OutMinHz = 500.0f;
            OutMaxHz = 2000.0f;
            break;
        case ERshipFrequencyBand::HighMid:
            OutMinHz = 2000.0f;
            OutMaxHz = 4000.0f;
            break;
        case ERshipFrequencyBand::High:
            OutMinHz = 4000.0f;
            OutMaxHz = 6000.0f;
            break;
        case ERshipFrequencyBand::Presence:
            OutMinHz = 6000.0f;
            OutMaxHz = 20000.0f;
            break;
        default:
            OutMinHz = 20.0f;
            OutMaxHz = 20000.0f;
            break;
    }
}

void URshipAudioReactive::DetectBeat()
{
    // Get energy for beat detection
    float CurrentEnergy;
    if (bUseBassForBeats)
    {
        CurrentEnergy = GetBandEnergy(60.0f, 250.0f);
    }
    else
    {
        CurrentEnergy = CurrentAnalysis.RMS;
    }

    // Store in history
    EnergyHistory.RemoveAt(0);
    EnergyHistory.Add(CurrentEnergy);

    // Calculate average energy
    float AverageEnergy = 0.0f;
    for (float E : EnergyHistory)
    {
        AverageEnergy += E;
    }
    AverageEnergy /= EnergyHistory.Num();

    // Calculate variance
    float Variance = 0.0f;
    for (float E : EnergyHistory)
    {
        Variance += FMath::Pow(E - AverageEnergy, 2);
    }
    Variance /= EnergyHistory.Num();

    // Dynamic threshold based on variance
    float DynamicThreshold = AverageEnergy + BeatThreshold * FMath::Sqrt(Variance);

    // Check for beat
    double CurrentTime = FPlatformTime::Seconds();
    float TimeSinceLast = (float)(CurrentTime - LastBeatTime);

    if (CurrentEnergy > DynamicThreshold && TimeSinceLast > MinBeatInterval)
    {
        CurrentAnalysis.bBeatDetected = true;
        BeatEnergy = CurrentEnergy;
        BeatTimes.Add(CurrentTime);
        LastBeatTime = CurrentTime;

        // Keep last 16 beats for BPM estimation
        while (BeatTimes.Num() > 16)
        {
            BeatTimes.RemoveAt(0);
        }
    }

    // Calculate confidence based on energy ratio
    if (AverageEnergy > 0.0f)
    {
        CurrentAnalysis.BeatConfidence = FMath::Clamp((CurrentEnergy - AverageEnergy) / AverageEnergy, 0.0f, 1.0f);
    }
}

void URshipAudioReactive::UpdateBPMEstimate()
{
    if (BeatTimes.Num() < 4) return;

    // Calculate average interval between beats
    TArray<float> Intervals;
    for (int32 i = 1; i < BeatTimes.Num(); i++)
    {
        Intervals.Add((float)(BeatTimes[i] - BeatTimes[i - 1]));
    }

    // Sort and take median to filter outliers
    Intervals.Sort();
    float MedianInterval = Intervals[Intervals.Num() / 2];

    if (MedianInterval > 0.0f)
    {
        float BPM = 60.0f / MedianInterval;

        // Clamp to reasonable range
        if (BPM >= 60.0f && BPM <= 200.0f)
        {
            // Smooth the BPM estimate
            if (CurrentAnalysis.EstimatedBPM > 0.0f)
            {
                CurrentAnalysis.EstimatedBPM = FMath::Lerp(CurrentAnalysis.EstimatedBPM, BPM, 0.1f);
            }
            else
            {
                CurrentAnalysis.EstimatedBPM = BPM;
            }
        }
    }
}

void URshipAudioReactive::ApplySmoothing(float DeltaTime)
{
    // Smooth level
    float Alpha = 1.0f - FMath::Pow(LevelSmoothing, DeltaTime * 60.0f);
    CurrentAnalysis.Level = FMath::Lerp(CurrentAnalysis.Level, CurrentAnalysis.RMS, Alpha);

    // Smooth frequency bands
    for (int32 i = 0; i < FrequencyBands.Num(); i++)
    {
        FRshipFrequencyBandDef& Band = FrequencyBands[i];
        float BandAlpha = 1.0f - FMath::Pow(Band.Smoothing, DeltaTime * 60.0f);
        Band.CurrentValue = FMath::Lerp(Band.CurrentValue, Band.TargetValue, BandAlpha);

        if (i < CurrentAnalysis.Bands.Num())
        {
            CurrentAnalysis.Bands[i] = Band.CurrentValue;
        }
    }
}

void URshipAudioReactive::EmitToRship()
{
    if (!Subsystem || TargetId.IsEmpty()) return;

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

    // Add level data
    if (bEmitLevel)
    {
        Data->SetNumberField(TEXT("level"), CurrentAnalysis.Level);
        Data->SetNumberField(TEXT("peak"), CurrentAnalysis.Peak);
        Data->SetNumberField(TEXT("rms"), CurrentAnalysis.RMS);
    }

    // Add beat data
    if (bEmitBeat)
    {
        Data->SetBoolField(TEXT("beat"), CurrentAnalysis.bBeatDetected);
        Data->SetNumberField(TEXT("bpm"), CurrentAnalysis.EstimatedBPM);
        Data->SetNumberField(TEXT("beatConfidence"), CurrentAnalysis.BeatConfidence);
    }

    // Add frequency bands
    if (bEmitBands)
    {
        for (int32 i = 0; i < FrequencyBands.Num(); i++)
        {
            const FRshipFrequencyBandDef& Band = FrequencyBands[i];
            if (!Band.OutputField.IsEmpty())
            {
                Data->SetNumberField(Band.OutputField, Band.CurrentValue);
            }
        }
    }

    // Add spectral features
    Data->SetNumberField(TEXT("brightness"), CurrentAnalysis.SpectralCentroid);
    Data->SetNumberField(TEXT("noisiness"), CurrentAnalysis.SpectralFlatness);

    Subsystem->PulseEmitter(TargetId, EmitterId, Data);
}

float URshipAudioReactive::GetBandValue(int32 BandIndex) const
{
    if (BandIndex >= 0 && BandIndex < FrequencyBands.Num())
    {
        return FrequencyBands[BandIndex].CurrentValue;
    }
    return 0.0f;
}

void URshipAudioReactive::TriggerBeat(float Intensity)
{
    CurrentAnalysis.bBeatDetected = true;
    BeatEnergy = Intensity;
    LastBeatTime = FPlatformTime::Seconds();
    BeatTimes.Add(LastBeatTime);

    OnBeatDetected.Broadcast(Intensity);
}

// ============================================================================
// AUDIO MANAGER
// ============================================================================

void URshipAudioManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogRshipAudio, Log, TEXT("Audio Manager initialized"));
}

void URshipAudioManager::Shutdown()
{
    ActiveComponents.Empty();
    UE_LOG(LogRshipAudio, Log, TEXT("Audio Manager shut down"));
}

void URshipAudioManager::RegisterComponent(URshipAudioReactive* Component)
{
    if (Component && !ActiveComponents.Contains(Component))
    {
        ActiveComponents.Add(Component);
    }
}

void URshipAudioManager::UnregisterComponent(URshipAudioReactive* Component)
{
    ActiveComponents.Remove(Component);
}

float URshipAudioManager::GetGlobalLevel() const
{
    float MaxLevel = 0.0f;
    for (URshipAudioReactive* Comp : ActiveComponents)
    {
        if (Comp && Comp->bEnabled)
        {
            MaxLevel = FMath::Max(MaxLevel, Comp->GetLevel());
        }
    }
    return MaxLevel;
}

bool URshipAudioManager::WasAnyBeatDetected() const
{
    for (URshipAudioReactive* Comp : ActiveComponents)
    {
        if (Comp && Comp->bEnabled && Comp->WasBeatDetected())
        {
            return true;
        }
    }
    return false;
}

float URshipAudioManager::GetGlobalBPM() const
{
    // Return first non-zero BPM
    for (URshipAudioReactive* Comp : ActiveComponents)
    {
        if (Comp && Comp->bEnabled && Comp->GetBPM() > 0.0f)
        {
            return Comp->GetBPM();
        }
    }
    return 0.0f;
}
