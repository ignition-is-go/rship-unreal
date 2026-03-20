#include "NiagaraDataInterfaceFieldSampler.h"

#include "RshipFieldComponent.h"
#include "RshipFieldSubsystem.h"

#include "Engine/TextureRenderTarget2D.h"
#include "NiagaraCompileHashVisitor.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "TextureResource.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceFieldSampler"

const FName UNiagaraDataInterfaceFieldSampler::SampleFieldName(TEXT("SampleField"));
const FName UNiagaraDataInterfaceFieldSampler::SampleFieldVisualName(TEXT("SampleFieldVisual"));
static const TCHAR* FieldSamplerDITemplateShaderFile = TEXT("/Plugin/RshipField/Private/NiagaraDataInterfaceFieldSampler.ush");

struct FNDIFieldSamplerInstanceData
{
    int32 FieldResolution = 0;
    int32 TilesPerRow = 0;
    FVector4f DomainMinCm = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
    FVector4f DomainMaxCm = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
    FTextureRHIRef ScalarAtlasRHI;
    FTextureRHIRef VectorAtlasRHI;
};

struct FNDIFieldSamplerProxy : public FNiagaraDataInterfaceProxy
{
    virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return sizeof(FNDIFieldSamplerInstanceData); }

    static void ProvidePerInstanceDataForRenderThread(void* InDataForRenderThread, void* InDataFromGameThread, const FNiagaraSystemInstanceID& SystemInstance)
    {
        FNDIFieldSamplerInstanceData* DataForRT = new (InDataForRenderThread) FNDIFieldSamplerInstanceData();
        const FNDIFieldSamplerInstanceData* DataFromGT = static_cast<FNDIFieldSamplerInstanceData*>(InDataFromGameThread);
        *DataForRT = *DataFromGT;
    }

    virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& InstanceID) override
    {
        FNDIFieldSamplerInstanceData* InstanceDataFromGT = static_cast<FNDIFieldSamplerInstanceData*>(PerInstanceData);
        FNDIFieldSamplerInstanceData& InstanceData = SystemInstancesToInstanceData_RT.FindOrAdd(InstanceID);
        InstanceData = *InstanceDataFromGT;
        InstanceDataFromGT->~FNDIFieldSamplerInstanceData();
    }

    TMap<FNiagaraSystemInstanceID, FNDIFieldSamplerInstanceData> SystemInstancesToInstanceData_RT;
};

UNiagaraDataInterfaceFieldSampler::UNiagaraDataInterfaceFieldSampler(FObjectInitializer const& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Proxy.Reset(new FNDIFieldSamplerProxy());
}

void UNiagaraDataInterfaceFieldSampler::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
        FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
    }
}

bool UNiagaraDataInterfaceFieldSampler::Equals(const UNiagaraDataInterface* Other) const
{
    if (!Super::Equals(Other))
    {
        return false;
    }

    const UNiagaraDataInterfaceFieldSampler* OtherField = CastChecked<const UNiagaraDataInterfaceFieldSampler>(Other);
    return FieldId == OtherField->FieldId;
}

bool UNiagaraDataInterfaceFieldSampler::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if (!Super::CopyToInternal(Destination))
    {
        return false;
    }

    UNiagaraDataInterfaceFieldSampler* Dest = CastChecked<UNiagaraDataInterfaceFieldSampler>(Destination);
    Dest->FieldId = FieldId;
    return true;
}

int32 UNiagaraDataInterfaceFieldSampler::PerInstanceDataSize() const
{
    return sizeof(FNDIFieldSamplerInstanceData);
}

bool UNiagaraDataInterfaceFieldSampler::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
    new (PerInstanceData) FNDIFieldSamplerInstanceData();
    return true;
}

