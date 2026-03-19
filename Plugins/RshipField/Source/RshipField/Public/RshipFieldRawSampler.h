#pragma once

#include "CoreMinimal.h"
#include "RshipFieldSamplerComponent.h"
#include "RshipFieldRawSampler.generated.h"

class URshipFieldComponent;
class UTextureRenderTarget2D;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Field Raw Sampler"))
class RSHIPFIELD_API URshipFieldRawSampler : public URshipFieldSamplerComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString FieldId = TEXT("default");

    // Sampled at actor world position each tick.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    float ScalarValue = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    FVector VectorValue = FVector::ZeroVector;

    // Sample the field at any world position. Returns scalar and vector values.
    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void SampleAtWorldPosition(FVector WorldPosition, float& OutScalar, FVector& OutVector) const;

    // Get the scalar atlas render target for direct texture access.
    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    UTextureRenderTarget2D* GetScalarAtlas() const;

    // Get the vector atlas render target for direct texture access.
    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    UTextureRenderTarget2D* GetVectorAtlas() const;

    virtual void ApplySampledValue(const FString& InFieldId, float Scalar, const FVector& Vector) override;
    virtual TArray<FString> GetRequiredFieldIds() const override;

private:
    virtual void RegisterOrRefreshTarget() override;
    URshipFieldComponent* ResolveField() const;
};
