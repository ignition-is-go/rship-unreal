#pragma once

#include "CoreMinimal.h"
#include "RshipFieldTypes.generated.h"

// ============================================================================
// Enums
// ============================================================================

UENUM(BlueprintType)
enum class ERshipFieldBlendOp : uint8
{
    Add UMETA(DisplayName = "Add"),
    Subtract UMETA(DisplayName = "Subtract"),
    Min UMETA(DisplayName = "Min"),
    Max UMETA(DisplayName = "Max"),
    Multiply UMETA(DisplayName = "Multiply")
};

UENUM(BlueprintType)
enum class ERshipFieldWaveform : uint8
{
    Sine UMETA(DisplayName = "Sine"),
    Cosine UMETA(DisplayName = "Cosine"),
    Triangle UMETA(DisplayName = "Triangle"),
    Saw UMETA(DisplayName = "Saw"),
    Square UMETA(DisplayName = "Square")
};

UENUM(BlueprintType)
enum class ERshipFieldNoiseMode : uint8
{
    Value UMETA(DisplayName = "Value"),
    Simplex UMETA(DisplayName = "Simplex"),
    Curl UMETA(DisplayName = "Curl")
};

UENUM(BlueprintType)
enum class ERshipFieldDerive : uint8
{
    // Speed is derived from Frequency × Wavelength.
    Speed UMETA(DisplayName = "Derive Speed"),
    // Frequency is derived from Speed / Wavelength.
    Frequency UMETA(DisplayName = "Derive Frequency"),
    // Wavelength is derived from Speed / Frequency.
    Wavelength UMETA(DisplayName = "Derive Wavelength")
};

UENUM(BlueprintType)
enum class ERshipFieldResolution : uint8
{
    Res64 UMETA(DisplayName = "64"),
    Res128 UMETA(DisplayName = "128"),
    Res192 UMETA(DisplayName = "192"),
    Res256 UMETA(DisplayName = "256"),
    Res320 UMETA(DisplayName = "320"),
    Res384 UMETA(DisplayName = "384"),
    Res512 UMETA(DisplayName = "512")
};

inline int32 GetFieldResolutionValue(ERshipFieldResolution Res)
{
    switch (Res)
    {
    case ERshipFieldResolution::Res64:  return 64;
    case ERshipFieldResolution::Res128: return 128;
    case ERshipFieldResolution::Res192: return 192;
    case ERshipFieldResolution::Res256: return 256;
    case ERshipFieldResolution::Res320: return 320;
    case ERshipFieldResolution::Res384: return 384;
    case ERshipFieldResolution::Res512: return 512;
    default: return 128;
    }
}

UENUM(BlueprintType)
enum class ERshipFieldWaveMode : uint8
{
    // sin(kx) · cos(ωt) — fixed spatial pattern, amplitude breathes in place.
    Standing UMETA(DisplayName = "Standing"),
    // sin(kx - ωt) — wavefronts expand outward from source.
    Traveling UMETA(DisplayName = "Traveling")
};


// ============================================================================
// Typed effectors (artist-facing)
// ============================================================================

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldWaveEffector
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldWaveMode WaveMode = ERshipFieldWaveMode::Standing;

    // Tick to emit a single wavefront. Resets automatically after emission.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", Transient, meta = (EditCondition = "WaveMode == ERshipFieldWaveMode::Traveling"))
    bool bEmitOnce = false;

    // Automatically emit wavefronts at RepeatHz rate.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (EditCondition = "WaveMode == ERshipFieldWaveMode::Traveling"))
    bool bAutoEmit = true;

    // Auto-emission rate in Hz. Higher values approach continuous flow.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.01", EditCondition = "WaveMode == ERshipFieldWaveMode::Traveling && bAutoEmit"))
    float RepeatHz = 1.0f;

    // Sync group for tempo-locked oscillation. Empty = free-running.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString SyncGroup;

    // --- Always active ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector PositionCm = FVector::ZeroVector;

    // Polarization axis for the vector field. Zero = scalar only, no vector contribution.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector Polarization = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float RadiusCm = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bInfiniteRange = false;

    // 0 = no falloff (hard cutoff at radius), 1 = linear, 2 = quadratic.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float FalloffExponent = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldWaveform Waveform = ERshipFieldWaveform::Sine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Amplitude = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float PhaseOffset = 0.0f;

    // --- Wave parameters (v = f · λ) ---

    // Traveling: which parameter to derive from the other two.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (EditCondition = "WaveMode == ERshipFieldWaveMode::Traveling"))
    ERshipFieldDerive Derive = ERshipFieldDerive::Frequency;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.001", EditCondition = "WaveMode == ERshipFieldWaveMode::Standing || Derive != ERshipFieldDerive::Wavelength"))
    float WavelengthCm = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0", EditCondition = "WaveMode == ERshipFieldWaveMode::Standing || Derive != ERshipFieldDerive::Frequency"))
    float FrequencyHz = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.1", EditCondition = "WaveMode == ERshipFieldWaveMode::Traveling && Derive != ERshipFieldDerive::Speed"))
    float WaveSpeedCmPerSec = 500.0f;

    // --- Traveling (greyed out in Standing) ---

    // Gaussian spatial width of each wavefront pulse.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "1.0", EditCondition = "WaveMode == ERshipFieldWaveMode::Traveling"))
    float EnvelopeWidthCm = 500.0f;

    // Max concurrent wavefronts. Higher = more ripples in flight, more GPU cost per voxel.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "1", ClampMax = "64", EditCondition = "WaveMode == ERshipFieldWaveMode::Traveling"))
    int32 MaxWavefronts = 16;

};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldNoiseEffector
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector PositionCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float RadiusCm = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Amplitude = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldNoiseMode NoiseMode = ERshipFieldNoiseMode::Simplex;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float Scale = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bInfiniteRange = false;

    // Assign to a phase group for tempo-synced behavior. Empty = free-running.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString SyncGroup;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldAttractorEffector
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector PositionCm = FVector::ZeroVector;

    // Positive = attract, negative = repel.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Strength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float RadiusCm = 1000.0f;

    // Controls how quickly the force falls off with distance. 1 = linear, 2 = quadratic (natural for point forces), etc.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.01"))
    float FalloffExponent = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bInfiniteRange = false;

    // Assign to a phase group for tempo-synced behavior. Empty = free-running.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString SyncGroup;
};

