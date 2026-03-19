#include "RshipFieldRawSampler.h"

#include "RshipFieldComponent.h"
#include "RshipFieldSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void URshipFieldRawSampler::RegisterOrRefreshTarget()
{
    FRshipTargetProxy Target = ResolveChildTarget(ChildTargetSuffix, TEXT("fieldSampler"));
    if (!Target.IsValid())
    {
        return;
    }

    Target
        .AddPropertyAction(this, TEXT("FieldId"))
        .AddPropertyAction(this, TEXT("ScalarValue"))
        .AddPropertyAction(this, TEXT("VectorValue"));
}

TArray<FString> URshipFieldRawSampler::GetRequiredFieldIds() const
{
    TArray<FString> Ids;
    if (!FieldId.IsEmpty())
    {
        Ids.Add(FieldId);
    }
    return Ids;
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

void URshipFieldRawSampler::SampleAtWorldPosition(FVector WorldPosition, float& OutScalar, FVector& OutVector) const
{
    OutScalar = 0.0f;
    OutVector = FVector::ZeroVector;

    UWorld* World = GetWorld();
    if (!World || FieldId.IsEmpty())
    {
        return;
    }

    URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>();
    if (Subsystem)
    {
        Subsystem->SampleFieldAtPosition(FieldId, WorldPosition, OutScalar, OutVector);
    }
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

void URshipFieldRawSampler::ApplySampledValue(const FString& InFieldId, float Scalar, const FVector& Vector)
{
    if (InFieldId != FieldId)
    {
        return;
    }

    ScalarValue = Scalar;
    VectorValue = Vector;

    if (GEngine && IsDebugTextEnabled())
    {
        const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("?");
        GEngine->AddOnScreenDebugMessage(
            static_cast<uint64>(GetUniqueID()),
            0.0f,
            FColor::Cyan,
            FString::Printf(TEXT("[Field] %s  S=%.3f  V=(%.2f, %.2f, %.2f)"),
                *OwnerName, ScalarValue, VectorValue.X, VectorValue.Y, VectorValue.Z));
    }
}
