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

    bool CopyMappingById(
        URshipContentMappingManager* Manager,
        const FString& Id,
        FRshipContentMappingState& OutMapping)
    {
        if (!Manager)
        {
            return false;
        }

        const TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
        if (const FRshipContentMappingState* Found = FindMappingById(Mappings, Id))
        {
            OutMapping = *Found;
            return true;
        }

        return false;
    }

    TSharedPtr<FJsonObject> MakeRectPx(int32 X, int32 Y, int32 W, int32 H)
    {
        TSharedPtr<FJsonObject> Rect = MakeShared<FJsonObject>();
        Rect->SetNumberField(TEXT("x"), X);
        Rect->SetNumberField(TEXT("y"), Y);
        Rect->SetNumberField(TEXT("w"), W);
        Rect->SetNumberField(TEXT("h"), H);
        return Rect;
    }

    TSharedPtr<FJsonObject> MakeFeedSource(
        const FString& SourceId,
        const FString& ContextId,
        int32 Width,
        int32 Height)
    {
        TSharedPtr<FJsonObject> Source = MakeShared<FJsonObject>();
        Source->SetStringField(TEXT("id"), SourceId);
        if (!ContextId.IsEmpty())
        {
            Source->SetStringField(TEXT("contextId"), ContextId);
        }
        Source->SetNumberField(TEXT("width"), Width);
        Source->SetNumberField(TEXT("height"), Height);
        return Source;
    }

    TSharedPtr<FJsonObject> MakeFeedDestination(
        const FString& DestinationId,
        const FString& SurfaceId,
        int32 Width,
        int32 Height)
    {
        TSharedPtr<FJsonObject> Destination = MakeShared<FJsonObject>();
        Destination->SetStringField(TEXT("id"), DestinationId);
        if (!SurfaceId.IsEmpty())
        {
            Destination->SetStringField(TEXT("surfaceId"), SurfaceId);
        }
        Destination->SetNumberField(TEXT("width"), Width);
        Destination->SetNumberField(TEXT("height"), Height);
        return Destination;
    }

    TSharedPtr<FJsonObject> MakeFeedRoute(
        const FString& RouteId,
        const FString& SourceId,
        const FString& DestinationId,
        const TSharedPtr<FJsonObject>& SourceRect,
        const TSharedPtr<FJsonObject>& DestinationRect)
    {
        TSharedPtr<FJsonObject> Route = MakeShared<FJsonObject>();
        Route->SetStringField(TEXT("id"), RouteId);
        Route->SetStringField(TEXT("sourceId"), SourceId);
        Route->SetStringField(TEXT("destinationId"), DestinationId);
        Route->SetBoolField(TEXT("enabled"), true);
        Route->SetNumberField(TEXT("opacity"), 1.0);
        if (SourceRect.IsValid())
        {
            Route->SetObjectField(TEXT("sourceRect"), SourceRect);
        }
        if (DestinationRect.IsValid())
        {
            Route->SetObjectField(TEXT("destinationRect"), DestinationRect);
        }
        return Route;
    }

    TSharedPtr<FJsonObject> MakeFeedV2(
        const TArray<TSharedPtr<FJsonValue>>& Sources,
        const TArray<TSharedPtr<FJsonValue>>& Destinations,
        const TArray<TSharedPtr<FJsonValue>>& Routes)
    {
        TSharedPtr<FJsonObject> FeedV2 = MakeShared<FJsonObject>();
        FeedV2->SetStringField(TEXT("coordinateSpace"), TEXT("pixel"));
        FeedV2->SetArrayField(TEXT("sources"), Sources);
        FeedV2->SetArrayField(TEXT("destinations"), Destinations);
        FeedV2->SetArrayField(TEXT("routes"), Routes);
        return FeedV2;
    }

    TSharedPtr<FJsonObject> BuildFeedConfig(const TSharedPtr<FJsonObject>& FeedV2)
    {
        TSharedPtr<FJsonObject> Config = MakeShared<FJsonObject>();
        Config->SetStringField(TEXT("uvMode"), TEXT("feed"));
        if (FeedV2.IsValid())
        {
            Config->SetObjectField(TEXT("feedV2"), FeedV2);
        }
        return Config;
    }

    TSharedPtr<FJsonObject> BuildFeedMappingEvent(const FString& MappingId, const TSharedPtr<FJsonObject>& FeedV2)
    {
        TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();
        Event->SetStringField(TEXT("id"), MappingId);
        Event->SetStringField(TEXT("name"), TEXT("FeedEvent"));
        Event->SetStringField(TEXT("type"), TEXT("feed"));
        Event->SetBoolField(TEXT("enabled"), true);
        Event->SetNumberField(TEXT("opacity"), 1.0);
        Event->SetObjectField(TEXT("config"), BuildFeedConfig(FeedV2));
        return Event;
    }

    const TSharedPtr<FJsonObject> GetFeedV2Config(const FRshipContentMappingState* Mapping)
    {
        if (!Mapping || !Mapping->Config.IsValid() || !Mapping->Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
        {
            return nullptr;
        }
        return Mapping->Config->GetObjectField(TEXT("feedV2"));
    }

    int32 GetFeedArrayCount(const FRshipContentMappingState* Mapping, const TCHAR* FieldName)
    {
        const TSharedPtr<FJsonObject> FeedV2 = GetFeedV2Config(Mapping);
        if (!FeedV2.IsValid() || !FeedV2->HasTypedField<EJson::Array>(FieldName))
        {
            return -1;
        }
        return FeedV2->GetArrayField(FieldName).Num();
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
        {TEXT("surface-projection"), TEXT("planar"), 4.0f},
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingFeedExplicitEmptyArraysPreservedTest,
    "Rship.ContentMapping.Feed.ExplicitEmptyArraysPreserved",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingFeedExplicitEmptyArraysPreservedTest::RunTest(const FString& Parameters)
{
    URshipContentMappingManager* Manager = NewObject<URshipContentMappingManager>();
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager)
    {
        return false;
    }

    FRshipContentMappingState Mapping;
    Mapping.Name = TEXT("FeedEmptyArrays");
    Mapping.Type = TEXT("surface-uv");
    Mapping.bEnabled = true;
    Mapping.Opacity = 1.0f;
    Mapping.Config = BuildFeedConfig(MakeFeedV2(
        TArray<TSharedPtr<FJsonValue>>(),
        TArray<TSharedPtr<FJsonValue>>(),
        TArray<TSharedPtr<FJsonValue>>()));

    const FString MappingId = Manager->CreateMapping(Mapping);
    TestTrue(TEXT("CreateMapping should return an id"), !MappingId.IsEmpty());
    if (MappingId.IsEmpty())
    {
        return false;
    }

    FRshipContentMappingState Stored;
    TestTrue(TEXT("Stored mapping should exist"), CopyMappingById(Manager, MappingId, Stored));
    if (Stored.Id.IsEmpty())
    {
        return false;
    }

    TestEqual(TEXT("sources should remain explicitly empty"), GetFeedArrayCount(&Stored, TEXT("sources")), 0);
    TestEqual(TEXT("destinations should remain explicitly empty"), GetFeedArrayCount(&Stored, TEXT("destinations")), 0);
    TestEqual(TEXT("routes should remain explicitly empty"), GetFeedArrayCount(&Stored, TEXT("routes")), 0);

    FRshipContentMappingState Updated = Stored;
    Updated.Opacity = 0.35f;
    TestTrue(TEXT("UpdateMapping should succeed"), Manager->UpdateMapping(Updated));

    FRshipContentMappingState AfterUpdate;
    TestTrue(TEXT("Mapping should still exist after update"), CopyMappingById(Manager, MappingId, AfterUpdate));
    if (AfterUpdate.Id.IsEmpty())
    {
        return false;
    }

    TestEqual(TEXT("sources should stay empty after update"), GetFeedArrayCount(&AfterUpdate, TEXT("sources")), 0);
    TestEqual(TEXT("destinations should stay empty after update"), GetFeedArrayCount(&AfterUpdate, TEXT("destinations")), 0);
    TestEqual(TEXT("routes should stay empty after update"), GetFeedArrayCount(&AfterUpdate, TEXT("routes")), 0);

    TSharedPtr<FJsonObject> StaleFeedV2 = MakeShared<FJsonObject>();
    StaleFeedV2->SetStringField(TEXT("coordinateSpace"), TEXT("pixel"));
    Manager->ProcessMappingEvent(BuildFeedMappingEvent(MappingId, StaleFeedV2), false);

    FRshipContentMappingState AfterStaleEcho;
    TestTrue(TEXT("Mapping should still exist after stale echo"), CopyMappingById(Manager, MappingId, AfterStaleEcho));
    if (AfterStaleEcho.Id.IsEmpty())
    {
        return false;
    }

    TestEqual(TEXT("sources should stay empty after stale echo"), GetFeedArrayCount(&AfterStaleEcho, TEXT("sources")), 0);
    TestEqual(TEXT("destinations should stay empty after stale echo"), GetFeedArrayCount(&AfterStaleEcho, TEXT("destinations")), 0);
    TestEqual(TEXT("routes should stay empty after stale echo"), GetFeedArrayCount(&AfterStaleEcho, TEXT("routes")), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingFeedRemoveRouteGuardsStaleEchoTest,
    "Rship.ContentMapping.Feed.RemoveRouteGuardsStaleEcho",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingFeedRemoveRouteGuardsStaleEchoTest::RunTest(const FString& Parameters)
{
    URshipContentMappingManager* Manager = NewObject<URshipContentMappingManager>();
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager)
    {
        return false;
    }

    const FString SourceId = TEXT("source-1");
    const FString DestinationId = TEXT("dest-1");
    const FString RouteId = TEXT("route-1");
    const TSharedPtr<FJsonObject> FeedV2 = MakeFeedV2(
        {MakeShared<FJsonValueObject>(MakeFeedSource(SourceId, TEXT(""), 1920, 1080))},
        {MakeShared<FJsonValueObject>(MakeFeedDestination(DestinationId, TEXT(""), 1920, 1080))},
        {MakeShared<FJsonValueObject>(MakeFeedRoute(RouteId, SourceId, DestinationId, MakeRectPx(0, 0, 1920, 1080), MakeRectPx(0, 0, 1920, 1080)))});

    FRshipContentMappingState Mapping;
    Mapping.Name = TEXT("FeedRemoveRoute");
    Mapping.Type = TEXT("surface-uv");
    Mapping.bEnabled = true;
    Mapping.Opacity = 1.0f;
    Mapping.Config = BuildFeedConfig(FeedV2);

    const FString MappingId = Manager->CreateMapping(Mapping);
    TestTrue(TEXT("CreateMapping should return an id"), !MappingId.IsEmpty());
    if (MappingId.IsEmpty())
    {
        return false;
    }

    const FString TargetId = FString::Printf(TEXT("/content-mapping/mapping/%s"), *MappingId);
    TSharedRef<FJsonObject> RemoveRouteData = MakeShared<FJsonObject>();
    RemoveRouteData->SetStringField(TEXT("routeId"), RouteId);
    TestTrue(TEXT("removeFeedRoute action should be handled"), Manager->RouteAction(TargetId, TargetId + TEXT(":removeFeedRoute"), RemoveRouteData));

    FRshipContentMappingState AfterRouteDelete;
    TestTrue(TEXT("Mapping should still exist after route delete"), CopyMappingById(Manager, MappingId, AfterRouteDelete));
    if (AfterRouteDelete.Id.IsEmpty())
    {
        return false;
    }

    TestEqual(TEXT("routes should be empty after removeFeedRoute"), GetFeedArrayCount(&AfterRouteDelete, TEXT("routes")), 0);

    FRshipContentMappingState Updated = AfterRouteDelete;
    TestTrue(TEXT("UpdateMapping should preserve explicit empty routes"), Manager->UpdateMapping(Updated));
    FRshipContentMappingState AfterUpdate;
    TestTrue(TEXT("Mapping should exist after update"), CopyMappingById(Manager, MappingId, AfterUpdate));
    if (AfterUpdate.Id.IsEmpty())
    {
        return false;
    }
    TestEqual(TEXT("routes should remain empty after update"), GetFeedArrayCount(&AfterUpdate, TEXT("routes")), 0);

    Manager->ProcessMappingEvent(BuildFeedMappingEvent(MappingId, FeedV2), false);
    FRshipContentMappingState AfterStaleEcho;
    TestTrue(TEXT("Mapping should exist after stale route echo"), CopyMappingById(Manager, MappingId, AfterStaleEcho));
    if (AfterStaleEcho.Id.IsEmpty())
    {
        return false;
    }
    TestEqual(TEXT("stale echo must not re-add routes"), GetFeedArrayCount(&AfterStaleEcho, TEXT("routes")), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingFeedRemoveDestinationGuardsStaleEchoTest,
    "Rship.ContentMapping.Feed.RemoveDestinationGuardsStaleEcho",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingFeedRemoveDestinationGuardsStaleEchoTest::RunTest(const FString& Parameters)
{
    URshipContentMappingManager* Manager = NewObject<URshipContentMappingManager>();
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager)
    {
        return false;
    }

    const FString SourceId = TEXT("source-1");
    const FString DestinationId = TEXT("dest-1");
    const FString RouteId = TEXT("route-1");
    const TSharedPtr<FJsonObject> FeedV2 = MakeFeedV2(
        {MakeShared<FJsonValueObject>(MakeFeedSource(SourceId, TEXT(""), 1920, 1080))},
        {MakeShared<FJsonValueObject>(MakeFeedDestination(DestinationId, TEXT(""), 1920, 1080))},
        {MakeShared<FJsonValueObject>(MakeFeedRoute(RouteId, SourceId, DestinationId, MakeRectPx(0, 0, 1920, 1080), MakeRectPx(0, 0, 1920, 1080)))});

    FRshipContentMappingState Mapping;
    Mapping.Name = TEXT("FeedRemoveDestination");
    Mapping.Type = TEXT("surface-uv");
    Mapping.bEnabled = true;
    Mapping.Opacity = 1.0f;
    Mapping.Config = BuildFeedConfig(FeedV2);

    const FString MappingId = Manager->CreateMapping(Mapping);
    TestTrue(TEXT("CreateMapping should return an id"), !MappingId.IsEmpty());
    if (MappingId.IsEmpty())
    {
        return false;
    }

    const FString TargetId = FString::Printf(TEXT("/content-mapping/mapping/%s"), *MappingId);
    TSharedRef<FJsonObject> RemoveDestinationData = MakeShared<FJsonObject>();
    RemoveDestinationData->SetStringField(TEXT("destinationId"), DestinationId);
    TestTrue(TEXT("removeFeedDestination action should be handled"), Manager->RouteAction(TargetId, TargetId + TEXT(":removeFeedDestination"), RemoveDestinationData));

    FRshipContentMappingState AfterDestinationDelete;
    TestTrue(TEXT("Mapping should still exist after destination delete"), CopyMappingById(Manager, MappingId, AfterDestinationDelete));
    if (AfterDestinationDelete.Id.IsEmpty())
    {
        return false;
    }

    TestEqual(TEXT("destinations should be empty after removeFeedDestination"), GetFeedArrayCount(&AfterDestinationDelete, TEXT("destinations")), 0);
    TestEqual(TEXT("routes should also be empty after removing destination"), GetFeedArrayCount(&AfterDestinationDelete, TEXT("routes")), 0);

    FRshipContentMappingState Updated = AfterDestinationDelete;
    TestTrue(TEXT("UpdateMapping should preserve explicit empty destinations"), Manager->UpdateMapping(Updated));
    FRshipContentMappingState AfterUpdate;
    TestTrue(TEXT("Mapping should exist after update"), CopyMappingById(Manager, MappingId, AfterUpdate));
    if (AfterUpdate.Id.IsEmpty())
    {
        return false;
    }
    TestEqual(TEXT("destinations should remain empty after update"), GetFeedArrayCount(&AfterUpdate, TEXT("destinations")), 0);
    TestEqual(TEXT("routes should remain empty after update"), GetFeedArrayCount(&AfterUpdate, TEXT("routes")), 0);

    Manager->ProcessMappingEvent(BuildFeedMappingEvent(MappingId, FeedV2), false);
    FRshipContentMappingState AfterStaleEcho;
    TestTrue(TEXT("Mapping should exist after stale destination echo"), CopyMappingById(Manager, MappingId, AfterStaleEcho));
    if (AfterStaleEcho.Id.IsEmpty())
    {
        return false;
    }
    TestEqual(TEXT("stale echo must not re-add destinations"), GetFeedArrayCount(&AfterStaleEcho, TEXT("destinations")), 0);
    TestEqual(TEXT("stale echo must not re-add routes"), GetFeedArrayCount(&AfterStaleEcho, TEXT("routes")), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingFeedRemoveSourceGuardsStaleEchoTest,
    "Rship.ContentMapping.Feed.RemoveSourceGuardsStaleEcho",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingFeedRemoveSourceGuardsStaleEchoTest::RunTest(const FString& Parameters)
{
    URshipContentMappingManager* Manager = NewObject<URshipContentMappingManager>();
    TestNotNull(TEXT("Manager should be created"), Manager);
    if (!Manager)
    {
        return false;
    }

    const FString SourceId = TEXT("source-1");
    const FString DestinationId = TEXT("dest-1");
    const FString RouteId = TEXT("route-1");
    const TSharedPtr<FJsonObject> FeedV2 = MakeFeedV2(
        {MakeShared<FJsonValueObject>(MakeFeedSource(SourceId, TEXT(""), 1920, 1080))},
        {MakeShared<FJsonValueObject>(MakeFeedDestination(DestinationId, TEXT(""), 1920, 1080))},
        {MakeShared<FJsonValueObject>(MakeFeedRoute(RouteId, SourceId, DestinationId, MakeRectPx(0, 0, 1920, 1080), MakeRectPx(0, 0, 1920, 1080)))});

    FRshipContentMappingState Mapping;
    Mapping.Name = TEXT("FeedRemoveSource");
    Mapping.Type = TEXT("surface-uv");
    Mapping.bEnabled = true;
    Mapping.Opacity = 1.0f;
    Mapping.Config = BuildFeedConfig(FeedV2);

    const FString MappingId = Manager->CreateMapping(Mapping);
    TestTrue(TEXT("CreateMapping should return an id"), !MappingId.IsEmpty());
    if (MappingId.IsEmpty())
    {
        return false;
    }

    const FString TargetId = FString::Printf(TEXT("/content-mapping/mapping/%s"), *MappingId);
    TSharedRef<FJsonObject> RemoveSourceData = MakeShared<FJsonObject>();
    RemoveSourceData->SetStringField(TEXT("sourceId"), SourceId);
    TestTrue(TEXT("removeFeedSource action should be handled"), Manager->RouteAction(TargetId, TargetId + TEXT(":removeFeedSource"), RemoveSourceData));

    FRshipContentMappingState AfterSourceDelete;
    TestTrue(TEXT("Mapping should still exist after source delete"), CopyMappingById(Manager, MappingId, AfterSourceDelete));
    if (AfterSourceDelete.Id.IsEmpty())
    {
        return false;
    }

    TestEqual(TEXT("sources should be empty after removeFeedSource"), GetFeedArrayCount(&AfterSourceDelete, TEXT("sources")), 0);
    TestEqual(TEXT("routes should also be empty after removing source"), GetFeedArrayCount(&AfterSourceDelete, TEXT("routes")), 0);

    FRshipContentMappingState Updated = AfterSourceDelete;
    TestTrue(TEXT("UpdateMapping should preserve explicit empty sources"), Manager->UpdateMapping(Updated));
    FRshipContentMappingState AfterUpdate;
    TestTrue(TEXT("Mapping should exist after update"), CopyMappingById(Manager, MappingId, AfterUpdate));
    if (AfterUpdate.Id.IsEmpty())
    {
        return false;
    }
    TestEqual(TEXT("sources should remain empty after update"), GetFeedArrayCount(&AfterUpdate, TEXT("sources")), 0);
    TestEqual(TEXT("routes should remain empty after update"), GetFeedArrayCount(&AfterUpdate, TEXT("routes")), 0);

    Manager->ProcessMappingEvent(BuildFeedMappingEvent(MappingId, FeedV2), false);
    FRshipContentMappingState AfterStaleEcho;
    TestTrue(TEXT("Mapping should exist after stale source echo"), CopyMappingById(Manager, MappingId, AfterStaleEcho));
    if (AfterStaleEcho.Id.IsEmpty())
    {
        return false;
    }
    TestEqual(TEXT("stale echo must not re-add sources"), GetFeedArrayCount(&AfterStaleEcho, TEXT("sources")), 0);
    TestEqual(TEXT("stale echo must not re-add routes"), GetFeedArrayCount(&AfterStaleEcho, TEXT("routes")), 0);

    return true;
}

#endif
