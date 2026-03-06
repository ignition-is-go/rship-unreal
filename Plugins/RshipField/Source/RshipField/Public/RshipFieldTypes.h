#pragma once

#include "CoreMinimal.h"
#include "RshipFieldTypes.generated.h"

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
    Square UMETA(DisplayName = "Square"),
    CurveLUT UMETA(DisplayName = "Curve LUT")
};

UENUM(BlueprintType)
enum class ERshipFieldNoiseType : uint8
{
    None UMETA(DisplayName = "None"),
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
    IsolateEmitter UMETA(DisplayName = "Isolate Emitter"),
    IsolateLayer UMETA(DisplayName = "Isolate Layer"),
    ContribScalar UMETA(DisplayName = "Contribution Scalar"),
    ContribVector UMETA(DisplayName = "Contribution Vector")
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldTransportState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Bpm = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float BeatPhase = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bPlaying = true;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldGlobalParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "1.0", ClampMax = "240.0"))
    float UpdateHz = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float MasterScalarGain = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float MasterVectorGain = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    int32 FieldResolution = 256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector DomainCenterCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "1.0"))
    float DomainSizeCm = 10000.0f;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldLayerDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldBlendOp BlendOp = ERshipFieldBlendOp::Add;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float ClampMin = -100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float ClampMax = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString PhaseGroupId;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldPhaseGroupDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bSyncToTempo = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float TempoMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float PhaseOffset = 0.0f;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldEmitterDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString LayerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bInfiniteRange = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector PositionCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector Direction = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float RadiusCm = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Amplitude = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float WavelengthCm = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float FrequencyHz = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Speed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float PhaseOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float FadeWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float EnvelopeAttackSeconds = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float EnvelopeDecaySeconds = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldWaveform Waveform = ERshipFieldWaveform::Sine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldBlendOp BlendOp = ERshipFieldBlendOp::Add;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldNoiseType NoiseType = ERshipFieldNoiseType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float NoiseScale = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float NoiseAmplitude = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bAffectsScalar = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bAffectsVector = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float ClampMin = -100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float ClampMax = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString PhaseGroupId;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldSplineEmitterDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString LayerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    TArray<FVector> ControlPointsCm;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.01"))
    float CurvatureToleranceCm = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float Amplitude = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float RadiusCm = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldWaveform Waveform = ERshipFieldWaveform::Sine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldBlendOp BlendOp = ERshipFieldBlendOp::Add;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float ClampMin = -100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float ClampMax = 100.0f;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldTargetDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FGuid StableGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString VisibleTargetPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AxisWeightNormal = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AxisWeightWorld = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AxisWeightObject = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector WorldAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector ObjectAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float ScalarGain = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float VectorGain = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float MaxDisplacementCm = 100.0f;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldTargetIdentity
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString VisibleTargetPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FGuid StableGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString ActorPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString ComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString MeshPath;
};

USTRUCT(BlueprintType)
struct RSHIPFIELD_API FRshipFieldPacket
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "0"))
    int64 Sequence = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    int64 ApplyFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bHasApplyFrame = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FRshipFieldTransportState Transport;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FRshipFieldGlobalParams Globals;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    TArray<FRshipFieldLayerDesc> Layers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    TArray<FRshipFieldPhaseGroupDesc> PhaseGroups;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    TArray<FRshipFieldEmitterDesc> Emitters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    TArray<FRshipFieldSplineEmitterDesc> Splines;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    TArray<FRshipFieldTargetDesc> TargetOverrides;
};
