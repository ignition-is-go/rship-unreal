// Content Mapping Manager
// Manages render contexts, mapping surfaces, and surface mappings for content projection

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Dom/JsonObject.h"
#include "RshipContentMappingManager.generated.h"

class URshipSubsystem;
class URshipAssetStoreClient;
class ARshipCameraActor;
class AActor;
class ACameraActor;
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UTexture;
class UTexture2D;
class UTextureRenderTarget2D;

USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipRenderContextState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString ProjectId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString SourceType;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString CameraId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString AssetId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString DepthAssetId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    int32 Width = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    int32 Height = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString CaptureMode;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString DepthCaptureMode;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    bool bEnabled = true;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    bool bDepthCaptureEnabled = false;

    UPROPERTY(Transient)
    TWeakObjectPtr<ARshipCameraActor> CameraActor;

    UPROPERTY(Transient)
    TWeakObjectPtr<ACameraActor> SourceCameraActor;

    UPROPERTY(Transient)
    UTexture* ResolvedTexture = nullptr;

    UPROPERTY(Transient)
    UTexture* ResolvedDepthTexture = nullptr;

    UPROPERTY(Transient)
    TWeakObjectPtr<UTextureRenderTarget2D> DepthRenderTarget;

    UPROPERTY(Transient)
    TWeakObjectPtr<USceneCaptureComponent2D> DepthCaptureComponent;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString LastError;
};

USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipMappingSurfaceState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString ProjectId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString TargetId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    bool bEnabled = true;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    int32 UVChannel = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    TArray<int32> MaterialSlots;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString MeshComponentName;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString ActorPath;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString LastError;

    UPROPERTY(Transient)
    TWeakObjectPtr<UMeshComponent> MeshComponent;

    UPROPERTY(Transient)
    TMap<int32, TWeakObjectPtr<UMaterialInterface>> OriginalMaterials;

    UPROPERTY(Transient)
    TMap<int32, UMaterialInstanceDynamic*> MaterialInstances;

    UPROPERTY(Transient)
    TMap<int32, uint32> MaterialBindingHashes;

    UPROPERTY(Transient)
    double NextResolveRetryTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipContentMappingState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString ProjectId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString Type;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString ContextId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    TArray<FString> SurfaceIds;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    float Opacity = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    bool bEnabled = true;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString LastError;

    TSharedPtr<FJsonObject> Config;
};

UCLASS(BlueprintType)
class RSHIPEXEC_API URshipContentMappingManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    void ProcessRenderContextEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);
    void ProcessMappingSurfaceEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);
    void ProcessMappingEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);

    bool RouteAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data);

    TArray<FRshipRenderContextState> GetRenderContexts() const;
    TArray<FRshipMappingSurfaceState> GetMappingSurfaces() const;
    TArray<FRshipContentMappingState> GetMappings() const;

    void SetDebugOverlayEnabled(bool bEnabled);
    bool IsDebugOverlayEnabled() const;
    void SetCoveragePreviewEnabled(bool bEnabled);
    bool IsCoveragePreviewEnabled() const;

    // CRUD helpers (used by editor panel)
    FString CreateRenderContext(const FRshipRenderContextState& InState);
    bool UpdateRenderContext(const FRshipRenderContextState& InState);
    bool DeleteRenderContext(const FString& Id);

    FString CreateMappingSurface(const FRshipMappingSurfaceState& InState);
    bool UpdateMappingSurface(const FRshipMappingSurfaceState& InState);
    bool DeleteMappingSurface(const FString& Id);

    FString CreateMapping(const FRshipContentMappingState& InState);
    bool UpdateMapping(const FRshipContentMappingState& InState);
    bool DeleteMapping(const FString& Id);

#if WITH_DEV_AUTOMATION_TESTS
    bool ValidateMaterialContractForTest(UMaterialInterface* Material, FString& OutError) const
    {
        return ValidateMaterialContract(Material, OutError);
    }

    void ApplyMaterialParametersForTest(
        UMaterialInstanceDynamic* MID,
        const FRshipContentMappingState& MappingState,
        const FRshipMappingSurfaceState& SurfaceState,
        const FRshipRenderContextState* ContextState)
    {
        ApplyMaterialParameters(MID, MappingState, SurfaceState, ContextState);
    }

    TSharedPtr<FJsonObject> BuildRenderContextJsonForTest(const FRshipRenderContextState& ContextState) const
    {
        return BuildRenderContextJson(ContextState);
    }
#endif

