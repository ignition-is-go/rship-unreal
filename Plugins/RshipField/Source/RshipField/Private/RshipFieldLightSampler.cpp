#include "RshipFieldLightSampler.h"

#include "RshipFieldComponent.h"
#include "RshipFieldSubsystem.h"

#include "Components/LightComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void URshipFieldLightSampler::OnRegister()
{
    Super::OnRegister();

    CachedLightComponent = nullptr;
    if (AActor* Owner = GetOwner())
    {
        CachedLightComponent = Owner->FindComponentByClass<ULightComponent>();
    }

    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->RegisterLightSampler(this);
        }
    }
}

void URshipFieldLightSampler::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->UnregisterLightSampler(this);
        }
    }

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

void URshipFieldLightSampler::ApplyFieldSample(const FString& FieldId, float Scalar, const FVector& Vector)
{
    if (!CachedLightComponent)
    {
        return;
    }

    if (bDriveIntensity && IntensityFieldId == FieldId)
    {
        CachedLightComponent->SetIntensity(FMath::Max(0.0f, Scalar * IntensityScale));
    }

    if (bDriveColor && ColorFieldId == FieldId)
    {
        float T = FMath::Clamp(Scalar, 0.0f, 1.0f);
        CachedLightComponent->SetLightColor(FMath::Lerp(ColorA, ColorB, T));
    }

    if (GEngine)
    {
        bool bShowDebug = false;
        if (UWorld* World = GetWorld())
        {
            if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
            {
                if (URshipFieldComponent* F = Subsystem->FindFieldById(FieldId))
                {
                    bShowDebug = F->bShowDebugText;
                }
            }
        }

        if (bShowDebug)
        {
            const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("?");
            GEngine->AddOnScreenDebugMessage(
                static_cast<uint64>(GetUniqueID()),
                0.0f,
                FColor::Cyan,
                FString::Printf(TEXT("[FieldLight] %s  S=%.3f"), *OwnerName, Scalar));
        }
    }
}
