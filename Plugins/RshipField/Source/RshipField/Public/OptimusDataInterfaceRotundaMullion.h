#pragma once

#include "OptimusComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "RotundaMullionTypes.h"
#include "OptimusDataInterfaceRotundaMullion.generated.h"

class FOptimusRotundaMullionParameters;

UCLASS(Category = ComputeFramework)
class RSHIPFIELD_API UOptimusRotundaMullionDataInterface : public UOptimusComputeDataInterface
{
    GENERATED_BODY()

public:
    //~ Begin UOptimusComputeDataInterface Interface
    FString GetDisplayName() const override;
    TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const override;
    TSubclassOf<UActorComponent> GetRequiredComponentClass() const override;
    //~ End UOptimusComputeDataInterface Interface

    //~ Begin UComputeDataInterface Interface
    TCHAR const* GetClassName() const override { return TEXT("RotundaMullion"); }
    bool CanSupportUnifiedDispatch() const override { return true; }
    void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
    void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
    TCHAR const* GetShaderVirtualPath() const override;
    void GetShaderHash(FString& InOutKey) const override;
    void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
    UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
    //~ End UComputeDataInterface Interface

private:
    static TCHAR const* TemplateFilePath;
};

UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class RSHIPFIELD_API UOptimusRotundaMullionDataProvider : public UComputeDataProvider
{
    GENERATED_BODY()

public:
    TWeakObjectPtr<USceneComponent> SceneComponent = nullptr;

    //~ Begin UComputeDataProvider Interface
    FComputeDataProviderRenderProxy* GetRenderProxy() override;
    //~ End UComputeDataProvider Interface
};

class FOptimusRotundaMullionProviderProxy : public FComputeDataProviderRenderProxy
{
public:
    FOptimusRotundaMullionProviderProxy(USceneComponent* InSceneComponent);

    //~ Begin FComputeDataProviderRenderProxy Interface
    bool IsValid(FValidationData const& InValidationData) const override;
    void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData) override;
    void GatherDispatchData(FDispatchData const& InDispatchData) override;
    //~ End FComputeDataProviderRenderProxy Interface

private:
    using FParameters = FOptimusRotundaMullionParameters;

    bool bIsValid = false;
    int32 MullionCount = 0;
    int32 AtomCount = 0;

    TArray<FMullionData> MullionDataCopy;
    TArray<FAtomData> AtomDataCopy;
    TArray<FMullionControlLayer> ControlLayersCopy;

    FRDGBufferSRVRef MullionBufferSRV = nullptr;
    FRDGBufferSRVRef AtomBufferSRV = nullptr;
    FRDGBufferSRVRef ControlLayerBufferSRV = nullptr;
};
