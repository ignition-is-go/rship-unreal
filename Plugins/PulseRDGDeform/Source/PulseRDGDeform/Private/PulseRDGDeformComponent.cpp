#include "PulseRDGDeformComponent.h"

#include "PulseDeformCacheAsset.h"
#include "PulseRDGShaders.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderGraphBuilder.h"
#include "RenderingThread.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogPulseRDGDeform, Log, All);

UPulseRDGDeformComponent::UPulseRDGDeformComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UPulseRDGDeformComponent::BeginPlay()
{
    Super::BeginPlay();

    FString Error;
    if (!ValidateConfig(&Error))
    {
        UE_LOG(LogPulseRDGDeform, Warning, TEXT("Pulse RDG deform config invalid: %s"), *Error);
        return;
    }

    UpdateMaterialBindings();
}

void UPulseRDGDeformComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DynamicMaterials.Reset();
    Super::EndPlay(EndPlayReason);
}

void UPulseRDGDeformComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!GetWorld())
    {
        return;
    }

    FString Error;
    if (!ValidateConfig(&Error))
    {
        UE_LOG(LogPulseRDGDeform, Verbose, TEXT("Skipping pulse deform dispatch: %s"), *Error);
        return;
    }

    const float Step = 1.0f / FMath::Max(UpdateRateHz, 1.0f);
    TimeAccumulator += DeltaTime;
    if (TimeAccumulator + KINDA_SMALL_NUMBER < Step)
    {
        return;
    }

    TimeAccumulator = FMath::Fmod(TimeAccumulator, Step);
    DispatchDeformPass(GetWorld()->GetTimeSeconds());
}

bool UPulseRDGDeformComponent::ValidateConfig(FString* OutError) const
{
    auto SetError = [OutError](const TCHAR* Message)
    {
        if (OutError)
        {
            *OutError = Message;
        }
    };

    if (!DeformCache || !DeformCache->IsValidForDispatch())
    {
        SetError(TEXT("DeformCache is null or missing textures."));
        return false;
    }

    if (!DeformedPositionRT || !DeformedNormalRT)
    {
        SetError(TEXT("Output render targets are not assigned."));
        return false;
    }

    if (!DeformedPositionRT->bCanCreateUAV || !DeformedNormalRT->bCanCreateUAV)
    {
        SetError(TEXT("Output render targets must have bCanCreateUAV enabled."));
        return false;
    }

    if (DeformedPositionRT->SizeX != DeformCache->GridWidth || DeformedPositionRT->SizeY != DeformCache->GridHeight)
    {
        SetError(TEXT("DeformedPositionRT dimensions must match cache grid size."));
        return false;
    }

    if (DeformedNormalRT->SizeX != DeformCache->GridWidth || DeformedNormalRT->SizeY != DeformCache->GridHeight)
    {
        SetError(TEXT("DeformedNormalRT dimensions must match cache grid size."));
        return false;
    }

    if (DeformCache->RestPositionTexture->GetSizeX() != DeformCache->GridWidth
        || DeformCache->RestPositionTexture->GetSizeY() != DeformCache->GridHeight)
    {
        SetError(TEXT("RestPositionTexture dimensions must match cache grid size."));
        return false;
    }

    if (DeformCache->RestNormalTexture->GetSizeX() != DeformCache->GridWidth
        || DeformCache->RestNormalTexture->GetSizeY() != DeformCache->GridHeight)
    {
        SetError(TEXT("RestNormalTexture dimensions must match cache grid size."));
        return false;
    }

    if (DeformCache->MaskTexture->GetSizeX() != DeformCache->GridWidth
        || DeformCache->MaskTexture->GetSizeY() != DeformCache->GridHeight)
    {
        SetError(TEXT("MaskTexture dimensions must match cache grid size."));
        return false;
    }

    return true;
}

void UPulseRDGDeformComponent::UpdateMaterialBindings()
{
    DynamicMaterials.Reset();

    if (!TargetMeshComponent || !DeformCache || !DeformedPositionRT || !DeformedNormalRT)
    {
        return;
    }

    const int32 MaterialCount = TargetMeshComponent->GetNumMaterials();
    DynamicMaterials.Reserve(MaterialCount);

    for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
    {
        UMaterialInstanceDynamic* MID = TargetMeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
        if (!MID)
        {
            continue;
        }

        MID->SetTextureParameterValue(RestPositionParameterName, DeformCache->RestPositionTexture);
        MID->SetTextureParameterValue(DeformedPositionParameterName, DeformedPositionRT);
        MID->SetTextureParameterValue(DeformedNormalParameterName, DeformedNormalRT);
        DynamicMaterials.Add(MID);
    }
}

void UPulseRDGDeformComponent::DispatchDeformPass(float TimeSeconds)
{
    check(DeformCache);
    check(DeformedPositionRT);
    check(DeformedNormalRT);

    FTextureResource* RestPosResource = DeformCache->RestPositionTexture ? DeformCache->RestPositionTexture->GetResource() : nullptr;
    FTextureResource* RestNormalResource = DeformCache->RestNormalTexture ? DeformCache->RestNormalTexture->GetResource() : nullptr;
    FTextureResource* MaskResource = DeformCache->MaskTexture ? DeformCache->MaskTexture->GetResource() : nullptr;
    FTextureRenderTargetResource* OutPosResource = DeformedPositionRT->GameThread_GetRenderTargetResource();
    FTextureRenderTargetResource* OutNormalResource = DeformedNormalRT->GameThread_GetRenderTargetResource();

    PulseRDG::FDispatchInputs Inputs;
    Inputs.GridSize = FIntPoint(DeformCache->GridWidth, DeformCache->GridHeight);
    Inputs.TimeSeconds = TimeSeconds;
    Inputs.Speed = Speed;
    Inputs.Amplitude = Amplitude;
    Inputs.HeightFrequency = HeightFrequency;
    Inputs.CenterWidth = CenterWidth;
    Inputs.bAsyncCompute = bUseAsyncCompute;
    Inputs.RestPositionTexture = RestPosResource ? RestPosResource->TextureRHI : nullptr;
    Inputs.RestNormalTexture = RestNormalResource ? RestNormalResource->TextureRHI : nullptr;
    Inputs.MaskTexture = MaskResource ? MaskResource->TextureRHI : nullptr;
    Inputs.OutDeformedPositionTexture = OutPosResource ? OutPosResource->GetRenderTargetTexture() : nullptr;
    Inputs.OutDeformedNormalTexture = OutNormalResource ? OutNormalResource->GetRenderTargetTexture() : nullptr;

    if (!Inputs.IsValid())
    {
        UE_LOG(LogPulseRDGDeform, Verbose, TEXT("Pulse RDG deform inputs are not RHI-ready yet."));
        return;
    }

    ENQUEUE_RENDER_COMMAND(PulseRDGDispatch)(
        [Inputs](FRHICommandListImmediate& RHICmdList)
        {
            if (!Inputs.IsValid())
            {
                return;
            }

            FRDGBuilder GraphBuilder(RHICmdList);
            PulseRDG::AddDeformPass(GraphBuilder, Inputs);
            GraphBuilder.Execute();
        });
}
