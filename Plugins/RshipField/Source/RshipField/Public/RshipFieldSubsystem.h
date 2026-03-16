#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "RshipFieldTypes.h"
#include "RshipFieldSubsystem.generated.h"

class URshipFieldComponent;
class URshipFieldSamplerComponent;

UCLASS()
class RSHIPFIELD_API URshipFieldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;

    void RegisterField(URshipFieldComponent* Field);
    void UnregisterField(URshipFieldComponent* Field);

    void RegisterSampler(URshipFieldSamplerComponent* Sampler);
    void UnregisterSampler(URshipFieldSamplerComponent* Sampler);

    URshipFieldComponent* FindFieldById(const FString& InFieldId) const;

    void SetDebugEnabled(bool bEnabled);
    void SetDebugMode(ERshipFieldDebugMode InMode);

private:
    void TickField(URshipFieldComponent* Field, float DeltaTime);
    void DispatchFieldPasses(URshipFieldComponent* Field);
    void ReadbackAndDistributeSamplers();
    int32 NormalizeResolution(int32 RequestedResolution) const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldComponent>> RegisteredFields;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldSamplerComponent>> RegisteredSamplers;

    bool bDebugEnabled = false;
    ERshipFieldDebugMode DebugMode = ERshipFieldDebugMode::Off;
};
