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
    FVector PositionCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector Direction = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float RadiusCm = 1000.0f;

    // Controls how quickly the wave falls off with distance. 1 = linear, 2 = quadratic, etc.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.01"))
    float FalloffExponent = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Amplitude = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.001"))
    float WavelengthCm = 100.0f;

    // Oscillation rate in Hz. Ignored when effector is in a phase group (tempo drives rate instead).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float FrequencyHz = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float PhaseOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldWaveform Waveform = ERshipFieldWaveform::Sine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bInfiniteRange = false;

    // Assign to a phase group for tempo-synced oscillation. Empty = free-running.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString PhaseGroupId;
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bInfiniteRange = false;
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

USTRUCT()
struct RSHIPFIELD_API FRshipFieldEffectorDesc
{
    GENERATED_BODY()

    ERshipFieldEffectorType Type = ERshipFieldEffectorType::Wave;
    bool bEnabled = true;
    bool bInfiniteRange = false;
    FVector PositionCm = FVector::ZeroVector;
    FVector Direction = FVector::UpVector;
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
    FString PhaseGroupId;

    // Conversion from typed effectors
    static FRshipFieldEffectorDesc FromWave(const FRshipFieldWaveEffector& Wave);
    static FRshipFieldEffectorDesc FromNoise(const FRshipFieldNoiseEffector& Noise);
    static FRshipFieldEffectorDesc FromAttractor(const FRshipFieldAttractorEffector& Attractor);
};

// ============================================================================
// Phase Groups
// ============================================================================

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldPhaseGroup
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
