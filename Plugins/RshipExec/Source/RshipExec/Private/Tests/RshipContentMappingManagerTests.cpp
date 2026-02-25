#include "RshipContentMappingManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    const FRshipRenderContextState* FindContextById(const TArray<FRshipRenderContextState>& Contexts, const FString& Id)
    {
        for (const FRshipRenderContextState& Context : Contexts)
        {
            if (Context.Id == Id)
            {
                return &Context;
            }
        }
        return nullptr;
    }

    const FRshipContentMappingState* FindMappingById(const TArray<FRshipContentMappingState>& Mappings, const FString& Id)
    {
        for (const FRshipContentMappingState& Mapping : Mappings)
        {
            if (Mapping.Id == Id)
            {
                return &Mapping;
            }
        }
        return nullptr;
    }

#if WITH_EDITOR
    UMaterial* CreateProjectionProbeMaterial()
    {
        UMaterial* Mat = NewObject<UMaterial>();
        if (!Mat)
        {
            return nullptr;
        }

        Mat->MaterialDomain = EMaterialDomain::MD_Surface;
        Mat->BlendMode = BLEND_Opaque;
        Mat->SetShadingModel(MSM_Unlit);

        UMaterialExpressionScalarParameter* ProjectionTypeParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
        ProjectionTypeParam->ParameterName = FName(TEXT("RshipProjectionType"));
        ProjectionTypeParam->DefaultValue = 0.0f;
        Mat->GetExpressionCollection().AddExpression(ProjectionTypeParam);

        Mat->GetEditorOnlyData()->EmissiveColor.Expression = ProjectionTypeParam;
        Mat->GetEditorOnlyData()->EmissiveColor.OutputIndex = 0;

        Mat->PreEditChange(nullptr);
        Mat->PostEditChange();
        return Mat;
    }
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingMaterialContractValidationTest,
    "Rship.ContentMapping.Contract.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingMaterialContractValidationTest::RunTest(const FString& Parameters)
{
    URshipContentMappingManager* Manager = NewObject<URshipContentMappingManager>();
    TestNotNull(TEXT("Manager should be created"), Manager);

    UMaterial* EmptyMaterial = NewObject<UMaterial>();
    TestNotNull(TEXT("Empty material should be created"), EmptyMaterial);

    FString Error;
    const bool bValid = Manager->ValidateMaterialContractForTest(EmptyMaterial, Error);
    TestFalse(TEXT("Empty material should fail mapping contract validation"), bValid);
    TestTrue(TEXT("Validation error should mention missing context texture"), Error.Contains(TEXT("RshipContextTexture")));
    TestTrue(TEXT("Validation error should mention missing projection type"), Error.Contains(TEXT("RshipProjectionType")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingProjectionTypeRoutingTest,
    "Rship.ContentMapping.Parameters.ProjectionTypeRouting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingProjectionTypeRoutingTest::RunTest(const FString& Parameters)
{
#if !WITH_EDITOR
    AddWarning(TEXT("Projection routing test requires WITH_EDITOR material construction."));
    return true;
#else
    URshipContentMappingManager* Manager = NewObject<URshipContentMappingManager>();
    TestNotNull(TEXT("Manager should be created"), Manager);

    UMaterial* ProbeMaterial = CreateProjectionProbeMaterial();
    TestNotNull(TEXT("Probe material should be created"), ProbeMaterial);
    if (!ProbeMaterial)
    {
        return false;
    }

    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ProbeMaterial, Manager);
    TestNotNull(TEXT("MID should be created"), MID);
    if (!MID)
    {
        return false;
    }

    FRshipMappingSurfaceState SurfaceState;
    SurfaceState.Id = TEXT("surface-1");
    SurfaceState.UVChannel = 0;

    struct FProjectionExpectation
    {
        FString MappingType;
        FString ProjectionType;
        float ExpectedIndex;
    };

    const TArray<FProjectionExpectation> Cases =
    {
        {TEXT("perspective"), TEXT("perspective"), 0.0f},
        {TEXT("cylindrical"), TEXT("cylindrical"), 1.0f},
        {TEXT("surface-projection"), TEXT("planar"), 2.0f},
        {TEXT("spherical"), TEXT("spherical"), 3.0f},
        {TEXT("parallel"), TEXT("parallel"), 4.0f},
        {TEXT("radial"), TEXT("radial"), 5.0f},
        {TEXT("mesh"), TEXT("mesh"), 6.0f},
        {TEXT("fisheye"), TEXT("fisheye"), 7.0f},
        {TEXT("custom-matrix"), TEXT("custom-matrix"), 8.0f},
        {TEXT("camera-plate"), TEXT("camera-plate"), 9.0f},
        {TEXT("spatial"), TEXT("spatial"), 10.0f},
        {TEXT("depth-map"), TEXT("depth-map"), 11.0f}
    };

    for (const FProjectionExpectation& Case : Cases)
    {
        FRshipContentMappingState MappingState;
        MappingState.Type = Case.MappingType;
        MappingState.bEnabled = true;
        MappingState.Opacity = 1.0f;
        MappingState.Config = MakeShared<FJsonObject>();
        MappingState.Config->SetStringField(TEXT("projectionType"), Case.ProjectionType);

        Manager->ApplyMaterialParametersForTest(MID, MappingState, SurfaceState, nullptr);

        const float ActualIndex = MID->K2_GetScalarParameterValue(FName(TEXT("RshipProjectionType")));
        const bool bMatches = FMath::IsNearlyEqual(ActualIndex, Case.ExpectedIndex, KINDA_SMALL_NUMBER);
        if (!bMatches)
        {
            AddError(FString::Printf(
                TEXT("Projection routing mismatch for '%s'/'%s': expected %.1f, got %.3f"),
                *Case.MappingType,
                *Case.ProjectionType,
                Case.ExpectedIndex,
                ActualIndex));
        }
    }

    return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingDepthContextRoundTripTest,
    "Rship.ContentMapping.Context.DepthRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingDepthContextRoundTripTest::RunTest(const FString& Parameters)
{
    URshipContentMappingManager* Manager = NewObject<URshipContentMappingManager>();
    TestNotNull(TEXT("Manager should be created"), Manager);

    FRshipRenderContextState InContext;
    InContext.Name = TEXT("DepthContext");
    InContext.SourceType = TEXT("asset-store");
    InContext.AssetId = TEXT("color-asset-id");
    InContext.DepthAssetId = TEXT("depth-asset-id");
    InContext.DepthCaptureMode = TEXT("");
    InContext.bDepthCaptureEnabled = true;
    InContext.Width = 0;
    InContext.Height = 0;

    const FString ContextId = Manager->CreateRenderContext(InContext);
    TestTrue(TEXT("CreateRenderContext should return a context id"), !ContextId.IsEmpty());

    const TArray<FRshipRenderContextState> FirstContexts = Manager->GetRenderContexts();
    const FRshipRenderContextState* Stored = FindContextById(FirstContexts, ContextId);
    TestNotNull(TEXT("Stored context should exist"), Stored);
    if (!Stored)
    {
        return false;
    }

    TestEqual(TEXT("Width should normalize to 1920"), Stored->Width, 1920);
    TestEqual(TEXT("Height should normalize to 1080"), Stored->Height, 1080);
    TestEqual(TEXT("Depth capture mode should normalize to SceneDepth"), Stored->DepthCaptureMode, FString(TEXT("SceneDepth")));
    TestEqual(TEXT("Depth asset id should be preserved"), Stored->DepthAssetId, FString(TEXT("depth-asset-id")));
    TestTrue(TEXT("Depth capture enabled should be preserved"), Stored->bDepthCaptureEnabled);

    TSharedPtr<FJsonObject> Serialized = Manager->BuildRenderContextJsonForTest(*Stored);
    TestTrue(TEXT("Serialized context should be valid"), Serialized.IsValid());
    if (!Serialized.IsValid())
    {
        return false;
    }

    TestEqual(TEXT("Serialized depthAssetId should match"), Serialized->GetStringField(TEXT("depthAssetId")), FString(TEXT("depth-asset-id")));
    TestEqual(TEXT("Serialized depthCaptureMode should match"), Serialized->GetStringField(TEXT("depthCaptureMode")), FString(TEXT("SceneDepth")));

    bool bDepthCaptureEnabled = false;
    TestTrue(TEXT("Serialized depthCaptureEnabled should exist"), Serialized->TryGetBoolField(TEXT("depthCaptureEnabled"), bDepthCaptureEnabled));
    TestTrue(TEXT("Serialized depthCaptureEnabled should be true"), bDepthCaptureEnabled);

    Manager->ProcessRenderContextEvent(Serialized, false);

    const TArray<FRshipRenderContextState> RoundTrippedContexts = Manager->GetRenderContexts();
    const FRshipRenderContextState* RoundTripped = FindContextById(RoundTrippedContexts, ContextId);
    TestNotNull(TEXT("Round-tripped context should exist"), RoundTripped);
    if (!RoundTripped)
    {
        return false;
    }

    TestEqual(TEXT("Round-tripped depth capture mode should remain SceneDepth"), RoundTripped->DepthCaptureMode, FString(TEXT("SceneDepth")));
    TestEqual(TEXT("Round-tripped depth asset id should remain set"), RoundTripped->DepthAssetId, FString(TEXT("depth-asset-id")));
    TestTrue(TEXT("Round-tripped depth capture enabled should remain true"), RoundTripped->bDepthCaptureEnabled);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingDeleteTombstoneGuardsStaleUpsertTest,
    "Rship.ContentMapping.Mapping.DeleteTombstoneGuardsStaleUpsert",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingDeleteTombstoneGuardsStaleUpsertTest::RunTest(const FString& Parameters)
{
    URshipContentMappingManager* Manager = NewObject<URshipContentMappingManager>();
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager)
    {
        return false;
    }

    FRshipContentMappingState Mapping;
    Mapping.Name = TEXT("DeleteGuardProbe");
    Mapping.Type = TEXT("surface-uv");
    Mapping.bEnabled = true;
    Mapping.Opacity = 1.0f;
    Mapping.Config = MakeShared<FJsonObject>();
    Mapping.Config->SetStringField(TEXT("uvMode"), TEXT("direct"));

    const FString MappingId = Manager->CreateMapping(Mapping);
    TestTrue(TEXT("CreateMapping should return an id"), !MappingId.IsEmpty());
    if (MappingId.IsEmpty())
    {
        return false;
    }

    TestTrue(TEXT("DeleteMapping should succeed"), Manager->DeleteMapping(MappingId));

    // Simulate delete echo after local delete.
    TSharedPtr<FJsonObject> DeleteEcho = MakeShared<FJsonObject>();
    DeleteEcho->SetStringField(TEXT("id"), MappingId);
    Manager->ProcessMappingEvent(DeleteEcho, true);

    // Simulate a stale upsert arriving out-of-order after delete.
    TSharedPtr<FJsonObject> StaleUpsert = MakeShared<FJsonObject>();
    StaleUpsert->SetStringField(TEXT("id"), MappingId);
    StaleUpsert->SetStringField(TEXT("name"), TEXT("StaleRecreate"));
    StaleUpsert->SetStringField(TEXT("type"), TEXT("direct"));
    StaleUpsert->SetBoolField(TEXT("enabled"), true);
    StaleUpsert->SetNumberField(TEXT("opacity"), 1.0);
    TSharedPtr<FJsonObject> StaleConfig = MakeShared<FJsonObject>();
    StaleConfig->SetStringField(TEXT("uvMode"), TEXT("direct"));
    StaleUpsert->SetObjectField(TEXT("config"), StaleConfig);
    Manager->ProcessMappingEvent(StaleUpsert, false);

    const TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
    TestNull(TEXT("Stale upsert should be ignored while delete tombstone is active"), FindMappingById(Mappings, MappingId));

    return true;
}

#endif
