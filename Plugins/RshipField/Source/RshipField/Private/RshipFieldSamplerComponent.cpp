#include "RshipFieldSamplerComponent.h"

#include "RshipFieldSubsystem.h"

#include "Engine/World.h"

URshipFieldSamplerComponent::URshipFieldSamplerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void URshipFieldSamplerComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->RegisterSampler(this);
        }
    }
}

void URshipFieldSamplerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->UnregisterSampler(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}
