// Content Mapping Manager implementation

#include "RshipContentMappingManager.h"
#include "RshipSubsystem.h"
#include "RshipSettings.h"
#include "RshipAssetStoreClient.h"
#include "RshipCameraActor.h"
#include "RshipSceneConverter.h"
#include "Logs.h"

#include "Dom/JsonValue.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
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

static const FName ParamContextTexture(TEXT("RshipContextTexture"));
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

static TAutoConsoleVariable<int32> CVarRshipContentMappingPerfStats(
    TEXT("rship.cm.perf_stats"),
    0,
    TEXT("Enable content mapping perf stats logging once per second."));

static TAutoConsoleVariable<int32> CVarRshipContentMappingCaptureUseMainView(
    TEXT("rship.cm.capture_use_main_view"),
    1,
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
    0,
    TEXT("Capture quality profile for mapping contexts. 0=performance, 1=balanced, 2=fidelity."));

static TAutoConsoleVariable<float> CVarRshipContentMappingCaptureMaxViewDistance(
    TEXT("rship.cm.capture_max_view_distance"),
    0.0f,
    TEXT("Optional max view distance override for mapping scene captures (0 disables)."));

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

    ACameraActor* FindAnySourceCameraActor()
    {
        if (!GEngine)
        {
            return nullptr;
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

                for (TActorIterator<ACameraActor> It(World); It; ++It)
                {
                    ACameraActor* Candidate = *It;
                    if (Candidate && !Candidate->IsA<ARshipCameraActor>())
                    {
                        return Candidate;
                    }
                }
            }
        }

        return nullptr;
    }

    UTexture* GetDefaultPreviewTexture()
    {
        static TWeakObjectPtr<UTexture> CachedDefaultTexture;
        if (!CachedDefaultTexture.IsValid())
        {
            CachedDefaultTexture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
        }
        return CachedDefaultTexture.Get();
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

    ACameraActor* FindSourceCameraActorByEntityId(URshipSubsystem* Subsystem, const FString& CameraId)
    {
        if (!Subsystem || !GEngine)
        {
            return nullptr;
        }

        if (CameraId.IsEmpty())
        {
            return FindAnySourceCameraActor();
        }

        URshipSceneConverter* Converter = Subsystem->GetSceneConverter();
        ACameraActor* FirstCameraFallback = nullptr;

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

                for (TActorIterator<ACameraActor> It(World); It; ++It)
                {
                    ACameraActor* Candidate = *It;
                    if (!Candidate || Candidate->IsA<ARshipCameraActor>())
                    {
                        continue;
                    }

                    if (!FirstCameraFallback)
                    {
                        FirstCameraFallback = Candidate;
                    }

                    const FString CandidateName = Candidate->GetName();
                    const FString CandidateLabel = GetActorLabelCompat(Candidate);
                    if (CandidateName.Equals(CameraId, ESearchCase::IgnoreCase)
                        || CandidateLabel.Equals(CameraId, ESearchCase::IgnoreCase))
                    {
                        return Candidate;
                    }

                    if (Converter)
                    {
                        const FString ConvertedId = Converter->GetConvertedEntityId(Candidate);
                        if (ConvertedId.Equals(CameraId, ESearchCase::CaseSensitive)
                            || ConvertedId.Equals(CameraId, ESearchCase::IgnoreCase))
                        {
                            return Candidate;
                        }
                    }
                }
            }
        }

        return FirstCameraFallback ? FirstCameraFallback : FindAnySourceCameraActor();
    }

    AActor* FindSourceAnchorActorByEntityId(URshipSubsystem* Subsystem, const FString& SourceId)
    {
        if (!Subsystem || !GEngine)
        {
            return nullptr;
        }

        const FString RequestedId = SourceId.TrimStartAndEnd();
        const FString RequestedShortId = GetShortIdToken(RequestedId);
        if (RequestedId.IsEmpty())
        {
            return nullptr;
        }

        URshipSceneConverter* Converter = Subsystem->GetSceneConverter();
        AActor* FirstFallback = nullptr;

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
                    if (!Candidate || Candidate->IsA<ARshipCameraActor>())
                    {
                        continue;
                    }

                    if (!FirstFallback)
                    {
                        FirstFallback = Candidate;
                    }

                    const FString CandidateName = Candidate->GetName();
                    const FString CandidateLabel = GetActorLabelCompat(Candidate);
                    if (CandidateName.Equals(RequestedId, ESearchCase::IgnoreCase)
                        || CandidateLabel.Equals(RequestedId, ESearchCase::IgnoreCase)
                        || CandidateName.Equals(RequestedShortId, ESearchCase::IgnoreCase)
                        || CandidateLabel.Equals(RequestedShortId, ESearchCase::IgnoreCase))
                    {
                        return Candidate;
                    }

                    if (Converter)
                    {
                        const FString ConvertedId = Converter->GetConvertedEntityId(Candidate);
                        const FString ConvertedShortId = GetShortIdToken(ConvertedId);
                        if (ConvertedId.Equals(RequestedId, ESearchCase::CaseSensitive)
                            || ConvertedId.Equals(RequestedId, ESearchCase::IgnoreCase)
                            || ConvertedShortId.Equals(RequestedId, ESearchCase::IgnoreCase)
                            || ConvertedId.Equals(RequestedShortId, ESearchCase::IgnoreCase)
                            || ConvertedShortId.Equals(RequestedShortId, ESearchCase::IgnoreCase))
                        {
                            return Candidate;
                        }
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
            && A.Width == B.Width
            && A.Height == B.Height
            && A.CaptureMode == B.CaptureMode
            && A.DepthCaptureMode == B.DepthCaptureMode
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

        return Value;
    }

    FString NormalizeProjectionModeToken(const FString& InMode, const FString& DefaultMode = TEXT("perspective"))
    {
        FString Value = InMode.TrimStartAndEnd().ToLower();
        if (Value.IsEmpty())
        {
            Value = DefaultMode.TrimStartAndEnd().ToLower();
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
        State.CaptureMode = State.CaptureMode.TrimStartAndEnd();
        State.DepthCaptureMode = State.DepthCaptureMode.TrimStartAndEnd();

        FString SourceType = NormalizeSourceTypeToken(State.SourceType);
        if (SourceType.IsEmpty())
        {
            SourceType = (!State.AssetId.IsEmpty() && State.CameraId.IsEmpty()) ? TEXT("asset-store") : TEXT("camera");
        }

        if (SourceType != TEXT("camera") && SourceType != TEXT("asset-store"))
        {
            if (!State.CameraId.IsEmpty())
            {
                SourceType = TEXT("camera");
            }
            else if (!State.AssetId.IsEmpty())
            {
                SourceType = TEXT("asset-store");
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

        State.SourceType = SourceType;

        if (State.SourceType == TEXT("camera"))
        {
            State.AssetId.Reset();
        }
        else if (State.SourceType == TEXT("asset-store"))
        {
            State.CameraId.Reset();
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
                TSharedPtr<FJsonObject> FeedRect = State.Config->HasTypedField<EJson::Object>(TEXT("feedRect"))
                    ? State.Config->GetObjectField(TEXT("feedRect"))
                    : MakeShared<FJsonObject>();
                FeedRect->SetNumberField(TEXT("u"), ReadNumber(FeedRect, TEXT("u"), 0.0f));
                FeedRect->SetNumberField(TEXT("v"), ReadNumber(FeedRect, TEXT("v"), 0.0f));
                FeedRect->SetNumberField(TEXT("width"), ReadNumber(FeedRect, TEXT("width"), 1.0f));
                FeedRect->SetNumberField(TEXT("height"), ReadNumber(FeedRect, TEXT("height"), 1.0f));
                State.Config->SetObjectField(TEXT("feedRect"), FeedRect);

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
                    if (!FeedV2->HasTypedField<EJson::Array>(TEXT("routes")) && !FeedV2->HasTypedField<EJson::Array>(TEXT("links")))
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
}

void URshipContentMappingManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    bMappingsArmed = true;
    bCoveragePreviewEnabled = false;

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

    // Use one deterministic material resolution order across all platforms.
    if (Settings && !Settings->ContentMappingMaterialPath.IsEmpty())
    {
        ContentMappingMaterial = TryLoadMaterialPath(Settings->ContentMappingMaterialPath);
        if (!ContentMappingMaterial)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("ContentMapping material override failed to load: %s"),
                *Settings->ContentMappingMaterialPath);
        }
    }

    if (!ContentMappingMaterial)
    {
        static const TCHAR* RuntimeMaterialCandidates[] =
        {
            TEXT("/RshipExec/Materials/MI_RshipContentMapping.MI_RshipContentMapping"),
            TEXT("/RshipExec/Materials/M_RshipContentMapping.M_RshipContentMapping")
        };
        for (const TCHAR* CandidatePath : RuntimeMaterialCandidates)
        {
            if (UMaterialInterface* Candidate = TryLoadMaterialPath(CandidatePath))
            {
                ContentMappingMaterial = Candidate;
                break;
            }
        }
    }

#if WITH_EDITOR
    if (!ContentMappingMaterial)
    {
        // Keep editor usable even when plugin/project content is missing.
        BuildFallbackMaterial();
    }
#endif

    if (!ContentMappingMaterial)
    {
        ContentMappingMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
        UE_LOG(LogRshipExec, Warning, TEXT("Runtime mapping material unavailable; using Engine DefaultMaterial."));
    }

    LoadCache();
    MarkMappingsDirty();
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
        Pair.Value.DepthCaptureComponent.Reset();
        Pair.Value.DepthRenderTarget.Reset();
        Pair.Value.ResolvedTexture = nullptr;
        Pair.Value.ResolvedDepthTexture = nullptr;
    }

    RenderContexts.Empty();
    MappingSurfaces.Empty();
    Mappings.Empty();
    FeedCompositeTargets.Empty();
    FeedCompositeStaticSignatures.Empty();
    FeedSingleRtBindingCache.Empty();
    EffectiveSurfaceIdsCache.Empty();
    RequiredContextIdsCache.Empty();
    RenderContextRuntimeStates.Empty();
    CachedEnabledTextureContextId.Reset();
    CachedAnyTextureContextId.Reset();
    CachedEnabledContextId.Reset();
    CachedAnyContextId.Reset();
    AssetTextureCache.Empty();
    PendingAssetDownloads.Empty();
    bMappingsArmed = false;
    bRuntimePreparePending = true;
}

void URshipContentMappingManager::Tick(float DeltaTime)
{
    if (!Subsystem)
    {
        return;
    }

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
        const double SaveStartSeconds = FPlatformTime::Seconds();
        SaveCache();
        bCacheDirty = false;
        LastTickMsCacheSave = static_cast<float>((FPlatformTime::Seconds() - SaveStartSeconds) * 1000.0);
    }

    const bool bHasEnabledMappings = HasAnyEnabledMappings();
    if (bDidRebuild || bHasEnabledMappings)
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
                TEXT("CMPerf total=%.3fms rebuild=%.3fms refresh=%.3fms cache=%.3fms enabled=%d contexts=%d appliedSurfaces=%d"),
                LastTickMsTotal,
                LastTickMsRebuild,
                LastTickMsRefresh,
                LastTickMsCacheSave,
                LastTickEnabledMappings,
                LastTickActiveContexts,
                LastTickAppliedSurfaces);
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

            const bool bConnected = Subsystem && Subsystem->IsConnected();
            FString DebugText = FString::Printf(
                TEXT("Rship Content Mapping (%s)\nContexts: %d (%d err)  Surfaces: %d (%d err)  Mappings: %d (%d err)\nPending assets: %d"),
                bConnected ? TEXT("connected") : TEXT("offline"),
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
        if (ARshipCameraActor* CameraActor = ContextState.CameraActor.Get())
        {
            CameraActor->bEnableSceneCapture = false;
            if (CameraActor->SceneCapture)
            {
                CameraActor->SceneCapture->bCaptureEveryFrame = false;
                CameraActor->SceneCapture->bCaptureOnMovement = false;
            }
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

    for (auto& Pair : RenderContexts)
    {
        FRshipRenderContextState& ContextState = Pair.Value;
        NormalizeRenderContextState(ContextState);

        if (!RequiredContextIds.Contains(ContextState.Id))
        {
            ContextState.ResolvedTexture = nullptr;
            ContextState.ResolvedDepthTexture = nullptr;
            ContextState.LastError.Empty();
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

        ResolveRenderContext(ContextState);
        if (ContextState.ResolvedTexture)
        {
            ++ActiveResolvedContexts;
            if (!Signature.IsEmpty())
            {
                SignatureToResolvedContextId.Add(Signature, ContextState.Id);
            }
        }
    }

    RefreshResolvedContextFallbackIds();
    LastTickActiveContexts = ActiveResolvedContexts;

    int32 EnabledMappingCount = 0;
    int32 AppliedSurfaceCount = 0;
    FString FirstMappingError;
    UWorld* PreferredWorld = GetBestWorld();
    const double NowSeconds = FPlatformTime::Seconds();

    for (auto& MappingPair : Mappings)
    {
        FRshipContentMappingState& MappingState = MappingPair.Value;
        if (!MappingState.bEnabled)
        {
            continue;
        }
        ++EnabledMappingCount;

        const bool bFeedV2 = IsFeedV2Mapping(MappingState);
        const FRshipRenderContextState* ContextState = ResolveEffectiveContextState(MappingState, bFeedV2);
        if (bFeedV2 && (!ContextState || !ContextState->ResolvedTexture))
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
        for (const FString& SurfaceId : EffectiveSurfaceIds)
        {
            FRshipMappingSurfaceState* SurfaceState = MappingSurfaces.Find(SurfaceId);
            if (!SurfaceState || !SurfaceState->bEnabled)
            {
                continue;
            }

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
                continue;
            }

            ApplyMappingToSurface(MappingState, *SurfaceState, ContextState);
            if (SurfaceState->LastError.IsEmpty())
            {
                ++AppliedSurfaceCount;
            }
        }

        if (FirstMappingError.IsEmpty() && !MappingState.LastError.IsEmpty())
        {
            FirstMappingError = MappingState.LastError;
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
        const double NowSeconds = FPlatformTime::Seconds();
        if ((NowSeconds - LastNoSurfaceWarningTime) >= 1.0)
        {
            LastNoSurfaceWarningTime = NowSeconds;
            UE_LOG(LogRshipExec, Warning,
                TEXT("ContentMapping produced no applied surfaces (enabledMappings=%d, contexts=%d, contextsWithTexture=%d, surfaces=%d, firstError='%s')"),
                EnabledMappingCount,
                RenderContexts.Num(),
                ContextsWithTexture,
                MappingSurfaces.Num(),
                *FirstMappingError);
        }
    }

    LastTickEnabledMappings = EnabledMappingCount;
    LastTickAppliedSurfaces = AppliedSurfaceCount;
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

void URshipContentMappingManager::SetDebugOverlayEnabled(bool bEnabled)
{
    bDebugOverlayEnabled = bEnabled;
    DebugOverlayAccumulated = 0.0f;
}

bool URshipContentMappingManager::IsDebugOverlayEnabled() const
{
    return bDebugOverlayEnabled;
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

FString URshipContentMappingManager::CreateRenderContext(const FRshipRenderContextState& InState)
{
    ArmMappings();

    FRshipRenderContextState NewState = InState;
    if (NewState.Id.IsEmpty())
    {
        NewState.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    }
    NormalizeRenderContextState(NewState);
    RenderContexts.Add(NewState.Id, NewState);
    RenderContextRuntimeStates.Remove(NewState.Id);
    ResolveRenderContext(RenderContexts[NewState.Id]);
    RegisterContextTarget(RenderContexts[NewState.Id]);
    EmitContextState(RenderContexts[NewState.Id]);
    if (Subsystem)
    {
        Subsystem->SetItem(TEXT("RenderContext"), BuildRenderContextJson(RenderContexts[NewState.Id]), ERshipMessagePriority::High, NewState.Id);
    }
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


    TWeakObjectPtr<ARshipCameraActor> PreviousCamera = Stored.CameraActor;
    TWeakObjectPtr<ACameraActor> PreviousSourceCamera = Stored.SourceCameraActor;
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
    if (Subsystem)
    {
        Subsystem->SetItem(TEXT("RenderContext"), BuildRenderContextJson(Stored), ERshipMessagePriority::High, InState.Id);
    }
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
    if (Subsystem)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), Id);
        Obj->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->DelItem(TEXT("RenderContext"), Obj, ERshipMessagePriority::High, Id);
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
    if (Subsystem)
    {
        Subsystem->SetItem(TEXT("MappingSurface"), BuildMappingSurfaceJson(MappingSurfaces[NewState.Id]), ERshipMessagePriority::High, NewState.Id);
    }
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
    if (Subsystem)
    {
        Subsystem->SetItem(TEXT("MappingSurface"), BuildMappingSurfaceJson(Stored), ERshipMessagePriority::High, InState.Id);
    }
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

    if (Subsystem)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), Id);
        Obj->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->DelItem(TEXT("MappingSurface"), Obj, ERshipMessagePriority::High, Id);
    }
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
    RegisterMappingTarget(Mappings[NewState.Id]);
    EmitMappingState(Mappings[NewState.Id]);
    if (Subsystem)
    {
        Subsystem->SetItem(TEXT("Mapping"), BuildMappingJson(Mappings[NewState.Id]), ERshipMessagePriority::High, NewState.Id);
    }
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
    NormalizeMappingState(Clamped);
    if (EnsureMappingRuntimeReady(Clamped))
    {
        NormalizeMappingState(Clamped);
    }
    if (!IsFeedV2Mapping(Clamped))
    {
        RemoveFeedCompositeTexturesForMapping(InState.Id);
    }
    if (const FRshipContentMappingState* Existing = Mappings.Find(InState.Id))
    {
        if (AreMappingStatesEquivalent(*Existing, Clamped))
        {
            return true;
        }
    }

    Mappings[InState.Id] = Clamped;
    TrackPendingMappingUpsert(Mappings[InState.Id]);
    RegisterMappingTarget(Mappings[InState.Id]);
    EmitMappingState(Mappings[InState.Id]);
    if (Subsystem)
    {
        Subsystem->SetItem(TEXT("Mapping"), BuildMappingJson(Mappings[InState.Id]), ERshipMessagePriority::High, InState.Id);
    }
    MarkMappingsDirty();
    MarkCacheDirty();
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

    if (Subsystem)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), Id);
        Obj->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->DelItem(TEXT("Mapping"), Obj, ERshipMessagePriority::High, Id);
    }
    DeleteTargetForPath(BuildMappingTargetId(Id));
    MarkMappingsDirty();
    if (bMappingsArmed)
    {
        RebuildMappings();
        bMappingsDirty = false;
        RefreshLiveMappings();
    }
    MarkCacheDirty();
    return true;
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
    TWeakObjectPtr<ARshipCameraActor> PreviousCamera = Stored.CameraActor;
    TWeakObjectPtr<ACameraActor> PreviousSourceCamera = Stored.SourceCameraActor;
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
            if (bMappingsArmed)
            {
                RebuildMappings();
                bMappingsDirty = false;
                RefreshLiveMappings();
            }
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
        State.Config = Data->GetObjectField(TEXT("config"));
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
    if (!IsFeedV2Mapping(State))
    {
        RemoveFeedCompositeTexturesForMapping(Id);
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
        if (AreMappingStatesEquivalent(*Existing, State))
        {
            return;
        }
    }

    FRshipContentMappingState& Stored = Mappings.FindOrAdd(Id);
    Stored = State;

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
    PendingMappingDeletes.Remove(State.Id);
    PendingMappingUpserts.Add(State.Id, State);
    PendingMappingUpsertExpiry.Add(State.Id, ExpiresAt);
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

