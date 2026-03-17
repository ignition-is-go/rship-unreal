#pragma once

#include "CoreMinimal.h"
#include "RshipFieldSamplerComponent.h"
#include "RshipFieldRawSampler.generated.h"

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Field Raw Sampler"))
class RSHIPFIELD_API URshipFieldRawSampler : public URshipFieldSamplerComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString FieldId = TEXT("default");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    float ScalarValue = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    FVector VectorValue = FVector::ZeroVector;

    virtual void ApplySampledValue(const FString& InFieldId, float Scalar, const FVector& Vector) override;
    virtual TArray<FString> GetRequiredFieldIds() const override;

private:
    virtual void RegisterOrRefreshTarget() override;
};