void UNiagaraDataInterfaceFieldSampler::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
    FNDIFieldSamplerInstanceData* InstanceData = static_cast<FNDIFieldSamplerInstanceData*>(PerInstanceData);
    InstanceData->~FNDIFieldSamplerInstanceData();

    ENQUEUE_RENDER_COMMAND(RemoveProxy)(
        [RT_Proxy = GetProxyAs<FNDIFieldSamplerProxy>(), InstanceID = SystemInstance->GetId()](FRHICommandListImmediate& CmdList)
        {
            RT_Proxy->SystemInstancesToInstanceData_RT.Remove(InstanceID);
        });
}

bool UNiagaraDataInterfaceFieldSampler::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
    FNDIFieldSamplerInstanceData* InstanceData = static_cast<FNDIFieldSamplerInstanceData*>(PerInstanceData);
    if (!InstanceData)
    {
        return true;
    }

    InstanceData->ScalarAtlasRHI = nullptr;
    InstanceData->VectorAtlasRHI = nullptr;
    InstanceData->FieldResolution = 0;

    UWorld* World = SystemInstance->GetWorld();
    if (!World)
    {
        return false;
    }

    URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>();
    if (!Subsystem)
    {
        return false;
    }

    URshipFieldComponent* Field = Subsystem->FindFieldById(FieldId);
    if (!Field)
    {
        return false;
    }

    UTextureRenderTarget2D* ScalarRT = Field->GetScalarAtlas();
    UTextureRenderTarget2D* VectorRT = Field->GetVectorAtlas();
    if (!ScalarRT || !VectorRT)
    {
        return false;
    }

    FTextureRenderTargetResource* ScalarResource = ScalarRT->GameThread_GetRenderTargetResource();
    FTextureRenderTargetResource* VectorResource = VectorRT->GameThread_GetRenderTargetResource();
    if (!ScalarResource || !VectorResource)
    {
        return false;
    }

    const int32 Resolution = GetFieldResolutionValue(Field->FieldResolution);
    const int32 TilesPerRow = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Resolution))));
    const FVector DomainHalfExtent = FVector(Field->DomainSizeCm * 0.5f);

    InstanceData->FieldResolution = Resolution;
    InstanceData->TilesPerRow = TilesPerRow;
    InstanceData->DomainMinCm = FVector4f(FVector3f(Field->DomainCenterCm - DomainHalfExtent), 0.0f);
    InstanceData->DomainMaxCm = FVector4f(FVector3f(Field->DomainCenterCm + DomainHalfExtent), 0.0f);
    InstanceData->ScalarAtlasRHI = ScalarResource->GetRenderTargetTexture();
    InstanceData->VectorAtlasRHI = VectorResource->GetRenderTargetTexture();

    return false;
}

void UNiagaraDataInterfaceFieldSampler::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
    FNDIFieldSamplerProxy::ProvidePerInstanceDataForRenderThread(DataForRenderThread, PerInstanceData, SystemInstance);
}

void UNiagaraDataInterfaceFieldSampler::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
    // GPU only — no VM implementation needed.
}

#if WITH_EDITORONLY_DATA

void UNiagaraDataInterfaceFieldSampler::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = SampleFieldName;
        Sig.Description = LOCTEXT("SampleFieldDescription", "Samples the field at a world position, returning scalar and vector values.");
        Sig.bMemberFunction = true;
        Sig.bRequiresExecPin = false;
        Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("FieldSampler")));
        Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("WorldPosition")));
        Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Scalar")));
        Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Vector")));
        OutFunctions.Add(Sig);
    }

    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = SampleFieldVisualName;
        Sig.Description = LOCTEXT("SampleFieldVisualDescription", "Samples the field and returns mesh-ready outputs: orientation quat, scale, and color with opacity.");
        Sig.bMemberFunction = true;
        Sig.bRequiresExecPin = false;
        Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("FieldSampler")));
        Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("WorldPosition")));
        Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Orientation")));
        Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("MeshScale")));
        Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
        OutFunctions.Add(Sig);
    }
}