void URshipContentMappingManager::MarkMappingsDirty()
{
    bMappingsDirty = true;
    bRuntimePreparePending = true;
    FeedSingleRtBindingCache.Empty();
    EffectiveSurfaceIdsCache.Empty();
    RequiredContextIdsCache.Empty();
    CachedEnabledTextureContextId.Reset();
    CachedAnyTextureContextId.Reset();
    CachedEnabledContextId.Reset();
    CachedAnyContextId.Reset();
    RuntimeStateRevision++;
    if (RuntimeStateRevision == 0)
    {
        RuntimeStateRevision = 1;
    }
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

    const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();
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

            LastValidWorld = World;
            return World;
        }
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

    if (!ContextState.bEnabled)
    {
        if (ARshipCameraActor* CameraActor = ContextState.CameraActor.Get())
        {
            CameraActor->bEnableSceneCapture = false;
            if (CameraActor->SceneCapture)
            {
                CameraActor->SceneCapture->bCaptureEveryFrame = false;
                CameraActor->SceneCapture->bCaptureOnMovement = false;
            }
        }
        if (USceneCaptureComponent2D* DepthCapture = ContextState.DepthCaptureComponent.Get())
        {
            DepthCapture->bCaptureEveryFrame = false;
            DepthCapture->bCaptureOnMovement = false;
        }
        return;
    }

    if (ContextState.SourceType.Equals(TEXT("camera"), ESearchCase::IgnoreCase))
    {
        if (ContextState.CameraId.IsEmpty())
        {
            if (ACameraActor* FallbackCamera = FindAnySourceCameraActor())
            {
                FString ResolvedCameraId;
                if (Subsystem)
                {
                    if (URshipSceneConverter* Converter = Subsystem->GetSceneConverter())
                    {
                        ResolvedCameraId = Converter->GetConvertedEntityId(FallbackCamera);
                    }
                }

                if (ResolvedCameraId.IsEmpty())
                {
                    ResolvedCameraId = FallbackCamera->GetName();
                }

                if (!ResolvedCameraId.IsEmpty())
                {
                    ContextState.CameraId = ResolvedCameraId;
                    ContextState.SourceCameraActor = FallbackCamera;
                    MarkCacheDirty();
                    UE_LOG(LogRshipExec, Log, TEXT("ResolveRenderContext[%s]: Auto-selected camera '%s' -> id '%s'"),
                        *ContextState.Id,
                        *FallbackCamera->GetName(),
                        *ResolvedCameraId);
                }
            }

            if (ContextState.CameraId.IsEmpty())
            {
                ContextState.LastError = TEXT("CameraId not set");
                return;
            }
        }

        UWorld* PreferredWorld = GetBestWorld();

        ACameraActor* SourceCamera = ContextState.SourceCameraActor.Get();
        if (!SourceCamera || !IsValid(SourceCamera))
        {
            SourceCamera = FindSourceCameraActorByEntityId(Subsystem, ContextState.CameraId);
            ContextState.SourceCameraActor = SourceCamera;
        }
        if (SourceCamera && PreferredWorld && SourceCamera->GetWorld() != PreferredWorld)
        {
            SourceCamera = FindSourceCameraActorByEntityId(Subsystem, ContextState.CameraId);
            ContextState.SourceCameraActor = SourceCamera;
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
        if (SourceAnchorActor)
        {
            World = SourceAnchorActor->GetWorld();
        }
        if (!World)
        {
            World = PreferredWorld;
        }
        if (ARshipCameraActor* ExistingCamera = ContextState.CameraActor.Get())
        {
            if (!World)
            {
                World = ExistingCamera->GetWorld();
            }
        }
        if (!World)
        {
            bNeedsWorldResolutionRetry = true;
            return;
        }

        ARshipCameraActor* CameraActor = ContextState.CameraActor.Get();
        if (CameraActor && CameraActor->GetWorld() != World)
        {
            CameraActor->Destroy();
            ContextState.CameraActor.Reset();
            RenderContextRuntimeStates.Remove(ContextState.Id);
            CameraActor = nullptr;
        }
        if (!CameraActor)
        {
            const FString DesiredActorName = FString::Printf(TEXT("RshipContentMappingCam_%s"), *ContextState.Id);

            // Reuse an existing helper actor if one already exists for this context.
            for (TActorIterator<ARshipCameraActor> It(World); It; ++It)
            {
                ARshipCameraActor* Candidate = *It;
                if (Candidate && Candidate->GetName().Equals(DesiredActorName, ESearchCase::CaseSensitive))
                {
                    CameraActor = Candidate;
                    break;
                }
            }
        }

        if (!CameraActor)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.ObjectFlags |= RF_Transient;
            CameraActor = World->SpawnActor<ARshipCameraActor>(SpawnParams);
        }

        if (!CameraActor)
        {
            ContextState.LastError = TEXT("Failed to spawn camera actor");
            return;
        }

        CameraActor->CameraId = ContextState.CameraId;
        CameraActor->bEnableSceneCapture = true;
        CameraActor->bShowFrustumVisualization = false;
        CameraActor->SetActorTickEnabled(false);
        CameraActor->SetActorHiddenInGame(true);
        if (CameraActor->CameraMesh)
        {
            CameraActor->CameraMesh->SetVisibility(false, true);
            CameraActor->CameraMesh->SetHiddenInGame(true);
        }

        const ERshipCaptureQualityProfile CaptureQualityProfile = GetCaptureQualityProfile();
        const bool bUseMainViewCapture = CVarRshipContentMappingCaptureUseMainView.GetValueOnGameThread() > 0;
        const bool bUseMainViewCamera = bUseMainViewCapture && (CVarRshipContentMappingCaptureUseMainViewCamera.GetValueOnGameThread() > 0);
        const int32 RequestedMainViewDivisor = CVarRshipContentMappingCaptureMainViewDivisor.GetValueOnGameThread();
        const int32 MainViewDivisor = GetEffectiveCaptureDivisor(CaptureQualityProfile, RequestedMainViewDivisor);
        const float RequestedCaptureLodFactor = CVarRshipContentMappingCaptureLodFactor.GetValueOnGameThread();
        const float CaptureLodFactor = GetEffectiveCaptureLodFactor(CaptureQualityProfile, RequestedCaptureLodFactor);
        const float CaptureMaxViewDistance = FMath::Max(0.0f, CVarRshipContentMappingCaptureMaxViewDistance.GetValueOnGameThread());
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
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(bUseMainViewCapture));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(bUseMainViewCamera));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(MainViewDivisor));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(CaptureLodFactor));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(CaptureMaxViewDistance));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(static_cast<uint8>(CaptureQualityProfile)));
        ContextSetupHash = HashCombineFast(ContextSetupHash, GetTypeHash(static_cast<int32>(CaptureSource)));
        FRenderContextRuntimeState& RuntimeState = RenderContextRuntimeStates.FindOrAdd(ContextState.Id);
        const bool bNeedsCaptureSetup = (RuntimeState.SetupHash != ContextSetupHash);

        if (CameraActor->SceneCapture)
        {
            if (!CameraActor->SceneCapture->bCaptureEveryFrame)
            {
                CameraActor->SceneCapture->bCaptureEveryFrame = true;
            }
            if (CameraActor->SceneCapture->bCaptureOnMovement)
            {
                CameraActor->SceneCapture->bCaptureOnMovement = false;
            }
            if (!CameraActor->SceneCapture->bAlwaysPersistRenderingState)
            {
                CameraActor->SceneCapture->bAlwaysPersistRenderingState = true;
            }

            if (bNeedsCaptureSetup)
            {
                CameraActor->SceneCapture->SetRelativeLocation(FVector::ZeroVector);
                CameraActor->SceneCapture->SetRelativeRotation(FRotator::ZeroRotator);
                CameraActor->SceneCapture->bMainViewFamily = bUseMainViewCapture;
                CameraActor->SceneCapture->bMainViewResolution = bUseMainViewCapture;
                CameraActor->SceneCapture->bMainViewCamera = bUseMainViewCamera;
                CameraActor->SceneCapture->bInheritMainViewCameraPostProcessSettings = bUseMainViewCamera;
                CameraActor->SceneCapture->bIgnoreScreenPercentage = false;
                CameraActor->SceneCapture->MainViewResolutionDivisor = FIntPoint(MainViewDivisor, MainViewDivisor);
                CameraActor->SceneCapture->bRenderInMainRenderer = bUseMainViewCapture;
                CameraActor->SceneCapture->LODDistanceFactor = CaptureLodFactor;
                CameraActor->SceneCapture->MaxViewDistanceOverride = CaptureMaxViewDistance;
                CameraActor->SceneCapture->CaptureSource = CaptureSource;
                ApplyCaptureQualityProfile(CameraActor->SceneCapture, CaptureQualityProfile, false);
                RuntimeState.SetupHash = ContextSetupHash;
            }
        }
        else
        {
            ContextState.LastError = TEXT("Camera capture component missing");
            return;
        }

        if (CameraActor->CaptureRenderTarget)
        {
            int32 Width = ContextState.Width > 0 ? ContextState.Width : CameraActor->CaptureRenderTarget->SizeX;
            int32 Height = ContextState.Height > 0 ? ContextState.Height : CameraActor->CaptureRenderTarget->SizeY;
            if (Width <= 0)
            {
                Width = 1920;
            }
            if (Height <= 0)
            {
                Height = 1080;
            }

            if (CameraActor->CaptureRenderTarget->SizeX != Width || CameraActor->CaptureRenderTarget->SizeY != Height)
            {
                CameraActor->CaptureRenderTarget->InitAutoFormat(Width, Height);
                CameraActor->CaptureRenderTarget->UpdateResourceImmediate();
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
        else if (CameraActor->SceneCapture)
        {
            CameraActor->CaptureRenderTarget = NewObject<UTextureRenderTarget2D>(CameraActor);
            const int32 Width = ContextState.Width > 0 ? ContextState.Width : 1920;
            const int32 Height = ContextState.Height > 0 ? ContextState.Height : 1080;
            CameraActor->CaptureRenderTarget->InitAutoFormat(Width, Height);
            CameraActor->CaptureRenderTarget->UpdateResourceImmediate();
            CameraActor->SceneCapture->TextureTarget = CameraActor->CaptureRenderTarget;
            RuntimeState.SetupHash = 0;
            if (ContextState.Width <= 0 || ContextState.Height <= 0)
            {
                ContextState.Width = Width;
                ContextState.Height = Height;
                MarkCacheDirty();
                bRuntimePreparePending = true;
            }
        }

        // Ensure scene capture always writes into the current render target.
        if (CameraActor->SceneCapture && CameraActor->CaptureRenderTarget)
        {
            FTransform DesiredTransform = CameraActor->GetActorTransform();
            float DesiredFov = CameraActor->SceneCapture->FOVAngle;
            bool bHasDesiredTransform = false;
            bool bHasDesiredFov = false;

            if (SourceCamera)
            {
                if (UCameraComponent* SourceCameraComponent = SourceCamera->GetCameraComponent())
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
            else if (SourceAnchorActor)
            {
                DesiredTransform = SourceAnchorActor->GetActorTransform();
                bHasDesiredTransform = true;
            }
            else
            {
                bool bAppliedPlayerViewFallback = false;
                if (World)
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

                if (bAppliedPlayerViewFallback)
                {
                    UE_LOG(LogRshipExec, Verbose, TEXT("ResolveRenderContext[%s]: using player camera fallback for CameraId '%s'"),
                        *ContextState.Id, *ContextState.CameraId);
                }
                else
                {
                    UE_LOG(LogRshipExec, Warning, TEXT("ResolveRenderContext[%s]: no source actor resolved for CameraId '%s'"),
                        *ContextState.Id, *ContextState.CameraId);
                }
            }

            if (bHasDesiredTransform)
            {
                if (!RuntimeState.bHasAppliedTransform
                    || !RuntimeState.LastAppliedTransform.Equals(DesiredTransform, 0.01f))
                {
                    CameraActor->SetActorTransform(DesiredTransform);
                    RuntimeState.LastAppliedTransform = DesiredTransform;
                    RuntimeState.bHasAppliedTransform = true;
                }
            }

            if (bHasDesiredFov && !FMath::IsNearlyEqual(RuntimeState.LastAppliedFov, DesiredFov, 0.01f))
            {
                CameraActor->SceneCapture->FOVAngle = DesiredFov;
                RuntimeState.LastAppliedFov = DesiredFov;
            }

            if (CameraActor->SceneCapture->TextureTarget != CameraActor->CaptureRenderTarget)
            {
                CameraActor->SceneCapture->TextureTarget = CameraActor->CaptureRenderTarget;
            }
        }

        if (ContextState.bDepthCaptureEnabled)
        {
            UTextureRenderTarget2D* DepthTarget = ContextState.DepthRenderTarget.Get();
            if (!DepthTarget)
            {
                DepthTarget = NewObject<UTextureRenderTarget2D>(CameraActor);
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
            if (!DepthCapture)
            {
                DepthCapture = NewObject<USceneCaptureComponent2D>(CameraActor);
                if (DepthCapture)
                {
                    DepthCapture->SetupAttachment(CameraActor->GetRootComponent());
                    DepthCapture->RegisterComponent();
                    ContextState.DepthCaptureComponent = DepthCapture;
                }
            }

            if (DepthCapture)
            {
                if (!DepthCapture->bCaptureEveryFrame)
                {
                    DepthCapture->bCaptureEveryFrame = true;
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
                    DepthCapture->bMainViewFamily = bUseMainViewCapture;
                    DepthCapture->bMainViewResolution = bUseMainViewCapture;
                    DepthCapture->bMainViewCamera = false;
                    DepthCapture->bInheritMainViewCameraPostProcessSettings = false;
                    DepthCapture->bIgnoreScreenPercentage = false;
                    DepthCapture->MainViewResolutionDivisor = FIntPoint(MainViewDivisor, MainViewDivisor);
                    DepthCapture->bRenderInMainRenderer = bUseMainViewCapture;
                    DepthCapture->LODDistanceFactor = CaptureLodFactor;
                    DepthCapture->MaxViewDistanceOverride = CaptureMaxViewDistance;
                    ApplyCaptureQualityProfile(DepthCapture, CaptureQualityProfile, true);
                }

                DepthCapture->TextureTarget = ContextState.DepthRenderTarget.Get();
                if (CameraActor->SceneCapture)
                {
                    DepthCapture->FOVAngle = CameraActor->SceneCapture->FOVAngle;
                }
            }

            ContextState.ResolvedDepthTexture = ContextState.DepthRenderTarget.Get();
        }
        else if (USceneCaptureComponent2D* DepthCapture = ContextState.DepthCaptureComponent.Get())
        {
            DepthCapture->bCaptureEveryFrame = false;
            DepthCapture->bCaptureOnMovement = false;
        }

        ContextState.CameraActor = CameraActor;
        ContextState.ResolvedTexture = CameraActor->CaptureRenderTarget;
        if (CameraActor->CaptureRenderTarget)
        {
            UE_LOG(LogRshipExec, VeryVerbose, TEXT("ResolveRenderContext[%s]: texture ready %dx%d"),
                *ContextState.Id,
                CameraActor->CaptureRenderTarget->SizeX,
                CameraActor->CaptureRenderTarget->SizeY);
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
                        && Candidate->GetName().Equals(RequestedActorObjectName, ESearchCase::IgnoreCase))
                    {
                        return Candidate;
                    }
                }
                return nullptr;
            };

            if (PreferredWorld)
            {
                ExplicitOwner = TryResolveInWorld(PreferredWorld, true);
            }

            if (!ExplicitOwner)
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
                bRequireActorPathMatch = false;
                UE_LOG(LogRshipExec, Warning, TEXT("ResolveMappingSurface[%s]: actorPath '%s' has no mesh, using fallback search"),
                    *SurfaceState.Id,
                    *RequestedActorPath);
            }
        }
        else
        {
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

    if (!BestMesh || !BestOwner)
    {
        bNeedsWorldResolutionRetry = true;
        SurfaceState.LastError = bSawRelevantWorld ? TEXT("No mesh component found") : TEXT("World not available");
        UE_LOG(LogRshipExec, Warning, TEXT("ResolveMappingSurface[%s]: failed (mesh='%s' name='%s' actorPath='%s') -> %s"),
            *SurfaceState.Id,
            *RequestedMeshName,
            *SurfaceName,
            *RequestedActorPath,
            *SurfaceState.LastError);
        return;
    }

    SurfaceState.MeshComponent = BestMesh;
    SurfaceState.MeshComponentName = BestMesh->GetName();
    SurfaceState.ActorPath = BestOwner->GetPathName();
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

    UE_LOG(LogRshipExec, Log, TEXT("ResolveMappingSurface[%s]: mesh='%s' actor='%s' slots=%d score=%d"),
        *SurfaceState.Id,
        *SurfaceState.MeshComponentName,
        *BestOwner->GetName(),
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
                if (Subsystem)
                {
                    Subsystem->SetItem(TEXT("Mapping"), BuildMappingJson(MappingState), ERshipMessagePriority::High, MappingState.Id);
                    EmitMappingState(MappingState);
                }
            }
        }
    }

    if (bAnyChanged)
    {
        FeedSingleRtBindingCache.Empty();
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
    if ((CurrentContextId.IsEmpty() || !IsKnownRenderContextId(CurrentContextId))
        && !PreferredContextId.IsEmpty())
    {
        MappingState.ContextId = PreferredContextId;
        bChanged = true;
    }

    bool bHasKnownSurface = false;
    for (const FString& RawSurfaceId : MappingState.SurfaceIds)
    {
        const FString SurfaceId = RawSurfaceId.TrimStartAndEnd();
        if (!SurfaceId.IsEmpty() && MappingSurfaces.Contains(SurfaceId))
        {
            bHasKnownSurface = true;
            break;
        }
    }

    if (!bHasKnownSurface)
    {
        const int32 BeforeCount = MappingState.SurfaceIds.Num();

        for (const TPair<FString, FRshipMappingSurfaceState>& Pair : MappingSurfaces)
        {
            if (!Pair.Key.IsEmpty() && Pair.Value.bEnabled)
            {
                MappingState.SurfaceIds.AddUnique(Pair.Key);
            }
        }
        if (MappingState.SurfaceIds.Num() == BeforeCount)
        {
            for (const TPair<FString, FRshipMappingSurfaceState>& Pair : MappingSurfaces)
            {
                if (!Pair.Key.IsEmpty())
                {
                    MappingState.SurfaceIds.AddUnique(Pair.Key);
                }
            }
        }

        if (MappingState.SurfaceIds.Num() != BeforeCount)
        {
            bChanged = true;
        }
    }

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

    if (SanitizedSources.Num() == 0)
    {
        int32 Width = 1920;
        int32 Height = 1080;
        ResolveContextDimensions(DefaultContextId, Width, Height);

        TSharedPtr<FJsonObject> SourceObj = MakeShared<FJsonObject>();
        const FString SourceId = FString::Printf(TEXT("source-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
        SourceObj->SetStringField(TEXT("id"), SourceId);
        if (!DefaultContextId.IsEmpty())
        {
            SourceObj->SetStringField(TEXT("contextId"), DefaultContextId);
        }
        SourceObj->SetNumberField(TEXT("width"), FMath::Max(1, Width));
        SourceObj->SetNumberField(TEXT("height"), FMath::Max(1, Height));
        SourceDimensions.Add(SourceId, FIntPoint(FMath::Max(1, Width), FMath::Max(1, Height)));
        SanitizedSources.Add(MakeShared<FJsonValueObject>(SourceObj));
        bChanged = true;
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

    for (const FString& SurfaceId : MappingSurfaceIds)
    {
        const bool bHasDestinationForSurface = SanitizedDestinations.ContainsByPredicate([&](const TSharedPtr<FJsonValue>& Value)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                return false;
            }
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            return Obj.IsValid() && GetStringField(Obj, TEXT("surfaceId")).TrimStartAndEnd() == SurfaceId;
        });
        if (bHasDestinationForSurface)
        {
            continue;
        }

        const FString DestinationId = FString::Printf(TEXT("dest-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
        TSharedPtr<FJsonObject> DestinationObj = MakeShared<FJsonObject>();
        DestinationObj->SetStringField(TEXT("id"), DestinationId);
        DestinationObj->SetStringField(TEXT("surfaceId"), SurfaceId);
        DestinationObj->SetNumberField(TEXT("width"), FallbackSourceWidth);
        DestinationObj->SetNumberField(TEXT("height"), FallbackSourceHeight);
        DestinationDimensions.Add(DestinationId, FIntPoint(FallbackSourceWidth, FallbackSourceHeight));
        SanitizedDestinations.Add(MakeShared<FJsonValueObject>(DestinationObj));
        bChanged = true;
    }

    if (SanitizedDestinations.Num() == 0)
    {
        const FString DestinationId = FString::Printf(TEXT("dest-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
        TSharedPtr<FJsonObject> DestinationObj = MakeShared<FJsonObject>();
        DestinationObj->SetStringField(TEXT("id"), DestinationId);
        if (MappingSurfaceIds.Num() > 0)
        {
            DestinationObj->SetStringField(TEXT("surfaceId"), MappingSurfaceIds[0]);
        }
        DestinationObj->SetNumberField(TEXT("width"), FallbackSourceWidth);
        DestinationObj->SetNumberField(TEXT("height"), FallbackSourceHeight);
        DestinationDimensions.Add(DestinationId, FIntPoint(FallbackSourceWidth, FallbackSourceHeight));
        SanitizedDestinations.Add(MakeShared<FJsonValueObject>(DestinationObj));
        bChanged = true;
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

    if (SanitizedRoutes.Num() == 0 && SourceIds.Num() > 0 && DestinationIds.Num() > 0)
    {
        for (const FString& DestinationId : DestinationIds)
        {
            const FIntPoint SourceDim = SourceDimensions[SourceIds[0]];
            const FIntPoint DestinationDim = DestinationDimensions[DestinationId];
            TSharedPtr<FJsonObject> RouteObj = MakeShared<FJsonObject>();
            RouteObj->SetStringField(TEXT("id"), FString::Printf(TEXT("route-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
            RouteObj->SetStringField(TEXT("sourceId"), SourceIds[0]);
            RouteObj->SetStringField(TEXT("destinationId"), DestinationId);
            RouteObj->SetBoolField(TEXT("enabled"), true);
            RouteObj->SetNumberField(TEXT("opacity"), 1.0f);
            RouteObj->SetObjectField(TEXT("sourceRect"), MakeRectObject(0, 0, SourceDim.X, SourceDim.Y));
            RouteObj->SetObjectField(TEXT("destinationRect"), MakeRectObject(0, 0, DestinationDim.X, DestinationDim.Y));
            SanitizedRoutes.Add(MakeShared<FJsonValueObject>(RouteObj));
        }
        bChanged = true;
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

bool URshipContentMappingManager::TryResolveFeedSingleRtBinding(
    const FRshipContentMappingState& MappingState,
    const FRshipMappingSurfaceState& SurfaceState,
    FFeedSingleRtBinding& OutBinding)
{
    OutBinding = FFeedSingleRtBinding();

    if (!IsFeedV2Mapping(MappingState)
        || !MappingState.Config.IsValid()
        || !MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
    {
        return false;
    }

    const TSharedPtr<FJsonObject> FeedV2 = MappingState.Config->GetObjectField(TEXT("feedV2"));
    if (!FeedV2.IsValid())
    {
        return false;
    }

    const FString CacheKey = MakeFeedCompositeKey(MappingState.Id, SurfaceState.Id);
    FFeedSingleRtPreparedRoute* PreparedRoute = FeedSingleRtBindingCache.Find(CacheKey);
    if (!PreparedRoute)
    {
        FFeedSingleRtPreparedRoute NewPreparedRoute;
        NewPreparedRoute.bPrepared = true;

        const FString CoordinateSpace = GetStringField(FeedV2, TEXT("coordinateSpace"), TEXT("pixel")).TrimStartAndEnd().ToLower();
        if (!CoordinateSpace.IsEmpty() && CoordinateSpace != TEXT("pixel"))
        {
            NewPreparedRoute.Error = FString::Printf(
                TEXT("feedV2 coordinateSpace '%s' is not supported (expected 'pixel')"),
                *CoordinateSpace);
            PreparedRoute = &FeedSingleRtBindingCache.Add(CacheKey, MoveTemp(NewPreparedRoute));
        }
        else
        {
            TMap<FString, FString> SourceContextById;
            TMap<FString, FIntPoint> SourceDimensionsById;
            FString FirstSourceId;

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

                    const FString SourceId = GetStringField(SourceObj, TEXT("id")).TrimStartAndEnd();
                    if (SourceId.IsEmpty())
                    {
                        continue;
                    }
                    if (FirstSourceId.IsEmpty())
                    {
                        FirstSourceId = SourceId;
                    }

                    SourceContextById.Add(SourceId, GetStringField(SourceObj, TEXT("contextId")).TrimStartAndEnd());
                    const int32 Width = FMath::Max(0, GetIntField(SourceObj, TEXT("width"), 0));
                    const int32 Height = FMath::Max(0, GetIntField(SourceObj, TEXT("height"), 0));
                    SourceDimensionsById.Add(SourceId, FIntPoint(Width, Height));
                }
            }

            struct FDestinationSpec
            {
                FString Id;
                FString SurfaceId;
                int32 Width = 0;
                int32 Height = 0;
            };

            TArray<FDestinationSpec> DestinationSpecs;
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

                    FDestinationSpec Destination;
                    Destination.Id = GetStringField(DestinationObj, TEXT("id")).TrimStartAndEnd();
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
                        DestinationSpecs.Add(Destination);
                    }
                }
            }

            if (DestinationSpecs.Num() == 0)
            {
                FDestinationSpec Synthetic;
                Synthetic.Id = SurfaceState.Id;
                Synthetic.SurfaceId = SurfaceState.Id;
                DestinationSpecs.Add(Synthetic);
            }

            TArray<FDestinationSpec> MatchingDestinations;
            for (const FDestinationSpec& Destination : DestinationSpecs)
            {
                if (Destination.SurfaceId == SurfaceState.Id || Destination.Id == SurfaceState.Id)
                {
                    MatchingDestinations.Add(Destination);
                }
            }
            if (MatchingDestinations.Num() == 0 && DestinationSpecs.Num() == 1)
            {
                MatchingDestinations.Add(DestinationSpecs[0]);
            }
            if (MatchingDestinations.Num() == 0)
            {
                FDestinationSpec Synthetic;
                Synthetic.Id = SurfaceState.Id;
                Synthetic.SurfaceId = SurfaceState.Id;
                MatchingDestinations.Add(Synthetic);
            }

            TSharedPtr<FJsonObject> SelectedRoute;
            FString SelectedSourceId;
            FDestinationSpec SelectedDestination = MatchingDestinations[0];

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
                    if (!RouteObj.IsValid() || !GetBoolField(RouteObj, TEXT("enabled"), true))
                    {
                        continue;
                    }

                    FString RouteDestinationId = GetStringField(RouteObj, TEXT("destinationId")).TrimStartAndEnd();
                    if (RouteDestinationId.IsEmpty())
                    {
                        RouteDestinationId = GetStringField(RouteObj, TEXT("surfaceId")).TrimStartAndEnd();
                    }
                    if (RouteDestinationId.IsEmpty() && MatchingDestinations.Num() == 1)
                    {
                        RouteDestinationId = MatchingDestinations[0].Id;
                    }

                    const FDestinationSpec* MatchedDestination = nullptr;
                    for (const FDestinationSpec& CandidateDestination : MatchingDestinations)
                    {
                        if (RouteDestinationId == CandidateDestination.Id || RouteDestinationId == CandidateDestination.SurfaceId)
                        {
                            MatchedDestination = &CandidateDestination;
                            break;
                        }
                    }
                    if (!MatchedDestination)
                    {
                        continue;
                    }

                    SelectedRoute = RouteObj;
                    SelectedSourceId = GetStringField(RouteObj, TEXT("sourceId")).TrimStartAndEnd();
                    SelectedDestination = *MatchedDestination;
                    break;
                }
            }

            if (SelectedSourceId.IsEmpty())
            {
                SelectedSourceId = FirstSourceId;
            }

            auto AddCandidate = [&NewPreparedRoute](const FString& Candidate)
            {
                const FString Trimmed = Candidate.TrimStartAndEnd();
                if (!Trimmed.IsEmpty())
                {
                    NewPreparedRoute.ContextCandidates.AddUnique(Trimmed);
                }
            };

            if (!SelectedSourceId.IsEmpty())
            {
                AddCandidate(SelectedSourceId);
                if (const FString* ContextId = SourceContextById.Find(SelectedSourceId))
                {
                    AddCandidate(*ContextId);
                }
                if (const FIntPoint* SourceDim = SourceDimensionsById.Find(SelectedSourceId))
                {
                    NewPreparedRoute.SourceWidth = FMath::Max(0, SourceDim->X);
                    NewPreparedRoute.SourceHeight = FMath::Max(0, SourceDim->Y);
                }
            }
            AddCandidate(MappingState.ContextId);

            NewPreparedRoute.DestinationWidth = FMath::Max(0, SelectedDestination.Width);
            NewPreparedRoute.DestinationHeight = FMath::Max(0, SelectedDestination.Height);

            const int32 DefaultSourceWidth = FMath::Max(1, NewPreparedRoute.SourceWidth > 0 ? NewPreparedRoute.SourceWidth : 1920);
            const int32 DefaultSourceHeight = FMath::Max(1, NewPreparedRoute.SourceHeight > 0 ? NewPreparedRoute.SourceHeight : 1080);
            const int32 DefaultDestinationWidth = FMath::Max(1, NewPreparedRoute.DestinationWidth > 0 ? NewPreparedRoute.DestinationWidth : DefaultSourceWidth);
            const int32 DefaultDestinationHeight = FMath::Max(1, NewPreparedRoute.DestinationHeight > 0 ? NewPreparedRoute.DestinationHeight : DefaultSourceHeight);

            NewPreparedRoute.SourceX = 0;
            NewPreparedRoute.SourceY = 0;
            NewPreparedRoute.SourceW = DefaultSourceWidth;
            NewPreparedRoute.SourceH = DefaultSourceHeight;
            NewPreparedRoute.DestinationX = 0;
            NewPreparedRoute.DestinationY = 0;
            NewPreparedRoute.DestinationW = DefaultDestinationWidth;
            NewPreparedRoute.DestinationH = DefaultDestinationHeight;
            NewPreparedRoute.bHasRoute = SelectedRoute.IsValid();

            auto ParseRectPx = [this](const TSharedPtr<FJsonObject>& RectObj, int32& OutX, int32& OutY, int32& OutW, int32& OutH) -> bool
            {
                if (!RectObj.IsValid())
                {
                    return false;
                }
                OutX = GetIntField(RectObj, TEXT("x"), GetIntField(RectObj, TEXT("u"), OutX));
                OutY = GetIntField(RectObj, TEXT("y"), GetIntField(RectObj, TEXT("v"), OutY));
                OutW = GetIntField(RectObj, TEXT("w"), GetIntField(RectObj, TEXT("width"), OutW));
                OutH = GetIntField(RectObj, TEXT("h"), GetIntField(RectObj, TEXT("height"), OutH));
                return true;
            };

            if (SelectedRoute.IsValid())
            {
                TSharedPtr<FJsonObject> SourceRectObj;
                if (SelectedRoute->HasTypedField<EJson::Object>(TEXT("sourceRect")))
                {
                    SourceRectObj = SelectedRoute->GetObjectField(TEXT("sourceRect"));
                }
                else if (SelectedRoute->HasTypedField<EJson::Object>(TEXT("srcRect")))
                {
                    SourceRectObj = SelectedRoute->GetObjectField(TEXT("srcRect"));
                }
                if (!ParseRectPx(SourceRectObj, NewPreparedRoute.SourceX, NewPreparedRoute.SourceY, NewPreparedRoute.SourceW, NewPreparedRoute.SourceH))
                {
                    NewPreparedRoute.SourceX = GetIntField(SelectedRoute, TEXT("sourceX"), GetIntField(SelectedRoute, TEXT("srcX"), NewPreparedRoute.SourceX));
                    NewPreparedRoute.SourceY = GetIntField(SelectedRoute, TEXT("sourceY"), GetIntField(SelectedRoute, TEXT("srcY"), NewPreparedRoute.SourceY));
                    NewPreparedRoute.SourceW = GetIntField(SelectedRoute, TEXT("sourceW"), GetIntField(SelectedRoute, TEXT("srcW"), NewPreparedRoute.SourceW));
                    NewPreparedRoute.SourceH = GetIntField(SelectedRoute, TEXT("sourceH"), GetIntField(SelectedRoute, TEXT("srcH"), NewPreparedRoute.SourceH));
                }

                TSharedPtr<FJsonObject> DestinationRectObj;
                if (SelectedRoute->HasTypedField<EJson::Object>(TEXT("destinationRect")))
                {
                    DestinationRectObj = SelectedRoute->GetObjectField(TEXT("destinationRect"));
                }
                else if (SelectedRoute->HasTypedField<EJson::Object>(TEXT("dstRect")))
                {
                    DestinationRectObj = SelectedRoute->GetObjectField(TEXT("dstRect"));
                }
                if (!ParseRectPx(DestinationRectObj, NewPreparedRoute.DestinationX, NewPreparedRoute.DestinationY, NewPreparedRoute.DestinationW, NewPreparedRoute.DestinationH))
                {
                    NewPreparedRoute.DestinationX = GetIntField(SelectedRoute, TEXT("destinationX"), GetIntField(SelectedRoute, TEXT("dstX"), NewPreparedRoute.DestinationX));
                    NewPreparedRoute.DestinationY = GetIntField(SelectedRoute, TEXT("destinationY"), GetIntField(SelectedRoute, TEXT("dstY"), NewPreparedRoute.DestinationY));
                    NewPreparedRoute.DestinationW = GetIntField(SelectedRoute, TEXT("destinationW"), GetIntField(SelectedRoute, TEXT("dstW"), NewPreparedRoute.DestinationW));
                    NewPreparedRoute.DestinationH = GetIntField(SelectedRoute, TEXT("destinationH"), GetIntField(SelectedRoute, TEXT("dstH"), NewPreparedRoute.DestinationH));
                }
            }

            PreparedRoute = &FeedSingleRtBindingCache.Add(CacheKey, MoveTemp(NewPreparedRoute));
        }
    }

    if (!PreparedRoute)
    {
        return false;
    }

    if (!PreparedRoute->Error.IsEmpty())
    {
        OutBinding.Error = PreparedRoute->Error;
        return false;
    }

    auto ResolveContextByCandidates = [this, PreparedRoute]() -> const FRshipRenderContextState*
    {
        auto FindContextById = [this](const FString& ContextId) -> const FRshipRenderContextState*
        {
            return ContextId.IsEmpty() ? nullptr : RenderContexts.Find(ContextId);
        };

        for (const FString& CandidateId : PreparedRoute->ContextCandidates)
        {
            if (const FRshipRenderContextState* CandidateContext = FindContextById(CandidateId))
            {
                if (CandidateContext->ResolvedTexture)
                {
                    return CandidateContext;
                }
            }
        }
        for (const FString& CandidateId : PreparedRoute->ContextCandidates)
        {
            if (const FRshipRenderContextState* CandidateContext = FindContextById(CandidateId))
            {
                return CandidateContext;
            }
        }
        if (const FRshipRenderContextState* CandidateContext = FindContextById(CachedEnabledTextureContextId))
        {
            return CandidateContext;
        }
        if (const FRshipRenderContextState* CandidateContext = FindContextById(CachedAnyTextureContextId))
        {
            return CandidateContext;
        }
        if (const FRshipRenderContextState* CandidateContext = FindContextById(CachedEnabledContextId))
        {
            return CandidateContext;
        }
        if (const FRshipRenderContextState* CandidateContext = FindContextById(CachedAnyContextId))
        {
            return CandidateContext;
        }
        return nullptr;
    };

    const FRshipRenderContextState* SourceContext = ResolveContextByCandidates();
    if (!SourceContext || !SourceContext->ResolvedTexture)
    {
        OutBinding.Error = TEXT("No source texture available for feed route");
        return false;
    }

    const int32 TextureWidth = FMath::Max(1, SourceContext->ResolvedTexture->GetSurfaceWidth());
    const int32 TextureHeight = FMath::Max(1, SourceContext->ResolvedTexture->GetSurfaceHeight());

    const int32 SourceWidth = FMath::Max(
        1,
        PreparedRoute->SourceWidth > 0
            ? PreparedRoute->SourceWidth
            : (SourceContext->Width > 0 ? SourceContext->Width : TextureWidth));
    const int32 SourceHeight = FMath::Max(
        1,
        PreparedRoute->SourceHeight > 0
            ? PreparedRoute->SourceHeight
            : (SourceContext->Height > 0 ? SourceContext->Height : TextureHeight));

    const int32 DestinationWidth = FMath::Max(
        1,
        PreparedRoute->DestinationWidth > 0 ? PreparedRoute->DestinationWidth : SourceWidth);
    const int32 DestinationHeight = FMath::Max(
        1,
        PreparedRoute->DestinationHeight > 0 ? PreparedRoute->DestinationHeight : SourceHeight);

    int32 SourceX = PreparedRoute->bHasRoute ? PreparedRoute->SourceX : 0;
    int32 SourceY = PreparedRoute->bHasRoute ? PreparedRoute->SourceY : 0;
    int32 SourceW = PreparedRoute->bHasRoute ? PreparedRoute->SourceW : SourceWidth;
    int32 SourceH = PreparedRoute->bHasRoute ? PreparedRoute->SourceH : SourceHeight;
    int32 DestinationX = PreparedRoute->bHasRoute ? PreparedRoute->DestinationX : 0;
    int32 DestinationY = PreparedRoute->bHasRoute ? PreparedRoute->DestinationY : 0;
    int32 DestinationW = PreparedRoute->bHasRoute ? PreparedRoute->DestinationW : DestinationWidth;
    int32 DestinationH = PreparedRoute->bHasRoute ? PreparedRoute->DestinationH : DestinationHeight;

    SourceX = FMath::Clamp(SourceX, 0, SourceWidth - 1);
    SourceY = FMath::Clamp(SourceY, 0, SourceHeight - 1);
    SourceW = FMath::Clamp(SourceW, 1, SourceWidth - SourceX);
    SourceH = FMath::Clamp(SourceH, 1, SourceHeight - SourceY);

    DestinationX = FMath::Clamp(DestinationX, 0, DestinationWidth - 1);
    DestinationY = FMath::Clamp(DestinationY, 0, DestinationHeight - 1);
    DestinationW = FMath::Clamp(DestinationW, 1, DestinationWidth - DestinationX);
    DestinationH = FMath::Clamp(DestinationH, 1, DestinationHeight - DestinationY);

    OutBinding.bValid = true;
    OutBinding.Texture = SourceContext->ResolvedTexture;
    OutBinding.DepthTexture = SourceContext->ResolvedDepthTexture;
    OutBinding.bHasSourceRect = true;
    OutBinding.SourceU = static_cast<float>(SourceX) / static_cast<float>(SourceWidth);
    OutBinding.SourceV = static_cast<float>(SourceY) / static_cast<float>(SourceHeight);
    OutBinding.SourceW = static_cast<float>(SourceW) / static_cast<float>(SourceWidth);
    OutBinding.SourceH = static_cast<float>(SourceH) / static_cast<float>(SourceHeight);
    OutBinding.bHasDestinationRect = true;
    OutBinding.DestinationU = static_cast<float>(DestinationX) / static_cast<float>(DestinationWidth);
    OutBinding.DestinationV = static_cast<float>(DestinationY) / static_cast<float>(DestinationHeight);
    OutBinding.DestinationW = static_cast<float>(DestinationW) / static_cast<float>(DestinationWidth);
    OutBinding.DestinationH = static_cast<float>(DestinationH) / static_cast<float>(DestinationHeight);
    return true;
}

UTexture* URshipContentMappingManager::BuildFeedCompositeTextureForSurface(
    const FRshipContentMappingState& MappingState,
    const FRshipMappingSurfaceState& SurfaceState,
    FString& OutError)
{
    OutError.Reset();

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

    const bool bUseRoutesField = FeedV2->HasTypedField<EJson::Array>(TEXT("routes"));
    const bool bUseLinksField = FeedV2->HasTypedField<EJson::Array>(TEXT("links"));
    if (bUseRoutesField || bUseLinksField)
    {
        const TArray<TSharedPtr<FJsonValue>> RouteArray = bUseRoutesField
            ? FeedV2->GetArrayField(TEXT("routes"))
            : FeedV2->GetArrayField(TEXT("links"));
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

            if (Route.SourceId.IsEmpty())
            {
                Route.SourceId = GetStringField(RouteObj, TEXT("source")).TrimStartAndEnd();
            }
            if (Route.DestinationId.IsEmpty())
            {
                Route.DestinationId = GetStringField(RouteObj, TEXT("destination")).TrimStartAndEnd();
            }
            if (Route.DestinationId.IsEmpty())
            {
                Route.DestinationId = GetStringField(RouteObj, TEXT("surfaceId")).TrimStartAndEnd();
            }

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
            else if (RouteObj->HasTypedField<EJson::Object>(TEXT("srcRect")))
            {
                SourceRectObj = RouteObj->GetObjectField(TEXT("srcRect"));
            }
            ParseRectPx(SourceRectObj, DefaultSourceRect, Route.SourceRect);
            if (!SourceRectObj.IsValid())
            {
                Route.SourceRect.X = GetIntField(RouteObj, TEXT("sourceX"), GetIntField(RouteObj, TEXT("srcX"), 0));
                Route.SourceRect.Y = GetIntField(RouteObj, TEXT("sourceY"), GetIntField(RouteObj, TEXT("srcY"), 0));
                Route.SourceRect.W = GetIntField(RouteObj, TEXT("sourceW"), GetIntField(RouteObj, TEXT("srcW"), 1));
                Route.SourceRect.H = GetIntField(RouteObj, TEXT("sourceH"), GetIntField(RouteObj, TEXT("srcH"), 1));
            }

            TSharedPtr<FJsonObject> DestinationRectObj;
            if (RouteObj->HasTypedField<EJson::Object>(TEXT("destinationRect")))
            {
                DestinationRectObj = RouteObj->GetObjectField(TEXT("destinationRect"));
            }
            else if (RouteObj->HasTypedField<EJson::Object>(TEXT("dstRect")))
            {
                DestinationRectObj = RouteObj->GetObjectField(TEXT("dstRect"));
            }
            ParseRectPx(DestinationRectObj, DefaultDestinationRect, Route.DestinationRect);
            if (!DestinationRectObj.IsValid())
            {
                Route.DestinationRect.X = GetIntField(RouteObj, TEXT("destinationX"), GetIntField(RouteObj, TEXT("dstX"), 0));
                Route.DestinationRect.Y = GetIntField(RouteObj, TEXT("destinationY"), GetIntField(RouteObj, TEXT("dstY"), 0));
                Route.DestinationRect.W = GetIntField(RouteObj, TEXT("destinationW"), GetIntField(RouteObj, TEXT("dstW"), 1));
                Route.DestinationRect.H = GetIntField(RouteObj, TEXT("destinationH"), GetIntField(RouteObj, TEXT("dstH"), 1));
            }

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

    bool bHasRouteForDestination = false;
    const bool bSingleDestination = Spec.Destinations.Num() <= 1;
    for (const FFeedRouteSpec& Route : Spec.Routes)
    {
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

    if (!bHasRouteForDestination && Spec.Sources.Num() > 0)
    {
        const FFeedSourceSpec* FallbackSource = nullptr;
        if (!MappingState.ContextId.IsEmpty())
        {
            for (const TPair<FString, FFeedSourceSpec>& Pair : Spec.Sources)
            {
                if (Pair.Value.ContextId.Equals(MappingState.ContextId, ESearchCase::IgnoreCase))
                {
                    FallbackSource = &Pair.Value;
                    break;
                }
            }
        }
        if (!FallbackSource)
        {
            for (const TPair<FString, FFeedSourceSpec>& Pair : Spec.Sources)
            {
                if (!Pair.Value.ContextId.IsEmpty())
                {
                    FallbackSource = &Pair.Value;
                    break;
                }
            }
        }
        if (!FallbackSource)
        {
            for (const TPair<FString, FFeedSourceSpec>& Pair : Spec.Sources)
            {
                FallbackSource = &Pair.Value;
                break;
            }
        }

        if (FallbackSource)
        {
            int32 FallbackSourceWidth = FMath::Max(1, FallbackSource->Width);
            int32 FallbackSourceHeight = FMath::Max(1, FallbackSource->Height);
            if (!FallbackSource->ContextId.IsEmpty())
            {
                if (const FRshipRenderContextState* SourceContext = RenderContexts.Find(FallbackSource->ContextId))
                {
                    if (SourceContext->Width > 0)
                    {
                        FallbackSourceWidth = SourceContext->Width;
                    }
                    if (SourceContext->Height > 0)
                    {
                        FallbackSourceHeight = SourceContext->Height;
                    }
                    if (SourceContext->ResolvedTexture)
                    {
                        FallbackSourceWidth = FMath::Max(1, SourceContext->ResolvedTexture->GetSurfaceWidth());
                        FallbackSourceHeight = FMath::Max(1, SourceContext->ResolvedTexture->GetSurfaceHeight());
                    }
                }
            }

            FFeedRouteSpec Route;
            Route.Id = FString::Printf(TEXT("auto-route-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
            Route.SourceId = FallbackSource->Id;
            Route.DestinationId = DestinationSpec.Id.IsEmpty() ? DestinationSpec.SurfaceId : DestinationSpec.Id;
            Route.bEnabled = true;
            Route.Opacity = 1.0f;
            Route.SourceRect.X = 0;
            Route.SourceRect.Y = 0;
            Route.SourceRect.W = FMath::Max(1, FallbackSourceWidth);
            Route.SourceRect.H = FMath::Max(1, FallbackSourceHeight);
            Route.DestinationRect.X = 0;
            Route.DestinationRect.Y = 0;
            Route.DestinationRect.W = DestinationWidth;
            Route.DestinationRect.H = DestinationHeight;
            Spec.Routes.Add(Route);
        }
    }

    const FString CompositeKey = MakeFeedCompositeKey(MappingState.Id, SurfaceState.Id);
    UTextureRenderTarget2D* CompositeRT = FeedCompositeTargets.FindRef(CompositeKey);
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

    UWorld* World = GetBestWorld();
    if (!World)
    {
        OutError = TEXT("World not available for feed composition");
        return CompositeRT;
    }

    auto ResolveContextForRoute = [this, &MappingState](const FFeedSourceSpec* SourceSpec, const FString& RouteSourceId) -> const FRshipRenderContextState*
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
        return !SourceContext->SourceType.Equals(TEXT("asset"), ESearchCase::IgnoreCase);
    };

    uint32 CompositeSignature = HashCombineFast(GetTypeHash(DestinationWidth), GetTypeHash(DestinationHeight));
    CompositeSignature = HashCombineFast(CompositeSignature, GetTypeHash(MappingState.Id));
    CompositeSignature = HashCombineFast(CompositeSignature, GetTypeHash(SurfaceState.Id));
    CompositeSignature = HashCombineFast(CompositeSignature, GetTypeHash(static_cast<uint32>(RuntimeStateRevision)));
    CompositeSignature = HashCombineFast(CompositeSignature, GetTypeHash(static_cast<uint32>(RuntimeStateRevision >> 32)));
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
        const FRshipRenderContextState* FallbackContextForSignature = nullptr;
        if (!MappingState.ContextId.IsEmpty())
        {
            FallbackContextForSignature = RenderContexts.Find(MappingState.ContextId);
        }
        if ((!FallbackContextForSignature || !FallbackContextForSignature->ResolvedTexture))
        {
            for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
            {
                if (Pair.Value.bEnabled && Pair.Value.ResolvedTexture)
                {
                    FallbackContextForSignature = &Pair.Value;
                    break;
                }
            }
        }
        if (FallbackContextForSignature && FallbackContextForSignature->ResolvedTexture)
        {
            CompositeSignature = HashCombineFast(CompositeSignature, PointerHash(FallbackContextForSignature->ResolvedTexture));
            if (IsDynamicRouteSource(FallbackContextForSignature, FallbackContextForSignature->ResolvedTexture))
            {
                bHasDynamicRouteSource = true;
            }
        }
        else
        {
            CompositeSignature = HashCombineFast(CompositeSignature, 0x8AC69E17u);
        }
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
        const FRshipRenderContextState* BypassContext = nullptr;
        if (!MappingState.ContextId.IsEmpty())
        {
            BypassContext = RenderContexts.Find(MappingState.ContextId);
        }
        if (!BypassContext || !BypassContext->ResolvedTexture)
        {
            for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
            {
                if (Pair.Value.bEnabled && Pair.Value.ResolvedTexture)
                {
                    BypassContext = &Pair.Value;
                    break;
                }
            }
        }
        UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, DrawContext);
        if (BypassContext && BypassContext->ResolvedTexture)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("Feed composite canvas unavailable map=%s surf=%s; bypassing to source texture"),
                *MappingState.Id, *SurfaceState.Id);
            FeedCompositeStaticSignatures.Remove(CompositeKey);
            return BypassContext->ResolvedTexture;
        }
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

    if (DrawnRouteCount == 0 && Canvas)
    {
        const FRshipRenderContextState* FallbackContext = nullptr;
        if (!MappingState.ContextId.IsEmpty())
        {
            FallbackContext = RenderContexts.Find(MappingState.ContextId);
        }
        if ((!FallbackContext || !FallbackContext->ResolvedTexture))
        {
            for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
            {
                if (Pair.Value.bEnabled && Pair.Value.ResolvedTexture)
                {
                    FallbackContext = &Pair.Value;
                    break;
                }
            }
        }
        if ((!FallbackContext || !FallbackContext->ResolvedTexture))
        {
            for (const TPair<FString, FRshipRenderContextState>& Pair : RenderContexts)
            {
                if (Pair.Value.ResolvedTexture)
                {
                    FallbackContext = &Pair.Value;
                    break;
                }
            }
        }

        if (FallbackContext && FallbackContext->ResolvedTexture)
        {
            Canvas->K2_DrawTexture(
                FallbackContext->ResolvedTexture,
                FVector2D(0.0f, 0.0f),
                FVector2D(static_cast<float>(DestinationWidth), static_cast<float>(DestinationHeight)),
                FVector2D::ZeroVector,
                FVector2D(1.0f, 1.0f),
                FLinearColor::White,
                BLEND_Opaque,
                0.0f,
                FVector2D::ZeroVector);
            ++DrawnRouteCount;
            RouteIssues.Add(TEXT("No valid feed routes; using fallback full-frame source"));
        }
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, DrawContext);

    if (RouteIssues.Num() > 0)
    {
        if (DrawnRouteCount > 0)
        {
            UE_LOG(LogRshipExec, Verbose, TEXT("Feed composite recovered map=%s surf=%s: %s"),
                *MappingState.Id, *SurfaceState.Id, *FString::Join(RouteIssues, TEXT("; ")));
        }
        else
        {
            OutError = FString::Join(RouteIssues, TEXT("; "));
            UE_LOG(LogRshipExec, Warning, TEXT("Feed composite issues map=%s surf=%s: %s"),
                *MappingState.Id, *SurfaceState.Id, *OutError);
        }
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
    for (auto& Pair : MappingSurfaces)
    {
        RestoreSurfaceMaterials(Pair.Value);
        ResolveMappingSurface(Pair.Value);
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

            if (ARshipCameraActor* CameraActor = ContextState.CameraActor.Get())
            {
                CameraActor->bEnableSceneCapture = false;
                if (CameraActor->SceneCapture)
                {
                    CameraActor->SceneCapture->bCaptureEveryFrame = false;
                    CameraActor->SceneCapture->bCaptureOnMovement = false;
                }
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

        const bool bFeedV2 = IsFeedV2Mapping(MappingState);

        if (!MappingState.bEnabled)
        {
            continue;
        }

        const FRshipRenderContextState* ContextState = ResolveEffectiveContextState(MappingState, bFeedV2);
        if (bFeedV2 && (!ContextState || !ContextState->ResolvedTexture))
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
                ApplyMappingToSurface(MappingState, *SurfaceState, ContextState);
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
            continue;
        }

        if (OriginalMaterial->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || OriginalMaterial->IsUnreachable())
        {
            continue;
        }

        Mesh->SetMaterial(Pair.Key, OriginalMaterial);
    }

    SurfaceState.MaterialInstances.Empty();
    SurfaceState.OriginalMaterials.Empty();
    SurfaceState.MaterialBindingHashes.Empty();
}

void URshipContentMappingManager::ApplyMappingToSurface(
    const FRshipContentMappingState& MappingState,
    FRshipMappingSurfaceState& SurfaceState,
    const FRshipRenderContextState* ContextState)
{
    UMeshComponent* Mesh = SurfaceState.MeshComponent.Get();
    if (!IsMeshReadyForMaterialMutation(Mesh))
    {
        SurfaceState.LastError = TEXT("Mesh component not resolved");
        return;
    }

    if (!ContentMappingMaterial)
    {
        BuildFallbackMaterial();
    }

    EnsureMaterialContract();
    if (!bMaterialContractValid)
    {
        BuildFallbackMaterial();
        EnsureMaterialContract();
    }

    if (!ContentMappingMaterial)
    {
        SurfaceState.LastError = TEXT("Content mapping material unavailable");
        return;
    }
    if (!bMaterialContractValid)
    {
        SurfaceState.LastError = MaterialContractError.IsEmpty()
            ? TEXT("Content mapping material contract invalid")
            : MaterialContractError;
        return;
    }

    UMaterialInterface* BaseMaterial = ContentMappingMaterial;

    const int32 SlotCount = Mesh->GetNumMaterials();
    if (SlotCount <= 0)
    {
        SurfaceState.LastError = TEXT("Mesh has no material slots");
        UE_LOG(LogRshipExec, Warning, TEXT("ApplyMappingToSurface[%s]: mesh '%s' has no material slots"),
            *SurfaceState.Id, *Mesh->GetName());
        return;
    }

    const bool bUseFeedV2 = IsFeedV2Mapping(MappingState);
    FString FeedCompositeError;
    UTexture* FeedCompositeTexture = nullptr;
    if (bUseFeedV2)
    {
        FeedCompositeTexture = BuildFeedCompositeTextureForSurface(MappingState, SurfaceState, FeedCompositeError);

        if (!FeedCompositeError.IsEmpty())
        {
            SurfaceState.LastError = FeedCompositeError;
        }
    }

    const bool bHasTexture = (FeedCompositeTexture != nullptr) || (ContextState && ContextState->ResolvedTexture);
    if (bUseFeedV2 && !bHasTexture)
    {
        SurfaceState.LastError = FeedCompositeError.IsEmpty()
            ? TEXT("No feed source texture available")
            : FeedCompositeError;
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
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(static_cast<uint32>(RuntimeStateRevision)));
    BaseBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(static_cast<uint32>(RuntimeStateRevision >> 32)));
    if (MappingState.Config.IsValid())
    {
        BaseBindingHash = HashCombineFast(BaseBindingHash, PointerHash(MappingState.Config.Get()));
    }
    if (FeedCompositeTexture)
    {
        BaseBindingHash = HashCombineFast(BaseBindingHash, PointerHash(FeedCompositeTexture));
    }
    if (ContextState && ContextState->ResolvedTexture)
    {
        BaseBindingHash = HashCombineFast(BaseBindingHash, PointerHash(ContextState->ResolvedTexture));
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
        }

        if (!MID)
        {
            MID = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh);
            SurfaceState.MaterialInstances.Add(SlotIndex, MID);
            Mesh->SetMaterial(SlotIndex, MID);
        }

        const uint32 SlotBindingHash = HashCombineFast(BaseBindingHash, GetTypeHash(SlotIndex));
        if (const uint32* ExistingHash = SurfaceState.MaterialBindingHashes.Find(SlotIndex))
        {
            if (*ExistingHash == SlotBindingHash)
            {
                continue;
            }
        }

        ApplyMaterialParameters(MID, MappingState, SurfaceState, ContextState, bUseFeedV2, nullptr);
        if (FeedCompositeTexture)
        {
            MID->SetTextureParameterValue(ParamContextTexture, FeedCompositeTexture);
        }
        SurfaceState.MaterialBindingHashes.Add(SlotIndex, SlotBindingHash);
    }
}

void URshipContentMappingManager::ApplyMaterialParameters(
    UMaterialInstanceDynamic* MID,
    const FRshipContentMappingState& MappingState,
    const FRshipMappingSurfaceState& SurfaceState,
    const FRshipRenderContextState* ContextState,
    bool bUseFeedV2,
    const FFeedSingleRtBinding* FeedBinding)
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
    if (FeedBinding && FeedBinding->bValid && FeedBinding->Texture)
    {
        ResolvedContextTexture = FeedBinding->Texture;
    }
    else if (ContextState && ContextState->ResolvedTexture)
    {
        ResolvedContextTexture = ContextState->ResolvedTexture;
    }

    if (ResolvedContextTexture)
    {
        MID->SetTextureParameterValue(ParamContextTexture, ResolvedContextTexture);
    }
    else
    {
        MID->SetTextureParameterValue(ParamContextTexture, GetDefaultPreviewTexture());
    }

    UTexture* ResolvedDepthTexture = nullptr;
    if (FeedBinding && FeedBinding->bValid && FeedBinding->DepthTexture)
    {
        ResolvedDepthTexture = FeedBinding->DepthTexture;
    }
    else if (ContextState && ContextState->ResolvedDepthTexture)
    {
        ResolvedDepthTexture = ContextState->ResolvedDepthTexture;
    }

    if (ResolvedDepthTexture)
    {
        MID->SetTextureParameterValue(ParamContextDepthTexture, ResolvedDepthTexture);
    }
    else
    {
        MID->SetTextureParameterValue(ParamContextDepthTexture, GetDefaultPreviewTexture());
    }

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
        bool bFoundFeedRect = false;
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

            auto ReadFeedRect = [this](const TSharedPtr<FJsonObject>& RectObj, float& OutU, float& OutV, float& OutW, float& OutH)
            {
                if (!RectObj.IsValid())
                {
                    return false;
                }
                OutU = GetNumberField(RectObj, TEXT("u"), OutU);
                OutV = GetNumberField(RectObj, TEXT("v"), OutV);
                OutW = GetNumberField(RectObj, TEXT("width"), OutW);
                OutH = GetNumberField(RectObj, TEXT("height"), OutH);
                return true;
            };

            if (!bUseFeedV2 && MappingState.Config->HasTypedField<EJson::Array>(TEXT("feedRects")))
            {
                const TArray<TSharedPtr<FJsonValue>> FeedRects = MappingState.Config->GetArrayField(TEXT("feedRects"));
                for (const TSharedPtr<FJsonValue>& Value : FeedRects)
                {
                    if (!Value.IsValid() || Value->Type != EJson::Object)
                    {
                        continue;
                    }
                    TSharedPtr<FJsonObject> RectObj = Value->AsObject();
                    if (!RectObj.IsValid() || !RectObj->HasTypedField<EJson::String>(TEXT("surfaceId")))
                    {
                        continue;
                    }
                    const FString SurfaceId = RectObj->GetStringField(TEXT("surfaceId"));
                    if (SurfaceId == SurfaceState.Id)
                    {
                        if (ReadFeedRect(RectObj, FeedU, FeedV, FeedW, FeedH))
                        {
                            bFeedMode = true;
                            bFoundFeedRect = true;
                        }
                        break;
                    }
                }
            }

            if (!bUseFeedV2 && !bFoundFeedRect && MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedRect")))
            {
                TSharedPtr<FJsonObject> RectObj = MappingState.Config->GetObjectField(TEXT("feedRect"));
                if (ReadFeedRect(RectObj, FeedU, FeedV, FeedW, FeedH))
                {
                    bFeedMode = true;
                    bFoundFeedRect = true;
                }
            }
        }

        if (FeedBinding && FeedBinding->bValid && FeedBinding->bHasSourceRect)
        {
            FeedU = FeedBinding->SourceU;
            FeedV = FeedBinding->SourceV;
            FeedW = FeedBinding->SourceW;
            FeedH = FeedBinding->SourceH;
            bFoundFeedRect = true;

            if (FeedBinding->bHasDestinationRect)
            {
                const float SafeDestW = FMath::Max(0.0001f, FeedBinding->DestinationW);
                const float SafeDestH = FMath::Max(0.0001f, FeedBinding->DestinationH);
                ScaleU = FeedW / SafeDestW;
                ScaleV = FeedH / SafeDestH;
                OffsetU = FeedU - (FeedBinding->DestinationU * ScaleU);
                OffsetV = FeedV - (FeedBinding->DestinationV * ScaleV);
                Rotation = 0.0f;
                PivotU = 0.5f;
                PivotV = 0.5f;
                bFeedMode = false;
            }
            else
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
    if (!Subsystem || !Subsystem->IsConnected())
    {
        return;
    }

    const FString TargetId = BuildContextTargetId(ContextState.Id);
    const FString ServiceId = Subsystem->GetServiceId();

    TArray<TSharedPtr<FJsonValue>> ActionIds;
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setEnabled")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setCameraId")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setAssetId")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setDepthAssetId")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setDepthCaptureEnabled")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setDepthCaptureMode")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setResolution")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setCaptureMode")));

    TArray<TSharedPtr<FJsonValue>> EmitterIds;
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":state")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":status")));

    TSharedPtr<FJsonObject> TargetJson = MakeShared<FJsonObject>();
    TargetJson->SetStringField(TEXT("id"), TargetId);
    TargetJson->SetStringField(TEXT("name"), ContextState.Name);
    TargetJson->SetStringField(TEXT("serviceId"), ServiceId);
    TargetJson->SetStringField(TEXT("category"), TEXT("content-mapping"));
    TargetJson->SetArrayField(TEXT("actionIds"), ActionIds);
    TargetJson->SetArrayField(TEXT("emitterIds"), EmitterIds);
    TargetJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    Subsystem->SetItem(TEXT("Target"), TargetJson, ERshipMessagePriority::High, TargetId);

    auto RegisterAction = [&](const FString& Name)
    {
        TSharedPtr<FJsonObject> ActionJson = MakeShared<FJsonObject>();
        ActionJson->SetStringField(TEXT("id"), TargetId + TEXT(":") + Name);
        ActionJson->SetStringField(TEXT("name"), Name);
        ActionJson->SetStringField(TEXT("targetId"), TargetId);
        ActionJson->SetStringField(TEXT("serviceId"), ServiceId);
        TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        ActionJson->SetObjectField(TEXT("schema"), Schema);
        ActionJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->SetItem(TEXT("Action"), ActionJson, ERshipMessagePriority::High, ActionJson->GetStringField(TEXT("id")));
    };

    RegisterAction(TEXT("setEnabled"));
    RegisterAction(TEXT("setCameraId"));
    RegisterAction(TEXT("setAssetId"));
    RegisterAction(TEXT("setDepthAssetId"));
    RegisterAction(TEXT("setDepthCaptureEnabled"));
    RegisterAction(TEXT("setDepthCaptureMode"));
    RegisterAction(TEXT("setResolution"));
    RegisterAction(TEXT("setCaptureMode"));

    auto RegisterEmitter = [&](const FString& Name)
    {
        TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
        EmitterJson->SetStringField(TEXT("id"), TargetId + TEXT(":") + Name);
        EmitterJson->SetStringField(TEXT("name"), Name);
        EmitterJson->SetStringField(TEXT("targetId"), TargetId);
        EmitterJson->SetStringField(TEXT("serviceId"), ServiceId);
        TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        EmitterJson->SetObjectField(TEXT("schema"), Schema);
        EmitterJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->SetItem(TEXT("Emitter"), EmitterJson, ERshipMessagePriority::High, EmitterJson->GetStringField(TEXT("id")));
    };

    RegisterEmitter(TEXT("state"));
    RegisterEmitter(TEXT("status"));
}

