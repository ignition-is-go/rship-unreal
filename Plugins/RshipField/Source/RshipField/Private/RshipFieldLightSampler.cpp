#include "RshipFieldLightSampler.h"

#include "Components/LightComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

void URshipFieldLightSampler::OnRegister()
{
    Super::OnRegister();

    CachedLightComponent = nullptr;
    if (AActor* Owner = GetOwner())
    {
        CachedLightComponent = Owner->FindComponentByClass<ULightComponent>();
    }
}

void URshipFieldLightSampler::OnUnregister()
{
    CachedLightComponent = nullptr;
    Super::OnUnregister();
}

void URshipFieldLightSampler::RegisterOrRefreshTarget()
{
    FRshipTargetProxy Target = ResolveChildTarget(ChildTargetSuffix, TEXT("fieldSampler"));
    if (!Target.IsValid())
    {
        return;
    }

    Target
        .AddPropertyAction(this, TEXT("bDriveIntensity"))
        .AddPropertyAction(this, TEXT("IntensityFieldId"))
        .AddPropertyAction(this, TEXT("IntensityScale"))
        .AddPropertyAction(this, TEXT("bDriveColor"))
        .AddPropertyAction(this, TEXT("ColorFieldId"))
        .AddPropertyAction(this, TEXT("ColorA"))
        .AddPropertyAction(this, TEXT("ColorB"));
}

TArray<FString> URshipFieldLightSampler::GetRequiredFieldIds() const
{
    TArray<FString> Ids;
    if (bDriveIntensity && !IntensityFieldId.IsEmpty())
    {
        Ids.AddUnique(IntensityFieldId);
    }
    if (bDriveColor && !ColorFieldId.IsEmpty())
    {
        Ids.AddUnique(ColorFieldId);
    }
    return Ids;
}

void URshipFieldLightSampler::ApplySampledValue(const FString& FieldId, float Scalar, const FVector& Vector)
{
    if (bDriveIntensity && IntensityFieldId == FieldId)
    {
        SampledIntensityScalar = Scalar;
        if (CachedLightComponent)
        {
            CachedLightComponent->SetIntensity(FMath::Max(0.0f, Scalar * IntensityScale));
        }
    }

    if (bDriveColor && ColorFieldId == FieldId)
    {
        SampledColorScalar = Scalar;
        if (CachedLightComponent)
        {
            const float T = FMath::Clamp(Scalar, 0.0f, 1.0f);
            CachedLightComponent->SetLightColor(FMath::Lerp(ColorA, ColorB, T));
        }
    }

    if (GEngine)
    {
        const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("?");
        GEngine->AddOnScreenDebugMessage(
            static_cast<uint64>(GetUniqueID()),
            0.0f,
            FColor::Cyan,
            FString::Printf(TEXT("[Field] %s  I=%.3f  C=%.3f"), *OwnerName, SampledIntensityScalar, SampledColorScalar));
    }
}