private:
    struct FFeedSingleRtBinding
    {
        bool bValid = false;
        UTexture* Texture = nullptr;
        UTexture* DepthTexture = nullptr;
        bool bHasSourceRect = false;
        float SourceU = 0.0f;
        float SourceV = 0.0f;
        float SourceW = 1.0f;
        float SourceH = 1.0f;
        bool bHasDestinationRect = false;
        float DestinationU = 0.0f;
        float DestinationV = 0.0f;
        float DestinationW = 1.0f;
        float DestinationH = 1.0f;
        FString Error;
    };

    struct FFeedSingleRtPreparedRoute
    {
        bool bPrepared = false;
        bool bHasRoute = false;
        FString Error;
        TArray<FString> ContextCandidates;
        int32 SourceWidth = 0;
        int32 SourceHeight = 0;
        int32 DestinationWidth = 0;
        int32 DestinationHeight = 0;
        int32 SourceX = 0;
        int32 SourceY = 0;
        int32 SourceW = 0;
        int32 SourceH = 0;
        int32 DestinationX = 0;
        int32 DestinationY = 0;
        int32 DestinationW = 0;
        int32 DestinationH = 0;
    };

    struct FRenderContextRuntimeState
    {
        uint32 SetupHash = 0;
        bool bHasAppliedTransform = false;
        FTransform LastAppliedTransform = FTransform::Identity;
        float LastAppliedFov = -1.0f;
    };

    struct FMappingRequiredContexts
    {
        TArray<FString> ContextIds;
        bool bKeepAllContextsAlive = false;
        bool bHasInvalidContextReference = false;
    };

    UPROPERTY()
    URshipSubsystem* Subsystem = nullptr;

    UPROPERTY()
    URshipAssetStoreClient* AssetStoreClient = nullptr;

    UPROPERTY(Transient)
    UMaterialInterface* ContentMappingMaterial = nullptr;

    TMap<FString, FRshipRenderContextState> RenderContexts;
    TMap<FString, FRshipMappingSurfaceState> MappingSurfaces;
    TMap<FString, FRshipContentMappingState> Mappings;
    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UTextureRenderTarget2D>> FeedCompositeTargets;
    UPROPERTY(Transient)
    TMap<FString, uint32> FeedCompositeStaticSignatures;
    TMap<FString, FFeedSingleRtPreparedRoute> FeedSingleRtBindingCache;
    TMap<FString, TArray<FString>> EffectiveSurfaceIdsCache;
    TMap<FString, FRenderContextRuntimeState> RenderContextRuntimeStates;
    TMap<FString, FMappingRequiredContexts> RequiredContextIdsCache;
    TMap<FString, FRshipContentMappingState> PendingMappingUpserts;
    TMap<FString, double> PendingMappingUpsertExpiry;
    TMap<FString, double> PendingMappingDeletes;

    TMap<FString, TWeakObjectPtr<UTexture2D>> AssetTextureCache;
    TSet<FString> PendingAssetDownloads;

    bool bMappingsDirty = false;
    bool bCacheDirty = false;
    bool bMappingsArmed = false;
    bool bWasConnected = false;
    bool bNeedsWorldResolutionRetry = false;
    bool bDebugOverlayEnabled = false;
    bool bCoveragePreviewEnabled = false;
    bool bMaterialContractValid = false;
    bool bMaterialContractChecked = false;
    TWeakObjectPtr<UMaterialInterface> LastContractMaterial;
    FString MaterialContractError;
    float DebugOverlayAccumulated = 0.0f;
    mutable TWeakObjectPtr<UWorld> LastValidWorld;
    uint64 RuntimeStateRevision = 1;
    double LastPerfLogTimeSeconds = 0.0;
    float LastTickMsTotal = 0.0f;
    float LastTickMsRebuild = 0.0f;
    float LastTickMsRefresh = 0.0f;
    float LastTickMsCacheSave = 0.0f;
    int32 LastTickEnabledMappings = 0;
    int32 LastTickActiveContexts = 0;
    int32 LastTickAppliedSurfaces = 0;
    bool bRuntimePreparePending = true;
    FString CachedEnabledTextureContextId;
    FString CachedAnyTextureContextId;
    FString CachedEnabledContextId;
    FString CachedAnyContextId;

    void MarkMappingsDirty();
    void ArmMappings();
    void MarkCacheDirty();
    bool HasAnyEnabledMappings() const;
    const TArray<FString>& GetEffectiveSurfaceIds(FRshipContentMappingState& MappingState);
    void RefreshResolvedContextFallbackIds();
    void RefreshLiveMappings();
    UWorld* GetBestWorld() const;

    void ResolveRenderContext(FRshipRenderContextState& ContextState);
    void ResolveMappingSurface(FRshipMappingSurfaceState& SurfaceState);
    void RebuildMappings();
    void RestoreSurfaceMaterials(FRshipMappingSurfaceState& SurfaceState);

    void ApplyMappingToSurface(
        const FRshipContentMappingState& MappingState,
        FRshipMappingSurfaceState& SurfaceState,
        const FRshipRenderContextState* ContextState);

    void ApplyMaterialParameters(
        UMaterialInstanceDynamic* MID,
        const FRshipContentMappingState& MappingState,
        const FRshipMappingSurfaceState& SurfaceState,
        const FRshipRenderContextState* ContextState,
        bool bUseFeedV2 = false,
        const FFeedSingleRtBinding* FeedBinding = nullptr);

    bool IsFeedV2Mapping(const FRshipContentMappingState& MappingState) const;
    bool IsKnownRenderContextId(const FString& ContextId) const;
    FString GetPreferredRuntimeContextId() const;
    void PrepareMappingsForRuntime(bool bEmitChanges);
    void CollectRequiredContextIdsForMappings(
        TSet<FString>& OutRequiredContextIds,
        bool& OutHasEnabledMappings,
        bool& OutKeepAllContextsAlive,
        bool& OutHasInvalidContextReference);
    const FRshipRenderContextState* ResolveEffectiveContextState(
        const FRshipContentMappingState& MappingState,
        bool bRequireTexture) const;
    bool EnsureMappingRuntimeReady(FRshipContentMappingState& MappingState);
    bool EnsureFeedMappingRuntimeReady(FRshipContentMappingState& MappingState);
    bool TryResolveFeedSingleRtBinding(
        const FRshipContentMappingState& MappingState,
        const FRshipMappingSurfaceState& SurfaceState,
        FFeedSingleRtBinding& OutBinding);
    UTexture* BuildFeedCompositeTextureForSurface(
        const FRshipContentMappingState& MappingState,
        const FRshipMappingSurfaceState& SurfaceState,
        FString& OutError);
    FString MakeFeedCompositeKey(const FString& MappingId, const FString& SurfaceId) const;
    void RemoveFeedCompositeTexturesForMapping(const FString& MappingId);
    void RemoveFeedCompositeTexturesForSurface(const FString& SurfaceId);

    void RegisterAllTargets();
    void RegisterContextTarget(const FRshipRenderContextState& ContextState);
    void RegisterSurfaceTarget(const FRshipMappingSurfaceState& SurfaceState);
    void RegisterMappingTarget(const FRshipContentMappingState& MappingState);
    void DeleteTargetForPath(const FString& TargetPath);

    FString BuildContextTargetId(const FString& ContextId) const;
    FString BuildSurfaceTargetId(const FString& SurfaceId) const;
    FString BuildMappingTargetId(const FString& MappingId) const;

    void EmitContextState(const FRshipRenderContextState& ContextState);
    void EmitSurfaceState(const FRshipMappingSurfaceState& SurfaceState);
    void EmitMappingState(const FRshipContentMappingState& MappingState);
    void EmitStatus(const FString& TargetId, const FString& Status, const FString& LastError);

    TSharedPtr<FJsonObject> BuildRenderContextJson(const FRshipRenderContextState& ContextState) const;
    TSharedPtr<FJsonObject> BuildMappingSurfaceJson(const FRshipMappingSurfaceState& SurfaceState) const;
    TSharedPtr<FJsonObject> BuildMappingJson(const FRshipContentMappingState& MappingState) const;

    bool HandleContextAction(const FString& ContextId, const FString& ActionName, const TSharedRef<FJsonObject>& Data);
    bool HandleSurfaceAction(const FString& SurfaceId, const FString& ActionName, const TSharedRef<FJsonObject>& Data);
    bool HandleMappingAction(const FString& MappingId, const FString& ActionName, const TSharedRef<FJsonObject>& Data);

    void SaveCache();
    void LoadCache();
    FString GetCachePath() const;
    FString GetAssetCacheDirectory() const;
    FString GetAssetCachePathForId(const FString& AssetId) const;

    void RequestAssetDownload(const FString& AssetId);
    void OnAssetDownloaded(const FString& AssetId, const FString& LocalPath);
    void OnAssetDownloadFailed(const FString& AssetId, const FString& ErrorMessage);
    UTexture2D* LoadTextureFromFile(const FString& LocalPath) const;
    void BuildFallbackMaterial();
    bool ValidateMaterialContract(UMaterialInterface* Material, FString& OutError) const;
    void EnsureMaterialContract();
    void TrackPendingMappingUpsert(const FRshipContentMappingState& State);
    void TrackPendingMappingDelete(const FString& MappingId);
    void PrunePendingMappingGuards(double NowSeconds);

    static FString GetStringField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, const FString& DefaultValue = TEXT(""));
    static bool GetBoolField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, bool DefaultValue);
    static int32 GetIntField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32 DefaultValue);
    static float GetNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, float DefaultValue);
    static TArray<FString> GetStringArrayField(const TSharedPtr<FJsonObject>& Obj, const FString& Field);
    static TArray<int32> GetIntArrayField(const TSharedPtr<FJsonObject>& Obj, const FString& Field);
};
