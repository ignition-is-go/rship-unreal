#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RotundaMullionTypes.h"
#include "RotundaMullionComponent.generated.h"

/**
 * Manages rotunda mullion/atom structured buffers for an Optimus deformer kernel.
 * Loads static geometry from CSV, exposes layered control via rship actions.
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rotunda Mullion"))
class RSHIPFIELD_API URotundaMullionComponent : public URshipControllerComponent
{
    GENERATED_BODY()

public:
    URotundaMullionComponent();

    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // -- Configuration --

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Rotunda")
    FString ChildTargetSuffix = TEXT("mullion");

    /** Path to mullion CSV file (relative to project Content dir, or absolute). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Rotunda|Data")
    FFilePath MullionCSVPath;

    /** Path to atom CSV file (relative to project Content dir, or absolute). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Rotunda|Data")
    FFilePath AtomCSVPath;

    // -- Control layers (editor-editable, packed to GPU each tick) --

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Rotunda|Layers")
    TArray<FMullionLayerDesc> Layers;

    // -- Data access (read by Optimus DI proxy) --

    const TArray<FMullionData>& GetMullionData() const { return MullionDataArray; }
    const TArray<FAtomData>& GetAtomData() const { return AtomDataArray; }
    const TArray<FMullionControlLayer>& GetControlLayers() const { return ControlLayers; }

    /** Load or reload CSV data. Returns true on success. */
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Rship|Rotunda")
    bool LoadCSVData();

    // -- Rship actions --

    UFUNCTION()
    void SetLayerAction(const FString& LayerJson);

    UFUNCTION()
    void SetLayerSelectionAction(int32 LayerIndex, const FString& SelectionJson);

    UFUNCTION()
    void SetLayerTransformAction(int32 LayerIndex, float RotX, float RotY, float RotZ, float RotW, float TransX, float TransY, float TransZ, float InScale);

    UFUNCTION()
    void SetLayerWeightAction(int32 LayerIndex, float Weight);

    UFUNCTION()
    void SetLayerBlendModeAction(int32 LayerIndex, EMullionBlendMode BlendMode);

    UFUNCTION()
    void ClearLayerAction(int32 LayerIndex);

    UFUNCTION()
    void SetLayersBatchAction(const FString& BatchJson);

    // Selection mask helpers
    void PackMullionMask(const TArray<int32>& Indices, uint32 OutMask[3]);
    void PackAtomMask(const TArray<int32>& Atoms, const TArray<int32>& Rows, const TArray<int32>& Columns, uint32 OutMask[5]);

private:
    virtual void RegisterOrRefreshTarget() override;
    void PackLayersToGPU();

    // CSV parsing helpers
    bool LoadMullionCSV(const FString& FilePath);
    bool LoadAtomCSV(const FString& FilePath);
    FString ResolveCSVPath(const FFilePath& InPath) const;

    TArray<FMullionData> MullionDataArray;
    TArray<FAtomData> AtomDataArray;
    TArray<FMullionControlLayer> ControlLayers;
};
