#include "RshipFieldMaterialSampler.h"

#include "RshipFieldComponent.h"
#include "RshipFieldSubsystem.h"

#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

URshipFieldMaterialSampler::URshipFieldMaterialSampler()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    bTickInEditor = true;
}

void URshipFieldMaterialSampler::OnRegister()
{
    Super::OnRegister();

    CachedMeshComponent = nullptr;
    bMIDsInitialized = false;

    if (AActor* Owner = GetOwner())
    {
        CachedMeshComponent = Owner->FindComponentByClass<UMeshComponent>();
    }
}

void URshipFieldMaterialSampler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedMeshComponent)
    {
        if (AActor* Owner = GetOwner())
        {
            CachedMeshComponent = Owner->FindComponentByClass<UMeshComponent>();
        }
        if (!CachedMeshComponent)
        {
            return;
        }
    }

    if (!bMIDsInitialized)
    {
        EnsureMaterialInstances();
    }

    // Check if MIDs are still valid — reconstruction can invalidate them
    for (const TObjectPtr<UMaterialInstanceDynamic>& MID : CachedMIDs)
    {
        if (!MID)
        {
            bMIDsInitialized = false;
            EnsureMaterialInstances();
            break;
        }
    }

    PushFieldParameters();
}

void URshipFieldMaterialSampler::RegisterOrRefreshTarget()
{
    FRshipTargetProxy Target = ResolveChildTarget(ChildTargetSuffix, TEXT("fieldMaterialSampler"));
    if (!Target.IsValid())
    {
        return;
    }

    Target.AddPropertyAction(this, TEXT("FieldId"));
}

void URshipFieldMaterialSampler::EnsureMaterialInstances()
{
    CachedMIDs.Reset();

    if (!CachedMeshComponent)
    {
        return;
    }

    const int32 NumMaterials = CachedMeshComponent->GetNumMaterials();

    for (int32 i = 0; i < NumMaterials; ++i)
    {
        // If FieldSamplerMaterialSlots is specified, only target those slots
        if (FieldSamplerMaterialSlots.Num() > 0 && !FieldSamplerMaterialSlots.Contains(i))
        {
            continue;
        }

        UMaterialInterface* Existing = CachedMeshComponent->GetMaterial(i);
        if (!Existing)
        {
            continue;
        }

        UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Existing);
        if (!MID)
        {
            MID = CachedMeshComponent->CreateDynamicMaterialInstance(i, Existing);
        }

        if (MID)
        {
            CachedMIDs.Add(MID);
        }
    }

    bMIDsInitialized = true;
}

void URshipFieldMaterialSampler::PushFieldParameters()
{
    if (CachedMIDs.Num() == 0)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>();
    if (!Subsystem)
    {
        return;
    }

    URshipFieldComponent* Field = Subsystem->FindFieldById(FieldId);
    if (!Field)
    {
        UE_LOG(LogTemp, Verbose, TEXT("FieldMaterialSampler: No field '%s', MIDs=%d"), *FieldId, CachedMIDs.Num());
        return;
    }

    UTextureRenderTarget2D* ScalarAtlas = Field->GetScalarAtlas();
    UTextureRenderTarget2D* VectorAtlas = Field->GetVectorAtlas();
    if (!ScalarAtlas || !VectorAtlas)
    {
        return;
    }

    const FVector DomainHalfExtent = FVector(Field->DomainSizeCm * 0.5f);
    const FLinearColor DomainMin = FLinearColor(
        Field->DomainCenterCm.X - DomainHalfExtent.X,
        Field->DomainCenterCm.Y - DomainHalfExtent.Y,
        Field->DomainCenterCm.Z - DomainHalfExtent.Z, 0.0f);
    const FLinearColor DomainMax = FLinearColor(
        Field->DomainCenterCm.X + DomainHalfExtent.X,
        Field->DomainCenterCm.Y + DomainHalfExtent.Y,
        Field->DomainCenterCm.Z + DomainHalfExtent.Z, 0.0f);

    static const FName PN_ScalarAtlas(TEXT("FieldScalarAtlas"));
    static const FName PN_VectorAtlas(TEXT("FieldVectorAtlas"));
    static const FName PN_DomainMin(TEXT("FieldDomainMin"));
    static const FName PN_DomainMax(TEXT("FieldDomainMax"));
    static const FName PN_Resolution(TEXT("FieldResolution"));

    for (UMaterialInstanceDynamic* MID : CachedMIDs)
    {
        if (!MID)
        {
            continue;
        }

        MID->SetTextureParameterValue(PN_ScalarAtlas, ScalarAtlas);
        MID->SetTextureParameterValue(PN_VectorAtlas, VectorAtlas);
        MID->SetVectorParameterValue(PN_DomainMin, DomainMin);
        MID->SetVectorParameterValue(PN_DomainMax, DomainMax);
        MID->SetScalarParameterValue(PN_Resolution, static_cast<float>(GetFieldResolutionValue(Field->FieldResolution)));
    }

    // Debug text
    if (GEngine && Field->bShowDebugText)
    {
        const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("?");
        GEngine->AddOnScreenDebugMessage(
            static_cast<uint64>(GetUniqueID()),
            0.0f,
            FColor::Magenta,
            FString::Printf(TEXT("[FieldMaterial] %s  field='%s'  MIDs=%d  res=%d"),
                *OwnerName, *FieldId, CachedMIDs.Num(), GetFieldResolutionValue(Field->FieldResolution)));
    }
}
