#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceFieldSampler.generated.h"

class URshipFieldComponent;
class URshipFieldSubsystem;

UCLASS(EditInlineNew, Category = "Rship", meta = (DisplayName = "Rship Field Sampler"))
class RSHIPFIELD_API UNiagaraDataInterfaceFieldSampler : public UNiagaraDataInterface
{
    GENERATED_UCLASS_BODY()

    BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
        SHADER_PARAMETER(int32, FieldResolution)
        SHADER_PARAMETER(int32, TilesPerRow)
        SHADER_PARAMETER(FVector3f, DomainMinCm)
        SHADER_PARAMETER(FVector3f, DomainMaxCm)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScalarAtlas)
        SHADER_PARAMETER_SAMPLER(SamplerState, ScalarAtlasSampler)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VectorAtlas)
        SHADER_PARAMETER_SAMPLER(SamplerState, VectorAtlasSampler)
    END_SHADER_PARAMETER_STRUCT()

public:
    UPROPERTY(EditAnywhere, Category = "Field")
    FString FieldId = TEXT("default");

    virtual void PostInitProperties() override;

    virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
    virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::GPUComputeSim; }

    virtual int32 PerInstanceDataSize() const override;
    virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
    virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
    virtual bool HasPreSimulateTick() const override { return true; }
    virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
    virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;

    virtual bool Equals(const UNiagaraDataInterface* Other) const override;

#if WITH_EDITORONLY_DATA
    virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
    virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
    virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
#endif
    virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
    virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

protected:
#if WITH_EDITORONLY_DATA
    virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif
    virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

private:
    static const FName SampleFieldName;
    static const FName SampleFieldVisualName;
};
