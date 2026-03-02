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

UENUM(BlueprintType)
enum class ERshipContentMappingRuntimeHealth : uint8
{
    Ready,
    Degraded,
    Blocked
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipRenderContextState
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
struct RSHIPMAPPING_API FRshipMappingSurfaceState
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
struct RSHIPMAPPING_API FRshipContentMappingState
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
class RSHIPMAPPING_API URshipContentMappingManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    // Reflection bridge for RshipExec (keeps module dependency one-way).
    UFUNCTION()
    void InitializeForSubsystem(URshipSubsystem* InSubsystem);

    UFUNCTION()
    void ShutdownForSubsystem();

    UFUNCTION()
    void TickForSubsystem(float DeltaSeconds);

    void ProcessRenderContextEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);
    void ProcessMappingSurfaceEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);
    void ProcessMappingEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);

    UFUNCTION()
    void ProcessRenderContextEventJson(const FString& Json, bool bDelete);

    UFUNCTION()
    void ProcessMappingSurfaceEventJson(const FString& Json, bool bDelete);

    UFUNCTION()
    void ProcessMappingEventJson(const FString& Json, bool bDelete);

    bool RouteAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data);

    UFUNCTION()
    bool RouteActionJson(const FString& TargetId, const FString& ActionId, const FString& DataJson);

    TArray<FRshipRenderContextState> GetRenderContexts() const;
    TArray<FRshipMappingSurfaceState> GetMappingSurfaces() const;
    TArray<FRshipContentMappingState> GetMappings() const;

    void SetDebugOverlayEnabled(bool bEnabled);
    bool IsDebugOverlayEnabled() const;
    UFUNCTION()
    void SetDebugOverlayEnabledForSubsystem(bool bEnabled);
    UFUNCTION()
    bool IsDebugOverlayEnabledForSubsystem() const;

    UFUNCTION()
    FString GetRenderContextsJsonForSubsystem() const;
    UFUNCTION(BlueprintPure, Category = "Rship|ContentMapping")
    ERshipContentMappingRuntimeHealth GetRuntimeHealth() const;
    UFUNCTION(BlueprintPure, Category = "Rship|ContentMapping")
    FString GetRuntimeHealthReason() const;
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
    struct FRenderContextRuntimeState
    {
        uint32 SetupHash = 0;
        bool bHasAppliedTransform = false;
        FTransform LastAppliedTransform = FTransform::Identity;
        float LastAppliedFov = -1.0f;
        double NextResolveRetryTimeSeconds = 0.0;
        double NextMissingSourceWarningTimeSeconds = 0.0;
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
    bool bAutoBootstrapComplete = false;
    bool bMaterialContractValid = false;
    bool bMaterialContractChecked = false;
    ERshipContentMappingRuntimeHealth RuntimeHealth = ERshipContentMappingRuntimeHealth::Ready;
    FString RuntimeHealthReason;
    double NextMaterialResolveAttemptSeconds = 0.0;
    TWeakObjectPtr<UMaterialInterface> LastContractMaterial;
    FString MaterialContractError;
    float DebugOverlayAccumulated = 0.0f;
    mutable TWeakObjectPtr<UWorld> LastValidWorld;
    double LastPerfLogTimeSeconds = 0.0;
    float LastTickMsTotal = 0.0f;
    float LastTickMsRebuild = 0.0f;
    float LastTickMsRefresh = 0.0f;
    float LastTickMsCacheSave = 0.0f;
    float LastTickMsContextResolve = 0.0f;
    float LastTickMsApplyMappings = 0.0f;
    float LastTickMsConfigHash = 0.0f;
    int32 LastTickEnabledMappings = 0;
    int32 LastTickActiveContexts = 0;
    int32 LastTickAppliedSurfaces = 0;
    int32 LastTickMaterialBindingsUpdated = 0;
    int32 LastTickMaterialBindingsSkipped = 0;
    bool bRuntimePreparePending = true;
    double CacheSaveDueTimeSeconds = 0.0;
    double NextAutoBootstrapAttemptSeconds = 0.0;
    FString CachedEnabledTextureContextId;
    FString CachedAnyTextureContextId;
    FString CachedEnabledContextId;
    FString CachedAnyContextId;
    TMap<FString, uint32> LastEmittedStateHashes;
    TMap<FString, uint32> LastEmittedStatusHashes;

    void MarkMappingsDirty();
    void ArmMappings();
    void MarkCacheDirty();
    bool HasAnyEnabledMappings() const;
    bool HasAnyMappingsRequiringContinuousRefresh() const;
    bool HasPendingRuntimeBindings();
    bool TryAutoBootstrapDefaults();
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
        const FRshipRenderContextState* ContextState,
        uint32 MappingConfigHash = 0,
        double* OutApplyMs = nullptr,
        int32* OutMaterialBindingsUpdated = nullptr,
        int32* OutMaterialBindingsSkipped = nullptr);

    void ApplyMaterialParameters(
        UMaterialInstanceDynamic* MID,
        const FRshipContentMappingState& MappingState,
        const FRshipMappingSurfaceState& SurfaceState,
        const FRshipRenderContextState* ContextState,
        bool bUseFeedV2 = false);

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
    bool PulseEmitterIfChanged(
        const FString& TargetId,
        const FString& EmitterName,
        const TSharedPtr<FJsonObject>& Payload,
        TMap<FString, uint32>& CacheStore);
    void ClearEmitterCacheForTarget(const FString& TargetId);

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
    bool ValidateMaterialContract(UMaterialInterface* Material, FString& OutError) const;
    void EnsureMaterialContract();
    UMaterialInterface* ResolveSurfaceFallbackMaterial(
        const FRshipMappingSurfaceState& SurfaceState,
        UMeshComponent* Mesh,
        FString& OutError) const;
    bool ResolveContentMappingMaterial();
    void RunRuntimePreflight(bool bForceMaterialResolve);
    void SetRuntimeHealth(ERshipContentMappingRuntimeHealth NewHealth, const FString& NewReason);
    bool IsRuntimeBlocked() const;
    FString GetRuntimeHealthStatusToken() const;
    FString GetTargetStatusForEnabledFlag(bool bEnabled) const;
    void ApplyRuntimeHealthToStates(bool bEmitChanges);
    static FString BuildRuntimeBlockedError(const FString& Reason);
    static bool IsRuntimeBlockedError(const FString& ErrorText);
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
