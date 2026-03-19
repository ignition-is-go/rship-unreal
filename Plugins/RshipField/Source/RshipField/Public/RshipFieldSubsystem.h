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

    // Sample the field at any world position. Uses cached readback from current tick.
    bool SampleFieldAtPosition(const FString& InFieldId, const FVector& WorldPosition, float& OutScalar, FVector& OutVector);

private:
    void DispatchFieldPasses(URshipFieldComponent* Field);
    void EnsureFieldReadbackCache(URshipFieldComponent* Field);
    int32 NormalizeResolution(int32 RequestedResolution) const;

    struct FFieldReadbackCache
    {
        TArray<FLinearColor> ScalarPixels;
        TArray<FLinearColor> VectorPixels;
        int32 Resolution = 0;
        int32 TilesPerRow = 0;
        int32 AtlasDim = 0;
        FVector DomainMin = FVector::ZeroVector;
        FVector InvDomainSize = FVector::ZeroVector;
        uint64 FrameNumber = 0;
    };

    bool SampleFromCache(const FFieldReadbackCache& Cache, const FVector& WorldPosition, float& OutScalar, FVector& OutVector) const;

    TMap<FString, FFieldReadbackCache> ReadbackCaches;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldComponent>> RegisteredFields;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldSamplerComponent>> RegisteredSamplers;
};