bool UNiagaraDataInterfaceFieldSampler::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
    if (!Super::AppendCompileHash(InVisitor))
    {
        return false;
    }

    InVisitor->UpdateShaderFile(FieldSamplerDITemplateShaderFile);
    InVisitor->UpdateShaderParameters<FShaderParameters>();
    return true;
}

bool UNiagaraDataInterfaceFieldSampler::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
    return FunctionInfo.DefinitionName == SampleFieldName || FunctionInfo.DefinitionName == SampleFieldVisualName;
}

void UNiagaraDataInterfaceFieldSampler::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
    const TMap<FString, FStringFormatArg> TemplateArgs =
    {
        {TEXT("ParameterName"), ParamInfo.DataInterfaceHLSLSymbol},
    };
    AppendTemplateHLSL(OutHLSL, FieldSamplerDITemplateShaderFile, TemplateArgs);
}

#endif

void UNiagaraDataInterfaceFieldSampler::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
    ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void UNiagaraDataInterfaceFieldSampler::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
    FNDIFieldSamplerProxy& DIProxy = Context.GetProxy<FNDIFieldSamplerProxy>();
    FNDIFieldSamplerInstanceData* InstanceData = DIProxy.SystemInstancesToInstanceData_RT.Find(Context.GetSystemInstanceID());
    FRDGBuilder& GraphBuilder = Context.GetGraphBuilder();

    FShaderParameters* Params = Context.GetParameterNestedStruct<FShaderParameters>();

    if (InstanceData && InstanceData->ScalarAtlasRHI.IsValid() && InstanceData->VectorAtlasRHI.IsValid())
    {
        Params->FieldResolution = InstanceData->FieldResolution;
        Params->TilesPerRow = InstanceData->TilesPerRow;
        Params->DomainMinCm = InstanceData->DomainMinCm;
        Params->DomainMaxCm = InstanceData->DomainMaxCm;
        Params->ScalarAtlasSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI();
        Params->VectorAtlasSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI();

        if (Context.IsResourceBound(&Params->ScalarAtlas))
        {
            FRDGTextureRef ScalarRDG = GraphBuilder.FindExternalTexture(InstanceData->ScalarAtlasRHI);
            if (!ScalarRDG)
            {
                ScalarRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InstanceData->ScalarAtlasRHI, TEXT("FieldScalarAtlas")));
                GraphBuilder.UseInternalAccessMode(ScalarRDG);
                Context.GetRDGExternalAccessQueue().Add(ScalarRDG);
            }
            Params->ScalarAtlas = ScalarRDG;
        }

        if (Context.IsResourceBound(&Params->VectorAtlas))
        {
            FRDGTextureRef VectorRDG = GraphBuilder.FindExternalTexture(InstanceData->VectorAtlasRHI);
            if (!VectorRDG)
            {
                VectorRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InstanceData->VectorAtlasRHI, TEXT("FieldVectorAtlas")));
                GraphBuilder.UseInternalAccessMode(VectorRDG);
                Context.GetRDGExternalAccessQueue().Add(VectorRDG);
            }
            Params->VectorAtlas = VectorRDG;
        }
    }
    else
    {
        Params->FieldResolution = 0;
        Params->TilesPerRow = 1;
        Params->DomainMinCm = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
        Params->DomainMaxCm = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
        Params->ScalarAtlasSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI();
        Params->VectorAtlasSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI();

        // Provide fallback black textures to prevent GPU crash.
        if (Context.IsResourceBound(&Params->ScalarAtlas))
        {
            Params->ScalarAtlas = Context.GetComputeDispatchInterface().GetBlackTexture(GraphBuilder, ETextureDimension::Texture2D);
        }
        if (Context.IsResourceBound(&Params->VectorAtlas))
        {
            Params->VectorAtlas = Context.GetComputeDispatchInterface().GetBlackTexture(GraphBuilder, ETextureDimension::Texture2D);
        }
    }
}

#undef LOCTEXT_NAMESPACE
