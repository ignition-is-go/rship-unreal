#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipFieldSamplerComponent.generated.h"

UCLASS(Abstract, ClassGroup = (Rship))
class RSHIPFIELD_API URshipFieldSamplerComponent : public URshipControllerComponent
{
    GENERATED_BODY()

public:
    URshipFieldSamplerComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString ChildTargetSuffix = TEXT("fieldSampler");

    virtual void ApplySampledValue(const FString& FieldId, float Scalar, const FVector& Vector) {}
    virtual TArray<FString> GetRequiredFieldIds() const { return {}; }
};
