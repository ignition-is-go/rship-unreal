#include "RshipFieldShaders.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

namespace
{
constexpr uint32 FieldGroupSizeX = 4;
constexpr uint32 FieldGroupSizeY = 4;
constexpr uint32 FieldGroupSizeZ = 4;
constexpr uint32 TargetGroupSizeX = 8;
constexpr uint32 TargetGroupSizeY = 8;

class FRshipFieldBuildGlobalCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FRshipFieldBuildGlobalCS);
    SHADER_USE_PARAMETER_STRUCT(FRshipFieldBuildGlobalCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(int32, FieldResolution)
        SHADER_PARAMETER(int32, TilesPerRow)
        SHADER_PARAMETER(float, TimeSeconds)
        SHADER_PARAMETER(float, BPM)
        SHADER_PARAMETER(float, TransportPhase)
        SHADER_PARAMETER(float, MasterScalarGain)
        SHADER_PARAMETER(float, MasterVectorGain)
        SHADER_PARAMETER(FVector4f, DomainMinCm)
        SHADER_PARAMETER(FVector4f, DomainMaxCm)
        SHADER_PARAMETER(uint32, LayerCount)
        SHADER_PARAMETER(uint32, SyncGroupCount)
        SHADER_PARAMETER(uint32, EffectorCount)
        SHADER_PARAMETER(int32, DebugMode)
        SHADER_PARAMETER(int32, DebugSelectionIndex)

        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, LayerDataA)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, LayerDataB)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, SyncGroupData)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData0)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData1)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData2)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData3)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData4)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData5)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData6)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData7)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, WavefrontData)

        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutScalarFieldAtlasTex)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutVectorFieldAtlasTex)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("FIELD_GROUP_SIZE_X"), FieldGroupSizeX);
        OutEnvironment.SetDefine(TEXT("FIELD_GROUP_SIZE_Y"), FieldGroupSizeY);
        OutEnvironment.SetDefine(TEXT("FIELD_GROUP_SIZE_Z"), FieldGroupSizeZ);
    }
};

class FRshipFieldAccumulateInfiniteCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FRshipFieldAccumulateInfiniteCS);
    SHADER_USE_PARAMETER_STRUCT(FRshipFieldAccumulateInfiniteCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(int32, FieldResolution)
        SHADER_PARAMETER(int32, TilesPerRow)
        SHADER_PARAMETER(float, TimeSeconds)
        SHADER_PARAMETER(float, BPM)
        SHADER_PARAMETER(float, TransportPhase)
        SHADER_PARAMETER(float, MasterScalarGain)
        SHADER_PARAMETER(float, MasterVectorGain)
        SHADER_PARAMETER(FVector4f, DomainMinCm)
        SHADER_PARAMETER(FVector4f, DomainMaxCm)
        SHADER_PARAMETER(uint32, LayerCount)
        SHADER_PARAMETER(uint32, SyncGroupCount)
        SHADER_PARAMETER(uint32, EffectorCount)
        SHADER_PARAMETER(int32, DebugMode)
        SHADER_PARAMETER(int32, DebugSelectionIndex)

        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, LayerDataA)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, LayerDataB)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, SyncGroupData)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData0)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData1)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData2)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData3)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData4)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData5)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData6)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EffectorData7)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, WavefrontData)

        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutScalarFieldAtlasTex)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutVectorFieldAtlasTex)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("FIELD_GROUP_SIZE_X"), FieldGroupSizeX);
        OutEnvironment.SetDefine(TEXT("FIELD_GROUP_SIZE_Y"), FieldGroupSizeY);
        OutEnvironment.SetDefine(TEXT("FIELD_GROUP_SIZE_Z"), FieldGroupSizeZ);
    }
};

class FRshipFieldSampleDeformCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FRshipFieldSampleDeformCS);
    SHADER_USE_PARAMETER_STRUCT(FRshipFieldSampleDeformCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(FIntPoint, GridSize)
        SHADER_PARAMETER(int32, FieldResolution)
        SHADER_PARAMETER(int32, TilesPerRow)
        SHADER_PARAMETER(float, TimeSeconds)
        SHADER_PARAMETER(float, ScalarGain)
        SHADER_PARAMETER(float, VectorGain)
        SHADER_PARAMETER(float, AxisWeightNormal)
        SHADER_PARAMETER(float, AxisWeightWorld)
        SHADER_PARAMETER(float, AxisWeightObject)
        SHADER_PARAMETER(float, MaxDisplacementCm)
        SHADER_PARAMETER(FVector4f, DomainMinCm)
        SHADER_PARAMETER(FVector4f, DomainMaxCm)
        SHADER_PARAMETER(FVector4f, WorldAxis)
        SHADER_PARAMETER(FVector4f, ObjectAxis)

        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RestPositionTex)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RestNormalTex)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, MaskTex)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ScalarFieldAtlasTex)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, VectorFieldAtlasTex)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutDeformedPositionTex)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("TARGET_GROUP_SIZE_X"), TargetGroupSizeX);
        OutEnvironment.SetDefine(TEXT("TARGET_GROUP_SIZE_Y"), TargetGroupSizeY);
    }
};

class FRshipFieldRecomputeNormalsCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FRshipFieldRecomputeNormalsCS);
    SHADER_USE_PARAMETER_STRUCT(FRshipFieldRecomputeNormalsCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(FIntPoint, GridSize)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DeformedPositionTex)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutDeformedNormalTex)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("TARGET_GROUP_SIZE_X"), TargetGroupSizeX);
        OutEnvironment.SetDefine(TEXT("TARGET_GROUP_SIZE_Y"), TargetGroupSizeY);
    }
};

class FRshipFieldDebugViewCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FRshipFieldDebugViewCS);
    SHADER_USE_PARAMETER_STRUCT(FRshipFieldDebugViewCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(int32, FieldResolution)
        SHADER_PARAMETER(int32, TilesPerRow)
        SHADER_PARAMETER(uint32, DebugMode)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, VectorFieldAtlasTex)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutScalarFieldAtlasTex)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("TARGET_GROUP_SIZE_X"), TargetGroupSizeX);
        OutEnvironment.SetDefine(TEXT("TARGET_GROUP_SIZE_Y"), TargetGroupSizeY);
    }
};
class FRshipFieldSampleAtPointsCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FRshipFieldSampleAtPointsCS);
    SHADER_USE_PARAMETER_STRUCT(FRshipFieldSampleAtPointsCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(int32, FieldResolution)
        SHADER_PARAMETER(int32, TilesPerRow)
        SHADER_PARAMETER(FVector4f, DomainMinCm)
        SHADER_PARAMETER(FVector4f, DomainMaxCm)
        SHADER_PARAMETER(uint32, NumSamples)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ScalarFieldAtlasTex)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, VectorFieldAtlasTex)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, SamplePositions)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutSampleResults)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};
} // namespace

