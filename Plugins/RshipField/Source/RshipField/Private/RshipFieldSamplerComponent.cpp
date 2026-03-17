#include "RshipFieldSamplerComponent.h"

#include "RshipFieldSubsystem.h"

#include "Engine/World.h"

void URshipFieldSamplerComponent::OnRegister()
{
    Super::OnRegister();

    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->RegisterSampler(this);
        }
    }
}

void URshipFieldSamplerComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>())
        {
            Subsystem->UnregisterSampler(this);
        }
    }

    Super::OnUnregister();
}