void URshipContentMappingManager::RegisterSurfaceTarget(const FRshipMappingSurfaceState& SurfaceState)
{
    if (!Subsystem || !Subsystem->IsConnected())
    {
        return;
    }

    const FString TargetId = BuildSurfaceTargetId(SurfaceState.Id);
    const FString ServiceId = Subsystem->GetServiceId();

    TArray<TSharedPtr<FJsonValue>> ActionIds;
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setEnabled")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setActorPath")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setUvChannel")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setMaterialSlots")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setMeshComponentName")));

    TArray<TSharedPtr<FJsonValue>> EmitterIds;
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":state")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":status")));

    TSharedPtr<FJsonObject> TargetJson = MakeShared<FJsonObject>();
    TargetJson->SetStringField(TEXT("id"), TargetId);
    TargetJson->SetStringField(TEXT("name"), SurfaceState.Name);
    TargetJson->SetStringField(TEXT("serviceId"), ServiceId);
    TargetJson->SetStringField(TEXT("category"), TEXT("content-mapping"));
    TargetJson->SetArrayField(TEXT("actionIds"), ActionIds);
    TargetJson->SetArrayField(TEXT("emitterIds"), EmitterIds);
    TargetJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    Subsystem->SetItem(TEXT("Target"), TargetJson, ERshipMessagePriority::High, TargetId);

    auto RegisterAction = [&](const FString& Name)
    {
        TSharedPtr<FJsonObject> ActionJson = MakeShared<FJsonObject>();
        ActionJson->SetStringField(TEXT("id"), TargetId + TEXT(":") + Name);
        ActionJson->SetStringField(TEXT("name"), Name);
        ActionJson->SetStringField(TEXT("targetId"), TargetId);
        ActionJson->SetStringField(TEXT("serviceId"), ServiceId);
        TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        ActionJson->SetObjectField(TEXT("schema"), Schema);
        ActionJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->SetItem(TEXT("Action"), ActionJson, ERshipMessagePriority::High, ActionJson->GetStringField(TEXT("id")));
    };

    RegisterAction(TEXT("setEnabled"));
    RegisterAction(TEXT("setActorPath"));
    RegisterAction(TEXT("setUvChannel"));
    RegisterAction(TEXT("setMaterialSlots"));
    RegisterAction(TEXT("setMeshComponentName"));

    auto RegisterEmitter = [&](const FString& Name)
    {
        TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
        EmitterJson->SetStringField(TEXT("id"), TargetId + TEXT(":") + Name);
        EmitterJson->SetStringField(TEXT("name"), Name);
        EmitterJson->SetStringField(TEXT("targetId"), TargetId);
        EmitterJson->SetStringField(TEXT("serviceId"), ServiceId);
        TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        EmitterJson->SetObjectField(TEXT("schema"), Schema);
        EmitterJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->SetItem(TEXT("Emitter"), EmitterJson, ERshipMessagePriority::High, EmitterJson->GetStringField(TEXT("id")));
    };

    RegisterEmitter(TEXT("state"));
    RegisterEmitter(TEXT("status"));
}