// ============================================================================
// Internal effector (GPU pipeline, not directly artist-facing)
// ============================================================================

UENUM()
enum class ERshipFieldEffectorType : uint8
{
    Wave,
    Noise,
    Attractor
};

// Single active wavefront for a traveling-mode effector.
struct FRshipFieldWavefront
{
    float BirthTime = 0.0f;
    FVector BirthPositionCm = FVector::ZeroVector;
};

// Per-effector runtime state for traveling wave management.
struct FRshipFieldWaveEffectorState
{
    TArray<FRshipFieldWavefront> Wavefronts;
    float LastEmitTime = -1e6f;
};

USTRUCT()
struct RSHIPFIELD_API FRshipFieldEffectorDesc
{
    GENERATED_BODY()

    ERshipFieldEffectorType Type = ERshipFieldEffectorType::Wave;
    bool bEnabled = true;
    bool bInfiniteRange = false;
    FVector PositionCm = FVector::ZeroVector;
    FVector Polarization = FVector::ZeroVector;
    float RadiusCm = 1000.0f;
    float FalloffExponent = 1.0f;
    float Amplitude = 1.0f;
    float WavelengthCm = 100.0f;
    float FrequencyHz = 1.0f;
    float PhaseOffset = 0.0f;
    float FadeWeight = 1.0f;
    ERshipFieldWaveform Waveform = ERshipFieldWaveform::Sine;
    ERshipFieldBlendOp BlendOp = ERshipFieldBlendOp::Add;
    ERshipFieldNoiseMode NoiseMode = ERshipFieldNoiseMode::Value;
    float NoiseScale = 0.1f;
    float NoiseAmplitude = 0.0f;
    bool bAffectsScalar = true;
    bool bAffectsVector = true;
    float ClampMin = -100.0f;
    float ClampMax = 100.0f;
    FString SyncGroup;

    ERshipFieldWaveMode WaveMode = ERshipFieldWaveMode::Standing;
    float WaveSpeedCmPerSec = 500.0f;
    float EnvelopeWidthCm = 200.0f;

    // Set by subsystem at dispatch time — index range into flat wavefront buffer.
    int32 WavefrontOffset = 0;
    int32 WavefrontCount = 0;

    // Conversion from typed effectors
    static FRshipFieldEffectorDesc FromWave(const FRshipFieldWaveEffector& Wave);
    static FRshipFieldEffectorDesc FromNoise(const FRshipFieldNoiseEffector& Noise);
    static FRshipFieldEffectorDesc FromAttractor(const FRshipFieldAttractorEffector& Attractor);
};

// ============================================================================
// Sync Groups
// ============================================================================

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldSyncGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString Id;

    // Multiplier on the transport BPM. 1.0 = quarter notes, 0.5 = half-time, 2.0 = double-time.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float TempoMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float PhaseOffset = 0.0f;
};

// ============================================================================
// Layers (commented out, will return)
// ============================================================================

// USTRUCT(BlueprintType)
// struct RSHIPFIELD_API FRshipFieldLayerDesc
// {
//     GENERATED_BODY()
//     FString Id;
//     bool bEnabled = true;
//     ERshipFieldBlendOp BlendOp = ERshipFieldBlendOp::Add;
//     float Weight = 1.0f;
//     float ClampMin = -100.0f;
//     float ClampMax = 100.0f;
// };
