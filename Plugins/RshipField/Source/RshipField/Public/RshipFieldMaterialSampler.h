#pragma once

#include "CoreMinimal.h"
#include "RshipFieldSamplerComponent.h"
#include "RshipFieldMaterialSampler.generated.h"

class UMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Pushes field atlas textures and domain parameters to material instances on the parent actor's mesh.
 * Materials use the MF_SampleRshipField material function to perform the atlas lookup.
 *
 * Parameter names pushed to materials:
 *   FieldScalarAtlas  (Texture)  - scalar field render target
 *   FieldVectorAtlas  (Texture)  - vector field render target
 *   FieldDomainMin    (Vector)   - world-space domain minimum
 *   FieldDomainMax    (Vector)   - world-space domain maximum
 *   FieldResolution   (Scalar)   - voxel resolution per axis
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Field Material Sampler"))
class RSHIPFIELD_API URshipFieldMaterialSampler : public URshipFieldSamplerComponent
{
    GENERATED_BODY()

public:
    URshipFieldMaterialSampler();

    virtual void OnRegister() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString FieldId = TEXT("default");

    // Which material slots use the field sampling material function. Leave empty if all slots are field materials.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    TArray<int32> FieldSamplerMaterialSlots;

private:
    virtual void RegisterOrRefreshTarget() override;
    void EnsureMaterialInstances();
    void PushFieldParameters();

    UPROPERTY(Transient)
    TObjectPtr<UMeshComponent> CachedMeshComponent = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> CachedMIDs;

    bool bMIDsInitialized = false;
};
