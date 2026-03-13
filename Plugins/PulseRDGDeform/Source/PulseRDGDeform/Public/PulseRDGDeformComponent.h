#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PulseRDGDeformComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
class UPulseDeformCacheAsset;

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class PULSERDGDEFORM_API UPulseRDGDeformComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPulseRDGDeformComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Cache")
    TObjectPtr<UPulseDeformCacheAsset> DeformCache = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Output")
    TObjectPtr<UTextureRenderTarget2D> DeformedPositionRT = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Output")
    TObjectPtr<UTextureRenderTarget2D> DeformedNormalRT = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Target")
    TObjectPtr<UStaticMeshComponent> TargetMeshComponent = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse|Animation", meta = (ClampMin = "0.0"))
    float Speed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse|Animation", meta = (ClampMin = "0.0"))
    float Amplitude = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse|Animation", meta = (ClampMin = "0.0"))
    float HeightFrequency = 0.03f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse|Animation", meta = (ClampMin = "0.001"))
    float CenterWidth = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse|Performance", meta = (ClampMin = "1.0", ClampMax = "240.0"))
    float UpdateRateHz = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse|Performance")
    bool bUseAsyncCompute = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Material")
    FName RestPositionParameterName = TEXT("RestPositionTex");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Material")
    FName DeformedPositionParameterName = TEXT("DeformedPositionTex");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Material")
    FName DeformedNormalParameterName = TEXT("DeformedNormalTex");

private:
    bool ValidateConfig(FString* OutError = nullptr) const;
    void UpdateMaterialBindings();
    void DispatchDeformPass(float TimeSeconds);

    float TimeAccumulator = 0.0f;
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
};
