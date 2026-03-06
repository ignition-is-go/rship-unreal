#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

class FRDGBuilder;

namespace PulseRDG
{
struct FDispatchInputs
{
    FIntPoint GridSize = FIntPoint::ZeroValue;

    float TimeSeconds = 0.0f;
    float Speed = 1.0f;
    float Amplitude = 8.0f;
    float HeightFrequency = 0.03f;
    float CenterWidth = 0.35f;

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

void AddDeformPass(FRDGBuilder& GraphBuilder, const FDispatchInputs& Inputs);
}