void URshipContentMappingManager::RegisterMappingTarget(const FRshipContentMappingState& MappingState)
{
    if (!Subsystem || !Subsystem->IsConnected())
    {
        return;
    }

    const FString TargetId = BuildMappingTargetId(MappingState.Id);
    const FString ServiceId = Subsystem->GetServiceId();

    TArray<TSharedPtr<FJsonValue>> ActionIds;
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setEnabled")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setOpacity")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setContextId")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setSurfaceIds")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setProjection")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setUVTransform")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setType")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setConfig")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setFeedV2")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":upsertFeedSource")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":removeFeedSource")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":upsertFeedDestination")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":removeFeedDestination")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":upsertFeedRoute")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":removeFeedRoute")));

    TArray<TSharedPtr<FJsonValue>> EmitterIds;
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":state")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":status")));

    TSharedPtr<FJsonObject> TargetJson = MakeShared<FJsonObject>();
    TargetJson->SetStringField(TEXT("id"), TargetId);
    TargetJson->SetStringField(TEXT("name"), MappingState.Name);
    TargetJson->SetStringField(TEXT("serviceId"), ServiceId);
    TargetJson->SetStringField(TEXT("category"), TEXT("content-mapping"));
    TargetJson->SetArrayField(TEXT("actionIds"), ActionIds);
    TargetJson->SetArrayField(TEXT("emitterIds"), EmitterIds);
    TargetJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    Subsystem->SetItem(TEXT("Target"), TargetJson, ERshipMessagePriority::High, TargetId);

    auto RegisterAction = [&](const FString& Name)
    {
        TSharedPtr<FJsonObject> ActionJson = MakeShared<FJsonObject>();
        ActionJson->SetStringField(TEXT("id"), TargetId + TEXT(":") + Name);
        ActionJson->SetStringField(TEXT("name"), Name);
        ActionJson->SetStringField(TEXT("targetId"), TargetId);
        ActionJson->SetStringField(TEXT("serviceId"), ServiceId);
        TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        ActionJson->SetObjectField(TEXT("schema"), Schema);
        ActionJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->SetItem(TEXT("Action"), ActionJson, ERshipMessagePriority::High, ActionJson->GetStringField(TEXT("id")));
    };

    RegisterAction(TEXT("setEnabled"));
    RegisterAction(TEXT("setOpacity"));
    RegisterAction(TEXT("setContextId"));
    RegisterAction(TEXT("setSurfaceIds"));
    RegisterAction(TEXT("setProjection"));
    RegisterAction(TEXT("setUVTransform"));
    RegisterAction(TEXT("setType"));
    RegisterAction(TEXT("setConfig"));
    RegisterAction(TEXT("setFeedV2"));
    RegisterAction(TEXT("upsertFeedSource"));
    RegisterAction(TEXT("removeFeedSource"));
    RegisterAction(TEXT("upsertFeedDestination"));
    RegisterAction(TEXT("removeFeedDestination"));
    RegisterAction(TEXT("upsertFeedRoute"));
    RegisterAction(TEXT("removeFeedRoute"));

    auto RegisterEmitter = [&](const FString& Name)
    {
        TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
        EmitterJson->SetStringField(TEXT("id"), TargetId + TEXT(":") + Name);
        EmitterJson->SetStringField(TEXT("name"), Name);
        EmitterJson->SetStringField(TEXT("targetId"), TargetId);
        EmitterJson->SetStringField(TEXT("serviceId"), ServiceId);
        TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        EmitterJson->SetObjectField(TEXT("schema"), Schema);
        EmitterJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->SetItem(TEXT("Emitter"), EmitterJson, ERshipMessagePriority::High, EmitterJson->GetStringField(TEXT("id")));
    };

    RegisterEmitter(TEXT("state"));
    RegisterEmitter(TEXT("status"));
}

