#include "RshipFieldRawSampler.h"

#include "RshipFieldComponent.h"
#include "RshipFieldSubsystem.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"

void URshipFieldRawSampler::RegisterOrRefreshTarget()
{
    FRshipTargetProxy Target = ResolveChildTarget(ChildTargetSuffix, TEXT("fieldSampler"));
    if (!Target.IsValid())
    {
        return;
    }

    Target.AddPropertyAction(this, TEXT("FieldId"));
}

URshipFieldComponent* URshipFieldRawSampler::ResolveField() const
{
    UWorld* World = GetWorld();
    if (!World || FieldId.IsEmpty())
    {
        return nullptr;
    }

    URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>();
    return Subsystem ? Subsystem->FindFieldById(FieldId) : nullptr;
}

UTextureRenderTarget2D* URshipFieldRawSampler::GetScalarAtlas() const
{
    URshipFieldComponent* Field = ResolveField();
    return Field ? Field->GetScalarAtlas() : nullptr;
}

UTextureRenderTarget2D* URshipFieldRawSampler::GetVectorAtlas() const
{
    URshipFieldComponent* Field = ResolveField();
    return Field ? Field->GetVectorAtlas() : nullptr;
}
