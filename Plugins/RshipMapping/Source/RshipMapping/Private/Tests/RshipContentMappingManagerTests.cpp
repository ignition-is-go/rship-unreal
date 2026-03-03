#include "RshipContentMappingManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RshipSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#endif

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

    const FRshipMappingSurfaceState* FindSurfaceById(const TArray<FRshipMappingSurfaceState>& Surfaces, const FString& Id)
    {
        for (const FRshipMappingSurfaceState& Surface : Surfaces)
        {
            if (Surface.Id == Id)
            {
                return &Surface;
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

    UMaterial* CreateContextOnlyProbeMaterial()
    {
        UMaterial* Mat = NewObject<UMaterial>();
        if (!Mat)
        {
            return nullptr;
        }

        Mat->MaterialDomain = EMaterialDomain::MD_Surface;
        Mat->BlendMode = BLEND_Opaque;
        Mat->SetShadingModel(MSM_Unlit);

        UMaterialExpressionTextureSampleParameter2D* ContextTextureParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(Mat);
        ContextTextureParam->ParameterName = FName(TEXT("RshipContextTexture"));
        ContextTextureParam->Texture = Cast<UTexture2D>(
            StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")));
        ContextTextureParam->SamplerType = SAMPLERTYPE_Color;
        Mat->GetExpressionCollection().AddExpression(ContextTextureParam);

        Mat->GetEditorOnlyData()->EmissiveColor.Expression = ContextTextureParam;
        Mat->GetEditorOnlyData()->EmissiveColor.OutputIndex = 0;

        Mat->PreEditChange(nullptr);
        Mat->PostEditChange();
        return Mat;
    }

    struct FContentMappingE2ESnapshot
    {
        int32 EnabledMappings = 0;
        int32 ExpectedEnabledSurfaces = 0;
        int32 SurfacesWithBoundTexture = 0;
        int32 ContextsWithTexture = 0;
        int32 LastTickAppliedSurfaces = 0;
        int32 LastTickActiveContexts = 0;
        bool bHasSampledRenderTarget = false;
        float MaxLuminance = -1.0f;
        float AvgLuminance = -1.0f;
    };

    static const FName ParamContextTextureName(TEXT("RshipContextTexture"));
    static const FName ParamMappingIntensityName(TEXT("RshipMappingIntensity"));
    static const FName ParamMappingModeName(TEXT("RshipMappingMode"));
    static const FName ParamProjectionTypeName(TEXT("RshipProjectionType"));

    struct FContentMappingSurfaceSample
    {
        bool bSurfaceResolved = false;
        bool bHasAnyDynamicMaterial = false;
        bool bHasMappedSignal = false;
        float MappingMode = -1.0f;
        float ProjectionType = -1.0f;
        float MappingIntensity = -1.0f;
        FString Issue;
    };

    URshipContentMappingManager* GetLiveContentMappingManager(FString& OutIssue)
    {
        OutIssue.Reset();

        if (!GEngine)
        {
            OutIssue = TEXT("Engine unavailable");
            return nullptr;
        }

        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem)
        {
            OutIssue = TEXT("RshipSubsystem unavailable");
            return nullptr;
        }

        UObject* ManagerObject = Subsystem->GetContentMappingManager();
        URshipContentMappingManager* Manager = Cast<URshipContentMappingManager>(ManagerObject);
        if (!Manager)
        {
            OutIssue = TEXT("ContentMappingManager unavailable");
            return nullptr;
        }

        return Manager;
    }

    bool TrySampleSurfaceMaterialState(
        URshipContentMappingManager* Manager,
        const FString& SurfaceId,
        bool bRequirePIEWorld,
        FContentMappingSurfaceSample& OutSample)
    {
        OutSample = FContentMappingSurfaceSample();
        if (!Manager)
        {
            OutSample.Issue = TEXT("ContentMappingManager unavailable");
            return false;
        }

        const UWorld* RequiredWorld = nullptr;
        if (bRequirePIEWorld)
        {
            if (!GEditor || !GEditor->PlayWorld)
            {
                OutSample.Issue = TEXT("PIE world not active");
                return false;
            }
            RequiredWorld = GEditor->PlayWorld;
        }

        const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
        const FRshipMappingSurfaceState* SurfaceState = FindSurfaceById(Surfaces, SurfaceId);
        if (!SurfaceState)
        {
            OutSample.Issue = FString::Printf(TEXT("Surface not found: %s"), *SurfaceId);
            return false;
        }

        UMeshComponent* Mesh = SurfaceState->MeshComponent.Get();
        if (!Mesh || !IsValid(Mesh))
        {
            OutSample.Issue = FString::Printf(TEXT("Surface '%s' mesh is unresolved"), *SurfaceId);
            return false;
        }

        if (RequiredWorld && Mesh->GetWorld() != RequiredWorld)
        {
            OutSample.Issue = FString::Printf(TEXT("Surface '%s' resolved to wrong world"), *SurfaceId);
            return false;
        }

        OutSample.bSurfaceResolved = true;

        TArray<int32> SlotIndices = SurfaceState->MaterialSlots;
        if (SlotIndices.Num() == 0)
        {
            const int32 SlotCount = Mesh->GetNumMaterials();
            for (int32 Slot = 0; Slot < SlotCount; ++Slot)
            {
                SlotIndices.Add(Slot);
            }
        }

        for (int32 SlotIndex : SlotIndices)
        {
            if (SlotIndex < 0 || SlotIndex >= Mesh->GetNumMaterials())
            {
                continue;
            }

            UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(SlotIndex));
            if (!MID)
            {
                continue;
            }

            OutSample.bHasAnyDynamicMaterial = true;

            const UTexture* ContextTexture = MID->K2_GetTextureParameterValue(ParamContextTextureName);
            const float MappingIntensity = MID->K2_GetScalarParameterValue(ParamMappingIntensityName);
            const float MappingMode = MID->K2_GetScalarParameterValue(ParamMappingModeName);
            const float ProjectionType = MID->K2_GetScalarParameterValue(ParamProjectionTypeName);

            if (!OutSample.bHasMappedSignal)
            {
                OutSample.MappingMode = MappingMode;
                OutSample.ProjectionType = ProjectionType;
                OutSample.MappingIntensity = MappingIntensity;
            }

            if (ContextTexture && MappingIntensity > 0.001f)
            {
                OutSample.bHasMappedSignal = true;
                OutSample.MappingMode = MappingMode;
                OutSample.ProjectionType = ProjectionType;
                OutSample.MappingIntensity = MappingIntensity;
                return true;
            }
        }

        if (!OutSample.bHasAnyDynamicMaterial)
        {
            OutSample.Issue = FString::Printf(TEXT("Surface '%s' has no dynamic mapping material bound"), *SurfaceId);
        }

        return true;
    }

    bool TrySelectContextAndSurfaceForWorld(
        URshipContentMappingManager* Manager,
        bool bRequirePIEWorld,
        FString& OutContextId,
        FString& OutSurfaceId,
        FString& OutIssue)
    {
        OutContextId.Reset();
        OutSurfaceId.Reset();
        OutIssue.Reset();

        if (!Manager)
        {
            OutIssue = TEXT("ContentMappingManager unavailable");
            return false;
        }

        const UWorld* RequiredWorld = nullptr;
        if (bRequirePIEWorld)
        {
            if (!GEditor || !GEditor->PlayWorld)
            {
                OutIssue = TEXT("PIE world not active");
                return false;
            }
            RequiredWorld = GEditor->PlayWorld;
        }

        const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
        for (const FRshipRenderContextState& Context : Contexts)
        {
            if (Context.bEnabled && Context.ResolvedTexture)
            {
                OutContextId = Context.Id;
                break;
            }
        }

        if (OutContextId.IsEmpty())
        {
            for (const FRshipRenderContextState& Context : Contexts)
            {
                if (Context.bEnabled)
                {
                    OutContextId = Context.Id;
                    break;
                }
            }
        }

        const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
        for (const FRshipMappingSurfaceState& Surface : Surfaces)
        {
            if (!Surface.bEnabled)
            {
                continue;
            }

            UMeshComponent* Mesh = Surface.MeshComponent.Get();
            if (!Mesh || !IsValid(Mesh))
            {
                continue;
            }

            if (RequiredWorld && Mesh->GetWorld() != RequiredWorld)
            {
                continue;
            }

            OutSurfaceId = Surface.Id;
            break;
        }

        if (OutContextId.IsEmpty())
        {
            OutIssue = TEXT("No enabled render context available for lifecycle test");
            return false;
        }

        if (OutSurfaceId.IsEmpty())
        {
            OutIssue = TEXT("No enabled mapping surface available for lifecycle test");
            return false;
        }

        return true;
    }

    bool ComputeRenderTargetLuminance(UTexture* Texture, float& OutAverageLuminance, float& OutMaxLuminance)
    {
        OutAverageLuminance = -1.0f;
        OutMaxLuminance = -1.0f;

        UTextureRenderTarget2D* RenderTarget = Cast<UTextureRenderTarget2D>(Texture);
        if (!RenderTarget)
        {
            return false;
        }

        FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
        if (!Resource)
        {
            return false;
        }

        TArray<FColor> Pixels;
        if (!Resource->ReadPixels(Pixels) || Pixels.Num() == 0)
        {
            return false;
        }

        const int32 Step = FMath::Max(1, Pixels.Num() / 4096);
        double LuminanceSum = 0.0;
        float MaxLuminance = 0.0f;
        int32 SampleCount = 0;

        for (int32 Index = 0; Index < Pixels.Num(); Index += Step)
        {
            const FColor& Pixel = Pixels[Index];
            const float R = static_cast<float>(Pixel.R) / 255.0f;
            const float G = static_cast<float>(Pixel.G) / 255.0f;
            const float B = static_cast<float>(Pixel.B) / 255.0f;
            const float Luminance = (0.2126f * R) + (0.7152f * G) + (0.0722f * B);
            LuminanceSum += Luminance;
            MaxLuminance = FMath::Max(MaxLuminance, Luminance);
            ++SampleCount;
        }

        if (SampleCount <= 0)
        {
            return false;
        }

        OutAverageLuminance = static_cast<float>(LuminanceSum / static_cast<double>(SampleCount));
        OutMaxLuminance = MaxLuminance;
        return true;
    }

    bool CaptureContentMappingE2ESnapshot(
        bool bRequirePIEWorld,
        FContentMappingE2ESnapshot& OutSnapshot,
        FString& OutIssue)
    {
        OutIssue.Reset();
        OutSnapshot = FContentMappingE2ESnapshot();

        if (!GEngine)
        {
            OutIssue = TEXT("Engine unavailable");
            return false;
        }

        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem)
        {
            OutIssue = TEXT("RshipSubsystem unavailable");
            return false;
        }

        UObject* ManagerObject = Subsystem->GetContentMappingManager();
        URshipContentMappingManager* Manager = Cast<URshipContentMappingManager>(ManagerObject);
        if (!Manager)
        {
            OutIssue = TEXT("ContentMappingManager unavailable");
            return false;
        }

        const UWorld* RequiredWorld = nullptr;
        if (bRequirePIEWorld)
        {
            if (!GEditor || !GEditor->PlayWorld)
            {
                OutIssue = TEXT("PIE world not active");
                return false;
            }
            RequiredWorld = GEditor->PlayWorld;
        }

        const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
        const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
        const TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();

        TSet<FString> ReferencedSurfaceIds;
        for (const FRshipContentMappingState& Mapping : Mappings)
        {
            if (!Mapping.bEnabled)
            {
                continue;
            }

            ++OutSnapshot.EnabledMappings;
            for (const FString& RawSurfaceId : Mapping.SurfaceIds)
            {
                const FString SurfaceId = RawSurfaceId.TrimStartAndEnd();
                if (!SurfaceId.IsEmpty())
                {
                    ReferencedSurfaceIds.Add(SurfaceId);
                }
            }
        }

        for (const FRshipRenderContextState& Context : Contexts)
        {
            if (!Context.bEnabled || !Context.ResolvedTexture)
            {
                continue;
            }

            ++OutSnapshot.ContextsWithTexture;

            float AvgLuma = -1.0f;
            float MaxLuma = -1.0f;
            if (ComputeRenderTargetLuminance(Context.ResolvedTexture, AvgLuma, MaxLuma))
            {
                OutSnapshot.bHasSampledRenderTarget = true;
                OutSnapshot.MaxLuminance = FMath::Max(OutSnapshot.MaxLuminance, MaxLuma);
                OutSnapshot.AvgLuminance = FMath::Max(OutSnapshot.AvgLuminance, AvgLuma);
            }
        }

        for (const FString& SurfaceId : ReferencedSurfaceIds)
        {
            const FRshipMappingSurfaceState* SurfaceState = FindSurfaceById(Surfaces, SurfaceId);
            if (!SurfaceState || !SurfaceState->bEnabled)
            {
                continue;
            }

            ++OutSnapshot.ExpectedEnabledSurfaces;

            UMeshComponent* Mesh = SurfaceState->MeshComponent.Get();
            if (!Mesh || !IsValid(Mesh))
            {
                continue;
            }

            if (RequiredWorld && Mesh->GetWorld() != RequiredWorld)
            {
                continue;
            }

            TArray<int32> SlotIndices = SurfaceState->MaterialSlots;
            if (SlotIndices.Num() == 0)
            {
                const int32 SlotCount = Mesh->GetNumMaterials();
                for (int32 Slot = 0; Slot < SlotCount; ++Slot)
                {
                    SlotIndices.Add(Slot);
                }
            }

            bool bSurfaceBound = false;
            for (int32 SlotIndex : SlotIndices)
            {
                if (SlotIndex < 0 || SlotIndex >= Mesh->GetNumMaterials())
                {
                    continue;
                }

                UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(SlotIndex));
                if (!MID)
                {
                    continue;
                }

                UTexture* ContextTexture = MID->K2_GetTextureParameterValue(FName(TEXT("RshipContextTexture")));
                const float MappingIntensity = MID->K2_GetScalarParameterValue(FName(TEXT("RshipMappingIntensity")));
                if (ContextTexture && MappingIntensity > 0.001f)
                {
                    bSurfaceBound = true;
                    break;
                }
            }

            if (bSurfaceBound)
            {
                ++OutSnapshot.SurfacesWithBoundTexture;
            }
        }

        OutSnapshot.LastTickAppliedSurfaces = Manager->GetLastTickAppliedSurfacesForTest();
        OutSnapshot.LastTickActiveContexts = Manager->GetLastTickActiveContextsForTest();

        const bool bHasMappings = OutSnapshot.EnabledMappings > 0;
        const bool bHasExpectedSurfaces = OutSnapshot.ExpectedEnabledSurfaces > 0;
        const bool bAllExpectedSurfacesBound = bHasExpectedSurfaces
            && OutSnapshot.SurfacesWithBoundTexture >= OutSnapshot.ExpectedEnabledSurfaces;
        const bool bHasContextTexture = OutSnapshot.ContextsWithTexture > 0;
        const bool bAppliedAnySurfaceThisTick = OutSnapshot.LastTickAppliedSurfaces > 0;
        const bool bLuminancePass = !OutSnapshot.bHasSampledRenderTarget || OutSnapshot.MaxLuminance > 0.01f;

        if (bHasMappings
            && bHasExpectedSurfaces
            && bAllExpectedSurfacesBound
            && bHasContextTexture
            && bAppliedAnySurfaceThisTick
            && bLuminancePass)
        {
            return true;
        }

        OutIssue = FString::Printf(
            TEXT("enabledMappings=%d expectedEnabledSurfaces=%d boundSurfaces=%d contextsWithTexture=%d lastTickAppliedSurfaces=%d lastTickActiveContexts=%d sampledRT=%d maxLuma=%.4f avgLuma=%.4f"),
            OutSnapshot.EnabledMappings,
            OutSnapshot.ExpectedEnabledSurfaces,
            OutSnapshot.SurfacesWithBoundTexture,
            OutSnapshot.ContextsWithTexture,
            OutSnapshot.LastTickAppliedSurfaces,
            OutSnapshot.LastTickActiveContexts,
            OutSnapshot.bHasSampledRenderTarget ? 1 : 0,
            OutSnapshot.MaxLuminance,
            OutSnapshot.AvgLuminance);
        return false;
    }

    class FWaitForPIEStateCommand final : public IAutomationLatentCommand
    {
    public:
        FWaitForPIEStateCommand(
            FAutomationTestBase* InTest,
            bool bInExpectPIEActive,
            float InTimeoutSeconds,
            const FString& InStageLabel)
            : Test(InTest)
            , bExpectPIEActive(bInExpectPIEActive)
            , TimeoutSeconds(FMath::Max(0.1f, InTimeoutSeconds))
            , StageLabel(InStageLabel)
            , StartSeconds(FPlatformTime::Seconds())
        {
        }

        virtual bool Update() override
        {
            const bool bPIEActive = GEditor && GEditor->PlayWorld;
            if (bPIEActive == bExpectPIEActive)
            {
                return true;
            }

            const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
            if (Elapsed < TimeoutSeconds)
            {
                return false;
            }

            if (Test)
            {
                Test->AddError(FString::Printf(
                    TEXT("PIE state wait timed out at stage '%s' (expectedPIE=%d actualPIE=%d timeout=%.1fs)"),
                    *StageLabel,
                    bExpectPIEActive ? 1 : 0,
                    bPIEActive ? 1 : 0,
                    TimeoutSeconds));
            }
            return true;
        }

    private:
        FAutomationTestBase* Test = nullptr;
        bool bExpectPIEActive = false;
        float TimeoutSeconds = 0.0f;
        FString StageLabel;
        double StartSeconds = 0.0;
    };

    class FWaitForContentMappingSignalCommand final : public IAutomationLatentCommand
    {
    public:
        FWaitForContentMappingSignalCommand(
            FAutomationTestBase* InTest,
            bool bInRequirePIEWorld,
            float InTimeoutSeconds,
            const FString& InStageLabel)
            : Test(InTest)
            , bRequirePIEWorld(bInRequirePIEWorld)
            , TimeoutSeconds(FMath::Max(0.1f, InTimeoutSeconds))
            , StageLabel(InStageLabel)
            , StartSeconds(FPlatformTime::Seconds())
        {
        }

        virtual bool Update() override
        {
            FContentMappingE2ESnapshot Snapshot;
            FString Issue;
            if (CaptureContentMappingE2ESnapshot(bRequirePIEWorld, Snapshot, Issue))
            {
                if (Test)
                {
                    Test->AddInfo(FString::Printf(
                        TEXT("Content mapping healthy at '%s': contextsWithTexture=%d boundSurfaces=%d/%d lastTickApplied=%d maxLuma=%.4f"),
                        *StageLabel,
                        Snapshot.ContextsWithTexture,
                        Snapshot.SurfacesWithBoundTexture,
                        Snapshot.ExpectedEnabledSurfaces,
                        Snapshot.LastTickAppliedSurfaces,
                        Snapshot.MaxLuminance));
                }
                return true;
            }

            const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
            if (Elapsed < TimeoutSeconds)
            {
                return false;
            }

            if (Test)
            {
                Test->AddError(FString::Printf(
                    TEXT("Content mapping did not converge at '%s' within %.1fs: %s"),
                    *StageLabel,
                    TimeoutSeconds,
                    *Issue));
            }
            return true;
        }

    private:
        FAutomationTestBase* Test = nullptr;
        bool bRequirePIEWorld = false;
        float TimeoutSeconds = 0.0f;
        FString StageLabel;
        double StartSeconds = 0.0;
    };

    class FEnsureContentMappingBaselineCommand final : public IAutomationLatentCommand
    {
    public:
        FEnsureContentMappingBaselineCommand(
            FAutomationTestBase* InTest,
            float InTimeoutSeconds,
            const FString& InStageLabel)
            : Test(InTest)
            , TimeoutSeconds(FMath::Max(0.1f, InTimeoutSeconds))
            , StageLabel(InStageLabel)
            , StartSeconds(FPlatformTime::Seconds())
        {
        }

        virtual bool Update() override
        {
            FString ManagerIssue;
            URshipContentMappingManager* Manager = GetLiveContentMappingManager(ManagerIssue);
            if (!Manager)
            {
                return HandleRetryOrFailure(FString::Printf(
                    TEXT("Unable to get content mapping manager at '%s': %s"),
                    *StageLabel,
                    *ManagerIssue));
            }

            TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
            TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();

            FString SelectedContextId;
            for (const FRshipRenderContextState& Context : Contexts)
            {
                if (Context.bEnabled)
                {
                    SelectedContextId = Context.Id;
                    break;
                }
            }
            if (SelectedContextId.IsEmpty() && Contexts.Num() > 0)
            {
                SelectedContextId = Contexts[0].Id;
            }
            if (SelectedContextId.IsEmpty())
            {
                FRshipRenderContextState NewContext;
                NewContext.Name = TEXT("E2E Baseline Context");
                NewContext.SourceType = TEXT("camera");
                NewContext.CameraId = TEXT("AUTO");
                NewContext.CaptureMode = TEXT("FinalColorLDR");
                NewContext.Width = 1920;
                NewContext.Height = 1080;
                NewContext.bEnabled = true;
                SelectedContextId = Manager->CreateRenderContext(NewContext);
                if (SelectedContextId.IsEmpty())
                {
                    return HandleRetryOrFailure(FString::Printf(
                        TEXT("Failed to create baseline context at '%s'"),
                        *StageLabel));
                }
                Contexts = Manager->GetRenderContexts();
            }

            if (const FRshipRenderContextState* ExistingContext = FindContextById(Contexts, SelectedContextId))
            {
                if (!ExistingContext->bEnabled)
                {
                    FRshipRenderContextState EnabledContext = *ExistingContext;
                    EnabledContext.bEnabled = true;
                    if (!Manager->UpdateRenderContext(EnabledContext))
                    {
                        return HandleRetryOrFailure(FString::Printf(
                            TEXT("Failed to enable baseline context '%s' at '%s'"),
                            *SelectedContextId,
                            *StageLabel));
                    }
                }
            }

            TArray<FString> EnabledSurfaceIds;
            TArray<FString> AllSurfaceIds;
            for (const FRshipMappingSurfaceState& Surface : Surfaces)
            {
                if (Surface.Id.IsEmpty())
                {
                    continue;
                }
                AllSurfaceIds.Add(Surface.Id);
                if (Surface.bEnabled)
                {
                    EnabledSurfaceIds.Add(Surface.Id);
                }
            }

            if (AllSurfaceIds.Num() == 0)
            {
                return HandleRetryOrFailure(FString::Printf(
                    TEXT("No mapping surfaces are available at '%s'"),
                    *StageLabel));
            }

            if (EnabledSurfaceIds.Num() == 0)
            {
                for (const FRshipMappingSurfaceState& Surface : Surfaces)
                {
                    FRshipMappingSurfaceState EnabledSurface = Surface;
                    EnabledSurface.bEnabled = true;
                    Manager->UpdateMappingSurface(EnabledSurface);
                    EnabledSurfaceIds.Add(Surface.Id);
                }
                Surfaces = Manager->GetMappingSurfaces();
            }

            TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
            bool bHasEnabledMapping = false;
            for (const FRshipContentMappingState& Mapping : Mappings)
            {
                if (Mapping.bEnabled)
                {
                    bHasEnabledMapping = true;
                    break;
                }
            }

            if (!bHasEnabledMapping)
            {
                if (Mappings.Num() > 0)
                {
                    FRshipContentMappingState SeedMapping = Mappings[0];
                    SeedMapping.bEnabled = true;
                    SeedMapping.Opacity = FMath::Clamp(SeedMapping.Opacity, 0.01f, 1.0f);
                    if (SeedMapping.ContextId.IsEmpty())
                    {
                        SeedMapping.ContextId = SelectedContextId;
                    }
                    if (SeedMapping.SurfaceIds.Num() == 0)
                    {
                        SeedMapping.SurfaceIds = EnabledSurfaceIds;
                    }

                    if (!Manager->UpdateMapping(SeedMapping))
                    {
                        return HandleRetryOrFailure(FString::Printf(
                            TEXT("Failed to enable baseline mapping '%s' at '%s'"),
                            *SeedMapping.Id,
                            *StageLabel));
                    }
                }
                else
                {
                    FRshipContentMappingState NewMapping;
                    NewMapping.Name = TEXT("E2E Baseline Mapping");
                    NewMapping.Type = TEXT("direct");
                    NewMapping.ContextId = SelectedContextId;
                    NewMapping.SurfaceIds = EnabledSurfaceIds;
                    NewMapping.Opacity = 1.0f;
                    NewMapping.bEnabled = true;
                    NewMapping.Config = MakeShared<FJsonObject>();
                    NewMapping.Config->SetStringField(TEXT("uvMode"), TEXT("direct"));
                    NewMapping.Config->SetObjectField(TEXT("uvTransform"), MakeShared<FJsonObject>());

                    const FString MappingId = Manager->CreateMapping(NewMapping);
                    if (MappingId.IsEmpty())
                    {
                        return HandleRetryOrFailure(FString::Printf(
                            TEXT("Failed to create baseline mapping at '%s'"),
                            *StageLabel));
                    }
                }
            }

            Mappings = Manager->GetMappings();
            for (const FRshipContentMappingState& Mapping : Mappings)
            {
                if (Mapping.bEnabled)
                {
                    if (Test)
                    {
                        Test->AddInfo(FString::Printf(
                            TEXT("Content mapping baseline ready at '%s': context=%s enabledSurfaces=%d mapping=%s"),
                            *StageLabel,
                            *SelectedContextId,
                            EnabledSurfaceIds.Num(),
                            *Mapping.Id));
                    }
                    return true;
                }
            }

            return HandleRetryOrFailure(FString::Printf(
                TEXT("No enabled mapping available after baseline setup at '%s'"),
                *StageLabel));
        }

    private:
        bool HandleRetryOrFailure(const FString& Message)
        {
            const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
            if (Elapsed < TimeoutSeconds)
            {
                return false;
            }

            if (Test)
            {
                Test->AddError(Message);
            }
            return true;
        }

    private:
        FAutomationTestBase* Test = nullptr;
        float TimeoutSeconds = 0.0f;
        FString StageLabel;
        double StartSeconds = 0.0;
    };

    class FValidateMappingTypeLifecycleCommand final : public IAutomationLatentCommand
    {
    public:
        FValidateMappingTypeLifecycleCommand(
            FAutomationTestBase* InTest,
            bool bInRequirePIEWorld,
            float InPhaseTimeoutSeconds,
            const FString& InStageLabel)
            : Test(InTest)
            , bRequirePIEWorld(bInRequirePIEWorld)
            , PhaseTimeoutSeconds(FMath::Max(0.5f, InPhaseTimeoutSeconds))
            , StageLabel(InStageLabel)
            , PhaseStartSeconds(FPlatformTime::Seconds())
        {
            TypeExpectations =
            {
                {TEXT("direct"), 0.0f, 0.0f, TEXT("surface-uv"), TEXT("direct")},
                {TEXT("surface-uv"), 0.0f, 0.0f, TEXT("surface-uv"), TEXT("direct")},
                {TEXT("feed"), 0.0f, 0.0f, TEXT("surface-uv"), TEXT("feed")},
                {TEXT("surface-feed"), 0.0f, 0.0f, TEXT("surface-uv"), TEXT("feed")},

                {TEXT("perspective"), 1.0f, 0.0f, TEXT("surface-projection"), TEXT("perspective")},
                {TEXT("projection"), 1.0f, 0.0f, TEXT("surface-projection"), TEXT("perspective")},
                {TEXT("projector"), 1.0f, 0.0f, TEXT("surface-projection"), TEXT("perspective")},
                {TEXT("surface-projection"), 1.0f, 0.0f, TEXT("surface-projection"), TEXT("perspective")},

                {TEXT("cylindrical"), 1.0f, 1.0f, TEXT("surface-projection"), TEXT("cylindrical")},
                {TEXT("spherical"), 1.0f, 3.0f, TEXT("surface-projection"), TEXT("spherical")},
                {TEXT("orthographic"), 1.0f, 4.0f, TEXT("surface-projection"), TEXT("parallel")},
                {TEXT("ortho"), 1.0f, 4.0f, TEXT("surface-projection"), TEXT("parallel")},
                {TEXT("planar"), 1.0f, 4.0f, TEXT("surface-projection"), TEXT("parallel")},
                {TEXT("parallel"), 1.0f, 4.0f, TEXT("surface-projection"), TEXT("parallel")},
                {TEXT("radial"), 1.0f, 5.0f, TEXT("surface-projection"), TEXT("radial")},

                {TEXT("mesh"), 1.0f, 6.0f, TEXT("surface-projection"), TEXT("mesh")},
                {TEXT("mesh-projection"), 1.0f, 6.0f, TEXT("surface-projection"), TEXT("mesh")},
                {TEXT("mesh projection"), 1.0f, 6.0f, TEXT("surface-projection"), TEXT("mesh")},
                {TEXT("mesh-camera"), 1.0f, 6.0f, TEXT("surface-projection"), TEXT("mesh")},
                {TEXT("mesh camera"), 1.0f, 6.0f, TEXT("surface-projection"), TEXT("mesh")},
                {TEXT("ndisplay"), 1.0f, 6.0f, TEXT("surface-projection"), TEXT("mesh")},
                {TEXT("n-display"), 1.0f, 6.0f, TEXT("surface-projection"), TEXT("mesh")},

                {TEXT("fisheye"), 1.0f, 7.0f, TEXT("surface-projection"), TEXT("fisheye")},
                {TEXT("custom-matrix"), 1.0f, 8.0f, TEXT("surface-projection"), TEXT("custom-matrix")},
                {TEXT("custom matrix"), 1.0f, 8.0f, TEXT("surface-projection"), TEXT("custom-matrix")},
                {TEXT("matrix"), 1.0f, 8.0f, TEXT("surface-projection"), TEXT("custom-matrix")},
                {TEXT("camera-plate"), 1.0f, 9.0f, TEXT("surface-projection"), TEXT("camera-plate")},
                {TEXT("camera plate"), 1.0f, 9.0f, TEXT("surface-projection"), TEXT("camera-plate")},
                {TEXT("cameraplate"), 1.0f, 9.0f, TEXT("surface-projection"), TEXT("camera-plate")},
                {TEXT("spatial"), 1.0f, 10.0f, TEXT("surface-projection"), TEXT("spatial")},
                {TEXT("depth-map"), 1.0f, 11.0f, TEXT("surface-projection"), TEXT("depth-map")},
                {TEXT("depth map"), 1.0f, 11.0f, TEXT("surface-projection"), TEXT("depth-map")},
                {TEXT("depthmap"), 1.0f, 11.0f, TEXT("surface-projection"), TEXT("depth-map")}
            };
        }

        virtual bool Update() override
        {
            if (Phase == EPhase::Done)
            {
                return true;
            }

            FString ManagerIssue;
            URshipContentMappingManager* Manager = GetLiveContentMappingManager(ManagerIssue);
            if (!Manager)
            {
                if (!PhaseTimedOut())
                {
                    return false;
                }

                if (!bRecordedFailure)
                {
                    EnterFailure(FString::Printf(TEXT("Mapping lifecycle manager unavailable at '%s': %s"), *StageLabel, *ManagerIssue));
                    return false;
                }

                // Cleanup cannot proceed without a manager; finish once failure has been recorded.
                return true;
            }

            if (Phase != EPhase::Cleanup
                && Phase != EPhase::Done
                && bCapturedOriginalMappings)
            {
                ForceDisableCompetingMappings(Manager);
            }

            switch (Phase)
            {
            case EPhase::Setup:
            {
                if (bRequirePIEWorld && (!GEditor || !GEditor->PlayWorld))
                {
                    if (!PhaseTimedOut())
                    {
                        return false;
                    }
                    EnterFailure(FString::Printf(TEXT("PIE world unavailable at '%s' during setup"), *StageLabel));
                    return false;
                }

                if (!bCapturedOriginalMappings)
                {
                    OriginalMappings = Manager->GetMappings();
                    bCapturedOriginalMappings = true;
                }

                if (!bDisabledOriginalMappings)
                {
                    for (const FRshipContentMappingState& Mapping : OriginalMappings)
                    {
                        if (!Mapping.bEnabled)
                        {
                            continue;
                        }

                        FRshipContentMappingState DisabledState = Mapping;
                        DisabledState.bEnabled = false;
                        Manager->UpdateMapping(DisabledState);
                    }
                    bDisabledOriginalMappings = true;
                }

                FString SelectionIssue;
                if (!TrySelectContextAndSurfaceForWorld(Manager, bRequirePIEWorld, TargetContextId, TargetSurfaceId, SelectionIssue))
                {
                    if (!PhaseTimedOut())
                    {
                        return false;
                    }

                    EnterFailure(FString::Printf(
                        TEXT("Failed to select lifecycle context/surface at '%s': %s"),
                        *StageLabel,
                        *SelectionIssue));
                    return false;
                }

                if (TempMappingId.IsEmpty())
                {
                    FRshipContentMappingState TempMapping;
                    TempMapping.Name = FString::Printf(TEXT("E2E Lifecycle %s"), *StageLabel);
                    TempMapping.Type = TEXT("direct");
                    TempMapping.ContextId = TargetContextId;
                    TempMapping.SurfaceIds = {TargetSurfaceId};
                    TempMapping.Opacity = 1.0f;
                    TempMapping.bEnabled = true;
                    TempMapping.Config = MakeShared<FJsonObject>();
                    TempMapping.Config->SetStringField(TEXT("uvMode"), TEXT("direct"));
                    TempMapping.Config->SetObjectField(TEXT("uvTransform"), MakeShared<FJsonObject>());

                    TempMappingId = Manager->CreateMapping(TempMapping);
                    if (TempMappingId.IsEmpty())
                    {
                        if (!PhaseTimedOut())
                        {
                            return false;
                        }
                        EnterFailure(FString::Printf(TEXT("Failed to create lifecycle mapping at '%s'"), *StageLabel));
                        return false;
                    }
                }

                if (Test)
                {
                    Test->AddInfo(FString::Printf(
                        TEXT("Lifecycle setup at '%s': context=%s surface=%s mapping=%s"),
                        *StageLabel,
                        *TargetContextId,
                        *TargetSurfaceId,
                        *TempMappingId));
                }

                AdvancePhase(EPhase::WaitBaselineSignal);
                return false;
            }

            case EPhase::WaitBaselineSignal:
            {
                FContentMappingSurfaceSample Sample;
                if (TrySampleSurfaceMaterialState(Manager, TargetSurfaceId, bRequirePIEWorld, Sample) && Sample.bHasMappedSignal)
                {
                    if (Test)
                    {
                        Test->AddInfo(FString::Printf(
                            TEXT("Lifecycle baseline active at '%s': mode=%.1f projection=%.1f intensity=%.3f"),
                            *StageLabel,
                            Sample.MappingMode,
                            Sample.ProjectionType,
                            Sample.MappingIntensity));
                    }
                    AdvancePhase(EPhase::ApplyType);
                    return false;
                }

                if (!PhaseTimedOut())
                {
                    return false;
                }

                EnterFailure(FString::Printf(
                    TEXT("Lifecycle baseline did not appear at '%s': %s"),
                    *StageLabel,
                    *Sample.Issue));
                return false;
            }

            case EPhase::ApplyType:
            {
                if (CurrentTypeIndex >= TypeExpectations.Num())
                {
                    AdvancePhase(EPhase::DisableMapping);
                    return false;
                }

                const FTypeExpectation& Expected = TypeExpectations[CurrentTypeIndex];
                if (!ApplyMappingUpdateSetType(Manager, TempMappingId, Expected.TypeToken))
                {
                    EnterFailure(FString::Printf(
                        TEXT("Failed to update mapping type='%s' at '%s'"),
                        *Expected.TypeToken,
                        *StageLabel));
                    return false;
                }

                AdvancePhase(EPhase::WaitTypeApplied);
                return false;
            }

            case EPhase::WaitTypeApplied:
            {
                const FTypeExpectation& Expected = TypeExpectations[CurrentTypeIndex];
                FContentMappingSurfaceSample Sample;
                FRshipContentMappingState StoredMapping;
                const bool bHasStoredMapping = CopyMappingById(Manager, TempMappingId, StoredMapping);
                const FString ObservedCanonicalType = bHasStoredMapping ? StoredMapping.Type : TEXT("<missing>");
                const bool bExpectProjectionMode = Expected.ExpectedCanonicalType.Equals(TEXT("surface-projection"), ESearchCase::IgnoreCase);
                const TCHAR* ExpectedModeField = bExpectProjectionMode ? TEXT("projectionType") : TEXT("uvMode");
                FString ObservedCanonicalMode = TEXT("<missing>");
                if (bHasStoredMapping && StoredMapping.Config.IsValid() && StoredMapping.Config->HasTypedField<EJson::String>(ExpectedModeField))
                {
                    ObservedCanonicalMode = StoredMapping.Config->GetStringField(ExpectedModeField).TrimStartAndEnd().ToLower();
                }

                if (TrySampleSurfaceMaterialState(Manager, TargetSurfaceId, bRequirePIEWorld, Sample)
                    && Sample.bHasMappedSignal)
                {
                    const bool bModeMatches = FMath::IsNearlyEqual(Sample.MappingMode, Expected.ExpectedMappingMode, 0.05f);
                    const bool bProjectionMatches = FMath::IsNearlyEqual(Sample.ProjectionType, Expected.ExpectedProjectionType, 0.05f);
                    const bool bCanonicalTypeMatches = bHasStoredMapping
                        && StoredMapping.Type.Equals(Expected.ExpectedCanonicalType, ESearchCase::IgnoreCase);
                    const bool bCanonicalModeMatches = bHasStoredMapping
                        && !Expected.ExpectedCanonicalModeToken.IsEmpty()
                        && ObservedCanonicalMode.Equals(Expected.ExpectedCanonicalModeToken, ESearchCase::IgnoreCase);
                    if (bModeMatches && bProjectionMatches && bCanonicalTypeMatches && bCanonicalModeMatches)
                    {
                        if (Test)
                        {
                            Test->AddInfo(FString::Printf(
                                TEXT("Type '%s' applied at '%s': mode=%.1f projection=%.1f canonicalType=%s canonicalMode=%s"),
                                *Expected.TypeToken,
                                *StageLabel,
                                Sample.MappingMode,
                                Sample.ProjectionType,
                                *StoredMapping.Type,
                                *ObservedCanonicalMode));
                        }
                        ++CurrentTypeIndex;
                        AdvancePhase(EPhase::ApplyType);
                        return false;
                    }
                }

                if (!PhaseTimedOut())
                {
                    return false;
                }

                EnterFailure(FString::Printf(
                    TEXT("Type '%s' did not apply at '%s' (observed mode=%.3f projection=%.3f signal=%d canonicalType=%s canonicalMode=%s issue=%s)"),
                    *Expected.TypeToken,
                    *StageLabel,
                    Sample.MappingMode,
                    Sample.ProjectionType,
                    Sample.bHasMappedSignal ? 1 : 0,
                    *ObservedCanonicalType,
                    *ObservedCanonicalMode,
                    *Sample.Issue));
                return false;
            }

            case EPhase::DisableMapping:
            {
                if (!ApplyMappingUpdateSetEnabled(Manager, TempMappingId, false))
                {
                    EnterFailure(FString::Printf(TEXT("Failed to disable lifecycle mapping at '%s'"), *StageLabel));
                    return false;
                }
                AdvancePhase(EPhase::WaitMappingDisabled);
                return false;
            }

            case EPhase::WaitMappingDisabled:
            {
                FContentMappingSurfaceSample Sample;
                if (TrySampleSurfaceMaterialState(Manager, TargetSurfaceId, bRequirePIEWorld, Sample) && !Sample.bHasMappedSignal)
                {
                    if (Test)
                    {
                        Test->AddInfo(FString::Printf(
                            TEXT("Mapping disable cleared surface at '%s' (hasMID=%d)"),
                            *StageLabel,
                            Sample.bHasAnyDynamicMaterial ? 1 : 0));
                    }
                    AdvancePhase(EPhase::ReEnableMapping);
                    return false;
                }

                if (!PhaseTimedOut())
                {
                    return false;
                }

                EnterFailure(FString::Printf(
                    TEXT("Disabling mapping did not clear surface at '%s'"),
                    *StageLabel));
                return false;
            }

            case EPhase::ReEnableMapping:
            {
                if (!ApplyMappingUpdateSetEnabled(Manager, TempMappingId, true))
                {
                    EnterFailure(FString::Printf(TEXT("Failed to re-enable lifecycle mapping at '%s'"), *StageLabel));
                    return false;
                }
                AdvancePhase(EPhase::WaitMappingReenabled);
                return false;
            }

            case EPhase::WaitMappingReenabled:
            {
                FContentMappingSurfaceSample Sample;
                if (TrySampleSurfaceMaterialState(Manager, TargetSurfaceId, bRequirePIEWorld, Sample) && Sample.bHasMappedSignal)
                {
                    if (Test)
                    {
                        Test->AddInfo(FString::Printf(
                            TEXT("Mapping re-enabled at '%s': intensity=%.3f"),
                            *StageLabel,
                            Sample.MappingIntensity));
                    }
                    AdvancePhase(EPhase::DeleteMapping);
                    return false;
                }

                if (!PhaseTimedOut())
                {
                    return false;
                }

                EnterFailure(FString::Printf(
                    TEXT("Re-enabling mapping did not restore signal at '%s'"),
                    *StageLabel));
                return false;
            }

            case EPhase::DeleteMapping:
            {
                FRshipContentMappingState Existing;
                if (CopyMappingById(Manager, TempMappingId, Existing))
                {
                    if (!Manager->DeleteMapping(TempMappingId))
                    {
                        EnterFailure(FString::Printf(TEXT("Failed to delete lifecycle mapping at '%s'"), *StageLabel));
                        return false;
                    }
                }
                AdvancePhase(EPhase::WaitMappingDeleted);
                return false;
            }

            case EPhase::WaitMappingDeleted:
            {
                FContentMappingSurfaceSample Sample;
                FRshipContentMappingState Existing;
                const bool bStillExists = CopyMappingById(Manager, TempMappingId, Existing);
                if (TrySampleSurfaceMaterialState(Manager, TargetSurfaceId, bRequirePIEWorld, Sample)
                    && !Sample.bHasMappedSignal
                    && !bStillExists)
                {
                    if (Test)
                    {
                        Test->AddInfo(FString::Printf(
                            TEXT("Mapping delete cleared surface at '%s'"),
                            *StageLabel));
                    }
                    AdvancePhase(EPhase::Cleanup);
                    return false;
                }

                if (!PhaseTimedOut())
                {
                    return false;
                }

                EnterFailure(FString::Printf(
                    TEXT("Deleting mapping did not clear surface at '%s' (stillExists=%d)"),
                    *StageLabel,
                    bStillExists ? 1 : 0));
                return false;
            }

            case EPhase::Cleanup:
            {
                if (!bCleanupComplete)
                {
                    PerformCleanup(Manager);
                    bCleanupComplete = true;
                }
                AdvancePhase(EPhase::Done);
                return true;
            }

            case EPhase::Done:
            default:
                return true;
            }
        }

    private:
        struct FTypeExpectation
        {
            FString TypeToken;
            float ExpectedMappingMode = 0.0f;
            float ExpectedProjectionType = 0.0f;
            FString ExpectedCanonicalType;
            FString ExpectedCanonicalModeToken;
        };

        enum class EPhase : uint8
        {
            Setup,
            WaitBaselineSignal,
            ApplyType,
            WaitTypeApplied,
            DisableMapping,
            WaitMappingDisabled,
            ReEnableMapping,
            WaitMappingReenabled,
            DeleteMapping,
            WaitMappingDeleted,
            Cleanup,
            Done
        };

        bool PhaseTimedOut() const
        {
            return (FPlatformTime::Seconds() - PhaseStartSeconds) >= PhaseTimeoutSeconds;
        }

        void AdvancePhase(EPhase NewPhase)
        {
            Phase = NewPhase;
            PhaseStartSeconds = FPlatformTime::Seconds();
        }

        void EnterFailure(const FString& Message)
        {
            if (!bRecordedFailure && Test)
            {
                Test->AddError(Message);
            }
            bRecordedFailure = true;
            AdvancePhase(EPhase::Cleanup);
        }

        bool ApplyMappingUpdateSetType(
            URshipContentMappingManager* Manager,
            const FString& MappingId,
            const FString& TypeToken) const
        {
            if (!Manager || MappingId.IsEmpty())
            {
                return false;
            }

            FRshipContentMappingState Mapping;
            if (!CopyMappingById(Manager, MappingId, Mapping))
            {
                return false;
            }

            Mapping.Type = TypeToken;
            return Manager->UpdateMapping(Mapping);
        }

        bool ApplyMappingUpdateSetEnabled(
            URshipContentMappingManager* Manager,
            const FString& MappingId,
            bool bEnabled) const
        {
            if (!Manager || MappingId.IsEmpty())
            {
                return false;
            }

            FRshipContentMappingState Mapping;
            if (!CopyMappingById(Manager, MappingId, Mapping))
            {
                return false;
            }

            Mapping.bEnabled = bEnabled;
            return Manager->UpdateMapping(Mapping);
        }

        void ForceDisableCompetingMappings(URshipContentMappingManager* Manager) const
        {
            if (!Manager)
            {
                return;
            }

            const TArray<FRshipContentMappingState> CurrentMappings = Manager->GetMappings();
            for (const FRshipContentMappingState& Mapping : CurrentMappings)
            {
                if (Mapping.Id.IsEmpty())
                {
                    continue;
                }

                if (!TempMappingId.IsEmpty() && Mapping.Id.Equals(TempMappingId, ESearchCase::CaseSensitive))
                {
                    continue;
                }

                if (!Mapping.bEnabled)
                {
                    continue;
                }

                FRshipContentMappingState DisabledState = Mapping;
                DisabledState.bEnabled = false;
                Manager->UpdateMapping(DisabledState);
            }
        }

        void PerformCleanup(URshipContentMappingManager* Manager)
        {
            if (!Manager)
            {
                return;
            }

            FRshipContentMappingState Existing;
            if (!TempMappingId.IsEmpty() && CopyMappingById(Manager, TempMappingId, Existing))
            {
                Manager->DeleteMapping(TempMappingId);
            }

            for (const FRshipContentMappingState& Original : OriginalMappings)
            {
                if (Original.Id.IsEmpty())
                {
                    continue;
                }

                FRshipContentMappingState Current;
                if (CopyMappingById(Manager, Original.Id, Current))
                {
                    Manager->UpdateMapping(Original);
                }
                else
                {
                    Manager->CreateMapping(Original);
                }
            }

            if (Test)
            {
                Test->AddInfo(FString::Printf(
                    TEXT("Lifecycle cleanup complete at '%s' (restoredMappings=%d)"),
                    *StageLabel,
                    OriginalMappings.Num()));
            }
        }

    private:
        FAutomationTestBase* Test = nullptr;
        bool bRequirePIEWorld = false;
        float PhaseTimeoutSeconds = 0.0f;
        FString StageLabel;
        EPhase Phase = EPhase::Setup;
        double PhaseStartSeconds = 0.0;
        bool bRecordedFailure = false;
        bool bCapturedOriginalMappings = false;
        bool bDisabledOriginalMappings = false;
        bool bCleanupComplete = false;
        FString TargetContextId;
        FString TargetSurfaceId;
        FString TempMappingId;
        TArray<FRshipContentMappingState> OriginalMappings;
        TArray<FTypeExpectation> TypeExpectations;
        int32 CurrentTypeIndex = 0;
    };
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