void URshipContentMappingManager::DeleteTargetForPath(const FString& TargetPath)
{
    if (!Subsystem)
    {
        return;
    }

    TSharedPtr<FJsonObject> TargetJson = MakeShared<FJsonObject>();
    TargetJson->SetStringField(TEXT("id"), TargetPath);
    TargetJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    Subsystem->DelItem(TEXT("Target"), TargetJson, ERshipMessagePriority::High, TargetPath);
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

void URshipContentMappingManager::EmitContextState(const FRshipRenderContextState& ContextState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildContextTargetId(ContextState.Id);
    Subsystem->PulseEmitter(TargetId, TEXT("state"), BuildRenderContextJson(ContextState));

    TSharedPtr<FJsonObject> StatusPayload = MakeShared<FJsonObject>();
    StatusPayload->SetStringField(TEXT("status"), ContextState.bEnabled ? TEXT("enabled") : TEXT("disabled"));
    if (!ContextState.LastError.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("lastError"), ContextState.LastError);
    }
    if (!ContextState.CameraId.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("cameraId"), ContextState.CameraId);
    }
    if (!ContextState.AssetId.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("assetId"), ContextState.AssetId);
    }
    StatusPayload->SetBoolField(TEXT("hasTexture"), ContextState.ResolvedTexture != nullptr);
    Subsystem->PulseEmitter(TargetId, TEXT("status"), StatusPayload);
}

