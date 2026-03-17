#include "RshipFieldComponent.h"

#include "RshipFieldSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogRshipFieldComponent, Log, All);

URshipFieldComponent::URshipFieldComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bTickEvenWhenPaused = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bTickInEditor = true;
}

void URshipFieldComponent::OnRegister()
{
    Super::OnRegister();

    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->RegisterField(this);
        }
    }

    UE_LOG(LogRshipFieldComponent, Log, TEXT("Field '%s' registered: %d waves, %d noise, %d attractors"), *FieldId, WaveEffectors.Num(), NoiseEffectors.Num(), AttractorEffectors.Num());
}

void URshipFieldComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->UnregisterField(this);
        }
    }

    ScalarAtlas = nullptr;
    VectorAtlas = nullptr;
    Super::OnUnregister();
}

void URshipFieldComponent::BeginPlay()
{
    Super::BeginPlay();
}

void URshipFieldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void URshipFieldComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UWorld* World = GetWorld();
    if (!World || !bEnabled)
    {
        return;
    }

    // Drive simulation + dispatch from the component tick so it works in editor.
    if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
    {
        Subsystem->TickField(this, DeltaTime);
        Subsystem->DistributeSamplersForField(this);
    }

    if (!bDebugEnabled)
    {
        return;
    }

    const FVector HalfExtent = FVector(DomainSizeCm * 0.5f);

    // Domain box
    DrawDebugBox(World, DomainCenterCm, HalfExtent, FColor::Green, false, -1.0f, 0, 2.0f);

    // Wave effectors
    for (const FRshipFieldWaveEffector& Wave : WaveEffectors)
    {
        if (!Wave.bEnabled)
        {
            continue;
        }

        const FColor Color = FColor::Cyan;
        DrawDebugSphere(World, Wave.PositionCm, Wave.bInfiniteRange ? 50.0f : Wave.RadiusCm, 16, Color, false, -1.0f, 0, 1.5f);
        DrawDebugDirectionalArrow(World, Wave.PositionCm, Wave.PositionCm + Wave.Direction.GetSafeNormal() * 100.0f, 20.0f, Color, false, -1.0f, 0, 2.0f);
    }

    // Noise effectors
    for (const FRshipFieldNoiseEffector& Noise : NoiseEffectors)
    {
        if (!Noise.bEnabled)
        {
            continue;
        }

        DrawDebugSphere(World, Noise.PositionCm, Noise.bInfiniteRange ? 50.0f : Noise.RadiusCm, 12, FColor::Purple, false, -1.0f, 0, 1.5f);
    }

    // Attractors
    for (const FRshipFieldAttractorEffector& Attractor : AttractorEffectors)
    {
        if (!Attractor.bEnabled)
        {
            continue;
        }

        const FColor Color = Attractor.Strength >= 0.0f ? FColor::Orange : FColor::Red;
        DrawDebugSphere(World, Attractor.PositionCm, Attractor.bInfiniteRange ? 50.0f : Attractor.RadiusCm, 12, Color, false, -1.0f, 0, 1.5f);
    }
}

void URshipFieldComponent::RegisterOrRefreshTarget()
{
    FRshipTargetProxy Target = ResolveChildTarget(ChildTargetSuffix, TEXT("field"));
    if (!Target.IsValid())
    {
        return;
    }

    Target
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetUpdateHzAction), TEXT("SetUpdateHz"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetFieldResolutionAction), TEXT("SetFieldResolution"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetMasterScalarGainAction), TEXT("SetMasterScalarGain"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetMasterVectorGainAction), TEXT("SetMasterVectorGain"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetDomainCenterAction), TEXT("SetDomainCenter"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetDomainSizeAction), TEXT("SetDomainSize"));

    Target
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetBpmAction), TEXT("SetBpm"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetTransportAction), TEXT("SetTransport"));

    Target.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipFieldComponent, SetFieldState), TEXT("SetFieldState"));
}

bool URshipFieldComponent::EnsureAtlasTextures()
{
    // Snap to allowed resolution
    const int32 Allowed[] = { 64, 128, 192, 256, 320 };
    int32 Resolution = Allowed[0];
    int32 BestDist = FMath::Abs(FieldResolution - Resolution);
    for (const int32 Candidate : Allowed)
    {
        const int32 Dist = FMath::Abs(FieldResolution - Candidate);
        if (Dist < BestDist)
        {
            Resolution = Candidate;
            BestDist = Dist;
        }
    }

    const int32 TilesPerRow = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Resolution))));
    const int32 AtlasDim = TilesPerRow * Resolution;

    auto EnsureTarget = [this, AtlasDim](TObjectPtr<UTextureRenderTarget2D>& Texture, ETextureRenderTargetFormat Format, const TCHAR* Name) -> bool
    {
        if (!Texture)
        {
            Texture = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), FName(Name), RF_Transient);
        }

        if (!Texture)
        {
            return false;
        }

        const bool bNeedsInit = Texture->SizeX != AtlasDim || Texture->SizeY != AtlasDim || Texture->RenderTargetFormat != Format || !Texture->bCanCreateUAV;
        if (bNeedsInit)
        {
            Texture->RenderTargetFormat = Format;
            Texture->bCanCreateUAV = true;
            Texture->bAutoGenerateMips = false;
            Texture->AddressX = TA_Clamp;
            Texture->AddressY = TA_Clamp;
            Texture->InitAutoFormat(AtlasDim, AtlasDim);
            Texture->ClearColor = FLinearColor::Black;
            Texture->UpdateResourceImmediate(true);
        }

        return true;
    };

    const FString ScalarName = FString::Printf(TEXT("RshipField_%s_ScalarAtlas"), *FieldId);
    const FString VectorName = FString::Printf(TEXT("RshipField_%s_VectorAtlas"), *FieldId);

    if (!EnsureTarget(ScalarAtlas, RTF_R16f, *ScalarName))
    {
        return false;
    }

    if (!EnsureTarget(VectorAtlas, RTF_RGBA16f, *VectorName))
    {
        return false;
    }

    return true;
}

// --- Actions ---

void URshipFieldComponent::SetUpdateHzAction(float Hz)
{
    UpdateHz = FMath::Clamp(Hz, 1.0f, 240.0f);
}

void URshipFieldComponent::SetFieldResolutionAction(int32 Resolution)
{
    FieldResolution = Resolution;
}

void URshipFieldComponent::SetMasterScalarGainAction(float Gain)
{
    MasterScalarGain = Gain;
}

void URshipFieldComponent::SetMasterVectorGainAction(float Gain)
{
    MasterVectorGain = Gain;
}

void URshipFieldComponent::SetDomainCenterAction(float X, float Y, float Z)
{
    DomainCenterCm = FVector(X, Y, Z);
}

void URshipFieldComponent::SetDomainSizeAction(float SizeCm)
{
    DomainSizeCm = FMath::Max(SizeCm, 1.0f);
}

void URshipFieldComponent::SetBpmAction(float InBpm)
{
    Bpm = FMath::Clamp(InBpm, 1.0f, 400.0f);
}

void URshipFieldComponent::SetTransportAction(float Phase, bool Playing)
{
    BeatPhase = Phase;
    bPlaying = Playing;
}

void URshipFieldComponent::SetFieldState(const FString& StateJson)
{
    LastFieldStateError.Reset();
    // TODO(ms): parse JSON into local properties
}
