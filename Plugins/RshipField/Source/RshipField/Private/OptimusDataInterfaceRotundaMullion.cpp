#include "OptimusDataInterfaceRotundaMullion.h"

#include "RotundaMullionComponent.h"

#include "Components/SceneComponent.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusDataInterfaceRotundaMullion)

// Shader parameters — must match the HLSL template uniforms
BEGIN_SHADER_PARAMETER_STRUCT(FOptimusRotundaMullionParameters, )
    SHADER_PARAMETER(int32, MullionCount)
    SHADER_PARAMETER(int32, AtomCount)
    SHADER_PARAMETER(int32, LayerCount)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FMullionData>, MullionBuffer)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FAtomData>, AtomBuffer)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FMullionControlLayer>, ControlLayerBuffer)
END_SHADER_PARAMETER_STRUCT()

TCHAR const* UOptimusRotundaMullionDataInterface::TemplateFilePath = TEXT("/Plugin/RshipField/Private/DataInterfaceRotundaMullion.ush");

// ---------------------------------------------------------------------------
// Data Interface
// ---------------------------------------------------------------------------

FString UOptimusRotundaMullionDataInterface::GetDisplayName() const
{
    return TEXT("Rotunda Mullion");
}

TArray<FOptimusCDIPinDefinition> UOptimusRotundaMullionDataInterface::GetPinDefinitions() const
{
    TArray<FOptimusCDIPinDefinition> Defs;
    Defs.Add({"Buffers", "ReadMullionCount"});
    return Defs;
}

TSubclassOf<UActorComponent> UOptimusRotundaMullionDataInterface::GetRequiredComponentClass() const
{
    return USceneComponent::StaticClass();
}

void UOptimusRotundaMullionDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
    OutFunctions.AddDefaulted_GetRef()
        .SetName(TEXT("ReadMullionCount"))
        .AddReturnType(EShaderFundamentalType::Int);
}

void UOptimusRotundaMullionDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
    InOutBuilder.AddNestedStruct<FOptimusRotundaMullionParameters>(UID);
}

TCHAR const* UOptimusRotundaMullionDataInterface::GetShaderVirtualPath() const
{
    return TemplateFilePath;
}

void UOptimusRotundaMullionDataInterface::GetShaderHash(FString& InOutKey) const
{
    GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusRotundaMullionDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
    TMap<FString, FStringFormatArg> TemplateArgs =
    {
        { TEXT("DataInterfaceName"), InDataInterfaceName },
    };

    FString TemplateFile;
    LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
    OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusRotundaMullionDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
    UOptimusRotundaMullionDataProvider* Provider = NewObject<UOptimusRotundaMullionDataProvider>();
    Provider->SceneComponent = Cast<USceneComponent>(InBinding);
    return Provider;
}

// ---------------------------------------------------------------------------
// Data Provider
// ---------------------------------------------------------------------------

FComputeDataProviderRenderProxy* UOptimusRotundaMullionDataProvider::GetRenderProxy()
{
    return new FOptimusRotundaMullionProviderProxy(SceneComponent.Get());
}

// ---------------------------------------------------------------------------
// Render Proxy
// ---------------------------------------------------------------------------

FOptimusRotundaMullionProviderProxy::FOptimusRotundaMullionProviderProxy(USceneComponent* InSceneComponent)
{
    bIsValid = false;

    if (!InSceneComponent)
    {
        return;
    }

    // Find URotundaMullionComponent on the same actor
    AActor* Owner = InSceneComponent->GetOwner();
    if (!Owner)
    {
        return;
    }

    URotundaMullionComponent* MullionComp = Owner->FindComponentByClass<URotundaMullionComponent>();
    if (!MullionComp)
    {
        return;
    }

    // Copy data for render-thread use
    MullionDataCopy = MullionComp->GetMullionData();
    AtomDataCopy = MullionComp->GetAtomData();
    ControlLayersCopy = MullionComp->GetControlLayers();

    MullionCount = MullionDataCopy.Num();
    AtomCount = AtomDataCopy.Num();

    // Need at least one entry in each buffer for a valid SRV
    bIsValid = MullionCount > 0 && AtomCount > 0 && ControlLayersCopy.Num() == MULLION_MAX_LAYERS;
}

bool FOptimusRotundaMullionProviderProxy::IsValid(FValidationData const& InValidationData) const
{
    if (InValidationData.ParameterStructSize != sizeof(FParameters))
    {
        return false;
    }
    return bIsValid;
}

void FOptimusRotundaMullionProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
    MullionBufferSRV = nullptr;
    AtomBufferSRV = nullptr;
    ControlLayerBufferSRV = nullptr;

    if (!bIsValid)
    {
        return;
    }

    // Mullion data (static)
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FMullionData), MullionCount);
        FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, TEXT("RotundaMullionData"));
        GraphBuilder.QueueBufferUpload(Buffer, MullionDataCopy.GetData(), MullionCount * sizeof(FMullionData), ERDGInitialDataFlags::NoCopy);
        MullionBufferSRV = GraphBuilder.CreateSRV(Buffer);
    }

    // Atom data (static)
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FAtomData), AtomCount);
        FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, TEXT("RotundaAtomData"));
        GraphBuilder.QueueBufferUpload(Buffer, AtomDataCopy.GetData(), AtomCount * sizeof(FAtomData), ERDGInitialDataFlags::NoCopy);
        AtomBufferSRV = GraphBuilder.CreateSRV(Buffer);
    }

    // Control layers (runtime)
    {
        FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FMullionControlLayer), MULLION_MAX_LAYERS);
        FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, TEXT("RotundaControlLayers"));
        GraphBuilder.QueueBufferUpload(Buffer, ControlLayersCopy.GetData(), MULLION_MAX_LAYERS * sizeof(FMullionControlLayer), ERDGInitialDataFlags::NoCopy);
        ControlLayerBufferSRV = GraphBuilder.CreateSRV(Buffer);
    }
}

void FOptimusRotundaMullionProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
    const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
    for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
    {
        FParameters& Parameters = ParameterArray[InvocationIndex];
        Parameters.MullionCount = MullionCount;
        Parameters.AtomCount = AtomCount;
        Parameters.LayerCount = MULLION_MAX_LAYERS;
        Parameters.MullionBuffer = MullionBufferSRV;
        Parameters.AtomBuffer = AtomBufferSRV;
        Parameters.ControlLayerBuffer = ControlLayerBufferSRV;
    }
}