void URshipContentMappingManager::EmitSurfaceState(const FRshipMappingSurfaceState& SurfaceState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildSurfaceTargetId(SurfaceState.Id);
    Subsystem->PulseEmitter(TargetId, TEXT("state"), BuildMappingSurfaceJson(SurfaceState));
    EmitStatus(TargetId, SurfaceState.bEnabled ? TEXT("enabled") : TEXT("disabled"), SurfaceState.LastError);
}

void URshipContentMappingManager::EmitMappingState(const FRshipContentMappingState& MappingState)
{
    if (!Subsystem)
    {
        return;
    }

    const FString TargetId = BuildMappingTargetId(MappingState.Id);
    Subsystem->PulseEmitter(TargetId, TEXT("state"), BuildMappingJson(MappingState));
    EmitStatus(TargetId, MappingState.bEnabled ? TEXT("enabled") : TEXT("disabled"), MappingState.LastError);
}

void URshipContentMappingManager::EmitStatus(const FString& TargetId, const FString& Status, const FString& LastError)
{
    if (!Subsystem)
    {
        return;
    }

    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("status"), Status);
    if (!LastError.IsEmpty())
    {
        Payload->SetStringField(TEXT("lastError"), LastError);
    }
    Subsystem->PulseEmitter(TargetId, TEXT("status"), Payload);
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
                || MappingState.Config->HasTypedField<EJson::Object>(TEXT("feedRect"))
                || MappingState.Config->HasTypedField<EJson::Array>(TEXT("feedRects"))
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

    bool bHandled = true;
    if (ActionName == TEXT("setEnabled"))
    {
        ContextState->bEnabled = GetBoolField(Data, TEXT("enabled"), ContextState->bEnabled);
    }
    else if (ActionName == TEXT("setCameraId"))
    {
        ContextState->CameraId = GetStringField(Data, TEXT("cameraId"), ContextState->CameraId);
    }
    else if (ActionName == TEXT("setAssetId"))
    {
        ContextState->AssetId = GetStringField(Data, TEXT("assetId"), ContextState->AssetId);
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

    if (bHandled && Subsystem)
    {
        ResolveRenderContext(*ContextState);
        Subsystem->SetItem(TEXT("RenderContext"), BuildRenderContextJson(*ContextState), ERshipMessagePriority::High, ContextState->Id);
        EmitContextState(*ContextState);
        MarkMappingsDirty();
        MarkCacheDirty();
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

    bool bHandled = true;
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

    if (bHandled && Subsystem)
    {
        ResolveMappingSurface(*SurfaceState);
        Subsystem->SetItem(TEXT("MappingSurface"), BuildMappingSurfaceJson(*SurfaceState), ERshipMessagePriority::High, SurfaceState->Id);
        EmitSurfaceState(*SurfaceState);
        MarkMappingsDirty();
        MarkCacheDirty();
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

    bool bHandled = true;
    auto CloneObject = [](const TSharedPtr<FJsonObject>& InObj) -> TSharedPtr<FJsonObject>
    {
        return InObj.IsValid() ? MakeShared<FJsonObject>(*InObj) : MakeShared<FJsonObject>();
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
            MappingState->Config = Data->GetObjectField(TEXT("config"));
        }
        else
        {
            if (!MappingState->Config.IsValid())
            {
                MappingState->Config = MakeShared<FJsonObject>();
            }
            MappingState->Config->SetStringField(TEXT("projectionType"), GetStringField(Data, TEXT("projectionType")));
            if (Data->HasTypedField<EJson::Object>(TEXT("projectorPosition")))
            {
                MappingState->Config->SetObjectField(TEXT("projectorPosition"), Data->GetObjectField(TEXT("projectorPosition")));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("projectorRotation")))
            {
                MappingState->Config->SetObjectField(TEXT("projectorRotation"), Data->GetObjectField(TEXT("projectorRotation")));
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
                MappingState->Config->SetObjectField(TEXT("cylindrical"), Data->GetObjectField(TEXT("cylindrical")));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("cameraPlate")))
            {
                MappingState->Config->SetObjectField(TEXT("cameraPlate"), Data->GetObjectField(TEXT("cameraPlate")));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("spatial")))
            {
                MappingState->Config->SetObjectField(TEXT("spatial"), Data->GetObjectField(TEXT("spatial")));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("depthMap")))
            {
                MappingState->Config->SetObjectField(TEXT("depthMap"), Data->GetObjectField(TEXT("depthMap")));
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
                MappingState->Config->SetObjectField(TEXT("customProjectionMatrix"), Data->GetObjectField(TEXT("customProjectionMatrix")));
            }
            if (Data->HasTypedField<EJson::Object>(TEXT("matrix")))
            {
                MappingState->Config->SetObjectField(TEXT("customProjectionMatrix"), Data->GetObjectField(TEXT("matrix")));
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
        if (Data->HasTypedField<EJson::Object>(TEXT("uvTransform")))
        {
            MappingState->Config->SetObjectField(TEXT("uvTransform"), Data->GetObjectField(TEXT("uvTransform")));
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
            else if (Data->HasTypedField<EJson::Array>(TEXT("links")))
            {
                FeedV2->SetArrayField(TEXT("routes"), Data->GetArrayField(TEXT("links")));
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
    }

    if (bHandled && Subsystem)
    {
        Subsystem->SetItem(TEXT("Mapping"), BuildMappingJson(*MappingState), ERshipMessagePriority::High, MappingState->Id);
        EmitMappingState(*MappingState);
        MarkMappingsDirty();
        MarkCacheDirty();
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

bool URshipContentMappingManager::ValidateMaterialContract(UMaterialInterface* Material, FString& OutError) const
{
    OutError.Reset();
    if (!Material)
    {
        OutError = TEXT("ContentMapping material is null.");
        return false;
    }

    TSet<FName> ScalarParams;
    TSet<FName> VectorParams;
    TSet<FName> TextureParams;

    {
        TArray<FMaterialParameterInfo> Infos;
        TArray<FGuid> Ids;
        Material->GetAllScalarParameterInfo(Infos, Ids);
        for (const FMaterialParameterInfo& Info : Infos)
        {
            ScalarParams.Add(Info.Name);
        }
    }
    {
        TArray<FMaterialParameterInfo> Infos;
        TArray<FGuid> Ids;
        Material->GetAllVectorParameterInfo(Infos, Ids);
        for (const FMaterialParameterInfo& Info : Infos)
        {
            VectorParams.Add(Info.Name);
        }
    }
    {
        TArray<FMaterialParameterInfo> Infos;
        TArray<FGuid> Ids;
        Material->GetAllTextureParameterInfo(Infos, Ids);
        for (const FMaterialParameterInfo& Info : Infos)
        {
            TextureParams.Add(Info.Name);
        }
    }

    static const FName RequiredScalars[] =
    {
        ParamMappingMode,
        ParamProjectionType,
        ParamUVRotation,
        ParamUVScaleU,
        ParamUVScaleV,
        ParamUVOffsetU,
        ParamUVOffsetV,
        ParamOpacity,
        ParamMappingIntensity,
        ParamUVChannel,
        ParamDebugCoverage,
        ParamRadialFlag,
        ParamContentMode,
        ParamBorderExpansion
    };

    static const FName RequiredVectors[] =
    {
        ParamProjectorRow0, ParamProjectorRow1, ParamProjectorRow2, ParamProjectorRow3,
        ParamUVTransform,
        ParamPreviewTint,
        ParamDebugUnmappedColor,
        ParamDebugMappedColor,
        ParamCylinderParams,
        ParamCylinderExtent,
        ParamSphereParams,
        ParamSphereArc,
        ParamParallelSize,
        ParamMaskAngle,
        ParamFisheyeParams,
        ParamMeshEyepoint,
        ParamCameraPlateParams,
        ParamSpatialParams0,
        ParamSpatialParams1,
        ParamDepthMapParams
    };

    static const FName RequiredTextures[] =
    {
        ParamContextTexture,
        ParamContextDepthTexture
    };

    TArray<FString> Missing;
    auto AppendMissing = [&Missing](const TCHAR* Category, const FName& Name)
    {
        Missing.Add(FString::Printf(TEXT("%s:%s"), Category, *Name.ToString()));
    };

    for (const FName& Name : RequiredScalars)
    {
        if (!ScalarParams.Contains(Name))
        {
            AppendMissing(TEXT("scalar"), Name);
        }
    }
    for (const FName& Name : RequiredVectors)
    {
        if (!VectorParams.Contains(Name))
        {
            AppendMissing(TEXT("vector"), Name);
        }
    }
    for (const FName& Name : RequiredTextures)
    {
        if (!TextureParams.Contains(Name))
        {
            AppendMissing(TEXT("texture"), Name);
        }
    }

    if (Missing.Num() > 0)
    {
        OutError = FString::Printf(
            TEXT("Material '%s' missing contract params: %s"),
            *Material->GetName(),
            *FString::Join(Missing, TEXT(", ")));
        return false;
    }

    return true;
}

void URshipContentMappingManager::EnsureMaterialContract()
{
    if (bMaterialContractChecked && LastContractMaterial.Get() == ContentMappingMaterial)
    {
        return;
    }

    bMaterialContractChecked = true;
    LastContractMaterial = ContentMappingMaterial;
    bMaterialContractValid = ValidateMaterialContract(ContentMappingMaterial, MaterialContractError);
    if (!bMaterialContractValid)
    {
        UE_LOG(LogRshipExec, Error, TEXT("%s"), *MaterialContractError);
    }
}

void URshipContentMappingManager::BuildFallbackMaterial()
{
#if WITH_EDITOR
    UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
    if (!Mat)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Failed to create transient fallback mapping material"));
        return;
    }

    Mat->MaterialDomain = EMaterialDomain::MD_Surface;
    Mat->BlendMode = BLEND_Opaque;
    Mat->TwoSided = true;
    Mat->SetShadingModel(MSM_Unlit);

    auto AddExpression = [Mat](UMaterialExpression* Expression) -> UMaterialExpression*
    {
        Mat->GetExpressionCollection().AddExpression(Expression);
        return Expression;
    };

    auto MakeScalarParam = [Mat, &AddExpression](const FName& Name, float DefaultValue) -> UMaterialExpressionScalarParameter*
    {
        UMaterialExpressionScalarParameter* Parameter = NewObject<UMaterialExpressionScalarParameter>(Mat);
        Parameter->ParameterName = Name;
        Parameter->DefaultValue = DefaultValue;
        AddExpression(Parameter);
        return Parameter;
    };

    auto MakeVectorParam = [Mat, &AddExpression](const FName& Name, const FLinearColor& DefaultValue) -> UMaterialExpressionVectorParameter*
    {
        UMaterialExpressionVectorParameter* Parameter = NewObject<UMaterialExpressionVectorParameter>(Mat);
        Parameter->ParameterName = Name;
        Parameter->DefaultValue = DefaultValue;
        AddExpression(Parameter);
        return Parameter;
    };

    auto MakeTextureParam = [Mat, &AddExpression](const FName& Name) -> UMaterialExpressionTextureSampleParameter2D*
    {
        UMaterialExpressionTextureSampleParameter2D* Parameter = NewObject<UMaterialExpressionTextureSampleParameter2D>(Mat);
        Parameter->ParameterName = Name;
        Parameter->SamplerType = SAMPLERTYPE_Color;
        Parameter->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
        AddExpression(Parameter);
        return Parameter;
    };

    // Material vector parameter alpha is unreliable on some shader paths (Metal can lower it
    // to float3). Build a float4 by appending a constant default alpha to avoid ComponentMask
    // compile failures.
    auto MakeVector4Input = [Mat, &AddExpression](UMaterialExpressionVectorParameter* VectorParam) -> UMaterialExpression*
    {
        if (!VectorParam)
        {
            return nullptr;
        }

        UMaterialExpressionConstant* AlphaDefault = NewObject<UMaterialExpressionConstant>(Mat);
        AlphaDefault->R = VectorParam->DefaultValue.A;
        AddExpression(AlphaDefault);

        UMaterialExpressionAppendVector* Append = NewObject<UMaterialExpressionAppendVector>(Mat);
        Append->A.Expression = VectorParam;
        Append->B.Expression = AlphaDefault;
        AddExpression(Append);
        return Append;
    };

    auto AddCustomInput = [](UMaterialExpressionCustom* Custom, const TCHAR* Name, UMaterialExpression* Source)
    {
        FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
        Input.InputName = Name;
        Input.Input.Expression = Source;
    };

    UMaterialExpressionTextureCoordinate* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(Mat);
    TexCoord->CoordinateIndex = 0;
    AddExpression(TexCoord);

    UMaterialExpressionWorldPosition* WorldPosition = NewObject<UMaterialExpressionWorldPosition>(Mat);
    AddExpression(WorldPosition);

    UMaterialExpressionTextureSampleParameter2D* ContextTextureParam = MakeTextureParam(ParamContextTexture);
    UMaterialExpressionTextureSampleParameter2D* DepthTextureParam = MakeTextureParam(ParamContextDepthTexture);

    UMaterialExpressionScalarParameter* MappingModeParam = MakeScalarParam(ParamMappingMode, 0.0f);
    UMaterialExpressionScalarParameter* ProjectionTypeParam = MakeScalarParam(ParamProjectionType, 0.0f);
    UMaterialExpressionScalarParameter* UVRotationParam = MakeScalarParam(ParamUVRotation, 0.0f);
    UMaterialExpressionScalarParameter* UVScaleUParam = MakeScalarParam(ParamUVScaleU, 1.0f);
    UMaterialExpressionScalarParameter* UVScaleVParam = MakeScalarParam(ParamUVScaleV, 1.0f);
    UMaterialExpressionScalarParameter* UVOffsetUParam = MakeScalarParam(ParamUVOffsetU, 0.0f);
    UMaterialExpressionScalarParameter* UVOffsetVParam = MakeScalarParam(ParamUVOffsetV, 0.0f);
    UMaterialExpressionScalarParameter* OpacityParam = MakeScalarParam(ParamOpacity, 1.0f);
    UMaterialExpressionScalarParameter* MappingIntensityParam = MakeScalarParam(ParamMappingIntensity, 1.0f);
    UMaterialExpressionScalarParameter* UVChannelParam = MakeScalarParam(ParamUVChannel, 0.0f);
    UMaterialExpressionScalarParameter* DebugCoverageParam = MakeScalarParam(ParamDebugCoverage, 0.0f);
    UMaterialExpressionScalarParameter* RadialFlagParam = MakeScalarParam(ParamRadialFlag, 0.0f);
    UMaterialExpressionScalarParameter* ContentModeParam = MakeScalarParam(ParamContentMode, 0.0f);
    UMaterialExpressionScalarParameter* BorderExpansionParam = MakeScalarParam(ParamBorderExpansion, 0.0f);

    UMaterialExpressionVectorParameter* ProjectorRow0Param = MakeVectorParam(ParamProjectorRow0, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* ProjectorRow1Param = MakeVectorParam(ParamProjectorRow1, FLinearColor(0.0f, 1.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* ProjectorRow2Param = MakeVectorParam(ParamProjectorRow2, FLinearColor(0.0f, 0.0f, 1.0f, 0.0f));
    UMaterialExpressionVectorParameter* ProjectorRow3Param = MakeVectorParam(ParamProjectorRow3, FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
    UMaterialExpressionVectorParameter* UVTransformParam = MakeVectorParam(ParamUVTransform, FLinearColor(1.0f, 1.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* PreviewTintParam = MakeVectorParam(ParamPreviewTint, FLinearColor::White);
    UMaterialExpressionVectorParameter* DebugUnmappedColorParam = MakeVectorParam(ParamDebugUnmappedColor, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
    UMaterialExpressionVectorParameter* DebugMappedColorParam = MakeVectorParam(ParamDebugMappedColor, FLinearColor::White);
    UMaterialExpressionVectorParameter* CylinderParamsParam = MakeVectorParam(ParamCylinderParams, FLinearColor(0.0f, 0.0f, 1.0f, 500.0f));
    UMaterialExpressionVectorParameter* CylinderExtentParam = MakeVectorParam(ParamCylinderExtent, FLinearColor(1000.0f, 0.0f, 360.0f, 0.0f));
    UMaterialExpressionVectorParameter* SphereParamsParam = MakeVectorParam(ParamSphereParams, FLinearColor(0.0f, 0.0f, 0.0f, 500.0f));
    UMaterialExpressionVectorParameter* SphereArcParam = MakeVectorParam(ParamSphereArc, FLinearColor(360.0f, 180.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* ParallelSizeParam = MakeVectorParam(ParamParallelSize, FLinearColor(1000.0f, 1000.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* MaskAngleParam = MakeVectorParam(ParamMaskAngle, FLinearColor(0.0f, 360.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* FisheyeParamsParam = MakeVectorParam(ParamFisheyeParams, FLinearColor(180.0f, 0.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* MeshEyepointParam = MakeVectorParam(ParamMeshEyepoint, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* CameraPlateParamsParam = MakeVectorParam(ParamCameraPlateParams, FLinearColor(0.0f, 0.5f, 0.5f, 0.0f));
    UMaterialExpressionVectorParameter* SpatialParams0Param = MakeVectorParam(ParamSpatialParams0, FLinearColor(1.0f, 1.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* SpatialParams1Param = MakeVectorParam(ParamSpatialParams1, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
    UMaterialExpressionVectorParameter* DepthMapParamsParam = MakeVectorParam(ParamDepthMapParams, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));

    UMaterialExpression* ProjectorRow0Input = MakeVector4Input(ProjectorRow0Param);
    UMaterialExpression* ProjectorRow1Input = MakeVector4Input(ProjectorRow1Param);
    UMaterialExpression* ProjectorRow2Input = MakeVector4Input(ProjectorRow2Param);
    UMaterialExpression* ProjectorRow3Input = MakeVector4Input(ProjectorRow3Param);
    UMaterialExpression* UVTransformInput = MakeVector4Input(UVTransformParam);
    UMaterialExpression* CameraPlateParamsInput = MakeVector4Input(CameraPlateParamsParam);
    UMaterialExpression* SpatialParams0Input = MakeVector4Input(SpatialParams0Param);
    UMaterialExpression* DepthMapParamsInput = MakeVector4Input(DepthMapParamsParam);

    UMaterialExpressionCustom* ResolveUvCustom = NewObject<UMaterialExpressionCustom>(Mat);
    ResolveUvCustom->OutputType = CMOT_Float2;
    ResolveUvCustom->Code = TEXT(R"UVHLSL(
const float PI = 3.14159265f;
float2 uv = TexCoord0;
const float2 uvScale = float2(max(0.0001f, UVScaleU), max(0.0001f, UVScaleV));
uv = uv * uvScale + float2(UVOffsetU, UVOffsetV);

const float2 pivot = UVTransform.zw;
const float rotationRad = UVRotation * (PI / 180.0f);
const float s = sin(rotationRad);
const float c = cos(rotationRad);
const float2 centered = uv - pivot;
uv = float2((centered.x * c) - (centered.y * s), (centered.x * s) + (centered.y * c)) + pivot;
uv = (uv - 0.5f) * max(UVTransform.xy, float2(0.0001f, 0.0001f)) + 0.5f;
uv += UVChannel * 0.0f;

if (MappingMode > 0.5f)
{
    const float4 p = float4(WorldPos.xyz, 1.0f);
    const float4 clip = float4(dot(ProjectorRow0, p), dot(ProjectorRow1, p), dot(ProjectorRow2, p), dot(ProjectorRow3, p));
    const float invW = (abs(clip.w) > 0.0001f) ? (1.0f / clip.w) : 0.0f;
    uv = (clip.xy * invW * 0.5f) + 0.5f;

    // Perspective mode in this pipeline is equirectangular from projector position.
    if (ProjectionType < 0.5f)
    {
        const float3 dir = normalize(WorldPos.xyz - SpatialParams1.xyz);
        uv = float2((atan2(dir.y, dir.x) / (2.0f * PI)) + 0.5f, acos(clamp(dir.z, -1.0f, 1.0f)) / PI);
    }
    else if (ProjectionType > 0.5f && ProjectionType < 1.5f)
    {
        float3 axis = CylinderParams.xyz;
        axis = (dot(axis, axis) > 0.0001f) ? normalize(axis) : float3(0.0f, 0.0f, 1.0f);
        const float3 rel = WorldPos.xyz - SpatialParams1.xyz;
        const float start = radians(CylinderExtent.y);
        const float end = radians(CylinderExtent.z);
        const float span = max(0.0001f, end - start);
        const float angle = atan2(rel.y, rel.x);
        const float height = max(1.0f, CylinderExtent.x);
        const float v = (dot(rel, axis) + (height * 0.5f)) / height;
        uv = float2((angle - start) / span, v);
    }
    else if (ProjectionType > 2.5f && ProjectionType < 3.5f)
    {
        float3 dir = normalize(WorldPos.xyz - SphereParams.xyz);
        uv = float2((atan2(dir.y, dir.x) / (2.0f * PI)) + 0.5f, acos(clamp(dir.z, -1.0f, 1.0f)) / PI);
        uv.x *= max(0.0001f, SphereArc.x / 360.0f);
        uv.y *= max(0.0001f, SphereArc.y / 180.0f);
    }
    else if (ProjectionType > 3.5f && ProjectionType < 4.5f)
    {
        const float2 size = max(ParallelSize.xy, float2(1.0f, 1.0f));
        uv = ((WorldPos.xy - SpatialParams1.xy) / size) + 0.5f;
    }
    else if ((ProjectionType > 4.5f && ProjectionType < 5.5f) || RadialFlag > 0.5f)
    {
        const float2 radial = (WorldPos.xy - SpatialParams1.xy) * 0.001f;
        uv = frac((radial + 1.0f) * 0.5f);
    }
    else if (ProjectionType > 5.5f && ProjectionType < 6.5f)
    {
        // Mesh mode uses eyepoint-driven clip projection (rays from eyepoint through mesh surface).
        // Keep mesh UVs as a deterministic fallback for degenerate clip space.
        if (abs(clip.w) <= 0.0001f)
        {
            uv = TexCoord0;
        }
    }
    else if (ProjectionType > 6.5f && ProjectionType < 7.5f)
    {
        const float3 dir = normalize(WorldPos.xyz - SpatialParams1.xyz);
        const float theta = acos(clamp(dir.z, -1.0f, 1.0f));
        const float radius = theta / radians(max(1.0f, FisheyeParams.x));
        const float phi = atan2(dir.y, dir.x);
        uv = float2(cos(phi), sin(phi)) * (radius * 0.5f) + 0.5f;
    }
    else if (ProjectionType > 8.5f && ProjectionType < 9.5f)
    {
        const float2 anchor = saturate(CameraPlateParams.yz);
        float2 fromAnchor = uv - anchor;
        if (CameraPlateParams.x < 0.5f)
        {
            fromAnchor *= 0.85f;
        }
        else if (CameraPlateParams.x < 1.5f)
        {
            fromAnchor *= 1.15f;
        }
        uv = fromAnchor + anchor;
        if (CameraPlateParams.w > 0.5f)
        {
            uv.y = 1.0f - uv.y;
        }
    }
    else if (ProjectionType > 9.5f && ProjectionType < 10.5f)
    {
        uv = (uv * SpatialParams0.xy) + SpatialParams0.zw;
    }
    else if (ProjectionType > 10.5f && ProjectionType < 11.5f)
    {
        uv += float2(DepthMapParams.y, DepthMapParams.x) * (WorldPos.z * 0.0001f);
    }
}

const float border = BorderExpansion * 0.001f;
uv = (uv - 0.5f) * (1.0f + (border * 2.0f)) + 0.5f;

if (MaskAngle.z > 0.5f)
{
    const float2 radial = uv - 0.5f;
    float angleDeg = degrees(atan2(radial.y, radial.x));
    if (angleDeg < 0.0f)
    {
        angleDeg += 360.0f;
    }

    const float start = MaskAngle.x;
    const float end = MaskAngle.y;
    float inRange = 0.0f;
    if (start <= end)
    {
        inRange = (angleDeg >= start && angleDeg <= end) ? 1.0f : 0.0f;
    }
    else
    {
        inRange = (angleDeg >= start || angleDeg <= end) ? 1.0f : 0.0f;
    }

    if (inRange < 0.5f)
    {
        uv = float2(-1.0f, -1.0f);
    }
}

return uv;
)UVHLSL");
    AddExpression(ResolveUvCustom);

    AddCustomInput(ResolveUvCustom, TEXT("TexCoord0"), TexCoord);
    AddCustomInput(ResolveUvCustom, TEXT("WorldPos"), WorldPosition);
    AddCustomInput(ResolveUvCustom, TEXT("MappingMode"), MappingModeParam);
    AddCustomInput(ResolveUvCustom, TEXT("ProjectionType"), ProjectionTypeParam);
    AddCustomInput(ResolveUvCustom, TEXT("UVTransform"), UVTransformInput ? UVTransformInput : UVTransformParam);
    AddCustomInput(ResolveUvCustom, TEXT("UVRotation"), UVRotationParam);
    AddCustomInput(ResolveUvCustom, TEXT("UVScaleU"), UVScaleUParam);
    AddCustomInput(ResolveUvCustom, TEXT("UVScaleV"), UVScaleVParam);
    AddCustomInput(ResolveUvCustom, TEXT("UVOffsetU"), UVOffsetUParam);
    AddCustomInput(ResolveUvCustom, TEXT("UVOffsetV"), UVOffsetVParam);
    AddCustomInput(ResolveUvCustom, TEXT("UVChannel"), UVChannelParam);
    AddCustomInput(ResolveUvCustom, TEXT("ProjectorRow0"), ProjectorRow0Input ? ProjectorRow0Input : ProjectorRow0Param);
    AddCustomInput(ResolveUvCustom, TEXT("ProjectorRow1"), ProjectorRow1Input ? ProjectorRow1Input : ProjectorRow1Param);
    AddCustomInput(ResolveUvCustom, TEXT("ProjectorRow2"), ProjectorRow2Input ? ProjectorRow2Input : ProjectorRow2Param);
    AddCustomInput(ResolveUvCustom, TEXT("ProjectorRow3"), ProjectorRow3Input ? ProjectorRow3Input : ProjectorRow3Param);
    AddCustomInput(ResolveUvCustom, TEXT("CameraPlateParams"), CameraPlateParamsInput ? CameraPlateParamsInput : CameraPlateParamsParam);
    AddCustomInput(ResolveUvCustom, TEXT("SpatialParams0"), SpatialParams0Input ? SpatialParams0Input : SpatialParams0Param);
    AddCustomInput(ResolveUvCustom, TEXT("SpatialParams1"), SpatialParams1Param);
    AddCustomInput(ResolveUvCustom, TEXT("DepthMapParams"), DepthMapParamsInput ? DepthMapParamsInput : DepthMapParamsParam);
    AddCustomInput(ResolveUvCustom, TEXT("CylinderParams"), CylinderParamsParam);
    AddCustomInput(ResolveUvCustom, TEXT("CylinderExtent"), CylinderExtentParam);
    AddCustomInput(ResolveUvCustom, TEXT("SphereParams"), SphereParamsParam);
    AddCustomInput(ResolveUvCustom, TEXT("SphereArc"), SphereArcParam);
    AddCustomInput(ResolveUvCustom, TEXT("ParallelSize"), ParallelSizeParam);
    AddCustomInput(ResolveUvCustom, TEXT("MaskAngle"), MaskAngleParam);
    AddCustomInput(ResolveUvCustom, TEXT("FisheyeParams"), FisheyeParamsParam);
    AddCustomInput(ResolveUvCustom, TEXT("MeshEyepoint"), MeshEyepointParam);
    AddCustomInput(ResolveUvCustom, TEXT("RadialFlag"), RadialFlagParam);
    AddCustomInput(ResolveUvCustom, TEXT("BorderExpansion"), BorderExpansionParam);

    ContextTextureParam->Coordinates.Expression = ResolveUvCustom;
    DepthTextureParam->Coordinates.Expression = ResolveUvCustom;

    UMaterialExpressionCustom* ResolveColorCustom = NewObject<UMaterialExpressionCustom>(Mat);
    ResolveColorCustom->OutputType = CMOT_Float3;
    ResolveColorCustom->Code = TEXT(R"COLORHLSL(
float mapped = (UV.x >= 0.0f && UV.x <= 1.0f && UV.y >= 0.0f && UV.y <= 1.0f) ? 1.0f : 0.0f;
float3 color = ContextColor.rgb;

if (ProjectionType > 10.5f && ProjectionType < 11.5f)
{
    const float depthValue = DepthColor.r;
    const float depthNorm = saturate((depthValue * DepthMapParams.x + DepthMapParams.y - DepthMapParams.z) / max(0.0001f, DepthMapParams.w - DepthMapParams.z));
    color = lerp(color, depthNorm.xxx, 0.5f);
}

if (DebugCoverage > 0.5f)
{
    color = lerp(DebugUnmappedColor.rgb, DebugMappedColor.rgb, mapped);
}

if (ContentMode > 2.5f)
{
    color = floor(saturate(color) * 255.0f) / 255.0f;
}

color *= PreviewTint.rgb;
color *= saturate(MappingIntensity * Opacity);
return color;
)COLORHLSL");
    AddExpression(ResolveColorCustom);

    AddCustomInput(ResolveColorCustom, TEXT("ContextColor"), ContextTextureParam);
    AddCustomInput(ResolveColorCustom, TEXT("DepthColor"), DepthTextureParam);
    AddCustomInput(ResolveColorCustom, TEXT("UV"), ResolveUvCustom);
    AddCustomInput(ResolveColorCustom, TEXT("ProjectionType"), ProjectionTypeParam);
    AddCustomInput(ResolveColorCustom, TEXT("DepthMapParams"), DepthMapParamsInput ? DepthMapParamsInput : DepthMapParamsParam);
    AddCustomInput(ResolveColorCustom, TEXT("DebugCoverage"), DebugCoverageParam);
    AddCustomInput(ResolveColorCustom, TEXT("DebugUnmappedColor"), DebugUnmappedColorParam);
    AddCustomInput(ResolveColorCustom, TEXT("DebugMappedColor"), DebugMappedColorParam);
    AddCustomInput(ResolveColorCustom, TEXT("PreviewTint"), PreviewTintParam);
    AddCustomInput(ResolveColorCustom, TEXT("MappingIntensity"), MappingIntensityParam);
    AddCustomInput(ResolveColorCustom, TEXT("Opacity"), OpacityParam);
    AddCustomInput(ResolveColorCustom, TEXT("ContentMode"), ContentModeParam);

    Mat->GetEditorOnlyData()->EmissiveColor.Expression = ResolveColorCustom;
    Mat->GetEditorOnlyData()->EmissiveColor.OutputIndex = 0;
    Mat->GetEditorOnlyData()->BaseColor.Expression = ResolveColorCustom;
    Mat->GetEditorOnlyData()->BaseColor.OutputIndex = 0;

    // Keep this parameter live even though fallback is opaque.
    UMaterialExpressionMultiply* OpacityMultiply = NewObject<UMaterialExpressionMultiply>(Mat);
    OpacityMultiply->A.Expression = OpacityParam;
    OpacityMultiply->B.Expression = MappingIntensityParam;
    AddExpression(OpacityMultiply);
    Mat->GetEditorOnlyData()->Opacity.Expression = OpacityMultiply;
    Mat->GetEditorOnlyData()->Opacity.OutputIndex = 0;

    Mat->PreEditChange(nullptr);
    Mat->PostEditChange();

    ContentMappingMaterial = Mat;
    bMaterialContractChecked = false;
    LastContractMaterial = nullptr;
    UE_LOG(LogRshipExec, Log, TEXT("ContentMapping material rebuilt (deterministic contract fallback)"));
#else
    ContentMappingMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    bMaterialContractChecked = false;
    LastContractMaterial = nullptr;
    UE_LOG(LogRshipExec, Warning, TEXT("ContentMapping fallback material authoring is editor-only; using DefaultMaterial at runtime."));
#endif
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
