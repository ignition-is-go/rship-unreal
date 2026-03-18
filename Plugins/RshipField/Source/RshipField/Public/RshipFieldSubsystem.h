#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RshipFieldTypes.h"
#include "RshipFieldSubsystem.generated.h"

class URshipFieldComponent;
class URshipFieldSamplerComponent;

UCLASS()
class RSHIPFIELD_API URshipFieldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void RegisterField(URshipFieldComponent* Field);
    void UnregisterField(URshipFieldComponent* Field);

    void RegisterSampler(URshipFieldSamplerComponent* Sampler);
    void UnregisterSampler(URshipFieldSamplerComponent* Sampler);

    URshipFieldComponent* FindFieldById(const FString& InFieldId) const;

    // Called by field components each tick.
    void TickField(URshipFieldComponent* Field, float DeltaTime);
    void DistributeSamplersForField(URshipFieldComponent* Field);

private:
    void DispatchFieldPasses(URshipFieldComponent* Field);
    int32 NormalizeResolution(int32 RequestedResolution) const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldComponent>> RegisteredFields;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldSamplerComponent>> RegisteredSamplers;
};
