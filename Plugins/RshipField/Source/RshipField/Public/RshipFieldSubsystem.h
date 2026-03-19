#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RshipFieldTypes.h"
#include "RshipFieldSubsystem.generated.h"

class URshipFieldComponent;
class URshipFieldLightSampler;
class UTextureRenderTarget2D;

UCLASS()
class RSHIPFIELD_API URshipFieldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void RegisterField(URshipFieldComponent* Field);
    void UnregisterField(URshipFieldComponent* Field);

    void RegisterLightSampler(URshipFieldLightSampler* Sampler);
    void UnregisterLightSampler(URshipFieldLightSampler* Sampler);

    URshipFieldComponent* FindFieldById(const FString& InFieldId) const;

    // Called by field components each tick.
    void TickField(URshipFieldComponent* Field, float DeltaTime);

    // Dispatch point sample pass and distribute results to light samplers.
    void DistributeLightSamplerResults(URshipFieldComponent* Field);

private:
    void DispatchFieldPasses(URshipFieldComponent* Field);
    int32 NormalizeResolution(int32 RequestedResolution) const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldComponent>> RegisteredFields;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldLightSampler>> RegisteredLightSamplers;

    // Tiny Nx1 render target for point sample results
    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> PointSampleRT = nullptr;

    TArray<FColor> PointSamplePixels;
};
