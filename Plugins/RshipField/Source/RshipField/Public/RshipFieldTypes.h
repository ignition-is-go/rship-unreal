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

UENUM(BlueprintType)
enum class ERshipFieldDebugMode : uint8
{
    Off UMETA(DisplayName = "Off"),
    Heatmap UMETA(DisplayName = "Heatmap"),
    SliceScalar UMETA(DisplayName = "Slice Scalar"),
    SliceVector UMETA(DisplayName = "Slice Vector"),
    IsolateEffector UMETA(DisplayName = "Isolate Effector"),
    IsolateLayer UMETA(DisplayName = "Isolate Layer"),
    ContribScalar UMETA(DisplayName = "Contribution Scalar"),
    ContribVector UMETA(DisplayName = "Contribution Vector")
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Amplitude = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.001"))
    float WavelengthCm = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0"))
    float FrequencyHz = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Speed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float PhaseOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldWaveform Waveform = ERshipFieldWaveform::Sine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bInfiniteRange = true;
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
    bool bInfiniteRange = true;
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
    bool bInfiniteRange = true;
    FVector PositionCm = FVector::ZeroVector;
    FVector Direction = FVector::UpVector;
    float RadiusCm = 1000.0f;
    float Amplitude = 1.0f;
    float WavelengthCm = 100.0f;
    float FrequencyHz = 1.0f;
    float Speed = 1.0f;
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

    // Conversion from typed effectors
    static FRshipFieldEffectorDesc FromWave(const FRshipFieldWaveEffector& Wave);
    static FRshipFieldEffectorDesc FromNoise(const FRshipFieldNoiseEffector& Noise);
    static FRshipFieldEffectorDesc FromAttractor(const FRshipFieldAttractorEffector& Attractor);
};

// ============================================================================
// Layers / Phase Groups (commented out for MVP, will return)
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
//     FString PhaseGroupId;
// };

// USTRUCT(BlueprintType)
// struct RSHIPFIELD_API FRshipFieldPhaseGroupDesc
// {
//     GENERATED_BODY()
//     FString Id;
//     bool bSyncToTempo = true;
//     float TempoMultiplier = 1.0f;
//     float PhaseOffset = 0.0f;
// };