IMPLEMENT_GLOBAL_SHADER(FRshipFieldBuildGlobalCS, "/Plugin/RshipField/Private/RshipFieldCS.usf", "BuildGlobalFieldCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FRshipFieldAccumulateInfiniteCS, "/Plugin/RshipField/Private/RshipFieldCS.usf", "AccumulateInfiniteCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FRshipFieldSampleDeformCS, "/Plugin/RshipField/Private/RshipFieldCS.usf", "SampleAndDeformTargetCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FRshipFieldRecomputeNormalsCS, "/Plugin/RshipField/Private/RshipFieldCS.usf", "RecomputeNormalsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FRshipFieldDebugViewCS, "/Plugin/RshipField/Private/RshipFieldCS.usf", "DebugViewCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FRshipFieldSampleAtPointsCS, "/Plugin/RshipField/Private/RshipFieldCS.usf", "SampleFieldAtPointsCS", SF_Compute);

void RshipFieldRDG::AddFieldPasses(
    FRDGBuilder& GraphBuilder,
    const FGlobalDispatchInputs& GlobalInputs,
    const TArray<FTargetDispatchInputs>& TargetInputs)
{
    if (!GlobalInputs.IsValid())
    {
        return;
    }

    FRDGTextureRef ScalarAtlasTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(GlobalInputs.OutScalarFieldAtlasTexture, TEXT("RshipField.ScalarAtlas")));
    FRDGTextureRef VectorAtlasTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(GlobalInputs.OutVectorFieldAtlasTexture, TEXT("RshipField.VectorAtlas")));

    auto MakePackedBufferSRV = [&GraphBuilder](const TCHAR* Name, const TArray<FVector4f>& Source) -> FRDGBufferSRVRef
    {
        TArray<FVector4f> SafeData = Source;
        if (SafeData.Num() == 0)
        {
            SafeData.Add(FVector4f::Zero());
        }

        FRDGBufferRef Buffer = CreateStructuredBuffer(
            GraphBuilder,
            Name,
            TConstArrayView<FVector4f>(SafeData));
        return GraphBuilder.CreateSRV(Buffer);
    };

    FRDGBufferSRVRef LayerDataASRV = MakePackedBufferSRV(TEXT("RshipField.LayerDataA"), GlobalInputs.LayerDataA);
    FRDGBufferSRVRef LayerDataBSRV = MakePackedBufferSRV(TEXT("RshipField.LayerDataB"), GlobalInputs.LayerDataB);
    FRDGBufferSRVRef SyncGroupDataSRV = MakePackedBufferSRV(TEXT("RshipField.SyncGroupData"), GlobalInputs.SyncGroupData);

    FRDGBufferSRVRef EffectorData0SRV = MakePackedBufferSRV(TEXT("RshipField.EffectorData0"), GlobalInputs.EffectorData0);
    FRDGBufferSRVRef EffectorData1SRV = MakePackedBufferSRV(TEXT("RshipField.EffectorData1"), GlobalInputs.EffectorData1);
    FRDGBufferSRVRef EffectorData2SRV = MakePackedBufferSRV(TEXT("RshipField.EffectorData2"), GlobalInputs.EffectorData2);
    FRDGBufferSRVRef EffectorData3SRV = MakePackedBufferSRV(TEXT("RshipField.EffectorData3"), GlobalInputs.EffectorData3);
    FRDGBufferSRVRef EffectorData4SRV = MakePackedBufferSRV(TEXT("RshipField.EffectorData4"), GlobalInputs.EffectorData4);
    FRDGBufferSRVRef EffectorData5SRV = MakePackedBufferSRV(TEXT("RshipField.EffectorData5"), GlobalInputs.EffectorData5);
    FRDGBufferSRVRef EffectorData6SRV = MakePackedBufferSRV(TEXT("RshipField.EffectorData6"), GlobalInputs.EffectorData6);
    FRDGBufferSRVRef EffectorData7SRV = MakePackedBufferSRV(TEXT("RshipField.EffectorData7"), GlobalInputs.EffectorData7);
    FRDGBufferSRVRef WavefrontDataSRV = MakePackedBufferSRV(TEXT("RshipField.WavefrontData"), GlobalInputs.WavefrontData);

    const FIntVector FieldVoxelCount(GlobalInputs.FieldResolution, GlobalInputs.FieldResolution, GlobalInputs.FieldResolution);
    const FIntVector FieldGroupCount = FComputeShaderUtils::GetGroupCount(
        FieldVoxelCount,
        FIntVector(FieldGroupSizeX, FieldGroupSizeY, FieldGroupSizeZ));

    {
        TShaderMapRef<FRshipFieldBuildGlobalCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FRshipFieldBuildGlobalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRshipFieldBuildGlobalCS::FParameters>();
        PassParameters->FieldResolution = GlobalInputs.FieldResolution;
        PassParameters->TilesPerRow = GlobalInputs.TilesPerRow;
        PassParameters->TimeSeconds = GlobalInputs.TimeSeconds;
        PassParameters->BPM = GlobalInputs.BPM;
        PassParameters->TransportPhase = GlobalInputs.TransportPhase;
        PassParameters->MasterScalarGain = GlobalInputs.MasterScalarGain;
        PassParameters->MasterVectorGain = GlobalInputs.MasterVectorGain;
        PassParameters->DomainMinCm = GlobalInputs.DomainMinCm;
        PassParameters->DomainMaxCm = GlobalInputs.DomainMaxCm;
        PassParameters->LayerCount = GlobalInputs.LayerCount;
        PassParameters->SyncGroupCount = GlobalInputs.SyncGroupCount;
        PassParameters->EffectorCount = GlobalInputs.EffectorCount;
        PassParameters->DebugMode = GlobalInputs.DebugMode;
        PassParameters->DebugSelectionIndex = GlobalInputs.DebugSelectionIndex;
        PassParameters->LayerDataA = LayerDataASRV;
        PassParameters->LayerDataB = LayerDataBSRV;
        PassParameters->SyncGroupData = SyncGroupDataSRV;
        PassParameters->EffectorData0 = EffectorData0SRV;
        PassParameters->EffectorData1 = EffectorData1SRV;
        PassParameters->EffectorData2 = EffectorData2SRV;
        PassParameters->EffectorData3 = EffectorData3SRV;
        PassParameters->EffectorData4 = EffectorData4SRV;
        PassParameters->EffectorData5 = EffectorData5SRV;
        PassParameters->EffectorData6 = EffectorData6SRV;
        PassParameters->EffectorData7 = EffectorData7SRV;
        PassParameters->WavefrontData = WavefrontDataSRV;
        PassParameters->OutScalarFieldAtlasTex = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScalarAtlasTex));
        PassParameters->OutVectorFieldAtlasTex = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(VectorAtlasTex));

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("RshipField.BuildGlobalField"),
            ERDGPassFlags::Compute,
            ComputeShader,
            PassParameters,
            FieldGroupCount);
    }

    // AccumulateInfinite pass disabled — all effectors use finite range for MVP.
    // The pass was causing a UAV race condition overwriting BuildGlobalField results.

    for (const FTargetDispatchInputs& Target : TargetInputs)
    {
        if (!Target.IsValid())
        {
            continue;
        }

        FRDGTextureRef RestPositionTex = GraphBuilder.RegisterExternalTexture(
            CreateRenderTarget(Target.RestPositionTexture, TEXT("RshipField.RestPosition")));
        FRDGTextureRef RestNormalTex = GraphBuilder.RegisterExternalTexture(
            CreateRenderTarget(Target.RestNormalTexture, TEXT("RshipField.RestNormal")));
        FRDGTextureRef MaskTex = GraphBuilder.RegisterExternalTexture(
            CreateRenderTarget(Target.MaskTexture, TEXT("RshipField.Mask")));
        FRDGTextureRef OutPosTex = GraphBuilder.RegisterExternalTexture(
            CreateRenderTarget(Target.OutDeformedPositionTexture, TEXT("RshipField.OutPosition")));
        FRDGTextureRef OutNormalTex = GraphBuilder.RegisterExternalTexture(
            CreateRenderTarget(Target.OutDeformedNormalTexture, TEXT("RshipField.OutNormal")));

        const ERDGPassFlags PassFlags = Target.bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
        const FIntVector TargetGroupCount = FComputeShaderUtils::GetGroupCount(
            FIntVector(Target.GridSize.X, Target.GridSize.Y, 1),
            FIntVector(TargetGroupSizeX, TargetGroupSizeY, 1));

        {
            TShaderMapRef<FRshipFieldSampleDeformCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
            FRshipFieldSampleDeformCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRshipFieldSampleDeformCS::FParameters>();
            PassParameters->GridSize = Target.GridSize;
            PassParameters->FieldResolution = GlobalInputs.FieldResolution;
            PassParameters->TilesPerRow = GlobalInputs.TilesPerRow;
            PassParameters->TimeSeconds = Target.TimeSeconds;
            PassParameters->ScalarGain = Target.ScalarGain;
            PassParameters->VectorGain = Target.VectorGain;
            PassParameters->AxisWeightNormal = Target.AxisWeightNormal;
            PassParameters->AxisWeightWorld = Target.AxisWeightWorld;
            PassParameters->AxisWeightObject = Target.AxisWeightObject;
            PassParameters->MaxDisplacementCm = Target.MaxDisplacementCm;
            PassParameters->DomainMinCm = Target.DomainMinCm;
            PassParameters->DomainMaxCm = Target.DomainMaxCm;
            PassParameters->WorldAxis = Target.WorldAxis;
            PassParameters->ObjectAxis = Target.ObjectAxis;
            PassParameters->RestPositionTex = RestPositionTex;
            PassParameters->RestNormalTex = RestNormalTex;
            PassParameters->MaskTex = MaskTex;
            PassParameters->ScalarFieldAtlasTex = ScalarAtlasTex;
            PassParameters->VectorFieldAtlasTex = VectorAtlasTex;
            PassParameters->OutDeformedPositionTex = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutPosTex));

            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("RshipField.SampleAndDeformTarget"),
                PassFlags,
                ComputeShader,
                PassParameters,
                TargetGroupCount);
        }

        {
            TShaderMapRef<FRshipFieldRecomputeNormalsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
            FRshipFieldRecomputeNormalsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRshipFieldRecomputeNormalsCS::FParameters>();
            PassParameters->GridSize = Target.GridSize;
            PassParameters->DeformedPositionTex = OutPosTex;
            PassParameters->OutDeformedNormalTex = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutNormalTex));

            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("RshipField.RecomputeNormals"),
                PassFlags,
                ComputeShader,
                PassParameters,
                TargetGroupCount);
        }
    }

}