#if WITH_EDITOR
    UMaterial* ContextOnlyMaterial = CreateContextOnlyProbeMaterial();
    TestNotNull(TEXT("Context-only material should be created"), ContextOnlyMaterial);
    if (ContextOnlyMaterial)
    {
        FString BaseError;
        const bool bBaseContractValid = Manager->ValidateMaterialContractForTest(
            ContextOnlyMaterial,
            BaseError,
            /*bRequireProjectionContract=*/false);
        TestTrue(TEXT("Context-only material should satisfy base contract"), bBaseContractValid);

        FString ProjectionError;
        const bool bProjectionContractValid = Manager->ValidateMaterialContractForTest(
            ContextOnlyMaterial,
            ProjectionError,
            /*bRequireProjectionContract=*/true);
        TestFalse(TEXT("Context-only material should fail projection contract"), bProjectionContractValid);
        TestTrue(TEXT("Projection error should mention projection parameters"), ProjectionError.Contains(TEXT("RshipProjectionType")));
    }
#endif

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingE2EPIESignalVisibleTest,
    "Rship.ContentMapping.E2E.PIE.SignalVisible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingE2EPIESignalVisibleTest::RunTest(const FString& Parameters)
{
#if !WITH_EDITOR
    AddWarning(TEXT("E2E PIE signal test requires editor context."));
    return true;
#else
    FString MapPath = TEXT("/Game/VprodProject/Maps/Main");
    FParse::Value(FCommandLine::Get(), TEXT("RshipContentMappingE2EMap="), MapPath);
    MapPath = MapPath.TrimStartAndEnd();
    if (MapPath.IsEmpty())
    {
        AddError(TEXT("Rship content mapping E2E map path is empty."));
        return false;
    }

    const float EditorConvergenceTimeoutSeconds = 25.0f;
    const float SimulateConvergenceTimeoutSeconds = 30.0f;

    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(MapPath));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStateCommand(this, false, 10.0f, TEXT("editor-idle")));
    ADD_LATENT_AUTOMATION_COMMAND(FEnsureContentMappingBaselineCommand(this, 15.0f, TEXT("editor-world")));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForContentMappingSignalCommand(this, false, EditorConvergenceTimeoutSeconds, TEXT("editor-world")));

    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(true));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStateCommand(this, true, 20.0f, TEXT("simulate-start")));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForContentMappingSignalCommand(this, true, SimulateConvergenceTimeoutSeconds, TEXT("simulate-world")));

    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStateCommand(this, false, 20.0f, TEXT("simulate-end")));

    return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipContentMappingE2EPIEMappingLifecycleAndTypesTest,
    "Rship.ContentMapping.E2E.PIE.MappingLifecycleAndTypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipContentMappingE2EPIEMappingLifecycleAndTypesTest::RunTest(const FString& Parameters)
{
#if !WITH_EDITOR
    AddWarning(TEXT("E2E PIE lifecycle/type test requires editor context."));
    return true;
#else
    FString MapPath = TEXT("/Game/VprodProject/Maps/Main");
    FParse::Value(FCommandLine::Get(), TEXT("RshipContentMappingE2EMap="), MapPath);
    MapPath = MapPath.TrimStartAndEnd();
    if (MapPath.IsEmpty())
    {
        AddError(TEXT("Rship content mapping E2E map path is empty."));
        return false;
    }

    const float EditorConvergenceTimeoutSeconds = 25.0f;
    const float SimulateConvergenceTimeoutSeconds = 30.0f;
    const float LifecyclePhaseTimeoutSeconds = 20.0f;

    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(MapPath));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStateCommand(this, false, 10.0f, TEXT("editor-idle")));
    ADD_LATENT_AUTOMATION_COMMAND(FEnsureContentMappingBaselineCommand(this, 15.0f, TEXT("editor-world")));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForContentMappingSignalCommand(this, false, EditorConvergenceTimeoutSeconds, TEXT("editor-world")));
    ADD_LATENT_AUTOMATION_COMMAND(FValidateMappingTypeLifecycleCommand(this, false, LifecyclePhaseTimeoutSeconds, TEXT("editor-world")));

    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(true));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStateCommand(this, true, 20.0f, TEXT("simulate-start")));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForContentMappingSignalCommand(this, true, SimulateConvergenceTimeoutSeconds, TEXT("simulate-world")));
    ADD_LATENT_AUTOMATION_COMMAND(FValidateMappingTypeLifecycleCommand(this, true, LifecyclePhaseTimeoutSeconds, TEXT("simulate-world")));

    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEStateCommand(this, false, 40.0f, TEXT("simulate-end")));

    return true;
#endif
}

#endif
