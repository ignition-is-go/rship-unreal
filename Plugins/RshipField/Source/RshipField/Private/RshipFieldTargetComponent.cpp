#include "RshipFieldTargetComponent.h"

#include "RshipFieldBindingRegistryAsset.h"
#include "RshipFieldSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TextureResource.h"

URshipFieldTargetComponent::URshipFieldTargetComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void URshipFieldTargetComponent::OnRegister()
{
    Super::OnRegister();
    RefreshDeterministicIdentity();
}

void URshipFieldTargetComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->RegisterTarget(this);
            UpdateMaterialBindings(Subsystem->GetGlobalScalarFieldAtlas(), Subsystem->GetGlobalVectorFieldAtlas());
        }
    }
}

void URshipFieldTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->UnregisterTarget(this);
        }
    }

    DynamicMaterials.Reset();
    Super::EndPlay(EndPlayReason);
}

void URshipFieldTargetComponent::RefreshDeterministicIdentity()
{
    AActor* Owner = GetOwner();
    FingerprintActorPath = Owner ? Owner->GetPathName() : FString();
    FingerprintComponentName = GetName();

    if (!TargetMeshComponent && Owner)
    {
        TargetMeshComponent = Owner->FindComponentByClass<UStaticMeshComponent>();
    }

    FingerprintMeshPath.Reset();
    if (TargetMeshComponent && TargetMeshComponent->GetStaticMesh())
    {
        FingerprintMeshPath = TargetMeshComponent->GetStaticMesh()->GetPathName();
    }

    if (VisibleTargetPath.IsEmpty())
    {
        const FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");
        VisibleTargetPath = FString::Printf(TEXT("/field/%s/%s"), *OwnerName, *GetName());
    }

    if (!StableGuid.IsValid() && BindingRegistryAsset)
    {
        if (const FRshipFieldTargetIdentity* FoundByPath = BindingRegistryAsset->FindByVisibleTargetPath(VisibleTargetPath))
        {
            StableGuid = FoundByPath->StableGuid;
        }
        if (!StableGuid.IsValid())
        {
            if (const FRshipFieldTargetIdentity* FoundByFingerprint = BindingRegistryAsset->FindByFingerprint(FingerprintActorPath, FingerprintComponentName, FingerprintMeshPath))
            {
                StableGuid = FoundByFingerprint->StableGuid;
            }
        }
    }

    if (!StableGuid.IsValid())
    {
        const uint32 A = GetTypeHash(FingerprintActorPath);
        const uint32 B = HashCombine(GetTypeHash(FingerprintComponentName), GetTypeHash(FingerprintMeshPath));
        const uint32 C = HashCombine(A, B);
        const uint32 D = HashCombine(C, 0x52534850u); // "RSHP"
        StableGuid = FGuid(A, B, C, D);
    }

    PersistIdentityToRegistry();
}

bool URshipFieldTargetComponent::ValidateConfig(FString* OutError) const
{
    auto SetError = [OutError](const TCHAR* Message)
    {
        if (OutError)
        {
            *OutError = Message;
        }
    };

    if (!RestPositionTexture || !RestNormalTexture || !MaskTexture)
    {
        SetError(TEXT("RestPositionTexture, RestNormalTexture, and MaskTexture are required."));
        return false;
    }

    if (!DeformedPositionRT || !DeformedNormalRT)
    {
        SetError(TEXT("DeformedPositionRT and DeformedNormalRT are required."));
        return false;
    }

    if (!DeformedPositionRT->bCanCreateUAV || !DeformedNormalRT->bCanCreateUAV)
    {
        SetError(TEXT("Output render targets must have bCanCreateUAV enabled."));
        return false;
    }

    if (DeformedPositionRT->SizeX != DeformedNormalRT->SizeX || DeformedPositionRT->SizeY != DeformedNormalRT->SizeY)
    {
        SetError(TEXT("Deformed output render targets must match dimensions."));
        return false;
    }

    if (RestPositionTexture->GetSizeX() != DeformedPositionRT->SizeX || RestPositionTexture->GetSizeY() != DeformedPositionRT->SizeY)
    {
        SetError(TEXT("RestPositionTexture dimensions must match output render targets."));
        return false;
    }

    if (RestNormalTexture->GetSizeX() != DeformedPositionRT->SizeX || RestNormalTexture->GetSizeY() != DeformedPositionRT->SizeY)
    {
        SetError(TEXT("RestNormalTexture dimensions must match output render targets."));
        return false;
    }

    if (MaskTexture->GetSizeX() != DeformedPositionRT->SizeX || MaskTexture->GetSizeY() != DeformedPositionRT->SizeY)
    {
        SetError(TEXT("MaskTexture dimensions must match output render targets."));
        return false;
    }

    return true;
}

void URshipFieldTargetComponent::UpdateMaterialBindings(UTexture* GlobalScalarAtlas, UTexture* GlobalVectorAtlas)
{
    DynamicMaterials.Reset();

    if (!TargetMeshComponent || !RestPositionTexture || !DeformedPositionRT || !DeformedNormalRT)
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

        MID->SetTextureParameterValue(RestPositionParameterName, RestPositionTexture);
        MID->SetTextureParameterValue(DeformedPositionParameterName, DeformedPositionRT);
        MID->SetTextureParameterValue(DeformedNormalParameterName, DeformedNormalRT);

        if (GlobalScalarAtlas)
        {
            MID->SetTextureParameterValue(GlobalScalarFieldParameterName, GlobalScalarAtlas);
        }

        if (GlobalVectorAtlas)
        {
            MID->SetTextureParameterValue(GlobalVectorFieldParameterName, GlobalVectorAtlas);
        }

        DynamicMaterials.Add(MID);
    }
}

FRshipFieldTargetIdentity URshipFieldTargetComponent::BuildIdentity() const
{
    FRshipFieldTargetIdentity Identity;
    Identity.VisibleTargetPath = VisibleTargetPath;
    Identity.StableGuid = StableGuid;
    Identity.ActorPath = FingerprintActorPath;
    Identity.ComponentName = FingerprintComponentName;
    Identity.MeshPath = FingerprintMeshPath;
    return Identity;
}

void URshipFieldTargetComponent::PersistIdentityToRegistry()
{
    if (!BindingRegistryAsset || !StableGuid.IsValid())
    {
        return;
    }

    FRshipFieldTargetIdentity Identity = BuildIdentity();
    BindingRegistryAsset->UpsertIdentity(Identity);
#if WITH_EDITOR
    BindingRegistryAsset->MarkPackageDirty();
#endif
}
