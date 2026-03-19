#include "RotundaDeformerComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "OptimusDeformerInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RotundaDeformerComponent)

URotundaDeformerComponent::URotundaDeformerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bTickInEditor = true;
}

void URotundaDeformerComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheDeformerInstance();
}

void URotundaDeformerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedDeformerInstance.IsValid())
    {
        CacheDeformerInstance();
        if (!CachedDeformerInstance.IsValid())
        {
            return;
        }
    }

    PushVariables();
}

void URotundaDeformerComponent::CacheDeformerInstance()
{
    CachedSkelMesh = nullptr;
    CachedDeformerInstance = nullptr;

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    CachedSkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!CachedSkelMesh)
    {
        return;
    }

    UMeshDeformerInstance* DeformerInst = CachedSkelMesh->GetMeshDeformerInstance();
    if (!DeformerInst)
    {
        return;
    }

    CachedDeformerInstance = Cast<UOptimusDeformerInstance>(DeformerInst);
}

void URotundaDeformerComponent::PushVariables()
{
    UOptimusDeformerInstance* Inst = CachedDeformerInstance.Get();
    if (!Inst)
    {
        return;
    }

    // Global
    Inst->SetFloatVariable(TEXT("RS_Enabled"), RS_Enabled);
    Inst->SetVectorVariable(TEXT("RS_RotundaCenter"), RS_RotundaCenter);
    Inst->SetFloatVariable(TEXT("RS_Energy"), RS_Energy);
    Inst->SetFloatVariable(TEXT("RS_Amplitude"), RS_Amplitude);
    Inst->SetFloatVariable(TEXT("RS_RingRadius"), RS_RingRadius);

    // Orbital
    Inst->SetFloatVariable(TEXT("RS_Lobes"), RS_Lobes);
    Inst->SetFloatVariable(TEXT("RS_Speed1"), RS_Speed1);
    Inst->SetFloatVariable(TEXT("RS_Speed2"), RS_Speed2);
    Inst->SetFloatVariable(TEXT("RS_OrbitalWeight"), RS_OrbitalWeight);

    // Vertical
    Inst->SetFloatVariable(TEXT("RS_VertLobes"), RS_VertLobes);
    Inst->SetFloatVariable(TEXT("RS_VertSpeed"), RS_VertSpeed);
    Inst->SetFloatVariable(TEXT("RS_VerticalWeight"), RS_VerticalWeight);

    // Structural
    Inst->SetFloatVariable(TEXT("RS_VerticalProfile"), RS_VerticalProfile);
    Inst->SetFloatVariable(TEXT("RS_AnchorWidth"), RS_AnchorWidth);

    // Shaping
    Inst->SetFloatVariable(TEXT("RS_WaveShape"), RS_WaveShape);
    Inst->SetFloatVariable(TEXT("RS_CrestSharpness"), RS_CrestSharpness);
    Inst->SetFloatVariable(TEXT("RS_RestThreshold"), RS_RestThreshold);
    Inst->SetFloatVariable(TEXT("RS_DecayK"), RS_DecayK);
}
