#include "RshipFieldRawSampler.h"

#include "Engine/Engine.h"
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

void URshipFieldRawSampler::ApplySampledValue(const FString& InFieldId, float Scalar, const FVector& Vector)
{
    if (InFieldId != FieldId)
    {
        return;
    }

    ScalarValue = Scalar;
    VectorValue = Vector;

    if (GEngine)
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
