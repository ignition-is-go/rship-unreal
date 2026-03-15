// Content Mapping Manager implementation

#include "RshipContentMappingManager.h"
#include "RshipSubsystem.h"
#include "RshipSettings.h"
#include "RshipAssetStoreClient.h"
#include "RshipContentMappingTargetProxy.h"
#include "RshipActorRegistrationComponent.h"
#include "Controllers/RshipCameraController.h"
#include "Logs.h"

#include "Dom/JsonValue.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/Crc.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "UObject/SoftObjectPath.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

static const FName ParamContextTexture(TEXT("RshipContextTexture"));
static const FName ParamContextTextureAliasSlateUI(TEXT("SlateUI"));
static const FName ParamContextTextureAliasTexture(TEXT("Texture"));
static const FName ParamContextDepthTexture(TEXT("RshipContextDepthTexture"));
static const FName ParamMappingMode(TEXT("RshipMappingMode"));
static const FName ParamProjectionType(TEXT("RshipProjectionType"));
static const FName ParamProjectorRow0(TEXT("RshipProjectorRow0"));
static const FName ParamProjectorRow1(TEXT("RshipProjectorRow1"));
static const FName ParamProjectorRow2(TEXT("RshipProjectorRow2"));
static const FName ParamProjectorRow3(TEXT("RshipProjectorRow3"));
static const FName ParamUVTransform(TEXT("RshipUVTransform"));
static const FName ParamUVRotation(TEXT("RshipUVRotation"));
static const FName ParamUVScaleU(TEXT("RshipUVScaleU"));
static const FName ParamUVScaleV(TEXT("RshipUVScaleV"));
static const FName ParamUVOffsetU(TEXT("RshipUVOffsetU"));
static const FName ParamUVOffsetV(TEXT("RshipUVOffsetV"));
static const FName ParamOpacity(TEXT("RshipOpacity"));
static const FName ParamMappingIntensity(TEXT("RshipMappingIntensity"));
static const FName ParamUVChannel(TEXT("RshipUVChannel"));
static const FName ParamPreviewTint(TEXT("RshipPreviewTint"));
static const FName ParamDebugCoverage(TEXT("RshipDebugCoverage"));
static const FName ParamDebugUnmappedColor(TEXT("RshipDebugUnmappedColor"));
static const FName ParamDebugMappedColor(TEXT("RshipDebugMappedColor"));
static const FName ParamCylinderParams(TEXT("RshipCylinderParams"));
static const FName ParamCylinderExtent(TEXT("RshipCylinderExtent"));
static const FName ParamSphereParams(TEXT("RshipSphereParams"));
static const FName ParamSphereArc(TEXT("RshipSphereArc"));
static const FName ParamParallelSize(TEXT("RshipParallelSize"));
static const FName ParamRadialFlag(TEXT("RshipRadialFlag"));
static const FName ParamContentMode(TEXT("RshipContentMode"));
static const FName ParamMaskAngle(TEXT("RshipMaskAngle"));
static const FName ParamBorderExpansion(TEXT("RshipBorderExpansion"));
static const FName ParamFisheyeParams(TEXT("RshipFisheyeParams"));
static const FName ParamMeshEyepoint(TEXT("RshipMeshEyepoint"));
static const FName ParamCameraPlateParams(TEXT("RshipCameraPlateParams"));
static const FName ParamSpatialParams0(TEXT("RshipSpatialParams0"));
static const FName ParamSpatialParams1(TEXT("RshipSpatialParams1"));
static const FName ParamDepthMapParams(TEXT("RshipDepthMapParams"));

static const TCHAR* MaterialProfileDirect = TEXT("direct");
static const TCHAR* MaterialProfileProjection = TEXT("projection");
static const TCHAR* MaterialProfileCameraPlate = TEXT("camera-plate");
static const TCHAR* MaterialProfileSpatial = TEXT("spatial");
static const TCHAR* MaterialProfileDepthMap = TEXT("depth-map");

static TAutoConsoleVariable<int32> CVarRshipContentMappingPerfStats(
    TEXT("rship.cm.perf_stats"),
    0,
    TEXT("Enable content mapping perf stats logging once per second."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingCaptureUseMainView(
    TEXT("rship.cm.capture_use_main_view"),
    0,
    TEXT("Use main-view scene capture integration for mapping camera contexts."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingCaptureUseMainViewCamera(
    TEXT("rship.cm.capture_use_main_view_camera"),
    0,
    TEXT("Force mapping captures to use main view camera transform (usually should stay 0)."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingCaptureMainViewDivisor(
    TEXT("rship.cm.capture_main_view_divisor"),
    1,
    TEXT("Main-view resolution divisor for mapping captures (1=full res, 2=half)."));

static TAutoConsoleVariable<float> CVarRshipContentMappingCaptureLodFactor(
    TEXT("rship.cm.capture_lod_factor"),
    1.0f,
    TEXT("LOD distance factor for mapping scene captures (>=1.0)."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingCaptureQualityProfile(
    TEXT("rship.cm.capture_quality_profile"),
    1,
    TEXT("Capture quality profile for mapping contexts. 0=performance, 1=balanced, 2=fidelity."));

static TAutoConsoleVariable<float> CVarRshipContentMappingCaptureMaxViewDistance(
    TEXT("rship.cm.capture_max_view_distance"),
    0.0f,
    TEXT("Optional max view distance override for mapping scene captures (0 disables)."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingCaptureExplicitRefresh(
    TEXT("rship.cm.capture_explicit_refresh"),
    1,
    TEXT("Issue explicit CaptureScene refreshes for mapping contexts as a default reliability fallback when engine auto-capture is unreliable."));

static TAutoConsoleVariable<float> CVarRshipContentMappingCaptureExplicitRefreshInterval(
    TEXT("rship.cm.capture_explicit_refresh_interval"),
    0.0f,
    TEXT("Interval in seconds between explicit CaptureScene refreshes when enabled (0 captures only on first setup/changes)."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingAllowSurfaceMaterialFallback(
    TEXT("rship.cm.allow_surface_material_fallback"),
    0,
    TEXT("Allow runtime to use existing surface materials when canonical mapping material is unavailable (debug escape hatch; production should remain 0)."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingAllowSemanticFallbacks(
    TEXT("rship.cm.allow_semantic_fallbacks"),
    0,
    TEXT("Allow runtime to substitute alternate cameras/contexts/surfaces/material profiles when exact bindings are unavailable. 0=enforce deterministic fail-closed behavior (default), 1=legacy fallback behavior."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingAutoBootstrap(
    TEXT("rship.cm.autobootstrap"),
    1,
    TEXT("Automatically create a default content mapping setup when no contexts/surfaces/mappings exist."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingAutoBootstrapMaxSurfaces(
    TEXT("rship.cm.autobootstrap_max_surfaces"),
    8,
    TEXT("Maximum number of surfaces created by content mapping autobootstrap."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingPreferPlayerViewInPlay(
    TEXT("rship.cm.prefer_player_view_in_play"),
    0,
    TEXT("When in PIE/Game and mapped source camera is unavailable, allow fallback to active player view."));

static const TCHAR* RuntimeBlockedErrorPrefix = TEXT("Runtime blocked: ");

static FString GetActionName(const FString& ActionId)
{
    int32 Index = INDEX_NONE;
    if (ActionId.FindLastChar(TEXT(':'), Index))
    {
        return ActionId.Mid(Index + 1);
    }
    return ActionId;
}

namespace
{
    struct FMaterialParameterInventory
    {
        TSet<FName> ScalarParams;
        TSet<FName> VectorParams;
        TSet<FName> TextureParams;
    };

    FMaterialParameterInventory GatherMaterialParameterInventory(UMaterialInterface* Material)
    {
        FMaterialParameterInventory Inventory;
        if (!Material)
        {
            return Inventory;
        }

        {
            TArray<FMaterialParameterInfo> Infos;
            TArray<FGuid> Ids;
            Material->GetAllScalarParameterInfo(Infos, Ids);
            for (const FMaterialParameterInfo& Info : Infos)
            {
                Inventory.ScalarParams.Add(Info.Name);
            }
        }

        {
            TArray<FMaterialParameterInfo> Infos;
            TArray<FGuid> Ids;
            Material->GetAllVectorParameterInfo(Infos, Ids);
            for (const FMaterialParameterInfo& Info : Infos)
            {
                Inventory.VectorParams.Add(Info.Name);
            }
        }

        {
            TArray<FMaterialParameterInfo> Infos;
            TArray<FGuid> Ids;
            Material->GetAllTextureParameterInfo(Infos, Ids);
            for (const FMaterialParameterInfo& Info : Infos)
            {
                Inventory.TextureParams.Add(Info.Name);
            }
        }

        return Inventory;
    }

    bool HasContextTextureParameter(const FMaterialParameterInventory& Inventory)
    {
        return Inventory.TextureParams.Contains(ParamContextTexture)
            || Inventory.TextureParams.Contains(ParamContextTextureAliasSlateUI)
            || Inventory.TextureParams.Contains(ParamContextTextureAliasTexture);
    }

    void AppendMissingParameters(
        const TSet<FName>& Available,
        const TArray<FName>& Required,
        TArray<FString>& OutMissing)
    {
        for (const FName& Param : Required)
        {
            if (!Available.Contains(Param))
            {
                OutMissing.Add(Param.ToString());
            }
        }
    }

    bool HasProjectionContractParameters(const FMaterialParameterInventory& Inventory, TArray<FString>* OutMissing = nullptr)
    {
        TArray<FString> Missing;
        Missing.Reserve(8);

        static const TArray<FName> RequiredScalars =
        {
            ParamMappingMode,
            ParamProjectionType
        };

        static const TArray<FName> RequiredVectors =
        {
            ParamProjectorRow0,
            ParamProjectorRow1,
            ParamProjectorRow2,
            ParamProjectorRow3
        };

        AppendMissingParameters(Inventory.ScalarParams, RequiredScalars, Missing);
        AppendMissingParameters(Inventory.VectorParams, RequiredVectors, Missing);

        if (OutMissing)
        {
            *OutMissing = Missing;
        }
        return Missing.Num() == 0;
    }

    bool HasAdvancedProfileContractParameters(
        const FMaterialParameterInventory& Inventory,
        const FString& ProfileToken,
        TArray<FString>* OutMissing = nullptr)
    {
        TArray<FString> Missing;

        if (ProfileToken.Equals(MaterialProfileCameraPlate, ESearchCase::IgnoreCase))
        {
            static const TArray<FName> RequiredVectors = { ParamCameraPlateParams };
            AppendMissingParameters(Inventory.VectorParams, RequiredVectors, Missing);
        }
        else if (ProfileToken.Equals(MaterialProfileSpatial, ESearchCase::IgnoreCase))
        {
            static const TArray<FName> RequiredVectors = { ParamSpatialParams0, ParamSpatialParams1 };
            AppendMissingParameters(Inventory.VectorParams, RequiredVectors, Missing);
        }
        else if (ProfileToken.Equals(MaterialProfileDepthMap, ESearchCase::IgnoreCase))
        {
            static const TArray<FName> RequiredVectors = { ParamDepthMapParams };
            static const TArray<FName> RequiredTextures = { ParamContextDepthTexture };
            AppendMissingParameters(Inventory.VectorParams, RequiredVectors, Missing);
            AppendMissingParameters(Inventory.TextureParams, RequiredTextures, Missing);
        }

        if (OutMissing)
        {
            *OutMissing = Missing;
        }
        return Missing.Num() == 0;
    }

    int32 ScoreMaterialCapability(const FMaterialParameterInventory& Inventory)
    {
        // Prefer materials that expose richer projection controls while still supporting direct UV.
        int32 Score = 0;
        if (HasContextTextureParameter(Inventory))
        {
            Score += 1000;
        }

        if (Inventory.ScalarParams.Contains(ParamMappingMode))
        {
            Score += 200;
        }
        if (Inventory.VectorParams.Contains(ParamUVTransform))
        {
            Score += 150;
        }
        if (Inventory.ScalarParams.Contains(ParamProjectionType))
        {
            Score += 250;
        }
        if (Inventory.VectorParams.Contains(ParamProjectorRow0)) Score += 120;
        if (Inventory.VectorParams.Contains(ParamProjectorRow1)) Score += 120;
        if (Inventory.VectorParams.Contains(ParamProjectorRow2)) Score += 120;
        if (Inventory.VectorParams.Contains(ParamProjectorRow3)) Score += 120;
        if (Inventory.TextureParams.Contains(ParamContextDepthTexture))
        {
            Score += 80;
        }
        if (HasProjectionContractParameters(Inventory))
        {
            Score += 500;
        }

        return Score;
    }

    FString RuntimeHealthToToken(ERshipContentMappingRuntimeHealth Health)
    {
        switch (Health)
        {
            case ERshipContentMappingRuntimeHealth::Blocked:
                return TEXT("blocked");
            case ERshipContentMappingRuntimeHealth::Degraded:
                return TEXT("degraded");
            case ERshipContentMappingRuntimeHealth::Ready:
            default:
                return TEXT("ready");
        }
    }

    enum class ERshipCaptureQualityProfile : uint8
    {
        Performance = 0,
        Balanced = 1,
        Fidelity = 2
    };

    ERshipCaptureQualityProfile GetCaptureQualityProfile()
    {
        const int32 RawValue = CVarRshipContentMappingCaptureQualityProfile.GetValueOnGameThread();
        if (RawValue <= 0)
        {
            return ERshipCaptureQualityProfile::Performance;
        }
        if (RawValue >= 2)
        {
            return ERshipCaptureQualityProfile::Fidelity;
        }
        return ERshipCaptureQualityProfile::Balanced;
    }

    int32 GetEffectiveCaptureDivisor(ERshipCaptureQualityProfile Profile, int32 RequestedDivisor)
    {
        const int32 ClampedRequested = FMath::Max(1, RequestedDivisor);
        switch (Profile)
        {
            case ERshipCaptureQualityProfile::Performance:
                return FMath::Max(2, ClampedRequested);
            case ERshipCaptureQualityProfile::Balanced:
                return ClampedRequested;
            case ERshipCaptureQualityProfile::Fidelity:
            default:
                return ClampedRequested;
        }
    }

    float GetEffectiveCaptureLodFactor(ERshipCaptureQualityProfile Profile, float RequestedFactor)
    {
        const float ClampedRequested = FMath::Max(1.0f, RequestedFactor);
        switch (Profile)
        {
            case ERshipCaptureQualityProfile::Performance:
                return FMath::Max(2.0f, ClampedRequested);
            case ERshipCaptureQualityProfile::Balanced:
                return FMath::Max(1.35f, ClampedRequested);
            case ERshipCaptureQualityProfile::Fidelity:
            default:
                return ClampedRequested;
        }
    }

    void ApplyCaptureQualityProfile(
        USceneCaptureComponent2D* Capture,
        ERshipCaptureQualityProfile Profile,
        bool bDepthCapture)
    {
        if (!Capture)
        {
            return;
        }

        Capture->ShowFlags = FEngineShowFlags(ESFIM_Game);
        Capture->ShowFlags.SetMotionBlur(false);

        if (bDepthCapture)
        {
            Capture->ShowFlags.DisableAdvancedFeatures();
            Capture->ShowFlags.SetPostProcessing(false);
            Capture->ShowFlags.SetBloom(false);
            Capture->ShowFlags.SetTonemapper(false);
            Capture->ShowFlags.SetFog(false);
            Capture->ShowFlags.SetAtmosphere(false);
            Capture->ShowFlags.SetSkyLighting(false);
            Capture->ShowFlags.SetVolumetricFog(false);
            Capture->ShowFlags.SetAmbientOcclusion(false);
            Capture->ShowFlags.SetDistanceFieldAO(false);
            Capture->ShowFlags.SetScreenSpaceReflections(false);
            Capture->ShowFlags.SetLumenGlobalIllumination(false);
            Capture->ShowFlags.SetLumenReflections(false);
            Capture->ShowFlags.SetReflectionEnvironment(false);
            Capture->bUseRayTracingIfEnabled = false;
            Capture->bExcludeFromSceneTextureExtents = true;
            return;
        }

        switch (Profile)
        {
            case ERshipCaptureQualityProfile::Performance:
                Capture->ShowFlags.DisableAdvancedFeatures();
                Capture->ShowFlags.SetPostProcessing(false);
                Capture->ShowFlags.SetBloom(false);
                Capture->ShowFlags.SetTonemapper(false);
                Capture->ShowFlags.SetAntiAliasing(false);
                Capture->ShowFlags.SetTemporalAA(false);
                Capture->ShowFlags.SetFog(false);
                Capture->ShowFlags.SetAtmosphere(false);
                Capture->ShowFlags.SetSkyLighting(false);
                Capture->ShowFlags.SetVolumetricFog(false);
                Capture->ShowFlags.SetAmbientOcclusion(false);
                Capture->ShowFlags.SetDistanceFieldAO(false);
                Capture->ShowFlags.SetScreenSpaceReflections(false);
                Capture->ShowFlags.SetLumenGlobalIllumination(false);
                Capture->ShowFlags.SetLumenReflections(false);
                Capture->ShowFlags.SetReflectionEnvironment(false);
                Capture->bUseRayTracingIfEnabled = false;
                Capture->bExcludeFromSceneTextureExtents = true;
                break;

            case ERshipCaptureQualityProfile::Balanced:
                Capture->ShowFlags.SetPostProcessing(true);
                Capture->ShowFlags.SetBloom(true);
                Capture->ShowFlags.SetTonemapper(true);
                Capture->ShowFlags.SetAntiAliasing(true);
                Capture->ShowFlags.SetTemporalAA(true);
                Capture->ShowFlags.SetAmbientOcclusion(false);
                Capture->ShowFlags.SetDistanceFieldAO(false);
                Capture->ShowFlags.SetScreenSpaceReflections(false);
                Capture->ShowFlags.SetLumenGlobalIllumination(false);
                Capture->ShowFlags.SetLumenReflections(false);
                Capture->ShowFlags.SetVolumetricFog(false);
                Capture->ShowFlags.SetReflectionEnvironment(true);
                Capture->ShowFlags.SetSkyLighting(true);
                Capture->ShowFlags.SetFog(true);
                Capture->ShowFlags.SetAtmosphere(true);
                Capture->bUseRayTracingIfEnabled = false;
                Capture->bExcludeFromSceneTextureExtents = true;
                break;

            case ERshipCaptureQualityProfile::Fidelity:
            default:
                Capture->ShowFlags.SetPostProcessing(true);
                Capture->ShowFlags.SetBloom(true);
                Capture->ShowFlags.SetTonemapper(true);
                Capture->ShowFlags.SetAntiAliasing(true);
                Capture->ShowFlags.SetTemporalAA(true);
                Capture->ShowFlags.SetAmbientOcclusion(true);
                Capture->ShowFlags.SetDistanceFieldAO(true);
                Capture->ShowFlags.SetScreenSpaceReflections(true);
                Capture->ShowFlags.SetLumenGlobalIllumination(true);
                Capture->ShowFlags.SetLumenReflections(true);
                Capture->ShowFlags.SetVolumetricFog(true);
                Capture->ShowFlags.SetReflectionEnvironment(true);
                Capture->ShowFlags.SetSkyLighting(true);
                Capture->ShowFlags.SetFog(true);
                Capture->ShowFlags.SetAtmosphere(true);
                Capture->bUseRayTracingIfEnabled = true;
                Capture->bExcludeFromSceneTextureExtents = false;
                break;
        }
    }

    TSharedPtr<FJsonObject> ParseJsonObjectString(const FString& Json)
    {
        if (Json.IsEmpty())
        {
            return MakeShared<FJsonObject>();
        }

        TSharedPtr<FJsonObject> Parsed = MakeShared<FJsonObject>();
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
        {
            return nullptr;
        }

        return Parsed;
    }

    struct FFeedRectPx
    {
        int32 X = 0;
        int32 Y = 0;
        int32 W = 0;
        int32 H = 0;
    };

    struct FFeedSourceSpec
    {
        FString Id;
        FString Label;
        FString ContextId;
        int32 Width = 0;
        int32 Height = 0;
    };

    struct FFeedDestinationSpec
    {
        FString Id;
        FString Label;
        FString SurfaceId;
        int32 Width = 0;
        int32 Height = 0;
    };

    struct FFeedRouteSpec
    {
        FString Id;
        FString Label;
        FString SourceId;
        FString DestinationId;
        bool bEnabled = true;
        float Opacity = 1.0f;
        FFeedRectPx SourceRect;
        FFeedRectPx DestinationRect;
    };

    struct FFeedV2Spec
    {
        bool bValid = false;
        FString CoordinateSpace = TEXT("pixel");
        TMap<FString, FFeedSourceSpec> Sources;
        TMap<FString, FFeedDestinationSpec> Destinations;
        TArray<FFeedRouteSpec> Routes;
    };

    bool IsRelevantContentMappingWorldType(EWorldType::Type WorldType)
    {
        return WorldType == EWorldType::Editor
            || WorldType == EWorldType::EditorPreview
            || WorldType == EWorldType::PIE
            || WorldType == EWorldType::Game;
    }

    bool IsPlayContentMappingWorldType(EWorldType::Type WorldType)
    {
        return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
    }

    UWorld* GetPreferredContentMappingViewportWorld()
    {
#if WITH_EDITOR
        if (GEditor && GEditor->PlayWorld && !GEditor->PlayWorld->bIsTearingDown)
        {
            return GEditor->PlayWorld;
        }
#endif

        if (GEngine && GEngine->GameViewport)
        {
            if (UWorld* ViewportWorld = GEngine->GameViewport->GetWorld())
            {
                if (!ViewportWorld->bIsTearingDown)
                {
                    return ViewportWorld;
                }
            }
        }

        return nullptr;
    }

    bool TryGetEditorViewportFallback(UWorld* PreferredWorld, FTransform& OutTransform, float& OutFov)
    {
#if WITH_EDITOR
        if (!GEditor)
        {
            return false;
        }

        auto IsViewportUsableForWorld = [PreferredWorld](const FEditorViewportClient* ViewportClient) -> bool
        {
            if (!ViewportClient || !ViewportClient->IsPerspective())
            {
                return false;
            }

            if (!PreferredWorld)
            {
                return true;
            }

            UWorld* ViewportWorld = ViewportClient->GetWorld();
            return !ViewportWorld || ViewportWorld == PreferredWorld;
        };

        FEditorViewportClient* BestViewport = nullptr;
        const TArray<FEditorViewportClient*>& Viewports = GEditor->GetAllViewportClients();
        for (FEditorViewportClient* ViewportClient : Viewports)
        {
            if (IsViewportUsableForWorld(ViewportClient))
            {
                BestViewport = ViewportClient;
                break;
            }
        }

        if (!BestViewport)
        {
            return false;
        }

        OutTransform = FTransform(BestViewport->GetViewRotation(), BestViewport->GetViewLocation());
        OutFov = BestViewport->ViewFOV;
        return true;
#else
        return false;
#endif
    }

    bool IsEditorContentMappingWorldType(EWorldType::Type WorldType)
    {
        return WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview;
    }

    bool IsLikelyScreenActor(const AActor* Actor)
    {
        if (!Actor)
        {
            return false;
        }

        if (Actor->IsA<ACameraActor>() || Actor->FindComponentByClass<UCameraComponent>())
        {
            return false;
        }

        TArray<UMeshComponent*> MeshComponents;
        const_cast<AActor*>(Actor)->GetComponents(MeshComponents);
        return MeshComponents.Num() > 0;
    }

    FString GetShortIdToken(const FString& Value);
    FString GetActorLabelCompat(const AActor* Actor);

    UCameraComponent* ResolveSourceCameraComponent(AActor* Actor)
    {
        if (!Actor)
        {
            return nullptr;
        }

        if (ACameraActor* CameraActor = Cast<ACameraActor>(Actor))
        {
            if (UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
            {
                return CameraComponent;
            }
        }

        return Actor->FindComponentByClass<UCameraComponent>();
    }

    bool IsCameraSourceActor(AActor* Actor)
    {
        return ResolveSourceCameraComponent(Actor) != nullptr;
    }

    int32 ScoreSourceCameraActor(AActor* Actor)
    {
        if (!Actor)
        {
            return MIN_int32;
        }

        int32 Score = 0;
        if (Actor->FindComponentByClass<URshipCameraController>())
        {
            Score += 100;
        }
        if (Actor->FindComponentByClass<URshipActorRegistrationComponent>())
        {
            Score += 50;
        }
        if (Actor->IsA<ACameraActor>())
        {
            Score += 20;
        }
        return Score;
    }

    FString BuildActorTargetId(URshipSubsystem* Subsystem, AActor* Actor)
    {
        if (!Actor)
        {
            return FString();
        }

        if (const URshipActorRegistrationComponent* Registration = Actor->FindComponentByClass<URshipActorRegistrationComponent>())
        {
            const FString FullTargetId = Registration->GetFullTargetId().TrimStartAndEnd();
            if (!FullTargetId.IsEmpty())
            {
                return FullTargetId;
            }

            const FString TargetId = Registration->GetTargetId().TrimStartAndEnd();
            if (!TargetId.IsEmpty())
            {
                if (!TargetId.Contains(TEXT(":")) && Subsystem)
                {
                    const FString ServiceId = Subsystem->GetServiceId().TrimStartAndEnd();
                    if (!ServiceId.IsEmpty())
                    {
                        return ServiceId + TEXT(":") + TargetId;
                    }
                }
                return TargetId;
            }
        }

        FString DisplayName = GetActorLabelCompat(Actor).TrimStartAndEnd();
        if (DisplayName.IsEmpty())
        {
            DisplayName = Actor->GetName();
        }

        if (DisplayName.IsEmpty())
        {
            return FString();
        }

        if (Subsystem && !DisplayName.Contains(TEXT(":")))
        {
            const FString ServiceId = Subsystem->GetServiceId().TrimStartAndEnd();
            if (!ServiceId.IsEmpty())
            {
                return ServiceId + TEXT(":") + DisplayName;
            }
        }

        return DisplayName;
    }

    bool MatchesRequestedIdToken(const FString& Candidate, const FString& RequestedId, const FString& RequestedShortId, bool bAllowPartialMatch)
    {
        if (Candidate.IsEmpty())
        {
            return false;
        }

        return Candidate.Equals(RequestedId, ESearchCase::IgnoreCase)
            || Candidate.Equals(RequestedShortId, ESearchCase::IgnoreCase)
            || (bAllowPartialMatch && Candidate.Contains(RequestedId, ESearchCase::IgnoreCase))
            || (bAllowPartialMatch && Candidate.Contains(RequestedShortId, ESearchCase::IgnoreCase));
    }

    bool DoesActorMatchRequestedId(URshipSubsystem* Subsystem, AActor* Actor, const FString& RequestedId, const FString& RequestedShortId, bool bAllowPartialMatch)
    {
        if (!Actor)
        {
            return false;
        }

        const FString CandidateName = Actor->GetName();
        const FString CandidateLabel = GetActorLabelCompat(Actor);
        const FString CandidateTargetId = BuildActorTargetId(Subsystem, Actor);
        const FString CandidateShortTargetId = GetShortIdToken(CandidateTargetId);
        const URshipActorRegistrationComponent* Registration = Actor->FindComponentByClass<URshipActorRegistrationComponent>();
        const FString CandidateRegistrationId = Registration ? Registration->GetTargetId().TrimStartAndEnd() : FString();

        return MatchesRequestedIdToken(CandidateName, RequestedId, RequestedShortId, bAllowPartialMatch)
            || MatchesRequestedIdToken(CandidateLabel, RequestedId, RequestedShortId, bAllowPartialMatch)
            || MatchesRequestedIdToken(CandidateTargetId, RequestedId, RequestedShortId, bAllowPartialMatch)
            || MatchesRequestedIdToken(CandidateShortTargetId, RequestedId, RequestedShortId, bAllowPartialMatch)
            || MatchesRequestedIdToken(CandidateRegistrationId, RequestedId, RequestedShortId, bAllowPartialMatch);
    }

    AActor* FindAnySourceCameraActor()
    {
        if (!GEngine)
        {
            return nullptr;
        }

        for (int32 Pass = 0; Pass < 3; ++Pass)
        {
            AActor* BestActor = nullptr;
            int32 BestScore = MIN_int32;

            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !IsRelevantContentMappingWorldType(Context.WorldType))
                {
                    continue;
                }

                const bool bIsPlay = IsPlayContentMappingWorldType(Context.WorldType);
                const bool bIsEditor = IsEditorContentMappingWorldType(Context.WorldType);
                if (Pass == 0 && !bIsPlay)
                {
                    continue;
                }
                if (Pass == 1 && !bIsEditor)
                {
                    continue;
                }
                if (Pass == 2 && (bIsPlay || bIsEditor))
                {
                    continue;
                }

                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Candidate = *It;
                    if (!IsCameraSourceActor(Candidate))
                    {
                        continue;
                    }

                    const int32 Score = ScoreSourceCameraActor(Candidate);
                    if (!BestActor || Score > BestScore)
                    {
                        BestActor = Candidate;
                        BestScore = Score;
                    }
                }
            }

            if (BestActor)
            {
                return BestActor;
            }
        }

        return nullptr;
    }

    AActor* FindAnySourceCameraAnchorActor()
    {
        return FindAnySourceCameraActor();
    }

    bool IsMeshReadyForMaterialMutation(const UMeshComponent* Mesh)
    {
        if (!Mesh || !IsValid(Mesh))
        {
            return false;
        }

        if (Mesh->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || Mesh->IsUnreachable())
        {
            return false;
        }

        const AActor* Owner = Mesh->GetOwner();
        if (!Owner || !IsValid(Owner))
        {
            return false;
        }

        if (Owner->IsActorBeingDestroyed()
            || Owner->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)
            || Owner->IsUnreachable())
        {
            return false;
        }

        const UWorld* World = Owner->GetWorld();
        if (!World || World->bIsTearingDown)
        {
            return false;
        }

        return true;
    }

    FString GetShortIdToken(const FString& Value)
    {
        FString Token = Value.TrimStartAndEnd();
        int32 ColonIndex = INDEX_NONE;
        if (Token.FindLastChar(TEXT(':'), ColonIndex))
        {
            Token = Token.Mid(ColonIndex + 1);
        }
        return Token;
    }

    int32 ScoreTokenMatch(const FString& Candidate, const FString& Token, int32 ExactScore, int32 PartialScore)
    {
        if (Token.IsEmpty() || Candidate.IsEmpty())
        {
            return 0;
        }

        if (Candidate.Equals(Token, ESearchCase::IgnoreCase))
        {
            return ExactScore;
        }

        if (Candidate.Contains(Token, ESearchCase::IgnoreCase))
        {
            return PartialScore;
        }

        return 0;
    }

    FString GetActorLabelCompat(const AActor* Actor)
    {
        if (!Actor)
        {
            return FString();
        }
#if WITH_EDITOR
        return Actor->GetActorLabel();
#else
        return Actor->GetName();
#endif
    }

    TArray<FString> GatherEffectiveSurfaceIdsForMapping(const FRshipContentMappingState& MappingState)
    {
        TArray<FString> EffectiveSurfaceIds;
        TSet<FString> SeenSurfaceIds;

        auto AddSurfaceId = [&EffectiveSurfaceIds, &SeenSurfaceIds](const FString& RawSurfaceId)
        {
            const FString SurfaceId = RawSurfaceId.TrimStartAndEnd();
            if (!SurfaceId.IsEmpty() && !SeenSurfaceIds.Contains(SurfaceId))
            {
                SeenSurfaceIds.Add(SurfaceId);
                EffectiveSurfaceIds.Add(SurfaceId);
            }
        };

        for (const FString& SurfaceId : MappingState.SurfaceIds)
        {
            AddSurfaceId(SurfaceId);
        }

        if (MappingState.Config.IsValid() && MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
        {
            const TSharedPtr<FJsonObject> FeedV2 = MappingState.Config->GetObjectField(TEXT("feedV2"));
            if (FeedV2.IsValid() && FeedV2->HasTypedField<EJson::Array>(TEXT("destinations")))
            {
                const TArray<TSharedPtr<FJsonValue>> Destinations = FeedV2->GetArrayField(TEXT("destinations"));
                for (const TSharedPtr<FJsonValue>& DestinationValue : Destinations)
                {
                    if (!DestinationValue.IsValid() || DestinationValue->Type != EJson::Object)
                    {
                        continue;
                    }
                    const TSharedPtr<FJsonObject> DestinationObj = DestinationValue->AsObject();
                    if (!DestinationObj.IsValid())
                    {
                        continue;
                    }

                    FString DestinationSurfaceId;
                    if (DestinationObj->TryGetStringField(TEXT("surfaceId"), DestinationSurfaceId))
                    {
                        AddSurfaceId(DestinationSurfaceId);
                    }
                }
            }
        }

        return EffectiveSurfaceIds;
    }

    uint32 HashFeedRouteRectPx(const FFeedRectPx& Rect)
    {
        uint32 Hash = HashCombineFast(GetTypeHash(Rect.X), GetTypeHash(Rect.Y));
        Hash = HashCombineFast(Hash, GetTypeHash(Rect.W));
        Hash = HashCombineFast(Hash, GetTypeHash(Rect.H));
        return Hash;
    }

    AActor* FindSourceCameraActorByEntityId(URshipSubsystem* Subsystem, const FString& CameraId)
    {
        if (!GEngine)
        {
            return nullptr;
        }

        if (CameraId.IsEmpty() || CameraId.Equals(TEXT("AUTO"), ESearchCase::IgnoreCase))
        {
            return FindAnySourceCameraActor();
        }

        const FString RequestedId = CameraId.TrimStartAndEnd();
        const FString RequestedShortId = GetShortIdToken(RequestedId);
        const bool bAllowPartialMatch = RequestedShortId.Len() >= 4 || RequestedId.Len() >= 4;

        if (Subsystem)
        {
            if (URshipActorRegistrationComponent* Registration = Subsystem->FindTargetComponent(RequestedId))
            {
                if (AActor* Owner = Registration->GetOwner())
                {
                    if (IsCameraSourceActor(Owner))
                    {
                        return Owner;
                    }
                }
            }
        }

        for (int32 Pass = 0; Pass < 3; ++Pass)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !IsRelevantContentMappingWorldType(Context.WorldType))
                {
                    continue;
                }

                const bool bIsPlay = IsPlayContentMappingWorldType(Context.WorldType);
                const bool bIsEditor = IsEditorContentMappingWorldType(Context.WorldType);
                if (Pass == 0 && !bIsPlay)
                {
                    continue;
                }
                if (Pass == 1 && !bIsEditor)
                {
                    continue;
                }
                if (Pass == 2 && (bIsPlay || bIsEditor))
                {
                    continue;
                }

                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Candidate = *It;
                    if (!IsCameraSourceActor(Candidate))
                    {
                        continue;
                    }

                    if (DoesActorMatchRequestedId(Subsystem, Candidate, RequestedId, RequestedShortId, bAllowPartialMatch))
                    {
                        return Candidate;
                    }
                }
            }
        }

        // If a specific camera ID was requested and could not be resolved, do not silently
        // bind to an arbitrary camera. This avoids stale/wrong-camera output in PIE.
        return nullptr;
    }

    AActor* FindSourceAnchorActorByEntityId(URshipSubsystem* Subsystem, const FString& SourceId)
    {
        if (!GEngine)
        {
            return nullptr;
        }

        const FString RequestedId = SourceId.TrimStartAndEnd();
        const FString RequestedShortId = GetShortIdToken(RequestedId);
        const bool bAllowPartialMatch = RequestedShortId.Len() >= 4 || RequestedId.Len() >= 4;
        if (RequestedId.IsEmpty())
        {
            return nullptr;
        }

        if (Subsystem)
        {
            if (URshipActorRegistrationComponent* Registration = Subsystem->FindTargetComponent(RequestedId))
            {
                if (AActor* Owner = Registration->GetOwner())
                {
                    return Owner;
                }
            }
        }

        for (int32 Pass = 0; Pass < 3; ++Pass)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !IsRelevantContentMappingWorldType(Context.WorldType))
                {
                    continue;
                }

                const bool bIsPlay = IsPlayContentMappingWorldType(Context.WorldType);
                const bool bIsEditor = IsEditorContentMappingWorldType(Context.WorldType);
                if (Pass == 0 && !bIsPlay)
                {
                    continue;
                }
                if (Pass == 1 && !bIsEditor)
                {
                    continue;
                }
                if (Pass == 2 && (bIsPlay || bIsEditor))
                {
                    continue;
                }

                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Candidate = *It;
                    if (!Candidate)
                    {
                        continue;
                    }

                    if (DoesActorMatchRequestedId(Subsystem, Candidate, RequestedId, RequestedShortId, bAllowPartialMatch))
                    {
                        return Candidate;
                    }
                }
            }
        }

        return nullptr;
    }

    AActor* FindActorByNameToken(const FString& ActorToken, bool bPreferScreenActors)
    {
        if (!GEngine)
        {
            return nullptr;
        }

        const FString Requested = ActorToken.TrimStartAndEnd();
        if (Requested.IsEmpty())
        {
            return nullptr;
        }

        AActor* FirstNameMatch = nullptr;
        AActor* FirstAnyMatch = nullptr;

        for (int32 Pass = 0; Pass < 3; ++Pass)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !IsRelevantContentMappingWorldType(Context.WorldType))
                {
                    continue;
                }

                const bool bIsPlay = IsPlayContentMappingWorldType(Context.WorldType);
                const bool bIsEditor = IsEditorContentMappingWorldType(Context.WorldType);
                if (Pass == 0 && !bIsPlay)
                {
                    continue;
                }
                if (Pass == 1 && !bIsEditor)
                {
                    continue;
                }
                if (Pass == 2 && (bIsPlay || bIsEditor))
                {
                    continue;
                }

                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Candidate = *It;
                    if (!Candidate)
                    {
                        continue;
                    }

                    if (bPreferScreenActors && !IsLikelyScreenActor(Candidate))
                    {
                        continue;
                    }

                    if (!FirstAnyMatch)
                    {
                        FirstAnyMatch = Candidate;
                    }

                    const FString CandidateName = Candidate->GetName();
                    const FString CandidateLabel = GetActorLabelCompat(Candidate);
                    if (CandidateName.Equals(Requested, ESearchCase::IgnoreCase)
                        || CandidateLabel.Equals(Requested, ESearchCase::IgnoreCase))
                    {
                        return Candidate;
                    }

                    if (!FirstNameMatch
                        && (CandidateName.Contains(Requested, ESearchCase::IgnoreCase)
                            || CandidateLabel.Contains(Requested, ESearchCase::IgnoreCase)))
                    {
                        FirstNameMatch = Candidate;
                    }
                }
            }
        }

        return FirstNameMatch ? FirstNameMatch : FirstAnyMatch;
    }

    UMaterialInterface* TryLoadMaterialPath(const FString& RawPath)
    {
        if (RawPath.IsEmpty())
        {
            return nullptr;
        }

        const FString Trimmed = RawPath.TrimStartAndEnd();
        if (Trimmed.IsEmpty())
        {
            return nullptr;
        }

        if (UObject* Loaded = StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *Trimmed))
        {
            return Cast<UMaterialInterface>(Loaded);
        }

        const FSoftObjectPath SoftPath(Trimmed);
        if (SoftPath.IsValid())
        {
            if (UObject* SoftLoaded = SoftPath.TryLoad())
            {
                return Cast<UMaterialInterface>(SoftLoaded);
            }
        }

        return nullptr;
    }

    FString JsonToString(const TSharedPtr<FJsonObject>& JsonObj)
    {
        if (!JsonObj.IsValid())
        {
            return FString();
        }

        FString Out;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
        FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
        return Out;
    }

    TSharedPtr<FJsonObject> DeepCloneJsonObject(const TSharedPtr<FJsonObject>& InObject)
    {
        if (!InObject.IsValid())
        {
            return MakeShared<FJsonObject>();
        }

        const FString Serialized = JsonToString(InObject);
        if (Serialized.IsEmpty())
        {
            return MakeShared<FJsonObject>();
        }

        TSharedPtr<FJsonObject> Cloned = MakeShared<FJsonObject>();
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
        if (!FJsonSerializer::Deserialize(Reader, Cloned) || !Cloned.IsValid())
        {
            return MakeShared<FJsonObject>();
        }
        return Cloned;
    }

    uint32 HashJsonPayload(const TSharedPtr<FJsonObject>& JsonObj)
    {
        return FCrc::StrCrc32(*JsonToString(JsonObj));
    }

    bool AreJsonObjectsEqual(const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
    {
        return JsonToString(A) == JsonToString(B);
    }

    bool AreIntArraysEqual(const TArray<int32>& A, const TArray<int32>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }

        TArray<int32> SortedA = A;
        TArray<int32> SortedB = B;
        SortedA.Sort();
        SortedB.Sort();
        return SortedA == SortedB;
    }

    bool AreStringArraysEqual(const TArray<FString>& A, const TArray<FString>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }

        TArray<FString> SortedA = A;
        TArray<FString> SortedB = B;
        SortedA.Sort();
        SortedB.Sort();
        return SortedA == SortedB;
    }

    bool AreRenderContextStatesEquivalent(const FRshipRenderContextState& A, const FRshipRenderContextState& B)
    {
        return A.Id == B.Id
            && A.Name == B.Name
            && A.ProjectId == B.ProjectId
            && A.SourceType == B.SourceType
            && A.CameraId == B.CameraId
            && A.AssetId == B.AssetId
            && A.DepthAssetId == B.DepthAssetId
            && A.ExternalSourceId == B.ExternalSourceId
            && A.Width == B.Width
            && A.Height == B.Height
            && A.CaptureMode == B.CaptureMode
            && A.DepthCaptureMode == B.DepthCaptureMode
            && A.bEnabled == B.bEnabled
            && A.bDepthCaptureEnabled == B.bDepthCaptureEnabled;
    }

    bool AreRenderContextStatesFunctionallyEquivalent(const FRshipRenderContextState& A, const FRshipRenderContextState& B)
    {
        return A.ProjectId == B.ProjectId
            && A.SourceType.Equals(B.SourceType, ESearchCase::IgnoreCase)
            && A.CameraId.Equals(B.CameraId, ESearchCase::IgnoreCase)
            && A.AssetId.Equals(B.AssetId, ESearchCase::IgnoreCase)
            && A.DepthAssetId.Equals(B.DepthAssetId, ESearchCase::IgnoreCase)
            && A.ExternalSourceId.Equals(B.ExternalSourceId, ESearchCase::IgnoreCase)
            && A.Width == B.Width
            && A.Height == B.Height
            && A.CaptureMode.Equals(B.CaptureMode, ESearchCase::IgnoreCase)
            && A.DepthCaptureMode.Equals(B.DepthCaptureMode, ESearchCase::IgnoreCase)
            && A.bEnabled == B.bEnabled
            && A.bDepthCaptureEnabled == B.bDepthCaptureEnabled;
    }

    bool AreMappingSurfaceStatesEquivalent(const FRshipMappingSurfaceState& A, const FRshipMappingSurfaceState& B)
    {
        return A.Id == B.Id
            && A.Name == B.Name
            && A.ProjectId == B.ProjectId
            && A.TargetId == B.TargetId
            && A.bEnabled == B.bEnabled
            && A.UVChannel == B.UVChannel
            && A.MeshComponentName == B.MeshComponentName
            && A.ActorPath == B.ActorPath
            && AreIntArraysEqual(A.MaterialSlots, B.MaterialSlots);
    }

    void NormalizeMappingSurfaceState(FRshipMappingSurfaceState& State, URshipSubsystem* Subsystem)
    {
        (void)Subsystem;
        State.Name = State.Name.TrimStartAndEnd();
        State.ProjectId = State.ProjectId.TrimStartAndEnd();
        State.TargetId.Reset();
        State.MeshComponentName = State.MeshComponentName.TrimStartAndEnd();
        State.ActorPath = State.ActorPath.TrimStartAndEnd();
        State.UVChannel = FMath::Max(0, State.UVChannel);

        TArray<int32> SanitizedSlots;
        TSet<int32> SeenSlots;
        for (int32 Slot : State.MaterialSlots)
        {
            if (Slot >= 0 && !SeenSlots.Contains(Slot))
            {
                SanitizedSlots.Add(Slot);
                SeenSlots.Add(Slot);
            }
        }
        State.MaterialSlots = MoveTemp(SanitizedSlots);
    }

    bool AreMappingStatesEquivalent(const FRshipContentMappingState& A, const FRshipContentMappingState& B)
    {
        return A.Id == B.Id
            && A.Name == B.Name
            && A.ProjectId == B.ProjectId
            && A.Type == B.Type
            && A.ContextId == B.ContextId
            && FMath::IsNearlyEqual(A.Opacity, B.Opacity)
            && A.bEnabled == B.bEnabled
            && AreStringArraysEqual(A.SurfaceIds, B.SurfaceIds)
            && AreJsonObjectsEqual(A.Config, B.Config);
    }

    FString NormalizeSourceTypeToken(const FString& InSourceType)
    {
        const FString Value = InSourceType.TrimStartAndEnd().ToLower();
        if (Value.IsEmpty())
        {
            return FString();
        }

        if (Value == TEXT("camera")
            || Value == TEXT("scene-camera")
            || Value == TEXT("scene camera")
            || Value == TEXT("cinecamera")
            || Value == TEXT("cine-camera")
            || Value == TEXT("camera-actor")
            || Value == TEXT("camera actor")
            || Value == TEXT("mesh-camera")
            || Value == TEXT("mesh camera")
            || Value == TEXT("projection-camera")
            || Value == TEXT("projection camera")
            || Value == TEXT("ndisplay")
            || Value == TEXT("n-display")
            || Value == TEXT("ndisplay-camera")
            || Value == TEXT("displaycluster"))
        {
            return TEXT("camera");
        }

        if (Value == TEXT("asset-store")
            || Value == TEXT("asset store")
            || Value == TEXT("asset")
            || Value == TEXT("texture")
            || Value == TEXT("image")
            || Value == TEXT("media")
            || Value == TEXT("media-texture")
            || Value == TEXT("file"))
        {
            return TEXT("asset-store");
        }

        if (Value == TEXT("external")
            || Value == TEXT("external-texture")
            || Value == TEXT("external texture")
            || Value == TEXT("stream-texture")
            || Value == TEXT("stream texture")
            || Value == TEXT("media-profile")
            || Value == TEXT("media profile")
            || Value == TEXT("receiver")
            || Value == TEXT("st2110")
            || Value == TEXT("2110"))
        {
            return TEXT("external-texture");
        }

        return Value;
    }

    FString NormalizeProjectionModeToken(const FString& InMode, const FString& DefaultMode = TEXT("perspective"))
    {
        FString Value = InMode.TrimStartAndEnd().ToLower();
        if (Value.IsEmpty())
        {
            Value = DefaultMode.TrimStartAndEnd().ToLower();
        }

        if (Value == TEXT("surface uv") || Value == TEXT("surface-uv") || Value == TEXT("uv"))
        {
            return TEXT("direct");
        }
        if (Value == TEXT("surface feed") || Value == TEXT("surface-feed"))
        {
            return TEXT("feed");
        }
        if (Value == TEXT("direct") || Value == TEXT("feed"))
        {
            return Value;
        }
        if (Value == TEXT("custom matrix") || Value == TEXT("matrix") || Value == TEXT("custommatrix"))
        {
            return TEXT("custom-matrix");
        }
        if (Value == TEXT("camera plate") || Value == TEXT("cameraplate"))
        {
            return TEXT("camera-plate");
        }
        if (Value == TEXT("depth map") || Value == TEXT("depthmap"))
        {
            return TEXT("depth-map");
        }
        if (Value == TEXT("orthographic") || Value == TEXT("ortho") || Value == TEXT("planar"))
        {
            return TEXT("parallel");
        }
        if (Value == TEXT("ndisplay")
            || Value == TEXT("n-display")
            || Value == TEXT("mesh-camera")
            || Value == TEXT("mesh camera")
            || Value == TEXT("mesh-projection")
            || Value == TEXT("mesh projection"))
        {
            return TEXT("mesh");
        }
        if (Value == TEXT("mesh"))
        {
            return TEXT("mesh");
        }
        if (Value == TEXT("projection") || Value == TEXT("projector"))
        {
            return TEXT("perspective");
        }

        if (Value == TEXT("perspective")
            || Value == TEXT("cylindrical")
            || Value == TEXT("spherical")
            || Value == TEXT("parallel")
            || Value == TEXT("radial")
            || Value == TEXT("mesh")
            || Value == TEXT("fisheye")
            || Value == TEXT("custom-matrix")
            || Value == TEXT("camera-plate")
            || Value == TEXT("spatial")
            || Value == TEXT("depth-map"))
        {
            return Value;
        }

        return DefaultMode.TrimStartAndEnd().ToLower();
    }

    FString NormalizeUvModeToken(const FString& InMode, const FString& DefaultMode = TEXT("direct"))
    {
        FString Value = InMode.TrimStartAndEnd().ToLower();
        if (Value.IsEmpty())
        {
            Value = DefaultMode.TrimStartAndEnd().ToLower();
        }

        if (Value == TEXT("surface-feed"))
        {
            return TEXT("feed");
        }
        if (Value != TEXT("feed"))
        {
            return TEXT("direct");
        }
        return Value;
    }

    void NormalizeRenderContextState(FRshipRenderContextState& State)
    {
        State.Name = State.Name.TrimStartAndEnd();
        State.ProjectId = State.ProjectId.TrimStartAndEnd();
        State.CameraId = State.CameraId.TrimStartAndEnd();
        State.AssetId = State.AssetId.TrimStartAndEnd();
        State.DepthAssetId = State.DepthAssetId.TrimStartAndEnd();
        State.ExternalSourceId = State.ExternalSourceId.TrimStartAndEnd();
        State.CaptureMode = State.CaptureMode.TrimStartAndEnd();
        State.DepthCaptureMode = State.DepthCaptureMode.TrimStartAndEnd();

        FString SourceType = NormalizeSourceTypeToken(State.SourceType);
        if (SourceType.IsEmpty())
        {
            if (!State.ExternalSourceId.IsEmpty() && State.CameraId.IsEmpty() && State.AssetId.IsEmpty())
            {
                SourceType = TEXT("external-texture");
            }
            else
            {
                SourceType = (!State.AssetId.IsEmpty() && State.CameraId.IsEmpty()) ? TEXT("asset-store") : TEXT("camera");
            }
        }

        if (SourceType != TEXT("camera")
            && SourceType != TEXT("asset-store")
            && SourceType != TEXT("external-texture"))
        {
            if (!State.CameraId.IsEmpty())
            {
                SourceType = TEXT("camera");
            }
            else if (!State.AssetId.IsEmpty())
            {
                SourceType = TEXT("asset-store");
            }
            else if (!State.ExternalSourceId.IsEmpty())
            {
                SourceType = TEXT("external-texture");
            }
            else
            {
                SourceType = TEXT("camera");
            }
        }

        if (SourceType == TEXT("camera") && State.CameraId.IsEmpty() && !State.AssetId.IsEmpty())
        {
            SourceType = TEXT("asset-store");
        }
        else if (SourceType == TEXT("asset-store") && State.AssetId.IsEmpty() && !State.CameraId.IsEmpty())
        {
            SourceType = TEXT("camera");
        }
        else if (SourceType == TEXT("external-texture")
            && State.ExternalSourceId.IsEmpty()
            && !State.CameraId.IsEmpty())
        {
            SourceType = TEXT("camera");
        }
        else if (SourceType == TEXT("external-texture")
            && State.ExternalSourceId.IsEmpty()
            && !State.AssetId.IsEmpty())
        {
            SourceType = TEXT("asset-store");
        }
        else if (SourceType == TEXT("camera")
            && State.CameraId.IsEmpty()
            && State.AssetId.IsEmpty()
            && !State.ExternalSourceId.IsEmpty())
        {
            SourceType = TEXT("external-texture");
        }
        else if (SourceType == TEXT("asset-store")
            && State.AssetId.IsEmpty()
            && State.CameraId.IsEmpty()
            && !State.ExternalSourceId.IsEmpty())
        {
            SourceType = TEXT("external-texture");
        }

        State.SourceType = SourceType;

        if (State.SourceType == TEXT("camera"))
        {
            State.AssetId.Reset();
            State.ExternalSourceId.Reset();
        }
        else if (State.SourceType == TEXT("asset-store"))
        {
            State.CameraId.Reset();
            State.ExternalSourceId.Reset();
        }
        else if (State.SourceType == TEXT("external-texture"))
        {
            State.CameraId.Reset();
            State.AssetId.Reset();
        }

        if (State.Width <= 0)
        {
            State.Width = 1920;
        }
        if (State.Height <= 0)
        {
            State.Height = 1080;
        }
        if (State.CaptureMode.IsEmpty())
        {
            State.CaptureMode = TEXT("FinalColorLDR");
        }
        if (State.DepthCaptureMode.IsEmpty())
        {
            State.DepthCaptureMode = TEXT("SceneDepth");
        }
    }

    void NormalizeMappingState(FRshipContentMappingState& State)
    {
        State.Name = State.Name.TrimStartAndEnd();
        State.ProjectId = State.ProjectId.TrimStartAndEnd();
        State.ContextId = State.ContextId.TrimStartAndEnd();
        State.Opacity = FMath::Clamp(State.Opacity, 0.0f, 1.0f);

        TArray<FString> SanitizedSurfaceIds;
        TSet<FString> SeenSurfaceIds;
        for (const FString& RawSurfaceId : State.SurfaceIds)
        {
            const FString SurfaceId = RawSurfaceId.TrimStartAndEnd();
            if (!SurfaceId.IsEmpty() && !SeenSurfaceIds.Contains(SurfaceId))
            {
                SanitizedSurfaceIds.Add(SurfaceId);
                SeenSurfaceIds.Add(SurfaceId);
            }
        }
        State.SurfaceIds = MoveTemp(SanitizedSurfaceIds);

        if (!State.Config.IsValid())
        {
            State.Config = MakeShared<FJsonObject>();
        }

        auto ReadNumber = [](const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float DefaultValue) -> float
        {
            if (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field))
            {
                return static_cast<float>(Obj->GetNumberField(Field));
            }
            return DefaultValue;
        };

        auto ReadString = [](const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& DefaultValue) -> FString
        {
            if (Obj.IsValid() && Obj->HasTypedField<EJson::String>(Field))
            {
                return Obj->GetStringField(Field);
            }
            return DefaultValue;
        };

        auto EnsureVector3Object = [&ReadNumber](const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float X, float Y, float Z)
        {
            TSharedPtr<FJsonObject> VecObj = Obj->HasTypedField<EJson::Object>(Field)
                ? Obj->GetObjectField(Field)
                : MakeShared<FJsonObject>();
            VecObj->SetNumberField(TEXT("x"), ReadNumber(VecObj, TEXT("x"), X));
            VecObj->SetNumberField(TEXT("y"), ReadNumber(VecObj, TEXT("y"), Y));
            VecObj->SetNumberField(TEXT("z"), ReadNumber(VecObj, TEXT("z"), Z));
            Obj->SetObjectField(Field, VecObj);
        };

        auto EnsureCustomMatrixObject = [&ReadNumber](const TSharedPtr<FJsonObject>& Obj)
        {
            TSharedPtr<FJsonObject> MatrixObj;
            if (Obj->HasTypedField<EJson::Object>(TEXT("customProjectionMatrix")))
            {
                MatrixObj = Obj->GetObjectField(TEXT("customProjectionMatrix"));
            }
            else if (Obj->HasTypedField<EJson::Object>(TEXT("matrix")))
            {
                MatrixObj = Obj->GetObjectField(TEXT("matrix"));
            }
            else
            {
                MatrixObj = MakeShared<FJsonObject>();
            }

            for (int32 Row = 0; Row < 4; ++Row)
            {
                for (int32 Col = 0; Col < 4; ++Col)
                {
                    const FString FieldName = FString::Printf(TEXT("m%d%d"), Row, Col);
                    MatrixObj->SetNumberField(FieldName, ReadNumber(MatrixObj, *FieldName, Row == Col ? 1.0f : 0.0f));
                }
            }
            Obj->SetObjectField(TEXT("customProjectionMatrix"), MatrixObj);
            Obj->RemoveField(TEXT("matrix"));
        };

        const FString RawType = State.Type.TrimStartAndEnd().ToLower();
        bool bUseUvType = false;
        bool bUseProjectionType = false;
        FString RequestedUvMode;
        FString RequestedProjectionMode;

        if (RawType == TEXT("surface-uv"))
        {
            bUseUvType = true;
        }
        else if (RawType == TEXT("direct")
            || RawType == TEXT("feed")
            || RawType == TEXT("surface-feed"))
        {
            bUseUvType = true;
            RequestedUvMode = RawType;
        }
        else if (RawType == TEXT("surface-projection"))
        {
            bUseProjectionType = true;
        }
        else if (!RawType.IsEmpty())
        {
            bUseProjectionType = true;
            RequestedProjectionMode = RawType;
        }

        if (!bUseUvType && !bUseProjectionType)
        {
            if (State.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
            {
                bUseUvType = true;
                RequestedUvMode = TEXT("feed");
            }
            else if (State.Config->HasTypedField<EJson::String>(TEXT("uvMode")))
            {
                bUseUvType = true;
                RequestedUvMode = State.Config->GetStringField(TEXT("uvMode"));
            }
            else
            {
                bUseProjectionType = true;
                RequestedProjectionMode = ReadString(State.Config, TEXT("projectionType"), TEXT("perspective"));
            }
        }

        if (bUseUvType)
        {
            State.Type = TEXT("surface-uv");
            const FString UvMode = NormalizeUvModeToken(
                RequestedUvMode.IsEmpty() ? ReadString(State.Config, TEXT("uvMode"), TEXT("direct")) : RequestedUvMode,
                TEXT("direct"));
            State.Config->SetStringField(TEXT("uvMode"), UvMode);
            State.Config->RemoveField(TEXT("projectionType"));

            TSharedPtr<FJsonObject> UvTransform = State.Config->HasTypedField<EJson::Object>(TEXT("uvTransform"))
                ? State.Config->GetObjectField(TEXT("uvTransform"))
                : MakeShared<FJsonObject>();
            UvTransform->SetNumberField(TEXT("scaleU"), ReadNumber(UvTransform, TEXT("scaleU"), 1.0f));
            UvTransform->SetNumberField(TEXT("scaleV"), ReadNumber(UvTransform, TEXT("scaleV"), 1.0f));
            UvTransform->SetNumberField(TEXT("offsetU"), ReadNumber(UvTransform, TEXT("offsetU"), 0.0f));
            UvTransform->SetNumberField(TEXT("offsetV"), ReadNumber(UvTransform, TEXT("offsetV"), 0.0f));
            UvTransform->SetNumberField(TEXT("rotationDeg"), ReadNumber(UvTransform, TEXT("rotationDeg"), 0.0f));
            UvTransform->SetNumberField(TEXT("pivotU"), ReadNumber(UvTransform, TEXT("pivotU"), 0.5f));
            UvTransform->SetNumberField(TEXT("pivotV"), ReadNumber(UvTransform, TEXT("pivotV"), 0.5f));
            State.Config->SetObjectField(TEXT("uvTransform"), UvTransform);

            if (UvMode == TEXT("feed"))
            {
                if (State.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
                {
                    TSharedPtr<FJsonObject> FeedV2 = State.Config->GetObjectField(TEXT("feedV2"));
                    const FString CoordSpace = ReadString(FeedV2, TEXT("coordinateSpace"), TEXT("pixel")).TrimStartAndEnd().ToLower();
                    FeedV2->SetStringField(TEXT("coordinateSpace"), CoordSpace.IsEmpty() ? TEXT("pixel") : CoordSpace);
                    if (!FeedV2->HasTypedField<EJson::Array>(TEXT("sources")))
                    {
                        FeedV2->SetArrayField(TEXT("sources"), TArray<TSharedPtr<FJsonValue>>());
                    }
                    if (!FeedV2->HasTypedField<EJson::Array>(TEXT("destinations")))
                    {
                        FeedV2->SetArrayField(TEXT("destinations"), TArray<TSharedPtr<FJsonValue>>());
                    }
                    if (!FeedV2->HasTypedField<EJson::Array>(TEXT("routes")))
                    {
                        FeedV2->SetArrayField(TEXT("routes"), TArray<TSharedPtr<FJsonValue>>());
                    }
                    State.Config->SetObjectField(TEXT("feedV2"), FeedV2);
                }
            }

            return;
        }

        State.Type = TEXT("surface-projection");
        const FString ProjectionMode = NormalizeProjectionModeToken(
            RequestedProjectionMode.IsEmpty()
                ? ReadString(State.Config, TEXT("projectionType"), TEXT("perspective"))
                : RequestedProjectionMode,
            TEXT("perspective"));
        State.Config->SetStringField(TEXT("projectionType"), ProjectionMode);
        State.Config->RemoveField(TEXT("uvMode"));

        EnsureVector3Object(State.Config, TEXT("projectorPosition"), 0.0f, 0.0f, 0.0f);
        EnsureVector3Object(State.Config, TEXT("projectorRotation"), 0.0f, 0.0f, 0.0f);
        double DefaultEyeX = 0.0;
        double DefaultEyeY = 0.0;
        double DefaultEyeZ = 0.0;
        if (State.Config->HasTypedField<EJson::Object>(TEXT("projectorPosition")))
        {
            const TSharedPtr<FJsonObject> ProjectorPos = State.Config->GetObjectField(TEXT("projectorPosition"));
            DefaultEyeX = ReadNumber(ProjectorPos, TEXT("x"), 0.0);
            DefaultEyeY = ReadNumber(ProjectorPos, TEXT("y"), 0.0);
            DefaultEyeZ = ReadNumber(ProjectorPos, TEXT("z"), 0.0);
        }
        EnsureVector3Object(State.Config, TEXT("eyepoint"), DefaultEyeX, DefaultEyeY, DefaultEyeZ);
        State.Config->SetNumberField(TEXT("fov"), ReadNumber(State.Config, TEXT("fov"), 60.0f));
        State.Config->SetNumberField(TEXT("aspectRatio"), ReadNumber(State.Config, TEXT("aspectRatio"), 1.7778f));
        State.Config->SetNumberField(TEXT("near"), ReadNumber(State.Config, TEXT("near"), 10.0f));
        State.Config->SetNumberField(TEXT("far"), ReadNumber(State.Config, TEXT("far"), 10000.0f));
        State.Config->SetNumberField(TEXT("angleMaskStart"), ReadNumber(State.Config, TEXT("angleMaskStart"), 0.0f));
        State.Config->SetNumberField(TEXT("angleMaskEnd"), ReadNumber(State.Config, TEXT("angleMaskEnd"), 360.0f));
        State.Config->SetBoolField(TEXT("clipOutsideRegion"), State.Config->HasTypedField<EJson::Boolean>(TEXT("clipOutsideRegion"))
            ? State.Config->GetBoolField(TEXT("clipOutsideRegion"))
            : false);
        State.Config->SetNumberField(TEXT("borderExpansion"), ReadNumber(State.Config, TEXT("borderExpansion"), 0.0f));

        if (ProjectionMode == TEXT("cylindrical") || ProjectionMode == TEXT("radial"))
        {
            TSharedPtr<FJsonObject> Cyl = State.Config->HasTypedField<EJson::Object>(TEXT("cylindrical"))
                ? State.Config->GetObjectField(TEXT("cylindrical"))
                : MakeShared<FJsonObject>();
            FString Axis = ReadString(Cyl, TEXT("axis"), TEXT("y")).TrimStartAndEnd().ToLower();
            if (Axis.IsEmpty())
            {
                Axis = TEXT("y");
            }
            Cyl->SetStringField(TEXT("axis"), Axis);
            Cyl->SetNumberField(TEXT("radius"), ReadNumber(Cyl, TEXT("radius"), 100.0f));
            Cyl->SetNumberField(TEXT("height"), ReadNumber(Cyl, TEXT("height"), 1000.0f));
            Cyl->SetNumberField(TEXT("startAngle"), ReadNumber(Cyl, TEXT("startAngle"), 0.0f));
            Cyl->SetNumberField(TEXT("endAngle"), ReadNumber(Cyl, TEXT("endAngle"), 90.0f));
            State.Config->SetObjectField(TEXT("cylindrical"), Cyl);
        }

        if (ProjectionMode == TEXT("spherical"))
        {
            State.Config->SetNumberField(TEXT("sphereRadius"), ReadNumber(State.Config, TEXT("sphereRadius"), 500.0f));
            State.Config->SetNumberField(TEXT("horizontalArc"), ReadNumber(State.Config, TEXT("horizontalArc"), 360.0f));
            State.Config->SetNumberField(TEXT("verticalArc"), ReadNumber(State.Config, TEXT("verticalArc"), 180.0f));
        }

        if (ProjectionMode == TEXT("parallel"))
        {
            State.Config->SetNumberField(TEXT("sizeW"), ReadNumber(State.Config, TEXT("sizeW"), 1000.0f));
            State.Config->SetNumberField(TEXT("sizeH"), ReadNumber(State.Config, TEXT("sizeH"), 1000.0f));
        }

        if (ProjectionMode == TEXT("fisheye"))
        {
            State.Config->SetNumberField(TEXT("fisheyeFov"), ReadNumber(State.Config, TEXT("fisheyeFov"), 180.0f));
            State.Config->SetStringField(TEXT("lensType"), ReadString(State.Config, TEXT("lensType"), TEXT("equidistant")));
        }

        if (ProjectionMode == TEXT("camera-plate"))
        {
            TSharedPtr<FJsonObject> CameraPlate = State.Config->HasTypedField<EJson::Object>(TEXT("cameraPlate"))
                ? State.Config->GetObjectField(TEXT("cameraPlate"))
                : MakeShared<FJsonObject>();
            CameraPlate->SetStringField(TEXT("fit"), ReadString(CameraPlate, TEXT("fit"), TEXT("contain")));
            CameraPlate->SetStringField(TEXT("anchor"), ReadString(CameraPlate, TEXT("anchor"), TEXT("center")));
            CameraPlate->SetBoolField(TEXT("flipV"), CameraPlate->HasTypedField<EJson::Boolean>(TEXT("flipV"))
                ? CameraPlate->GetBoolField(TEXT("flipV"))
                : false);
            State.Config->SetObjectField(TEXT("cameraPlate"), CameraPlate);
        }

        if (ProjectionMode == TEXT("spatial"))
        {
            TSharedPtr<FJsonObject> Spatial = State.Config->HasTypedField<EJson::Object>(TEXT("spatial"))
                ? State.Config->GetObjectField(TEXT("spatial"))
                : MakeShared<FJsonObject>();
            Spatial->SetNumberField(TEXT("scaleU"), ReadNumber(Spatial, TEXT("scaleU"), 1.0f));
            Spatial->SetNumberField(TEXT("scaleV"), ReadNumber(Spatial, TEXT("scaleV"), 1.0f));
            Spatial->SetNumberField(TEXT("offsetU"), ReadNumber(Spatial, TEXT("offsetU"), 0.0f));
            Spatial->SetNumberField(TEXT("offsetV"), ReadNumber(Spatial, TEXT("offsetV"), 0.0f));
            State.Config->SetObjectField(TEXT("spatial"), Spatial);
        }

        if (ProjectionMode == TEXT("depth-map"))
        {
            TSharedPtr<FJsonObject> DepthMap = State.Config->HasTypedField<EJson::Object>(TEXT("depthMap"))
                ? State.Config->GetObjectField(TEXT("depthMap"))
                : MakeShared<FJsonObject>();
            const float DepthScale = ReadNumber(DepthMap, TEXT("depthScale"), ReadNumber(State.Config, TEXT("depthScale"), 1.0f));
            const float DepthBias = ReadNumber(DepthMap, TEXT("depthBias"), ReadNumber(State.Config, TEXT("depthBias"), 0.0f));
            const float DepthNear = ReadNumber(DepthMap, TEXT("depthNear"), ReadNumber(State.Config, TEXT("depthNear"), 0.0f));
            const float DepthFar = ReadNumber(DepthMap, TEXT("depthFar"), ReadNumber(State.Config, TEXT("depthFar"), 1.0f));
            DepthMap->SetNumberField(TEXT("depthScale"), DepthScale);
            DepthMap->SetNumberField(TEXT("depthBias"), DepthBias);
            DepthMap->SetNumberField(TEXT("depthNear"), DepthNear);
            DepthMap->SetNumberField(TEXT("depthFar"), DepthFar);
            State.Config->SetObjectField(TEXT("depthMap"), DepthMap);
            State.Config->SetNumberField(TEXT("depthScale"), DepthScale);
            State.Config->SetNumberField(TEXT("depthBias"), DepthBias);
            State.Config->SetNumberField(TEXT("depthNear"), DepthNear);
            State.Config->SetNumberField(TEXT("depthFar"), DepthFar);
        }

        if (ProjectionMode == TEXT("custom-matrix"))
        {
            EnsureCustomMatrixObject(State.Config);
        }
    }

    struct FCanonicalMappingMode
    {
        FString CanonicalType;
        FString CanonicalMode;
    };

    FCanonicalMappingMode GetCanonicalMappingMode(const FRshipContentMappingState& InState)
    {
        FRshipContentMappingState Normalized = InState;
        NormalizeMappingState(Normalized);

        auto ReadConfigString = [](const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& DefaultValue) -> FString
        {
            if (Obj.IsValid() && Obj->HasTypedField<EJson::String>(Field))
            {
                return Obj->GetStringField(Field);
            }
            return DefaultValue;
        };

        FCanonicalMappingMode Mode;
        Mode.CanonicalType = Normalized.Type.TrimStartAndEnd().ToLower();
        if (Mode.CanonicalType == TEXT("surface-uv"))
        {
            Mode.CanonicalMode = NormalizeUvModeToken(
                ReadConfigString(Normalized.Config, TEXT("uvMode"), TEXT("direct")),
                TEXT("direct"));
            return Mode;
        }

        Mode.CanonicalType = TEXT("surface-projection");
        Mode.CanonicalMode = NormalizeProjectionModeToken(
            ReadConfigString(Normalized.Config, TEXT("projectionType"), TEXT("perspective")),
            TEXT("perspective"));
        return Mode;
    }

    bool AreCanonicalMappingModesEquivalent(
        const FCanonicalMappingMode& A,
        const FCanonicalMappingMode& B)
    {
        return A.CanonicalType.Equals(B.CanonicalType, ESearchCase::IgnoreCase)
            && A.CanonicalMode.Equals(B.CanonicalMode, ESearchCase::IgnoreCase);
    }
}

URshipContentMappingManager* URshipContentMappingManager::Get()
{
    return GEngine ? GEngine->GetEngineSubsystem<URshipContentMappingManager>() : nullptr;
}

void URshipContentMappingManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RefreshSubsystemBinding();
    Initialize(Subsystem);
}

void URshipContentMappingManager::Deinitialize()
{
    Shutdown();
    Super::Deinitialize();
}

void URshipContentMappingManager::RefreshSubsystemBinding()
{
    const bool bHadSubsystem = Subsystem != nullptr;
    if (!Subsystem && GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!bHadSubsystem && Subsystem)
        {
            RegisterAllTargets();
        }
    }
}

URshipContentMappingTargetProxy* URshipContentMappingManager::EnsureTargetProxy(const FString& TargetId)
{
    if (TargetId.IsEmpty())
    {
        return nullptr;
    }

    if (TObjectPtr<URshipContentMappingTargetProxy>* Existing = TargetProxies.Find(TargetId))
    {
        if (*Existing)
        {
            (*Existing)->Initialize(this, TargetId);
            return Existing->Get();
        }
    }

    URshipContentMappingTargetProxy* Proxy = NewObject<URshipContentMappingTargetProxy>(this);
    if (!Proxy)
    {
        return nullptr;
    }

    Proxy->Initialize(this, TargetId);
    TargetProxies.Add(TargetId, Proxy);
    return Proxy;
}

void URshipContentMappingManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    bMappingsArmed = true;
    bCoveragePreviewEnabled = false;
    RuntimeHealth = ERshipContentMappingRuntimeHealth::Ready;
    RuntimeHealthReason.Empty();
    NextMaterialResolveAttemptSeconds = 0.0;
    bAutoBootstrapComplete = false;
    NextAutoBootstrapAttemptSeconds = 0.0;

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (Settings && !Settings->bEnableContentMapping)
    {
        return;
    }

    if (!AssetStoreClient)
    {
        AssetStoreClient = NewObject<URshipAssetStoreClient>(this);
        if (Settings && !Settings->AssetStoreUrl.IsEmpty())
        {
            AssetStoreClient->Connect(Settings->AssetStoreUrl);
        }
        AssetStoreClient->OnDownloadCompleteNative.AddUObject(
            this,
            &URshipContentMappingManager::OnAssetDownloaded);
        AssetStoreClient->OnDownloadFailedNative.AddUObject(
            this,
            &URshipContentMappingManager::OnAssetDownloadFailed);
    }

    ResolveContentMappingMaterial(/*bRequireProjectionContract=*/false);
    RunRuntimePreflight(/*bForceMaterialResolve=*/false);
    LoadCache();
    RunRuntimePreflight(/*bForceMaterialResolve=*/false);
    MarkMappingsDirty();
    RegisterAllTargets();
}

void URshipContentMappingManager::InitializeForSubsystem(URshipSubsystem* InSubsystem)
{
    Initialize(InSubsystem);
}

void URshipContentMappingManager::Shutdown()
{
    const bool bEngineExitRequested = IsEngineExitRequested();

    if (bCacheDirty)
    {
        SaveCache();
        bCacheDirty = false;
    }

    if (AssetStoreClient)
    {
        AssetStoreClient->Disconnect();
        AssetStoreClient = nullptr;
    }

    for (auto& Pair : MappingSurfaces)
    {
        FRshipMappingSurfaceState& SurfaceState = Pair.Value;
        if (!bEngineExitRequested)
        {
            RestoreSurfaceMaterials(SurfaceState);
        }
        SurfaceState.MaterialInstances.Empty();
        SurfaceState.OriginalMaterials.Empty();
        SurfaceState.MaterialBindingHashes.Empty();
        SurfaceState.MeshComponent.Reset();
    }

    for (auto& Pair : RenderContexts)
    {
        if (!bEngineExitRequested && Pair.Value.CameraActor.IsValid())
        {
            Pair.Value.CameraActor->Destroy();
        }
        Pair.Value.CameraActor.Reset();
        Pair.Value.SourceCameraActor.Reset();
        Pair.Value.CaptureComponent.Reset();
        Pair.Value.CaptureRenderTarget.Reset();
        Pair.Value.DepthCaptureComponent.Reset();
        Pair.Value.DepthRenderTarget.Reset();
        Pair.Value.ResolvedTexture = nullptr;
        Pair.Value.ResolvedDepthTexture = nullptr;
    }

    if (Subsystem)
    {
        for (const auto& Pair : RenderContexts)
        {
            DeleteTargetForPath(BuildContextTargetId(Pair.Key));
        }
        for (const auto& Pair : MappingSurfaces)
        {
            DeleteTargetForPath(BuildSurfaceTargetId(Pair.Key));
        }
        for (const auto& Pair : Mappings)
        {
            DeleteTargetForPath(BuildMappingTargetId(Pair.Key));
        }
    }

    RenderContexts.Empty();
    MappingSurfaces.Empty();
    Mappings.Empty();
    FeedCompositeTargets.Empty();
    FeedCompositeStaticSignatures.Empty();
    EffectiveSurfaceIdsCache.Empty();
    RequiredContextIdsCache.Empty();
    RenderContextRuntimeStates.Empty();
    CachedEnabledTextureContextId.Reset();
    CachedAnyTextureContextId.Reset();
    CachedEnabledContextId.Reset();
    CachedAnyContextId.Reset();
    LastEmittedStateHashes.Empty();
    LastEmittedStatusHashes.Empty();
    AssetTextureCache.Empty();
    PendingAssetDownloads.Empty();
    ExternalTextureSources.Empty();
    bMappingsArmed = false;
    RuntimeHealth = ERshipContentMappingRuntimeHealth::Ready;
    RuntimeHealthReason.Empty();
    NextMaterialResolveAttemptSeconds = 0.0;
    bRuntimePreparePending = true;
    CacheSaveDueTimeSeconds = 0.0;
    bAutoBootstrapComplete = false;
    NextAutoBootstrapAttemptSeconds = 0.0;
    TargetProxies.Empty();
    Subsystem = nullptr;
}

void URshipContentMappingManager::ShutdownForSubsystem()
{
    Shutdown();
}

void URshipContentMappingManager::Tick(float DeltaTime)
{
    RefreshSubsystemBinding();
    if (!Subsystem)
    {
        return;
    }

    RunRuntimePreflight(/*bForceMaterialResolve=*/false);
    TryAutoBootstrapDefaults();

    const bool bConnected = Subsystem->IsConnected();
    if (bConnected && !bWasConnected)
    {
        RegisterAllTargets();
    }
    bWasConnected = bConnected;

    const double TickStartSeconds = FPlatformTime::Seconds();
    bool bDidRebuild = false;
    LastTickMsRebuild = 0.0f;
    LastTickMsRefresh = 0.0f;
    LastTickMsCacheSave = 0.0f;
    LastTickMsContextResolve = 0.0f;
    LastTickMsApplyMappings = 0.0f;
    LastTickMsConfigHash = 0.0f;
    LastTickMaterialBindingsUpdated = 0;
    LastTickMaterialBindingsSkipped = 0;

    if (bMappingsDirty)
    {
        const double RebuildStartSeconds = FPlatformTime::Seconds();
        bNeedsWorldResolutionRetry = false;
        RebuildMappings();
        bMappingsDirty = bNeedsWorldResolutionRetry;
        LastTickMsRebuild = static_cast<float>((FPlatformTime::Seconds() - RebuildStartSeconds) * 1000.0);
        bDidRebuild = true;
    }

    if (bCacheDirty)
    {
        const double NowSeconds = FPlatformTime::Seconds();
        if (CacheSaveDueTimeSeconds <= 0.0)
        {
            CacheSaveDueTimeSeconds = NowSeconds + 0.2;
        }
        if (NowSeconds >= CacheSaveDueTimeSeconds)
        {
            const double SaveStartSeconds = FPlatformTime::Seconds();
            SaveCache();
            bCacheDirty = false;
            CacheSaveDueTimeSeconds = 0.0;
            LastTickMsCacheSave = static_cast<float>((FPlatformTime::Seconds() - SaveStartSeconds) * 1000.0);
        }
    }

    const bool bHasEnabledMappings = HasAnyEnabledMappings();
    const bool bRequiresContinuousRefresh = bHasEnabledMappings && HasAnyMappingsRequiringContinuousRefresh();
    const bool bNeedsConvergenceRefresh = bHasEnabledMappings && HasPendingRuntimeBindings();
    if (bDidRebuild || bRequiresContinuousRefresh || bRuntimePreparePending || bNeedsConvergenceRefresh)
    {
        const double RefreshStartSeconds = FPlatformTime::Seconds();
        RefreshLiveMappings();
        LastTickMsRefresh = static_cast<float>((FPlatformTime::Seconds() - RefreshStartSeconds) * 1000.0);
    }
    else
    {
        LastTickEnabledMappings = 0;
        LastTickAppliedSurfaces = 0;
        LastTickActiveContexts = 0;
    }

    LastTickMsTotal = static_cast<float>((FPlatformTime::Seconds() - TickStartSeconds) * 1000.0);

    if (CVarRshipContentMappingPerfStats.GetValueOnGameThread() > 0)
    {
        const double NowSeconds = FPlatformTime::Seconds();
        if (LastPerfLogTimeSeconds <= 0.0 || (NowSeconds - LastPerfLogTimeSeconds) >= 1.0)
        {
            LastPerfLogTimeSeconds = NowSeconds;
            UE_LOG(LogRshipExec, Log,
                TEXT("CMPerf total=%.3fms rebuild=%.3fms refresh=%.3fms cache=%.3fms ctxResolve=%.3fms apply=%.3fms cfgHash=%.3fms enabled=%d contexts=%d appliedSurfaces=%d bindsUpdated=%d bindsSkipped=%d"),
                LastTickMsTotal,
                LastTickMsRebuild,
                LastTickMsRefresh,
                LastTickMsCacheSave,
                LastTickMsContextResolve,
                LastTickMsApplyMappings,
                LastTickMsConfigHash,
                LastTickEnabledMappings,
                LastTickActiveContexts,
                LastTickAppliedSurfaces,
                LastTickMaterialBindingsUpdated,
                LastTickMaterialBindingsSkipped);
        }
    }

    if (bDebugOverlayEnabled && GEngine)
    {
        DebugOverlayAccumulated += DeltaTime;
        if (DebugOverlayAccumulated >= 0.5f)
        {
            DebugOverlayAccumulated = 0.0f;

            int32 ContextErrors = 0;
            int32 SurfaceErrors = 0;
            int32 MappingErrors = 0;
            FString FirstError;

            for (const auto& Pair : RenderContexts)
            {
                if (!Pair.Value.LastError.IsEmpty())
                {
                    ContextErrors++;
                    if (FirstError.IsEmpty())
                    {
                        FirstError = Pair.Value.LastError;
                    }
                }
            }

            for (const auto& Pair : MappingSurfaces)
            {
                if (!Pair.Value.LastError.IsEmpty())
                {
                    SurfaceErrors++;
                    if (FirstError.IsEmpty())
                    {
                        FirstError = Pair.Value.LastError;
                    }
                }
            }

            for (const auto& Pair : Mappings)
            {
                if (!Pair.Value.LastError.IsEmpty())
                {
                    MappingErrors++;
                    if (FirstError.IsEmpty())
                    {
                        FirstError = Pair.Value.LastError;
                    }
                }
            }

            const bool bIsCurrentlyConnected = Subsystem && Subsystem->IsConnected();
            FString DebugText = FString::Printf(
                TEXT("Rship Content Mapping (%s)\nRuntime: %s\nContexts: %d (%d err)  Surfaces: %d (%d err)  Mappings: %d (%d err)\nPending assets: %d"),
                bIsCurrentlyConnected ? TEXT("connected") : TEXT("offline"),
                *GetRuntimeHealthStatusToken(),
                RenderContexts.Num(),
                ContextErrors,
                MappingSurfaces.Num(),
                SurfaceErrors,
                Mappings.Num(),
                MappingErrors,
                PendingAssetDownloads.Num());

            if (!FirstError.IsEmpty())
            {
                DebugText += FString::Printf(TEXT("\nLast error: %s"), *FirstError);
            }

            GEngine->AddOnScreenDebugMessage(0xC0FFEE, 0.6f, FColor::Cyan, DebugText);
        }
    }
}

void URshipContentMappingManager::TickForSubsystem(float DeltaSeconds)
{
    Tick(DeltaSeconds);
}

void URshipContentMappingManager::RefreshLiveMappings()
{
    if (!bMappingsArmed)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RefreshLiveMappings skipped: mappings are not armed"));
        LastTickEnabledMappings = 0;
        LastTickAppliedSurfaces = 0;
        LastTickActiveContexts = 0;
        return;
    }

    LastTickEnabledMappings = 0;
    LastTickAppliedSurfaces = 0;
    LastTickActiveContexts = 0;

    if (bRuntimePreparePending)
    {
        PrepareMappingsForRuntime(true);
    }

    if (IsRuntimeBlocked())
    {
        LastTickEnabledMappings = 0;
        LastTickAppliedSurfaces = 0;
        LastTickActiveContexts = 0;
        LastTickMsContextResolve = 0.0f;
        LastTickMsApplyMappings = 0.0f;
        LastTickMsConfigHash = 0.0f;
        LastTickMaterialBindingsUpdated = 0;
        LastTickMaterialBindingsSkipped = 0;
        return;
    }

    TSet<FString> RequiredContextIds;
    bool bHasEnabledMappings = false;
    bool bKeepAllContextsAlive = false;
    bool bHasInvalidContextReference = false;
    CollectRequiredContextIdsForMappings(
        RequiredContextIds,
        bHasEnabledMappings,
        bKeepAllContextsAlive,
        bHasInvalidContextReference);

    if (bKeepAllContextsAlive || bHasInvalidContextReference)
    {
        for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
        {
            if (!Pair.Key.IsEmpty())
            {
                RequiredContextIds.Add(Pair.Key);
            }
        }
    }

    if (RequiredContextIds.Num() == 0 && bHasEnabledMappings)
    {
        const FString PreferredContextId = GetPreferredRuntimeContextId();
        if (!PreferredContextId.IsEmpty())
        {
            RequiredContextIds.Add(PreferredContextId);
        }

        for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
        {
            if (!Pair.Key.IsEmpty())
            {
                RequiredContextIds.Add(Pair.Key);
                break;
            }
        }
    }

    auto DisableContextCapture = [](FRshipRenderContextState& ContextState)
    {
        if (USceneCaptureComponent2D* CaptureComponent = ContextState.CaptureComponent.Get())
        {
            CaptureComponent->bCaptureEveryFrame = false;
            CaptureComponent->bCaptureOnMovement = false;
        }
        if (USceneCaptureComponent2D* DepthCapture = ContextState.DepthCaptureComponent.Get())
        {
            DepthCapture->bCaptureEveryFrame = false;
            DepthCapture->bCaptureOnMovement = false;
        }
    };

    auto BuildRuntimeContextSignature = [](const FRshipRenderContextState& ContextState) -> FString
    {
        if (!ContextState.SourceType.Equals(TEXT("camera"), ESearchCase::IgnoreCase))
        {
            return FString();
        }

        FString CameraToken = ContextState.CameraId.TrimStartAndEnd().ToLower();
        if (CameraToken.IsEmpty())
        {
            return FString();
        }

        const int32 Width = FMath::Max(1, ContextState.Width);
        const int32 Height = FMath::Max(1, ContextState.Height);
        const FString CaptureMode = ContextState.CaptureMode.TrimStartAndEnd().ToLower();
        const FString DepthMode = ContextState.DepthCaptureMode.TrimStartAndEnd().ToLower();

        return FString::Printf(
            TEXT("camera|%s|%d|%d|%s|depth:%d|%s"),
            *CameraToken,
            Width,
            Height,
            *CaptureMode,
            ContextState.bDepthCaptureEnabled ? 1 : 0,
            *DepthMode);
    };

    TMap<FString, FString> SignatureToResolvedContextId;
    int32 ActiveResolvedContexts = 0;
    double ContextResolveMsAccum = 0.0;
    double ApplyMappingsMsAccum = 0.0;
    double ConfigHashMsAccum = 0.0;
    int32 MaterialBindingsUpdated = 0;
    int32 MaterialBindingsSkipped = 0;

    for (auto& Pair : RenderContexts)
    {
        FRshipRenderContextState& ContextState = Pair.Value;
        NormalizeRenderContextState(ContextState);
        FRenderContextRuntimeState& RuntimeState = RenderContextRuntimeStates.FindOrAdd(ContextState.Id);

        if (!RequiredContextIds.Contains(ContextState.Id))
        {
            DisableContextCapture(ContextState);
            if (AActor* CameraActor = ContextState.CameraActor.Get())
            {
                CameraActor->Destroy();
            }
            if (USceneCaptureComponent2D* CaptureComponent = ContextState.CaptureComponent.Get())
            {
                CaptureComponent->DestroyComponent();
            }
            ContextState.CameraActor.Reset();
            ContextState.SourceCameraActor.Reset();
            ContextState.CaptureComponent.Reset();
            ContextState.CaptureRenderTarget.Reset();
            if (USceneCaptureComponent2D* DepthCapture = ContextState.DepthCaptureComponent.Get())
            {
                DepthCapture->DestroyComponent();
            }
            ContextState.DepthCaptureComponent.Reset();
            ContextState.DepthRenderTarget.Reset();
            ContextState.ResolvedTexture = nullptr;
            ContextState.ResolvedDepthTexture = nullptr;
            ContextState.LastError.Empty();
            RenderContextRuntimeStates.Remove(ContextState.Id);
            continue;
        }

        const double NowSeconds = FPlatformTime::Seconds();
        if (RuntimeState.NextResolveRetryTimeSeconds > 0.0 && NowSeconds < RuntimeState.NextResolveRetryTimeSeconds)
        {
            ContextState.ResolvedTexture = nullptr;
            ContextState.ResolvedDepthTexture = nullptr;
            DisableContextCapture(ContextState);
            continue;
        }

        const FString Signature = BuildRuntimeContextSignature(ContextState);
        if (!Signature.IsEmpty())
        {
            if (const FString* ExistingResolvedContextId = SignatureToResolvedContextId.Find(Signature))
            {
                if (const FRshipRenderContextState* ExistingResolvedContext = RenderContexts.Find(*ExistingResolvedContextId))
                {
                    if (ExistingResolvedContext->ResolvedTexture && ExistingResolvedContext->LastError.IsEmpty())
                    {
                        ContextState.ResolvedTexture = ExistingResolvedContext->ResolvedTexture;
                        ContextState.ResolvedDepthTexture = ExistingResolvedContext->ResolvedDepthTexture;
                        ContextState.LastError.Empty();
                        DisableContextCapture(ContextState);
                        continue;
                    }
                }
            }
        }

        const double ResolveStartSeconds = FPlatformTime::Seconds();
        ResolveRenderContext(ContextState);
        ContextResolveMsAccum += (FPlatformTime::Seconds() - ResolveStartSeconds) * 1000.0;
        if (ContextState.ResolvedTexture)
        {
            RuntimeState.NextResolveRetryTimeSeconds = 0.0;
            ++ActiveResolvedContexts;
            if (!Signature.IsEmpty())
            {
                SignatureToResolvedContextId.Add(Signature, ContextState.Id);
            }
        }
        else
        {
            const bool bLikelyMissingSource = ContextState.LastError.Contains(TEXT("No source actor resolved"), ESearchCase::IgnoreCase)
                || ContextState.LastError.Contains(TEXT("CameraId not set"), ESearchCase::IgnoreCase);
            RuntimeState.NextResolveRetryTimeSeconds = NowSeconds + (bLikelyMissingSource ? 0.20 : 0.05);
        }
    }

    RefreshResolvedContextFallbackIds();
    LastTickActiveContexts = ActiveResolvedContexts;

    int32 EnabledMappingCount = 0;
    int32 AppliedSurfaceCount = 0;
    int32 SkippedMissingSurfaceStateCount = 0;
    int32 SkippedDisabledSurfaceCount = 0;
    int32 SkippedMeshNotReadyCount = 0;
    FString FirstMappingError;
    UWorld* PreferredWorld = GetBestWorld();
    const double NowSeconds = FPlatformTime::Seconds();

    for (auto& MappingPair : Mappings)
    {
        FRshipContentMappingState& MappingState = MappingPair.Value;
        MappingState.LastError.Empty();
        if (!MappingState.bEnabled)
        {
            continue;
        }
        ++EnabledMappingCount;

        uint32 MappingConfigHash = 0;
        if (MappingState.Config.IsValid())
        {
            const double HashStartSeconds = FPlatformTime::Seconds();
            MappingConfigHash = HashJsonPayload(MappingState.Config);
            ConfigHashMsAccum += (FPlatformTime::Seconds() - HashStartSeconds) * 1000.0;
        }

        const bool bFeedV2 = IsFeedV2Mapping(MappingState);
        const FRshipRenderContextState* ContextState = ResolveEffectiveContextState(MappingState, bFeedV2);
        if ((CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0)
            && bFeedV2 && (!ContextState || !ContextState->ResolvedTexture))
        {
            // Feed mappings can resolve per-route contexts; keep a soft fallback context bound so
            // materials still have a live texture when route composition has a transient miss.
            ContextState = ResolveEffectiveContextState(MappingState, false);
        }
        if (!bFeedV2 && !ContextState)
        {
            continue;
        }

        const TArray<FString>& EffectiveSurfaceIds = GetEffectiveSurfaceIds(MappingState);
        bool bSawSurfaceEntry = false;
        bool bSawEnabledSurface = false;
        bool bSawMeshReady = false;
        for (const FString& SurfaceId : EffectiveSurfaceIds)
        {
            bSawSurfaceEntry = true;
            FRshipMappingSurfaceState* SurfaceState = MappingSurfaces.Find(SurfaceId);
            if (!SurfaceState)
            {
                ++SkippedMissingSurfaceStateCount;
                continue;
            }
            if (!SurfaceState->bEnabled)
            {
                ++SkippedDisabledSurfaceCount;
                continue;
            }
            bSawEnabledSurface = true;

            if (!IsMeshReadyForMaterialMutation(SurfaceState->MeshComponent.Get()))
            {
                if (NowSeconds >= SurfaceState->NextResolveRetryTimeSeconds)
                {
                    ResolveMappingSurface(*SurfaceState);
                    SurfaceState->NextResolveRetryTimeSeconds = IsMeshReadyForMaterialMutation(SurfaceState->MeshComponent.Get())
                        ? 0.0
                        : (NowSeconds + 0.25);
                }
            }
            else if (PreferredWorld)
            {
                if (UMeshComponent* SurfaceMesh = SurfaceState->MeshComponent.Get())
                {
                    if (SurfaceMesh->GetWorld() != PreferredWorld)
                    {
                        if (NowSeconds >= SurfaceState->NextResolveRetryTimeSeconds)
                        {
                            ResolveMappingSurface(*SurfaceState);
                            SurfaceState->NextResolveRetryTimeSeconds = IsMeshReadyForMaterialMutation(SurfaceState->MeshComponent.Get())
                                ? (NowSeconds + 1.0)
                                : (NowSeconds + 0.25);
                        }
                    }
                }
            }

            if (!IsMeshReadyForMaterialMutation(SurfaceState->MeshComponent.Get()))
            {
                ++SkippedMeshNotReadyCount;
                continue;
            }
            bSawMeshReady = true;

            ApplyMappingToSurface(
                MappingState,
                *SurfaceState,
                ContextState,
                MappingConfigHash,
                &ApplyMappingsMsAccum,
                &MaterialBindingsUpdated,
                &MaterialBindingsSkipped);
            if (SurfaceState->LastError.IsEmpty())
            {
                ++AppliedSurfaceCount;
            }
            else if (MappingState.LastError.IsEmpty())
            {
                MappingState.LastError = SurfaceState->LastError;
            }
        }

        if (FirstMappingError.IsEmpty() && !MappingState.LastError.IsEmpty())
        {
            FirstMappingError = MappingState.LastError;
        }
        if (MappingState.LastError.IsEmpty())
        {
            if (!bSawSurfaceEntry)
            {
                MappingState.LastError = TEXT("No mapping surfaces assigned");
            }
            else if (!bSawEnabledSurface)
            {
                MappingState.LastError = TEXT("All mapping surfaces are disabled");
            }
            else if (!bSawMeshReady)
            {
                MappingState.LastError = TEXT("All mapping surfaces unresolved");
            }

            if (FirstMappingError.IsEmpty() && !MappingState.LastError.IsEmpty())
            {
                FirstMappingError = MappingState.LastError;
            }
        }
    }

    if (EnabledMappingCount > 0 && AppliedSurfaceCount == 0)
    {
        int32 ContextsWithTexture = 0;
        for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
        {
            if (Pair.Value.ResolvedTexture)
            {
                ++ContextsWithTexture;
            }
        }

        static double LastNoSurfaceWarningTime = 0.0;
        const double WarningNowSeconds = FPlatformTime::Seconds();
        if ((WarningNowSeconds - LastNoSurfaceWarningTime) >= 1.0)
        {
            LastNoSurfaceWarningTime = WarningNowSeconds;
            UE_LOG(LogRshipExec, Warning,
                TEXT("ContentMapping produced no applied surfaces (enabledMappings=%d, contexts=%d, contextsWithTexture=%d, surfaces=%d, missingSurfaceState=%d, surfaceDisabled=%d, meshNotReady=%d, firstError='%s')"),
                EnabledMappingCount,
                RenderContexts.Num(),
                ContextsWithTexture,
                MappingSurfaces.Num(),
                SkippedMissingSurfaceStateCount,
                SkippedDisabledSurfaceCount,
                SkippedMeshNotReadyCount,
                *FirstMappingError);
        }
    }

    LastTickEnabledMappings = EnabledMappingCount;
    LastTickAppliedSurfaces = AppliedSurfaceCount;
    LastTickMsContextResolve = static_cast<float>(ContextResolveMsAccum);
    LastTickMsApplyMappings = static_cast<float>(ApplyMappingsMsAccum);
    LastTickMsConfigHash = static_cast<float>(ConfigHashMsAccum);
    LastTickMaterialBindingsUpdated = MaterialBindingsUpdated;
    LastTickMaterialBindingsSkipped = MaterialBindingsSkipped;
}

TArray<FRshipRenderContextState> URshipContentMappingManager::GetRenderContexts() const
{
    TArray<FRshipRenderContextState> Result;
    RenderContexts.GenerateValueArray(Result);
    return Result;
}

TArray<FRshipMappingSurfaceState> URshipContentMappingManager::GetMappingSurfaces() const
{
    TArray<FRshipMappingSurfaceState> Result;
    MappingSurfaces.GenerateValueArray(Result);
    return Result;
}

TArray<FRshipContentMappingState> URshipContentMappingManager::GetMappings() const
{
    TArray<FRshipContentMappingState> Result;
    Mappings.GenerateValueArray(Result);
    return Result;
}

bool URshipContentMappingManager::RegisterExternalTextureSource(
    const FString& SourceId,
    UTexture* Texture,
    int32 Width,
    int32 Height)
{
    const FString NormalizedSourceId = SourceId.TrimStartAndEnd();
    if (NormalizedSourceId.IsEmpty() || !Texture || !IsValid(Texture))
    {
        return false;
    }

    FExternalTextureSourceState& SourceState = ExternalTextureSources.FindOrAdd(NormalizedSourceId);
    SourceState.Texture = Texture;
    SourceState.Width = FMath::Max(0, Width);
    SourceState.Height = FMath::Max(0, Height);

    bool bTouchedContexts = false;
    for (TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        FRshipRenderContextState& ContextState = Pair.Value;
        if (!ContextState.SourceType.Equals(TEXT("external-texture"), ESearchCase::IgnoreCase)
            || !ContextState.ExternalSourceId.Equals(NormalizedSourceId, ESearchCase::IgnoreCase))
        {
            continue;
        }

        ResolveRenderContext(ContextState);
        EmitContextState(ContextState);
        bTouchedContexts = true;
    }

    if (bTouchedContexts)
    {
        MarkMappingsDirty();
        SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
    }

    return true;
}

bool URshipContentMappingManager::UnregisterExternalTextureSource(const FString& SourceId)
{
    const FString NormalizedSourceId = SourceId.TrimStartAndEnd();
    if (NormalizedSourceId.IsEmpty())
    {
        return false;
    }

    const bool bRemoved = ExternalTextureSources.Remove(NormalizedSourceId) > 0;
    if (!bRemoved)
    {
        return false;
    }

    bool bTouchedContexts = false;
    for (TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        FRshipRenderContextState& ContextState = Pair.Value;
        if (!ContextState.SourceType.Equals(TEXT("external-texture"), ESearchCase::IgnoreCase)
            || !ContextState.ExternalSourceId.Equals(NormalizedSourceId, ESearchCase::IgnoreCase))
        {
            continue;
        }

        ContextState.ResolvedTexture = nullptr;
        ContextState.LastError = FString::Printf(
            TEXT("External texture source '%s' not registered"),
            *NormalizedSourceId);
        EmitContextState(ContextState);
        bTouchedContexts = true;
    }

    if (bTouchedContexts)
    {
        MarkMappingsDirty();
        SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
    }

    return true;
}

bool URshipContentMappingManager::ResolveRenderContextRenderTarget(
    const FString& ContextId,
    UTextureRenderTarget2D*& OutRenderTarget) const
{
    OutRenderTarget = nullptr;

    const FString NormalizedContextId = ContextId.TrimStartAndEnd();
    if (NormalizedContextId.IsEmpty())
    {
        return false;
    }

    const FRshipRenderContextState* ContextState = RenderContexts.Find(NormalizedContextId);
    if (!ContextState || !ContextState->bEnabled)
    {
        return false;
    }

    OutRenderTarget = Cast<UTextureRenderTarget2D>(ContextState->ResolvedTexture);
    return OutRenderTarget != nullptr;
}

bool URshipContentMappingManager::ResolveMappingOutputRenderTarget(
    const FString& MappingId,
    const FString& SurfaceId,
    UTextureRenderTarget2D*& OutRenderTarget,
    FString& OutError)
{
    OutRenderTarget = nullptr;
    OutError.Reset();

    const FString NormalizedMappingId = MappingId.TrimStartAndEnd();
    if (NormalizedMappingId.IsEmpty())
    {
        OutError = TEXT("MappingId is empty");
        return false;
    }

    FRshipContentMappingState* MappingState = Mappings.Find(NormalizedMappingId);
    if (!MappingState)
    {
        OutError = FString::Printf(TEXT("Mapping '%s' not found"), *NormalizedMappingId);
        return false;
    }
    if (!MappingState->bEnabled)
    {
        OutError = FString::Printf(TEXT("Mapping '%s' is disabled"), *NormalizedMappingId);
        return false;
    }

    EnsureMappingRuntimeReady(*MappingState);

    const TArray<FString>& EffectiveSurfaceIds = GetEffectiveSurfaceIds(*MappingState);
    FString NormalizedSurfaceId = SurfaceId.TrimStartAndEnd();
    if (NormalizedSurfaceId.IsEmpty())
    {
        if (EffectiveSurfaceIds.Num() == 1)
        {
            NormalizedSurfaceId = EffectiveSurfaceIds[0];
        }
        else
        {
            OutError = FString::Printf(
                TEXT("Mapping '%s' requires an explicit surfaceId (bound surfaces=%d)"),
                *NormalizedMappingId,
                EffectiveSurfaceIds.Num());
            return false;
        }
    }

    if (!EffectiveSurfaceIds.Contains(NormalizedSurfaceId))
    {
        OutError = FString::Printf(
            TEXT("Surface '%s' is not assigned to mapping '%s'"),
            *NormalizedSurfaceId,
            *NormalizedMappingId);
        return false;
    }

    FRshipMappingSurfaceState* SurfaceState = MappingSurfaces.Find(NormalizedSurfaceId);
    if (!SurfaceState)
    {
        OutError = FString::Printf(TEXT("Surface '%s' not found"), *NormalizedSurfaceId);
        return false;
    }
    if (!SurfaceState->bEnabled)
    {
        OutError = FString::Printf(TEXT("Surface '%s' is disabled"), *NormalizedSurfaceId);
        return false;
    }

    if (IsFeedV2Mapping(*MappingState))
    {
        EnsureFeedMappingRuntimeReady(*MappingState);

        FString FeedError;
        UTexture* FeedTexture = BuildFeedCompositeTextureForSurface(*MappingState, *SurfaceState, FeedError);
        OutRenderTarget = Cast<UTextureRenderTarget2D>(FeedTexture);
        if (!OutRenderTarget)
        {
            OutError = !FeedError.IsEmpty()
                ? FeedError
                : FString::Printf(
                    TEXT("Mapping '%s' surface '%s' feed output is not a render target"),
                    *NormalizedMappingId,
                    *NormalizedSurfaceId);
            return false;
        }

        return true;
    }

    // Non-feed mappings resolve to the context source render target. The final
    // mesh shading pass happens in-world and is not represented as a standalone RT.
    const FRshipRenderContextState* ContextState = ResolveEffectiveContextState(*MappingState, true);
    if (!ContextState)
    {
        OutError = TEXT("Render context not available");
        return false;
    }
    if (!ContextState->bEnabled)
    {
        OutError = FString::Printf(TEXT("Render context '%s' is disabled"), *ContextState->Id);
        return false;
    }

    OutRenderTarget = Cast<UTextureRenderTarget2D>(ContextState->ResolvedTexture);
    if (!OutRenderTarget)
    {
        OutError = FString::Printf(
            TEXT("Render context '%s' texture is not a render target"),
            *ContextState->Id);
        return false;
    }

    return true;
}

void URshipContentMappingManager::SetDebugOverlayEnabled(bool bEnabled)
{
    bDebugOverlayEnabled = bEnabled;
    DebugOverlayAccumulated = 0.0f;
}

bool URshipContentMappingManager::IsDebugOverlayEnabled() const
{
    return bDebugOverlayEnabled;
}

void URshipContentMappingManager::SetDebugOverlayEnabledForSubsystem(bool bEnabled)
{
    SetDebugOverlayEnabled(bEnabled);
}

bool URshipContentMappingManager::IsDebugOverlayEnabledForSubsystem() const
{
    return IsDebugOverlayEnabled();
}

FString URshipContentMappingManager::GetRenderContextsJsonForSubsystem() const
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("runtimeHealth"), GetRuntimeHealthStatusToken());
    if (!RuntimeHealthReason.IsEmpty())
    {
        Root->SetStringField(TEXT("runtimeReason"), RuntimeHealthReason);
    }
    TArray<TSharedPtr<FJsonValue>> ContextValues;
    ContextValues.Reserve(RenderContexts.Num());

    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        const FRshipRenderContextState& Context = Pair.Value;
        TSharedPtr<FJsonObject> ContextObject = MakeShared<FJsonObject>();
        ContextObject->SetStringField(TEXT("id"), Context.Id);
        ContextObject->SetStringField(TEXT("name"), Context.Name);
        ContextObject->SetStringField(TEXT("sourceType"), Context.SourceType);
        ContextObject->SetStringField(TEXT("cameraId"), Context.CameraId);
        ContextObject->SetStringField(TEXT("externalSourceId"), Context.ExternalSourceId);
        ContextObject->SetNumberField(TEXT("width"), Context.Width);
        ContextObject->SetNumberField(TEXT("height"), Context.Height);
        ContextObject->SetBoolField(TEXT("enabled"), Context.bEnabled);
        ContextObject->SetBoolField(TEXT("hasRenderTarget"), Cast<UTextureRenderTarget2D>(Context.ResolvedTexture) != nullptr);
        ContextObject->SetStringField(TEXT("lastError"), Context.LastError);
        ContextValues.Add(MakeShared<FJsonValueObject>(ContextObject));
    }

    Root->SetArrayField(TEXT("contexts"), ContextValues);

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        return FString();
    }

    return Serialized;
}

void URshipContentMappingManager::SetCoveragePreviewEnabled(bool bEnabled)
{
    bCoveragePreviewEnabled = bEnabled;
    MarkMappingsDirty();
}

bool URshipContentMappingManager::IsCoveragePreviewEnabled() const
{
    return bCoveragePreviewEnabled;
}

void URshipContentMappingManager::RegisterPipelineEndpointAdapter(UObject* Adapter)
{
    if (!Adapter || !IsValid(Adapter))
    {
        return;
    }

    if (!Adapter->GetClass()->ImplementsInterface(URshipTexturePipelineEndpointAdapter::StaticClass()))
    {
        return;
    }

    const bool bAlreadyRegistered = PipelineEndpointAdapters.ContainsByPredicate(
        [Adapter](const TWeakObjectPtr<UObject>& Existing)
        {
            return Existing.Get() == Adapter;
        });
    if (!bAlreadyRegistered)
    {
        PipelineEndpointAdapters.Add(Adapter);
    }
}

void URshipContentMappingManager::UnregisterPipelineEndpointAdapter(UObject* Adapter)
{
    if (!Adapter)
    {
        return;
    }

    PipelineEndpointAdapters.RemoveAll(
        [Adapter](const TWeakObjectPtr<UObject>& Existing)
        {
            return Existing.Get() == Adapter;
        });
}

void URshipContentMappingManager::AddPipelineDiagnostic(
    TArray<FRshipPipelineDiagnostic>& Diagnostics,
    ERshipPipelineDiagnosticSeverity Severity,
    const FString& Code,
    const FString& Message,
    const FString& NodeId,
    const FString& EdgeId) const
{
    FRshipPipelineDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
    Diagnostic.Severity = Severity;
    Diagnostic.Code = Code;
    Diagnostic.Message = Message;
    Diagnostic.NodeId = NodeId;
    Diagnostic.EdgeId = EdgeId;
}

void URshipContentMappingManager::CapturePipelineRuntimeSnapshot()
{
    LastPipelineSnapshot = FPipelineRuntimeSnapshot();
    LastPipelineSnapshot.bValid = true;
    LastPipelineSnapshot.RenderContexts = RenderContexts;
    LastPipelineSnapshot.MappingSurfaces = MappingSurfaces;
    LastPipelineSnapshot.Mappings = Mappings;
}

void URshipContentMappingManager::RestorePipelineRuntimeSnapshot(const FPipelineRuntimeSnapshot& Snapshot)
{
    if (!Snapshot.bValid)
    {
        return;
    }

    for (TPair<FString, FRshipMappingSurfaceState>& Pair : MappingSurfaces)
    {
        RestoreSurfaceMaterials(Pair.Value);
    }

    TSet<FString> ContextIds;
    TSet<FString> SurfaceIds;
    TSet<FString> MappingIds;
    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        ContextIds.Add(Pair.Key);
    }
    for (const TPair<FString, FRshipRenderContextState>& Pair : Snapshot.RenderContexts)
    {
        ContextIds.Add(Pair.Key);
    }
    for (const TPair<FString, FRshipMappingSurfaceState>& Pair : MappingSurfaces)
    {
        SurfaceIds.Add(Pair.Key);
    }
    for (const TPair<FString, FRshipMappingSurfaceState>& Pair : Snapshot.MappingSurfaces)
    {
        SurfaceIds.Add(Pair.Key);
    }
    for (const TPair<FString, FRshipContentMappingState>& Pair : Mappings)
    {
        MappingIds.Add(Pair.Key);
    }
    for (const TPair<FString, FRshipContentMappingState>& Pair : Snapshot.Mappings)
    {
        MappingIds.Add(Pair.Key);
    }

    for (const FString& ContextId : ContextIds)
    {
        DeleteTargetForPath(BuildContextTargetId(ContextId));
    }
    for (const FString& SurfaceId : SurfaceIds)
    {
        DeleteTargetForPath(BuildSurfaceTargetId(SurfaceId));
    }
    for (const FString& MappingId : MappingIds)
    {
        DeleteTargetForPath(BuildMappingTargetId(MappingId));
    }

    RenderContexts = Snapshot.RenderContexts;
    MappingSurfaces = Snapshot.MappingSurfaces;
    Mappings = Snapshot.Mappings;

    FeedCompositeTargets.Empty();
    FeedCompositeStaticSignatures.Empty();
    EffectiveSurfaceIdsCache.Empty();
    RequiredContextIdsCache.Empty();
    RenderContextRuntimeStates.Empty();
    PendingMappingUpserts.Empty();
    PendingMappingUpsertExpiry.Empty();
    PendingMappingDeletes.Empty();

    RegisterAllTargets();
    MarkMappingsDirty();
    MarkCacheDirty();
    SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
}

bool URshipContentMappingManager::ValidatePipelineGraph(
    const URshipTexturePipelineAsset* PipelineAsset,
    TArray<FRshipPipelineDiagnostic>& OutDiagnostics) const
{
    OutDiagnostics.Reset();

    if (!PipelineAsset)
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.asset.missing"),
            TEXT("Pipeline asset is null"));
        return false;
    }

    const auto HasErrors = [&OutDiagnostics]() -> bool
    {
        return OutDiagnostics.ContainsByPredicate(
            [](const FRshipPipelineDiagnostic& Diagnostic)
            {
                return Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Error;
            });
    };

    TMap<FString, const FRshipPipelineNode*> NodeById;
    NodeById.Reserve(PipelineAsset->Nodes.Num());

    int32 OutputNodeCount = 0;
    int32 MediaEndpointNodeCount = 0;

    auto ValidateEndpointNode = [this, &OutDiagnostics](const FRshipPipelineNode& Node, const FString& Direction)
    {
        const FString AdapterId = Node.Params.FindRef(TEXT("adapterId")).TrimStartAndEnd();
        const FString EndpointId = Node.Params.FindRef(TEXT("endpointId")).TrimStartAndEnd();
        if (AdapterId.IsEmpty() || EndpointId.IsEmpty())
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.endpoint.missing"),
                TEXT("Media endpoint node requires adapterId and endpointId"),
                Node.Id);
            return;
        }

        int32 AdapterCount = 0;
        FString LastError;
        bool bValidated = false;
        for (const TWeakObjectPtr<UObject>& AdapterWeak : PipelineEndpointAdapters)
        {
            UObject* AdapterObject = AdapterWeak.Get();
            if (!AdapterObject || !IsValid(AdapterObject))
            {
                continue;
            }
            if (!AdapterObject->GetClass()->ImplementsInterface(URshipTexturePipelineEndpointAdapter::StaticClass()))
            {
                continue;
            }

            ++AdapterCount;
            FString AdapterError;
            const bool bOk = IRshipTexturePipelineEndpointAdapter::Execute_ValidateEndpoint(
                AdapterObject,
                AdapterId,
                EndpointId,
                Direction,
                AdapterError);
            if (bOk)
            {
                bValidated = true;
                break;
            }
            if (!AdapterError.IsEmpty())
            {
                LastError = AdapterError;
            }
        }

        if (AdapterCount == 0)
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.endpoint.no_adapter"),
                FString::Printf(
                    TEXT("No endpoint adapter is registered for %s endpoint '%s/%s'"),
                    *Direction,
                    *AdapterId,
                    *EndpointId),
                Node.Id);
            return;
        }

        if (!bValidated)
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.endpoint.invalid"),
                LastError.IsEmpty()
                    ? FString::Printf(
                        TEXT("Endpoint '%s/%s' rejected for direction '%s'"),
                        *AdapterId,
                        *EndpointId,
                        *Direction)
                    : LastError,
                Node.Id);
        }
    };

    for (const FRshipPipelineNode& Node : PipelineAsset->Nodes)
    {
        const FString NodeId = Node.Id.TrimStartAndEnd();
        if (NodeId.IsEmpty())
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.node.id_missing"),
                TEXT("Pipeline node id is required"));
            continue;
        }

        if (NodeById.Contains(NodeId))
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.node.id_duplicate"),
                FString::Printf(TEXT("Duplicate pipeline node id '%s'"), *NodeId),
                NodeId);
            continue;
        }

        NodeById.Add(NodeId, &Node);

        const FString NodeLabel = Node.Label.TrimStartAndEnd().IsEmpty() ? NodeId : Node.Label.TrimStartAndEnd();
        switch (Node.Kind)
        {
        case ERshipPipelineNodeKind::RenderContextInput:
        {
            const FString ContextId = Node.Params.FindRef(TEXT("contextId")).TrimStartAndEnd();
            const FString CameraId = Node.Params.FindRef(TEXT("cameraId")).TrimStartAndEnd();
            const FString AssetId = Node.Params.FindRef(TEXT("assetId")).TrimStartAndEnd();
            const FString ExternalSourceId = Node.Params.FindRef(TEXT("externalSourceId")).TrimStartAndEnd();
            if (ContextId.IsEmpty() && CameraId.IsEmpty() && AssetId.IsEmpty() && ExternalSourceId.IsEmpty())
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.node.render_context.unbound"),
                    FString::Printf(TEXT("Render context input '%s' must define contextId, cameraId, assetId, or externalSourceId"), *NodeLabel),
                    NodeId);
            }
            break;
        }
        case ERshipPipelineNodeKind::ExternalTextureInput:
        {
            const FString ExternalSourceId = Node.Params.FindRef(TEXT("externalSourceId")).TrimStartAndEnd();
            if (ExternalSourceId.IsEmpty())
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.node.external_input.unbound"),
                    FString::Printf(TEXT("External texture input '%s' requires externalSourceId"), *NodeLabel),
                    NodeId);
            }
            break;
        }
        case ERshipPipelineNodeKind::MediaProfileInput:
        {
            ++MediaEndpointNodeCount;
            ValidateEndpointNode(Node, TEXT("input"));
            break;
        }
        case ERshipPipelineNodeKind::Projection:
        {
            const FString ProjectionType = NormalizeProjectionModeToken(
                Node.Params.FindRef(TEXT("projectionType")),
                TEXT("perspective"));
            static const TSet<FString> SupportedProjectionTypes = {
                TEXT("direct"),
                TEXT("feed"),
                TEXT("perspective"),
                TEXT("custom-matrix"),
                TEXT("cylindrical"),
                TEXT("spherical"),
                TEXT("parallel"),
                TEXT("radial"),
                TEXT("mesh"),
                TEXT("fisheye"),
                TEXT("camera-plate"),
                TEXT("spatial"),
                TEXT("depth-map")
            };
            if (!SupportedProjectionTypes.Contains(ProjectionType))
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.node.projection.unsupported"),
                    FString::Printf(TEXT("Projection type '%s' is not supported"), *ProjectionType),
                    NodeId);
            }
            break;
        }
        case ERshipPipelineNodeKind::MappingSurfaceOutput:
        {
            ++OutputNodeCount;
            const FString SurfaceId = Node.Params.FindRef(TEXT("surfaceId")).TrimStartAndEnd();
            if (SurfaceId.IsEmpty())
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.node.surface_output.unbound"),
                    FString::Printf(TEXT("Mapping surface output '%s' requires surfaceId"), *NodeLabel),
                    NodeId);
            }
            break;
        }
        case ERshipPipelineNodeKind::MediaProfileOutput:
        {
            ++OutputNodeCount;
            ++MediaEndpointNodeCount;
            ValidateEndpointNode(Node, TEXT("output"));
            break;
        }
        case ERshipPipelineNodeKind::TransformRect:
        case ERshipPipelineNodeKind::Opacity:
        case ERshipPipelineNodeKind::BlendComposite:
            break;
        default:
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.node.kind.unknown"),
                FString::Printf(TEXT("Node '%s' has unsupported kind"), *NodeLabel),
                NodeId);
            break;
        }
    }

    if (NodeById.Num() == 0)
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.graph.empty"),
            TEXT("Pipeline graph has no nodes"));
    }

    if (OutputNodeCount == 0)
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.graph.no_output"),
            TEXT("Pipeline graph requires at least one output node"));
    }

    TMap<FString, int32> InDegree;
    TMap<FString, TArray<FString>> Outgoing;
    for (const TPair<FString, const FRshipPipelineNode*>& Pair : NodeById)
    {
        InDegree.Add(Pair.Key, 0);
        Outgoing.Add(Pair.Key, TArray<FString>());
    }

    for (const FRshipPipelineEdge& Edge : PipelineAsset->Edges)
    {
        const FString EdgeId = Edge.Id.TrimStartAndEnd();
        const FString FromNodeId = Edge.FromNodeId.TrimStartAndEnd();
        const FString ToNodeId = Edge.ToNodeId.TrimStartAndEnd();
        if (FromNodeId.IsEmpty() || ToNodeId.IsEmpty())
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.edge.endpoint_missing"),
                TEXT("Pipeline edge must define fromNodeId and toNodeId"),
                FString(),
                EdgeId);
            continue;
        }
        if (!NodeById.Contains(FromNodeId))
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.edge.from_missing"),
                FString::Printf(TEXT("Edge references missing source node '%s'"), *FromNodeId),
                FString(),
                EdgeId);
            continue;
        }
        if (!NodeById.Contains(ToNodeId))
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.edge.to_missing"),
                FString::Printf(TEXT("Edge references missing destination node '%s'"), *ToNodeId),
                FString(),
                EdgeId);
            continue;
        }
        if (FromNodeId == ToNodeId)
        {
            AddPipelineDiagnostic(
                OutDiagnostics,
                ERshipPipelineDiagnosticSeverity::Error,
                TEXT("pipeline.edge.self_cycle"),
                FString::Printf(TEXT("Edge '%s' creates a self-cycle on node '%s'"), *EdgeId, *FromNodeId),
                FromNodeId,
                EdgeId);
            continue;
        }

        Outgoing.FindOrAdd(FromNodeId).AddUnique(ToNodeId);
        int32& Degree = InDegree.FindOrAdd(ToNodeId);
        Degree += 1;
    }

    TArray<FString> Ready;
    for (const TPair<FString, int32>& Pair : InDegree)
    {
        if (Pair.Value <= 0)
        {
            Ready.Add(Pair.Key);
        }
    }
    Ready.Sort();

    int32 ProcessedNodes = 0;
    while (Ready.Num() > 0)
    {
        const FString Current = Ready[0];
        Ready.RemoveAt(0);
        ++ProcessedNodes;

        TArray<FString> Children = Outgoing.FindRef(Current);
        Children.Sort();
        for (const FString& Child : Children)
        {
            int32* Degree = InDegree.Find(Child);
            if (!Degree)
            {
                continue;
            }
            *Degree -= 1;
            if (*Degree <= 0)
            {
                Ready.AddUnique(Child);
            }
        }
        Ready.Sort();
    }

    if (ProcessedNodes != NodeById.Num())
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.graph.cycle"),
            TEXT("Pipeline graph must be acyclic (DAG validation failed)"));
    }

    if (MediaEndpointNodeCount == 0 && PipelineEndpointAdapters.Num() > 0)
    {
        OutDiagnostics.RemoveAll([](const FRshipPipelineDiagnostic& Diagnostic)
        {
            return Diagnostic.Code == TEXT("pipeline.endpoint.no_adapter")
                || Diagnostic.Code == TEXT("pipeline.endpoint.invalid")
                || Diagnostic.Code == TEXT("pipeline.endpoint.missing");
        });
    }

    return !HasErrors();
}

bool URshipContentMappingManager::CompilePipelineGraph(
    const URshipTexturePipelineAsset* PipelineAsset,
    FRshipCompiledPipelinePlan& OutPlan,
    TArray<FRshipPipelineDiagnostic>& OutDiagnostics) const
{
    OutPlan = FRshipCompiledPipelinePlan();
    OutDiagnostics.Reset();

    if (!ValidatePipelineGraph(PipelineAsset, OutDiagnostics))
    {
        OutPlan.Diagnostics = OutDiagnostics;
        OutPlan.bValid = false;
        if (PipelineAsset)
        {
            OutPlan.AssetId = PipelineAsset->AssetId;
            OutPlan.Revision = PipelineAsset->Revision;
        }
        return false;
    }

    if (!PipelineAsset)
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.asset.missing"),
            TEXT("Pipeline asset is null"));
        OutPlan.Diagnostics = OutDiagnostics;
        OutPlan.bValid = false;
        return false;
    }

    TMap<FString, const FRshipPipelineNode*> NodeById;
    TMap<FString, TArray<FString>> IncomingNodeIds;
    TMap<FString, TArray<FString>> OutgoingNodeIds;
    TMap<FString, int32> InDegree;

    for (const FRshipPipelineNode& Node : PipelineAsset->Nodes)
    {
        const FString NodeId = Node.Id.TrimStartAndEnd();
        if (NodeId.IsEmpty())
        {
            continue;
        }
        NodeById.Add(NodeId, &Node);
        IncomingNodeIds.Add(NodeId, TArray<FString>());
        OutgoingNodeIds.Add(NodeId, TArray<FString>());
        InDegree.Add(NodeId, 0);
    }

    for (const FRshipPipelineEdge& Edge : PipelineAsset->Edges)
    {
        const FString FromNodeId = Edge.FromNodeId.TrimStartAndEnd();
        const FString ToNodeId = Edge.ToNodeId.TrimStartAndEnd();
        if (FromNodeId.IsEmpty() || ToNodeId.IsEmpty())
        {
            continue;
        }
        if (!NodeById.Contains(FromNodeId) || !NodeById.Contains(ToNodeId) || FromNodeId == ToNodeId)
        {
            continue;
        }

        OutgoingNodeIds.FindOrAdd(FromNodeId).AddUnique(ToNodeId);
        IncomingNodeIds.FindOrAdd(ToNodeId).AddUnique(FromNodeId);
        int32& Degree = InDegree.FindOrAdd(ToNodeId);
        Degree += 1;
    }

    TArray<FString> Ready;
    for (const TPair<FString, int32>& Pair : InDegree)
    {
        if (Pair.Value <= 0)
        {
            Ready.Add(Pair.Key);
        }
    }
    Ready.Sort();

    TArray<FString> TopologicalOrder;
    while (Ready.Num() > 0)
    {
        const FString Current = Ready[0];
        Ready.RemoveAt(0);
        TopologicalOrder.Add(Current);

        TArray<FString> Children = OutgoingNodeIds.FindRef(Current);
        Children.Sort();
        for (const FString& Child : Children)
        {
            int32* Degree = InDegree.Find(Child);
            if (!Degree)
            {
                continue;
            }
            *Degree -= 1;
            if (*Degree <= 0)
            {
                Ready.AddUnique(Child);
            }
        }
        Ready.Sort();
    }

    if (TopologicalOrder.Num() != NodeById.Num())
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.graph.cycle"),
            TEXT("Pipeline graph must remain acyclic for compile"));
        OutPlan.Diagnostics = OutDiagnostics;
        OutPlan.bValid = false;
        OutPlan.AssetId = PipelineAsset->AssetId;
        OutPlan.Revision = PipelineAsset->Revision;
        return false;
    }

    const auto ParseBoolValue = [](const FString& InText, bool DefaultValue) -> bool
    {
        const FString Value = InText.TrimStartAndEnd().ToLower();
        if (Value == TEXT("1") || Value == TEXT("true") || Value == TEXT("yes"))
        {
            return true;
        }
        if (Value == TEXT("0") || Value == TEXT("false") || Value == TEXT("no"))
        {
            return false;
        }
        return DefaultValue;
    };

    const auto ParseFloatValue = [](const FString& InText, float DefaultValue) -> float
    {
        if (InText.TrimStartAndEnd().IsEmpty())
        {
            return DefaultValue;
        }
        return FCString::Atof(*InText);
    };

    const auto ParseIntValue = [](const FString& InText, int32 DefaultValue) -> int32
    {
        if (InText.TrimStartAndEnd().IsEmpty())
        {
            return DefaultValue;
        }
        return FCString::Atoi(*InText);
    };

    TMap<FString, FRshipPipelineContextSpec> ContextSpecs;
    TMap<FString, FString> ContextNodeToContextId;
    TMap<FString, FRshipPipelineSurfaceSpec> SurfaceSpecs;
    TMap<FString, FRshipPipelineMappingSpec> MappingSpecs;
    TArray<FRshipPipelineEndpointBinding> EndpointBindings;

    TMap<FString, int32> TopologicalIndex;
    for (int32 Index = 0; Index < TopologicalOrder.Num(); ++Index)
    {
        TopologicalIndex.Add(TopologicalOrder[Index], Index);
    }

    const bool bStrictNoFallback = PipelineAsset->bStrictNoFallback;

    const auto ReadNodeParam = [](const FRshipPipelineNode& Node, std::initializer_list<const TCHAR*> Keys) -> FString
    {
        for (const TCHAR* Key : Keys)
        {
            const FString Value = Node.Params.FindRef(Key).TrimStartAndEnd();
            if (!Value.IsEmpty())
            {
                return Value;
            }
        }
        return FString();
    };

    const auto ReadNodeFloat = [&ParseFloatValue, &ReadNodeParam](
        const FRshipPipelineNode& Node,
        std::initializer_list<const TCHAR*> Keys,
        float DefaultValue,
        bool* bOutProvided = nullptr) -> float
    {
        const FString RawValue = ReadNodeParam(Node, Keys);
        const bool bProvided = !RawValue.IsEmpty();
        if (bOutProvided)
        {
            *bOutProvided = bProvided;
        }
        return bProvided ? ParseFloatValue(RawValue, DefaultValue) : DefaultValue;
    };

    const auto ReadNodeBool = [&ParseBoolValue, &ReadNodeParam](
        const FRshipPipelineNode& Node,
        std::initializer_list<const TCHAR*> Keys,
        bool DefaultValue,
        bool* bOutProvided = nullptr) -> bool
    {
        const FString RawValue = ReadNodeParam(Node, Keys);
        const bool bProvided = !RawValue.IsEmpty();
        if (bOutProvided)
        {
            *bOutProvided = bProvided;
        }
        return bProvided ? ParseBoolValue(RawValue, DefaultValue) : DefaultValue;
    };

    auto BuildProjectionConfig = [
        &ReadNodeParam,
        &ReadNodeFloat,
        &ReadNodeBool
    ](
        const FRshipPipelineNode* ProjectionNode,
        TSharedPtr<FJsonObject>& OutConfig,
        FString& OutCanonicalType)
    {
        OutConfig = MakeShared<FJsonObject>();
        OutCanonicalType.Reset();

        if (!ProjectionNode)
        {
            return;
        }

        const FString ProjectionType = NormalizeProjectionModeToken(
            ReadNodeParam(*ProjectionNode, {TEXT("projectionType"), TEXT("type"), TEXT("mode")}),
            TEXT("perspective"));

        const float ProjectorX = ReadNodeFloat(*ProjectionNode, {TEXT("projectorX"), TEXT("positionX"), TEXT("x")}, 0.0f);
        const float ProjectorY = ReadNodeFloat(*ProjectionNode, {TEXT("projectorY"), TEXT("positionY"), TEXT("y")}, 0.0f);
        const float ProjectorZ = ReadNodeFloat(*ProjectionNode, {TEXT("projectorZ"), TEXT("positionZ"), TEXT("z")}, 0.0f);
        const float RotationX = ReadNodeFloat(*ProjectionNode, {TEXT("rotationX"), TEXT("rotX"), TEXT("pitch")}, 0.0f);
        const float RotationY = ReadNodeFloat(*ProjectionNode, {TEXT("rotationY"), TEXT("rotY"), TEXT("yaw")}, 0.0f);
        const float RotationZ = ReadNodeFloat(*ProjectionNode, {TEXT("rotationZ"), TEXT("rotZ"), TEXT("roll")}, 0.0f);
        const float Fov = ReadNodeFloat(*ProjectionNode, {TEXT("fov")}, 60.0f);
        const float Aspect = ReadNodeFloat(*ProjectionNode, {TEXT("aspectRatio"), TEXT("aspect")}, 1.7778f);
        const float Near = ReadNodeFloat(*ProjectionNode, {TEXT("near"), TEXT("nearPlane")}, 10.0f);
        const float Far = ReadNodeFloat(*ProjectionNode, {TEXT("far"), TEXT("farPlane")}, 10000.0f);

        if (ProjectionType == TEXT("direct"))
        {
            OutCanonicalType = TEXT("surface-uv");
            OutConfig->SetStringField(TEXT("uvMode"), TEXT("direct"));
            return;
        }

        if (ProjectionType == TEXT("feed"))
        {
            OutCanonicalType = TEXT("surface-uv");
            OutConfig->SetStringField(TEXT("uvMode"), TEXT("feed"));
            return;
        }

        OutCanonicalType = TEXT("surface-projection");
        OutConfig->SetStringField(TEXT("projectionType"), ProjectionType);

        TSharedPtr<FJsonObject> Position = MakeShared<FJsonObject>();
        Position->SetNumberField(TEXT("x"), ProjectorX);
        Position->SetNumberField(TEXT("y"), ProjectorY);
        Position->SetNumberField(TEXT("z"), ProjectorZ);
        OutConfig->SetObjectField(TEXT("projectorPosition"), Position);

        TSharedPtr<FJsonObject> Rotation = MakeShared<FJsonObject>();
        Rotation->SetNumberField(TEXT("x"), RotationX);
        Rotation->SetNumberField(TEXT("y"), RotationY);
        Rotation->SetNumberField(TEXT("z"), RotationZ);
        OutConfig->SetObjectField(TEXT("projectorRotation"), Rotation);

        OutConfig->SetNumberField(TEXT("fov"), Fov);
        OutConfig->SetNumberField(TEXT("aspectRatio"), Aspect);
        OutConfig->SetNumberField(TEXT("near"), Near);
        OutConfig->SetNumberField(TEXT("far"), Far);

        const float EyeX = ReadNodeFloat(*ProjectionNode, {TEXT("eyeX"), TEXT("eyepointX")}, ProjectorX);
        const float EyeY = ReadNodeFloat(*ProjectionNode, {TEXT("eyeY"), TEXT("eyepointY")}, ProjectorY);
        const float EyeZ = ReadNodeFloat(*ProjectionNode, {TEXT("eyeZ"), TEXT("eyepointZ")}, ProjectorZ);
        if (!FMath::IsNearlyEqual(EyeX, ProjectorX)
            || !FMath::IsNearlyEqual(EyeY, ProjectorY)
            || !FMath::IsNearlyEqual(EyeZ, ProjectorZ))
        {
            TSharedPtr<FJsonObject> Eye = MakeShared<FJsonObject>();
            Eye->SetNumberField(TEXT("x"), EyeX);
            Eye->SetNumberField(TEXT("y"), EyeY);
            Eye->SetNumberField(TEXT("z"), EyeZ);
            OutConfig->SetObjectField(TEXT("eyepoint"), Eye);
        }

        if (ProjectionType == TEXT("cylindrical") || ProjectionType == TEXT("radial"))
        {
            TSharedPtr<FJsonObject> Cylindrical = MakeShared<FJsonObject>();
            Cylindrical->SetStringField(TEXT("axis"), ReadNodeParam(*ProjectionNode, {TEXT("axis"), TEXT("cylinderAxis"), TEXT("cylAxis"), TEXT("cylindrical.axis")}).IsEmpty() ? TEXT("z") : ReadNodeParam(*ProjectionNode, {TEXT("axis"), TEXT("cylinderAxis"), TEXT("cylAxis"), TEXT("cylindrical.axis")}));
            Cylindrical->SetNumberField(TEXT("radius"), ReadNodeFloat(*ProjectionNode, {TEXT("radius"), TEXT("cylinderRadius"), TEXT("cylRadius"), TEXT("cylindrical.radius")}, 500.0f));
            Cylindrical->SetNumberField(TEXT("height"), ReadNodeFloat(*ProjectionNode, {TEXT("height"), TEXT("cylinderHeight"), TEXT("cylHeight"), TEXT("cylindrical.height")}, 1000.0f));
            Cylindrical->SetNumberField(TEXT("startAngle"), ReadNodeFloat(*ProjectionNode, {TEXT("startAngle"), TEXT("arcStart"), TEXT("cylindrical.startAngle")}, 0.0f));
            Cylindrical->SetNumberField(TEXT("endAngle"), ReadNodeFloat(*ProjectionNode, {TEXT("endAngle"), TEXT("arcEnd"), TEXT("cylindrical.endAngle")}, 360.0f));
            const FString EmitDirection = ReadNodeParam(*ProjectionNode, {TEXT("emitDirection"), TEXT("cylindrical.emitDirection")});
            if (!EmitDirection.IsEmpty())
            {
                Cylindrical->SetStringField(TEXT("emitDirection"), EmitDirection);
            }
            OutConfig->SetObjectField(TEXT("cylindrical"), Cylindrical);
        }
        else if (ProjectionType == TEXT("spherical"))
        {
            OutConfig->SetNumberField(TEXT("sphereRadius"), ReadNodeFloat(*ProjectionNode, {TEXT("sphereRadius"), TEXT("radius"), TEXT("spherical.radius")}, 500.0f));
            OutConfig->SetNumberField(TEXT("horizontalArc"), ReadNodeFloat(*ProjectionNode, {TEXT("horizontalArc"), TEXT("hArc"), TEXT("spherical.horizontalArc")}, 360.0f));
            OutConfig->SetNumberField(TEXT("verticalArc"), ReadNodeFloat(*ProjectionNode, {TEXT("verticalArc"), TEXT("vArc"), TEXT("spherical.verticalArc")}, 180.0f));
        }
        else if (ProjectionType == TEXT("parallel"))
        {
            OutConfig->SetNumberField(TEXT("sizeW"), ReadNodeFloat(*ProjectionNode, {TEXT("sizeW"), TEXT("parallelWidth"), TEXT("parallel.sizeW")}, 1000.0f));
            OutConfig->SetNumberField(TEXT("sizeH"), ReadNodeFloat(*ProjectionNode, {TEXT("sizeH"), TEXT("parallelHeight"), TEXT("parallel.sizeH")}, 1000.0f));
        }
        else if (ProjectionType == TEXT("fisheye"))
        {
            OutConfig->SetNumberField(TEXT("fisheyeFov"), ReadNodeFloat(*ProjectionNode, {TEXT("fisheyeFov"), TEXT("fov")}, 180.0f));
            const FString LensType = ReadNodeParam(*ProjectionNode, {TEXT("lensType"), TEXT("fisheye.lensType")});
            if (!LensType.IsEmpty())
            {
                OutConfig->SetStringField(TEXT("lensType"), LensType);
            }
        }
        else if (ProjectionType == TEXT("custom-matrix"))
        {
            TSharedPtr<FJsonObject> Matrix = MakeShared<FJsonObject>();
            for (int32 Row = 0; Row < 4; ++Row)
            {
                for (int32 Col = 0; Col < 4; ++Col)
                {
                    const FString Field = FString::Printf(TEXT("m%d%d"), Row, Col);
                    FString RawValue = ProjectionNode->Params.FindRef(Field).TrimStartAndEnd();
                    if (RawValue.IsEmpty())
                    {
                        RawValue = ProjectionNode->Params.FindRef(FString::Printf(TEXT("matrix.%s"), *Field)).TrimStartAndEnd();
                    }
                    const float DefaultValue = Row == Col ? 1.0f : 0.0f;
                    Matrix->SetNumberField(Field, RawValue.IsEmpty() ? DefaultValue : FCString::Atof(*RawValue));
                }
            }
            OutConfig->SetObjectField(TEXT("customProjectionMatrix"), Matrix);
        }
        else if (ProjectionType == TEXT("camera-plate"))
        {
            TSharedPtr<FJsonObject> CameraPlate = MakeShared<FJsonObject>();
            const FString Fit = ReadNodeParam(*ProjectionNode, {TEXT("fit"), TEXT("cameraPlateFit"), TEXT("cameraPlate.fit")});
            if (!Fit.IsEmpty())
            {
                CameraPlate->SetStringField(TEXT("fit"), Fit);
            }
            const FString Anchor = ReadNodeParam(*ProjectionNode, {TEXT("anchor"), TEXT("cameraPlateAnchor"), TEXT("cameraPlate.anchor")});
            if (!Anchor.IsEmpty())
            {
                CameraPlate->SetStringField(TEXT("anchor"), Anchor);
            }
            bool bHasFlipV = false;
            const bool bFlipV = ReadNodeBool(*ProjectionNode, {TEXT("flipV"), TEXT("cameraPlateFlipV"), TEXT("cameraPlate.flipV")}, false, &bHasFlipV);
            if (bHasFlipV)
            {
                CameraPlate->SetBoolField(TEXT("flipV"), bFlipV);
            }
            OutConfig->SetObjectField(TEXT("cameraPlate"), CameraPlate);
        }
        else if (ProjectionType == TEXT("spatial"))
        {
            TSharedPtr<FJsonObject> Spatial = MakeShared<FJsonObject>();
            Spatial->SetNumberField(TEXT("scaleU"), ReadNodeFloat(*ProjectionNode, {TEXT("scaleU"), TEXT("spatialScaleU"), TEXT("spatial.scaleU")}, 1.0f));
            Spatial->SetNumberField(TEXT("scaleV"), ReadNodeFloat(*ProjectionNode, {TEXT("scaleV"), TEXT("spatialScaleV"), TEXT("spatial.scaleV")}, 1.0f));
            Spatial->SetNumberField(TEXT("offsetU"), ReadNodeFloat(*ProjectionNode, {TEXT("offsetU"), TEXT("spatialOffsetU"), TEXT("spatial.offsetU")}, 0.0f));
            Spatial->SetNumberField(TEXT("offsetV"), ReadNodeFloat(*ProjectionNode, {TEXT("offsetV"), TEXT("spatialOffsetV"), TEXT("spatial.offsetV")}, 0.0f));
            OutConfig->SetObjectField(TEXT("spatial"), Spatial);
        }
        else if (ProjectionType == TEXT("depth-map"))
        {
            TSharedPtr<FJsonObject> DepthMap = MakeShared<FJsonObject>();
            DepthMap->SetNumberField(TEXT("depthScale"), ReadNodeFloat(*ProjectionNode, {TEXT("depthScale"), TEXT("depthMap.depthScale")}, 1.0f));
            DepthMap->SetNumberField(TEXT("depthBias"), ReadNodeFloat(*ProjectionNode, {TEXT("depthBias"), TEXT("depthMap.depthBias")}, 0.0f));
            DepthMap->SetNumberField(TEXT("depthNear"), ReadNodeFloat(*ProjectionNode, {TEXT("depthNear"), TEXT("depthMap.depthNear")}, 0.0f));
            DepthMap->SetNumberField(TEXT("depthFar"), ReadNodeFloat(*ProjectionNode, {TEXT("depthFar"), TEXT("depthMap.depthFar")}, 1.0f));
            OutConfig->SetObjectField(TEXT("depthMap"), DepthMap);
        }

        const FString ContentMode = ReadNodeParam(*ProjectionNode, {TEXT("contentMode")});
        if (!ContentMode.IsEmpty())
        {
            OutConfig->SetStringField(TEXT("contentMode"), ContentMode);
        }

        bool bHasClipOutside = false;
        const bool bClipOutside = ReadNodeBool(*ProjectionNode, {TEXT("clipOutsideRegion"), TEXT("clipOutside")}, false, &bHasClipOutside);
        if (bHasClipOutside)
        {
            OutConfig->SetBoolField(TEXT("clipOutsideRegion"), bClipOutside);
        }
        OutConfig->SetNumberField(TEXT("angleMaskStart"), ReadNodeFloat(*ProjectionNode, {TEXT("angleMaskStart"), TEXT("maskStart")}, 0.0f));
        OutConfig->SetNumberField(TEXT("angleMaskEnd"), ReadNodeFloat(*ProjectionNode, {TEXT("angleMaskEnd"), TEXT("maskEnd")}, 360.0f));
        OutConfig->SetNumberField(TEXT("borderExpansion"), ReadNodeFloat(*ProjectionNode, {TEXT("borderExpansion")}, 0.0f));
    };

    for (const FString& NodeId : TopologicalOrder)
    {
        const FRshipPipelineNode* Node = NodeById.FindRef(NodeId);
        if (!Node)
        {
            continue;
        }

        switch (Node->Kind)
        {
        case ERshipPipelineNodeKind::RenderContextInput:
        case ERshipPipelineNodeKind::ExternalTextureInput:
        case ERshipPipelineNodeKind::MediaProfileInput:
        {
            FRshipPipelineContextSpec Context;
            Context.Id = Node->Params.FindRef(TEXT("contextId")).TrimStartAndEnd();
            if (Context.Id.IsEmpty())
            {
                Context.Id = NodeId;
            }
            Context.Name = Node->Label.TrimStartAndEnd().IsEmpty() ? Context.Id : Node->Label.TrimStartAndEnd();
            Context.ProjectId = Node->Params.FindRef(TEXT("projectId")).TrimStartAndEnd();
            Context.Width = FMath::Max(1, ParseIntValue(Node->Params.FindRef(TEXT("width")), 1920));
            Context.Height = FMath::Max(1, ParseIntValue(Node->Params.FindRef(TEXT("height")), 1080));
            Context.CaptureMode = Node->Params.FindRef(TEXT("captureMode")).TrimStartAndEnd();
            if (Context.CaptureMode.IsEmpty())
            {
                Context.CaptureMode = TEXT("FinalColorLDR");
            }
            Context.bEnabled = ParseBoolValue(Node->Params.FindRef(TEXT("enabled")), true);

            if (Node->Kind == ERshipPipelineNodeKind::ExternalTextureInput)
            {
                Context.SourceType = TEXT("external-texture");
                Context.ExternalSourceId = Node->Params.FindRef(TEXT("externalSourceId")).TrimStartAndEnd();
            }
            else if (Node->Kind == ERshipPipelineNodeKind::MediaProfileInput)
            {
                Context.SourceType = TEXT("external-texture");
                const FString AdapterId = Node->Params.FindRef(TEXT("adapterId")).TrimStartAndEnd();
                const FString EndpointId = Node->Params.FindRef(TEXT("endpointId")).TrimStartAndEnd();
                Context.ExternalSourceId = FString::Printf(TEXT("media:%s:%s"), *AdapterId, *EndpointId);

                FRshipPipelineEndpointBinding& Binding = EndpointBindings.AddDefaulted_GetRef();
                Binding.AdapterId = AdapterId;
                Binding.EndpointId = EndpointId;
                Binding.Direction = TEXT("input");
                Binding.NodeId = NodeId;
            }
            else
            {
                Context.SourceType = Node->Params.FindRef(TEXT("sourceType")).TrimStartAndEnd();
                if (Context.SourceType.IsEmpty())
                {
                    Context.SourceType = TEXT("camera");
                }
                Context.CameraId = Node->Params.FindRef(TEXT("cameraId")).TrimStartAndEnd();
                Context.AssetId = Node->Params.FindRef(TEXT("assetId")).TrimStartAndEnd();
                Context.ExternalSourceId = Node->Params.FindRef(TEXT("externalSourceId")).TrimStartAndEnd();
            }

            ContextSpecs.Add(Context.Id, Context);
            ContextNodeToContextId.Add(NodeId, Context.Id);
            break;
        }
        case ERshipPipelineNodeKind::MappingSurfaceOutput:
        {
            FRshipPipelineSurfaceSpec Surface;
            Surface.Id = Node->Params.FindRef(TEXT("surfaceId")).TrimStartAndEnd();
            if (Surface.Id.IsEmpty())
            {
                Surface.Id = FString::Printf(TEXT("%s-surface"), *NodeId);
            }
            Surface.Name = Node->Params.FindRef(TEXT("surfaceName")).TrimStartAndEnd();
            if (Surface.Name.IsEmpty())
            {
                Surface.Name = Surface.Id;
            }
            Surface.ProjectId = Node->Params.FindRef(TEXT("projectId")).TrimStartAndEnd();
            Surface.ActorPath = Node->Params.FindRef(TEXT("actorPath")).TrimStartAndEnd();
            Surface.MeshComponentName = Node->Params.FindRef(TEXT("meshComponentName")).TrimStartAndEnd();
            Surface.UVChannel = FMath::Max(0, ParseIntValue(Node->Params.FindRef(TEXT("uvChannel")), 0));
            Surface.bEnabled = ParseBoolValue(Node->Params.FindRef(TEXT("enabled")), true);
            SurfaceSpecs.Add(Surface.Id, Surface);

            FRshipPipelineMappingSpec Mapping;
            Mapping.Id = Node->Params.FindRef(TEXT("mappingId")).TrimStartAndEnd();
            if (Mapping.Id.IsEmpty())
            {
                Mapping.Id = FString::Printf(TEXT("%s-mapping"), *NodeId);
            }
            Mapping.Name = Node->Label.TrimStartAndEnd().IsEmpty() ? Mapping.Id : Node->Label.TrimStartAndEnd();
            Mapping.ProjectId = Node->Params.FindRef(TEXT("projectId")).TrimStartAndEnd();
            Mapping.SurfaceIds = { Surface.Id };
            Mapping.Opacity = FMath::Clamp(ParseFloatValue(Node->Params.FindRef(TEXT("opacity")), 1.0f), 0.0f, 1.0f);
            Mapping.bEnabled = ParseBoolValue(Node->Params.FindRef(TEXT("enabled")), true);

            TSet<FString> UpstreamContexts;
            TArray<const FRshipPipelineNode*> ProjectionNodes;
            TArray<const FRshipPipelineNode*> TransformNodes;
            TArray<const FRshipPipelineNode*> OpacityNodes;
            bool bSawUnsupportedBlendNode = false;

            TSet<FString> VisitedNodeIds;
            TArray<FString> NodeStack = IncomingNodeIds.FindRef(NodeId);
            while (NodeStack.Num() > 0)
            {
                const FString CandidateNodeId = NodeStack.Pop(EAllowShrinking::No);
                if (VisitedNodeIds.Contains(CandidateNodeId))
                {
                    continue;
                }
                VisitedNodeIds.Add(CandidateNodeId);

                TArray<FString> UpstreamOfCandidate = IncomingNodeIds.FindRef(CandidateNodeId);
                UpstreamOfCandidate.Sort();
                for (int32 UpstreamIndex = UpstreamOfCandidate.Num() - 1; UpstreamIndex >= 0; --UpstreamIndex)
                {
                    NodeStack.Push(UpstreamOfCandidate[UpstreamIndex]);
                }
            }

            TArray<FString> OrderedVisitedIds = VisitedNodeIds.Array();
            OrderedVisitedIds.Sort([&TopologicalIndex](const FString& A, const FString& B)
            {
                const int32 IndexA = TopologicalIndex.FindRef(A);
                const int32 IndexB = TopologicalIndex.FindRef(B);
                if (IndexA == IndexB)
                {
                    return A < B;
                }
                return IndexA < IndexB;
            });

            for (const FString& VisitedNodeId : OrderedVisitedIds)
            {
                if (const FString* ContextId = ContextNodeToContextId.Find(VisitedNodeId))
                {
                    UpstreamContexts.Add(*ContextId);
                }

                const FRshipPipelineNode* CandidateNode = NodeById.FindRef(VisitedNodeId);
                if (!CandidateNode)
                {
                    continue;
                }

                switch (CandidateNode->Kind)
                {
                case ERshipPipelineNodeKind::Projection:
                    ProjectionNodes.Add(CandidateNode);
                    break;
                case ERshipPipelineNodeKind::TransformRect:
                    TransformNodes.Add(CandidateNode);
                    break;
                case ERshipPipelineNodeKind::Opacity:
                    OpacityNodes.Add(CandidateNode);
                    break;
                case ERshipPipelineNodeKind::BlendComposite:
                    bSawUnsupportedBlendNode = true;
                    break;
                default:
                    break;
                }
            }

            if (bSawUnsupportedBlendNode)
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.compile.blend_unsupported"),
                    FString::Printf(TEXT("Mapping output '%s' currently does not support blend/composite compile"), *NodeId),
                    NodeId);
            }

            if (UpstreamContexts.Num() == 0)
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.compile.missing_input"),
                    FString::Printf(TEXT("Mapping output '%s' has no upstream input context"), *NodeId),
                    NodeId);
            }
            else if (UpstreamContexts.Num() > 1)
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.compile.ambiguous_input"),
                    FString::Printf(TEXT("Mapping output '%s' has multiple upstream contexts"), *NodeId),
                    NodeId);
            }
            else
            {
                Mapping.ContextId = UpstreamContexts.Array()[0];
            }

            if (TransformNodes.Num() > 1)
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.compile.transform_ambiguous"),
                    FString::Printf(TEXT("Mapping output '%s' has multiple upstream transform nodes"), *NodeId),
                    NodeId);
            }

            if (ProjectionNodes.Num() > 1)
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.compile.ambiguous_projection"),
                    FString::Printf(TEXT("Mapping output '%s' has multiple upstream projection nodes"), *NodeId),
                    NodeId);
            }
            else if (ProjectionNodes.Num() == 0 && bStrictNoFallback)
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Error,
                    TEXT("pipeline.compile.missing_projection"),
                    FString::Printf(TEXT("Mapping output '%s' requires an explicit upstream projection node"), *NodeId),
                    NodeId);
            }

            TSharedPtr<FJsonObject> MappingConfig;
            FString CanonicalType;
            BuildProjectionConfig(ProjectionNodes.Num() == 1 ? ProjectionNodes[0] : nullptr, MappingConfig, CanonicalType);
            if (CanonicalType.IsEmpty())
            {
                Mapping.Type = TEXT("surface-uv");
                MappingConfig = MakeShared<FJsonObject>();
                MappingConfig->SetStringField(TEXT("uvMode"), TEXT("direct"));
            }
            else
            {
                Mapping.Type = CanonicalType;
            }

            if (TransformNodes.Num() == 1)
            {
                const FRshipPipelineNode* TransformNode = TransformNodes[0];
                if (TransformNode)
                {
                    TSharedPtr<FJsonObject> UVTransform = MakeShared<FJsonObject>();
                    UVTransform->SetNumberField(TEXT("scaleU"), ReadNodeFloat(*TransformNode, {TEXT("scaleU"), TEXT("uScale")}, 1.0f));
                    UVTransform->SetNumberField(TEXT("scaleV"), ReadNodeFloat(*TransformNode, {TEXT("scaleV"), TEXT("vScale")}, 1.0f));
                    UVTransform->SetNumberField(TEXT("offsetU"), ReadNodeFloat(*TransformNode, {TEXT("offsetU"), TEXT("uOffset"), TEXT("x")}, 0.0f));
                    UVTransform->SetNumberField(TEXT("offsetV"), ReadNodeFloat(*TransformNode, {TEXT("offsetV"), TEXT("vOffset"), TEXT("y")}, 0.0f));
                    UVTransform->SetNumberField(TEXT("rotationDeg"), ReadNodeFloat(*TransformNode, {TEXT("rotationDeg"), TEXT("rotation")}, 0.0f));
                    UVTransform->SetNumberField(TEXT("pivotU"), ReadNodeFloat(*TransformNode, {TEXT("pivotU")}, 0.5f));
                    UVTransform->SetNumberField(TEXT("pivotV"), ReadNodeFloat(*TransformNode, {TEXT("pivotV")}, 0.5f));
                    MappingConfig->SetObjectField(TEXT("uvTransform"), UVTransform);
                }
            }

            for (const FRshipPipelineNode* OpacityNode : OpacityNodes)
            {
                if (!OpacityNode)
                {
                    continue;
                }
                Mapping.Opacity *= FMath::Clamp(ReadNodeFloat(*OpacityNode, {TEXT("opacity"), TEXT("value"), TEXT("alpha")}, 1.0f), 0.0f, 1.0f);
            }
            Mapping.Opacity = FMath::Clamp(Mapping.Opacity, 0.0f, 1.0f);

            Mapping.ConfigJson = JsonToString(MappingConfig);

            FRshipPipelineMappingSpec* ExistingMapping = MappingSpecs.Find(Mapping.Id);
            if (!ExistingMapping)
            {
                MappingSpecs.Add(Mapping.Id, Mapping);
            }
            else
            {
                if (!ExistingMapping->ContextId.Equals(Mapping.ContextId, ESearchCase::CaseSensitive))
                {
                    AddPipelineDiagnostic(
                        OutDiagnostics,
                        ERshipPipelineDiagnosticSeverity::Error,
                        TEXT("pipeline.compile.mapping_context_conflict"),
                        FString::Printf(TEXT("Mapping '%s' has conflicting context bindings across outputs"), *Mapping.Id),
                        NodeId);
                }

                if (!ExistingMapping->Type.Equals(Mapping.Type, ESearchCase::CaseSensitive)
                    || ExistingMapping->ConfigJson != Mapping.ConfigJson)
                {
                    AddPipelineDiagnostic(
                        OutDiagnostics,
                        ERshipPipelineDiagnosticSeverity::Error,
                        TEXT("pipeline.compile.mapping_mode_conflict"),
                        FString::Printf(TEXT("Mapping '%s' has conflicting projection/UV config across outputs"), *Mapping.Id),
                        NodeId);
                }

                if (!FMath::IsNearlyEqual(ExistingMapping->Opacity, Mapping.Opacity))
                {
                    AddPipelineDiagnostic(
                        OutDiagnostics,
                        ERshipPipelineDiagnosticSeverity::Error,
                        TEXT("pipeline.compile.mapping_opacity_conflict"),
                        FString::Printf(TEXT("Mapping '%s' has conflicting opacity across outputs"), *Mapping.Id),
                        NodeId);
                }

                ExistingMapping->bEnabled = ExistingMapping->bEnabled && Mapping.bEnabled;
                ExistingMapping->SurfaceIds.AddUnique(Surface.Id);
                ExistingMapping->SurfaceIds.Sort();
            }
            break;
        }
        case ERshipPipelineNodeKind::MediaProfileOutput:
        {
            FRshipPipelineEndpointBinding& Binding = EndpointBindings.AddDefaulted_GetRef();
            Binding.AdapterId = Node->Params.FindRef(TEXT("adapterId")).TrimStartAndEnd();
            Binding.EndpointId = Node->Params.FindRef(TEXT("endpointId")).TrimStartAndEnd();
            Binding.Direction = TEXT("output");
            Binding.NodeId = NodeId;
            break;
        }
        case ERshipPipelineNodeKind::Projection:
        case ERshipPipelineNodeKind::TransformRect:
        case ERshipPipelineNodeKind::Opacity:
        case ERshipPipelineNodeKind::BlendComposite:
            break;
        default:
            break;
        }
    }

    const bool bHasErrors = OutDiagnostics.ContainsByPredicate(
        [](const FRshipPipelineDiagnostic& Diagnostic)
        {
            return Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Error;
        });

    OutPlan.AssetId = PipelineAsset->AssetId;
    OutPlan.Revision = PipelineAsset->Revision;
    OutPlan.TopologicalOrder = TopologicalOrder;
    OutPlan.EndpointBindings = EndpointBindings;

    {
        TArray<FString> Keys;
        ContextSpecs.GetKeys(Keys);
        Keys.Sort();
        for (const FString& Key : Keys)
        {
            OutPlan.Contexts.Add(ContextSpecs[Key]);
        }
    }

    {
        TArray<FString> Keys;
        SurfaceSpecs.GetKeys(Keys);
        Keys.Sort();
        for (const FString& Key : Keys)
        {
            OutPlan.Surfaces.Add(SurfaceSpecs[Key]);
        }
    }

    {
        TArray<FString> Keys;
        MappingSpecs.GetKeys(Keys);
        Keys.Sort();
        for (const FString& Key : Keys)
        {
            OutPlan.Mappings.Add(MappingSpecs[Key]);
        }
    }

    OutPlan.Diagnostics = OutDiagnostics;
    OutPlan.bValid = !bHasErrors;
    return OutPlan.bValid;
}

bool URshipContentMappingManager::ApplyCompiledPipelinePlan(
    const FRshipCompiledPipelinePlan& Plan,
    TArray<FRshipPipelineDiagnostic>& OutDiagnostics)
{
    OutDiagnostics.Reset();

    if (!Plan.bValid)
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.apply.invalid_plan"),
            TEXT("Compiled pipeline plan is not valid"));
        return false;
    }

    const auto ParsePlanBool = [](bool Value) -> bool
    {
        return Value;
    };

    CapturePipelineRuntimeSnapshot();

    const int32 PreviousSemanticFallback = CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread();
    const int32 PreviousSurfaceFallback = CVarRshipContentMappingAllowSurfaceMaterialFallback.GetValueOnGameThread();
    CVarRshipContentMappingAllowSemanticFallbacks->Set(0, ECVF_SetByCode);
    CVarRshipContentMappingAllowSurfaceMaterialFallback->Set(0, ECVF_SetByCode);

    auto RestoreFallbackCVars = [PreviousSemanticFallback, PreviousSurfaceFallback]()
    {
        CVarRshipContentMappingAllowSemanticFallbacks->Set(PreviousSemanticFallback, ECVF_SetByCode);
        CVarRshipContentMappingAllowSurfaceMaterialFallback->Set(PreviousSurfaceFallback, ECVF_SetByCode);
    };

    auto FailAndRollback = [this, &OutDiagnostics, &RestoreFallbackCVars](const FString& Code, const FString& Message) -> bool
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            Code,
            Message);
        TArray<FRshipPipelineDiagnostic> RollbackDiagnostics;
        RollbackLastPipelineApply(RollbackDiagnostics);
        OutDiagnostics.Append(RollbackDiagnostics);
        RestoreFallbackCVars();
        return false;
    };

    for (const FRshipPipelineEndpointBinding& Binding : Plan.EndpointBindings)
    {
        const FString AdapterId = Binding.AdapterId.TrimStartAndEnd();
        const FString EndpointId = Binding.EndpointId.TrimStartAndEnd();
        const FString Direction = Binding.Direction.TrimStartAndEnd().ToLower();
        if (AdapterId.IsEmpty() || EndpointId.IsEmpty())
        {
            return FailAndRollback(
                TEXT("pipeline.apply.endpoint_missing"),
                FString::Printf(
                    TEXT("Endpoint binding missing adapterId/endpointId for node '%s'"),
                    *Binding.NodeId));
        }

        bool bValidated = false;
        FString LastAdapterError;
        for (const TWeakObjectPtr<UObject>& AdapterWeak : PipelineEndpointAdapters)
        {
            UObject* AdapterObject = AdapterWeak.Get();
            if (!AdapterObject || !IsValid(AdapterObject))
            {
                continue;
            }
            if (!AdapterObject->GetClass()->ImplementsInterface(URshipTexturePipelineEndpointAdapter::StaticClass()))
            {
                continue;
            }

            FString AdapterError;
            if (IRshipTexturePipelineEndpointAdapter::Execute_ValidateEndpoint(
                AdapterObject,
                AdapterId,
                EndpointId,
                Direction,
                AdapterError))
            {
                bValidated = true;
                break;
            }
            if (!AdapterError.IsEmpty())
            {
                LastAdapterError = AdapterError;
            }
        }

        if (!bValidated)
        {
            return FailAndRollback(
                TEXT("pipeline.apply.endpoint_invalid"),
                LastAdapterError.IsEmpty()
                    ? FString::Printf(
                        TEXT("Endpoint '%s/%s' failed validation for direction '%s'"),
                        *AdapterId,
                        *EndpointId,
                        *Direction)
                    : LastAdapterError);
        }
    }

    TSet<FString> DesiredContextIds;
    for (const FRshipPipelineContextSpec& ContextSpec : Plan.Contexts)
    {
        const FString ContextId = ContextSpec.Id.TrimStartAndEnd();
        if (!ContextId.IsEmpty())
        {
            DesiredContextIds.Add(ContextId);
        }
    }

    TSet<FString> DesiredSurfaceIds;
    for (const FRshipPipelineSurfaceSpec& SurfaceSpec : Plan.Surfaces)
    {
        const FString SurfaceId = SurfaceSpec.Id.TrimStartAndEnd();
        if (!SurfaceId.IsEmpty())
        {
            DesiredSurfaceIds.Add(SurfaceId);
        }
    }

    TSet<FString> DesiredMappingIds;
    for (const FRshipPipelineMappingSpec& MappingSpec : Plan.Mappings)
    {
        const FString MappingId = MappingSpec.Id.TrimStartAndEnd();
        if (!MappingId.IsEmpty())
        {
            DesiredMappingIds.Add(MappingId);
        }
    }

    TArray<FString> ExistingMappingIds;
    Mappings.GetKeys(ExistingMappingIds);
    for (const FString& MappingId : ExistingMappingIds)
    {
        if (!DesiredMappingIds.Contains(MappingId))
        {
            DeleteMapping(MappingId);
        }
    }

    TArray<FString> ExistingSurfaceIds;
    MappingSurfaces.GetKeys(ExistingSurfaceIds);
    for (const FString& SurfaceId : ExistingSurfaceIds)
    {
        if (!DesiredSurfaceIds.Contains(SurfaceId))
        {
            DeleteMappingSurface(SurfaceId);
        }
    }

    TArray<FString> ExistingContextIds;
    RenderContexts.GetKeys(ExistingContextIds);
    for (const FString& ContextId : ExistingContextIds)
    {
        if (!DesiredContextIds.Contains(ContextId))
        {
            DeleteRenderContext(ContextId);
        }
    }

    TMap<FString, FString> ContextIdRemap;
    for (const FRshipPipelineContextSpec& ContextSpec : Plan.Contexts)
    {
        const FString ContextId = ContextSpec.Id.TrimStartAndEnd();
        if (ContextId.IsEmpty())
        {
            return FailAndRollback(TEXT("pipeline.apply.context_id_empty"), TEXT("Context spec has empty id"));
        }

        FRshipRenderContextState State;
        State.Id = ContextId;
        State.Name = ContextSpec.Name;
        State.ProjectId = ContextSpec.ProjectId;
        State.SourceType = ContextSpec.SourceType;
        State.CameraId = ContextSpec.CameraId;
        State.AssetId = ContextSpec.AssetId;
        State.ExternalSourceId = ContextSpec.ExternalSourceId;
        State.Width = ContextSpec.Width;
        State.Height = ContextSpec.Height;
        State.CaptureMode = ContextSpec.CaptureMode;
        State.bEnabled = ParsePlanBool(ContextSpec.bEnabled);

        if (RenderContexts.Contains(ContextId))
        {
            if (!UpdateRenderContext(State))
            {
                return FailAndRollback(
                    TEXT("pipeline.apply.context_update_failed"),
                    FString::Printf(TEXT("Failed to update context '%s'"), *ContextId));
            }
            ContextIdRemap.Add(ContextId, ContextId);
        }
        else
        {
            const FString CreatedId = CreateRenderContext(State);
            if (CreatedId.IsEmpty())
            {
                return FailAndRollback(
                    TEXT("pipeline.apply.context_create_failed"),
                    FString::Printf(TEXT("Failed to create context '%s'"), *ContextId));
            }
            ContextIdRemap.Add(ContextId, CreatedId);
            if (!CreatedId.Equals(ContextId, ESearchCase::CaseSensitive))
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Warning,
                    TEXT("pipeline.apply.context_id_remap"),
                    FString::Printf(TEXT("Context '%s' was remapped to '%s'"), *ContextId, *CreatedId));
            }
        }
    }

    for (const FRshipPipelineSurfaceSpec& SurfaceSpec : Plan.Surfaces)
    {
        const FString SurfaceId = SurfaceSpec.Id.TrimStartAndEnd();
        if (SurfaceId.IsEmpty())
        {
            return FailAndRollback(TEXT("pipeline.apply.surface_id_empty"), TEXT("Surface spec has empty id"));
        }

        FRshipMappingSurfaceState State;
        State.Id = SurfaceId;
        State.Name = SurfaceSpec.Name;
        State.ProjectId = SurfaceSpec.ProjectId;
        State.ActorPath = SurfaceSpec.ActorPath;
        State.MeshComponentName = SurfaceSpec.MeshComponentName;
        State.UVChannel = SurfaceSpec.UVChannel;
        State.MaterialSlots = SurfaceSpec.MaterialSlots;
        State.bEnabled = ParsePlanBool(SurfaceSpec.bEnabled);

        if (MappingSurfaces.Contains(SurfaceId))
        {
            if (!UpdateMappingSurface(State))
            {
                return FailAndRollback(
                    TEXT("pipeline.apply.surface_update_failed"),
                    FString::Printf(TEXT("Failed to update surface '%s'"), *SurfaceId));
            }
        }
        else
        {
            const FString CreatedId = CreateMappingSurface(State);
            if (CreatedId.IsEmpty())
            {
                return FailAndRollback(
                    TEXT("pipeline.apply.surface_create_failed"),
                    FString::Printf(TEXT("Failed to create surface '%s'"), *SurfaceId));
            }
            if (!CreatedId.Equals(SurfaceId, ESearchCase::CaseSensitive))
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Warning,
                    TEXT("pipeline.apply.surface_id_remap"),
                    FString::Printf(TEXT("Surface '%s' was remapped to '%s'"), *SurfaceId, *CreatedId));
            }
        }
    }

    for (const FRshipPipelineMappingSpec& MappingSpec : Plan.Mappings)
    {
        const FString MappingId = MappingSpec.Id.TrimStartAndEnd();
        if (MappingId.IsEmpty())
        {
            return FailAndRollback(TEXT("pipeline.apply.mapping_id_empty"), TEXT("Mapping spec has empty id"));
        }

        FRshipContentMappingState State;
        State.Id = MappingId;
        State.Name = MappingSpec.Name;
        State.ProjectId = MappingSpec.ProjectId;
        State.Type = MappingSpec.Type;
        State.ContextId = ContextIdRemap.FindRef(MappingSpec.ContextId.TrimStartAndEnd());
        if (State.ContextId.IsEmpty())
        {
            State.ContextId = MappingSpec.ContextId.TrimStartAndEnd();
        }
        State.SurfaceIds = MappingSpec.SurfaceIds;
        State.Opacity = MappingSpec.Opacity;
        State.bEnabled = ParsePlanBool(MappingSpec.bEnabled);
        State.Config = ParseJsonObjectString(MappingSpec.ConfigJson);
        if (!State.Config.IsValid())
        {
            State.Config = MakeShared<FJsonObject>();
        }

        if (Mappings.Contains(MappingId))
        {
            if (!UpdateMapping(State))
            {
                // Mapping mode/type updates are intentionally immutable in UpdateMapping.
                // Apply deterministic replacement when a mode transition is required.
                DeleteMapping(MappingId);
                const FString CreatedId = CreateMapping(State);
                if (CreatedId.IsEmpty())
                {
                    return FailAndRollback(
                        TEXT("pipeline.apply.mapping_replace_failed"),
                        FString::Printf(TEXT("Failed to replace mapping '%s'"), *MappingId));
                }
                if (!CreatedId.Equals(MappingId, ESearchCase::CaseSensitive))
                {
                    AddPipelineDiagnostic(
                        OutDiagnostics,
                        ERshipPipelineDiagnosticSeverity::Warning,
                        TEXT("pipeline.apply.mapping_id_remap"),
                        FString::Printf(TEXT("Mapping '%s' was remapped to '%s'"), *MappingId, *CreatedId));
                }
            }
        }
        else
        {
            const FString CreatedId = CreateMapping(State);
            if (CreatedId.IsEmpty())
            {
                return FailAndRollback(
                    TEXT("pipeline.apply.mapping_create_failed"),
                    FString::Printf(TEXT("Failed to create mapping '%s'"), *MappingId));
            }
            if (!CreatedId.Equals(MappingId, ESearchCase::CaseSensitive))
            {
                AddPipelineDiagnostic(
                    OutDiagnostics,
                    ERshipPipelineDiagnosticSeverity::Warning,
                    TEXT("pipeline.apply.mapping_id_remap"),
                    FString::Printf(TEXT("Mapping '%s' was remapped to '%s'"), *MappingId, *CreatedId));
            }
        }
    }

    for (const FRshipPipelineMappingSpec& MappingSpec : Plan.Mappings)
    {
        const FString MappingId = MappingSpec.Id.TrimStartAndEnd();
        const FRshipContentMappingState* Stored = Mappings.Find(MappingId);
        if (!Stored)
        {
            return FailAndRollback(
                TEXT("pipeline.apply.mapping_missing"),
                FString::Printf(TEXT("Mapping '%s' missing after apply"), *MappingId));
        }
        if (MappingSpec.bEnabled && !Stored->bEnabled)
        {
            return FailAndRollback(
                TEXT("pipeline.apply.mapping_disabled"),
                FString::Printf(TEXT("Mapping '%s' expected enabled but was disabled"), *MappingId));
        }
    }

    SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
    RestoreFallbackCVars();
    return true;
}

bool URshipContentMappingManager::RollbackLastPipelineApply(TArray<FRshipPipelineDiagnostic>& OutDiagnostics)
{
    OutDiagnostics.Reset();

    if (!LastPipelineSnapshot.bValid)
    {
        AddPipelineDiagnostic(
            OutDiagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.rollback.no_snapshot"),
            TEXT("No pipeline snapshot is available for rollback"));
        return false;
    }

    RestorePipelineRuntimeSnapshot(LastPipelineSnapshot);
    LastPipelineSnapshot = FPipelineRuntimeSnapshot();

    AddPipelineDiagnostic(
        OutDiagnostics,
        ERshipPipelineDiagnosticSeverity::Info,
        TEXT("pipeline.rollback.success"),
        TEXT("Pipeline runtime state was restored from snapshot"));
    return true;
}

bool URshipContentMappingManager::ValidatePipelineGraphJson(const FString& PipelineGraphJson, FString& OutDiagnosticsJson) const
{
    OutDiagnosticsJson.Reset();

    const TSharedPtr<FJsonObject> Root = ParseJsonObjectString(PipelineGraphJson);
    if (!Root.IsValid())
    {
        TArray<FRshipPipelineDiagnostic> Diagnostics;
        AddPipelineDiagnostic(
            Diagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.json.parse_failed"),
            TEXT("Pipeline graph JSON is invalid"));

        TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
        for (const FRshipPipelineDiagnostic& Diagnostic : Diagnostics)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            const FString Severity = Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Error
                ? TEXT("error")
                : (Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
            Obj->SetStringField(TEXT("severity"), Severity);
            Obj->SetStringField(TEXT("code"), Diagnostic.Code);
            Obj->SetStringField(TEXT("message"), Diagnostic.Message);
            Obj->SetStringField(TEXT("nodeId"), Diagnostic.NodeId);
            Obj->SetStringField(TEXT("edgeId"), Diagnostic.EdgeId);
            DiagnosticValues.Add(MakeShared<FJsonValueObject>(Obj));
        }
        Envelope->SetBoolField(TEXT("valid"), false);
        Envelope->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
        OutDiagnosticsJson = JsonToString(Envelope);
        return false;
    }

    URshipTexturePipelineAsset* PipelineAsset = NewObject<URshipTexturePipelineAsset>(GetTransientPackage());
    PipelineAsset->AssetId = GetStringField(Root, TEXT("assetId"), TEXT("runtime"));
    PipelineAsset->Revision = static_cast<int64>(GetIntField(Root, TEXT("revision"), 1));
    PipelineAsset->bStrictNoFallback = GetBoolField(Root, TEXT("bStrictNoFallback"), true);

    auto ParseNodeKind = [](const FString& KindText) -> ERshipPipelineNodeKind
    {
        const FString Token = KindText.TrimStartAndEnd().ToLower();
        if (Token == TEXT("rendercontextinput") || Token == TEXT("render-context-input")) return ERshipPipelineNodeKind::RenderContextInput;
        if (Token == TEXT("externaltextureinput") || Token == TEXT("external-texture-input")) return ERshipPipelineNodeKind::ExternalTextureInput;
        if (Token == TEXT("mediaprofileinput") || Token == TEXT("media-profile-input")) return ERshipPipelineNodeKind::MediaProfileInput;
        if (Token == TEXT("projection")) return ERshipPipelineNodeKind::Projection;
        if (Token == TEXT("transformrect") || Token == TEXT("transform-rect")) return ERshipPipelineNodeKind::TransformRect;
        if (Token == TEXT("opacity")) return ERshipPipelineNodeKind::Opacity;
        if (Token == TEXT("blendcomposite") || Token == TEXT("blend-composite")) return ERshipPipelineNodeKind::BlendComposite;
        if (Token == TEXT("mappingsurfaceoutput") || Token == TEXT("mapping-surface-output")) return ERshipPipelineNodeKind::MappingSurfaceOutput;
        if (Token == TEXT("mediaprofileoutput") || Token == TEXT("media-profile-output")) return ERshipPipelineNodeKind::MediaProfileOutput;
        return ERshipPipelineNodeKind::RenderContextInput;
    };

    if (Root->HasTypedField<EJson::Array>(TEXT("nodes")))
    {
        for (const TSharedPtr<FJsonValue>& NodeValue : Root->GetArrayField(TEXT("nodes")))
        {
            if (!NodeValue.IsValid() || NodeValue->Type != EJson::Object)
            {
                continue;
            }
            const TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
            if (!NodeObj.IsValid())
            {
                continue;
            }

            FRshipPipelineNode Node;
            Node.Id = GetStringField(NodeObj, TEXT("id"));
            Node.Label = GetStringField(NodeObj, TEXT("label"));
            Node.Kind = ParseNodeKind(GetStringField(NodeObj, TEXT("kind")));
            if (NodeObj->HasTypedField<EJson::Object>(TEXT("position")))
            {
                const TSharedPtr<FJsonObject> Pos = NodeObj->GetObjectField(TEXT("position"));
                Node.Position.X = static_cast<double>(GetNumberField(Pos, TEXT("x"), 0.0f));
                Node.Position.Y = static_cast<double>(GetNumberField(Pos, TEXT("y"), 0.0f));
            }

            if (NodeObj->HasTypedField<EJson::Object>(TEXT("params")))
            {
                const TSharedPtr<FJsonObject> ParamsObj = NodeObj->GetObjectField(TEXT("params"));
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : ParamsObj->Values)
                {
                    if (!Pair.Value.IsValid())
                    {
                        continue;
                    }
                    switch (Pair.Value->Type)
                    {
                    case EJson::String:
                        Node.Params.Add(Pair.Key, Pair.Value->AsString());
                        break;
                    case EJson::Boolean:
                        Node.Params.Add(Pair.Key, Pair.Value->AsBool() ? TEXT("true") : TEXT("false"));
                        break;
                    case EJson::Number:
                        Node.Params.Add(Pair.Key, FString::SanitizeFloat(Pair.Value->AsNumber()));
                        break;
                    default:
                        break;
                    }
                }
            }

            PipelineAsset->Nodes.Add(Node);
        }
    }

    if (Root->HasTypedField<EJson::Array>(TEXT("edges")))
    {
        for (const TSharedPtr<FJsonValue>& EdgeValue : Root->GetArrayField(TEXT("edges")))
        {
            if (!EdgeValue.IsValid() || EdgeValue->Type != EJson::Object)
            {
                continue;
            }
            const TSharedPtr<FJsonObject> EdgeObj = EdgeValue->AsObject();
            if (!EdgeObj.IsValid())
            {
                continue;
            }

            FRshipPipelineEdge Edge;
            Edge.Id = GetStringField(EdgeObj, TEXT("id"));
            Edge.FromNodeId = GetStringField(EdgeObj, TEXT("fromNodeId"));
            Edge.FromPinId = GetStringField(EdgeObj, TEXT("fromPinId"));
            Edge.ToNodeId = GetStringField(EdgeObj, TEXT("toNodeId"));
            Edge.ToPinId = GetStringField(EdgeObj, TEXT("toPinId"));
            PipelineAsset->Edges.Add(Edge);
        }
    }

    TArray<FRshipPipelineDiagnostic> Diagnostics;
    const bool bValid = ValidatePipelineGraph(PipelineAsset, Diagnostics);

    TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
    Envelope->SetBoolField(TEXT("valid"), bValid);
    TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
    for (const FRshipPipelineDiagnostic& Diagnostic : Diagnostics)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        const FString Severity = Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Error
            ? TEXT("error")
            : (Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
        Obj->SetStringField(TEXT("severity"), Severity);
        Obj->SetStringField(TEXT("code"), Diagnostic.Code);
        Obj->SetStringField(TEXT("message"), Diagnostic.Message);
        Obj->SetStringField(TEXT("nodeId"), Diagnostic.NodeId);
        Obj->SetStringField(TEXT("edgeId"), Diagnostic.EdgeId);
        DiagnosticValues.Add(MakeShared<FJsonValueObject>(Obj));
    }
    Envelope->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
    OutDiagnosticsJson = JsonToString(Envelope);
    return bValid;
}

bool URshipContentMappingManager::CompilePipelineGraphJson(
    const FString& PipelineGraphJson,
    FString& OutPlanJson,
    FString& OutDiagnosticsJson) const
{
    OutPlanJson.Reset();
    OutDiagnosticsJson.Reset();

    const TSharedPtr<FJsonObject> Root = ParseJsonObjectString(PipelineGraphJson);
    if (!Root.IsValid())
    {
        TArray<FRshipPipelineDiagnostic> Diagnostics;
        AddPipelineDiagnostic(
            Diagnostics,
            ERshipPipelineDiagnosticSeverity::Error,
            TEXT("pipeline.json.parse_failed"),
            TEXT("Pipeline graph JSON is invalid"));
        TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
        Envelope->SetBoolField(TEXT("valid"), false);
        TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
        for (const FRshipPipelineDiagnostic& Diagnostic : Diagnostics)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("severity"), TEXT("error"));
            Obj->SetStringField(TEXT("code"), Diagnostic.Code);
            Obj->SetStringField(TEXT("message"), Diagnostic.Message);
            DiagnosticValues.Add(MakeShared<FJsonValueObject>(Obj));
        }
        Envelope->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
        OutDiagnosticsJson = JsonToString(Envelope);
        return false;
    }

    URshipTexturePipelineAsset* PipelineAsset = NewObject<URshipTexturePipelineAsset>(GetTransientPackage());
    PipelineAsset->AssetId = GetStringField(Root, TEXT("assetId"), TEXT("runtime"));
    PipelineAsset->Revision = static_cast<int64>(GetIntField(Root, TEXT("revision"), 1));
    PipelineAsset->bStrictNoFallback = GetBoolField(Root, TEXT("bStrictNoFallback"), true);

    auto ParseNodeKind = [](const FString& KindText) -> ERshipPipelineNodeKind
    {
        const FString Token = KindText.TrimStartAndEnd().ToLower();
        if (Token == TEXT("rendercontextinput") || Token == TEXT("render-context-input")) return ERshipPipelineNodeKind::RenderContextInput;
        if (Token == TEXT("externaltextureinput") || Token == TEXT("external-texture-input")) return ERshipPipelineNodeKind::ExternalTextureInput;
        if (Token == TEXT("mediaprofileinput") || Token == TEXT("media-profile-input")) return ERshipPipelineNodeKind::MediaProfileInput;
        if (Token == TEXT("projection")) return ERshipPipelineNodeKind::Projection;
        if (Token == TEXT("transformrect") || Token == TEXT("transform-rect")) return ERshipPipelineNodeKind::TransformRect;
        if (Token == TEXT("opacity")) return ERshipPipelineNodeKind::Opacity;
        if (Token == TEXT("blendcomposite") || Token == TEXT("blend-composite")) return ERshipPipelineNodeKind::BlendComposite;
        if (Token == TEXT("mappingsurfaceoutput") || Token == TEXT("mapping-surface-output")) return ERshipPipelineNodeKind::MappingSurfaceOutput;
        if (Token == TEXT("mediaprofileoutput") || Token == TEXT("media-profile-output")) return ERshipPipelineNodeKind::MediaProfileOutput;
        return ERshipPipelineNodeKind::RenderContextInput;
    };

    if (Root->HasTypedField<EJson::Array>(TEXT("nodes")))
    {
        for (const TSharedPtr<FJsonValue>& NodeValue : Root->GetArrayField(TEXT("nodes")))
        {
            if (!NodeValue.IsValid() || NodeValue->Type != EJson::Object)
            {
                continue;
            }
            const TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
            if (!NodeObj.IsValid())
            {
                continue;
            }

            FRshipPipelineNode Node;
            Node.Id = GetStringField(NodeObj, TEXT("id"));
            Node.Label = GetStringField(NodeObj, TEXT("label"));
            Node.Kind = ParseNodeKind(GetStringField(NodeObj, TEXT("kind")));
            if (NodeObj->HasTypedField<EJson::Object>(TEXT("params")))
            {
                const TSharedPtr<FJsonObject> ParamsObj = NodeObj->GetObjectField(TEXT("params"));
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : ParamsObj->Values)
                {
                    if (!Pair.Value.IsValid())
                    {
                        continue;
                    }
                    switch (Pair.Value->Type)
                    {
                    case EJson::String:
                        Node.Params.Add(Pair.Key, Pair.Value->AsString());
                        break;
                    case EJson::Boolean:
                        Node.Params.Add(Pair.Key, Pair.Value->AsBool() ? TEXT("true") : TEXT("false"));
                        break;
                    case EJson::Number:
                        Node.Params.Add(Pair.Key, FString::SanitizeFloat(Pair.Value->AsNumber()));
                        break;
                    default:
                        break;
                    }
                }
            }

            PipelineAsset->Nodes.Add(Node);
        }
    }

    if (Root->HasTypedField<EJson::Array>(TEXT("edges")))
    {
        for (const TSharedPtr<FJsonValue>& EdgeValue : Root->GetArrayField(TEXT("edges")))
        {
            if (!EdgeValue.IsValid() || EdgeValue->Type != EJson::Object)
            {
                continue;
            }
            const TSharedPtr<FJsonObject> EdgeObj = EdgeValue->AsObject();
            if (!EdgeObj.IsValid())
            {
                continue;
            }

            FRshipPipelineEdge Edge;
            Edge.Id = GetStringField(EdgeObj, TEXT("id"));
            Edge.FromNodeId = GetStringField(EdgeObj, TEXT("fromNodeId"));
            Edge.FromPinId = GetStringField(EdgeObj, TEXT("fromPinId"));
            Edge.ToNodeId = GetStringField(EdgeObj, TEXT("toNodeId"));
            Edge.ToPinId = GetStringField(EdgeObj, TEXT("toPinId"));
            PipelineAsset->Edges.Add(Edge);
        }
    }

    FRshipCompiledPipelinePlan Plan;
    TArray<FRshipPipelineDiagnostic> Diagnostics;
    const bool bCompiled = CompilePipelineGraph(PipelineAsset, Plan, Diagnostics);
    FJsonObjectConverter::UStructToJsonObjectString(FRshipCompiledPipelinePlan::StaticStruct(), &Plan, OutPlanJson, 0, 0, 0, nullptr, false);

    TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
    Envelope->SetBoolField(TEXT("valid"), bCompiled);
    TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
    for (const FRshipPipelineDiagnostic& Diagnostic : Diagnostics)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        const FString Severity = Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Error
            ? TEXT("error")
            : (Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
        Obj->SetStringField(TEXT("severity"), Severity);
        Obj->SetStringField(TEXT("code"), Diagnostic.Code);
        Obj->SetStringField(TEXT("message"), Diagnostic.Message);
        Obj->SetStringField(TEXT("nodeId"), Diagnostic.NodeId);
        Obj->SetStringField(TEXT("edgeId"), Diagnostic.EdgeId);
        DiagnosticValues.Add(MakeShared<FJsonValueObject>(Obj));
    }
    Envelope->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
    OutDiagnosticsJson = JsonToString(Envelope);
    return bCompiled;
}

bool URshipContentMappingManager::ApplyCompiledPipelinePlanJson(const FString& CompiledPlanJson, FString& OutDiagnosticsJson)
{
    OutDiagnosticsJson.Reset();

    FRshipCompiledPipelinePlan Plan;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(CompiledPlanJson, &Plan, 0, 0))
    {
        TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
        Envelope->SetBoolField(TEXT("applied"), false);
        TArray<TSharedPtr<FJsonValue>> Diagnostics;
        TSharedPtr<FJsonObject> Diagnostic = MakeShared<FJsonObject>();
        Diagnostic->SetStringField(TEXT("severity"), TEXT("error"));
        Diagnostic->SetStringField(TEXT("code"), TEXT("pipeline.plan.parse_failed"));
        Diagnostic->SetStringField(TEXT("message"), TEXT("Compiled pipeline plan JSON is invalid"));
        Diagnostics.Add(MakeShared<FJsonValueObject>(Diagnostic));
        Envelope->SetArrayField(TEXT("diagnostics"), Diagnostics);
        OutDiagnosticsJson = JsonToString(Envelope);
        return false;
    }

    TArray<FRshipPipelineDiagnostic> Diagnostics;
    const bool bApplied = ApplyCompiledPipelinePlan(Plan, Diagnostics);

    TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
    Envelope->SetBoolField(TEXT("applied"), bApplied);
    TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
    for (const FRshipPipelineDiagnostic& Diagnostic : Diagnostics)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        const FString Severity = Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Error
            ? TEXT("error")
            : (Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
        Obj->SetStringField(TEXT("severity"), Severity);
        Obj->SetStringField(TEXT("code"), Diagnostic.Code);
        Obj->SetStringField(TEXT("message"), Diagnostic.Message);
        Obj->SetStringField(TEXT("nodeId"), Diagnostic.NodeId);
        Obj->SetStringField(TEXT("edgeId"), Diagnostic.EdgeId);
        DiagnosticValues.Add(MakeShared<FJsonValueObject>(Obj));
    }
    Envelope->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
    OutDiagnosticsJson = JsonToString(Envelope);
    return bApplied;
}

bool URshipContentMappingManager::RollbackLastPipelineApplyJson(FString& OutDiagnosticsJson)
{
    OutDiagnosticsJson.Reset();

    TArray<FRshipPipelineDiagnostic> Diagnostics;
    const bool bRolledBack = RollbackLastPipelineApply(Diagnostics);

    TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
    Envelope->SetBoolField(TEXT("rolledBack"), bRolledBack);
    TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
    for (const FRshipPipelineDiagnostic& Diagnostic : Diagnostics)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        const FString Severity = Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Error
            ? TEXT("error")
            : (Diagnostic.Severity == ERshipPipelineDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
        Obj->SetStringField(TEXT("severity"), Severity);
        Obj->SetStringField(TEXT("code"), Diagnostic.Code);
        Obj->SetStringField(TEXT("message"), Diagnostic.Message);
        Obj->SetStringField(TEXT("nodeId"), Diagnostic.NodeId);
        Obj->SetStringField(TEXT("edgeId"), Diagnostic.EdgeId);
        DiagnosticValues.Add(MakeShared<FJsonValueObject>(Obj));
    }
    Envelope->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
    OutDiagnosticsJson = JsonToString(Envelope);
    return bRolledBack;
}

FString URshipContentMappingManager::CreateRenderContext(const FRshipRenderContextState& InState)
{
    ArmMappings();

    FRshipRenderContextState NewState = InState;
    if (NewState.Id.IsEmpty())
    {
        NewState.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    }
    NormalizeRenderContextState(NewState);

    // Guard against runaway context creation: reuse an existing context when the
    // functional capture settings already match.
    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        const FRshipRenderContextState& Existing = Pair.Value;
        if (AreRenderContextStatesFunctionallyEquivalent(Existing, NewState))
        {
            return Existing.Id;
        }
    }

    RenderContexts.Add(NewState.Id, NewState);
    RenderContextRuntimeStates.Remove(NewState.Id);
    ResolveRenderContext(RenderContexts[NewState.Id]);
    RegisterContextTarget(RenderContexts[NewState.Id]);
    EmitContextState(RenderContexts[NewState.Id]);
    MarkMappingsDirty();
    MarkCacheDirty();
    return NewState.Id;
}

bool URshipContentMappingManager::UpdateRenderContext(const FRshipRenderContextState& InState)
{
    if (InState.Id.IsEmpty() || !RenderContexts.Contains(InState.Id))
    {
        return false;
    }
    ArmMappings();

    FRshipRenderContextState Clamped = InState;
    NormalizeRenderContextState(Clamped);
    if (const FRshipRenderContextState* Existing = RenderContexts.Find(InState.Id))
    {
        if (AreRenderContextStatesEquivalent(*Existing, Clamped))
        {
            return true;
        }
    }

    FRshipRenderContextState& Stored = RenderContexts[InState.Id];
    RenderContextRuntimeStates.Remove(InState.Id);


    TWeakObjectPtr<AActor> PreviousCamera = Stored.CameraActor;
    TWeakObjectPtr<AActor> PreviousSourceCamera = Stored.SourceCameraActor;
    TWeakObjectPtr<USceneCaptureComponent2D> PreviousCapture = Stored.CaptureComponent;
    TWeakObjectPtr<UTextureRenderTarget2D> PreviousCaptureRenderTarget = Stored.CaptureRenderTarget;
    TWeakObjectPtr<USceneCaptureComponent2D> PreviousDepthCapture = Stored.DepthCaptureComponent;
    TWeakObjectPtr<UTextureRenderTarget2D> PreviousDepthRenderTarget = Stored.DepthRenderTarget;
    const FString PreviousCameraId = Stored.CameraId;
    Stored = Clamped;
    if (PreviousCamera.IsValid())
    {
        if (Stored.SourceType == TEXT("camera"))
        {
            Stored.CameraActor = PreviousCamera;
            if (PreviousSourceCamera.IsValid() && Stored.CameraId == PreviousCameraId)
            {
                Stored.SourceCameraActor = PreviousSourceCamera;
            }
            if (PreviousCapture.IsValid())
            {
                Stored.CaptureComponent = PreviousCapture;
            }
            if (PreviousCaptureRenderTarget.IsValid())
            {
                Stored.CaptureRenderTarget = PreviousCaptureRenderTarget;
            }
            if (PreviousDepthCapture.IsValid())
            {
                Stored.DepthCaptureComponent = PreviousDepthCapture;
            }
            if (PreviousDepthRenderTarget.IsValid())
            {
                Stored.DepthRenderTarget = PreviousDepthRenderTarget;
            }
        }
        else
        {
            PreviousCamera->Destroy();
        }
    }
    ResolveRenderContext(Stored);
    RegisterContextTarget(Stored);
    EmitContextState(Stored);
    MarkMappingsDirty();
    MarkCacheDirty();
    return true;
}

bool URshipContentMappingManager::DeleteRenderContext(const FString& Id)
{
    FRshipRenderContextState Removed;
    if (!RenderContexts.RemoveAndCopyValue(Id, Removed))
    {
        return false;
    }
    RenderContextRuntimeStates.Remove(Id);
    ArmMappings();

    if (Removed.CameraActor.IsValid())
    {
        Removed.CameraActor->Destroy();
    }
    DeleteTargetForPath(BuildContextTargetId(Id));
    MarkMappingsDirty();
    MarkCacheDirty();
    return true;
}

FString URshipContentMappingManager::CreateMappingSurface(const FRshipMappingSurfaceState& InState)
{
    ArmMappings();

    FRshipMappingSurfaceState NewState = InState;
    if (NewState.Id.IsEmpty())
    {
        NewState.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    }
    NormalizeMappingSurfaceState(NewState, Subsystem);
    MappingSurfaces.Add(NewState.Id, NewState);
    ResolveMappingSurface(MappingSurfaces[NewState.Id]);
    RegisterSurfaceTarget(MappingSurfaces[NewState.Id]);
    EmitSurfaceState(MappingSurfaces[NewState.Id]);
    MarkMappingsDirty();
    MarkCacheDirty();
    return NewState.Id;
}

bool URshipContentMappingManager::UpdateMappingSurface(const FRshipMappingSurfaceState& InState)
{
    if (InState.Id.IsEmpty() || !MappingSurfaces.Contains(InState.Id))
    {
        return false;
    }
    ArmMappings();

    FRshipMappingSurfaceState Clamped = InState;
    NormalizeMappingSurfaceState(Clamped, Subsystem);

    FRshipMappingSurfaceState& Stored = MappingSurfaces[InState.Id];
    if (AreMappingSurfaceStatesEquivalent(Stored, Clamped))
    {
        return true;
    }

    if (Stored.MeshComponent.IsValid())
    {
        RestoreSurfaceMaterials(Stored);
    }
    Stored = Clamped;
    ResolveMappingSurface(Stored);
    RegisterSurfaceTarget(Stored);
    EmitSurfaceState(Stored);
    MarkMappingsDirty();
    MarkCacheDirty();
    return true;
}

bool URshipContentMappingManager::DeleteMappingSurface(const FString& Id)
{
    FRshipMappingSurfaceState Removed;
    if (!MappingSurfaces.RemoveAndCopyValue(Id, Removed))
    {
        return false;
    }
    ArmMappings();

    RestoreSurfaceMaterials(Removed);
    RemoveFeedCompositeTexturesForSurface(Id);
    DeleteTargetForPath(BuildSurfaceTargetId(Id));
    MarkMappingsDirty();
    MarkCacheDirty();
    return true;
}

FString URshipContentMappingManager::CreateMapping(const FRshipContentMappingState& InState)
{
    ArmMappings();

    FRshipContentMappingState NewState = InState;
    NewState.Config = DeepCloneJsonObject(InState.Config);
    if (NewState.Id.IsEmpty())
    {
        NewState.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    }

    NormalizeMappingState(NewState);
    if (EnsureMappingRuntimeReady(NewState))
    {
        NormalizeMappingState(NewState);
    }
    Mappings.Add(NewState.Id, NewState);
    TrackPendingMappingUpsert(Mappings[NewState.Id]);
    DisableOverlappingEnabledMappings(NewState.Id);
    RegisterMappingTarget(Mappings[NewState.Id]);
    EmitMappingState(Mappings[NewState.Id]);
    MarkMappingsDirty();
    MarkCacheDirty();
    return NewState.Id;
}

bool URshipContentMappingManager::UpdateMapping(const FRshipContentMappingState& InState)
{
    if (InState.Id.IsEmpty() || !Mappings.Contains(InState.Id))
    {
        return false;
    }
    ArmMappings();

    FRshipContentMappingState Clamped = InState;
    Clamped.Config = DeepCloneJsonObject(InState.Config);
    NormalizeMappingState(Clamped);
    if (EnsureMappingRuntimeReady(Clamped))
    {
        NormalizeMappingState(Clamped);
    }
    const FRshipContentMappingState* ExistingState = Mappings.Find(InState.Id);
    if (ExistingState)
    {
        const FCanonicalMappingMode ExistingMode = GetCanonicalMappingMode(*ExistingState);
        const FCanonicalMappingMode UpdatedMode = GetCanonicalMappingMode(Clamped);
        if (!AreCanonicalMappingModesEquivalent(ExistingMode, UpdatedMode))
        {
            UE_LOG(
                LogRshipExec,
                Warning,
                TEXT("Rejecting UpdateMapping mode change for '%s' (from %s/%s to %s/%s); create a new mapping id for type changes."),
                *InState.Id,
                *ExistingMode.CanonicalType,
                *ExistingMode.CanonicalMode,
                *UpdatedMode.CanonicalType,
                *UpdatedMode.CanonicalMode);
            return false;
        }
    }
    if (!IsFeedV2Mapping(Clamped))
    {
        RemoveFeedCompositeTexturesForMapping(InState.Id);
    }
    if (ExistingState)
    {
        if (AreMappingStatesEquivalent(*ExistingState, Clamped))
        {
            return true;
        }
    }

    bool bRequiresImmediateRebuild = true;
    bool bRequiresImmediateRefresh = true;
    if (ExistingState)
    {
        bRequiresImmediateRebuild = ExistingState->bEnabled != Clamped.bEnabled
            || ExistingState->Type != Clamped.Type
            || ExistingState->ContextId != Clamped.ContextId
            || !AreStringArraysEqual(ExistingState->SurfaceIds, Clamped.SurfaceIds);
        bRequiresImmediateRefresh = bRequiresImmediateRebuild
            || !FMath::IsNearlyEqual(ExistingState->Opacity, Clamped.Opacity)
            || !AreJsonObjectsEqual(ExistingState->Config, Clamped.Config);
    }

    Mappings[InState.Id] = Clamped;
    const bool bDisabledOverlaps = DisableOverlappingEnabledMappings(InState.Id);
    TrackPendingMappingUpsert(Mappings[InState.Id]);
    RegisterMappingTarget(Mappings[InState.Id]);
    EmitMappingState(Mappings[InState.Id]);
    MarkMappingsDirty();
    MarkCacheDirty();
    if (bRequiresImmediateRefresh || bDisabledOverlaps)
    {
        SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
    }
    return true;
}

bool URshipContentMappingManager::DeleteMapping(const FString& Id)
{
    FRshipContentMappingState Removed;
    if (!Mappings.RemoveAndCopyValue(Id, Removed))
    {
        return false;
    }
    ArmMappings();
    TrackPendingMappingDelete(Id);
    RemoveFeedCompositeTexturesForMapping(Id);
    DeleteTargetForPath(BuildMappingTargetId(Id));
    MarkMappingsDirty();
    SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
    MarkCacheDirty();
    return true;
}

void URshipContentMappingManager::ProcessRenderContextEventJson(const FString& Json, bool bDelete)
{
    const TSharedPtr<FJsonObject> Data = ParseJsonObjectString(Json);
    if (!Data.IsValid())
    {
        return;
    }
    ProcessRenderContextEvent(Data, bDelete);
}

void URshipContentMappingManager::ProcessMappingSurfaceEventJson(const FString& Json, bool bDelete)
{
    const TSharedPtr<FJsonObject> Data = ParseJsonObjectString(Json);
    if (!Data.IsValid())
    {
        return;
    }
    ProcessMappingSurfaceEvent(Data, bDelete);
}

void URshipContentMappingManager::ProcessMappingEventJson(const FString& Json, bool bDelete)
{
    const TSharedPtr<FJsonObject> Data = ParseJsonObjectString(Json);
    if (!Data.IsValid())
    {
        return;
    }
    ProcessMappingEvent(Data, bDelete);
}

void URshipContentMappingManager::ProcessRenderContextEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
{
    if (!Data.IsValid())
    {
        return;
    }

    const FString Id = GetStringField(Data, TEXT("id"));
    if (Id.IsEmpty())
    {
        return;
    }

    if (bIsDelete)
    {
        FRshipRenderContextState Removed;
        if (RenderContexts.RemoveAndCopyValue(Id, Removed))
        {
            RenderContextRuntimeStates.Remove(Id);
            if (Removed.CameraActor.IsValid())
            {
                Removed.CameraActor->Destroy();
            }
            DeleteTargetForPath(BuildContextTargetId(Id));
            MarkMappingsDirty();
            MarkCacheDirty();
        }
        return;
    }

    FRshipRenderContextState State;
    State.Id = Id;
    State.Name = GetStringField(Data, TEXT("name"));
    State.ProjectId = GetStringField(Data, TEXT("projectId"));
    State.SourceType = GetStringField(Data, TEXT("sourceType"));
    State.CameraId = GetStringField(Data, TEXT("cameraId"));
    State.AssetId = GetStringField(Data, TEXT("assetId"));
    State.DepthAssetId = GetStringField(Data, TEXT("depthAssetId"));
    State.ExternalSourceId = GetStringField(Data, TEXT("externalSourceId"), GetStringField(Data, TEXT("sourceId")));
    State.Width = GetIntField(Data, TEXT("width"), 0);
    State.Height = GetIntField(Data, TEXT("height"), 0);
    State.CaptureMode = GetStringField(Data, TEXT("captureMode"));
    State.DepthCaptureMode = GetStringField(Data, TEXT("depthCaptureMode"));
    State.bEnabled = GetBoolField(Data, TEXT("enabled"), true);
    State.bDepthCaptureEnabled = GetBoolField(Data, TEXT("depthCaptureEnabled"), false);
    NormalizeRenderContextState(State);

    if (const FRshipRenderContextState* Existing = RenderContexts.Find(Id))
    {
        if (AreRenderContextStatesEquivalent(*Existing, State))
        {
            return;
        }
    }

    FRshipRenderContextState& Stored = RenderContexts.FindOrAdd(Id);
    RenderContextRuntimeStates.Remove(Id);
    TWeakObjectPtr<AActor> PreviousCamera = Stored.CameraActor;
    TWeakObjectPtr<AActor> PreviousSourceCamera = Stored.SourceCameraActor;
    TWeakObjectPtr<USceneCaptureComponent2D> PreviousCapture = Stored.CaptureComponent;
    TWeakObjectPtr<UTextureRenderTarget2D> PreviousCaptureRenderTarget = Stored.CaptureRenderTarget;
    TWeakObjectPtr<USceneCaptureComponent2D> PreviousDepthCapture = Stored.DepthCaptureComponent;
    TWeakObjectPtr<UTextureRenderTarget2D> PreviousDepthRenderTarget = Stored.DepthRenderTarget;
    const FString PreviousCameraId = Stored.CameraId;
    Stored = State;
    if (PreviousCamera.IsValid())
    {
        if (Stored.SourceType == TEXT("camera"))
        {
            Stored.CameraActor = PreviousCamera;
            if (PreviousSourceCamera.IsValid() && Stored.CameraId == PreviousCameraId)
            {
                Stored.SourceCameraActor = PreviousSourceCamera;
            }
            if (PreviousCapture.IsValid())
            {
                Stored.CaptureComponent = PreviousCapture;
            }
            if (PreviousCaptureRenderTarget.IsValid())
            {
                Stored.CaptureRenderTarget = PreviousCaptureRenderTarget;
            }
            if (PreviousDepthCapture.IsValid())
            {
                Stored.DepthCaptureComponent = PreviousDepthCapture;
            }
            if (PreviousDepthRenderTarget.IsValid())
            {
                Stored.DepthRenderTarget = PreviousDepthRenderTarget;
            }
        }
        else
        {
            PreviousCamera->Destroy();
        }
    }

    ResolveRenderContext(Stored);
    RegisterContextTarget(Stored);
    EmitContextState(Stored);
    MarkMappingsDirty();
    MarkCacheDirty();
}

void URshipContentMappingManager::ProcessMappingSurfaceEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
{
    if (!Data.IsValid())
    {
        return;
    }

    const FString Id = GetStringField(Data, TEXT("id"));
    if (Id.IsEmpty())
    {
        return;
    }

    if (bIsDelete)
    {
        FRshipMappingSurfaceState Removed;
        if (MappingSurfaces.RemoveAndCopyValue(Id, Removed))
        {
            RestoreSurfaceMaterials(Removed);
            RemoveFeedCompositeTexturesForSurface(Id);
            DeleteTargetForPath(BuildSurfaceTargetId(Id));
            MarkMappingsDirty();
            MarkCacheDirty();
        }
        return;
    }

    FRshipMappingSurfaceState State;
    State.Id = Id;
    State.Name = GetStringField(Data, TEXT("name"));
    State.ProjectId = GetStringField(Data, TEXT("projectId"));
    State.TargetId.Reset();
    State.bEnabled = GetBoolField(Data, TEXT("enabled"), true);
    State.UVChannel = GetIntField(Data, TEXT("uvChannel"), 0);
    State.MaterialSlots = GetIntArrayField(Data, TEXT("materialSlots"));
    State.MeshComponentName = GetStringField(Data, TEXT("meshComponentName"));
    State.ActorPath = GetStringField(Data, TEXT("actorPath"));
    if (State.ActorPath.IsEmpty())
    {
        const FString LegacyTargetId = GetStringField(Data, TEXT("targetId"));
        if (!LegacyTargetId.IsEmpty())
        {
            const FString LegacyToken = GetShortIdToken(LegacyTargetId);
            if (!LegacyToken.IsEmpty())
            {
                if (AActor* LegacyActor = FindActorByNameToken(LegacyToken, true))
                {
                    State.ActorPath = LegacyActor->GetPathName();
                }
            }
        }
    }
    NormalizeMappingSurfaceState(State, Subsystem);

    if (const FRshipMappingSurfaceState* Existing = MappingSurfaces.Find(Id))
    {
        if (AreMappingSurfaceStatesEquivalent(*Existing, State))
        {
            return;
        }
    }

    FRshipMappingSurfaceState& Stored = MappingSurfaces.FindOrAdd(Id);
    if (Stored.MeshComponent.IsValid())
    {
        RestoreSurfaceMaterials(Stored);
    }
    Stored = State;

    ResolveMappingSurface(Stored);
    RegisterSurfaceTarget(Stored);
    EmitSurfaceState(Stored);
    MarkMappingsDirty();
    MarkCacheDirty();
}

void URshipContentMappingManager::ProcessMappingEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
{
    if (!Data.IsValid())
    {
        return;
    }

    const FString Id = GetStringField(Data, TEXT("id"));
    if (Id.IsEmpty())
    {
        return;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    PrunePendingMappingGuards(NowSeconds);

    if (bIsDelete)
    {
        if (const double* UpsertExpiry = PendingMappingUpsertExpiry.Find(Id))
        {
            if (NowSeconds <= *UpsertExpiry)
            {
                UE_LOG(LogRshipExec, Verbose, TEXT("Ignoring stale delete for mapping %s (local upsert pending)"), *Id);
                return;
            }
            PendingMappingUpsertExpiry.Remove(Id);
            PendingMappingUpserts.Remove(Id);
        }

        // Keep a delete tombstone long enough to reject out-of-order stale upserts.
        // This is required even when the local map entry is already gone.
        const double ExistingDeleteExpiry = PendingMappingDeletes.FindRef(Id);
        const double NewDeleteExpiry = FMath::Max(ExistingDeleteExpiry, NowSeconds + 15.0);
        PendingMappingDeletes.Add(Id, NewDeleteExpiry);

        if (!Mappings.Contains(Id))
        {
            return;
        }

        FRshipContentMappingState Removed;
        if (Mappings.RemoveAndCopyValue(Id, Removed))
        {
            RemoveFeedCompositeTexturesForMapping(Id);
            DeleteTargetForPath(BuildMappingTargetId(Id));
            MarkMappingsDirty();
            SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
            MarkCacheDirty();
        }
        return;
    }

    const FString RawType = GetStringField(Data, TEXT("type"));
    FString MappingType = RawType;
    FString DerivedMode;
    if (RawType.Equals(TEXT("direct"), ESearchCase::IgnoreCase))
    {
        MappingType = TEXT("surface-uv");
        DerivedMode = TEXT("direct");
    }
    else if (RawType.Equals(TEXT("feed"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("surface-feed"), ESearchCase::IgnoreCase))
    {
        MappingType = TEXT("surface-uv");
        DerivedMode = TEXT("feed");
    }
    else if (RawType.Equals(TEXT("perspective"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("cylindrical"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("spherical"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("parallel"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("radial"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("mesh"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("fisheye"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("custom-matrix"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("custom matrix"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("matrix"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("camera-plate"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("camera plate"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("cameraplate"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("spatial"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("depth-map"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("depth map"), ESearchCase::IgnoreCase)
        || RawType.Equals(TEXT("depthmap"), ESearchCase::IgnoreCase))
    {
        MappingType = TEXT("surface-projection");
        if (RawType.Equals(TEXT("camera plate"), ESearchCase::IgnoreCase) || RawType.Equals(TEXT("cameraplate"), ESearchCase::IgnoreCase))
        {
            DerivedMode = TEXT("camera-plate");
        }
        else if (RawType.Equals(TEXT("custom-matrix"), ESearchCase::IgnoreCase)
            || RawType.Equals(TEXT("custom matrix"), ESearchCase::IgnoreCase)
            || RawType.Equals(TEXT("matrix"), ESearchCase::IgnoreCase))
        {
            DerivedMode = TEXT("custom-matrix");
        }
        else if (RawType.Equals(TEXT("depth map"), ESearchCase::IgnoreCase) || RawType.Equals(TEXT("depthmap"), ESearchCase::IgnoreCase))
        {
            DerivedMode = TEXT("depth-map");
        }
        else
        {
            DerivedMode = RawType.ToLower();
        }
    }
    if (MappingType != TEXT("surface-uv") && MappingType != TEXT("surface-projection"))
    {
        MappingType = RawType;
    }

    FRshipContentMappingState State;
    State.Id = Id;
    State.Name = GetStringField(Data, TEXT("name"));
    State.ProjectId = GetStringField(Data, TEXT("projectId"));
    State.Type = MappingType;
    State.ContextId = GetStringField(Data, TEXT("contextId"));
    State.SurfaceIds = GetStringArrayField(Data, TEXT("surfaceIds"));
    State.Opacity = FMath::Clamp(GetNumberField(Data, TEXT("opacity"), 1.0f), 0.0f, 1.0f);
    State.bEnabled = GetBoolField(Data, TEXT("enabled"), true);

    if (Data->HasTypedField<EJson::Object>(TEXT("config")))
    {
        State.Config = DeepCloneJsonObject(Data->GetObjectField(TEXT("config")));
    }

    if (!DerivedMode.IsEmpty())
    {
        if (!State.Config.IsValid())
        {
            State.Config = MakeShared<FJsonObject>();
        }
        if (MappingType == TEXT("surface-uv"))
        {
            // If type explicitly says feed/direct, prefer it over stale config values.
            State.Config->SetStringField(TEXT("uvMode"), DerivedMode);
        }
        if (MappingType == TEXT("surface-projection"))
        {
            State.Config->SetStringField(TEXT("projectionType"), DerivedMode);
        }
    }

    NormalizeMappingState(State);
    if (EnsureMappingRuntimeReady(State))
    {
        NormalizeMappingState(State);
    }

    if (const double* DeleteExpiry = PendingMappingDeletes.Find(Id))
    {
        if (NowSeconds <= *DeleteExpiry)
        {
            UE_LOG(LogRshipExec, Verbose, TEXT("Ignoring stale upsert for deleted mapping %s"), *Id);
            return;
        }
        PendingMappingDeletes.Remove(Id);
    }

    if (const double* UpsertExpiry = PendingMappingUpsertExpiry.Find(Id))
    {
        if (NowSeconds <= *UpsertExpiry)
        {
            if (const FRshipContentMappingState* PendingState = PendingMappingUpserts.Find(Id))
            {
                if (!AreMappingStatesEquivalent(*PendingState, State))
                {
                    UE_LOG(LogRshipExec, Verbose, TEXT("Ignoring stale mapping state for %s while local update is pending"), *Id);
                    return;
                }
            }
            PendingMappingUpsertExpiry.Remove(Id);
            PendingMappingUpserts.Remove(Id);
        }
        else
        {
            PendingMappingUpsertExpiry.Remove(Id);
            PendingMappingUpserts.Remove(Id);
        }
    }

    if (const FRshipContentMappingState* Existing = Mappings.Find(Id))
    {
        const FCanonicalMappingMode ExistingMode = GetCanonicalMappingMode(*Existing);
        const FCanonicalMappingMode IncomingMode = GetCanonicalMappingMode(State);
        if (!AreCanonicalMappingModesEquivalent(ExistingMode, IncomingMode))
        {
            UE_LOG(
                LogRshipExec,
                Warning,
                TEXT("Ignoring mapping upsert mode change for '%s' (from %s/%s to %s/%s); mode is immutable per mapping id."),
                *Id,
                *ExistingMode.CanonicalType,
                *ExistingMode.CanonicalMode,
                *IncomingMode.CanonicalType,
                *IncomingMode.CanonicalMode);
            return;
        }

        if (AreMappingStatesEquivalent(*Existing, State))
        {
            return;
        }
    }

    if (!IsFeedV2Mapping(State))
    {
        RemoveFeedCompositeTexturesForMapping(Id);
    }

    FRshipContentMappingState& Stored = Mappings.FindOrAdd(Id);
    Stored = State;
    DisableOverlappingEnabledMappings(Id);

    RegisterMappingTarget(Stored);
    EmitMappingState(Stored);
    MarkMappingsDirty();
    MarkCacheDirty();
}

void URshipContentMappingManager::TrackPendingMappingUpsert(const FRshipContentMappingState& State)
{
    // Backend echoes can lag several seconds; keep a longer guard so stale state
    // does not immediately revert local mapping edits.
    const double ExpiresAt = FPlatformTime::Seconds() + 15.0;
    FRshipContentMappingState ClonedState = State;
    ClonedState.Config = DeepCloneJsonObject(State.Config);
    PendingMappingDeletes.Remove(State.Id);
    PendingMappingUpserts.Add(State.Id, MoveTemp(ClonedState));
    PendingMappingUpsertExpiry.Add(State.Id, ExpiresAt);
}

bool URshipContentMappingManager::DisableOverlappingEnabledMappings(const FString& PreferredMappingId)
{
    FRshipContentMappingState* Preferred = Mappings.Find(PreferredMappingId);
    if (!Preferred || !Preferred->bEnabled)
    {
        return false;
    }

    const TArray<FString>& PreferredSurfaceIds = GetEffectiveSurfaceIds(*Preferred);
    if (PreferredSurfaceIds.Num() == 0)
    {
        return false;
    }

    TSet<FString> PreferredSurfaceSet;
    for (const FString& SurfaceId : PreferredSurfaceIds)
    {
        const FString SanitizedSurfaceId = SurfaceId.TrimStartAndEnd();
        if (!SanitizedSurfaceId.IsEmpty())
        {
            PreferredSurfaceSet.Add(SanitizedSurfaceId);
        }
    }
    if (PreferredSurfaceSet.Num() == 0)
    {
        return false;
    }

    bool bDisabledAny = false;
    for (auto& Pair : Mappings)
    {
        FRshipContentMappingState& Candidate = Pair.Value;
        if (Candidate.Id == PreferredMappingId || !Candidate.bEnabled)
        {
            continue;
        }

        const TArray<FString>& CandidateSurfaceIds = GetEffectiveSurfaceIds(Candidate);
        bool bOverlapsSurface = false;
        for (const FString& CandidateSurfaceId : CandidateSurfaceIds)
        {
            if (PreferredSurfaceSet.Contains(CandidateSurfaceId.TrimStartAndEnd()))
            {
                bOverlapsSurface = true;
                break;
            }
        }
        if (!bOverlapsSurface)
        {
            continue;
        }

        Candidate.bEnabled = false;
        Candidate.LastError = FString::Printf(
            TEXT("Disabled due overlap with active mapping '%s'."),
            *PreferredMappingId);
        TrackPendingMappingUpsert(Candidate);

        RegisterMappingTarget(Candidate);
        EmitMappingState(Candidate);
        bDisabledAny = true;

        UE_LOG(LogRshipExec, Warning, TEXT("Disabled overlapping mapping '%s' (preferred='%s')"),
            *Candidate.Id, *PreferredMappingId);
    }

    if (bDisabledAny)
    {
        MarkMappingsDirty();
        MarkCacheDirty();
    }

    return bDisabledAny;
}

void URshipContentMappingManager::TrackPendingMappingDelete(const FString& MappingId)
{
    const double ExpiresAt = FPlatformTime::Seconds() + 15.0;
    PendingMappingUpserts.Remove(MappingId);
    PendingMappingUpsertExpiry.Remove(MappingId);
    PendingMappingDeletes.Add(MappingId, ExpiresAt);
}

void URshipContentMappingManager::PrunePendingMappingGuards(double NowSeconds)
{
    for (auto It = PendingMappingUpsertExpiry.CreateIterator(); It; ++It)
    {
        if (NowSeconds > It.Value())
        {
            PendingMappingUpserts.Remove(It.Key());
            It.RemoveCurrent();
        }
    }

    for (auto It = PendingMappingDeletes.CreateIterator(); It; ++It)
    {
        if (NowSeconds > It.Value())
        {
            It.RemoveCurrent();
        }
    }
}

bool URshipContentMappingManager::RouteAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data)
{
    if (TargetId.StartsWith(TEXT("/content-mapping/context/")))
    {
        FString ContextId = TargetId.Mid(25);
        return HandleContextAction(ContextId, GetActionName(ActionId), Data);
    }
    if (TargetId.StartsWith(TEXT("/content-mapping/surface/")))
    {
        FString SurfaceId = TargetId.Mid(25);
        return HandleSurfaceAction(SurfaceId, GetActionName(ActionId), Data);
    }
    if (TargetId.StartsWith(TEXT("/content-mapping/mapping/")))
    {
        FString MappingId = TargetId.Mid(25);
        return HandleMappingAction(MappingId, GetActionName(ActionId), Data);
    }

    return false;
}

bool URshipContentMappingManager::RouteActionJson(const FString& TargetId, const FString& ActionId, const FString& DataJson)
{
    TSharedPtr<FJsonObject> Parsed = ParseJsonObjectString(DataJson);
    if (!Parsed.IsValid())
    {
        Parsed = MakeShared<FJsonObject>();
    }
    return RouteAction(TargetId, ActionId, Parsed.ToSharedRef());
}

void URshipContentMappingManager::MarkMappingsDirty()
{
    bMappingsDirty = true;
    bRuntimePreparePending = true;
    EffectiveSurfaceIdsCache.Empty();
    RequiredContextIdsCache.Empty();
    CachedEnabledTextureContextId.Reset();
    CachedAnyTextureContextId.Reset();
    CachedEnabledContextId.Reset();
    CachedAnyContextId.Reset();
}

void URshipContentMappingManager::SyncRuntimeAfterMutation(bool bRequireRebuild)
{
    if (!bMappingsArmed)
    {
        return;
    }

    if (bRequireRebuild)
    {
        bNeedsWorldResolutionRetry = false;
        RebuildMappings();
        bMappingsDirty = bNeedsWorldResolutionRetry;
    }

    RefreshLiveMappings();
}

void URshipContentMappingManager::ArmMappings()
{
    if (!bMappingsArmed)
    {
        bMappingsArmed = true;
        bMappingsDirty = true;
        bRuntimePreparePending = true;
    }
}

void URshipContentMappingManager::MarkCacheDirty()
{
    bCacheDirty = true;
    CacheSaveDueTimeSeconds = FPlatformTime::Seconds() + 0.2;
}

bool URshipContentMappingManager::HasAnyEnabledMappings() const
{
    for (const TPair<FString, FRshipContentMappingState>& Pair : Mappings)
    {
        if (Pair.Value.bEnabled)
        {
            return true;
        }
    }
    return false;
}

bool URshipContentMappingManager::HasAnyMappingsRequiringContinuousRefresh() const
{
    auto IsContextDynamic = [this](const FString& InContextId) -> bool
    {
        const FString ContextId = InContextId.TrimStartAndEnd();
        if (ContextId.IsEmpty())
        {
            return true;
        }

        const FRshipRenderContextState* ContextState = RenderContexts.Find(ContextId);
        if (!ContextState)
        {
            return true;
        }

        return !ContextState->SourceType.Equals(TEXT("asset-store"), ESearchCase::IgnoreCase);
    };

    for (const TPair<FString, FRshipContentMappingState>& Pair : Mappings)
    {
        const FRshipContentMappingState& MappingState = Pair.Value;
        if (!MappingState.bEnabled)
        {
            continue;
        }

        if (!IsFeedV2Mapping(MappingState))
        {
            if (IsContextDynamic(MappingState.ContextId))
            {
                return true;
            }
            continue;
        }

        if (!MappingState.Config.IsValid()
            || !MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
        {
            return true;
        }

        const TSharedPtr<FJsonObject> FeedV2 = MappingState.Config->GetObjectField(TEXT("feedV2"));
        if (!FeedV2.IsValid() || !FeedV2->HasTypedField<EJson::Array>(TEXT("sources")))
        {
            return true;
        }

        bool bFoundAnySource = false;
        const TArray<TSharedPtr<FJsonValue>>& Sources = FeedV2->GetArrayField(TEXT("sources"));
        for (const TSharedPtr<FJsonValue>& SourceValue : Sources)
        {
            if (!SourceValue.IsValid() || SourceValue->Type != EJson::Object)
            {
                continue;
            }

            const TSharedPtr<FJsonObject> SourceObj = SourceValue->AsObject();
            if (!SourceObj.IsValid())
            {
                continue;
            }

            bFoundAnySource = true;
            if (IsContextDynamic(GetStringField(SourceObj, TEXT("contextId"))))
            {
                return true;
            }
        }

        if (!bFoundAnySource && IsContextDynamic(MappingState.ContextId))
        {
            return true;
        }
    }

    return false;
}

bool URshipContentMappingManager::HasPendingRuntimeBindings()
{
    if (IsRuntimeBlocked())
    {
        return false;
    }

    UWorld* PreferredWorld = GetBestWorld();

    for (auto& Pair : Mappings)
    {
        FRshipContentMappingState& MappingState = Pair.Value;
        if (!MappingState.bEnabled)
        {
            continue;
        }

        const bool bFeedV2 = IsFeedV2Mapping(MappingState);
        const FRshipRenderContextState* ContextState = ResolveEffectiveContextState(MappingState, bFeedV2);
        if ((CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0)
            && bFeedV2 && (!ContextState || !ContextState->ResolvedTexture))
        {
            ContextState = ResolveEffectiveContextState(MappingState, false);
        }

        if (!bFeedV2 && (!ContextState || !ContextState->ResolvedTexture))
        {
            return true;
        }

        const TArray<FString>& EffectiveSurfaceIds = GetEffectiveSurfaceIds(MappingState);
        for (const FString& SurfaceId : EffectiveSurfaceIds)
        {
            FRshipMappingSurfaceState* SurfaceState = MappingSurfaces.Find(SurfaceId);
            if (!SurfaceState || !SurfaceState->bEnabled)
            {
                continue;
            }

            UMeshComponent* Mesh = SurfaceState->MeshComponent.Get();
            if (!IsMeshReadyForMaterialMutation(Mesh))
            {
                return true;
            }
            if (PreferredWorld && Mesh && Mesh->GetWorld() != PreferredWorld)
            {
                return true;
            }

            const FString LastError = SurfaceState->LastError.TrimStartAndEnd();
            if (LastError.Contains(TEXT("World not available"), ESearchCase::IgnoreCase)
                || LastError.Contains(TEXT("Mesh component not resolved"), ESearchCase::IgnoreCase)
                || LastError.Contains(TEXT("No mesh component found"), ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
    }

    return false;
}

bool URshipContentMappingManager::TryAutoBootstrapDefaults()
{
    if (bAutoBootstrapComplete)
    {
        return false;
    }

    if (CVarRshipContentMappingAutoBootstrap.GetValueOnGameThread() <= 0)
    {
        bAutoBootstrapComplete = true;
        return false;
    }

    if (RenderContexts.Num() > 0 || MappingSurfaces.Num() > 0 || Mappings.Num() > 0)
    {
        bAutoBootstrapComplete = true;
        return false;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    if (NextAutoBootstrapAttemptSeconds > 0.0 && NowSeconds < NextAutoBootstrapAttemptSeconds)
    {
        return false;
    }

    UWorld* World = GetBestWorld();
    if (!World)
    {
        NextAutoBootstrapAttemptSeconds = NowSeconds + 1.0;
        return false;
    }

    const int32 MaxSurfaces = FMath::Clamp(
        CVarRshipContentMappingAutoBootstrapMaxSurfaces.GetValueOnGameThread(),
        1,
        64);

    TArray<AActor*> CandidateActors;
    TSet<const AActor*> SeenActors;
    auto AddCandidateIfValid = [&](AActor* Actor, bool bRequireNameHint) -> void
    {
        if (!Actor || SeenActors.Contains(Actor) || !IsLikelyScreenActor(Actor))
        {
            return;
        }

        if (bRequireNameHint)
        {
            const FString ActorName = Actor->GetName().ToLower();
            const FString ActorLabel = GetActorLabelCompat(Actor).ToLower();
            const bool bHasHint =
                ActorName.Contains(TEXT("screen")) || ActorName.Contains(TEXT("display"))
                || ActorName.Contains(TEXT("plane")) || ActorName.Contains(TEXT("panel"))
                || ActorName.Contains(TEXT("wall"))
                || ActorLabel.Contains(TEXT("screen")) || ActorLabel.Contains(TEXT("display"))
                || ActorLabel.Contains(TEXT("plane")) || ActorLabel.Contains(TEXT("panel"))
                || ActorLabel.Contains(TEXT("wall"));
            if (!bHasHint)
            {
                return;
            }
        }

        TArray<UMeshComponent*> MeshComponents;
        Actor->GetComponents(MeshComponents);
        if (MeshComponents.Num() == 0)
        {
            return;
        }

        CandidateActors.Add(Actor);
        SeenActors.Add(Actor);
    };

    for (TActorIterator<AActor> It(World); It && CandidateActors.Num() < MaxSurfaces; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || !Actor->FindComponentByClass<URshipActorRegistrationComponent>())
        {
            continue;
        }
        AddCandidateIfValid(Actor, false);
    }

    if (CandidateActors.Num() == 0)
    {
        for (TActorIterator<AActor> It(World); It && CandidateActors.Num() < MaxSurfaces; ++It)
        {
            AddCandidateIfValid(*It, true);
        }
    }

    if (CandidateActors.Num() == 0)
    {
        NextAutoBootstrapAttemptSeconds = NowSeconds + 2.0;
        return false;
    }

    FRshipRenderContextState ContextState;
    ContextState.Name = TEXT("Autobootstrap Context");
    ContextState.SourceType = TEXT("camera");
    ContextState.CameraId = TEXT("AUTO");
    ContextState.CaptureMode = TEXT("FinalColorLDR");
    ContextState.Width = 1920;
    ContextState.Height = 1080;
    ContextState.bEnabled = true;
    const FString ContextId = CreateRenderContext(ContextState);
    if (ContextId.IsEmpty())
    {
        NextAutoBootstrapAttemptSeconds = NowSeconds + 2.0;
        return false;
    }

    TArray<FString> SurfaceIds;
    SurfaceIds.Reserve(CandidateActors.Num());

    for (AActor* Actor : CandidateActors)
    {
        if (!Actor)
        {
            continue;
        }

        TArray<UMeshComponent*> MeshComponents;
        Actor->GetComponents(MeshComponents);
        UMeshComponent* Mesh = nullptr;
        for (UMeshComponent* CandidateMesh : MeshComponents)
        {
            if (CandidateMesh && IsValid(CandidateMesh))
            {
                Mesh = CandidateMesh;
                break;
            }
        }
        if (!Mesh)
        {
            continue;
        }

        FRshipMappingSurfaceState SurfaceState;
        SurfaceState.Name = GetActorLabelCompat(Actor);
        if (SurfaceState.Name.IsEmpty())
        {
            SurfaceState.Name = Actor->GetName();
        }
        SurfaceState.ActorPath = Actor->GetPathName();
        SurfaceState.MeshComponentName = Mesh->GetName();
        SurfaceState.UVChannel = 0;
        SurfaceState.bEnabled = true;

        const FString SurfaceId = CreateMappingSurface(SurfaceState);
        if (!SurfaceId.IsEmpty())
        {
            SurfaceIds.Add(SurfaceId);
        }
    }

    if (SurfaceIds.Num() == 0)
    {
        DeleteRenderContext(ContextId);
        NextAutoBootstrapAttemptSeconds = NowSeconds + 2.0;
        return false;
    }

    FRshipContentMappingState MappingState;
    MappingState.Name = TEXT("Autobootstrap Mapping");
    MappingState.Type = TEXT("direct");
    MappingState.ContextId = ContextId;
    MappingState.SurfaceIds = SurfaceIds;
    MappingState.Opacity = 1.0f;
    MappingState.bEnabled = true;
    MappingState.Config = MakeShared<FJsonObject>();
    MappingState.Config->SetStringField(TEXT("uvMode"), TEXT("direct"));
    MappingState.Config->SetObjectField(TEXT("uvTransform"), MakeShared<FJsonObject>());

    const FString MappingId = CreateMapping(MappingState);
    if (MappingId.IsEmpty())
    {
        NextAutoBootstrapAttemptSeconds = NowSeconds + 2.0;
        return false;
    }

    bAutoBootstrapComplete = true;
    NextAutoBootstrapAttemptSeconds = 0.0;

    UE_LOG(LogRshipExec, Warning,
        TEXT("ContentMapping autobootstrap created context=%s mapping=%s surfaces=%d"),
        *ContextId,
        *MappingId,
        SurfaceIds.Num());

    return true;
}

void URshipContentMappingManager::RefreshResolvedContextFallbackIds()
{
    CachedEnabledTextureContextId.Reset();
    CachedAnyTextureContextId.Reset();
    CachedEnabledContextId.Reset();
    CachedAnyContextId.Reset();

    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        if (CachedAnyContextId.IsEmpty())
        {
            CachedAnyContextId = Pair.Key;
        }
        if (Pair.Value.bEnabled && CachedEnabledContextId.IsEmpty())
        {
            CachedEnabledContextId = Pair.Key;
        }
        if (Pair.Value.ResolvedTexture)
        {
            if (CachedAnyTextureContextId.IsEmpty())
            {
                CachedAnyTextureContextId = Pair.Key;
            }
            if (Pair.Value.bEnabled && CachedEnabledTextureContextId.IsEmpty())
            {
                CachedEnabledTextureContextId = Pair.Key;
            }
        }

        if (!CachedEnabledTextureContextId.IsEmpty()
            && !CachedAnyTextureContextId.IsEmpty()
            && !CachedEnabledContextId.IsEmpty()
            && !CachedAnyContextId.IsEmpty())
        {
            break;
        }
    }
}

const TArray<FString>& URshipContentMappingManager::GetEffectiveSurfaceIds(FRshipContentMappingState& MappingState)
{
    if (TArray<FString>* Cached = EffectiveSurfaceIdsCache.Find(MappingState.Id))
    {
        return *Cached;
    }

    TArray<FString> Computed = GatherEffectiveSurfaceIdsForMapping(MappingState);
    return EffectiveSurfaceIdsCache.Add(MappingState.Id, MoveTemp(Computed));
}

UWorld* URshipContentMappingManager::GetBestWorld() const
{
    if (!GEngine)
    {
        return nullptr;
    }

    if (UWorld* PreferredViewportWorld = GetPreferredContentMappingViewportWorld())
    {
        LastValidWorld = PreferredViewportWorld;
        return PreferredViewportWorld;
    }

    const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();
    int32 BestScore = MIN_int32;
    UWorld* BestWorld = nullptr;

    auto ScoreContextWorld = [](const FWorldContext& Context, UWorld* World) -> int32
    {
        int32 Score = 0;
        const bool bIsPlay = IsPlayContentMappingWorldType(Context.WorldType);
        const bool bIsEditor = IsEditorContentMappingWorldType(Context.WorldType);

        if (bIsPlay)
        {
            Score += 3000;
            if (Context.GameViewport)
            {
                Score += 1200;
            }

            APlayerController* PrimaryController = World ? World->GetFirstPlayerController() : nullptr;
            if (PrimaryController)
            {
                Score += PrimaryController->IsLocalController() ? 900 : 300;
            }

            if (World)
            {
                switch (World->GetNetMode())
                {
                    case NM_DedicatedServer:
                        Score -= 4000;
                        break;
                    case NM_Client:
                        Score += 800;
                        break;
                    case NM_Standalone:
                        Score += 600;
                        break;
                    default:
                        break;
                }
            }
        }
        else if (bIsEditor)
        {
            Score += 2000;
        }
        else
        {
            Score += 500;
        }

        return Score;
    };

    for (int32 Pass = 0; Pass < 3; ++Pass)
    {
        for (const FWorldContext& Context : Contexts)
        {
            UWorld* World = Context.World();
            if (!World)
            {
                continue;
            }

            const bool bIsPlay = IsPlayContentMappingWorldType(Context.WorldType);
            const bool bIsEditor = IsEditorContentMappingWorldType(Context.WorldType);
            if (Pass == 0 && !bIsPlay)
            {
                continue;
            }
            if (Pass == 1 && !bIsEditor)
            {
                continue;
            }
            if (Pass == 2 && (bIsPlay || bIsEditor))
            {
                continue;
            }

            const int32 Score = ScoreContextWorld(Context, World);
            if (Score > BestScore)
            {
                BestScore = Score;
                BestWorld = World;
            }
        }
    }

    if (BestWorld)
    {
        LastValidWorld = BestWorld;
        return BestWorld;
    }

    if (LastValidWorld.IsValid())
    {
        UWorld* CachedWorld = LastValidWorld.Get();
        if (CachedWorld && !CachedWorld->bIsTearingDown)
        {
            return CachedWorld;
        }
    }

    if (Subsystem)
    {
        if (UWorld* SubsystemWorld = Subsystem->GetWorld())
        {
            LastValidWorld = SubsystemWorld;
            return SubsystemWorld;
        }
    }

    return nullptr;
}

void URshipContentMappingManager::ResolveRenderContext(FRshipRenderContextState& ContextState)
{
    ContextState.LastError.Empty();
    ContextState.ResolvedTexture = nullptr;
    ContextState.ResolvedDepthTexture = nullptr;
    NormalizeRenderContextState(ContextState);

    auto DisableCaptureComponent = [](USceneCaptureComponent2D* CaptureComponent)
    {
        if (!CaptureComponent)
        {
            return;
        }

        CaptureComponent->bCaptureEveryFrame = false;
        CaptureComponent->bCaptureOnMovement = false;
    };

    auto EnsureCaptureRoot = [](AActor* Actor) -> USceneComponent*
    {
        if (!Actor)
        {
            return nullptr;
        }

        if (USceneComponent* ExistingRoot = Actor->GetRootComponent())
        {
            return ExistingRoot;
        }

        USceneComponent* RootComponent = NewObject<USceneComponent>(Actor, TEXT("RshipContentMappingRoot"), RF_Transient);
        if (!RootComponent)
        {
            return nullptr;
        }

        Actor->AddInstanceComponent(RootComponent);
        RootComponent->OnComponentCreated();
        Actor->SetRootComponent(RootComponent);
        RootComponent->RegisterComponent();
        return RootComponent;
    };

    if (!ContextState.bEnabled)
    {
        DisableCaptureComponent(ContextState.CaptureComponent.Get());
        DisableCaptureComponent(ContextState.DepthCaptureComponent.Get());
        if (FRenderContextRuntimeState* RuntimeState = RenderContextRuntimeStates.Find(ContextState.Id))
        {
            RuntimeState->bHasIssuedExplicitCapture = false;
            RuntimeState->NextExplicitCaptureTimeSeconds = 0.0;
        }
        return;
    }

    const bool bAllowSemanticFallbacks = CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0;

    if (ContextState.SourceType.Equals(TEXT("camera"), ESearchCase::IgnoreCase))
    {
        const bool bUsesAutoCameraId = ContextState.CameraId.IsEmpty()
            || ContextState.CameraId.Equals(TEXT("AUTO"), ESearchCase::IgnoreCase);
        if (bUsesAutoCameraId)
        {
            if (!bAllowSemanticFallbacks && ContextState.CameraId.IsEmpty())
            {
                ContextState.LastError = TEXT("CameraId not set");
                return;
            }

            AActor* FallbackSourceActor = FindAnySourceCameraActor();
            if (!FallbackSourceActor)
            {
                FallbackSourceActor = FindAnySourceCameraAnchorActor();
            }

            if (FallbackSourceActor)
            {
                const FString ResolvedCameraId = BuildActorTargetId(Subsystem, FallbackSourceActor);

                if (!ResolvedCameraId.IsEmpty())
                {
                    ContextState.CameraId = ResolvedCameraId;
                    ContextState.SourceCameraActor = FallbackSourceActor;
                    MarkCacheDirty();
                    UE_LOG(LogRshipExec, Log, TEXT("ResolveRenderContext[%s]: Auto-selected camera '%s' -> id '%s'"),
                        *ContextState.Id,
                        *FallbackSourceActor->GetName(),
                        *ResolvedCameraId);
                }
            }

            if (ContextState.CameraId.IsEmpty() || ContextState.CameraId.Equals(TEXT("AUTO"), ESearchCase::IgnoreCase))
            {
                ContextState.LastError = TEXT("CameraId not set");
                return;
            }
        }

        UWorld* PreferredWorld = GetBestWorld();
        const bool bRequirePlayWorldBinding = PreferredWorld
            && IsPlayContentMappingWorldType(PreferredWorld->WorldType);

        AActor* SourceCamera = ContextState.SourceCameraActor.Get();
        if (!SourceCamera || !IsValid(SourceCamera) || !IsCameraSourceActor(SourceCamera))
        {
            SourceCamera = FindSourceCameraActorByEntityId(Subsystem, ContextState.CameraId);
            ContextState.SourceCameraActor = SourceCamera;
        }
        if (SourceCamera && PreferredWorld && SourceCamera->GetWorld() != PreferredWorld)
        {
            SourceCamera = FindSourceCameraActorByEntityId(Subsystem, ContextState.CameraId);
            ContextState.SourceCameraActor = SourceCamera;
        }

        // Auto-repair stale camera bindings by rebinding to an available camera actor.
        if (bAllowSemanticFallbacks
            && (!SourceCamera || !IsValid(SourceCamera))
            && !ContextState.CameraId.IsEmpty())
        {
            AActor* FallbackSourceActor = FindAnySourceCameraActor();
            if (!FallbackSourceActor)
            {
                FallbackSourceActor = FindAnySourceCameraAnchorActor();
            }

            if (FallbackSourceActor)
            {
                const FString ReboundCameraId = BuildActorTargetId(Subsystem, FallbackSourceActor);

                if (!ReboundCameraId.IsEmpty())
                {
                    const FString PreviousCameraId = ContextState.CameraId;
                    ContextState.CameraId = ReboundCameraId;
                    ContextState.SourceCameraActor = FallbackSourceActor;
                    SourceCamera = FallbackSourceActor;
                    MarkCacheDirty();
                    bRuntimePreparePending = true;

                    UE_LOG(LogRshipExec, Warning,
                        TEXT("ResolveRenderContext[%s]: rebound unresolved CameraId '%s' to camera '%s' (id '%s')"),
                        *ContextState.Id,
                        *PreviousCameraId,
                        *FallbackSourceActor->GetName(),
                        *ReboundCameraId);
                }
            }
        }

        AActor* SourceAnchorActor = SourceCamera;
        if ((!SourceAnchorActor || !IsValid(SourceAnchorActor)) && !ContextState.CameraId.IsEmpty())
        {
            SourceAnchorActor = FindSourceAnchorActorByEntityId(Subsystem, ContextState.CameraId);
        }
        if (SourceAnchorActor && PreferredWorld && SourceAnchorActor->GetWorld() != PreferredWorld)
        {
            SourceAnchorActor = FindSourceAnchorActorByEntityId(Subsystem, ContextState.CameraId);
        }

        UWorld* World = nullptr;
        if (SourceAnchorActor
            && (!bRequirePlayWorldBinding || !PreferredWorld || SourceAnchorActor->GetWorld() == PreferredWorld))
        {
            World = SourceAnchorActor->GetWorld();
        }
        if (!World)
        {
            World = PreferredWorld;
        }
        if (AActor* ExistingCamera = ContextState.CameraActor.Get())
        {
            if (!World)
            {
                World = ExistingCamera->GetWorld();
            }
        }
        if (!World)
        {
            // Runtime refresh retries context resolution continuously; avoid forcing
            // full mapping rebuild loops when world selection is temporarily unavailable.
            return;
        }

        FRenderContextRuntimeState& RuntimeState = RenderContextRuntimeStates.FindOrAdd(ContextState.Id);

        // Avoid spawning helper capture actors when the source camera/anchor is unresolved.
        bool bHasPlayerViewFallback = false;
        if (bAllowSemanticFallbacks && !SourceCamera && !SourceAnchorActor && World)
        {
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                if (It->Get())
                {
                    bHasPlayerViewFallback = true;
                    break;
                }
            }
        }
        bool bHasEditorViewFallback = false;
        if (bAllowSemanticFallbacks && !SourceCamera && !SourceAnchorActor && World)
        {
            FTransform EditorViewTransform = FTransform::Identity;
            float EditorViewFov = 60.0f;
            bHasEditorViewFallback = TryGetEditorViewportFallback(World, EditorViewTransform, EditorViewFov);
        }

        if (!SourceCamera && !SourceAnchorActor && !bHasPlayerViewFallback && !bHasEditorViewFallback)
        {
            ContextState.LastError = FString::Printf(TEXT("No source actor resolved for CameraId '%s'"), *ContextState.CameraId);
            const double NowSeconds = FPlatformTime::Seconds();
            if (NowSeconds >= RuntimeState.NextMissingSourceWarningTimeSeconds)
            {
                UE_LOG(LogRshipExec, Warning, TEXT("ResolveRenderContext[%s]: no source actor resolved for CameraId '%s'"),
                    *ContextState.Id, *ContextState.CameraId);
                RuntimeState.NextMissingSourceWarningTimeSeconds = NowSeconds + 1.0;
            }

            DisableCaptureComponent(ContextState.CaptureComponent.Get());
            ContextState.SourceCameraActor.Reset();
            DisableCaptureComponent(ContextState.DepthCaptureComponent.Get());
            RuntimeState.bHasAppliedTransform = false;
            RuntimeState.LastAppliedFov = -1.0f;
            RuntimeState.bHasIssuedExplicitCapture = false;
            RuntimeState.NextExplicitCaptureTimeSeconds = 0.0;
            return;
        }
        RuntimeState.NextMissingSourceWarningTimeSeconds = 0.0;

        AActor* CameraActor = ContextState.CameraActor.Get();
        if (CameraActor && CameraActor->GetWorld() != World)
        {
            CameraActor->Destroy();
            ContextState.CameraActor.Reset();
            ContextState.CaptureComponent.Reset();
            ContextState.CaptureRenderTarget.Reset();
            ContextState.DepthCaptureComponent.Reset();
            ContextState.DepthRenderTarget.Reset();
            RuntimeState.SetupHash = 0;
            RuntimeState.bHasAppliedTransform = false;
            RuntimeState.LastAppliedTransform = FTransform::Identity;
            RuntimeState.LastAppliedFov = -1.0f;
            RuntimeState.NextResolveRetryTimeSeconds = 0.0;
            RuntimeState.NextMissingSourceWarningTimeSeconds = 0.0;
            RuntimeState.bHasIssuedExplicitCapture = false;
            RuntimeState.NextExplicitCaptureTimeSeconds = 0.0;
            CameraActor = nullptr;
        }
        if (!CameraActor)
        {
            const FString DesiredActorName = FString::Printf(TEXT("RshipContentMappingCapture_%s"), *ContextState.Id);

            // Reuse an existing helper actor if one already exists for this context.
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor* Candidate = *It;
                if (!Candidate)
                {
                    continue;
                }
                const FString CandidateName = Candidate->GetName();
                if (CandidateName.Equals(DesiredActorName, ESearchCase::CaseSensitive)
                    || CandidateName.StartsWith(DesiredActorName + TEXT("_"), ESearchCase::CaseSensitive))
                {
                    CameraActor = Candidate;
                    break;
                }
            }
        }

        if (!CameraActor)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = FName(*FString::Printf(TEXT("RshipContentMappingCapture_%s"), *ContextState.Id));
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.ObjectFlags |= RF_Transient;
#if WITH_EDITOR
            SpawnParams.bTemporaryEditorActor = true;
#endif
            const FTransform SpawnTransform = FTransform::Identity;
            CameraActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, SpawnParams);
        }

        if (!CameraActor)
        {
            ContextState.LastError = TEXT("Failed to spawn camera actor");
            return;
        }

        CameraActor->SetActorTickEnabled(false);
        CameraActor->SetActorHiddenInGame(false);
        USceneComponent* CaptureRoot = EnsureCaptureRoot(CameraActor);
        if (!CaptureRoot)
        {
            ContextState.LastError = TEXT("Camera capture root missing");
            return;
        }

        USceneCaptureComponent2D* CaptureComponent = ContextState.CaptureComponent.Get();
        if (CaptureComponent && CaptureComponent->GetOwner() != CameraActor)
        {
            CaptureComponent->DestroyComponent();
            ContextState.CaptureComponent.Reset();
            CaptureComponent = nullptr;
        }
        if (!CaptureComponent)
        {
            CaptureComponent = NewObject<USceneCaptureComponent2D>(CameraActor, TEXT("RshipContentMappingCapture"), RF_Transient);
            if (CaptureComponent)
            {
                CaptureComponent->SetupAttachment(CaptureRoot);
                CaptureComponent->RegisterComponent();
                ContextState.CaptureComponent = CaptureComponent;
            }
        }
        if (!CaptureComponent)
        {
            ContextState.LastError = TEXT("Camera capture component missing");
            return;
        }

        UTextureRenderTarget2D* CaptureRenderTarget = ContextState.CaptureRenderTarget.Get();
        if (CaptureRenderTarget && CaptureRenderTarget->GetOuter() != CameraActor)
        {
            ContextState.CaptureRenderTarget.Reset();
            CaptureRenderTarget = nullptr;
        }

        const ERshipCaptureQualityProfile CaptureQualityProfile = GetCaptureQualityProfile();
        const bool bWorldSupportsMainViewCapture = World
            && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game);
        const bool bPreferPlayerViewInPlay = bAllowSemanticFallbacks
            && bWorldSupportsMainViewCapture
            && (CVarRshipContentMappingPreferPlayerViewInPlay.GetValueOnGameThread() > 0);
        const bool bUseMainViewCapture = (CVarRshipContentMappingCaptureUseMainView.GetValueOnGameThread() > 0)
            && bWorldSupportsMainViewCapture;
        const bool bUseResolvedMainViewCapture = bUseMainViewCapture;
        const bool bUseMainViewCamera = bUseMainViewCapture && (CVarRshipContentMappingCaptureUseMainViewCamera.GetValueOnGameThread() > 0);
        const int32 RequestedMainViewDivisor = CVarRshipContentMappingCaptureMainViewDivisor.GetValueOnGameThread();
        const int32 MainViewDivisor = GetEffectiveCaptureDivisor(CaptureQualityProfile, RequestedMainViewDivisor);
        const float RequestedCaptureLodFactor = CVarRshipContentMappingCaptureLodFactor.GetValueOnGameThread();
        const float CaptureLodFactor = GetEffectiveCaptureLodFactor(CaptureQualityProfile, RequestedCaptureLodFactor);
        const float CaptureMaxViewDistance = FMath::Max(0.0f, CVarRshipContentMappingCaptureMaxViewDistance.GetValueOnGameThread());
        const bool bEnableExplicitCaptureRefresh = CVarRshipContentMappingCaptureExplicitRefresh.GetValueOnGameThread() > 0;
        const double ExplicitCaptureIntervalSeconds = FMath::Max(
            0.0,
            static_cast<double>(CVarRshipContentMappingCaptureExplicitRefreshInterval.GetValueOnGameThread()));
        const ESceneCaptureSource CaptureSource = (ContextState.CaptureMode == TEXT("SceneColorHDR")
            || ContextState.CaptureMode == TEXT("RawSceneColor"))
            ? ESceneCaptureSource::SCS_SceneColorHDR
            : ESceneCaptureSource::SCS_FinalColorLDR;
        uint32 ContextSetupHash = 0;
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(ContextState.CaptureMode));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(ContextState.DepthCaptureMode));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(ContextState.bDepthCaptureEnabled));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(ContextState.Width));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(ContextState.Height));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(bUseResolvedMainViewCapture));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(bUseMainViewCamera));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(MainViewDivisor));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(CaptureLodFactor));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(CaptureMaxViewDistance));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(static_cast<uint8>(CaptureQualityProfile)));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(static_cast<int32>(CaptureSource)));
        const bool bNeedsCaptureSetup = (RuntimeState.SetupHash != ContextSetupHash);

        CaptureComponent->SetVisibility(true, true);
        CaptureComponent->SetHiddenInGame(false);
        bool bNeedsExplicitCapture = false;
        if (!CaptureComponent->bCaptureEveryFrame)
        {
            CaptureComponent->bCaptureEveryFrame = true;
            bNeedsExplicitCapture = true;
        }
        if (CaptureComponent->bCaptureOnMovement)
        {
            CaptureComponent->bCaptureOnMovement = false;
        }
        if (!CaptureComponent->bAlwaysPersistRenderingState)
        {
            CaptureComponent->bAlwaysPersistRenderingState = true;
        }

        if (bNeedsCaptureSetup)
        {
            CaptureComponent->SetRelativeLocation(FVector::ZeroVector);
            CaptureComponent->SetRelativeRotation(FRotator::ZeroRotator);
            CaptureComponent->bMainViewFamily = bUseResolvedMainViewCapture;
            CaptureComponent->bMainViewResolution = bUseResolvedMainViewCapture;
            CaptureComponent->bMainViewCamera = bUseMainViewCamera;
            CaptureComponent->bInheritMainViewCameraPostProcessSettings = bUseMainViewCamera;
            CaptureComponent->bIgnoreScreenPercentage = false;
            CaptureComponent->MainViewResolutionDivisor = FIntPoint(MainViewDivisor, MainViewDivisor);
            CaptureComponent->bRenderInMainRenderer = bUseResolvedMainViewCapture;
            CaptureComponent->LODDistanceFactor = CaptureLodFactor;
            CaptureComponent->MaxViewDistanceOverride = CaptureMaxViewDistance;
            CaptureComponent->CaptureSource = CaptureSource;
            ApplyCaptureQualityProfile(CaptureComponent, CaptureQualityProfile, false);
            RuntimeState.SetupHash = ContextSetupHash;
            bNeedsExplicitCapture = true;
        }

        if (bNeedsExplicitCapture)
        {
            RuntimeState.bHasIssuedExplicitCapture = false;
            RuntimeState.NextExplicitCaptureTimeSeconds = 0.0;
        }

        if (CaptureRenderTarget)
        {
            int32 Width = ContextState.Width > 0 ? ContextState.Width : CaptureRenderTarget->SizeX;
            int32 Height = ContextState.Height > 0 ? ContextState.Height : CaptureRenderTarget->SizeY;
            if (Width <= 0)
            {
                Width = 1920;
            }
            if (Height <= 0)
            {
                Height = 1080;
            }

            if (CaptureRenderTarget->SizeX != Width || CaptureRenderTarget->SizeY != Height)
            {
                CaptureRenderTarget->InitAutoFormat(Width, Height);
                CaptureRenderTarget->UpdateResourceImmediate();
                RuntimeState.SetupHash = 0;
            }

            if (ContextState.Width <= 0 || ContextState.Height <= 0)
            {
                ContextState.Width = Width;
                ContextState.Height = Height;
                MarkCacheDirty();
                bRuntimePreparePending = true;
            }
        }
        else
        {
            CaptureRenderTarget = NewObject<UTextureRenderTarget2D>(CameraActor, TEXT("RshipContentMappingRenderTarget"), RF_Transient);
            const int32 Width = ContextState.Width > 0 ? ContextState.Width : 1920;
            const int32 Height = ContextState.Height > 0 ? ContextState.Height : 1080;
            CaptureRenderTarget->InitAutoFormat(Width, Height);
            CaptureRenderTarget->UpdateResourceImmediate();
            ContextState.CaptureRenderTarget = CaptureRenderTarget;
            RuntimeState.SetupHash = 0;
            if (ContextState.Width <= 0 || ContextState.Height <= 0)
            {
                ContextState.Width = Width;
                ContextState.Height = Height;
                MarkCacheDirty();
                bRuntimePreparePending = true;
            }
        }

        if (CaptureRenderTarget)
        {
            ContextState.CaptureRenderTarget = CaptureRenderTarget;
            FTransform DesiredTransform = CameraActor->GetActorTransform();
            float DesiredFov = CaptureComponent->FOVAngle;
            bool bHasDesiredTransform = false;
            bool bHasDesiredFov = false;
            bool bNeedsTransformCapture = false;

            if (!bHasDesiredTransform && bPreferPlayerViewInPlay && World)
            {
                for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                {
                    APlayerController* PC = It->Get();
                    if (!PC)
                    {
                        continue;
                    }

                    FVector ViewLocation = FVector::ZeroVector;
                    FRotator ViewRotation = FRotator::ZeroRotator;
                    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
                    DesiredTransform = FTransform(ViewRotation, ViewLocation);
                    bHasDesiredTransform = true;

                    if (PC->PlayerCameraManager)
                    {
                        DesiredFov = PC->PlayerCameraManager->GetFOVAngle();
                        bHasDesiredFov = true;
                    }
                    break;
                }
            }

            if (!bHasDesiredTransform && SourceCamera)
            {
                if (UCameraComponent* SourceCameraComponent = ResolveSourceCameraComponent(SourceCamera))
                {
                    DesiredTransform = FTransform(SourceCameraComponent->GetComponentRotation(), SourceCameraComponent->GetComponentLocation());
                    DesiredFov = SourceCameraComponent->FieldOfView;
                    bHasDesiredTransform = true;
                    bHasDesiredFov = true;
                }
                else
                {
                    DesiredTransform = SourceCamera->GetActorTransform();
                    bHasDesiredTransform = true;
                }
            }
            else if (!bHasDesiredTransform && SourceAnchorActor)
            {
                if (UCameraComponent* AnchorCameraComponent = ResolveSourceCameraComponent(SourceAnchorActor))
                {
                    DesiredTransform = FTransform(AnchorCameraComponent->GetComponentRotation(), AnchorCameraComponent->GetComponentLocation());
                    DesiredFov = AnchorCameraComponent->FieldOfView;
                    bHasDesiredTransform = true;
                    bHasDesiredFov = true;
                }
                else
                {
                    DesiredTransform = SourceAnchorActor->GetActorTransform();
                    bHasDesiredTransform = true;
                }
            }
            if (!bHasDesiredTransform)
            {
                bool bAppliedPlayerViewFallback = false;
                const bool bAllowPlayerViewFallback = bAllowSemanticFallbacks;
                if (bAllowPlayerViewFallback && World)
                {
                    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                    {
                        APlayerController* PC = It->Get();
                        if (!PC)
                        {
                            continue;
                        }

                        FVector ViewLocation = FVector::ZeroVector;
                        FRotator ViewRotation = FRotator::ZeroRotator;
                        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
                        DesiredTransform = FTransform(ViewRotation, ViewLocation);
                        bHasDesiredTransform = true;

                        if (PC->PlayerCameraManager)
                        {
                            DesiredFov = PC->PlayerCameraManager->GetFOVAngle();
                            bHasDesiredFov = true;
                        }

                        bAppliedPlayerViewFallback = true;
                        break;
                    }
                }
                bool bAppliedEditorViewFallback = false;
                if (bAllowSemanticFallbacks && !bAppliedPlayerViewFallback)
                {
                    FTransform EditorViewTransform = FTransform::Identity;
                    float EditorViewFov = 60.0f;
                    if (TryGetEditorViewportFallback(World, EditorViewTransform, EditorViewFov))
                    {
                        DesiredTransform = EditorViewTransform;
                        DesiredFov = EditorViewFov;
                        bHasDesiredTransform = true;
                        bHasDesiredFov = true;
                        bAppliedEditorViewFallback = true;
                    }
                }

                if (bAppliedPlayerViewFallback)
                {
                    UE_LOG(LogRshipExec, Verbose, TEXT("ResolveRenderContext[%s]: using player camera fallback for CameraId '%s'"),
                        *ContextState.Id, *ContextState.CameraId);
                }
                else if (bAppliedEditorViewFallback)
                {
                    UE_LOG(LogRshipExec, Verbose, TEXT("ResolveRenderContext[%s]: using editor viewport fallback for CameraId '%s'"),
                        *ContextState.Id, *ContextState.CameraId);
                }
                else
                {
                    ContextState.LastError = FString::Printf(TEXT("No source actor resolved for CameraId '%s'"), *ContextState.CameraId);
                    const double NowSeconds = FPlatformTime::Seconds();
                    if (NowSeconds >= RuntimeState.NextMissingSourceWarningTimeSeconds)
                    {
                        UE_LOG(LogRshipExec, Warning, TEXT("ResolveRenderContext[%s]: no source actor resolved for CameraId '%s'"),
                            *ContextState.Id, *ContextState.CameraId);
                        RuntimeState.NextMissingSourceWarningTimeSeconds = NowSeconds + 1.0;
                    }
                }
            }

            if (!bHasDesiredTransform)
            {
                DisableCaptureComponent(CaptureComponent);
                RuntimeState.bHasAppliedTransform = false;
                RuntimeState.LastAppliedFov = -1.0f;
                RuntimeState.bHasIssuedExplicitCapture = false;
                RuntimeState.NextExplicitCaptureTimeSeconds = 0.0;
                return;
            }
            RuntimeState.NextMissingSourceWarningTimeSeconds = 0.0;

            if (bHasDesiredTransform)
            {
                if (!RuntimeState.bHasAppliedTransform
                    || !RuntimeState.LastAppliedTransform.Equals(DesiredTransform, 0.01f))
                {
                    CameraActor->SetActorTransform(DesiredTransform);
                    RuntimeState.LastAppliedTransform = DesiredTransform;
                    RuntimeState.bHasAppliedTransform = true;
                    bNeedsTransformCapture = true;
                }
            }

            if (bHasDesiredFov && !FMath::IsNearlyEqual(RuntimeState.LastAppliedFov, DesiredFov, 0.01f))
            {
                CaptureComponent->FOVAngle = DesiredFov;
                RuntimeState.LastAppliedFov = DesiredFov;
                bNeedsTransformCapture = true;
            }

            if (CaptureComponent->TextureTarget != CaptureRenderTarget)
            {
                CaptureComponent->TextureTarget = CaptureRenderTarget;
                bNeedsTransformCapture = true;
            }

            const double NowSeconds = FPlatformTime::Seconds();
            const bool bPeriodicExplicitCapture = bEnableExplicitCaptureRefresh
                && ExplicitCaptureIntervalSeconds > 0.0
                && NowSeconds >= RuntimeState.NextExplicitCaptureTimeSeconds;
            const bool bShouldCaptureNow = bEnableExplicitCaptureRefresh
                && (bNeedsTransformCapture
                || !RuntimeState.bHasIssuedExplicitCapture
                || bPeriodicExplicitCapture);
            if (bShouldCaptureNow)
            {
                const bool bWasCaptureEveryFrame = CaptureComponent->bCaptureEveryFrame;
                if (bWasCaptureEveryFrame)
                {
                    CaptureComponent->bCaptureEveryFrame = false;
                }
                CaptureComponent->CaptureScene();
                if (bWasCaptureEveryFrame)
                {
                    CaptureComponent->bCaptureEveryFrame = true;
                }
                RuntimeState.bHasIssuedExplicitCapture = true;
                RuntimeState.NextExplicitCaptureTimeSeconds = bEnableExplicitCaptureRefresh
                    ? (NowSeconds + ExplicitCaptureIntervalSeconds)
                    : 0.0;
            }
        }

        if (ContextState.bDepthCaptureEnabled)
        {
            UTextureRenderTarget2D* DepthTarget = ContextState.DepthRenderTarget.Get();
            if (DepthTarget && DepthTarget->GetOuter() != CameraActor)
            {
                ContextState.DepthRenderTarget.Reset();
                DepthTarget = nullptr;
            }
            if (!DepthTarget)
            {
                DepthTarget = NewObject<UTextureRenderTarget2D>(CameraActor, TEXT("RshipContentMappingDepthRenderTarget"), RF_Transient);
                if (DepthTarget)
                {
                    DepthTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_R16f;
                    DepthTarget->ClearColor = FLinearColor::Black;
                    ContextState.DepthRenderTarget = DepthTarget;
                }
            }

            if (DepthTarget)
            {
                const int32 DepthWidth = ContextState.Width > 0 ? ContextState.Width : 1920;
                const int32 DepthHeight = ContextState.Height > 0 ? ContextState.Height : 1080;
                if (DepthTarget->SizeX != DepthWidth || DepthTarget->SizeY != DepthHeight)
                {
                    DepthTarget->InitAutoFormat(DepthWidth, DepthHeight);
                    DepthTarget->UpdateResourceImmediate();
                }
            }

            USceneCaptureComponent2D* DepthCapture = ContextState.DepthCaptureComponent.Get();
            if (DepthCapture && DepthCapture->GetOwner() != CameraActor)
            {
                DepthCapture->DestroyComponent();
                ContextState.DepthCaptureComponent.Reset();
                DepthCapture = nullptr;
            }
            if (!DepthCapture)
            {
                DepthCapture = NewObject<USceneCaptureComponent2D>(CameraActor, TEXT("RshipContentMappingDepthCapture"), RF_Transient);
                if (DepthCapture)
                {
                    DepthCapture->SetupAttachment(CaptureRoot);
                    DepthCapture->RegisterComponent();
                    ContextState.DepthCaptureComponent = DepthCapture;
                }
            }

            if (DepthCapture)
            {
                DepthCapture->SetVisibility(true, true);
                DepthCapture->SetHiddenInGame(false);
                bool bNeedsExplicitDepthCapture = false;
                if (!DepthCapture->bCaptureEveryFrame)
                {
                    DepthCapture->bCaptureEveryFrame = true;
                    bNeedsExplicitDepthCapture = true;
                }
                if (DepthCapture->bCaptureOnMovement)
                {
                    DepthCapture->bCaptureOnMovement = false;
                }
                if (!DepthCapture->bAlwaysPersistRenderingState)
                {
                    DepthCapture->bAlwaysPersistRenderingState = true;
                }

                if (bNeedsCaptureSetup)
                {
                    DepthCapture->CaptureSource = ContextState.DepthCaptureMode.Equals(TEXT("DeviceDepth"), ESearchCase::IgnoreCase)
                        ? ESceneCaptureSource::SCS_DeviceDepth
                        : ESceneCaptureSource::SCS_SceneDepth;
                    DepthCapture->SetRelativeLocation(FVector::ZeroVector);
                    DepthCapture->SetRelativeRotation(FRotator::ZeroRotator);
                    DepthCapture->bMainViewFamily = bUseResolvedMainViewCapture;
                    DepthCapture->bMainViewResolution = bUseResolvedMainViewCapture;
                    DepthCapture->bMainViewCamera = false;
                    DepthCapture->bInheritMainViewCameraPostProcessSettings = false;
                    DepthCapture->bIgnoreScreenPercentage = false;
                    DepthCapture->MainViewResolutionDivisor = FIntPoint(MainViewDivisor, MainViewDivisor);
                    DepthCapture->bRenderInMainRenderer = bUseResolvedMainViewCapture;
                    DepthCapture->LODDistanceFactor = CaptureLodFactor;
                    DepthCapture->MaxViewDistanceOverride = CaptureMaxViewDistance;
                    ApplyCaptureQualityProfile(DepthCapture, CaptureQualityProfile, true);
                    bNeedsExplicitDepthCapture = true;
                }

                DepthCapture->TextureTarget = ContextState.DepthRenderTarget.Get();
                if (CaptureComponent)
                {
                    DepthCapture->FOVAngle = CaptureComponent->FOVAngle;
                    bNeedsExplicitDepthCapture = true;
                }

                if (bNeedsExplicitDepthCapture && !DepthCapture->bCaptureEveryFrame)
                {
                    DepthCapture->CaptureScene();
                }
            }

            ContextState.ResolvedDepthTexture = ContextState.DepthRenderTarget.Get();
        }
        else if (USceneCaptureComponent2D* DepthCapture = ContextState.DepthCaptureComponent.Get())
        {
            DisableCaptureComponent(DepthCapture);
        }

        ContextState.CameraActor = CameraActor;
        ContextState.CaptureComponent = CaptureComponent;
        ContextState.CaptureRenderTarget = CaptureRenderTarget;
        ContextState.ResolvedTexture = CaptureRenderTarget;
        if (CaptureRenderTarget)
        {
            UE_LOG(LogRshipExec, VeryVerbose, TEXT("ResolveRenderContext[%s]: texture ready %dx%d"),
                *ContextState.Id,
                CaptureRenderTarget->SizeX,
                CaptureRenderTarget->SizeY);
        }
        return;
    }

    if (ContextState.SourceType.Equals(TEXT("asset-store"), ESearchCase::IgnoreCase))
    {
        if (ContextState.AssetId.IsEmpty())
        {
            ContextState.LastError = TEXT("AssetId not set");
            return;
        }

        auto ResolveAssetTexture = [this](const FString& AssetId, UTexture*& OutTexture) -> bool
        {
            OutTexture = nullptr;
            if (AssetId.IsEmpty())
            {
                return true;
            }

            if (TWeakObjectPtr<UTexture2D>* Cached = AssetTextureCache.Find(AssetId))
            {
                if (Cached->IsValid())
                {
                    OutTexture = Cached->Get();
                    return true;
                }
            }

            const FString CachedPath = GetAssetCachePathForId(AssetId);
            if (IFileManager::Get().FileExists(*CachedPath))
            {
                if (UTexture2D* CachedTexture = LoadTextureFromFile(CachedPath))
                {
                    AssetTextureCache.Add(AssetId, CachedTexture);
                    OutTexture = CachedTexture;
                    return true;
                }
            }

            RequestAssetDownload(AssetId);
            return false;
        };

        if (!ResolveAssetTexture(ContextState.AssetId, ContextState.ResolvedTexture))
        {
            ContextState.LastError = TEXT("Asset downloading");
        }

        if (!ContextState.DepthAssetId.IsEmpty())
        {
            UTexture* DepthTexture = nullptr;
            if (!ResolveAssetTexture(ContextState.DepthAssetId, DepthTexture))
            {
                if (ContextState.LastError.IsEmpty())
                {
                    ContextState.LastError = TEXT("Depth asset downloading");
                }
            }
            ContextState.ResolvedDepthTexture = DepthTexture;
        }

        if (ContextState.ResolvedTexture)
        {
            return;
        }
        return;
    }

    if (ContextState.SourceType.Equals(TEXT("external-texture"), ESearchCase::IgnoreCase))
    {
        if (ContextState.ExternalSourceId.IsEmpty())
        {
            ContextState.LastError = TEXT("ExternalSourceId not set");
            return;
        }

        const FExternalTextureSourceState* ExternalSource = ExternalTextureSources.Find(ContextState.ExternalSourceId);
        if (!ExternalSource || !ExternalSource->Texture.IsValid())
        {
            ContextState.LastError = FString::Printf(
                TEXT("External texture source '%s' not registered"),
                *ContextState.ExternalSourceId);
            return;
        }

        UTexture* ExternalTexture = ExternalSource->Texture.Get();
        if (!IsValid(ExternalTexture))
        {
            ContextState.LastError = FString::Printf(
                TEXT("External texture source '%s' texture is invalid"),
                *ContextState.ExternalSourceId);
            return;
        }

        ContextState.ResolvedTexture = ExternalTexture;

        const int32 SourceWidth = ExternalSource->Width > 0
            ? ExternalSource->Width
            : FMath::Max(0, ExternalTexture->GetSurfaceWidth());
        const int32 SourceHeight = ExternalSource->Height > 0
            ? ExternalSource->Height
            : FMath::Max(0, ExternalTexture->GetSurfaceHeight());

        if (SourceWidth > 0)
        {
            ContextState.Width = SourceWidth;
        }
        if (SourceHeight > 0)
        {
            ContextState.Height = SourceHeight;
        }
        return;
    }

    ContextState.LastError = TEXT("Unsupported sourceType");
}

void URshipContentMappingManager::ResolveMappingSurface(FRshipMappingSurfaceState& SurfaceState)
{
    SurfaceState.LastError.Empty();
    SurfaceState.MeshComponent.Reset();

    if (!GEngine)
    {
        SurfaceState.LastError = TEXT("Engine not ready");
        return;
    }

    const FString SurfaceName = SurfaceState.Name.TrimStartAndEnd();
    const FString RequestedMeshName = SurfaceState.MeshComponentName.TrimStartAndEnd();
    const FString RequestedActorPath = SurfaceState.ActorPath.TrimStartAndEnd();
    const bool bHasActorPath = !RequestedActorPath.IsEmpty();
    const FString RequestedActorObjectName = RequestedActorPath.IsEmpty()
        ? FString()
        : RequestedActorPath.Mid(RequestedActorPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1);
    UWorld* PreferredWorld = GetBestWorld();
    const bool bRequirePlayWorldBinding = PreferredWorld
        && IsPlayContentMappingWorldType(PreferredWorld->WorldType);
    const bool bAllowSemanticFallbacks = CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0;

    int32 BestScore = -1;
    UMeshComponent* BestMesh = nullptr;
    AActor* BestOwner = nullptr;
    bool bSawRelevantWorld = false;
    bool bRequireActorPathMatch = bHasActorPath;

    auto ScoreMeshCandidate = [&](AActor* Owner, UMeshComponent* Mesh) -> int32
    {
        if (!Owner || !Mesh || !IsValid(Mesh))
        {
            return -1;
        }

        if (bRequirePlayWorldBinding && PreferredWorld && Owner->GetWorld() != PreferredWorld)
        {
            return -1;
        }

        int32 Score = 1;
        const FString MeshName = Mesh->GetName();
        const FString ActorName = Owner->GetName();
        const FString ActorLabel = GetActorLabelCompat(Owner);

        if (PreferredWorld && Owner->GetWorld() == PreferredWorld)
        {
            Score += 4000;
        }

        Score += ScoreTokenMatch(MeshName, RequestedMeshName, 3000, 600);
        Score += ScoreTokenMatch(ActorName, RequestedMeshName, 2400, 420);
        Score += ScoreTokenMatch(ActorLabel, RequestedMeshName, 2400, 420);

        Score += ScoreTokenMatch(MeshName, SurfaceName, 1400, 260);
        Score += ScoreTokenMatch(ActorName, SurfaceName, 1200, 220);
        Score += ScoreTokenMatch(ActorLabel, SurfaceName, 1200, 220);

        if (Mesh->GetNumMaterials() > 0)
        {
            Score += 5;
        }

        if (Cast<UStaticMeshComponent>(Mesh))
        {
            Score += 5;
        }

        return Score;
    };

    auto SelectMeshOnActor = [&](AActor* Owner) -> UMeshComponent*
    {
        if (!Owner || !IsLikelyScreenActor(Owner))
        {
            return nullptr;
        }

        TArray<UMeshComponent*> MeshComponents;
        Owner->GetComponents(MeshComponents);
        if (MeshComponents.Num() == 0)
        {
            return nullptr;
        }

        if (!RequestedMeshName.IsEmpty())
        {
            for (UMeshComponent* Mesh : MeshComponents)
            {
                if (Mesh && Mesh->GetName().Equals(RequestedMeshName, ESearchCase::IgnoreCase))
                {
                    return Mesh;
                }
            }
        }

        for (UMeshComponent* Mesh : MeshComponents)
        {
            if (Mesh)
            {
                return Mesh;
            }
        }

        return nullptr;
    };

    if (bHasActorPath)
    {
        AActor* ExplicitOwner = nullptr;
        if (AActor* FoundByPath = FindObject<AActor>(nullptr, *RequestedActorPath))
        {
            ExplicitOwner = FoundByPath;
        }

        if (PreferredWorld && ExplicitOwner && ExplicitOwner->GetWorld() != PreferredWorld)
        {
            // Saved actorPath usually points at editor world objects; prefer active PIE/Game world.
            ExplicitOwner = nullptr;
        }

        if (!ExplicitOwner && GEngine)
        {
            auto TryResolveInWorld = [&](UWorld* World, bool bAllowNameFallback) -> AActor*
            {
                if (!World)
                {
                    return nullptr;
                }
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Candidate = *It;
                    if (!Candidate)
                    {
                        continue;
                    }
                    if (Candidate->GetPathName().Equals(RequestedActorPath, ESearchCase::CaseSensitive))
                    {
                        return Candidate;
                    }
                    if (bAllowNameFallback
                        && !RequestedActorObjectName.IsEmpty()
                        && Candidate->GetName().Equals(RequestedActorObjectName, ESearchCase::CaseSensitive))
                    {
                        return Candidate;
                    }
                }
                return nullptr;
            };

            if (PreferredWorld)
            {
                // Deterministic editor->PIE remap: exact object name match in the active world.
                ExplicitOwner = TryResolveInWorld(PreferredWorld, true);
            }

            if (!ExplicitOwner && !bRequirePlayWorldBinding)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    UWorld* World = Context.World();
                    if (!World || !IsRelevantContentMappingWorldType(Context.WorldType))
                    {
                        continue;
                    }
                    if (World == PreferredWorld)
                    {
                        continue;
                    }
                    ExplicitOwner = TryResolveInWorld(World, true);
                    if (ExplicitOwner)
                    {
                        break;
                    }
                }
            }
        }

        if (ExplicitOwner && IsValid(ExplicitOwner))
        {
            if (UMeshComponent* ExplicitMesh = SelectMeshOnActor(ExplicitOwner))
            {
                BestOwner = ExplicitOwner;
                BestMesh = ExplicitMesh;
                BestScore = 100000;
            }
            else
            {
                if (!bAllowSemanticFallbacks)
                {
                    SurfaceState.LastError = FString::Printf(TEXT("actorPath '%s' resolved actor has no mesh"), *RequestedActorPath);
                    return;
                }

                bRequireActorPathMatch = false;
                UE_LOG(LogRshipExec, Warning, TEXT("ResolveMappingSurface[%s]: actorPath '%s' has no mesh, using fallback search"),
                    *SurfaceState.Id,
                    *RequestedActorPath);
            }
        }
        else
        {
            if (!bAllowSemanticFallbacks)
            {
                SurfaceState.LastError = FString::Printf(TEXT("actorPath not found '%s'"), *RequestedActorPath);
                return;
            }

            bRequireActorPathMatch = false;
            UE_LOG(LogRshipExec, Warning, TEXT("ResolveMappingSurface[%s]: actorPath not found '%s', using fallback search"),
                *SurfaceState.Id,
                *RequestedActorPath);
        }
    }

    if ((!BestMesh || !BestOwner) && !bRequireActorPathMatch)
    {
        auto ScanWorldForBest = [&](UWorld* World)
        {
            if (!World)
            {
                return;
            }

            bSawRelevantWorld = true;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor* Actor = *It;
                if (!IsLikelyScreenActor(Actor))
                {
                    continue;
                }

                TArray<UMeshComponent*> MeshComponents;
                Actor->GetComponents(MeshComponents);
                for (UMeshComponent* Mesh : MeshComponents)
                {
                    const int32 Score = ScoreMeshCandidate(Actor, Mesh);
                    if (Score > BestScore)
                    {
                        BestScore = Score;
                        BestOwner = Actor;
                        BestMesh = Mesh;
                    }
                }
            }
        };

        if (PreferredWorld)
        {
            ScanWorldForBest(PreferredWorld);
        }

        if (!bRequirePlayWorldBinding)
        {
            for (int32 Pass = 0; Pass < 3; ++Pass)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    UWorld* World = Context.World();
                    if (!World || !IsRelevantContentMappingWorldType(Context.WorldType) || World == PreferredWorld)
                    {
                        continue;
                    }

                    const bool bIsPlay = IsPlayContentMappingWorldType(Context.WorldType);
                    const bool bIsEditor = IsEditorContentMappingWorldType(Context.WorldType);
                    if (Pass == 0 && !bIsPlay)
                    {
                        continue;
                    }
                    if (Pass == 1 && !bIsEditor)
                    {
                        continue;
                    }
                    if (Pass == 2 && (bIsPlay || bIsEditor))
                    {
                        continue;
                    }

                    ScanWorldForBest(World);
                }
            }
        }
    }

    if (!BestMesh || !BestOwner)
    {
        // Surface resolution is retried in the live refresh path; do not trigger
        // full rebuild retries every tick for unresolved/stale surfaces.
        SurfaceState.LastError = bSawRelevantWorld ? TEXT("No mesh component found") : TEXT("World not available");
        UE_LOG(LogRshipExec, Warning, TEXT("ResolveMappingSurface[%s]: failed (mesh='%s' name='%s' actorPath='%s') -> %s"),
            *SurfaceState.Id,
            *RequestedMeshName,
            *SurfaceName,
            *RequestedActorPath,
            *SurfaceState.LastError);
        return;
    }

    const FString ResolvedMeshComponentName = BestMesh->GetName();
    const FString ResolvedActorPath = BestOwner->GetPathName();
    const bool bResolvedBindingChanged =
        !SurfaceState.MeshComponentName.Equals(ResolvedMeshComponentName, ESearchCase::CaseSensitive)
        || !SurfaceState.ActorPath.Equals(ResolvedActorPath, ESearchCase::CaseSensitive);

    if (bResolvedBindingChanged)
    {
        // World switches (Editor <-> PIE) can keep the same logical surface id while
        // resolving to a different mesh instance. Clear old MID/cache state before rebinding.
        RestoreSurfaceMaterials(SurfaceState);
    }

    SurfaceState.MeshComponent = BestMesh;
    SurfaceState.MeshComponentName = ResolvedMeshComponentName;
    SurfaceState.ActorPath = ResolvedActorPath;
    SurfaceState.TargetId.Reset();
    SurfaceState.NextResolveRetryTimeSeconds = 0.0;

    const int32 SlotCount = BestMesh->GetNumMaterials();
    TArray<int32> SanitizedSlots;

    if (SurfaceState.MaterialSlots.Num() == 0)
    {
        for (int32 Slot = 0; Slot < SlotCount; ++Slot)
        {
            SanitizedSlots.Add(Slot);
        }
    }
    else
    {
        for (int32 Slot : SurfaceState.MaterialSlots)
        {
            if (Slot >= 0 && Slot < SlotCount)
            {
                SanitizedSlots.AddUnique(Slot);
            }
        }
    }

    if (SanitizedSlots.Num() == 0)
    {
        for (int32 Slot = 0; Slot < SlotCount; ++Slot)
        {
            SanitizedSlots.Add(Slot);
        }
    }
    SurfaceState.MaterialSlots = MoveTemp(SanitizedSlots);

    if (bResolvedBindingChanged)
    {
        MarkCacheDirty();
    }

    UE_LOG(LogRshipExec, Log, TEXT("ResolveMappingSurface[%s]: mesh='%s' actor='%s' world='%s' slots=%d score=%d"),
        *SurfaceState.Id,
        *SurfaceState.MeshComponentName,
        *BestOwner->GetName(),
        BestOwner->GetWorld() ? *BestOwner->GetWorld()->GetPathName() : TEXT("<none>"),
        SurfaceState.MaterialSlots.Num(),
        BestScore);
}

bool URshipContentMappingManager::IsFeedV2Mapping(const FRshipContentMappingState& MappingState) const
{
    if (!MappingState.Config.IsValid() || !MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
    {
        return false;
    }

    const FString TypeToken = MappingState.Type.TrimStartAndEnd().ToLower();
    const bool bIsUvType = TypeToken == TEXT("surface-uv")
        || TypeToken == TEXT("feed")
        || TypeToken == TEXT("surface-feed");
    if (!bIsUvType)
    {
        return false;
    }

    const FString UvMode = GetStringField(MappingState.Config, TEXT("uvMode"), TEXT("feed")).TrimStartAndEnd().ToLower();
    return UvMode.IsEmpty() || UvMode == TEXT("feed") || UvMode == TEXT("surface-feed");
}

FString URshipContentMappingManager::GetMappingMaterialProfileToken(const FRshipContentMappingState& MappingState) const
{
    const FString TypeToken = MappingState.Type.TrimStartAndEnd().ToLower();
    const bool bIsUvType = TypeToken == TEXT("surface-uv")
        || TypeToken == TEXT("direct")
        || TypeToken == TEXT("feed")
        || TypeToken == TEXT("surface-feed");
    if (bIsUvType)
    {
        return MaterialProfileDirect;
    }

    FString ProjectionType = TEXT("perspective");
    if (MappingState.Config.IsValid())
    {
        ProjectionType = NormalizeProjectionModeToken(
            GetStringField(MappingState.Config, TEXT("projectionType"), ProjectionType),
            TEXT("perspective"));
    }

    if (ProjectionType.Equals(TEXT("camera-plate"), ESearchCase::IgnoreCase))
    {
        return MaterialProfileCameraPlate;
    }
    if (ProjectionType.Equals(TEXT("spatial"), ESearchCase::IgnoreCase))
    {
        return MaterialProfileSpatial;
    }
    if (ProjectionType.Equals(TEXT("depth-map"), ESearchCase::IgnoreCase))
    {
        return MaterialProfileDepthMap;
    }

    return MaterialProfileProjection;
}

bool URshipContentMappingManager::IsKnownRenderContextId(const FString& ContextId) const
{
    const FString Sanitized = ContextId.TrimStartAndEnd();
    return !Sanitized.IsEmpty() && RenderContexts.Contains(Sanitized);
}

FString URshipContentMappingManager::GetPreferredRuntimeContextId() const
{
    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        if (!Pair.Key.IsEmpty() && Pair.Value.bEnabled && Pair.Value.ResolvedTexture)
        {
            return Pair.Key;
        }
    }
    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        if (!Pair.Key.IsEmpty() && Pair.Value.bEnabled)
        {
            return Pair.Key;
        }
    }
    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        if (!Pair.Key.IsEmpty())
        {
            return Pair.Key;
        }
    }
    return FString();
}

void URshipContentMappingManager::PrepareMappingsForRuntime(bool bEmitChanges)
{
    bRuntimePreparePending = false;
    bool bAnyChanged = false;

    for (auto& MappingPair : Mappings)
    {
        FRshipContentMappingState& MappingState = MappingPair.Value;
        NormalizeMappingState(MappingState);
        if (EnsureMappingRuntimeReady(MappingState))
        {
            NormalizeMappingState(MappingState);
            bAnyChanged = true;

            if (bEmitChanges)
            {
                TrackPendingMappingUpsert(MappingState);
                RegisterMappingTarget(MappingState);
                EmitMappingState(MappingState);
            }
        }
    }

    if (bAnyChanged)
    {
        EffectiveSurfaceIdsCache.Empty();
        RequiredContextIdsCache.Empty();
        MarkCacheDirty();
        bRuntimePreparePending = true;
    }
}

void URshipContentMappingManager::CollectRequiredContextIdsForMappings(
    TSet<FString>& OutRequiredContextIds,
    bool& OutHasEnabledMappings,
    bool& OutKeepAllContextsAlive,
    bool& OutHasInvalidContextReference)
{
    OutRequiredContextIds.Reset();
    OutHasEnabledMappings = false;
    OutKeepAllContextsAlive = false;
    OutHasInvalidContextReference = false;

    auto GetOrBuildRequiredContexts = [this](const FRshipContentMappingState& MappingState) -> const FMappingRequiredContexts&
    {
        if (FMappingRequiredContexts* Cached = RequiredContextIdsCache.Find(MappingState.Id))
        {
            return *Cached;
        }

        FMappingRequiredContexts Built;

        auto AddContextId = [&Built](const FString& InContextId)
        {
            const FString Trimmed = InContextId.TrimStartAndEnd();
            if (!Trimmed.IsEmpty())
            {
                Built.ContextIds.AddUnique(Trimmed);
            }
        };

        const FString MappingContextId = MappingState.ContextId.TrimStartAndEnd();
        if (!MappingContextId.IsEmpty())
        {
            if (RenderContexts.Contains(MappingContextId))
            {
                AddContextId(MappingContextId);
            }
            else
            {
                Built.bHasInvalidContextReference = true;
            }
        }

        if (!IsFeedV2Mapping(MappingState)
            || !MappingState.Config.IsValid()
            || !MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
        {
            return RequiredContextIdsCache.Add(MappingState.Id, MoveTemp(Built));
        }

        const TSharedPtr<FJsonObject> FeedV2 = MappingState.Config->GetObjectField(TEXT("feedV2"));
        bool bFoundValidSourceContext = false;
        bool bFeedHasUnboundSources = false;
        TSet<FString> FeedSourceIds;

        if (FeedV2.IsValid() && FeedV2->HasTypedField<EJson::Array>(TEXT("sources")))
        {
            const TArray<TSharedPtr<FJsonValue>> Sources = FeedV2->GetArrayField(TEXT("sources"));
            for (const TSharedPtr<FJsonValue>& SourceValue : Sources)
            {
                if (!SourceValue.IsValid() || SourceValue->Type != EJson::Object)
                {
                    continue;
                }

                const TSharedPtr<FJsonObject> SourceObj = SourceValue->AsObject();
                if (!SourceObj.IsValid())
                {
                    continue;
                }

                const FString SourceId = GetStringField(SourceObj, TEXT("id")).TrimStartAndEnd();
                if (!SourceId.IsEmpty())
                {
                    FeedSourceIds.Add(SourceId);
                }

                const FString SourceContextId = GetStringField(SourceObj, TEXT("contextId")).TrimStartAndEnd();
                if (!SourceContextId.IsEmpty())
                {
                    if (RenderContexts.Contains(SourceContextId))
                    {
                        AddContextId(SourceContextId);
                        bFoundValidSourceContext = true;
                    }
                    else
                    {
                        bFeedHasUnboundSources = true;
                        Built.bHasInvalidContextReference = true;
                    }
                }
                else
                {
                    bFeedHasUnboundSources = true;
                }
            }
        }
        else
        {
            bFeedHasUnboundSources = true;
        }

        if (FeedV2.IsValid() && FeedV2->HasTypedField<EJson::Array>(TEXT("routes")))
        {
            const TArray<TSharedPtr<FJsonValue>> Routes = FeedV2->GetArrayField(TEXT("routes"));
            for (const TSharedPtr<FJsonValue>& RouteValue : Routes)
            {
                if (!RouteValue.IsValid() || RouteValue->Type != EJson::Object)
                {
                    continue;
                }
                const TSharedPtr<FJsonObject> RouteObj = RouteValue->AsObject();
                if (!RouteObj.IsValid())
                {
                    continue;
                }
                const FString RouteSourceId = GetStringField(RouteObj, TEXT("sourceId")).TrimStartAndEnd();
                if (!RouteSourceId.IsEmpty() && !FeedSourceIds.Contains(RouteSourceId))
                {
                    if (RenderContexts.Contains(RouteSourceId))
                    {
                        AddContextId(RouteSourceId);
                    }
                    else
                    {
                        bFeedHasUnboundSources = true;
                        Built.bHasInvalidContextReference = true;
                    }
                }
            }
        }

        if (!bFoundValidSourceContext)
        {
            bFeedHasUnboundSources = true;
        }

        if (bFeedHasUnboundSources)
        {
            Built.bKeepAllContextsAlive = true;
        }

        return RequiredContextIdsCache.Add(MappingState.Id, MoveTemp(Built));
    };

    for (const TPair<FString, FRshipContentMappingState>& MappingPair : Mappings)
    {
        const FRshipContentMappingState& MappingState = MappingPair.Value;
        if (!MappingState.bEnabled)
        {
            continue;
        }

        OutHasEnabledMappings = true;
        const FMappingRequiredContexts& Required = GetOrBuildRequiredContexts(MappingState);
        for (const FString& ContextId : Required.ContextIds)
        {
            OutRequiredContextIds.Add(ContextId);
        }
        OutKeepAllContextsAlive = OutKeepAllContextsAlive || Required.bKeepAllContextsAlive;
        OutHasInvalidContextReference = OutHasInvalidContextReference || Required.bHasInvalidContextReference;
    }
}

const FRshipRenderContextState* URshipContentMappingManager::ResolveEffectiveContextState(
    const FRshipContentMappingState& MappingState,
    bool bRequireTexture) const
{
    const bool bAllowSemanticFallbacks = CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0;

    auto FindContextById = [this](const FString& ContextId) -> const FRshipRenderContextState*
    {
        return ContextId.IsEmpty() ? nullptr : RenderContexts.Find(ContextId);
    };

    const FString MappingContextId = MappingState.ContextId.TrimStartAndEnd();
    if (!MappingContextId.IsEmpty())
    {
        if (const FRshipRenderContextState* Preferred = FindContextById(MappingContextId))
        {
            if (!bRequireTexture || Preferred->ResolvedTexture)
            {
                return Preferred;
            }
        }
    }

    if (!bAllowSemanticFallbacks)
    {
        return nullptr;
    }

    if (bRequireTexture)
    {
        if (const FRshipRenderContextState* Preferred = FindContextById(CachedEnabledTextureContextId))
        {
            return Preferred;
        }
        if (const FRshipRenderContextState* Preferred = FindContextById(CachedAnyTextureContextId))
        {
            return Preferred;
        }
    }
    else
    {
        if (const FRshipRenderContextState* Preferred = FindContextById(CachedEnabledTextureContextId))
        {
            return Preferred;
        }
        if (const FRshipRenderContextState* Preferred = FindContextById(CachedAnyTextureContextId))
        {
            return Preferred;
        }
        if (const FRshipRenderContextState* Preferred = FindContextById(CachedEnabledContextId))
        {
            return Preferred;
        }
        if (const FRshipRenderContextState* Preferred = FindContextById(CachedAnyContextId))
        {
            return Preferred;
        }
    }

    // Safety fallback when called before refresh pass has built fallback ids.
    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        if (Pair.Value.bEnabled && Pair.Value.ResolvedTexture)
        {
            return &Pair.Value;
        }
    }
    for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
    {
        if (Pair.Value.ResolvedTexture)
        {
            return &Pair.Value;
        }
    }
    if (!bRequireTexture)
    {
        for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
        {
            if (Pair.Value.bEnabled)
            {
                return &Pair.Value;
            }
        }
        for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
        {
            return &Pair.Value;
        }
    }

    return nullptr;
}

bool URshipContentMappingManager::EnsureMappingRuntimeReady(FRshipContentMappingState& MappingState)
{
    bool bChanged = false;

    const FString PreferredContextId = GetPreferredRuntimeContextId();
    const FString CurrentContextId = MappingState.ContextId.TrimStartAndEnd();
    if (CurrentContextId.IsEmpty()
        && !PreferredContextId.IsEmpty())
    {
        MappingState.ContextId = PreferredContextId;
        bChanged = true;
    }

    // Do not auto-repopulate surface bindings during runtime normalization.
    // Surface assignment is user-authored and should remain stable after deletes.

    if (EnsureFeedMappingRuntimeReady(MappingState))
    {
        bChanged = true;
    }

    return bChanged;
}

bool URshipContentMappingManager::EnsureFeedMappingRuntimeReady(FRshipContentMappingState& MappingState)
{
    if (!IsFeedV2Mapping(MappingState))
    {
        return false;
    }

    bool bChanged = false;

    if (!MappingState.Config.IsValid())
    {
        MappingState.Config = MakeShared<FJsonObject>();
        bChanged = true;
    }

    TSharedPtr<FJsonObject> FeedV2 = MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2"))
        ? MappingState.Config->GetObjectField(TEXT("feedV2"))
        : MakeShared<FJsonObject>();
    if (!MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
    {
        MappingState.Config->SetObjectField(TEXT("feedV2"), FeedV2);
        bChanged = true;
    }

    const FString CoordinateSpace = GetStringField(FeedV2, TEXT("coordinateSpace"), TEXT("pixel")).TrimStartAndEnd().ToLower();
    if (CoordinateSpace != TEXT("pixel"))
    {
        FeedV2->SetStringField(TEXT("coordinateSpace"), TEXT("pixel"));
        bChanged = true;
    }

    FString DefaultContextId = MappingState.ContextId.TrimStartAndEnd();
    if (!IsKnownRenderContextId(DefaultContextId))
    {
        DefaultContextId = GetPreferredRuntimeContextId();
    }
    if (MappingState.ContextId.TrimStartAndEnd().IsEmpty() && !DefaultContextId.IsEmpty())
    {
        MappingState.ContextId = DefaultContextId;
        bChanged = true;
    }

    auto ResolveContextDimensions = [this](const FString& ContextId, int32& OutWidth, int32& OutHeight)
    {
        OutWidth = 1920;
        OutHeight = 1080;
        const FString Id = ContextId.TrimStartAndEnd();
        if (Id.IsEmpty())
        {
            return;
        }
        if (const FRshipRenderContextState* State = RenderContexts.Find(Id))
        {
            OutWidth = FMath::Max(1, State->Width);
            OutHeight = FMath::Max(1, State->Height);
            if (State->ResolvedTexture)
            {
                OutWidth = FMath::Max(1, State->ResolvedTexture->GetSurfaceWidth());
                OutHeight = FMath::Max(1, State->ResolvedTexture->GetSurfaceHeight());
            }
        }
    };

    auto MakeRectObject = [](int32 X, int32 Y, int32 W, int32 H) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Rect = MakeShared<FJsonObject>();
        Rect->SetNumberField(TEXT("x"), FMath::Max(0, X));
        Rect->SetNumberField(TEXT("y"), FMath::Max(0, Y));
        Rect->SetNumberField(TEXT("w"), FMath::Max(1, W));
        Rect->SetNumberField(TEXT("h"), FMath::Max(1, H));
        return Rect;
    };

    TArray<TSharedPtr<FJsonValue>> SourceArray = FeedV2->HasTypedField<EJson::Array>(TEXT("sources"))
        ? FeedV2->GetArrayField(TEXT("sources"))
        : TArray<TSharedPtr<FJsonValue>>();
    if (!FeedV2->HasTypedField<EJson::Array>(TEXT("sources")))
    {
        bChanged = true;
    }

    TMap<FString, FIntPoint> SourceDimensions;
    TArray<TSharedPtr<FJsonValue>> SanitizedSources;
    for (const TSharedPtr<FJsonValue>& Value : SourceArray)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            bChanged = true;
            continue;
        }

        TSharedPtr<FJsonObject> SourceObj = Value->AsObject();
        if (!SourceObj.IsValid())
        {
            bChanged = true;
            continue;
        }

        FString SourceId = GetStringField(SourceObj, TEXT("id")).TrimStartAndEnd();
        if (SourceId.IsEmpty())
        {
            SourceId = FString::Printf(TEXT("source-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
            SourceObj->SetStringField(TEXT("id"), SourceId);
            bChanged = true;
        }

        FString SourceContextId = GetStringField(SourceObj, TEXT("contextId")).TrimStartAndEnd();
        if (SourceContextId.IsEmpty() || !IsKnownRenderContextId(SourceContextId))
        {
            if (!DefaultContextId.IsEmpty())
            {
                if (!SourceContextId.Equals(DefaultContextId, ESearchCase::CaseSensitive))
                {
                    SourceObj->SetStringField(TEXT("contextId"), DefaultContextId);
                    bChanged = true;
                }
                SourceContextId = DefaultContextId;
            }
        }

        int32 Width = GetIntField(SourceObj, TEXT("width"), 0);
        int32 Height = GetIntField(SourceObj, TEXT("height"), 0);
        if (Width <= 0 || Height <= 0)
        {
            ResolveContextDimensions(SourceContextId, Width, Height);
            SourceObj->SetNumberField(TEXT("width"), FMath::Max(1, Width));
            SourceObj->SetNumberField(TEXT("height"), FMath::Max(1, Height));
            bChanged = true;
        }
        else
        {
            Width = FMath::Max(1, Width);
            Height = FMath::Max(1, Height);
        }

        SourceDimensions.Add(SourceId, FIntPoint(Width, Height));
        SanitizedSources.Add(MakeShared<FJsonValueObject>(SourceObj));
    }

    FeedV2->SetArrayField(TEXT("sources"), SanitizedSources);

    TArray<FString> MappingSurfaceIds;
    for (const FString& RawSurfaceId : MappingState.SurfaceIds)
    {
        const FString SurfaceId = RawSurfaceId.TrimStartAndEnd();
        if (!SurfaceId.IsEmpty())
        {
            MappingSurfaceIds.AddUnique(SurfaceId);
        }
    }

    TArray<TSharedPtr<FJsonValue>> DestinationArray = FeedV2->HasTypedField<EJson::Array>(TEXT("destinations"))
        ? FeedV2->GetArrayField(TEXT("destinations"))
        : TArray<TSharedPtr<FJsonValue>>();
    if (!FeedV2->HasTypedField<EJson::Array>(TEXT("destinations")))
    {
        bChanged = true;
    }

    TMap<FString, FIntPoint> DestinationDimensions;
    TArray<TSharedPtr<FJsonValue>> SanitizedDestinations;
    int32 FallbackSurfaceIndex = 0;
    int32 FallbackSourceWidth = 1920;
    int32 FallbackSourceHeight = 1080;
    if (SourceDimensions.Num() > 0)
    {
        for (const TPair<FString, FIntPoint>& Pair : SourceDimensions)
        {
            FallbackSourceWidth = FMath::Max(1, Pair.Value.X);
            FallbackSourceHeight = FMath::Max(1, Pair.Value.Y);
            break;
        }
    }
    for (const TSharedPtr<FJsonValue>& Value : DestinationArray)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            bChanged = true;
            continue;
        }

        TSharedPtr<FJsonObject> DestinationObj = Value->AsObject();
        if (!DestinationObj.IsValid())
        {
            bChanged = true;
            continue;
        }

        FString DestinationId = GetStringField(DestinationObj, TEXT("id")).TrimStartAndEnd();
        if (DestinationId.IsEmpty())
        {
            DestinationId = FString::Printf(TEXT("dest-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
            DestinationObj->SetStringField(TEXT("id"), DestinationId);
            bChanged = true;
        }

        FString SurfaceId = GetStringField(DestinationObj, TEXT("surfaceId")).TrimStartAndEnd();
        if (SurfaceId.IsEmpty() && MappingSurfaceIds.Num() > 0)
        {
            SurfaceId = MappingSurfaceIds[FallbackSurfaceIndex % MappingSurfaceIds.Num()];
            DestinationObj->SetStringField(TEXT("surfaceId"), SurfaceId);
            ++FallbackSurfaceIndex;
            bChanged = true;
        }

        int32 Width = GetIntField(DestinationObj, TEXT("width"), 0);
        int32 Height = GetIntField(DestinationObj, TEXT("height"), 0);
        if (Width <= 0 || Height <= 0)
        {
            Width = FallbackSourceWidth;
            Height = FallbackSourceHeight;
            DestinationObj->SetNumberField(TEXT("width"), FMath::Max(1, Width));
            DestinationObj->SetNumberField(TEXT("height"), FMath::Max(1, Height));
            bChanged = true;
        }
        else
        {
            Width = FMath::Max(1, Width);
            Height = FMath::Max(1, Height);
        }

        DestinationDimensions.Add(DestinationId, FIntPoint(Width, Height));
        SanitizedDestinations.Add(MakeShared<FJsonValueObject>(DestinationObj));
    }

    FeedV2->SetArrayField(TEXT("destinations"), SanitizedDestinations);

    for (const TSharedPtr<FJsonValue>& Value : SanitizedDestinations)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            continue;
        }

        const TSharedPtr<FJsonObject> DestinationObj = Value->AsObject();
        if (!DestinationObj.IsValid())
        {
            continue;
        }

        const FString DestinationSurfaceId = GetStringField(DestinationObj, TEXT("surfaceId")).TrimStartAndEnd();
        if (!DestinationSurfaceId.IsEmpty() && !MappingState.SurfaceIds.Contains(DestinationSurfaceId))
        {
            MappingState.SurfaceIds.Add(DestinationSurfaceId);
            bChanged = true;
        }
    }

    TArray<FString> SourceIds;
    SourceDimensions.GetKeys(SourceIds);
    TArray<FString> DestinationIds;
    DestinationDimensions.GetKeys(DestinationIds);
    SourceIds.Sort();
    DestinationIds.Sort();

    const FString DefaultSourceId = SourceIds.Num() > 0 ? SourceIds[0] : FString();
    const FString DefaultDestinationId = DestinationIds.Num() > 0 ? DestinationIds[0] : FString();

    TArray<TSharedPtr<FJsonValue>> RouteArray = FeedV2->HasTypedField<EJson::Array>(TEXT("routes"))
        ? FeedV2->GetArrayField(TEXT("routes"))
        : TArray<TSharedPtr<FJsonValue>>();
    if (!FeedV2->HasTypedField<EJson::Array>(TEXT("routes")))
    {
        bChanged = true;
    }

    TArray<TSharedPtr<FJsonValue>> SanitizedRoutes;
    for (const TSharedPtr<FJsonValue>& Value : RouteArray)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            bChanged = true;
            continue;
        }

        TSharedPtr<FJsonObject> RouteObj = Value->AsObject();
        if (!RouteObj.IsValid())
        {
            bChanged = true;
            continue;
        }

        FString RouteId = GetStringField(RouteObj, TEXT("id")).TrimStartAndEnd();
        if (RouteId.IsEmpty())
        {
            RouteId = FString::Printf(TEXT("route-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
            RouteObj->SetStringField(TEXT("id"), RouteId);
            bChanged = true;
        }

        FString SourceId = GetStringField(RouteObj, TEXT("sourceId")).TrimStartAndEnd();
        if (SourceId.IsEmpty() || !SourceDimensions.Contains(SourceId))
        {
            if (!DefaultSourceId.IsEmpty())
            {
                RouteObj->SetStringField(TEXT("sourceId"), DefaultSourceId);
                SourceId = DefaultSourceId;
                bChanged = true;
            }
        }

        FString DestinationId = GetStringField(RouteObj, TEXT("destinationId")).TrimStartAndEnd();
        if (DestinationId.IsEmpty() || !DestinationDimensions.Contains(DestinationId))
        {
            if (!DefaultDestinationId.IsEmpty())
            {
                RouteObj->SetStringField(TEXT("destinationId"), DefaultDestinationId);
                DestinationId = DefaultDestinationId;
                bChanged = true;
            }
        }

        const bool bEnabled = GetBoolField(RouteObj, TEXT("enabled"), true);
        if (!RouteObj->HasTypedField<EJson::Boolean>(TEXT("enabled")))
        {
            RouteObj->SetBoolField(TEXT("enabled"), bEnabled);
            bChanged = true;
        }
        const float Opacity = FMath::Clamp(GetNumberField(RouteObj, TEXT("opacity"), 1.0f), 0.0f, 1.0f);
        if (!RouteObj->HasTypedField<EJson::Number>(TEXT("opacity")) || !FMath::IsNearlyEqual(Opacity, GetNumberField(RouteObj, TEXT("opacity"), Opacity)))
        {
            RouteObj->SetNumberField(TEXT("opacity"), Opacity);
            bChanged = true;
        }

        const FIntPoint SourceDim = SourceDimensions.Contains(SourceId) ? SourceDimensions[SourceId] : FIntPoint(1920, 1080);
        const FIntPoint DestinationDim = DestinationDimensions.Contains(DestinationId) ? DestinationDimensions[DestinationId] : FIntPoint(1920, 1080);

        TSharedPtr<FJsonObject> SourceRect = RouteObj->HasTypedField<EJson::Object>(TEXT("sourceRect"))
            ? RouteObj->GetObjectField(TEXT("sourceRect"))
            : nullptr;
        if (!SourceRect.IsValid())
        {
            SourceRect = MakeRectObject(0, 0, SourceDim.X, SourceDim.Y);
            RouteObj->SetObjectField(TEXT("sourceRect"), SourceRect);
            bChanged = true;
        }
        const int32 SrcX = FMath::Max(0, GetIntField(SourceRect, TEXT("x"), 0));
        const int32 SrcY = FMath::Max(0, GetIntField(SourceRect, TEXT("y"), 0));
        const int32 SrcW = FMath::Max(1, GetIntField(SourceRect, TEXT("w"), GetIntField(SourceRect, TEXT("width"), SourceDim.X)));
        const int32 SrcH = FMath::Max(1, GetIntField(SourceRect, TEXT("h"), GetIntField(SourceRect, TEXT("height"), SourceDim.Y)));
        if (GetIntField(SourceRect, TEXT("x"), SrcX) != SrcX
            || GetIntField(SourceRect, TEXT("y"), SrcY) != SrcY
            || GetIntField(SourceRect, TEXT("w"), SrcW) != SrcW
            || GetIntField(SourceRect, TEXT("h"), SrcH) != SrcH)
        {
            SourceRect->SetNumberField(TEXT("x"), SrcX);
            SourceRect->SetNumberField(TEXT("y"), SrcY);
            SourceRect->SetNumberField(TEXT("w"), SrcW);
            SourceRect->SetNumberField(TEXT("h"), SrcH);
            bChanged = true;
        }

        TSharedPtr<FJsonObject> DestinationRect = RouteObj->HasTypedField<EJson::Object>(TEXT("destinationRect"))
            ? RouteObj->GetObjectField(TEXT("destinationRect"))
            : nullptr;
        if (!DestinationRect.IsValid())
        {
            DestinationRect = MakeRectObject(0, 0, DestinationDim.X, DestinationDim.Y);
            RouteObj->SetObjectField(TEXT("destinationRect"), DestinationRect);
            bChanged = true;
        }
        const int32 DstX = FMath::Max(0, GetIntField(DestinationRect, TEXT("x"), 0));
        const int32 DstY = FMath::Max(0, GetIntField(DestinationRect, TEXT("y"), 0));
        const int32 DstW = FMath::Max(1, GetIntField(DestinationRect, TEXT("w"), GetIntField(DestinationRect, TEXT("width"), DestinationDim.X)));
        const int32 DstH = FMath::Max(1, GetIntField(DestinationRect, TEXT("h"), GetIntField(DestinationRect, TEXT("height"), DestinationDim.Y)));
        if (GetIntField(DestinationRect, TEXT("x"), DstX) != DstX
            || GetIntField(DestinationRect, TEXT("y"), DstY) != DstY
            || GetIntField(DestinationRect, TEXT("w"), DstW) != DstW
            || GetIntField(DestinationRect, TEXT("h"), DstH) != DstH)
        {
            DestinationRect->SetNumberField(TEXT("x"), DstX);
            DestinationRect->SetNumberField(TEXT("y"), DstY);
            DestinationRect->SetNumberField(TEXT("w"), DstW);
            DestinationRect->SetNumberField(TEXT("h"), DstH);
            bChanged = true;
        }

        SanitizedRoutes.Add(MakeShared<FJsonValueObject>(RouteObj));
    }

    FeedV2->SetArrayField(TEXT("routes"), SanitizedRoutes);
    MappingState.Config->SetObjectField(TEXT("feedV2"), FeedV2);
    return bChanged;
}

FString URshipContentMappingManager::MakeFeedCompositeKey(const FString& MappingId, const FString& SurfaceId) const
{
    return MappingId + TEXT(":") + SurfaceId;
}

void URshipContentMappingManager::RemoveFeedCompositeTexturesForMapping(const FString& MappingId)
{
    if (MappingId.IsEmpty())
    {
        return;
    }

    const FString Prefix = MappingId + TEXT(":");
    TArray<FString> Keys;
    FeedCompositeTargets.GetKeys(Keys);
    for (const FString& Key : Keys)
    {
        if (Key.StartsWith(Prefix))
        {
            FeedCompositeTargets.Remove(Key);
            FeedCompositeStaticSignatures.Remove(Key);
        }
    }
}

void URshipContentMappingManager::RemoveFeedCompositeTexturesForSurface(const FString& SurfaceId)
{
    if (SurfaceId.IsEmpty())
    {
        return;
    }

    TArray<FString> Keys;
    FeedCompositeTargets.GetKeys(Keys);
    for (const FString& Key : Keys)
    {
        FString Left;
        FString Right;
        if (Key.Split(TEXT(":"), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd) && Right == SurfaceId)
        {
            FeedCompositeTargets.Remove(Key);
            FeedCompositeStaticSignatures.Remove(Key);
        }
    }
}

UTexture* URshipContentMappingManager::BuildFeedCompositeTextureForSurface(
    const FRshipContentMappingState& MappingState,
    const FRshipMappingSurfaceState& SurfaceState,
    FString& OutError)
{
    OutError.Reset();
    const bool bAllowSemanticFallbacks = CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0;

    if (!MappingState.Config.IsValid() || !MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
    {
        return nullptr;
    }

    const TSharedPtr<FJsonObject> FeedV2 = MappingState.Config->GetObjectField(TEXT("feedV2"));
    if (!FeedV2.IsValid())
    {
        return nullptr;
    }

    const FString CoordinateSpace = GetStringField(FeedV2, TEXT("coordinateSpace"), TEXT("pixel")).TrimStartAndEnd().ToLower();
    if (!CoordinateSpace.IsEmpty() && CoordinateSpace != TEXT("pixel"))
    {
        OutError = FString::Printf(TEXT("feedV2 coordinateSpace '%s' is not supported (expected 'pixel')"), *CoordinateSpace);
        return nullptr;
    }

    FFeedV2Spec Spec;
    Spec.bValid = true;
    Spec.CoordinateSpace = CoordinateSpace.IsEmpty() ? TEXT("pixel") : CoordinateSpace;

    auto ParseRectPx = [this](const TSharedPtr<FJsonObject>& RectObj, const FFeedRectPx& Defaults, FFeedRectPx& OutRect)
    {
        OutRect = Defaults;
        if (!RectObj.IsValid())
        {
            return false;
        }
        OutRect.X = GetIntField(RectObj, TEXT("x"), GetIntField(RectObj, TEXT("u"), Defaults.X));
        OutRect.Y = GetIntField(RectObj, TEXT("y"), GetIntField(RectObj, TEXT("v"), Defaults.Y));
        OutRect.W = GetIntField(RectObj, TEXT("w"), GetIntField(RectObj, TEXT("width"), Defaults.W));
        OutRect.H = GetIntField(RectObj, TEXT("h"), GetIntField(RectObj, TEXT("height"), Defaults.H));
        return true;
    };

    if (FeedV2->HasTypedField<EJson::Array>(TEXT("sources")))
    {
        const TArray<TSharedPtr<FJsonValue>> SourceArray = FeedV2->GetArrayField(TEXT("sources"));
        for (const TSharedPtr<FJsonValue>& Value : SourceArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                continue;
            }

            const TSharedPtr<FJsonObject> SourceObj = Value->AsObject();
            if (!SourceObj.IsValid())
            {
                continue;
            }

            FFeedSourceSpec Source;
            Source.Id = GetStringField(SourceObj, TEXT("id")).TrimStartAndEnd();
            Source.Label = GetStringField(SourceObj, TEXT("label")).TrimStartAndEnd();
            Source.ContextId = GetStringField(SourceObj, TEXT("contextId")).TrimStartAndEnd();
            Source.Width = FMath::Max(0, GetIntField(SourceObj, TEXT("width"), 0));
            Source.Height = FMath::Max(0, GetIntField(SourceObj, TEXT("height"), 0));

            if (Source.Id.IsEmpty() && !Source.ContextId.IsEmpty())
            {
                Source.Id = Source.ContextId;
            }
            if (Source.ContextId.IsEmpty() && !Source.Id.IsEmpty())
            {
                Source.ContextId = Source.Id;
            }
            if (!Source.Id.IsEmpty())
            {
                Spec.Sources.Add(Source.Id, Source);
            }
        }
    }

    if (FeedV2->HasTypedField<EJson::Array>(TEXT("destinations")))
    {
        const TArray<TSharedPtr<FJsonValue>> DestinationArray = FeedV2->GetArrayField(TEXT("destinations"));
        for (const TSharedPtr<FJsonValue>& Value : DestinationArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                continue;
            }

            const TSharedPtr<FJsonObject> DestinationObj = Value->AsObject();
            if (!DestinationObj.IsValid())
            {
                continue;
            }

            FFeedDestinationSpec Destination;
            Destination.Id = GetStringField(DestinationObj, TEXT("id")).TrimStartAndEnd();
            Destination.Label = GetStringField(DestinationObj, TEXT("label")).TrimStartAndEnd();
            Destination.SurfaceId = GetStringField(DestinationObj, TEXT("surfaceId")).TrimStartAndEnd();
            Destination.Width = FMath::Max(0, GetIntField(DestinationObj, TEXT("width"), 0));
            Destination.Height = FMath::Max(0, GetIntField(DestinationObj, TEXT("height"), 0));

            if (Destination.Id.IsEmpty() && !Destination.SurfaceId.IsEmpty())
            {
                Destination.Id = Destination.SurfaceId;
            }
            if (Destination.SurfaceId.IsEmpty() && !Destination.Id.IsEmpty())
            {
                Destination.SurfaceId = Destination.Id;
            }
            if (!Destination.Id.IsEmpty())
            {
                Spec.Destinations.Add(Destination.Id, Destination);
            }
        }
    }

    if (Spec.Sources.Num() == 0 && !MappingState.ContextId.IsEmpty())
    {
        FFeedSourceSpec Source;
        Source.Id = TEXT("default-source");
        Source.Label = TEXT("Default Source");
        Source.ContextId = MappingState.ContextId;
        if (const FRshipRenderContextState* ContextState = RenderContexts.Find(MappingState.ContextId))
        {
            Source.Width = FMath::Max(0, ContextState->Width);
            Source.Height = FMath::Max(0, ContextState->Height);
        }
        Spec.Sources.Add(Source.Id, Source);
    }

    if (Spec.Destinations.Num() == 0)
    {
        for (const FString& SurfaceId : MappingState.SurfaceIds)
        {
            if (SurfaceId.IsEmpty())
            {
                continue;
            }
            FFeedDestinationSpec Destination;
            Destination.Id = SurfaceId;
            Destination.Label = SurfaceId;
            Destination.SurfaceId = SurfaceId;
            Spec.Destinations.Add(Destination.Id, Destination);
        }
    }

    if (FeedV2->HasTypedField<EJson::Array>(TEXT("routes")))
    {
        const TArray<TSharedPtr<FJsonValue>> RouteArray = FeedV2->GetArrayField(TEXT("routes"));
        for (const TSharedPtr<FJsonValue>& Value : RouteArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                continue;
            }

            const TSharedPtr<FJsonObject> RouteObj = Value->AsObject();
            if (!RouteObj.IsValid())
            {
                continue;
            }

            FFeedRouteSpec Route;
            Route.Id = GetStringField(RouteObj, TEXT("id")).TrimStartAndEnd();
            Route.Label = GetStringField(RouteObj, TEXT("label")).TrimStartAndEnd();
            Route.SourceId = GetStringField(RouteObj, TEXT("sourceId")).TrimStartAndEnd();
            Route.DestinationId = GetStringField(RouteObj, TEXT("destinationId")).TrimStartAndEnd();
            Route.bEnabled = GetBoolField(RouteObj, TEXT("enabled"), true);
            Route.Opacity = FMath::Clamp(GetNumberField(RouteObj, TEXT("opacity"), 1.0f), 0.0f, 1.0f);

            int32 DefaultSourceWidth = 1920;
            int32 DefaultSourceHeight = 1080;
            if (const FFeedSourceSpec* ParsedSource = Spec.Sources.Find(Route.SourceId))
            {
                if (ParsedSource->Width > 0)
                {
                    DefaultSourceWidth = ParsedSource->Width;
                }
                if (ParsedSource->Height > 0)
                {
                    DefaultSourceHeight = ParsedSource->Height;
                }
                if (!ParsedSource->ContextId.IsEmpty())
                {
                    if (const FRshipRenderContextState* ParsedSourceContext = RenderContexts.Find(ParsedSource->ContextId))
                    {
                        if (ParsedSourceContext->Width > 0)
                        {
                            DefaultSourceWidth = ParsedSourceContext->Width;
                        }
                        if (ParsedSourceContext->Height > 0)
                        {
                            DefaultSourceHeight = ParsedSourceContext->Height;
                        }
                    }
                }
            }
            else if (!MappingState.ContextId.IsEmpty())
            {
                if (const FRshipRenderContextState* MappingContext = RenderContexts.Find(MappingState.ContextId))
                {
                    if (MappingContext->Width > 0)
                    {
                        DefaultSourceWidth = MappingContext->Width;
                    }
                    if (MappingContext->Height > 0)
                    {
                        DefaultSourceHeight = MappingContext->Height;
                    }
                }
            }

            int32 DefaultDestinationWidth = 1920;
            int32 DefaultDestinationHeight = 1080;
            const FFeedDestinationSpec* ParsedDestination = Spec.Destinations.Find(Route.DestinationId);
            if (!ParsedDestination && !Route.DestinationId.IsEmpty())
            {
                for (const TPair<FString, FFeedDestinationSpec>& Pair : Spec.Destinations)
                {
                    if (Pair.Value.SurfaceId == Route.DestinationId)
                    {
                        ParsedDestination = &Pair.Value;
                        break;
                    }
                }
            }
            if (ParsedDestination)
            {
                if (ParsedDestination->Width > 0)
                {
                    DefaultDestinationWidth = ParsedDestination->Width;
                }
                if (ParsedDestination->Height > 0)
                {
                    DefaultDestinationHeight = ParsedDestination->Height;
                }
            }

            FFeedRectPx DefaultSourceRect;
            DefaultSourceRect.W = FMath::Max(1, DefaultSourceWidth);
            DefaultSourceRect.H = FMath::Max(1, DefaultSourceHeight);
            FFeedRectPx DefaultDestinationRect;
            DefaultDestinationRect.W = FMath::Max(1, DefaultDestinationWidth);
            DefaultDestinationRect.H = FMath::Max(1, DefaultDestinationHeight);

            TSharedPtr<FJsonObject> SourceRectObj;
            if (RouteObj->HasTypedField<EJson::Object>(TEXT("sourceRect")))
            {
                SourceRectObj = RouteObj->GetObjectField(TEXT("sourceRect"));
            }
            ParseRectPx(SourceRectObj, DefaultSourceRect, Route.SourceRect);

            TSharedPtr<FJsonObject> DestinationRectObj;
            if (RouteObj->HasTypedField<EJson::Object>(TEXT("destinationRect")))
            {
                DestinationRectObj = RouteObj->GetObjectField(TEXT("destinationRect"));
            }
            ParseRectPx(DestinationRectObj, DefaultDestinationRect, Route.DestinationRect);

            if (Route.Id.IsEmpty())
            {
                Route.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
            }
            if (!Route.SourceId.IsEmpty() && !Route.DestinationId.IsEmpty())
            {
                Spec.Routes.Add(Route);
            }
        }
    }

    if (!Spec.bValid)
    {
        return nullptr;
    }

    FFeedDestinationSpec DestinationSpec;
    bool bDestinationFound = false;
    for (const TPair<FString, FFeedDestinationSpec>& Pair : Spec.Destinations)
    {
        if (Pair.Value.SurfaceId == SurfaceState.Id || Pair.Key == SurfaceState.Id)
        {
            DestinationSpec = Pair.Value;
            bDestinationFound = true;
            break;
        }
    }

    if (!bDestinationFound)
    {
        DestinationSpec.Id = SurfaceState.Id;
        DestinationSpec.SurfaceId = SurfaceState.Id;
        DestinationSpec.Label = SurfaceState.Name.IsEmpty() ? SurfaceState.Id : SurfaceState.Name;
        bDestinationFound = true;
    }

    const auto ResolveDestinationDimension = [this, &DestinationSpec, &MappingState](bool bWidth) -> int32
    {
        int32 Value = bWidth ? DestinationSpec.Width : DestinationSpec.Height;
        if (Value > 0)
        {
            return Value;
        }
        if (const FRshipRenderContextState* ContextState = RenderContexts.Find(MappingState.ContextId))
        {
            Value = bWidth ? ContextState->Width : ContextState->Height;
            if (Value > 0)
            {
                return Value;
            }
        }
        return bWidth ? 1920 : 1080;
    };

    const int32 DestinationWidth = FMath::Max(1, ResolveDestinationDimension(true));
    const int32 DestinationHeight = FMath::Max(1, ResolveDestinationDimension(false));

    auto ResolveDirectFeedFallbackTexture = [this, &MappingState, &Spec, bAllowSemanticFallbacks]() -> UTexture*
    {
        auto ResolveContextTextureById = [this](const FString& RawContextId) -> UTexture*
        {
            const FString ContextId = RawContextId.TrimStartAndEnd();
            if (ContextId.IsEmpty())
            {
                return nullptr;
            }

            if (const FRshipRenderContextState* ContextState = RenderContexts.Find(ContextId))
            {
                return ContextState->ResolvedTexture;
            }

            return nullptr;
        };

        for (const TPair<FString, FFeedSourceSpec>& Pair : Spec.Sources)
        {
            if (UTexture* SourceTexture = ResolveContextTextureById(Pair.Value.ContextId))
            {
                return SourceTexture;
            }
            if (UTexture* SourceTexture = ResolveContextTextureById(Pair.Value.Id))
            {
                return SourceTexture;
            }
            if (UTexture* SourceTexture = ResolveContextTextureById(Pair.Key))
            {
                return SourceTexture;
            }
        }

        if (bAllowSemanticFallbacks)
        {
            if (const FRshipRenderContextState* FallbackContext = ResolveEffectiveContextState(MappingState, true))
            {
                return FallbackContext->ResolvedTexture;
            }
        }

        return nullptr;
    };

    bool bHasRouteForDestination = false;
    const bool bSingleDestination = Spec.Destinations.Num() <= 1;
    for (const FFeedRouteSpec& Route : Spec.Routes)
    {
        if (!Route.bEnabled)
        {
            continue;
        }

        FString RouteDestinationId = Route.DestinationId;
        if (RouteDestinationId.IsEmpty() && bSingleDestination)
        {
            RouteDestinationId = DestinationSpec.Id;
        }
        if (RouteDestinationId == DestinationSpec.Id || RouteDestinationId == DestinationSpec.SurfaceId)
        {
            bHasRouteForDestination = true;
            break;
        }
    }

    const FString CompositeKey = MakeFeedCompositeKey(MappingState.Id, SurfaceState.Id);
    UTextureRenderTarget2D* CompositeRT = FeedCompositeTargets.FindRef(CompositeKey);

    if (!bHasRouteForDestination)
    {
        if (bAllowSemanticFallbacks)
        {
            if (UTexture* DirectFallbackTexture = ResolveDirectFeedFallbackTexture())
            {
                FeedCompositeTargets.Remove(CompositeKey);
                FeedCompositeStaticSignatures.Remove(CompositeKey);
                return DirectFallbackTexture;
            }
        }

        OutError = TEXT("No feed routes mapped to this destination");
        FeedCompositeTargets.Remove(CompositeKey);
        FeedCompositeStaticSignatures.Remove(CompositeKey);
        return nullptr;
    }

    UWorld* World = GetBestWorld();
    if (!World)
    {
        OutError = TEXT("World not available for feed composition");
        return nullptr;
    }

    auto ResolveContextForRoute = [this, &MappingState, bAllowSemanticFallbacks](const FFeedSourceSpec* SourceSpec, const FString& RouteSourceId) -> const FRshipRenderContextState*
    {
        TArray<FString> Candidates;
        auto AddCandidate = [&Candidates](const FString& Value)
        {
            const FString Trimmed = Value.TrimStartAndEnd();
            if (!Trimmed.IsEmpty())
            {
                Candidates.AddUnique(Trimmed);
            }
        };

        if (SourceSpec)
        {
            AddCandidate(SourceSpec->ContextId);
            AddCandidate(SourceSpec->Id);
        }
        AddCandidate(RouteSourceId);
        AddCandidate(MappingState.ContextId);

        for (const FString& Candidate : Candidates)
        {
            if (const FRshipRenderContextState* ContextState = RenderContexts.Find(Candidate))
            {
                if (ContextState->ResolvedTexture)
                {
                    return ContextState;
                }
            }
        }

        for (const FString& Candidate : Candidates)
        {
            if (const FRshipRenderContextState* ContextState = RenderContexts.Find(Candidate))
            {
                return ContextState;
            }
        }

        if (bAllowSemanticFallbacks)
        {
            if (const FRshipRenderContextState* FallbackWithTexture = ResolveEffectiveContextState(MappingState, true))
            {
                return FallbackWithTexture;
            }
            if (const FRshipRenderContextState* FallbackAny = ResolveEffectiveContextState(MappingState, false))
            {
                return FallbackAny;
            }
        }
        return nullptr;
    };

    auto IsDynamicRouteSource = [](const FRshipRenderContextState* SourceContext, const UTexture* SourceTexture) -> bool
    {
        if (!SourceContext || !SourceTexture)
        {
            return false;
        }
        if (SourceTexture->IsA<UTextureRenderTarget2D>())
        {
            return true;
        }
        return !SourceContext->SourceType.Equals(TEXT("asset-store"), ESearchCase::IgnoreCase)
            && !SourceContext->SourceType.Equals(TEXT("asset"), ESearchCase::IgnoreCase);
    };

    bool bHasRenderableRoute = false;
    TArray<FString> PreflightIssues;
    const bool bSingleSourceForPreflight = Spec.Sources.Num() == 1;
    for (const FFeedRouteSpec& Route : Spec.Routes)
    {
        if (!Route.bEnabled)
        {
            continue;
        }

        FString RouteDestinationId = Route.DestinationId;
        if (RouteDestinationId.IsEmpty() && bSingleDestination)
        {
            RouteDestinationId = DestinationSpec.Id;
        }
        if (RouteDestinationId != DestinationSpec.Id && RouteDestinationId != DestinationSpec.SurfaceId)
        {
            continue;
        }

        FString RouteSourceId = Route.SourceId;
        if (RouteSourceId.IsEmpty() && bSingleSourceForPreflight)
        {
            for (const TPair<FString, FFeedSourceSpec>& Pair : Spec.Sources)
            {
                RouteSourceId = Pair.Key;
                break;
            }
        }
        if (RouteSourceId.IsEmpty())
        {
            PreflightIssues.Add(FString::Printf(TEXT("Route '%s' has no source"), *Route.Id));
            continue;
        }

        const FFeedSourceSpec* SourceSpec = Spec.Sources.Find(RouteSourceId);
        const FRshipRenderContextState* SourceContext = ResolveContextForRoute(SourceSpec, RouteSourceId);
        if (SourceContext && SourceContext->ResolvedTexture)
        {
            bHasRenderableRoute = true;
            break;
        }

        PreflightIssues.Add(FString::Printf(TEXT("Route '%s' source '%s' texture unavailable"), *Route.Id, *RouteSourceId));
    }

    if (!bHasRenderableRoute)
    {
        if (bAllowSemanticFallbacks)
        {
            if (UTexture* DirectFallbackTexture = ResolveDirectFeedFallbackTexture())
            {
                FeedCompositeTargets.Remove(CompositeKey);
                FeedCompositeStaticSignatures.Remove(CompositeKey);
                return DirectFallbackTexture;
            }
        }

        OutError = PreflightIssues.Num() > 0
            ? FString::Join(PreflightIssues, TEXT("; "))
            : TEXT("No valid feed routes for this destination");
        FeedCompositeTargets.Remove(CompositeKey);
        FeedCompositeStaticSignatures.Remove(CompositeKey);
        return nullptr;
    }

    uint32 CompositeSignature = HashCombineFast(GetTypeHash(DestinationWidth), GetTypeHash(DestinationHeight));
    CompositeSignature = HashCombineFast(CompositeSignature, GetTypeHash(MappingState.Id));
    CompositeSignature = HashCombineFast(CompositeSignature, GetTypeHash(SurfaceState.Id));
    bool bHasDynamicRouteSource = false;
    int32 SignatureRouteCount = 0;

    for (const FFeedRouteSpec& Route : Spec.Routes)
    {
        if (!Route.bEnabled)
        {
            continue;
        }

        FString RouteDestinationId = Route.DestinationId;
        if (RouteDestinationId.IsEmpty() && bSingleDestination)
        {
            RouteDestinationId = DestinationSpec.Id;
        }
        if (RouteDestinationId != DestinationSpec.Id && RouteDestinationId != DestinationSpec.SurfaceId)
        {
            continue;
        }

        ++SignatureRouteCount;
        uint32 RouteHash = HashCombineFast(GetTypeHash(Route.Id), GetTypeHash(Route.SourceId));
        RouteHash = HashCombineFast(RouteHash, GetTypeHash(RouteDestinationId));
        RouteHash = HashCombineFast(RouteHash, GetTypeHash(Route.Opacity));
        RouteHash = HashCombineFast(RouteHash, HashFeedRouteRectPx(Route.SourceRect));
        RouteHash = HashCombineFast(RouteHash, HashFeedRouteRectPx(Route.DestinationRect));

        FString RouteSourceId = Route.SourceId;
        if (RouteSourceId.IsEmpty() && Spec.Sources.Num() == 1)
        {
            for (const TPair<FString, FFeedSourceSpec>& Pair : Spec.Sources)
            {
                RouteSourceId = Pair.Key;
                break;
            }
        }

        const FFeedSourceSpec* SourceSpec = Spec.Sources.Find(RouteSourceId);
        const FRshipRenderContextState* SourceContext = ResolveContextForRoute(SourceSpec, RouteSourceId);
        if (SourceContext && SourceContext->ResolvedTexture)
        {
            RouteHash = HashCombineFast(RouteHash, PointerHash(SourceContext->ResolvedTexture));
            if (IsDynamicRouteSource(SourceContext, SourceContext->ResolvedTexture))
            {
                bHasDynamicRouteSource = true;
            }
        }
        else
        {
            RouteHash = HashCombineFast(RouteHash, 0xE3AF5A9Du);
        }

        CompositeSignature = HashCombineFast(CompositeSignature, RouteHash);
    }

    if (SignatureRouteCount == 0)
    {
        CompositeSignature = HashCombineFast(CompositeSignature, 0x8AC69E17u);
    }

    // Fast path: one full-frame opaque route can use the source texture directly.
    int32 MatchingEnabledRouteCount = 0;
    const FFeedRouteSpec* SoleRoute = nullptr;
    const bool bSingleSourceForFastPath = Spec.Sources.Num() == 1;
    for (const FFeedRouteSpec& Route : Spec.Routes)
    {
        if (!Route.bEnabled)
        {
            continue;
        }

        FString RouteDestinationId = Route.DestinationId;
        if (RouteDestinationId.IsEmpty() && bSingleDestination)
        {
            RouteDestinationId = DestinationSpec.Id;
        }
        if (RouteDestinationId != DestinationSpec.Id && RouteDestinationId != DestinationSpec.SurfaceId)
        {
            continue;
        }

        ++MatchingEnabledRouteCount;
        SoleRoute = &Route;
        if (MatchingEnabledRouteCount > 1)
        {
            break;
        }
    }

    if (MatchingEnabledRouteCount == 1 && SoleRoute)
    {
        FString RouteSourceId = SoleRoute->SourceId;
        if (RouteSourceId.IsEmpty() && bSingleSourceForFastPath)
        {
            for (const TPair<FString, FFeedSourceSpec>& Pair : Spec.Sources)
            {
                RouteSourceId = Pair.Key;
                break;
            }
        }

        const FFeedSourceSpec* SourceSpec = Spec.Sources.Find(RouteSourceId);
        const FRshipRenderContextState* SourceContext = ResolveContextForRoute(SourceSpec, RouteSourceId);
        if (SourceContext && SourceContext->ResolvedTexture && SoleRoute->Opacity >= 0.999f)
        {
            UTexture* SourceTexture = SourceContext->ResolvedTexture;
            const int32 TextureWidth = FMath::Max(1, SourceTexture->GetSurfaceWidth());
            const int32 TextureHeight = FMath::Max(1, SourceTexture->GetSurfaceHeight());
            const int32 SourceWidth = FMath::Max(1, SourceSpec && SourceSpec->Width > 0 ? SourceSpec->Width : (SourceContext->Width > 0 ? SourceContext->Width : TextureWidth));
            const int32 SourceHeight = FMath::Max(1, SourceSpec && SourceSpec->Height > 0 ? SourceSpec->Height : (SourceContext->Height > 0 ? SourceContext->Height : TextureHeight));

            const int32 SrcX = FMath::Clamp(SoleRoute->SourceRect.X, 0, SourceWidth - 1);
            const int32 SrcY = FMath::Clamp(SoleRoute->SourceRect.Y, 0, SourceHeight - 1);
            const int32 SrcW = FMath::Clamp(SoleRoute->SourceRect.W, 1, SourceWidth - SrcX);
            const int32 SrcH = FMath::Clamp(SoleRoute->SourceRect.H, 1, SourceHeight - SrcY);

            const int32 DstX = FMath::Clamp(SoleRoute->DestinationRect.X, 0, DestinationWidth - 1);
            const int32 DstY = FMath::Clamp(SoleRoute->DestinationRect.Y, 0, DestinationHeight - 1);
            const int32 DstW = FMath::Clamp(SoleRoute->DestinationRect.W, 1, DestinationWidth - DstX);
            const int32 DstH = FMath::Clamp(SoleRoute->DestinationRect.H, 1, DestinationHeight - DstY);

            const bool bSourceFullFrame = (SrcX == 0 && SrcY == 0 && SrcW == SourceWidth && SrcH == SourceHeight);
            const bool bDestinationFullFrame = (DstX == 0 && DstY == 0 && DstW == DestinationWidth && DstH == DestinationHeight);
            if (bSourceFullFrame && bDestinationFullFrame)
            {
                FeedCompositeStaticSignatures.Remove(CompositeKey);
                return SourceTexture;
            }
        }
    }

    const bool bNeedsNewRT = !CompositeRT
        || !IsValid(CompositeRT)
        || CompositeRT->SizeX != DestinationWidth
        || CompositeRT->SizeY != DestinationHeight;
    if (bNeedsNewRT)
    {
        CompositeRT = NewObject<UTextureRenderTarget2D>(this);
        if (!CompositeRT)
        {
            OutError = TEXT("Failed to allocate feed composite render target");
            return nullptr;
        }
        CompositeRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
        CompositeRT->AddressX = TA_Clamp;
        CompositeRT->AddressY = TA_Clamp;
        CompositeRT->ClearColor = FLinearColor::Black;
        CompositeRT->InitCustomFormat(DestinationWidth, DestinationHeight, PF_B8G8R8A8, false);
        CompositeRT->UpdateResourceImmediate(true);
        FeedCompositeTargets.Add(CompositeKey, CompositeRT);
    }

    if (!bHasDynamicRouteSource)
    {
        if (const uint32* CachedSignature = FeedCompositeStaticSignatures.Find(CompositeKey))
        {
            if (*CachedSignature == CompositeSignature && !bNeedsNewRT)
            {
                return CompositeRT;
            }
        }
    }

    UKismetRenderingLibrary::ClearRenderTarget2D(World, CompositeRT, FLinearColor::Black);

    UCanvas* Canvas = nullptr;
    FVector2D CanvasSize(0.0f, 0.0f);
    FDrawToRenderTargetContext DrawContext;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, CompositeRT, Canvas, CanvasSize, DrawContext);

    if (!Canvas)
    {
        UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, DrawContext);
        OutError = TEXT("Feed composite canvas unavailable");
        if (!bHasDynamicRouteSource)
        {
            FeedCompositeStaticSignatures.Add(CompositeKey, CompositeSignature);
        }
        else
        {
            FeedCompositeStaticSignatures.Remove(CompositeKey);
        }
        return CompositeRT;
    }

    TArray<FString> RouteIssues;
    const bool bSingleSource = Spec.Sources.Num() == 1;
    int32 DrawnRouteCount = 0;

    for (const FFeedRouteSpec& Route : Spec.Routes)
    {
        if (!Route.bEnabled)
        {
            continue;
        }

        FString RouteDestinationId = Route.DestinationId;
        if (RouteDestinationId.IsEmpty() && bSingleDestination)
        {
            RouteDestinationId = DestinationSpec.Id;
        }
        if (RouteDestinationId != DestinationSpec.Id && RouteDestinationId != DestinationSpec.SurfaceId)
        {
            continue;
        }

        FString RouteSourceId = Route.SourceId;
        if (RouteSourceId.IsEmpty() && bSingleSource)
        {
            for (const TPair<FString, FFeedSourceSpec>& Pair : Spec.Sources)
            {
                RouteSourceId = Pair.Key;
                break;
            }
        }
        if (RouteSourceId.IsEmpty())
        {
            RouteIssues.Add(FString::Printf(TEXT("Route '%s' has no source"), *Route.Id));
            continue;
        }

        const FFeedSourceSpec* SourceSpec = Spec.Sources.Find(RouteSourceId);
        const FRshipRenderContextState* SourceContext = ResolveContextForRoute(SourceSpec, RouteSourceId);
        if (!SourceContext || !SourceContext->ResolvedTexture)
        {
            RouteIssues.Add(FString::Printf(TEXT("Route '%s' source '%s' texture unavailable"), *Route.Id, *RouteSourceId));
            continue;
        }

        UTexture* SourceTexture = SourceContext->ResolvedTexture;
        const int32 TextureWidth = FMath::Max(1, SourceTexture->GetSurfaceWidth());
        const int32 TextureHeight = FMath::Max(1, SourceTexture->GetSurfaceHeight());
        const int32 SourceWidth = FMath::Max(1, SourceSpec && SourceSpec->Width > 0 ? SourceSpec->Width : (SourceContext->Width > 0 ? SourceContext->Width : TextureWidth));
        const int32 SourceHeight = FMath::Max(1, SourceSpec && SourceSpec->Height > 0 ? SourceSpec->Height : (SourceContext->Height > 0 ? SourceContext->Height : TextureHeight));

        const int32 SrcX = FMath::Clamp(Route.SourceRect.X, 0, SourceWidth - 1);
        const int32 SrcY = FMath::Clamp(Route.SourceRect.Y, 0, SourceHeight - 1);
        const int32 SrcW = FMath::Clamp(Route.SourceRect.W, 1, SourceWidth - SrcX);
        const int32 SrcH = FMath::Clamp(Route.SourceRect.H, 1, SourceHeight - SrcY);

        const int32 DstX = FMath::Clamp(Route.DestinationRect.X, 0, DestinationWidth - 1);
        const int32 DstY = FMath::Clamp(Route.DestinationRect.Y, 0, DestinationHeight - 1);
        const int32 DstW = FMath::Clamp(Route.DestinationRect.W, 1, DestinationWidth - DstX);
        const int32 DstH = FMath::Clamp(Route.DestinationRect.H, 1, DestinationHeight - DstY);

        const FVector2D UVPos(
            static_cast<float>(SrcX) / static_cast<float>(SourceWidth),
            static_cast<float>(SrcY) / static_cast<float>(SourceHeight));
        const FVector2D UVSize(
            static_cast<float>(SrcW) / static_cast<float>(SourceWidth),
            static_cast<float>(SrcH) / static_cast<float>(SourceHeight));

        if (Canvas)
        {
            const float RouteOpacity = FMath::Clamp(Route.Opacity, 0.0f, 1.0f);
            const EBlendMode CopyBlend = (RouteOpacity >= 0.999f) ? BLEND_Opaque : BLEND_Translucent;
            Canvas->K2_DrawTexture(
                SourceTexture,
                FVector2D(static_cast<float>(DstX), static_cast<float>(DstY)),
                FVector2D(static_cast<float>(DstW), static_cast<float>(DstH)),
                UVPos,
                UVSize,
                FLinearColor(1.0f, 1.0f, 1.0f, RouteOpacity),
                CopyBlend,
                0.0f,
                FVector2D::ZeroVector);
            ++DrawnRouteCount;
        }
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, DrawContext);

    if (DrawnRouteCount == 0)
    {
        if (bAllowSemanticFallbacks)
        {
            if (UTexture* DirectFallbackTexture = ResolveDirectFeedFallbackTexture())
            {
                FeedCompositeTargets.Remove(CompositeKey);
                FeedCompositeStaticSignatures.Remove(CompositeKey);
                return DirectFallbackTexture;
            }
        }

        RouteIssues.Add(TEXT("No valid feed routes for this destination"));
    }

    if (RouteIssues.Num() > 0)
    {
        OutError = FString::Join(RouteIssues, TEXT("; "));
        UE_LOG(LogRshipExec, Warning, TEXT("Feed composite issues map=%s surf=%s: %s"),
            *MappingState.Id, *SurfaceState.Id, *OutError);
    }

    if (!bHasDynamicRouteSource)
    {
        FeedCompositeStaticSignatures.Add(CompositeKey, CompositeSignature);
    }
    else
    {
        FeedCompositeStaticSignatures.Remove(CompositeKey);
    }

    return CompositeRT;
}

void URshipContentMappingManager::RebuildMappings()
{
    UWorld* PreferredWorld = GetBestWorld();
    for (auto& Pair : MappingSurfaces)
    {
        RestoreSurfaceMaterials(Pair.Value);
        UMeshComponent* Mesh = Pair.Value.MeshComponent.Get();
        const bool bNeedsResolve = !IsMeshReadyForMaterialMutation(Mesh)
            || (PreferredWorld && Mesh && Mesh->GetWorld() != PreferredWorld);
        if (bNeedsResolve)
        {
            ResolveMappingSurface(Pair.Value);
        }
    }

    if (!bMappingsArmed)
    {
        for (auto& Pair : RenderContexts)
        {
            FRshipRenderContextState& ContextState = Pair.Value;
            NormalizeRenderContextState(ContextState);
            ContextState.LastError.Empty();
            ContextState.ResolvedTexture = nullptr;
            ContextState.ResolvedDepthTexture = nullptr;

            if (USceneCaptureComponent2D* CaptureComponent = ContextState.CaptureComponent.Get())
            {
                CaptureComponent->bCaptureEveryFrame = false;
                CaptureComponent->bCaptureOnMovement = false;
            }

            EmitContextState(ContextState);
        }

        for (auto& MappingPair : Mappings)
        {
            FRshipContentMappingState& MappingState = MappingPair.Value;
            NormalizeMappingState(MappingState);
            if (EnsureMappingRuntimeReady(MappingState))
            {
                NormalizeMappingState(MappingState);
            }
            MappingState.LastError.Empty();
            EmitMappingState(MappingState);
        }

        return;
    }

    if (IsRuntimeBlocked())
    {
        for (auto& Pair : RenderContexts)
        {
            NormalizeRenderContextState(Pair.Value);
            EmitContextState(Pair.Value);
        }
        ApplyRuntimeHealthToStates(/*bEmitChanges=*/true);
        return;
    }

    for (auto& Pair : RenderContexts)
    {
        NormalizeRenderContextState(Pair.Value);
        ResolveRenderContext(Pair.Value);
    }

    PrepareMappingsForRuntime(false);

    TSet<FString> SurfacesWithResolvedContext;

    for (auto& MappingPair : Mappings)
    {
        FRshipContentMappingState& MappingState = MappingPair.Value;
        MappingState.LastError.Empty();
        const uint32 MappingConfigHash = MappingState.Config.IsValid() ? HashJsonPayload(MappingState.Config) : 0;

        const bool bFeedV2 = IsFeedV2Mapping(MappingState);

        if (!MappingState.bEnabled)
        {
            continue;
        }

        const FRshipRenderContextState* ContextState = ResolveEffectiveContextState(MappingState, bFeedV2);
        if ((CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0)
            && bFeedV2 && (!ContextState || !ContextState->ResolvedTexture))
        {
            ContextState = ResolveEffectiveContextState(MappingState, false);
        }
        if (!bFeedV2 && !ContextState)
        {
            MappingState.LastError = TEXT("Render context not available");
        }

        const bool bContextHasTexture = bFeedV2 || (ContextState && ContextState->ResolvedTexture);
        bool bAnySurfaceApplied = false;
        if (!bFeedV2 && ContextState && !bContextHasTexture && MappingState.LastError.IsEmpty())
        {
            MappingState.LastError = ContextState->LastError.IsEmpty()
                ? TEXT("Render context has no texture")
                : ContextState->LastError;
        }

        const TArray<FString>& EffectiveSurfaceIds = GetEffectiveSurfaceIds(MappingState);
        if (EffectiveSurfaceIds.Num() == 0 && MappingState.LastError.IsEmpty())
        {
            MappingState.LastError = TEXT("No mapping surfaces assigned");
        }

        for (const FString& SurfaceId : EffectiveSurfaceIds)
        {
            if (!bFeedV2 && !bContextHasTexture && SurfacesWithResolvedContext.Contains(SurfaceId))
            {
                continue;
            }

            FRshipMappingSurfaceState* SurfaceState = MappingSurfaces.Find(SurfaceId);
            if (SurfaceState && SurfaceState->bEnabled)
            {
                ApplyMappingToSurface(MappingState, *SurfaceState, ContextState, MappingConfigHash);
                if (!SurfaceState->LastError.IsEmpty())
                {
                    if (MappingState.LastError.IsEmpty())
                    {
                        const FString SurfaceLabel = SurfaceState->Name.IsEmpty() ? SurfaceState->Id : SurfaceState->Name;
                        MappingState.LastError = FString::Printf(TEXT("Screen '%s': %s"), *SurfaceLabel, *SurfaceState->LastError);
                    }
                }
                else
                {
                    bAnySurfaceApplied = true;
                }

                if (!bFeedV2 && bContextHasTexture)
                {
                    SurfacesWithResolvedContext.Add(SurfaceId);
                }
            }
            else if (MappingState.LastError.IsEmpty())
            {
                MappingState.LastError = TEXT("Mapping surface not found");
            }
        }

        if (!bAnySurfaceApplied && MappingState.LastError.IsEmpty())
        {
            MappingState.LastError = TEXT("No screens could be applied");
        }

        EmitMappingState(MappingState);
    }
}

void URshipContentMappingManager::RestoreSurfaceMaterials(FRshipMappingSurfaceState& SurfaceState)
{
    UMeshComponent* Mesh = SurfaceState.MeshComponent.Get();
    if (!IsMeshReadyForMaterialMutation(Mesh))
    {
        SurfaceState.MaterialInstances.Empty();
        SurfaceState.OriginalMaterials.Empty();
        SurfaceState.MaterialBindingHashes.Empty();
        SurfaceState.MeshComponent.Reset();
        return;
    }

    const int32 SlotCount = Mesh->GetNumMaterials();
    auto NeutralizeMappingMidAtSlot = [&](int32 SlotIndex)
    {
        if (SlotIndex < 0 || SlotIndex >= SlotCount)
        {
            return;
        }

        UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(SlotIndex));
        if (!MID)
        {
            return;
        }

        MID->SetTextureParameterValue(ParamContextTexture, nullptr);
        MID->SetTextureParameterValue(ParamContextTextureAliasSlateUI, nullptr);
        MID->SetTextureParameterValue(ParamContextTextureAliasTexture, nullptr);
        MID->SetTextureParameterValue(ParamContextDepthTexture, nullptr);
        MID->SetScalarParameterValue(ParamMappingIntensity, 0.0f);
        MID->SetScalarParameterValue(ParamMappingMode, 0.0f);
        MID->SetScalarParameterValue(ParamProjectionType, 0.0f);
    };

    TSet<int32> RestoredSlots;
    for (const auto& Pair : SurfaceState.OriginalMaterials)
    {
        if (Pair.Key < 0 || Pair.Key >= SlotCount)
        {
            continue;
        }

        if (!IsMeshReadyForMaterialMutation(Mesh))
        {
            break;
        }

        UMaterialInterface* OriginalMaterial = Pair.Value.Get();
        if (!OriginalMaterial || !IsValid(OriginalMaterial))
        {
            NeutralizeMappingMidAtSlot(Pair.Key);
            continue;
        }

        if (OriginalMaterial->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || OriginalMaterial->IsUnreachable())
        {
            NeutralizeMappingMidAtSlot(Pair.Key);
            continue;
        }

        Mesh->SetMaterial(Pair.Key, OriginalMaterial);
        RestoredSlots.Add(Pair.Key);
    }

    TArray<int32> CandidateSlots = SurfaceState.MaterialSlots;
    if (CandidateSlots.Num() == 0)
    {
        CandidateSlots.Reserve(SlotCount);
        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            CandidateSlots.Add(SlotIndex);
        }
    }

    for (int32 SlotIndex : CandidateSlots)
    {
        if (RestoredSlots.Contains(SlotIndex))
        {
            continue;
        }
        NeutralizeMappingMidAtSlot(SlotIndex);
    }

    SurfaceState.MaterialInstances.Empty();
    SurfaceState.OriginalMaterials.Empty();
    SurfaceState.MaterialBindingHashes.Empty();
}

void URshipContentMappingManager::ApplyMappingToSurface(
    const FRshipContentMappingState& MappingState,
    FRshipMappingSurfaceState& SurfaceState,
    const FRshipRenderContextState* ContextState,
    uint32 MappingConfigHash,
    double* OutApplyMs,
    int32* OutMaterialBindingsUpdated,
    int32* OutMaterialBindingsSkipped)
{
    const double ApplyStartSeconds = OutApplyMs ? FPlatformTime::Seconds() : 0.0;
    auto FinishApplyTiming = [&]()
    {
        if (OutApplyMs)
        {
            *OutApplyMs += (FPlatformTime::Seconds() - ApplyStartSeconds) * 1000.0;
        }
    };

    SurfaceState.LastError.Empty();

    UMeshComponent* Mesh = SurfaceState.MeshComponent.Get();
    if (!IsMeshReadyForMaterialMutation(Mesh))
    {
        SurfaceState.LastError = TEXT("Mesh component not resolved");
        FinishApplyTiming();
        return;
    }

    RunRuntimePreflight(/*bForceMaterialResolve=*/false);
    UMaterialInterface* BaseMaterial = nullptr;
    FString MaterialResolveError;
    if (!ResolveMaterialForMapping(MappingState, BaseMaterial, MaterialResolveError) || !BaseMaterial)
    {
        RestoreSurfaceMaterials(SurfaceState);
        SurfaceState.LastError = MaterialResolveError.IsEmpty()
            ? TEXT("No compatible mapping material available for mapping profile")
            : MaterialResolveError;
        FinishApplyTiming();
        return;
    }

    const int32 SlotCount = Mesh->GetNumMaterials();
    if (SlotCount <= 0)
    {
        SurfaceState.LastError = TEXT("Mesh has no material slots");
        UE_LOG(LogRshipExec, Warning, TEXT("ApplyMappingToSurface[%s]: mesh '%s' has no material slots"),
            *SurfaceState.Id, *Mesh->GetName());
        FinishApplyTiming();
        return;
    }

    const bool bUseFeedV2 = IsFeedV2Mapping(MappingState);
    const bool bAllowSemanticFallbacks = CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0;
    FString FeedCompositeError;
    UTexture* FeedCompositeTexture = nullptr;
    UTexture* EffectiveTexture = ContextState ? ContextState->ResolvedTexture : nullptr;
    if (bUseFeedV2)
    {
        FeedCompositeTexture = BuildFeedCompositeTextureForSurface(MappingState, SurfaceState, FeedCompositeError);
        EffectiveTexture = FeedCompositeTexture;
        if (bAllowSemanticFallbacks && !EffectiveTexture && ContextState && ContextState->ResolvedTexture)
        {
            // Feed can still fall back to the mapping context when route composition fails.
            EffectiveTexture = ContextState->ResolvedTexture;
        }
        if (!FeedCompositeError.IsEmpty())
        {
            const TCHAR* LogLabel = bAllowSemanticFallbacks ? TEXT("Feed composite fallback") : TEXT("Feed composite failure");
            UE_LOG(LogRshipExec, Warning, TEXT("%s map=%s surf=%s reason=%s"),
                LogLabel, *MappingState.Id, *SurfaceState.Id, *FeedCompositeError);
        }
    }

    const bool bHasTexture = EffectiveTexture != nullptr;
    if (!bHasTexture)
    {
        RestoreSurfaceMaterials(SurfaceState);
        SurfaceState.LastError = bUseFeedV2
            ? (FeedCompositeError.IsEmpty() ? TEXT("No feed source texture available") : FeedCompositeError)
            : TEXT("Render context has no texture");
        static double LastMissingTextureWarningTimeSeconds = 0.0;
        const double NowSeconds = FPlatformTime::Seconds();
        if ((NowSeconds - LastMissingTextureWarningTimeSeconds) >= 1.0)
        {
            LastMissingTextureWarningTimeSeconds = NowSeconds;
            UE_LOG(LogRshipExec, Warning,
                TEXT("Mapping texture missing map=%s surf=%s ctx=%s feed=%d ctxErr='%s' surfErr='%s'"),
                *MappingState.Id,
                *SurfaceState.Id,
                ContextState ? *ContextState->Id : TEXT("<none>"),
                bUseFeedV2 ? 1 : 0,
                ContextState ? *ContextState->LastError : TEXT(""),
                *SurfaceState.LastError);
        }
        FinishApplyTiming();
        return;
    }
    UE_LOG(LogRshipExec, VeryVerbose, TEXT("ApplyMappingToSurface map=%s surf=%s mesh=%s slots=%d hasContext=%d hasTexture=%d"),
        *MappingState.Id,
        *SurfaceState.Id,
        *Mesh->GetName(),
        SlotCount,
        ContextState ? 1 : 0,
        bHasTexture ? 1 : 0);

    uint32 BaseBindingHash = HashCombineFast(GetTypeHash(MappingState.Id), GetTypeHash(MappingState.Type));
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(MappingState.ContextId));
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(MappingState.Opacity));
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(MappingState.bEnabled));
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(SurfaceState.Id));
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(SurfaceState.UVChannel));
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(bUseFeedV2));
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(bCoveragePreviewEnabled));
    BaseBindingHash = HashCombineFast(BaseBindingHash, PointerHash(BaseMaterial));
    if (MappingState.Config.IsValid())
    {
        BaseBindingHash = HashCombineFast(BaseBindingHash, MappingConfigHash);
    }
    if (FeedCompositeTexture)
    {
        BaseBindingHash = HashCombineFast(BaseBindingHash, PointerHash(FeedCompositeTexture));
    }
    if (EffectiveTexture)
    {
        BaseBindingHash = HashCombineFast(BaseBindingHash, PointerHash(EffectiveTexture));
    }
    if (ContextState && ContextState->ResolvedDepthTexture)
    {
        BaseBindingHash = HashCombineFast(BaseBindingHash, PointerHash(ContextState->ResolvedDepthTexture));
    }

    for (int32 SlotIndex : SurfaceState.MaterialSlots)
    {
        if (SlotIndex < 0 || SlotIndex >= SlotCount)
        {
            SurfaceState.LastError = TEXT("Invalid material slot");
            continue;
        }

        if (!SurfaceState.OriginalMaterials.Contains(SlotIndex))
        {
            SurfaceState.OriginalMaterials.Add(SlotIndex, Mesh->GetMaterial(SlotIndex));
        }

        UMaterialInstanceDynamic* MID = nullptr;
        if (UMaterialInstanceDynamic** Existing = SurfaceState.MaterialInstances.Find(SlotIndex))
        {
            MID = *Existing;
            if (!MID
                || !IsValid(MID)
                || MID->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)
                || MID->IsUnreachable()
                || MID->GetOuter() != Mesh)
            {
                SurfaceState.MaterialInstances.Remove(SlotIndex);
                SurfaceState.MaterialBindingHashes.Remove(SlotIndex);
                MID = nullptr;
            }
            else if (MID->Parent != BaseMaterial)
            {
                // Mapping profile/material changed; recreate MID so the new shader contract is applied.
                SurfaceState.MaterialInstances.Remove(SlotIndex);
                SurfaceState.MaterialBindingHashes.Remove(SlotIndex);
                MID = nullptr;
            }
        }

        bool bMaterialRebound = false;
        if (!MID)
        {
            MID = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh);
            if (!MID || !IsValid(MID))
            {
                SurfaceState.MaterialInstances.Remove(SlotIndex);
                SurfaceState.MaterialBindingHashes.Remove(SlotIndex);
                SurfaceState.LastError = TEXT("Failed to create dynamic material instance");
                continue;
            }
            SurfaceState.MaterialInstances.Add(SlotIndex, MID);
            Mesh->SetMaterial(SlotIndex, MID);
            bMaterialRebound = true;
        }

        // Keep mapping MID ownership resilient against external material rebinds
        // (PIE BeginPlay scripts, sequencer tracks, blueprint overrides, etc.).
        if (Mesh->GetMaterial(SlotIndex) != MID)
        {
            Mesh->SetMaterial(SlotIndex, MID);
            bMaterialRebound = true;
        }

        const uint32 SlotBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(SlotIndex));
        if (const uint32* ExistingHash = SurfaceState.MaterialBindingHashes.Find(SlotIndex))
        {
            if (*ExistingHash == SlotBindingHash && !bMaterialRebound)
            {
                if (OutMaterialBindingsSkipped)
                {
                    ++(*OutMaterialBindingsSkipped);
                }
                continue;
            }
        }

        ApplyMaterialParameters(MID, MappingState, SurfaceState, ContextState, bUseFeedV2);
        if (bUseFeedV2 && MID)
        {
            MID->SetTextureParameterValue(ParamContextTexture, EffectiveTexture);
            MID->SetTextureParameterValue(ParamContextTextureAliasSlateUI, EffectiveTexture);
            MID->SetTextureParameterValue(ParamContextTextureAliasTexture, EffectiveTexture);
        }
        SurfaceState.MaterialBindingHashes.Add(SlotIndex, SlotBindingHash);
        if (OutMaterialBindingsUpdated)
        {
            ++(*OutMaterialBindingsUpdated);
        }
    }

    FinishApplyTiming();
}

void URshipContentMappingManager::ApplyMaterialParameters(
    UMaterialInstanceDynamic* MID,
    const FRshipContentMappingState& MappingState,
    const FRshipMappingSurfaceState& SurfaceState,
    const FRshipRenderContextState* ContextState,
    bool bUseFeedV2)
{
    if (!MID)
    {
        return;
    }

    const float MappingIntensity = MappingState.bEnabled ? FMath::Clamp(MappingState.Opacity, 0.0f, 1.0f) : 0.0f;
    MID->SetScalarParameterValue(ParamMappingIntensity, MappingIntensity);
    MID->SetScalarParameterValue(ParamOpacity, 1.0f);
    MID->SetVectorParameterValue(ParamPreviewTint, FLinearColor::White);

    MID->SetScalarParameterValue(ParamUVChannel, static_cast<float>(SurfaceState.UVChannel));

    if (bCoveragePreviewEnabled)
    {
        MID->SetScalarParameterValue(ParamDebugCoverage, 1.0f);
        MID->SetVectorParameterValue(ParamDebugUnmappedColor, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
        MID->SetVectorParameterValue(ParamDebugMappedColor, FLinearColor::White);
    }
    else
    {
        MID->SetScalarParameterValue(ParamDebugCoverage, 0.0f);
    }

    UTexture* ResolvedContextTexture = nullptr;
    if (ContextState && ContextState->ResolvedTexture)
    {
        ResolvedContextTexture = ContextState->ResolvedTexture;
    }

    MID->SetTextureParameterValue(ParamContextTexture, ResolvedContextTexture);
    MID->SetTextureParameterValue(ParamContextTextureAliasSlateUI, ResolvedContextTexture);
    MID->SetTextureParameterValue(ParamContextTextureAliasTexture, ResolvedContextTexture);

    UTexture* ResolvedDepthTexture = nullptr;
    if (ContextState && ContextState->ResolvedDepthTexture)
    {
        ResolvedDepthTexture = ContextState->ResolvedDepthTexture;
    }
    else
    {
        // Keep depth-driven materials stable when depth capture is disabled.
        ResolvedDepthTexture = ResolvedContextTexture;
    }

    MID->SetTextureParameterValue(ParamContextDepthTexture, ResolvedDepthTexture);

    const bool bIsUvMapping = MappingState.Type == TEXT("surface-uv")
        || MappingState.Type.Equals(TEXT("direct"), ESearchCase::IgnoreCase)
        || MappingState.Type.Equals(TEXT("feed"), ESearchCase::IgnoreCase)
        || MappingState.Type.Equals(TEXT("surface-feed"), ESearchCase::IgnoreCase);
	    const bool bIsProjectionMapping = MappingState.Type == TEXT("surface-projection")
	        || MappingState.Type.Equals(TEXT("perspective"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("cylindrical"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("spherical"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("parallel"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("radial"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("mesh"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("fisheye"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("camera-plate"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("spatial"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("depth-map"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("custom-matrix"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("custom matrix"), ESearchCase::IgnoreCase)
	        || MappingState.Type.Equals(TEXT("matrix"), ESearchCase::IgnoreCase);

    if (!bIsUvMapping && !bIsProjectionMapping)
    {
        MID->SetScalarParameterValue(ParamMappingMode, 0.0f);
        MID->SetScalarParameterValue(ParamProjectionType, 0.0f);
        MID->SetVectorParameterValue(ParamUVTransform, FLinearColor(1.0f, 1.0f, 0.0f, 0.0f));
        MID->SetScalarParameterValue(ParamUVRotation, 0.0f);
        MID->SetScalarParameterValue(ParamUVScaleU, 1.0f);
        MID->SetScalarParameterValue(ParamUVScaleV, 1.0f);
        MID->SetScalarParameterValue(ParamUVOffsetU, 0.0f);
        MID->SetScalarParameterValue(ParamUVOffsetV, 0.0f);
    }

    if (bIsUvMapping)
    {
        MID->SetScalarParameterValue(ParamMappingMode, 0.0f);
        MID->SetScalarParameterValue(ParamProjectionType, 0.0f);

        if (!MappingState.Config.IsValid())
        {
            MID->SetVectorParameterValue(ParamUVTransform, FLinearColor(1.0f, 1.0f, 0.0f, 0.0f));
            MID->SetScalarParameterValue(ParamUVRotation, 0.0f);
            MID->SetScalarParameterValue(ParamUVScaleU, 1.0f);
            MID->SetScalarParameterValue(ParamUVScaleV, 1.0f);
            MID->SetScalarParameterValue(ParamUVOffsetU, 0.0f);
            MID->SetScalarParameterValue(ParamUVOffsetV, 0.0f);
            return;
        }

        float ScaleU = 1.0f;
        float ScaleV = 1.0f;
        float OffsetU = 0.0f;
        float OffsetV = 0.0f;
        float Rotation = 0.0f;
        float PivotU = 0.5f;
        float PivotV = 0.5f;
        bool bFeedMode = false;
        float FeedU = 0.0f;
        float FeedV = 0.0f;
        float FeedW = 1.0f;
        float FeedH = 1.0f;

        if (MappingState.Type.Equals(TEXT("feed"), ESearchCase::IgnoreCase)
            || MappingState.Type.Equals(TEXT("surface-feed"), ESearchCase::IgnoreCase))
        {
            bFeedMode = true;
        }

        if (bUseFeedV2)
        {
            bFeedMode = false;
        }

        if (MappingState.Config.IsValid())
        {
            if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("uvTransform")))
            {
                TSharedPtr<FJsonObject> Transform = MappingState.Config->GetObjectField(TEXT("uvTransform"));
                ScaleU = GetNumberField(Transform, TEXT("scaleU"), 1.0f);
                ScaleV = GetNumberField(Transform, TEXT("scaleV"), 1.0f);
                OffsetU = GetNumberField(Transform, TEXT("offsetU"), 0.0f);
                OffsetV = GetNumberField(Transform, TEXT("offsetV"), 0.0f);
                Rotation = GetNumberField(Transform, TEXT("rotationDeg"), 0.0f);
                PivotU = GetNumberField(Transform, TEXT("pivotU"), 0.5f);
                PivotV = GetNumberField(Transform, TEXT("pivotV"), 0.5f);
            }

            const FString UvMode = GetStringField(MappingState.Config, TEXT("uvMode"), TEXT(""));
            if (!bUseFeedV2 && UvMode.Equals(TEXT("feed"), ESearchCase::IgnoreCase))
            {
                bFeedMode = true;
            }
        }

        if (bFeedMode)
        {
            const float SafeW = FMath::Max(0.0001f, FeedW);
            const float SafeH = FMath::Max(0.0001f, FeedH);
            ScaleU *= SafeW;
            ScaleV *= SafeH;
            OffsetU = FeedU + (OffsetU * SafeW);
            OffsetV = FeedV + (OffsetV * SafeH);
        }

        OffsetU = OffsetU - PivotU + 0.5f;
        OffsetV = OffsetV - PivotV + 0.5f;

        MID->SetVectorParameterValue(
            ParamUVTransform,
            FLinearColor(ScaleU, ScaleV, OffsetU, OffsetV));
        MID->SetScalarParameterValue(ParamUVRotation, Rotation);
        MID->SetScalarParameterValue(ParamUVScaleU, ScaleU);
        MID->SetScalarParameterValue(ParamUVScaleV, ScaleV);
        MID->SetScalarParameterValue(ParamUVOffsetU, OffsetU);
        MID->SetScalarParameterValue(ParamUVOffsetV, OffsetV);
        return;
    }

    if (bIsProjectionMapping)
    {
        MID->SetScalarParameterValue(ParamMappingMode, 1.0f);

        FString ProjectionType = TEXT("perspective");
	        if (MappingState.Type.Equals(TEXT("cylindrical"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("spherical"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("perspective"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("parallel"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("radial"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("mesh"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("fisheye"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("camera-plate"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("spatial"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("depth-map"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("custom-matrix"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("custom matrix"), ESearchCase::IgnoreCase)
	            || MappingState.Type.Equals(TEXT("matrix"), ESearchCase::IgnoreCase))
	        {
            ProjectionType = MappingState.Type;
        }
        FVector Position(0.0f, 0.0f, 0.0f);
        FVector Rotation(0.0f, 0.0f, 0.0f);
        float Fov = 60.0f;
        float Aspect = 1.7778f;
        float Near = 10.0f;
        float Far = 10000.0f;
        bool bAspectProvided = false;

        if (MappingState.Config.IsValid())
        {
            ProjectionType = GetStringField(MappingState.Config, TEXT("projectionType"), ProjectionType);
            if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("projectorPosition")))
            {
                TSharedPtr<FJsonObject> PosObj = MappingState.Config->GetObjectField(TEXT("projectorPosition"));
                Position.X = GetNumberField(PosObj, TEXT("x"), 0.0f);
                Position.Y = GetNumberField(PosObj, TEXT("y"), 0.0f);
                Position.Z = GetNumberField(PosObj, TEXT("z"), 0.0f);
            }
            if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("projectorRotation")))
            {
                TSharedPtr<FJsonObject> RotObj = MappingState.Config->GetObjectField(TEXT("projectorRotation"));
                Rotation.X = GetNumberField(RotObj, TEXT("x"), 0.0f);
                Rotation.Y = GetNumberField(RotObj, TEXT("y"), 0.0f);
                Rotation.Z = GetNumberField(RotObj, TEXT("z"), 0.0f);
            }
            Fov = GetNumberField(MappingState.Config, TEXT("fov"), Fov);
            if (MappingState.Config->HasTypedField<EJson::Number>(TEXT("aspectRatio")))
            {
                Aspect = GetNumberField(MappingState.Config, TEXT("aspectRatio"), Aspect);
                bAspectProvided = true;
            }
            Near = GetNumberField(MappingState.Config, TEXT("near"), Near);
            Far = GetNumberField(MappingState.Config, TEXT("far"), Far);
        }
        ProjectionType = NormalizeProjectionModeToken(ProjectionType, TEXT("perspective"));

        FVector ProjectionEyepoint = Position;
        bool bHasProjectionEyepoint = false;
        if (MappingState.Config.IsValid()
            && MappingState.Config->HasTypedField<EJson::Object>(TEXT("eyepoint")))
        {
            const TSharedPtr<FJsonObject> EpObj = MappingState.Config->GetObjectField(TEXT("eyepoint"));
            ProjectionEyepoint.X = GetNumberField(EpObj, TEXT("x"), Position.X);
            ProjectionEyepoint.Y = GetNumberField(EpObj, TEXT("y"), Position.Y);
            ProjectionEyepoint.Z = GetNumberField(EpObj, TEXT("z"), Position.Z);
            bHasProjectionEyepoint = true;

            // All projection modes may define a dedicated eyepoint origin.
            Position = ProjectionEyepoint;
        }

        bool bHasCustomProjectionMatrix = false;
        FMatrix CustomProjectionMatrix = FMatrix::Identity;
        if (MappingState.Config.IsValid())
        {
            TSharedPtr<FJsonObject> MatrixObj;
            if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("customProjectionMatrix")))
            {
                MatrixObj = MappingState.Config->GetObjectField(TEXT("customProjectionMatrix"));
            }
            else if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("matrix")))
            {
                MatrixObj = MappingState.Config->GetObjectField(TEXT("matrix"));
            }

            if (MatrixObj.IsValid())
            {
                auto ReadMatrixElement = [this, MatrixObj](int32 Row, int32 Col, float DefaultValue) -> float
                {
                    const FString FieldName = FString::Printf(TEXT("m%d%d"), Row, Col);
                    return GetNumberField(MatrixObj, FieldName, DefaultValue);
                };

                for (int32 Row = 0; Row < 4; ++Row)
                {
                    for (int32 Col = 0; Col < 4; ++Col)
                    {
                        const float DefaultValue = (Row == Col) ? 1.0f : 0.0f;
                        CustomProjectionMatrix.M[Row][Col] = ReadMatrixElement(Row, Col, DefaultValue);
                    }
                }
                bHasCustomProjectionMatrix = true;
            }
        }

        float ProjectionTypeIndex = 0.0f;
        if (ProjectionType.Equals(TEXT("cylindrical"), ESearchCase::IgnoreCase))
        {
            ProjectionTypeIndex = 1.0f;
        }
        else if (ProjectionType.Equals(TEXT("planar"), ESearchCase::IgnoreCase))
        {
            ProjectionTypeIndex = 2.0f;
        }
        else if (ProjectionType.Equals(TEXT("spherical"), ESearchCase::IgnoreCase))
        {
            ProjectionTypeIndex = 3.0f;
        }
        else if (ProjectionType.Equals(TEXT("parallel"), ESearchCase::IgnoreCase))
        {
            ProjectionTypeIndex = 4.0f;
        }
        else if (ProjectionType.Equals(TEXT("radial"), ESearchCase::IgnoreCase))
        {
            ProjectionTypeIndex = 5.0f;
        }
        else if (ProjectionType.Equals(TEXT("mesh"), ESearchCase::IgnoreCase))
        {
            ProjectionTypeIndex = 6.0f;
        }
        else if (ProjectionType.Equals(TEXT("fisheye"), ESearchCase::IgnoreCase))
        {
            ProjectionTypeIndex = 7.0f;
        }
	        else if (ProjectionType.Equals(TEXT("custom-matrix"), ESearchCase::IgnoreCase)
	            || ProjectionType.Equals(TEXT("custom matrix"), ESearchCase::IgnoreCase)
	            || ProjectionType.Equals(TEXT("matrix"), ESearchCase::IgnoreCase))
	        {
	            ProjectionTypeIndex = 8.0f;
	        }
	        else if (ProjectionType.Equals(TEXT("camera-plate"), ESearchCase::IgnoreCase))
	        {
	            ProjectionTypeIndex = 9.0f;
	        }
	        else if (ProjectionType.Equals(TEXT("spatial"), ESearchCase::IgnoreCase))
	        {
	            ProjectionTypeIndex = 10.0f;
	        }
	        else if (ProjectionType.Equals(TEXT("depth-map"), ESearchCase::IgnoreCase))
	        {
	            ProjectionTypeIndex = 11.0f;
	        }

        MID->SetScalarParameterValue(ParamProjectionType, ProjectionTypeIndex);
        MID->SetScalarParameterValue(ParamRadialFlag, 0.0f);

        float CameraPlateFit = 0.0f; // 0=contain,1=cover,2=stretch
        float CameraPlateAnchorU = 0.5f;
        float CameraPlateAnchorV = 0.5f;
        float CameraPlateFlipV = 0.0f;
        float SpatialScaleU = 1.0f;
        float SpatialScaleV = 1.0f;
        float SpatialOffsetU = 0.0f;
        float SpatialOffsetV = 0.0f;
        float DepthScale = 1.0f;
        float DepthBias = 0.0f;
        float DepthNearParam = 0.0f;
        float DepthFarParam = 1.0f;
        MID->SetVectorParameterValue(ParamCylinderParams, FLinearColor(0.0f, 0.0f, 1.0f, 500.0f));
        MID->SetVectorParameterValue(ParamCylinderExtent, FLinearColor(1000.0f, 0.0f, 360.0f, 0.0f));
        MID->SetVectorParameterValue(ParamSphereParams, FLinearColor(Position.X, Position.Y, Position.Z, 500.0f));
        MID->SetVectorParameterValue(ParamSphereArc, FLinearColor(360.0f, 180.0f, 0.0f, 0.0f));
        MID->SetVectorParameterValue(ParamParallelSize, FLinearColor(1000.0f, 1000.0f, 0.0f, 0.0f));
        MID->SetVectorParameterValue(ParamMeshEyepoint, FLinearColor(Position.X, Position.Y, Position.Z, 0.0f));
        MID->SetVectorParameterValue(ParamFisheyeParams, FLinearColor(180.0f, 0.0f, 0.0f, 0.0f));

        if (MappingState.Config.IsValid())
        {
            if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("cameraPlate")))
            {
                const TSharedPtr<FJsonObject> CameraPlate = MappingState.Config->GetObjectField(TEXT("cameraPlate"));
                const FString Fit = GetStringField(CameraPlate, TEXT("fit"), TEXT("contain"));
                if (Fit.Equals(TEXT("cover"), ESearchCase::IgnoreCase) || Fit.Equals(TEXT("fill"), ESearchCase::IgnoreCase))
                {
                    CameraPlateFit = 1.0f;
                }
                else if (Fit.Equals(TEXT("stretch"), ESearchCase::IgnoreCase))
                {
                    CameraPlateFit = 2.0f;
                }

                auto DecodeAnchor = [](const FString& Anchor, float& OutU, float& OutV)
                {
                    FString Value = Anchor.TrimStartAndEnd().ToLower();
                    if (Value.IsEmpty() || Value == TEXT("center"))
                    {
                        OutU = 0.5f;
                        OutV = 0.5f;
                        return;
                    }

                    if (Value.Contains(TEXT("left"))) OutU = 0.0f;
                    else if (Value.Contains(TEXT("right"))) OutU = 1.0f;
                    else OutU = 0.5f;

                    if (Value.Contains(TEXT("top"))) OutV = 0.0f;
                    else if (Value.Contains(TEXT("bottom"))) OutV = 1.0f;
                    else OutV = 0.5f;
                };

                DecodeAnchor(GetStringField(CameraPlate, TEXT("anchor"), TEXT("center")), CameraPlateAnchorU, CameraPlateAnchorV);
                CameraPlateFlipV = GetBoolField(CameraPlate, TEXT("flipV"), false) ? 1.0f : 0.0f;
            }

            if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("spatial")))
            {
                const TSharedPtr<FJsonObject> Spatial = MappingState.Config->GetObjectField(TEXT("spatial"));
                SpatialScaleU = GetNumberField(Spatial, TEXT("scaleU"), SpatialScaleU);
                SpatialScaleV = GetNumberField(Spatial, TEXT("scaleV"), SpatialScaleV);
                SpatialOffsetU = GetNumberField(Spatial, TEXT("offsetU"), SpatialOffsetU);
                SpatialOffsetV = GetNumberField(Spatial, TEXT("offsetV"), SpatialOffsetV);
            }

            if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("depthMap")))
            {
                const TSharedPtr<FJsonObject> DepthMap = MappingState.Config->GetObjectField(TEXT("depthMap"));
                DepthScale = GetNumberField(DepthMap, TEXT("depthScale"), DepthScale);
                DepthBias = GetNumberField(DepthMap, TEXT("depthBias"), DepthBias);
                DepthNearParam = GetNumberField(DepthMap, TEXT("depthNear"), DepthNearParam);
                DepthFarParam = GetNumberField(DepthMap, TEXT("depthFar"), DepthFarParam);
            }
            DepthScale = GetNumberField(MappingState.Config, TEXT("depthScale"), DepthScale);
            DepthBias = GetNumberField(MappingState.Config, TEXT("depthBias"), DepthBias);
            DepthNearParam = GetNumberField(MappingState.Config, TEXT("depthNear"), DepthNearParam);
            DepthFarParam = GetNumberField(MappingState.Config, TEXT("depthFar"), DepthFarParam);
        }

        MID->SetVectorParameterValue(ParamCameraPlateParams, FLinearColor(CameraPlateFit, CameraPlateAnchorU, CameraPlateAnchorV, CameraPlateFlipV));
        MID->SetVectorParameterValue(ParamSpatialParams0, FLinearColor(SpatialScaleU, SpatialScaleV, SpatialOffsetU, SpatialOffsetV));
        MID->SetVectorParameterValue(ParamSpatialParams1, FLinearColor(Position.X, Position.Y, Position.Z, 0.0f));
        MID->SetVectorParameterValue(ParamDepthMapParams, FLinearColor(DepthScale, DepthBias, DepthNearParam, DepthFarParam));

        const FTransform ProjectorTransform(FRotator::MakeFromEuler(Rotation), Position);
        const FMatrix ViewMatrix = ProjectorTransform.ToInverseMatrixWithScale();

        const float FovRad = FMath::DegreesToRadians(Fov);
        const float TanHalfFov = FMath::Tan(FovRad * 0.5f);
        float SafeAspect = Aspect <= 0.01f ? 1.0f : Aspect;
        if (!bAspectProvided && ContextState && ContextState->Width > 0 && ContextState->Height > 0)
        {
            SafeAspect = static_cast<float>(ContextState->Width) / static_cast<float>(ContextState->Height);
        }
        const float SafeNear = FMath::Max(0.01f, Near);
        const float SafeFar = FMath::Max(SafeNear + 0.01f, Far);

        // Build projection matrix based on type
        FMatrix Projection = FMatrix::Identity;

        if (ProjectionTypeIndex == 4.0f) // Parallel (orthographic)
        {
            float ParallelW = 1000.0f;
            float ParallelH = 1000.0f;
            if (MappingState.Config.IsValid())
            {
                ParallelW = GetNumberField(MappingState.Config, TEXT("sizeW"), ParallelW);
                ParallelH = GetNumberField(MappingState.Config, TEXT("sizeH"), ParallelH);
            }
            const float HalfW = ParallelW * 0.5f;
            const float HalfH = ParallelH * 0.5f;
            const float Depth = SafeFar - SafeNear;
            // Orthographic projection matrix
            Projection.M[0][0] = 1.0f / HalfW;
            Projection.M[1][1] = 1.0f / HalfH;
            Projection.M[2][2] = 1.0f / Depth;
            Projection.M[3][2] = -SafeNear / Depth;
            Projection.M[2][3] = 0.0f;
            Projection.M[3][3] = 1.0f;
            MID->SetVectorParameterValue(ParamParallelSize, FLinearColor(ParallelW, ParallelH, 0.0f, 0.0f));
        }
        else if (ProjectionTypeIndex == 8.0f && bHasCustomProjectionMatrix)
        {
            Projection = CustomProjectionMatrix;
        }
        else
        {
            // Perspective projection for perspective, cylindrical, spherical, radial, mesh, fisheye
            Projection.M[0][0] = 1.0f / (TanHalfFov * SafeAspect);
            Projection.M[1][1] = 1.0f / TanHalfFov;
            Projection.M[2][2] = SafeFar / (SafeFar - SafeNear);
            Projection.M[2][3] = 1.0f;
            Projection.M[3][2] = (-SafeNear * SafeFar) / (SafeFar - SafeNear);
            Projection.M[3][3] = 0.0f;
        }

        const FMatrix ViewProjection = ViewMatrix * Projection;

        MID->SetVectorParameterValue(
            ParamProjectorRow0,
            FLinearColor(
                ViewProjection.M[0][0],
                ViewProjection.M[0][1],
                ViewProjection.M[0][2],
                ViewProjection.M[0][3]));
        MID->SetVectorParameterValue(
            ParamProjectorRow1,
            FLinearColor(
                ViewProjection.M[1][0],
                ViewProjection.M[1][1],
                ViewProjection.M[1][2],
                ViewProjection.M[1][3]));
        MID->SetVectorParameterValue(
            ParamProjectorRow2,
            FLinearColor(
                ViewProjection.M[2][0],
                ViewProjection.M[2][1],
                ViewProjection.M[2][2],
                ViewProjection.M[2][3]));
        MID->SetVectorParameterValue(
            ParamProjectorRow3,
            FLinearColor(
                ViewProjection.M[3][0],
                ViewProjection.M[3][1],
                ViewProjection.M[3][2],
                ViewProjection.M[3][3]));

        // Cylindrical-specific params
        if (ProjectionTypeIndex == 1.0f || ProjectionTypeIndex == 5.0f) // Cylindrical or Radial
        {
            FVector CylAxis(0.0f, 0.0f, 1.0f);
            float CylRadius = 500.0f;
            float CylHeight = 1000.0f;
            float ArcStart = 0.0f;
            float ArcEnd = 360.0f;
            float EmitDir = 0.0f; // 0=outward, 1=inward
            bool bRadial = (ProjectionTypeIndex == 5.0f);

            if (MappingState.Config.IsValid())
            {
                if (MappingState.Config->HasTypedField<EJson::Object>(TEXT("cylindrical")))
                {
                    TSharedPtr<FJsonObject> Cyl = MappingState.Config->GetObjectField(TEXT("cylindrical"));
                    const FString AxisStr = GetStringField(Cyl, TEXT("axis"), TEXT("z"));
                    if (AxisStr.Equals(TEXT("x"), ESearchCase::IgnoreCase)) CylAxis = FVector(1, 0, 0);
                    else if (AxisStr.Equals(TEXT("y"), ESearchCase::IgnoreCase)) CylAxis = FVector(0, 1, 0);
                    else CylAxis = FVector(0, 0, 1);
                    CylRadius = GetNumberField(Cyl, TEXT("radius"), CylRadius);
                    CylHeight = GetNumberField(Cyl, TEXT("height"), CylHeight);
                    ArcStart = GetNumberField(Cyl, TEXT("startAngle"), ArcStart);
                    ArcEnd = GetNumberField(Cyl, TEXT("endAngle"), ArcEnd);
                    const FString EmitStr = GetStringField(Cyl, TEXT("emitDirection"), TEXT("outward"));
                    EmitDir = EmitStr.Equals(TEXT("inward"), ESearchCase::IgnoreCase) ? 1.0f : 0.0f;
                }
                CylRadius = GetNumberField(MappingState.Config, TEXT("cylinderRadius"), CylRadius);
                CylHeight = GetNumberField(MappingState.Config, TEXT("cylinderHeight"), CylHeight);
                ArcStart = GetNumberField(MappingState.Config, TEXT("arcStart"), ArcStart);
                ArcEnd = GetNumberField(MappingState.Config, TEXT("arcEnd"), ArcEnd);
            }

            MID->SetVectorParameterValue(ParamCylinderParams, FLinearColor(CylAxis.X, CylAxis.Y, CylAxis.Z, CylRadius));
            MID->SetVectorParameterValue(ParamCylinderExtent, FLinearColor(CylHeight, ArcStart, ArcEnd, EmitDir));
            MID->SetScalarParameterValue(ParamRadialFlag, bRadial ? 1.0f : 0.0f);
        }

        // Spherical-specific params
        if (ProjectionTypeIndex == 3.0f)
        {
            float SphRadius = 500.0f;
            float HArc = 360.0f;
            float VArc = 180.0f;

            if (MappingState.Config.IsValid())
            {
                SphRadius = GetNumberField(MappingState.Config, TEXT("sphereRadius"), SphRadius);
                HArc = GetNumberField(MappingState.Config, TEXT("horizontalArc"), HArc);
                VArc = GetNumberField(MappingState.Config, TEXT("verticalArc"), VArc);
            }

            MID->SetVectorParameterValue(ParamSphereParams, FLinearColor(Position.X, Position.Y, Position.Z, SphRadius));
            MID->SetVectorParameterValue(ParamSphereArc, FLinearColor(HArc, VArc, 0.0f, 0.0f));
        }

        // Mesh-specific params
        if (ProjectionTypeIndex == 6.0f)
        {
            const FVector EffectiveEyepoint = bHasProjectionEyepoint ? ProjectionEyepoint : Position;
            MID->SetVectorParameterValue(ParamMeshEyepoint, FLinearColor(EffectiveEyepoint.X, EffectiveEyepoint.Y, EffectiveEyepoint.Z, 0.0f));
        }

        // Fisheye-specific params
        if (ProjectionTypeIndex == 7.0f)
        {
            float FisheyeFov = 180.0f;
            float LensType = 0.0f; // 0=equidistant, 1=equisolid, 2=stereographic

            if (MappingState.Config.IsValid())
            {
                FisheyeFov = GetNumberField(MappingState.Config, TEXT("fisheyeFov"), FisheyeFov);
                const FString LensStr = GetStringField(MappingState.Config, TEXT("lensType"), TEXT("equidistant"));
                if (LensStr.Equals(TEXT("equisolid"), ESearchCase::IgnoreCase)) LensType = 1.0f;
                else if (LensStr.Equals(TEXT("stereographic"), ESearchCase::IgnoreCase)) LensType = 2.0f;
            }

            MID->SetVectorParameterValue(ParamFisheyeParams, FLinearColor(FisheyeFov, LensType, 0.0f, 0.0f));
        }

        // Common projection properties: masking and border expansion
        float MaskStart = 0.0f;
        float MaskEnd = 360.0f;
        float ClipOutside = 0.0f;
        float BorderExp = 0.0f;

        if (MappingState.Config.IsValid())
        {
            MaskStart = GetNumberField(MappingState.Config, TEXT("angleMaskStart"), MaskStart);
            MaskEnd = GetNumberField(MappingState.Config, TEXT("angleMaskEnd"), MaskEnd);
            ClipOutside = GetNumberField(MappingState.Config, TEXT("clipOutsideRegion"), 0.0f);
            if (!MappingState.Config->HasTypedField<EJson::Number>(TEXT("clipOutsideRegion")))
            {
                ClipOutside = GetBoolField(MappingState.Config, TEXT("clipOutsideRegion"), false) ? 1.0f : 0.0f;
            }
            BorderExp = GetNumberField(MappingState.Config, TEXT("borderExpansion"), BorderExp);
        }

        MID->SetVectorParameterValue(ParamMaskAngle, FLinearColor(MaskStart, MaskEnd, ClipOutside, 0.0f));
        MID->SetScalarParameterValue(ParamBorderExpansion, BorderExp);
    }

    // Content mode (applies to both UV and projection mappings)
    {
        float ContentModeVal = 0.0f; // 0=stretch
        if (MappingState.Config.IsValid())
        {
            const FString ModeStr = GetStringField(MappingState.Config, TEXT("contentMode"), TEXT("stretch"));
            if (ModeStr.Equals(TEXT("crop"), ESearchCase::IgnoreCase)) ContentModeVal = 1.0f;
            else if (ModeStr.Equals(TEXT("fit"), ESearchCase::IgnoreCase)) ContentModeVal = 2.0f;
            else if (ModeStr.Equals(TEXT("pixel-perfect"), ESearchCase::IgnoreCase)) ContentModeVal = 3.0f;
        }
        MID->SetScalarParameterValue(ParamContentMode, ContentModeVal);
    }
}

void URshipContentMappingManager::RegisterAllTargets()
{
    for (const auto& Pair : RenderContexts)
    {
        RegisterContextTarget(Pair.Value);
    }
    for (const auto& Pair : MappingSurfaces)
    {
        RegisterSurfaceTarget(Pair.Value);
    }
    for (const auto& Pair : Mappings)
    {
        RegisterMappingTarget(Pair.Value);
    }
}

void URshipContentMappingManager::RegisterContextTarget(const FRshipRenderContextState& ContextState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildContextTargetId(ContextState.Id);
    URshipContentMappingTargetProxy* Proxy = EnsureTargetProxy(TargetId);
    if (!Proxy)
    {
        return;
    }

    Subsystem->EnsureAutomationTarget(TargetId, ContextState.Name.IsEmpty() ? TargetId : ContextState.Name, {});
    Subsystem->RegisterEmitterForTarget(TargetId, Proxy, GET_MEMBER_NAME_CHECKED(URshipContentMappingTargetProxy, OnState), TEXT("state"));
    Subsystem->RegisterEmitterForTarget(TargetId, Proxy, GET_MEMBER_NAME_CHECKED(URshipContentMappingTargetProxy, OnStatus), TEXT("status"));

#define REGISTER_PROXY_ACTION(FuncName, ExposedName) \
    Subsystem->RegisterFunctionActionForTarget( \
        TargetId, \
        Proxy, \
        GET_FUNCTION_NAME_CHECKED(URshipContentMappingTargetProxy, FuncName), \
        TEXT(ExposedName))

    REGISTER_PROXY_ACTION(SetEnabled, "setEnabled");
    REGISTER_PROXY_ACTION(SetSourceType, "setSourceType");
    REGISTER_PROXY_ACTION(SetCameraId, "setCameraId");
    REGISTER_PROXY_ACTION(SetAssetId, "setAssetId");
    REGISTER_PROXY_ACTION(SetExternalSourceId, "setExternalSourceId");
    REGISTER_PROXY_ACTION(SetDepthAssetId, "setDepthAssetId");
    REGISTER_PROXY_ACTION(SetDepthCaptureEnabled, "setDepthCaptureEnabled");
    REGISTER_PROXY_ACTION(SetDepthCaptureMode, "setDepthCaptureMode");
    REGISTER_PROXY_ACTION(SetResolution, "setResolution");
    REGISTER_PROXY_ACTION(SetCaptureMode, "setCaptureMode");

#undef REGISTER_PROXY_ACTION
}

void URshipContentMappingManager::RegisterSurfaceTarget(const FRshipMappingSurfaceState& SurfaceState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildSurfaceTargetId(SurfaceState.Id);
    URshipContentMappingTargetProxy* Proxy = EnsureTargetProxy(TargetId);
    if (!Proxy)
    {
        return;
    }

    Subsystem->EnsureAutomationTarget(TargetId, SurfaceState.Name.IsEmpty() ? TargetId : SurfaceState.Name, {});
    Subsystem->RegisterEmitterForTarget(TargetId, Proxy, GET_MEMBER_NAME_CHECKED(URshipContentMappingTargetProxy, OnState), TEXT("state"));
    Subsystem->RegisterEmitterForTarget(TargetId, Proxy, GET_MEMBER_NAME_CHECKED(URshipContentMappingTargetProxy, OnStatus), TEXT("status"));

#define REGISTER_PROXY_ACTION(FuncName, ExposedName) \
    Subsystem->RegisterFunctionActionForTarget( \
        TargetId, \
        Proxy, \
        GET_FUNCTION_NAME_CHECKED(URshipContentMappingTargetProxy, FuncName), \
        TEXT(ExposedName))

    REGISTER_PROXY_ACTION(SetEnabled, "setEnabled");
    REGISTER_PROXY_ACTION(SetActorPath, "setActorPath");
    REGISTER_PROXY_ACTION(SetUvChannel, "setUvChannel");
    REGISTER_PROXY_ACTION(SetMaterialSlots, "setMaterialSlots");
    REGISTER_PROXY_ACTION(SetMeshComponentName, "setMeshComponentName");

#undef REGISTER_PROXY_ACTION
}

void URshipContentMappingManager::RegisterMappingTarget(const FRshipContentMappingState& MappingState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildMappingTargetId(MappingState.Id);
    URshipContentMappingTargetProxy* Proxy = EnsureTargetProxy(TargetId);
    if (!Proxy)
    {
        return;
    }

    Subsystem->EnsureAutomationTarget(TargetId, MappingState.Name.IsEmpty() ? TargetId : MappingState.Name, {});
    Subsystem->RegisterEmitterForTarget(TargetId, Proxy, GET_MEMBER_NAME_CHECKED(URshipContentMappingTargetProxy, OnState), TEXT("state"));
    Subsystem->RegisterEmitterForTarget(TargetId, Proxy, GET_MEMBER_NAME_CHECKED(URshipContentMappingTargetProxy, OnStatus), TEXT("status"));

#define REGISTER_PROXY_ACTION(FuncName, ExposedName) \
    Subsystem->RegisterFunctionActionForTarget( \
        TargetId, \
        Proxy, \
        GET_FUNCTION_NAME_CHECKED(URshipContentMappingTargetProxy, FuncName), \
        TEXT(ExposedName))

    REGISTER_PROXY_ACTION(SetEnabled, "setEnabled");
    REGISTER_PROXY_ACTION(SetOpacity, "setOpacity");
    REGISTER_PROXY_ACTION(SetContextId, "setContextId");
    REGISTER_PROXY_ACTION(SetSurfaceIds, "setSurfaceIds");
    REGISTER_PROXY_ACTION(SetProjection, "setProjection");
    REGISTER_PROXY_ACTION(SetUVTransform, "setUVTransform");
    REGISTER_PROXY_ACTION(SetType, "setType");
    REGISTER_PROXY_ACTION(SetConfig, "setConfig");
    REGISTER_PROXY_ACTION(SetFeedV2, "setFeedV2");
    REGISTER_PROXY_ACTION(UpsertFeedSource, "upsertFeedSource");
    REGISTER_PROXY_ACTION(RemoveFeedSource, "removeFeedSource");
    REGISTER_PROXY_ACTION(UpsertFeedDestination, "upsertFeedDestination");
    REGISTER_PROXY_ACTION(RemoveFeedDestination, "removeFeedDestination");
    REGISTER_PROXY_ACTION(UpsertFeedRoute, "upsertFeedRoute");
    REGISTER_PROXY_ACTION(RemoveFeedRoute, "removeFeedRoute");

#undef REGISTER_PROXY_ACTION
}

void URshipContentMappingManager::DeleteTargetForPath(const FString& TargetPath)
{
    if (TargetPath.IsEmpty())
    {
        return;
    }

    ClearEmitterCacheForTarget(TargetPath);

    if (!Subsystem)
    {
        return;
    }

    Subsystem->RemoveAutomationTarget(TargetPath);
    TargetProxies.Remove(TargetPath);
}

FString URshipContentMappingManager::BuildContextTargetId(const FString& ContextId) const
{
    return FString::Printf(TEXT("/content-mapping/context/%s"), *ContextId);
}

FString URshipContentMappingManager::BuildSurfaceTargetId(const FString& SurfaceId) const
{
    return FString::Printf(TEXT("/content-mapping/surface/%s"), *SurfaceId);
}

FString URshipContentMappingManager::BuildMappingTargetId(const FString& MappingId) const
{
    return FString::Printf(TEXT("/content-mapping/mapping/%s"), *MappingId);
}

bool URshipContentMappingManager::PulseEmitterIfChanged(
    const FString& TargetId,
    const FString& EmitterName,
    const TSharedPtr<FJsonObject>& Payload,
    TMap<FString, uint32>& CacheStore)
{
    if (!Subsystem || TargetId.IsEmpty() || EmitterName.IsEmpty() || !Payload.IsValid())
    {
        return false;
    }

    const FString CacheKey = TargetId + TEXT(":") + EmitterName;
    const uint32 PayloadHash = HashJsonPayload(Payload);
    if (const uint32* ExistingHash = CacheStore.Find(CacheKey))
    {
        if (*ExistingHash == PayloadHash)
        {
            return false;
        }
    }

    CacheStore.Add(CacheKey, PayloadHash);
    Subsystem->PulseEmitter(TargetId, EmitterName, Payload);
    return true;
}

void URshipContentMappingManager::ClearEmitterCacheForTarget(const FString& TargetId)
{
    LastEmittedStateHashes.Remove(TargetId + TEXT(":state"));
    LastEmittedStatusHashes.Remove(TargetId + TEXT(":status"));
}

void URshipContentMappingManager::EmitContextState(const FRshipRenderContextState& ContextState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildContextTargetId(ContextState.Id);
    TSharedPtr<FJsonObject> StatePayload = BuildRenderContextJson(ContextState);
    StatePayload->RemoveField(TEXT("hash"));
    PulseEmitterIfChanged(TargetId, TEXT("state"), StatePayload, LastEmittedStateHashes);

    TSharedPtr<FJsonObject> StatusPayload = MakeShared<FJsonObject>();
    StatusPayload->SetStringField(TEXT("status"), GetTargetStatusForEnabledFlag(ContextState.bEnabled));
    StatusPayload->SetStringField(TEXT("runtimeHealth"), GetRuntimeHealthStatusToken());
    if (!ContextState.LastError.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("lastError"), ContextState.LastError);
    }
    if (!RuntimeHealthReason.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("runtimeReason"), RuntimeHealthReason);
    }
    if (!ContextState.CameraId.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("cameraId"), ContextState.CameraId);
    }
    if (!ContextState.AssetId.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("assetId"), ContextState.AssetId);
    }
    if (!ContextState.ExternalSourceId.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("externalSourceId"), ContextState.ExternalSourceId);
    }
    StatusPayload->SetBoolField(TEXT("hasTexture"), ContextState.ResolvedTexture != nullptr);
    PulseEmitterIfChanged(TargetId, TEXT("status"), StatusPayload, LastEmittedStatusHashes);
}

void URshipContentMappingManager::EmitSurfaceState(const FRshipMappingSurfaceState& SurfaceState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildSurfaceTargetId(SurfaceState.Id);
    TSharedPtr<FJsonObject> StatePayload = BuildMappingSurfaceJson(SurfaceState);
    StatePayload->RemoveField(TEXT("hash"));
    PulseEmitterIfChanged(TargetId, TEXT("state"), StatePayload, LastEmittedStateHashes);
    EmitStatus(TargetId, GetTargetStatusForEnabledFlag(SurfaceState.bEnabled), SurfaceState.LastError);
}

void URshipContentMappingManager::EmitMappingState(const FRshipContentMappingState& MappingState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildMappingTargetId(MappingState.Id);
    TSharedPtr<FJsonObject> StatePayload = BuildMappingJson(MappingState);
    StatePayload->RemoveField(TEXT("hash"));
    PulseEmitterIfChanged(TargetId, TEXT("state"), StatePayload, LastEmittedStateHashes);
    EmitStatus(TargetId, GetTargetStatusForEnabledFlag(MappingState.bEnabled), MappingState.LastError);
}

void URshipContentMappingManager::EmitStatus(const FString& TargetId, const FString& Status, const FString& LastError)
{
    if (!Subsystem)
    {
        return;
    }

    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("status"), Status);
    Payload->SetStringField(TEXT("runtimeHealth"), GetRuntimeHealthStatusToken());
    if (!LastError.IsEmpty())
    {
        Payload->SetStringField(TEXT("lastError"), LastError);
    }
    if (!RuntimeHealthReason.IsEmpty())
    {
        Payload->SetStringField(TEXT("runtimeReason"), RuntimeHealthReason);
    }
    PulseEmitterIfChanged(TargetId, TEXT("status"), Payload, LastEmittedStatusHashes);
}

TSharedPtr<FJsonObject> URshipContentMappingManager::BuildRenderContextJson(const FRshipRenderContextState& ContextState) const
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("id"), ContextState.Id);
    Json->SetStringField(TEXT("name"), ContextState.Name);
    Json->SetStringField(TEXT("projectId"), ContextState.ProjectId);
    Json->SetStringField(TEXT("sourceType"), ContextState.SourceType);
    if (!ContextState.CameraId.IsEmpty())
    {
        Json->SetStringField(TEXT("cameraId"), ContextState.CameraId);
    }
    if (!ContextState.AssetId.IsEmpty())
    {
        Json->SetStringField(TEXT("assetId"), ContextState.AssetId);
    }
    if (!ContextState.DepthAssetId.IsEmpty())
    {
        Json->SetStringField(TEXT("depthAssetId"), ContextState.DepthAssetId);
    }
    if (!ContextState.ExternalSourceId.IsEmpty())
    {
        Json->SetStringField(TEXT("externalSourceId"), ContextState.ExternalSourceId);
    }
    if (ContextState.Width > 0)
    {
        Json->SetNumberField(TEXT("width"), ContextState.Width);
    }
    if (ContextState.Height > 0)
    {
        Json->SetNumberField(TEXT("height"), ContextState.Height);
    }
    if (!ContextState.CaptureMode.IsEmpty())
    {
        Json->SetStringField(TEXT("captureMode"), ContextState.CaptureMode);
    }
    if (!ContextState.DepthCaptureMode.IsEmpty())
    {
        Json->SetStringField(TEXT("depthCaptureMode"), ContextState.DepthCaptureMode);
    }
    Json->SetBoolField(TEXT("enabled"), ContextState.bEnabled);
    Json->SetBoolField(TEXT("depthCaptureEnabled"), ContextState.bDepthCaptureEnabled);
    Json->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    return Json;
}

TSharedPtr<FJsonObject> URshipContentMappingManager::BuildMappingSurfaceJson(const FRshipMappingSurfaceState& SurfaceState) const
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("id"), SurfaceState.Id);
    Json->SetStringField(TEXT("name"), SurfaceState.Name);
    Json->SetStringField(TEXT("projectId"), SurfaceState.ProjectId);
    Json->SetBoolField(TEXT("enabled"), SurfaceState.bEnabled);
    Json->SetNumberField(TEXT("uvChannel"), SurfaceState.UVChannel);
    if (SurfaceState.MaterialSlots.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Slots;
        for (int32 Slot : SurfaceState.MaterialSlots)
        {
            Slots.Add(MakeShared<FJsonValueNumber>(Slot));
        }
        Json->SetArrayField(TEXT("materialSlots"), Slots);
    }
    if (!SurfaceState.MeshComponentName.IsEmpty())
    {
        Json->SetStringField(TEXT("meshComponentName"), SurfaceState.MeshComponentName);
    }
    if (!SurfaceState.ActorPath.IsEmpty())
    {
        Json->SetStringField(TEXT("actorPath"), SurfaceState.ActorPath);
    }
    Json->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    return Json;
}

TSharedPtr<FJsonObject> URshipContentMappingManager::BuildMappingJson(const FRshipContentMappingState& MappingState) const
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("id"), MappingState.Id);
    Json->SetStringField(TEXT("name"), MappingState.Name);
    Json->SetStringField(TEXT("projectId"), MappingState.ProjectId);
    FString SerializedType = MappingState.Type;
    if (MappingState.Type.Equals(TEXT("surface-uv"), ESearchCase::IgnoreCase))
    {
        bool bFeedMode = false;
        if (MappingState.Config.IsValid())
        {
            const FString UvMode = GetStringField(MappingState.Config, TEXT("uvMode"), TEXT(""));
            bFeedMode = UvMode.Equals(TEXT("feed"), ESearchCase::IgnoreCase)
                || UvMode.Equals(TEXT("surface-feed"), ESearchCase::IgnoreCase)
                || MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2"));
        }
        SerializedType = bFeedMode ? TEXT("feed") : TEXT("direct");
    }
    Json->SetStringField(TEXT("type"), SerializedType);
    Json->SetBoolField(TEXT("enabled"), MappingState.bEnabled);
    Json->SetNumberField(TEXT("opacity"), MappingState.Opacity);
    if (!MappingState.ContextId.IsEmpty())
    {
        Json->SetStringField(TEXT("contextId"), MappingState.ContextId);
    }
    if (MappingState.SurfaceIds.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> SurfaceIds;
        for (const FString& SurfaceId : MappingState.SurfaceIds)
        {
            SurfaceIds.Add(MakeShared<FJsonValueString>(SurfaceId));
        }
        Json->SetArrayField(TEXT("surfaceIds"), SurfaceIds);
    }
    if (MappingState.Config.IsValid())
    {
        Json->SetObjectField(TEXT("config"), MappingState.Config);
    }
    Json->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    return Json;
}

bool URshipContentMappingManager::HandleContextAction(const FString& ContextId, const FString& ActionName, const TSharedRef<FJsonObject>& Data)
{
    FRshipRenderContextState* ContextState = RenderContexts.Find(ContextId);
    if (!ContextState)
    {
        return false;
    }
    const FRshipRenderContextState PreviousState = *ContextState;

    bool bHandled = true;
    if (ActionName == TEXT("setEnabled"))
    {
        ContextState->bEnabled = GetBoolField(Data, TEXT("enabled"), ContextState->bEnabled);
    }
    else if (ActionName == TEXT("setSourceType"))
    {
        ContextState->SourceType = GetStringField(Data, TEXT("sourceType"), ContextState->SourceType);
    }
    else if (ActionName == TEXT("setCameraId"))
    {
        ContextState->CameraId = GetStringField(Data, TEXT("cameraId"), ContextState->CameraId);
    }
    else if (ActionName == TEXT("setAssetId"))
    {
        ContextState->AssetId = GetStringField(Data, TEXT("assetId"), ContextState->AssetId);
    }
    else if (ActionName == TEXT("setExternalSourceId"))
    {
        ContextState->ExternalSourceId = GetStringField(
            Data,
            TEXT("externalSourceId"),
            GetStringField(Data, TEXT("sourceId"), ContextState->ExternalSourceId));
    }
    else if (ActionName == TEXT("setDepthAssetId"))
    {
        ContextState->DepthAssetId = GetStringField(Data, TEXT("depthAssetId"), ContextState->DepthAssetId);
    }
    else if (ActionName == TEXT("setDepthCaptureEnabled"))
    {
        ContextState->bDepthCaptureEnabled = GetBoolField(Data, TEXT("depthCaptureEnabled"), ContextState->bDepthCaptureEnabled);
    }
    else if (ActionName == TEXT("setDepthCaptureMode"))
    {
        ContextState->DepthCaptureMode = GetStringField(Data, TEXT("depthCaptureMode"), ContextState->DepthCaptureMode);
    }
    else if (ActionName == TEXT("setResolution"))
    {
        ContextState->Width = GetIntField(Data, TEXT("width"), ContextState->Width);
        ContextState->Height = GetIntField(Data, TEXT("height"), ContextState->Height);
    }
    else if (ActionName == TEXT("setCaptureMode"))
    {
        ContextState->CaptureMode = GetStringField(Data, TEXT("captureMode"), ContextState->CaptureMode);
    }
    else
    {
        bHandled = false;
    }

    if (bHandled)
    {
        NormalizeRenderContextState(*ContextState);
    }

    if (bHandled)
    {
        ResolveRenderContext(*ContextState);
        RegisterContextTarget(*ContextState);
        EmitContextState(*ContextState);
    }

    if (bHandled)
    {
        MarkMappingsDirty();
        MarkCacheDirty();
        if (!AreRenderContextStatesEquivalent(PreviousState, *ContextState))
        {
            SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
        }
    }

    return bHandled;
}

bool URshipContentMappingManager::HandleSurfaceAction(const FString& SurfaceId, const FString& ActionName, const TSharedRef<FJsonObject>& Data)
{
    FRshipMappingSurfaceState* SurfaceState = MappingSurfaces.Find(SurfaceId);
    if (!SurfaceState)
    {
        return false;
    }
    const FRshipMappingSurfaceState PreviousState = *SurfaceState;

    bool bHandled = true;
    bool bDisabledOverlaps = false;
    if (ActionName == TEXT("setEnabled"))
    {
        SurfaceState->bEnabled = GetBoolField(Data, TEXT("enabled"), SurfaceState->bEnabled);
    }
    else if (ActionName == TEXT("setActorPath"))
    {
        SurfaceState->ActorPath = GetStringField(Data, TEXT("actorPath"), SurfaceState->ActorPath);
    }
    else if (ActionName == TEXT("setUvChannel"))
    {
        SurfaceState->UVChannel = GetIntField(Data, TEXT("uvChannel"), SurfaceState->UVChannel);
    }
    else if (ActionName == TEXT("setMaterialSlots"))
    {
        SurfaceState->MaterialSlots = GetIntArrayField(Data, TEXT("materialSlots"));
    }
    else if (ActionName == TEXT("setMeshComponentName"))
    {
        SurfaceState->MeshComponentName = GetStringField(Data, TEXT("meshComponentName"), SurfaceState->MeshComponentName);
    }
    else
    {
        bHandled = false;
    }

    if (bHandled)
    {
        NormalizeMappingSurfaceState(*SurfaceState, Subsystem);
    }

    if (bHandled)
    {
        ResolveMappingSurface(*SurfaceState);
        RegisterSurfaceTarget(*SurfaceState);
        EmitSurfaceState(*SurfaceState);
    }

    if (bHandled)
    {
        MarkMappingsDirty();
        MarkCacheDirty();
        if (!AreMappingSurfaceStatesEquivalent(PreviousState, *SurfaceState))
        {
            SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
        }
    }

    return bHandled;
}

bool URshipContentMappingManager::HandleMappingAction(const FString& MappingId, const FString& ActionName, const TSharedRef<FJsonObject>& Data)
{
    FRshipContentMappingState* MappingState = Mappings.Find(MappingId);
    if (!MappingState)
    {
        return false;
    }
    FRshipContentMappingState PreviousState = *MappingState;
    PreviousState.Config = DeepCloneJsonObject(MappingState->Config);
    const FCanonicalMappingMode PreviousMode = GetCanonicalMappingMode(PreviousState);

    bool bHandled = true;
    bool bDisabledOverlaps = false;
    auto CloneObject = [](const TSharedPtr<FJsonObject>& InObj) -> TSharedPtr<FJsonObject>
    {
        return DeepCloneJsonObject(InObj);
    };
    auto EnsureConfig = [&]() -> TSharedPtr<FJsonObject>
    {
        if (!MappingState->Config.IsValid())
        {
            MappingState->Config = MakeShared<FJsonObject>();
        }
        return MappingState->Config;
    };
    auto ActivateFeedMode = [&]()
    {
        MappingState->Type = TEXT("surface-uv");
        TSharedPtr<FJsonObject> Config = EnsureConfig();
        Config->SetStringField(TEXT("uvMode"), TEXT("feed"));
    };
    auto EnsureFeedV2 = [&]() -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Config = EnsureConfig();
        TSharedPtr<FJsonObject> FeedV2 = Config->HasTypedField<EJson::Object>(TEXT("feedV2"))
            ? CloneObject(Config->GetObjectField(TEXT("feedV2")))
            : MakeShared<FJsonObject>();
        FeedV2->SetStringField(TEXT("coordinateSpace"), TEXT("pixel"));
        if (!FeedV2->HasTypedField<EJson::Array>(TEXT("sources")))
        {
            FeedV2->SetArrayField(TEXT("sources"), TArray<TSharedPtr<FJsonValue>>());
        }
        if (!FeedV2->HasTypedField<EJson::Array>(TEXT("destinations")))
        {
            FeedV2->SetArrayField(TEXT("destinations"), TArray<TSharedPtr<FJsonValue>>());
        }
        if (!FeedV2->HasTypedField<EJson::Array>(TEXT("routes")))
        {
            FeedV2->SetArrayField(TEXT("routes"), TArray<TSharedPtr<FJsonValue>>());
        }
        return FeedV2;
    };
    auto UpsertFeedObject = [&](TArray<TSharedPtr<FJsonValue>>& Array, const FString& IdField, const TSharedPtr<FJsonObject>& ObjectToUpsert) -> bool
    {
        if (!ObjectToUpsert.IsValid())
        {
            return false;
        }

        FString Id = GetStringField(ObjectToUpsert, IdField).TrimStartAndEnd();
        if (Id.IsEmpty() && IdField == TEXT("id"))
        {
            Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
            ObjectToUpsert->SetStringField(TEXT("id"), Id);
        }
        if (Id.IsEmpty())
        {
            return false;
        }

        for (int32 Index = 0; Index < Array.Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>& ExistingValue = Array[Index];
            if (!ExistingValue.IsValid() || ExistingValue->Type != EJson::Object)
            {
                continue;
            }

            const TSharedPtr<FJsonObject> ExistingObject = ExistingValue->AsObject();
            if (!ExistingObject.IsValid())
            {
                continue;
            }

            if (GetStringField(ExistingObject, IdField).TrimStartAndEnd().Equals(Id, ESearchCase::IgnoreCase))
            {
                Array[Index] = MakeShared<FJsonValueObject>(ObjectToUpsert);
                return true;
            }
        }

        Array.Add(MakeShared<FJsonValueObject>(ObjectToUpsert));
        return true;
    };
    auto RemoveFeedObjectById = [&](TArray<TSharedPtr<FJsonValue>>& Array, const FString& IdField, const FString& IdToRemove) -> bool
    {
        const FString SanitizedId = IdToRemove.TrimStartAndEnd();
        if (SanitizedId.IsEmpty())
        {
            return false;
        }

        const int32 RemovedCount = Array.RemoveAll([&](const TSharedPtr<FJsonValue>& Value)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                return false;
            }
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            return Obj.IsValid()
                && GetStringField(Obj, IdField).TrimStartAndEnd().Equals(SanitizedId, ESearchCase::IgnoreCase);
        });
        return RemovedCount > 0;
    };

    if (ActionName == TEXT("setEnabled"))
    {
        MappingState->bEnabled = GetBoolField(Data, TEXT("enabled"), MappingState->bEnabled);
    }
    else if (ActionName == TEXT("setOpacity"))
    {
        MappingState->Opacity = FMath::Clamp(GetNumberField(Data, TEXT("opacity"), MappingState->Opacity), 0.0f, 1.0f);
    }
    else if (ActionName == TEXT("setContextId"))
    {
        MappingState->ContextId = GetStringField(Data, TEXT("contextId"), MappingState->ContextId);
    }
    else if (ActionName == TEXT("setSurfaceIds"))
    {
        MappingState->SurfaceIds = GetStringArrayField(Data, TEXT("surfaceIds"));
    }
    else if (ActionName == TEXT("setProjection"))
    {
        MappingState->Type = TEXT("surface-projection");
        if (Data->HasTypedField<EJson::Object>(TEXT("config")))
        {
            MappingState->Config = CloneObject(Data->GetObjectField(TEXT("config")));
        }
        else
        {
            if (!MappingState->Config.IsValid())
            {
                MappingState->Config = MakeShared<FJsonObject>();
            }
            if (Data->HasTypedField<EJson::String>(TEXT("projectionType")))
            {
                MappingState->Config->SetStringField(TEXT("projectionType"), GetStringField(Data, TEXT("projectionType")));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("projectorPosition")))
            {
                MappingState->Config->SetObjectField(TEXT("projectorPosition"), CloneObject(Data->GetObjectField(TEXT("projectorPosition"))));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("projectorRotation")))
            {
                MappingState->Config->SetObjectField(TEXT("projectorRotation"), CloneObject(Data->GetObjectField(TEXT("projectorRotation"))));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("eyepoint")))
            {
                MappingState->Config->SetObjectField(TEXT("eyepoint"), CloneObject(Data->GetObjectField(TEXT("eyepoint"))));
            }
            if (Data->HasTypedField<EJson::Number>(TEXT("fov")))
            {
                MappingState->Config->SetNumberField(TEXT("fov"), Data->GetNumberField(TEXT("fov")));
            }
            if (Data->HasTypedField<EJson::Number>(TEXT("aspectRatio")))
            {
                MappingState->Config->SetNumberField(TEXT("aspectRatio"), Data->GetNumberField(TEXT("aspectRatio")));
            }
            if (Data->HasTypedField<EJson::Number>(TEXT("near")))
            {
                MappingState->Config->SetNumberField(TEXT("near"), Data->GetNumberField(TEXT("near")));
            }
            if (Data->HasTypedField<EJson::Number>(TEXT("far")))
            {
                MappingState->Config->SetNumberField(TEXT("far"), Data->GetNumberField(TEXT("far")));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("cylindrical")))
            {
                MappingState->Config->SetObjectField(TEXT("cylindrical"), CloneObject(Data->GetObjectField(TEXT("cylindrical"))));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("cameraPlate")))
            {
                MappingState->Config->SetObjectField(TEXT("cameraPlate"), CloneObject(Data->GetObjectField(TEXT("cameraPlate"))));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("spatial")))
            {
                MappingState->Config->SetObjectField(TEXT("spatial"), CloneObject(Data->GetObjectField(TEXT("spatial"))));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("depthMap")))
            {
                MappingState->Config->SetObjectField(TEXT("depthMap"), CloneObject(Data->GetObjectField(TEXT("depthMap"))));
            }
            if (Data->HasTypedField<EJson::Number>(TEXT("depthScale")))
            {
                MappingState->Config->SetNumberField(TEXT("depthScale"), Data->GetNumberField(TEXT("depthScale")));
            }
            if (Data->HasTypedField<EJson::Number>(TEXT("depthBias")))
            {
                MappingState->Config->SetNumberField(TEXT("depthBias"), Data->GetNumberField(TEXT("depthBias")));
            }
            if (Data->HasTypedField<EJson::Number>(TEXT("depthNear")))
            {
                MappingState->Config->SetNumberField(TEXT("depthNear"), Data->GetNumberField(TEXT("depthNear")));
            }
            if (Data->HasTypedField<EJson::Number>(TEXT("depthFar")))
            {
                MappingState->Config->SetNumberField(TEXT("depthFar"), Data->GetNumberField(TEXT("depthFar")));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("customProjectionMatrix")))
            {
                MappingState->Config->SetObjectField(TEXT("customProjectionMatrix"), CloneObject(Data->GetObjectField(TEXT("customProjectionMatrix"))));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("matrix")))
            {
                MappingState->Config->SetObjectField(TEXT("customProjectionMatrix"), CloneObject(Data->GetObjectField(TEXT("matrix"))));
            }
        }
    }
    else if (ActionName == TEXT("setUVTransform"))
    {
        MappingState->Type = TEXT("surface-uv");
        if (!MappingState->Config.IsValid())
        {
            MappingState->Config = MakeShared<FJsonObject>();
        }
        const FString ExistingUvMode = GetStringField(MappingState->Config, TEXT("uvMode"), TEXT("direct"));
        MappingState->Config->SetStringField(TEXT("uvMode"), NormalizeUvModeToken(GetStringField(Data, TEXT("uvMode"), ExistingUvMode), ExistingUvMode));
        if (Data->HasTypedField<EJson::Object>(TEXT("uvTransform")))
        {
            MappingState->Config->SetObjectField(TEXT("uvTransform"), CloneObject(Data->GetObjectField(TEXT("uvTransform"))));
        }
    }
    else if (ActionName == TEXT("setType"))
    {
        const FString IncomingType = GetStringField(Data, TEXT("type"), MappingState->Type).TrimStartAndEnd();
        if (!IncomingType.IsEmpty())
        {
            MappingState->Type = IncomingType;
        }
    }
    else if (ActionName == TEXT("setConfig"))
    {
        if (Data->HasTypedField<EJson::Object>(TEXT("config")))
        {
            MappingState->Config = CloneObject(Data->GetObjectField(TEXT("config")));
        }
        else
        {
            bHandled = false;
        }
    }
    else if (ActionName == TEXT("setFeedV2"))
    {
        ActivateFeedMode();
        TSharedPtr<FJsonObject> FeedV2 = EnsureFeedV2();

        if (Data->HasTypedField<EJson::Object>(TEXT("feedV2")))
        {
            FeedV2 = CloneObject(Data->GetObjectField(TEXT("feedV2")));
            FeedV2->SetStringField(TEXT("coordinateSpace"), TEXT("pixel"));
            if (!FeedV2->HasTypedField<EJson::Array>(TEXT("sources")))
            {
                FeedV2->SetArrayField(TEXT("sources"), TArray<TSharedPtr<FJsonValue>>());
            }
            if (!FeedV2->HasTypedField<EJson::Array>(TEXT("destinations")))
            {
                FeedV2->SetArrayField(TEXT("destinations"), TArray<TSharedPtr<FJsonValue>>());
            }
            if (!FeedV2->HasTypedField<EJson::Array>(TEXT("routes")))
            {
                FeedV2->SetArrayField(TEXT("routes"), TArray<TSharedPtr<FJsonValue>>());
            }
        }
        else
        {
            if (Data->HasTypedField<EJson::Array>(TEXT("sources")))
            {
                FeedV2->SetArrayField(TEXT("sources"), Data->GetArrayField(TEXT("sources")));
            }
            if (Data->HasTypedField<EJson::Array>(TEXT("destinations")))
            {
                FeedV2->SetArrayField(TEXT("destinations"), Data->GetArrayField(TEXT("destinations")));
            }
            if (Data->HasTypedField<EJson::Array>(TEXT("routes")))
            {
                FeedV2->SetArrayField(TEXT("routes"), Data->GetArrayField(TEXT("routes")));
            }
        }

        EnsureConfig()->SetObjectField(TEXT("feedV2"), FeedV2);
    }
    else if (ActionName == TEXT("upsertFeedSource"))
    {
        ActivateFeedMode();
        TSharedPtr<FJsonObject> FeedV2 = EnsureFeedV2();
        TSharedPtr<FJsonObject> SourceObj = Data->HasTypedField<EJson::Object>(TEXT("source"))
            ? CloneObject(Data->GetObjectField(TEXT("source")))
            : MakeShared<FJsonObject>(*Data);
        TArray<TSharedPtr<FJsonValue>> SourceArray = FeedV2->GetArrayField(TEXT("sources"));
        if (!UpsertFeedObject(SourceArray, TEXT("id"), SourceObj))
        {
            bHandled = false;
        }
        else
        {
            FeedV2->SetArrayField(TEXT("sources"), SourceArray);
            EnsureConfig()->SetObjectField(TEXT("feedV2"), FeedV2);
        }
    }
    else if (ActionName == TEXT("removeFeedSource"))
    {
        ActivateFeedMode();
        TSharedPtr<FJsonObject> FeedV2 = EnsureFeedV2();
        const FString SourceId = GetStringField(Data, TEXT("sourceId"), GetStringField(Data, TEXT("id")));
        TArray<TSharedPtr<FJsonValue>> SourceArray = FeedV2->GetArrayField(TEXT("sources"));
        TArray<TSharedPtr<FJsonValue>> RouteArray = FeedV2->GetArrayField(TEXT("routes"));
        const bool bRemovedSource = RemoveFeedObjectById(SourceArray, TEXT("id"), SourceId);
        const int32 RemovedRoutes = RouteArray.RemoveAll([&](const TSharedPtr<FJsonValue>& Value)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                return false;
            }
            const TSharedPtr<FJsonObject> RouteObj = Value->AsObject();
            return RouteObj.IsValid()
                && GetStringField(RouteObj, TEXT("sourceId")).TrimStartAndEnd().Equals(SourceId.TrimStartAndEnd(), ESearchCase::IgnoreCase);
        });
        if (!bRemovedSource && RemovedRoutes == 0)
        {
            bHandled = false;
        }
        else
        {
            FeedV2->SetArrayField(TEXT("sources"), SourceArray);
            FeedV2->SetArrayField(TEXT("routes"), RouteArray);
            EnsureConfig()->SetObjectField(TEXT("feedV2"), FeedV2);
        }
    }
    else if (ActionName == TEXT("upsertFeedDestination"))
    {
        ActivateFeedMode();
        TSharedPtr<FJsonObject> FeedV2 = EnsureFeedV2();
        TSharedPtr<FJsonObject> DestinationObj = Data->HasTypedField<EJson::Object>(TEXT("destination"))
            ? CloneObject(Data->GetObjectField(TEXT("destination")))
            : MakeShared<FJsonObject>(*Data);
        TArray<TSharedPtr<FJsonValue>> DestinationArray = FeedV2->GetArrayField(TEXT("destinations"));
        if (!UpsertFeedObject(DestinationArray, TEXT("id"), DestinationObj))
        {
            bHandled = false;
        }
        else
        {
            FeedV2->SetArrayField(TEXT("destinations"), DestinationArray);
            EnsureConfig()->SetObjectField(TEXT("feedV2"), FeedV2);
        }
    }
    else if (ActionName == TEXT("removeFeedDestination"))
    {
        ActivateFeedMode();
        TSharedPtr<FJsonObject> FeedV2 = EnsureFeedV2();
        const FString DestinationId = GetStringField(Data, TEXT("destinationId"), GetStringField(Data, TEXT("id")));
        TArray<TSharedPtr<FJsonValue>> DestinationArray = FeedV2->GetArrayField(TEXT("destinations"));
        TArray<TSharedPtr<FJsonValue>> RouteArray = FeedV2->GetArrayField(TEXT("routes"));
        const bool bRemovedDestination = RemoveFeedObjectById(DestinationArray, TEXT("id"), DestinationId);
        const int32 RemovedRoutes = RouteArray.RemoveAll([&](const TSharedPtr<FJsonValue>& Value)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                return false;
            }
            const TSharedPtr<FJsonObject> RouteObj = Value->AsObject();
            return RouteObj.IsValid()
                && GetStringField(RouteObj, TEXT("destinationId")).TrimStartAndEnd().Equals(DestinationId.TrimStartAndEnd(), ESearchCase::IgnoreCase);
        });
        if (!bRemovedDestination && RemovedRoutes == 0)
        {
            bHandled = false;
        }
        else
        {
            FeedV2->SetArrayField(TEXT("destinations"), DestinationArray);
            FeedV2->SetArrayField(TEXT("routes"), RouteArray);
            EnsureConfig()->SetObjectField(TEXT("feedV2"), FeedV2);
        }
    }
    else if (ActionName == TEXT("upsertFeedRoute"))
    {
        ActivateFeedMode();
        TSharedPtr<FJsonObject> FeedV2 = EnsureFeedV2();
        TSharedPtr<FJsonObject> RouteObj = Data->HasTypedField<EJson::Object>(TEXT("route"))
            ? CloneObject(Data->GetObjectField(TEXT("route")))
            : MakeShared<FJsonObject>(*Data);
        TArray<TSharedPtr<FJsonValue>> RouteArray = FeedV2->GetArrayField(TEXT("routes"));
        if (!UpsertFeedObject(RouteArray, TEXT("id"), RouteObj))
        {
            bHandled = false;
        }
        else
        {
            FeedV2->SetArrayField(TEXT("routes"), RouteArray);
            EnsureConfig()->SetObjectField(TEXT("feedV2"), FeedV2);
        }
    }
    else if (ActionName == TEXT("removeFeedRoute"))
    {
        ActivateFeedMode();
        TSharedPtr<FJsonObject> FeedV2 = EnsureFeedV2();
        const FString RouteId = GetStringField(Data, TEXT("routeId"), GetStringField(Data, TEXT("id")));
        TArray<TSharedPtr<FJsonValue>> RouteArray = FeedV2->GetArrayField(TEXT("routes"));
        if (!RemoveFeedObjectById(RouteArray, TEXT("id"), RouteId))
        {
            bHandled = false;
        }
        else
        {
            FeedV2->SetArrayField(TEXT("routes"), RouteArray);
            EnsureConfig()->SetObjectField(TEXT("feedV2"), FeedV2);
        }
    }
    else
    {
        bHandled = false;
    }

    if (bHandled)
    {
        NormalizeMappingState(*MappingState);
        if (EnsureMappingRuntimeReady(*MappingState))
        {
            NormalizeMappingState(*MappingState);
        }

        const FCanonicalMappingMode UpdatedMode = GetCanonicalMappingMode(*MappingState);
        if (!AreCanonicalMappingModesEquivalent(PreviousMode, UpdatedMode))
        {
            UE_LOG(
                LogRshipExec,
                Warning,
                TEXT("Rejecting mapping action '%s' for '%s' because it changes mode from %s/%s to %s/%s."),
                *ActionName,
                *MappingId,
                *PreviousMode.CanonicalType,
                *PreviousMode.CanonicalMode,
                *UpdatedMode.CanonicalType,
                *UpdatedMode.CanonicalMode);
            *MappingState = PreviousState;
            return false;
        }

        TrackPendingMappingUpsert(*MappingState);
        bDisabledOverlaps = DisableOverlappingEnabledMappings(MappingState->Id);
    }

    if (bHandled && !MappingState->bEnabled)
    {
        RemoveFeedCompositeTexturesForMapping(MappingState->Id);
    }

    if (bHandled)
    {
        RegisterMappingTarget(*MappingState);
        EmitMappingState(*MappingState);
    }

    if (bHandled)
    {
        MarkMappingsDirty();
        MarkCacheDirty();
        const bool bRequiresImmediateRebuild = PreviousState.bEnabled != MappingState->bEnabled
            || PreviousState.Type != MappingState->Type
            || PreviousState.ContextId != MappingState->ContextId
            || !AreStringArraysEqual(PreviousState.SurfaceIds, MappingState->SurfaceIds);
        const bool bRequiresImmediateRefresh = bRequiresImmediateRebuild
            || !FMath::IsNearlyEqual(PreviousState.Opacity, MappingState->Opacity)
            || !AreJsonObjectsEqual(PreviousState.Config, MappingState->Config);
        if (bRequiresImmediateRefresh || bDisabledOverlaps)
        {
            SyncRuntimeAfterMutation(/*bRequireRebuild=*/true);
        }
    }

    return bHandled;
}

void URshipContentMappingManager::SaveCache()
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    TArray<TSharedPtr<FJsonValue>> ContextArray;
    for (const auto& Pair : RenderContexts)
    {
        ContextArray.Add(MakeShared<FJsonValueObject>(BuildRenderContextJson(Pair.Value)));
    }
    Root->SetArrayField(TEXT("renderContexts"), ContextArray);

    TArray<TSharedPtr<FJsonValue>> SurfaceArray;
    for (const auto& Pair : MappingSurfaces)
    {
        SurfaceArray.Add(MakeShared<FJsonValueObject>(BuildMappingSurfaceJson(Pair.Value)));
    }
    Root->SetArrayField(TEXT("mappingSurfaces"), SurfaceArray);

    TArray<TSharedPtr<FJsonValue>> MappingArray;
    for (const auto& Pair : Mappings)
    {
        MappingArray.Add(MakeShared<FJsonValueObject>(BuildMappingJson(Pair.Value)));
    }
    Root->SetArrayField(TEXT("mappings"), MappingArray);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    const FString CachePath = GetCachePath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(CachePath), true);
    FFileHelper::SaveStringToFile(Output, *CachePath);
}

void URshipContentMappingManager::LoadCache()
{
    const FString CachePath = GetCachePath();
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *CachePath))
    {
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return;
    }

    if (Root->HasTypedField<EJson::Array>(TEXT("renderContexts")))
    {
        const TArray<TSharedPtr<FJsonValue>> ContextArray = Root->GetArrayField(TEXT("renderContexts"));
        for (const TSharedPtr<FJsonValue>& Value : ContextArray)
        {
            if (Value.IsValid() && Value->Type == EJson::Object)
            {
                ProcessRenderContextEvent(Value->AsObject(), false);
            }
        }
    }

    if (Root->HasTypedField<EJson::Array>(TEXT("mappingSurfaces")))
    {
        const TArray<TSharedPtr<FJsonValue>> SurfaceArray = Root->GetArrayField(TEXT("mappingSurfaces"));
        for (const TSharedPtr<FJsonValue>& Value : SurfaceArray)
        {
            if (Value.IsValid() && Value->Type == EJson::Object)
            {
                ProcessMappingSurfaceEvent(Value->AsObject(), false);
            }
        }
    }

    if (Root->HasTypedField<EJson::Array>(TEXT("mappings")))
    {
        const TArray<TSharedPtr<FJsonValue>> MappingArray = Root->GetArrayField(TEXT("mappings"));
        for (const TSharedPtr<FJsonValue>& Value : MappingArray)
        {
            if (Value.IsValid() && Value->Type == EJson::Object)
            {
                ProcessMappingEvent(Value->AsObject(), false);
            }
        }
    }
}

FString URshipContentMappingManager::GetCachePath() const
{
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (Settings && !Settings->ContentMappingCachePath.IsEmpty())
    {
        return Settings->ContentMappingCachePath;
    }

    return FPaths::ProjectSavedDir() / TEXT("Rship/ContentMappingCache.json");
}

bool URshipContentMappingManager::ValidateMaterialContract(
    UMaterialInterface* Material,
    FString& OutError,
    bool bRequireProjectionContract) const
{
    OutError.Reset();
    if (!Material)
    {
        OutError = TEXT("ContentMapping material is null.");
        return false;
    }

    const FMaterialParameterInventory Inventory = GatherMaterialParameterInventory(Material);
    if (!HasContextTextureParameter(Inventory))
    {
        OutError = FString::Printf(
            TEXT("Material '%s' missing required context texture parameter (%s or aliases %s/%s)."),
            *Material->GetName(),
            *ParamContextTexture.ToString(),
            *ParamContextTextureAliasSlateUI.ToString(),
            *ParamContextTextureAliasTexture.ToString());
        return false;
    }

    if (bRequireProjectionContract)
    {
        TArray<FString> MissingProjectionParams;
        if (!HasProjectionContractParameters(Inventory, &MissingProjectionParams))
        {
            OutError = FString::Printf(
                TEXT("Material '%s' missing projection contract parameters: %s"),
                *Material->GetName(),
                *FString::Join(MissingProjectionParams, TEXT(", ")));
            return false;
        }
    }

    return true;
}

bool URshipContentMappingManager::ValidateMaterialContractForProfile(
    UMaterialInterface* Material,
    const FString& ProfileToken,
    FString& OutError) const
{
    const bool bRequiresProjectionCore = !ProfileToken.Equals(MaterialProfileDirect, ESearchCase::IgnoreCase);
    if (!ValidateMaterialContract(Material, OutError, bRequiresProjectionCore))
    {
        return false;
    }

    if (!Material || ProfileToken.IsEmpty())
    {
        return true;
    }

    const FMaterialParameterInventory Inventory = GatherMaterialParameterInventory(Material);
    TArray<FString> MissingAdvancedParams;
    if (!HasAdvancedProfileContractParameters(Inventory, ProfileToken, &MissingAdvancedParams))
    {
        OutError = FString::Printf(
            TEXT("Material '%s' missing '%s' profile parameters: %s"),
            *Material->GetName(),
            *ProfileToken,
            *FString::Join(MissingAdvancedParams, TEXT(", ")));
        return false;
    }

    return true;
}

ERshipContentMappingRuntimeHealth URshipContentMappingManager::GetRuntimeHealth() const
{
    return RuntimeHealth;
}

FString URshipContentMappingManager::GetRuntimeHealthReason() const
{
    return RuntimeHealthReason;
}

UMaterialInterface* URshipContentMappingManager::ResolveSurfaceFallbackMaterial(
    const FRshipMappingSurfaceState& SurfaceState,
    UMeshComponent* Mesh,
    FString& OutError) const
{
    OutError.Empty();
    if (!Mesh || !IsValid(Mesh))
    {
        OutError = TEXT("Mesh component not resolved");
        return nullptr;
    }

    const int32 SlotCount = Mesh->GetNumMaterials();
    if (SlotCount <= 0)
    {
        OutError = TEXT("Mesh has no material slots");
        return nullptr;
    }

    TArray<int32> CandidateSlots = SurfaceState.MaterialSlots;
    if (CandidateSlots.Num() == 0)
    {
        CandidateSlots.Reserve(SlotCount);
        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            CandidateSlots.Add(SlotIndex);
        }
    }

    FString LastContractError;
    auto TryCandidate = [&](UMaterialInterface* Candidate) -> UMaterialInterface*
    {
        if (!Candidate || !IsValid(Candidate))
        {
            return nullptr;
        }

        FString ContractError;
        if (ValidateMaterialContract(Candidate, ContractError))
        {
            return Candidate;
        }

        if (!ContractError.IsEmpty())
        {
            LastContractError = ContractError;
        }
        return nullptr;
    };

    for (const int32 SlotIndex : CandidateSlots)
    {
        if (SlotIndex < 0 || SlotIndex >= SlotCount)
        {
            continue;
        }

        if (const TWeakObjectPtr<UMaterialInterface>* Original = SurfaceState.OriginalMaterials.Find(SlotIndex))
        {
            if (UMaterialInterface* OriginalMaterial = TryCandidate(Original->Get()))
            {
                return OriginalMaterial;
            }
        }

        if (UMaterialInterface* SlotMaterial = TryCandidate(Mesh->GetMaterial(SlotIndex)))
        {
            return SlotMaterial;
        }
    }

    OutError = LastContractError.IsEmpty()
        ? TEXT("No compatible fallback surface material (requires context texture parameter)")
        : LastContractError;
    return nullptr;
}

bool URshipContentMappingManager::ResolveMaterialForProfile(const FString& ProfileToken, UMaterialInterface*& OutMaterial, FString& OutError)
{
    OutMaterial = nullptr;
    OutError.Empty();

    FString NormalizedProfile = ProfileToken.TrimStartAndEnd().ToLower();
    if (NormalizedProfile.IsEmpty())
    {
        NormalizedProfile = MaterialProfileDirect;
    }

    if (TObjectPtr<UMaterialInterface>* Cached = ContentMappingMaterialsByProfile.Find(NormalizedProfile))
    {
        if (*Cached && IsValid(*Cached))
        {
            FString CachedError;
            if (ValidateMaterialContractForProfile(*Cached, NormalizedProfile, CachedError))
            {
                OutMaterial = *Cached;
                return true;
            }
        }
        ContentMappingMaterialsByProfile.Remove(NormalizedProfile);
    }

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const bool bAllowSemanticFallbacks = CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0;
    TArray<FString> CandidatePaths;
    CandidatePaths.Reserve(24);

    auto AddPath = [&CandidatePaths](const FString& RawPath)
    {
        const FString Path = RawPath.TrimStartAndEnd();
        if (!Path.IsEmpty())
        {
            CandidatePaths.AddUnique(Path);
        }
    };

    if (Settings)
    {
        if (NormalizedProfile == MaterialProfileDirect)
        {
            AddPath(Settings->ContentMappingMaterialPath);
        }
        else if (NormalizedProfile == MaterialProfileProjection)
        {
            AddPath(Settings->ContentMappingProjectionMaterialPath);
            if (bAllowSemanticFallbacks)
            {
                AddPath(Settings->ContentMappingMaterialPath);
            }
        }
        else if (NormalizedProfile == MaterialProfileCameraPlate)
        {
            AddPath(Settings->ContentMappingCameraPlateMaterialPath);
            if (bAllowSemanticFallbacks)
            {
                AddPath(Settings->ContentMappingProjectionMaterialPath);
                AddPath(Settings->ContentMappingMaterialPath);
            }
        }
        else if (NormalizedProfile == MaterialProfileSpatial)
        {
            AddPath(Settings->ContentMappingSpatialMaterialPath);
            if (bAllowSemanticFallbacks)
            {
                AddPath(Settings->ContentMappingProjectionMaterialPath);
                AddPath(Settings->ContentMappingMaterialPath);
            }
        }
        else if (NormalizedProfile == MaterialProfileDepthMap)
        {
            AddPath(Settings->ContentMappingDepthMapMaterialPath);
            if (bAllowSemanticFallbacks)
            {
                AddPath(Settings->ContentMappingProjectionMaterialPath);
                AddPath(Settings->ContentMappingMaterialPath);
            }
        }
    }

    auto AddDefaultPaths = [&AddPath](const TArray<const TCHAR*>& Paths)
    {
        for (const TCHAR* Path : Paths)
        {
            AddPath(Path);
        }
    };

    static const TArray<const TCHAR*> CanonicalDirectPaths =
    {
        TEXT("/RshipMapping/Materials/MI_RshipContentMapping.MI_RshipContentMapping"),
        TEXT("/RshipMapping/Materials/M_RshipContentMapping.M_RshipContentMapping")
    };

    static const TArray<const TCHAR*> LegacyDirectPaths =
    {
        TEXT("/Game/Rship/Materials/MI_RshipContentMapping.MI_RshipContentMapping"),
        TEXT("/Game/Rship/Materials/M_RshipContentMapping.M_RshipContentMapping"),
        TEXT("/RshipExec/Materials/MI_RshipContentMapping.MI_RshipContentMapping"),
        TEXT("/RshipExec/Materials/M_RshipContentMapping.M_RshipContentMapping"),
        TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"),
        TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent")
    };

    static const TArray<const TCHAR*> CanonicalProjectionPaths =
    {
        TEXT("/RshipMapping/Materials/MI_RshipContentMappingProjection.MI_RshipContentMappingProjection"),
        TEXT("/RshipMapping/Materials/M_RshipContentMappingProjection.M_RshipContentMappingProjection")
    };

    static const TArray<const TCHAR*> LegacyProjectionPaths =
    {
        TEXT("/Game/Rship/Materials/MI_RshipContentMappingProjection.MI_RshipContentMappingProjection"),
        TEXT("/Game/Rship/Materials/M_RshipContentMappingProjection.M_RshipContentMappingProjection"),
        TEXT("/RshipExec/Materials/MI_RshipContentMappingProjection.MI_RshipContentMappingProjection"),
        TEXT("/RshipExec/Materials/M_RshipContentMappingProjection.M_RshipContentMappingProjection")
    };

    static const TArray<const TCHAR*> CanonicalCameraPlatePaths =
    {
        TEXT("/RshipMapping/Materials/MI_RshipContentMappingCameraPlate.MI_RshipContentMappingCameraPlate"),
        TEXT("/RshipMapping/Materials/M_RshipContentMappingCameraPlate.M_RshipContentMappingCameraPlate")
    };

    static const TArray<const TCHAR*> LegacyCameraPlatePaths =
    {
        TEXT("/Game/Rship/Materials/MI_RshipContentMappingCameraPlate.MI_RshipContentMappingCameraPlate"),
        TEXT("/Game/Rship/Materials/M_RshipContentMappingCameraPlate.M_RshipContentMappingCameraPlate")
    };

    static const TArray<const TCHAR*> CanonicalSpatialPaths =
    {
        TEXT("/RshipMapping/Materials/MI_RshipContentMappingSpatial.MI_RshipContentMappingSpatial"),
        TEXT("/RshipMapping/Materials/M_RshipContentMappingSpatial.M_RshipContentMappingSpatial")
    };

    static const TArray<const TCHAR*> LegacySpatialPaths =
    {
        TEXT("/Game/Rship/Materials/MI_RshipContentMappingSpatial.MI_RshipContentMappingSpatial"),
        TEXT("/Game/Rship/Materials/M_RshipContentMappingSpatial.M_RshipContentMappingSpatial")
    };

    static const TArray<const TCHAR*> CanonicalDepthMapPaths =
    {
        TEXT("/RshipMapping/Materials/MI_RshipContentMappingDepthMap.MI_RshipContentMappingDepthMap"),
        TEXT("/RshipMapping/Materials/M_RshipContentMappingDepthMap.M_RshipContentMappingDepthMap")
    };

    static const TArray<const TCHAR*> LegacyDepthMapPaths =
    {
        TEXT("/Game/Rship/Materials/MI_RshipContentMappingDepthMap.MI_RshipContentMappingDepthMap"),
        TEXT("/Game/Rship/Materials/M_RshipContentMappingDepthMap.M_RshipContentMappingDepthMap")
    };

    if (NormalizedProfile == MaterialProfileDirect)
    {
        AddDefaultPaths(CanonicalDirectPaths);
        if (bAllowSemanticFallbacks)
        {
            AddDefaultPaths(LegacyDirectPaths);
        }
    }
    else if (NormalizedProfile == MaterialProfileProjection)
    {
        AddDefaultPaths(CanonicalProjectionPaths);
        if (bAllowSemanticFallbacks)
        {
            AddDefaultPaths(LegacyProjectionPaths);
            AddDefaultPaths(CanonicalDirectPaths);
            AddDefaultPaths(LegacyDirectPaths);
        }
    }
    else if (NormalizedProfile == MaterialProfileCameraPlate)
    {
        AddDefaultPaths(CanonicalCameraPlatePaths);
        if (bAllowSemanticFallbacks)
        {
            AddDefaultPaths(LegacyCameraPlatePaths);
            AddDefaultPaths(CanonicalProjectionPaths);
            AddDefaultPaths(LegacyProjectionPaths);
            AddDefaultPaths(CanonicalDirectPaths);
            AddDefaultPaths(LegacyDirectPaths);
        }
    }
    else if (NormalizedProfile == MaterialProfileSpatial)
    {
        AddDefaultPaths(CanonicalSpatialPaths);
        if (bAllowSemanticFallbacks)
        {
            AddDefaultPaths(LegacySpatialPaths);
            AddDefaultPaths(CanonicalProjectionPaths);
            AddDefaultPaths(LegacyProjectionPaths);
            AddDefaultPaths(CanonicalDirectPaths);
            AddDefaultPaths(LegacyDirectPaths);
        }
    }
    else if (NormalizedProfile == MaterialProfileDepthMap)
    {
        AddDefaultPaths(CanonicalDepthMapPaths);
        if (bAllowSemanticFallbacks)
        {
            AddDefaultPaths(LegacyDepthMapPaths);
            AddDefaultPaths(CanonicalProjectionPaths);
            AddDefaultPaths(LegacyProjectionPaths);
            AddDefaultPaths(CanonicalDirectPaths);
            AddDefaultPaths(LegacyDirectPaths);
        }
    }
    else
    {
        AddDefaultPaths(CanonicalDirectPaths);
        if (bAllowSemanticFallbacks)
        {
            AddDefaultPaths(LegacyDirectPaths);
        }
    }

    UMaterialInterface* BestCandidate = nullptr;
    int32 BestScore = MIN_int32;
    FString LastContractError;
    for (const FString& CandidatePath : CandidatePaths)
    {
        if (UMaterialInterface* Candidate = TryLoadMaterialPath(CandidatePath))
        {
            FString ContractError;
            if (!ValidateMaterialContractForProfile(Candidate, NormalizedProfile, ContractError))
            {
                if (LastContractError.IsEmpty() && !ContractError.IsEmpty())
                {
                    LastContractError = ContractError;
                }
                continue;
            }

            const int32 CandidateScore = ScoreMaterialCapability(GatherMaterialParameterInventory(Candidate));
            if (!BestCandidate || CandidateScore > BestScore)
            {
                BestCandidate = Candidate;
                BestScore = CandidateScore;
            }
        }
    }

    if (!BestCandidate)
    {
        OutError = !LastContractError.IsEmpty()
            ? LastContractError
            : FString::Printf(TEXT("No material found for mapping material profile '%s'"), *NormalizedProfile);
        return false;
    }

    OutMaterial = BestCandidate;
    ContentMappingMaterialsByProfile.Add(NormalizedProfile, BestCandidate);
    return true;
}

bool URshipContentMappingManager::ResolveMaterialForMapping(
    const FRshipContentMappingState& MappingState,
    UMaterialInterface*& OutMaterial,
    FString& OutError)
{
    OutMaterial = nullptr;
    OutError.Empty();

    FString ProfileToken = GetMappingMaterialProfileToken(MappingState);
    if (MappingState.Config.IsValid() && MappingState.Config->HasTypedField<EJson::String>(TEXT("materialProfile")))
    {
        const FString RequestedProfile = MappingState.Config->GetStringField(TEXT("materialProfile")).TrimStartAndEnd();
        if (!RequestedProfile.IsEmpty())
        {
            ProfileToken = RequestedProfile;
        }
    }

    if (MappingState.Config.IsValid() && MappingState.Config->HasTypedField<EJson::String>(TEXT("materialPath")))
    {
        const FString OverridePath = MappingState.Config->GetStringField(TEXT("materialPath")).TrimStartAndEnd();
        if (!OverridePath.IsEmpty())
        {
            if (UMaterialInterface* OverrideMaterial = TryLoadMaterialPath(OverridePath))
            {
                FString ContractError;
                if (ValidateMaterialContractForProfile(OverrideMaterial, ProfileToken, ContractError))
                {
                    OutMaterial = OverrideMaterial;
                    return true;
                }

                OutError = ContractError.IsEmpty()
                    ? FString::Printf(TEXT("Override material '%s' failed profile '%s' contract"), *OverridePath, *ProfileToken)
                    : ContractError;
                return false;
            }

            OutError = FString::Printf(TEXT("Override material path not found: %s"), *OverridePath);
            return false;
        }
    }

    const bool bNeedsProjectionContract = !ProfileToken.Equals(MaterialProfileDirect, ESearchCase::IgnoreCase);
    const bool bAllowSemanticFallbacks = CVarRshipContentMappingAllowSemanticFallbacks.GetValueOnGameThread() > 0;

    FString ProfileResolveError;
    if (ResolveMaterialForProfile(ProfileToken, OutMaterial, ProfileResolveError) && OutMaterial)
    {
        OutError = ProfileResolveError;
        return true;
    }

    if (!bAllowSemanticFallbacks)
    {
        OutError = ProfileResolveError.IsEmpty()
            ? FString::Printf(TEXT("No material found for mapping profile '%s'"), *ProfileToken)
            : ProfileResolveError;
        return false;
    }

    // Advanced projection profiles may fall back to the base projection profile when their
    // dedicated material is unavailable. This preserves projector semantics.
    if (bNeedsProjectionContract
        && !ProfileToken.Equals(MaterialProfileProjection, ESearchCase::IgnoreCase))
    {
        FString ProjectionResolveError;
        if (ResolveMaterialForProfile(MaterialProfileProjection, OutMaterial, ProjectionResolveError) && OutMaterial)
        {
            const FString PrimaryReason = ProfileResolveError.IsEmpty()
                ? TEXT("no profile-compatible material found")
                : ProfileResolveError;
            OutError = FString::Printf(
                TEXT("Mapping profile '%s' material unavailable (%s). Using projection profile material fallback."),
                *ProfileToken,
                *PrimaryReason);
            return true;
        }

        if (!ProjectionResolveError.IsEmpty())
        {
            if (!ProfileResolveError.IsEmpty())
            {
                ProfileResolveError = FString::Printf(TEXT("%s; projection fallback failed: %s"),
                    *ProfileResolveError,
                    *ProjectionResolveError);
            }
            else
            {
                ProfileResolveError = ProjectionResolveError;
            }
        }
    }

    if (!ProfileToken.Equals(MaterialProfileDirect, ESearchCase::IgnoreCase))
    {
        FString DirectResolveError;
        UMaterialInterface* DirectCandidate = nullptr;
        if (ResolveMaterialForProfile(MaterialProfileDirect, DirectCandidate, DirectResolveError) && DirectCandidate)
        {
            FString DirectContractError;
            if (!ValidateMaterialContract(DirectCandidate, DirectContractError, bNeedsProjectionContract))
            {
                DirectResolveError = DirectContractError.IsEmpty()
                    ? FString::Printf(
                        TEXT("Direct profile fallback material '%s' does not satisfy required contract"),
                        *DirectCandidate->GetName())
                    : DirectContractError;
            }
            else
            {
                OutMaterial = DirectCandidate;
            }
        }

        if (OutMaterial)
        {
            const FString PrimaryReason = ProfileResolveError.IsEmpty()
                ? TEXT("no profile-compatible material found")
                : ProfileResolveError;
            OutError = FString::Printf(
                TEXT("Mapping profile '%s' material unavailable (%s). Using direct profile material fallback."),
                *ProfileToken,
                *PrimaryReason);
            return true;
        }

        if (!ProfileResolveError.IsEmpty() && !DirectResolveError.IsEmpty())
        {
            OutError = FString::Printf(TEXT("%s; direct fallback failed: %s"),
                *ProfileResolveError,
                *DirectResolveError);
        }
        else
        {
            OutError = !ProfileResolveError.IsEmpty() ? ProfileResolveError : DirectResolveError;
        }
        return false;
    }

    OutError = ProfileResolveError;
    return false;
}

bool URshipContentMappingManager::ResolveContentMappingMaterial(bool bRequireProjectionContract)
{
    UMaterialInterface* PreviousMaterial = ContentMappingMaterial;
    UMaterialInterface* ResolvedMaterial = nullptr;
    FString ResolveError;

    const FString ProfileToken = bRequireProjectionContract ? MaterialProfileProjection : MaterialProfileDirect;
    const bool bResolved = ResolveMaterialForProfile(ProfileToken, ResolvedMaterial, ResolveError);

    if (!bRequireProjectionContract)
    {
        ContentMappingMaterial = bResolved ? ResolvedMaterial : nullptr;
    }

    if (PreviousMaterial != ContentMappingMaterial)
    {
        bMaterialContractChecked = false;
        LastContractMaterial = nullptr;
        MaterialContractError.Empty();
    }

    if (!bResolved && !ResolveError.IsEmpty())
    {
        UE_LOG(LogRshipExec, Warning, TEXT("%s"), *ResolveError);
    }

    return bResolved;
}

void URshipContentMappingManager::RunRuntimePreflight(bool bForceMaterialResolve)
{
    const double NowSeconds = FPlatformTime::Seconds();
    const bool bMaterialMissing = !ContentMappingMaterial || !IsValid(ContentMappingMaterial);
    const bool bRetryReady = NextMaterialResolveAttemptSeconds <= 0.0 || NowSeconds >= NextMaterialResolveAttemptSeconds;

    if (bForceMaterialResolve
        || (bMaterialMissing && bRetryReady))
    {
        ResolveContentMappingMaterial(/*bRequireProjectionContract=*/false);
        if (!ContentMappingMaterial)
        {
            NextMaterialResolveAttemptSeconds = NowSeconds + 1.0;
        }
        else
        {
            NextMaterialResolveAttemptSeconds = 0.0;
        }
    }

    EnsureMaterialContract(/*bRequireProjectionContract=*/false);
    if (!ContentMappingMaterial)
    {
        SetRuntimeHealth(
            ERshipContentMappingRuntimeHealth::Degraded,
            TEXT("Default content mapping material unavailable. Configure ContentMappingMaterialPath for direct/feed profile."));
        return;
    }
    if (!bMaterialContractValid)
    {
        SetRuntimeHealth(
            ERshipContentMappingRuntimeHealth::Degraded,
            MaterialContractError.IsEmpty()
                ? TEXT("Content mapping material contract invalid")
                : MaterialContractError);
        return;
    }

    SetRuntimeHealth(ERshipContentMappingRuntimeHealth::Ready, FString());
}

void URshipContentMappingManager::SetRuntimeHealth(
    ERshipContentMappingRuntimeHealth NewHealth,
    const FString& NewReason)
{
    const FString SanitizedReason = NewReason.TrimStartAndEnd();
    const bool bChanged = RuntimeHealth != NewHealth
        || !RuntimeHealthReason.Equals(SanitizedReason, ESearchCase::CaseSensitive);
    if (!bChanged)
    {
        return;
    }

    const FString PreviousToken = RuntimeHealthToToken(RuntimeHealth);
    RuntimeHealth = NewHealth;
    RuntimeHealthReason = SanitizedReason;

    UE_LOG(LogRshipExec, Warning, TEXT("ContentMapping runtime health changed %s -> %s (%s)"),
        *PreviousToken,
        *GetRuntimeHealthStatusToken(),
        RuntimeHealthReason.IsEmpty() ? TEXT("no reason") : *RuntimeHealthReason);

    ApplyRuntimeHealthToStates(/*bEmitChanges=*/true);
}

bool URshipContentMappingManager::IsRuntimeBlocked() const
{
    return RuntimeHealth == ERshipContentMappingRuntimeHealth::Blocked;
}

FString URshipContentMappingManager::GetRuntimeHealthStatusToken() const
{
    return RuntimeHealthToToken(RuntimeHealth);
}

FString URshipContentMappingManager::GetTargetStatusForEnabledFlag(bool bEnabled) const
{
    if (!bEnabled)
    {
        return TEXT("disabled");
    }
    if (RuntimeHealth == ERshipContentMappingRuntimeHealth::Blocked)
    {
        return TEXT("blocked");
    }
    if (RuntimeHealth == ERshipContentMappingRuntimeHealth::Degraded)
    {
        return TEXT("degraded");
    }
    return TEXT("enabled");
}

void URshipContentMappingManager::ApplyRuntimeHealthToStates(bool bEmitChanges)
{
    if (RuntimeHealth == ERshipContentMappingRuntimeHealth::Blocked)
    {
        const FString RuntimeError = BuildRuntimeBlockedError(RuntimeHealthReason);
        if (bEmitChanges)
        {
            for (const auto& Pair : RenderContexts)
            {
                EmitContextState(Pair.Value);
            }
        }
        for (auto& Pair : MappingSurfaces)
        {
            FRshipMappingSurfaceState& SurfaceState = Pair.Value;
            if (!SurfaceState.bEnabled)
            {
                continue;
            }

            RestoreSurfaceMaterials(SurfaceState);
            SurfaceState.LastError = RuntimeError;
            if (bEmitChanges)
            {
                EmitSurfaceState(SurfaceState);
            }
        }

        for (auto& Pair : Mappings)
        {
            FRshipContentMappingState& MappingState = Pair.Value;
            if (!MappingState.bEnabled)
            {
                continue;
            }

            MappingState.LastError = RuntimeError;
            if (bEmitChanges)
            {
                EmitMappingState(MappingState);
            }
        }
        return;
    }

    for (auto& Pair : MappingSurfaces)
    {
        FRshipMappingSurfaceState& SurfaceState = Pair.Value;
        if (IsRuntimeBlockedError(SurfaceState.LastError))
        {
            SurfaceState.LastError.Empty();
            if (bEmitChanges)
            {
                EmitSurfaceState(SurfaceState);
            }
        }
    }

    for (auto& Pair : Mappings)
    {
        FRshipContentMappingState& MappingState = Pair.Value;
        if (IsRuntimeBlockedError(MappingState.LastError))
        {
            MappingState.LastError.Empty();
            if (bEmitChanges)
            {
                EmitMappingState(MappingState);
            }
        }
    }

    if (bEmitChanges)
    {
        for (const auto& Pair : RenderContexts)
        {
            EmitContextState(Pair.Value);
        }
    }
}

FString URshipContentMappingManager::BuildRuntimeBlockedError(const FString& Reason)
{
    const FString SanitizedReason = Reason.TrimStartAndEnd();
    if (SanitizedReason.IsEmpty())
    {
        return FString::Printf(TEXT("%s%s"), RuntimeBlockedErrorPrefix, TEXT("runtime preflight failed"));
    }
    return FString::Printf(TEXT("%s%s"), RuntimeBlockedErrorPrefix, *SanitizedReason);
}

bool URshipContentMappingManager::IsRuntimeBlockedError(const FString& ErrorText)
{
    return ErrorText.StartsWith(RuntimeBlockedErrorPrefix, ESearchCase::CaseSensitive);
}

void URshipContentMappingManager::EnsureMaterialContract(bool bRequireProjectionContract)
{
    (void)bRequireProjectionContract;
    if (bMaterialContractChecked && LastContractMaterial.Get() == ContentMappingMaterial)
    {
        return;
    }

    bMaterialContractChecked = true;
    LastContractMaterial = ContentMappingMaterial;
    bMaterialContractValid = ValidateMaterialContractForProfile(ContentMappingMaterial, MaterialProfileDirect, MaterialContractError);
    if (!bMaterialContractValid)
    {
        UE_LOG(LogRshipExec, Error, TEXT("%s"), *MaterialContractError);
    }
}

FString URshipContentMappingManager::GetAssetCacheDirectory() const
{
    return FPaths::ProjectSavedDir() / TEXT("Rship/AssetCache");
}

FString URshipContentMappingManager::GetAssetCachePathForId(const FString& AssetId) const
{
    FString SafeName = FPaths::MakeValidFileName(AssetId);
    if (SafeName.IsEmpty())
    {
        SafeName = TEXT("asset");
    }
    return GetAssetCacheDirectory() / (SafeName + TEXT(".img"));
}

void URshipContentMappingManager::RequestAssetDownload(const FString& AssetId)
{
    if (!AssetStoreClient || AssetId.IsEmpty())
    {
        return;
    }

    if (PendingAssetDownloads.Contains(AssetId))
    {
        return;
    }

    PendingAssetDownloads.Add(AssetId);
    AssetStoreClient->DownloadAsset(AssetId);
}

void URshipContentMappingManager::OnAssetDownloaded(const FString& AssetId, const FString& LocalPath)
{
    PendingAssetDownloads.Remove(AssetId);

    FString CachePath = GetAssetCachePathForId(AssetId);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(CachePath), true);

    if (!LocalPath.IsEmpty() && LocalPath != CachePath)
    {
        IFileManager::Get().Copy(*CachePath, *LocalPath);
    }

    const FString UsePath = IFileManager::Get().FileExists(*CachePath) ? CachePath : LocalPath;
    UTexture2D* Texture = LoadTextureFromFile(UsePath);
    if (Texture)
    {
        AssetTextureCache.Add(AssetId, Texture);
        for (auto& Pair : RenderContexts)
        {
            if (Pair.Value.AssetId == AssetId || Pair.Value.DepthAssetId == AssetId)
            {
                ResolveRenderContext(Pair.Value);
                EmitContextState(Pair.Value);
            }
        }
        MarkMappingsDirty();
    }
}

void URshipContentMappingManager::OnAssetDownloadFailed(const FString& AssetId, const FString& ErrorMessage)
{
    PendingAssetDownloads.Remove(AssetId);

    for (auto& Pair : RenderContexts)
    {
        if (Pair.Value.AssetId == AssetId || Pair.Value.DepthAssetId == AssetId)
        {
            Pair.Value.LastError = ErrorMessage;
            EmitContextState(Pair.Value);
        }
    }
}

UTexture2D* URshipContentMappingManager::LoadTextureFromFile(const FString& LocalPath) const
{
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *LocalPath))
    {
        return nullptr;
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    EImageFormat Format = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
    if (Format == EImageFormat::Invalid)
    {
        return nullptr;
    }

    TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(Format);
    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        return nullptr;
    }

    TArray<uint8> RawData;
    if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
    {
        return nullptr;
    }

    UTexture2D* Texture = UTexture2D::CreateTransient(Wrapper->GetWidth(), Wrapper->GetHeight(), PF_B8G8R8A8);
    if (!Texture || !Texture->GetPlatformData())
    {
        return nullptr;
    }

    void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

    Texture->SRGB = true;
    Texture->UpdateResource();

    return Texture;
}

FString URshipContentMappingManager::GetStringField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, const FString& DefaultValue)
{
    if (!Obj.IsValid())
    {
        return DefaultValue;
    }
    if (Obj->HasTypedField<EJson::String>(Field))
    {
        return Obj->GetStringField(Field);
    }
    return DefaultValue;
}

bool URshipContentMappingManager::GetBoolField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, bool DefaultValue)
{
    if (!Obj.IsValid())
    {
        return DefaultValue;
    }
    if (Obj->HasTypedField<EJson::Boolean>(Field))
    {
        return Obj->GetBoolField(Field);
    }
    return DefaultValue;
}

int32 URshipContentMappingManager::GetIntField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32 DefaultValue)
{
    if (!Obj.IsValid())
    {
        return DefaultValue;
    }
    if (Obj->HasTypedField<EJson::Number>(Field))
    {
        return static_cast<int32>(Obj->GetNumberField(Field));
    }
    return DefaultValue;
}

float URshipContentMappingManager::GetNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, float DefaultValue)
{
    if (!Obj.IsValid())
    {
        return DefaultValue;
    }
    if (Obj->HasTypedField<EJson::Number>(Field))
    {
        return static_cast<float>(Obj->GetNumberField(Field));
    }
    return DefaultValue;
}

TArray<FString> URshipContentMappingManager::GetStringArrayField(const TSharedPtr<FJsonObject>& Obj, const FString& Field)
{
    TArray<FString> Result;
    if (!Obj.IsValid() || !Obj->HasTypedField<EJson::Array>(Field))
    {
        return Result;
    }

    const TArray<TSharedPtr<FJsonValue>> Values = Obj->GetArrayField(Field);
    for (const TSharedPtr<FJsonValue>& Value : Values)
    {
        if (Value.IsValid() && Value->Type == EJson::String)
        {
            Result.Add(Value->AsString());
        }
    }
    return Result;
}

TArray<int32> URshipContentMappingManager::GetIntArrayField(const TSharedPtr<FJsonObject>& Obj, const FString& Field)
{
    TArray<int32> Result;
    if (!Obj.IsValid() || !Obj->HasTypedField<EJson::Array>(Field))
    {
        return Result;
    }

    const TArray<TSharedPtr<FJsonValue>> Values = Obj->GetArrayField(Field);
    for (const TSharedPtr<FJsonValue>& Value : Values)
    {
        if (Value.IsValid() && Value->Type == EJson::Number)
        {
            Result.Add(static_cast<int32>(Value->AsNumber()));
        }
    }
    return Result;
}
