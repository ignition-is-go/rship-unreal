#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

class FRDGBuilder;

namespace RshipFieldRDG
{

struct FGlobalDispatchInputs
{
    int32 FieldResolution = 256;
    int32 TilesPerRow = 16;

    float TimeSeconds = 0.0f;
    float BPM = 120.0f;
    float TransportPhase = 0.0f;
    float MasterScalarGain = 1.0f;
    float MasterVectorGain = 1.0f;
    FVector3f DomainMinCm = FVector3f(-5000.0f, -5000.0f, -5000.0f);
    FVector3f DomainMaxCm = FVector3f(5000.0f, 5000.0f, 5000.0f);
    uint32 LayerCount = 0;
    uint32 PhaseGroupCount = 0;
    uint32 EffectorCount = 0;

    int32 DebugMode = 0;
    int32 DebugSelectionIndex = -1;

    FTextureRHIRef OutScalarFieldAtlasTexture;
    FTextureRHIRef OutVectorFieldAtlasTexture;

    TArray<FVector4f> LayerDataA;
    TArray<FVector4f> LayerDataB;
    TArray<FVector4f> PhaseGroupData;
    // Per-effector data, 7 separate buffers. Layout documented in RshipFieldCS.usf.
    TArray<FVector4f> EffectorData0;
    TArray<FVector4f> EffectorData1;
    TArray<FVector4f> EffectorData2;
    TArray<FVector4f> EffectorData3;
    TArray<FVector4f> EffectorData4;
    TArray<FVector4f> EffectorData5;
    TArray<FVector4f> EffectorData6;

    bool IsValid() const
    {
        return FieldResolution > 0
            && TilesPerRow > 0
            && OutScalarFieldAtlasTexture.IsValid()
            && OutVectorFieldAtlasTexture.IsValid();
    }
};

struct FTargetDispatchInputs
{
    FIntPoint GridSize = FIntPoint::ZeroValue;

    float TimeSeconds = 0.0f;
    float ScalarGain = 1.0f;
    float VectorGain = 1.0f;
    float AxisWeightNormal = 1.0f;
    float AxisWeightWorld = 0.0f;
    float AxisWeightObject = 0.0f;
    float MaxDisplacementCm = 100.0f;

    FVector3f DomainMinCm = FVector3f(-5000.0f, -5000.0f, -5000.0f);
    FVector3f DomainMaxCm = FVector3f(5000.0f, 5000.0f, 5000.0f);
    FVector3f WorldAxis = FVector3f(0.0f, 0.0f, 1.0f);
    FVector3f ObjectAxis = FVector3f(0.0f, 0.0f, 1.0f);

    bool bAsyncCompute = true;

    FTextureRHIRef RestPositionTexture;
    FTextureRHIRef RestNormalTexture;
    FTextureRHIRef MaskTexture;

    FTextureRHIRef OutDeformedPositionTexture;
    FTextureRHIRef OutDeformedNormalTexture;

    bool IsValid() const
    {
        return GridSize.X > 0
            && GridSize.Y > 0
            && RestPositionTexture.IsValid()
            && RestNormalTexture.IsValid()
            && MaskTexture.IsValid()
            && OutDeformedPositionTexture.IsValid()
            && OutDeformedNormalTexture.IsValid();
    }
};

struct FPointSampleInputs
{
    int32 FieldResolution = 0;
    int32 TilesPerRow = 0;
    FVector3f DomainMinCm = FVector3f::ZeroVector;
    FVector3f DomainMaxCm = FVector3f::ZeroVector;
    uint32 NumSamples = 0;

    FTextureRHIRef ScalarAtlasTexture;
    FTextureRHIRef VectorAtlasTexture;
    FTextureRHIRef OutResultsTexture;

    TArray<FVector4f> Positions;
};

void AddFieldPasses(
    FRDGBuilder& GraphBuilder,
    const FGlobalDispatchInputs& GlobalInputs,
    const TArray<FTargetDispatchInputs>& TargetInputs);

void AddPointSamplePass(
    FRDGBuilder& GraphBuilder,
    const FPointSampleInputs& Inputs);
}
