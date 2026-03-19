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

    // GPU-native access to field atlas textures.
    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    UTextureRenderTarget2D* GetScalarAtlas() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    UTextureRenderTarget2D* GetVectorAtlas() const;

private:
    virtual void RegisterOrRefreshTarget() override;
    URshipFieldComponent* ResolveField() const;
};
