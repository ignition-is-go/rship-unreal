#pragma once

#include "CoreMinimal.h"
#include "RshipFieldSamplerComponent.h"
#include "RshipFieldLightSampler.generated.h"

class ULightComponent;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Field Light Sampler"))
class RSHIPFIELD_API URshipFieldLightSampler : public URshipFieldSamplerComponent
{
    GENERATED_BODY()

public:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;

    // Intensity
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Intensity")
    bool bDriveIntensity = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Intensity", meta = (EditCondition = "bDriveIntensity"))
    FString IntensityFieldId = TEXT("default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Intensity", meta = (EditCondition = "bDriveIntensity"))
    float IntensityScale = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Intensity")
    float SampledIntensityScalar = 0.0f;

    // Color
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Color")
    bool bDriveColor = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Color", meta = (EditCondition = "bDriveColor"))
    FString ColorFieldId = TEXT("default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Color", meta = (EditCondition = "bDriveColor"))
    FLinearColor ColorA = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Color", meta = (EditCondition = "bDriveColor"))
    FLinearColor ColorB = FLinearColor::White;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Color")
    float SampledColorScalar = 0.0f;

    virtual void ApplySampledValue(const FString& FieldId, float Scalar, const FVector& Vector) override;
    virtual TArray<FString> GetRequiredFieldIds() const override;

private:
    virtual void RegisterOrRefreshTarget() override;

    UPROPERTY(Transient)
    TObjectPtr<ULightComponent> CachedLightComponent = nullptr;
};
