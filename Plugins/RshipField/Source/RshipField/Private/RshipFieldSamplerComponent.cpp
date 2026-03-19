#include "RshipFieldSamplerComponent.h"

#include "RshipFieldComponent.h"
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

bool URshipFieldSamplerComponent::IsDebugTextEnabled() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>();
    if (!Subsystem)
    {
        return false;
    }

    for (const FString& FieldId : GetRequiredFieldIds())
    {
        if (URshipFieldComponent* Field = Subsystem->FindFieldById(FieldId))
        {
            if (Field->bShowDebugText)
            {
                return true;
            }
        }
    }

    return false;
}