void RshipFieldRDG::AddPointSamplePass(
    FRDGBuilder& GraphBuilder,
    const FPointSampleInputs& Inputs)
{
    if (Inputs.NumSamples == 0 || !Inputs.ScalarAtlasTexture.IsValid() || !Inputs.VectorAtlasTexture.IsValid() || !Inputs.OutResultsTexture.IsValid())
    {
        return;
    }

    FRDGTextureRef ScalarAtlasTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(Inputs.ScalarAtlasTexture, TEXT("RshipField.ScalarAtlas.PointSample")));
    FRDGTextureRef VectorAtlasTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(Inputs.VectorAtlasTexture, TEXT("RshipField.VectorAtlas.PointSample")));
    FRDGTextureRef ResultsTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(Inputs.OutResultsTexture, TEXT("RshipField.PointSampleResults")));

    TArray<FVector4f> SafePositions = Inputs.Positions;
    if (SafePositions.Num() == 0)
    {
        SafePositions.Add(FVector4f::Zero());
    }

    FRDGBufferRef PositionBuffer = CreateStructuredBuffer(
        GraphBuilder,
        TEXT("RshipField.SamplePositions"),
        TConstArrayView<FVector4f>(SafePositions));
    FRDGBufferSRVRef PositionSRV = GraphBuilder.CreateSRV(PositionBuffer);

    TShaderMapRef<FRshipFieldSampleAtPointsCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    FRshipFieldSampleAtPointsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRshipFieldSampleAtPointsCS::FParameters>();
    PassParameters->FieldResolution = Inputs.FieldResolution;
    PassParameters->TilesPerRow = Inputs.TilesPerRow;
    PassParameters->DomainMinCm = Inputs.DomainMinCm;
    PassParameters->DomainMaxCm = Inputs.DomainMaxCm;
    PassParameters->NumSamples = Inputs.NumSamples;
    PassParameters->ScalarFieldAtlasTex = ScalarAtlasTex;
    PassParameters->VectorFieldAtlasTex = VectorAtlasTex;
    PassParameters->SamplePositions = PositionSRV;
    PassParameters->OutSampleResults = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ResultsTex));

    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Inputs.NumSamples, 1, 1), FIntVector(64, 1, 1));

    FComputeShaderUtils::AddPass(
        GraphBuilder,
        RDG_EVENT_NAME("RshipField.SampleAtPoints"),
        ERDGPassFlags::Compute,
        ComputeShader,
        PassParameters,
        GroupCount);
}
