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
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture;
class UTexture2D;

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
    int32 Width = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    int32 Height = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    FString CaptureMode;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|ContentMapping")
    bool bEnabled = true;

    UPROPERTY(Transient)
    TWeakObjectPtr<ARshipCameraActor> CameraActor;

    UPROPERTY(Transient)
    UTexture* ResolvedTexture = nullptr;

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
    FString LastError;

    UPROPERTY(Transient)
    TWeakObjectPtr<UMeshComponent> MeshComponent;

    UPROPERTY(Transient)
    TMap<int32, UMaterialInterface*> OriginalMaterials;

    UPROPERTY(Transient)
    TMap<int32, UMaterialInstanceDynamic*> MaterialInstances;
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

private:
    UPROPERTY()
    URshipSubsystem* Subsystem = nullptr;

    UPROPERTY()
    URshipAssetStoreClient* AssetStoreClient = nullptr;

    UPROPERTY(Transient)
    UMaterialInterface* ContentMappingMaterial = nullptr;

    TMap<FString, FRshipRenderContextState> RenderContexts;
    TMap<FString, FRshipMappingSurfaceState> MappingSurfaces;
    TMap<FString, FRshipContentMappingState> Mappings;

    TMap<FString, TWeakObjectPtr<UTexture2D>> AssetTextureCache;
    TSet<FString> PendingAssetDownloads;

    bool bMappingsDirty = false;
    bool bCacheDirty = false;
    bool bWasConnected = false;
    bool bDebugOverlayEnabled = false;
    float DebugOverlayAccumulated = 0.0f;

    void MarkMappingsDirty();
    void MarkCacheDirty();
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
        const FRshipRenderContextState* ContextState);

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

    static FString GetStringField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, const FString& DefaultValue = TEXT(""));
    static bool GetBoolField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, bool DefaultValue);
    static int32 GetIntField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32 DefaultValue);
    static float GetNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, float DefaultValue);
    static TArray<FString> GetStringArrayField(const TSharedPtr<FJsonObject>& Obj, const FString& Field);
    static TArray<int32> GetIntArrayField(const TSharedPtr<FJsonObject>& Obj, const FString& Field);
};
