#include "PulseRDGShaders.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

namespace
{
constexpr uint32 PulseGroupSizeX = 8;
constexpr uint32 PulseGroupSizeY = 8;

class FPulseDeformCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FPulseDeformCS);
    SHADER_USE_PARAMETER_STRUCT(FPulseDeformCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(FIntPoint, GridSize)
        SHADER_PARAMETER(float, TimeSeconds)
        SHADER_PARAMETER(float, Speed)
        SHADER_PARAMETER(float, Amplitude)
        SHADER_PARAMETER(float, HeightFrequency)
        SHADER_PARAMETER(float, InvCenterWidth)

        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RestPositionTex)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RestNormalTex)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, MaskTex)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutDeformedPositionTex)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), PulseGroupSizeX);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), PulseGroupSizeY);
    }
};

class FPulseNormalCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FPulseNormalCS);
    SHADER_USE_PARAMETER_STRUCT(FPulseNormalCS, FGlobalShader);

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
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), PulseGroupSizeX);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), PulseGroupSizeY);
    }
};
}

IMPLEMENT_GLOBAL_SHADER(FPulseDeformCS, "/Plugin/PulseRDGDeform/Private/PulseDeformCS.usf", "MainDeformCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPulseNormalCS, "/Plugin/PulseRDGDeform/Private/PulseDeformCS.usf", "MainNormalCS", SF_Compute);

void PulseRDG::AddDeformPass(FRDGBuilder& GraphBuilder, const FDispatchInputs& Inputs)
{
    check(Inputs.IsValid());

    FRDGTextureRef RestPositionTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(Inputs.RestPositionTexture, TEXT("Pulse.RestPositionTex")));
    FRDGTextureRef RestNormalTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(Inputs.RestNormalTexture, TEXT("Pulse.RestNormalTex")));
    FRDGTextureRef MaskTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(Inputs.MaskTexture, TEXT("Pulse.MaskTex")));
    FRDGTextureRef OutDeformedPositionTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(Inputs.OutDeformedPositionTexture, TEXT("Pulse.OutDeformedPositionTex")));
    FRDGTextureRef OutDeformedNormalTex = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(Inputs.OutDeformedNormalTexture, TEXT("Pulse.OutDeformedNormalTex")));

    const ERDGPassFlags PassFlags = Inputs.bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(
        FIntVector(Inputs.GridSize.X, Inputs.GridSize.Y, 1),
        FIntVector(PulseGroupSizeX, PulseGroupSizeY, 1));

    {
        TShaderMapRef<FPulseDeformCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FPulseDeformCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPulseDeformCS::FParameters>();
        PassParameters->GridSize = Inputs.GridSize;
        PassParameters->TimeSeconds = Inputs.TimeSeconds;
        PassParameters->Speed = Inputs.Speed;
        PassParameters->Amplitude = Inputs.Amplitude;
        PassParameters->HeightFrequency = Inputs.HeightFrequency;
        PassParameters->InvCenterWidth = 1.0f / FMath::Max(Inputs.CenterWidth, KINDA_SMALL_NUMBER);
        PassParameters->RestPositionTex = RestPositionTex;
        PassParameters->RestNormalTex = RestNormalTex;
        PassParameters->MaskTex = MaskTex;
        PassParameters->OutDeformedPositionTex = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutDeformedPositionTex));

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("PulseRDG.Deform"),
            PassFlags,
            ComputeShader,
            PassParameters,
            GroupCount);
    }

    {
        TShaderMapRef<FPulseNormalCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FPulseNormalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPulseNormalCS::FParameters>();
        PassParameters->GridSize = Inputs.GridSize;
        PassParameters->DeformedPositionTex = OutDeformedPositionTex;
        PassParameters->OutDeformedNormalTex = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutDeformedNormalTex));

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("PulseRDG.RecomputeNormals"),
            PassFlags,
            ComputeShader,
            PassParameters,
            GroupCount);
    }
}
