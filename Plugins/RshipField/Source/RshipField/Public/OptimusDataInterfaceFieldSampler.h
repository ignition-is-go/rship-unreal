#pragma once

#include "OptimusComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "OptimusDataInterfaceFieldSampler.generated.h"

class FOptimusFieldSamplerParameters;
class URshipFieldComponent;
class URshipFieldSubsystem;
class UTextureRenderTarget2D;

UCLASS(Category = ComputeFramework)
class RSHIPFIELD_API UOptimusFieldSamplerDataInterface : public UOptimusComputeDataInterface
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Field")
    FString FieldId = TEXT("default");

    //~ Begin UOptimusComputeDataInterface Interface
    FString GetDisplayName() const override;
    TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const override;
    TSubclassOf<UActorComponent> GetRequiredComponentClass() const override;
    //~ End UOptimusComputeDataInterface Interface

    //~ Begin UComputeDataInterface Interface
    TCHAR const* GetClassName() const override { return TEXT("RshipFieldSampler"); }
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
class RSHIPFIELD_API UOptimusFieldSamplerDataProvider : public UComputeDataProvider
{
    GENERATED_BODY()

public:
    FString FieldId;
    TWeakObjectPtr<USceneComponent> SceneComponent = nullptr;

    //~ Begin UComputeDataProvider Interface
    FComputeDataProviderRenderProxy* GetRenderProxy() override;
    //~ End UComputeDataProvider Interface
};

class FOptimusFieldSamplerProviderProxy : public FComputeDataProviderRenderProxy
{
public:
    FOptimusFieldSamplerProviderProxy(USceneComponent* InSceneComponent, const FString& InFieldId);

    //~ Begin FComputeDataProviderRenderProxy Interface
    bool IsValid(FValidationData const& InValidationData) const override;
    void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData) override;
    void GatherDispatchData(FDispatchData const& InDispatchData) override;
    //~ End FComputeDataProviderRenderProxy Interface

private:
    using FParameters = FOptimusFieldSamplerParameters;

    bool bIsValid = false;
    int32 FieldResolution = 0;
    int32 TilesPerRow = 0;
    FVector3f DomainMinCm = FVector3f::ZeroVector;
    FVector3f DomainMaxCm = FVector3f::ZeroVector;
    FTextureRHIRef ScalarAtlasRHI;
    FTextureRHIRef VectorAtlasRHI;

    FRDGTextureRef ScalarAtlasRDG = nullptr;
    FRDGTextureRef VectorAtlasRDG = nullptr;
};
