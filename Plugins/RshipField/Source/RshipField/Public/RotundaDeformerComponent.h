#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RotundaDeformerComponent.generated.h"

class USkeletalMeshComponent;
class UOptimusDeformerInstance;

/**
 * Sets Optimus deformer kernel variables on the parent actor's skeletal mesh.
 * Variable names match the MateoKernel.usf Read*() functions.
 * Properties prefixed RS_ for rship executor discovery.
 */
UCLASS(ClassGroup = (RshipField), meta = (BlueprintSpawnableComponent))
class RSHIPFIELD_API URotundaDeformerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URotundaDeformerComponent();

    // -- Global --
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Global")
    float RS_Enabled = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Global")
    FVector RS_RotundaCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Global", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RS_Energy = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Global")
    float RS_Amplitude = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Global")
    float RS_RingRadius = 100.0f;

    // -- Orbital --
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Orbital")
    float RS_Lobes = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Orbital")
    float RS_Speed1 = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Orbital")
    float RS_Speed2 = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Orbital")
    float RS_OrbitalWeight = 1.0f;

    // -- Vertical --
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Vertical")
    float RS_VertLobes = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Vertical")
    float RS_VertSpeed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Vertical")
    float RS_VerticalWeight = 1.0f;

    // -- Structural --
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Structural", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RS_VerticalProfile = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Structural", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float RS_AnchorWidth = 0.1f;

    // -- Shaping --
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Shaping", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RS_WaveShape = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Shaping")
    float RS_CrestSharpness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Shaping", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RS_RestThreshold = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deformer|Shaping")
    float RS_DecayK = 1.0f;

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void CacheDeformerInstance();
    void PushVariables();

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> CachedSkelMesh;

    TWeakObjectPtr<UOptimusDeformerInstance> CachedDeformerInstance;
};
