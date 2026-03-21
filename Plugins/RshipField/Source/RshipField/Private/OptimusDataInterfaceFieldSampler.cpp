#include "OptimusDataInterfaceFieldSampler.h"

#include "RshipFieldComponent.h"
#include "RshipFieldSubsystem.h"

#include "Components/SceneComponent.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusDataInterfaceFieldSampler)

// Shader parameters — must match the HLSL template
BEGIN_SHADER_PARAMETER_STRUCT(FOptimusFieldSamplerParameters, )
    SHADER_PARAMETER(int32, FieldResolution)
    SHADER_PARAMETER(int32, TilesPerRow)
    SHADER_PARAMETER(FVector4f, DomainMinCm)
    SHADER_PARAMETER(FVector4f, DomainMaxCm)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScalarAtlas)
    SHADER_PARAMETER_SAMPLER(SamplerState, ScalarAtlasSampler)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VectorAtlas)
    SHADER_PARAMETER_SAMPLER(SamplerState, VectorAtlasSampler)
END_SHADER_PARAMETER_STRUCT()

TCHAR const* UOptimusFieldSamplerDataInterface::TemplateFilePath = TEXT("/Plugin/RshipField/Private/DataInterfaceFieldSampler.ush");

FString UOptimusFieldSamplerDataInterface::GetDisplayName() const
{
    return TEXT("Rship Field Sampler — SampleFieldScalar(float3), SampleFieldVector(float3)");
}

TArray<FOptimusCDIPinDefinition> UOptimusFieldSamplerDataInterface::GetPinDefinitions() const
{
    TArray<FOptimusCDIPinDefinition> Defs;
    Defs.Add({"Field", "ReadFieldResolution"});
    return Defs;
}

TSubclassOf<UActorComponent> UOptimusFieldSamplerDataInterface::GetRequiredComponentClass() const
{
    return USceneComponent::StaticClass();
}

void UOptimusFieldSamplerDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
    OutFunctions.AddDefaulted_GetRef()
        .SetName(TEXT("ReadFieldResolution"))
        .AddReturnType(EShaderFundamentalType::Int);
}

void UOptimusFieldSamplerDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
    InOutBuilder.AddNestedStruct<FOptimusFieldSamplerParameters>(UID);
}

TCHAR const* UOptimusFieldSamplerDataInterface::GetShaderVirtualPath() const
{
    return TemplateFilePath;
}

void UOptimusFieldSamplerDataInterface::GetShaderHash(FString& InOutKey) const
{
    GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusFieldSamplerDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
    TMap<FString, FStringFormatArg> TemplateArgs =
    {
        { TEXT("DataInterfaceName"), InDataInterfaceName },
    };

    FString TemplateFile;
    LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
    OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusFieldSamplerDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
    UOptimusFieldSamplerDataProvider* Provider = NewObject<UOptimusFieldSamplerDataProvider>();
    Provider->SceneComponent = Cast<USceneComponent>(InBinding);
    Provider->FieldId = FieldId;
    return Provider;
}

FComputeDataProviderRenderProxy* UOptimusFieldSamplerDataProvider::GetRenderProxy()
{
    return new FOptimusFieldSamplerProviderProxy(SceneComponent.Get(), FieldId);
}

FOptimusFieldSamplerProviderProxy::FOptimusFieldSamplerProviderProxy(USceneComponent* InSceneComponent, const FString& InFieldId)
{
    bIsValid = false;

    if (!InSceneComponent)
    {
        return;
    }

    UWorld* World = InSceneComponent->GetWorld();
    if (!World)
    {
        return;
    }

    URshipFieldSubsystem* Subsystem = World->GetSubsystem<URshipFieldSubsystem>();
    if (!Subsystem)
    {
        return;
    }

    URshipFieldComponent* Field = Subsystem->FindFieldById(InFieldId);
    if (!Field)
    {
        return;
    }

    UTextureRenderTarget2D* ScalarRT = Field->GetScalarAtlas();
    UTextureRenderTarget2D* VectorRT = Field->GetVectorAtlas();
    if (!ScalarRT || !VectorRT)
    {
        return;
    }

    FTextureRenderTargetResource* ScalarResource = ScalarRT->GameThread_GetRenderTargetResource();
    FTextureRenderTargetResource* VectorResource = VectorRT->GameThread_GetRenderTargetResource();
    if (!ScalarResource || !VectorResource)
    {
        return;
    }

    FieldResolution = GetFieldResolutionValue(Field->FieldResolution);
    TilesPerRow = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(FieldResolution))));
    const FVector DomainHalfExtent = FVector(Field->DomainSizeCm * 0.5f);
    DomainMinCm = FVector4f(FVector3f(Field->DomainCenterCm - DomainHalfExtent), 0.0f);
    DomainMaxCm = FVector4f(FVector3f(Field->DomainCenterCm + DomainHalfExtent), 0.0f);
    ScalarAtlasRHI = ScalarResource->GetRenderTargetTexture();
    VectorAtlasRHI = VectorResource->GetRenderTargetTexture();

    bIsValid = ScalarAtlasRHI.IsValid() && VectorAtlasRHI.IsValid();
}

bool FOptimusFieldSamplerProviderProxy::IsValid(FValidationData const& InValidationData) const
{
    if (InValidationData.ParameterStructSize != sizeof(FParameters))
    {
        return false;
    }
    return bIsValid;
}

void FOptimusFieldSamplerProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
    ScalarAtlasRDG = nullptr;
    VectorAtlasRDG = nullptr;

    if (!bIsValid)
    {
        return;
    }

    if (ScalarAtlasRHI.IsValid())
    {
        ScalarAtlasRDG = GraphBuilder.FindExternalTexture(ScalarAtlasRHI);
        if (!ScalarAtlasRDG)
        {
            ScalarAtlasRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ScalarAtlasRHI, TEXT("FieldScalarAtlas")));
        }
    }

    if (VectorAtlasRHI.IsValid())
    {
        VectorAtlasRDG = GraphBuilder.FindExternalTexture(VectorAtlasRHI);
        if (!VectorAtlasRDG)
        {
            VectorAtlasRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(VectorAtlasRHI, TEXT("FieldVectorAtlas")));
        }
    }
}

void FOptimusFieldSamplerProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
    const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
    for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
    {
        FParameters& Parameters = ParameterArray[InvocationIndex];
        Parameters.FieldResolution = FieldResolution;
        Parameters.TilesPerRow = TilesPerRow;
        Parameters.DomainMinCm = DomainMinCm;
        Parameters.DomainMaxCm = DomainMaxCm;
        Parameters.ScalarAtlasSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI();
        Parameters.VectorAtlasSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI();
        Parameters.ScalarAtlas = ScalarAtlasRDG;
        Parameters.VectorAtlas = VectorAtlasRDG;
    }
}
