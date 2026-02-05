// Content Mapping Manager implementation

#include "RshipContentMappingManager.h"
#include "RshipSubsystem.h"
#include "RshipSettings.h"
#include "RshipAssetStoreClient.h"
#include "RshipCameraActor.h"
#include "RshipTargetComponent.h"
#include "RshipContentMappingTargetComponent.h"
#include "Logs.h"

#include "Dom/JsonValue.h"
#include "Components/MeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

static const FName ParamContextTexture(TEXT("RshipContextTexture"));
static const FName ParamMappingMode(TEXT("RshipMappingMode"));
static const FName ParamProjectionType(TEXT("RshipProjectionType"));
static const FName ParamProjectorRow0(TEXT("RshipProjectorRow0"));
static const FName ParamProjectorRow1(TEXT("RshipProjectorRow1"));
static const FName ParamProjectorRow2(TEXT("RshipProjectorRow2"));
static const FName ParamProjectorRow3(TEXT("RshipProjectorRow3"));
static const FName ParamUVTransform(TEXT("RshipUVTransform"));
static const FName ParamUVRotation(TEXT("RshipUVRotation"));
static const FName ParamOpacity(TEXT("RshipOpacity"));
static const FName ParamUVChannel(TEXT("RshipUVChannel"));
static const FName ParamPreviewTint(TEXT("RshipPreviewTint"));
static const FName ParamDebugCoverage(TEXT("RshipDebugCoverage"));
static const FName ParamDebugUnmappedColor(TEXT("RshipDebugUnmappedColor"));
static const FName ParamDebugMappedColor(TEXT("RshipDebugMappedColor"));

static FString GetActionName(const FString& ActionId)
{
    int32 Index = INDEX_NONE;
    if (ActionId.FindLastChar(TEXT(':'), Index))
    {
        return ActionId.Mid(Index + 1);
    }
    return ActionId;
}

void URshipContentMappingManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

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

    if (Settings && !Settings->ContentMappingMaterialPath.IsEmpty())
    {
        UMaterialInterface* LoadedMaterial = LoadObject<UMaterialInterface>(nullptr, *Settings->ContentMappingMaterialPath);
        if (LoadedMaterial)
        {
            ContentMappingMaterial = LoadedMaterial;
        }
        else
        {
            UE_LOG(LogRshipExec, Warning, TEXT("ContentMapping material not found: %s"), *Settings->ContentMappingMaterialPath);
        }
    }
    if (!ContentMappingMaterial)
    {
        BuildFallbackMaterial();
    }

    LoadCache();
    MarkMappingsDirty();
}

void URshipContentMappingManager::Shutdown()
{
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
        RestoreSurfaceMaterials(Pair.Value);
    }

    for (auto& Pair : RenderContexts)
    {
        if (Pair.Value.CameraActor.IsValid())
        {
            Pair.Value.CameraActor->Destroy();
        }
    }

    RenderContexts.Empty();
    MappingSurfaces.Empty();
    Mappings.Empty();
    AssetTextureCache.Empty();
    PendingAssetDownloads.Empty();
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

    if (bMappingsDirty)
    {
        RebuildMappings();
        bMappingsDirty = false;
    }

    if (bCacheDirty)
    {
        SaveCache();
        bCacheDirty = false;
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
    FRshipRenderContextState NewState = InState;
    if (NewState.Id.IsEmpty())
    {
        NewState.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    }
    RenderContexts.Add(NewState.Id, NewState);
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
    FRshipRenderContextState& Stored = RenderContexts[InState.Id];
    TWeakObjectPtr<ARshipCameraActor> PreviousCamera = Stored.CameraActor;
    Stored = InState;
    if (PreviousCamera.IsValid())
    {
        if (Stored.SourceType == TEXT("camera"))
        {
            Stored.CameraActor = PreviousCamera;
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
    FRshipMappingSurfaceState NewState = InState;
    if (NewState.Id.IsEmpty())
    {
        NewState.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    }
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
    FRshipMappingSurfaceState& Stored = MappingSurfaces[InState.Id];
    if (Stored.MeshComponent.IsValid())
    {
        RestoreSurfaceMaterials(Stored);
    }
    Stored = InState;
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
    if (Subsystem)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), Id);
        Obj->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->DelItem(TEXT("MappingSurface"), Obj, ERshipMessagePriority::High, Id);
    }
    RestoreSurfaceMaterials(Removed);
    DeleteTargetForPath(BuildSurfaceTargetId(Id));
    MarkMappingsDirty();
    MarkCacheDirty();
    return true;
}

FString URshipContentMappingManager::CreateMapping(const FRshipContentMappingState& InState)
{
    FRshipContentMappingState NewState = InState;
    if (NewState.Id.IsEmpty())
    {
        NewState.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    }
    NewState.Opacity = FMath::Clamp(NewState.Opacity, 0.0f, 1.0f);
    Mappings.Add(NewState.Id, NewState);
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
    FRshipContentMappingState Clamped = InState;
    Clamped.Opacity = FMath::Clamp(Clamped.Opacity, 0.0f, 1.0f);
    Mappings[InState.Id] = Clamped;
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
    if (Subsystem)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), Id);
        Obj->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Subsystem->DelItem(TEXT("Mapping"), Obj, ERshipMessagePriority::High, Id);
    }
    DeleteTargetForPath(BuildMappingTargetId(Id));
    MarkMappingsDirty();
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
    State.Width = GetIntField(Data, TEXT("width"), 0);
    State.Height = GetIntField(Data, TEXT("height"), 0);
    State.CaptureMode = GetStringField(Data, TEXT("captureMode"));
    State.bEnabled = GetBoolField(Data, TEXT("enabled"), true);

    FRshipRenderContextState& Stored = RenderContexts.FindOrAdd(Id);
    TWeakObjectPtr<ARshipCameraActor> PreviousCamera = Stored.CameraActor;
    Stored = State;
    if (PreviousCamera.IsValid())
    {
        if (Stored.SourceType == TEXT("camera"))
        {
            Stored.CameraActor = PreviousCamera;
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
    State.TargetId = GetStringField(Data, TEXT("targetId"));
    State.bEnabled = GetBoolField(Data, TEXT("enabled"), true);
    State.UVChannel = GetIntField(Data, TEXT("uvChannel"), 0);
    State.MaterialSlots = GetIntArrayField(Data, TEXT("materialSlots"));
    State.MeshComponentName = GetStringField(Data, TEXT("meshComponentName"));

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

    if (bIsDelete)
    {
        FRshipContentMappingState Removed;
        if (Mappings.RemoveAndCopyValue(Id, Removed))
        {
            DeleteTargetForPath(BuildMappingTargetId(Id));
            MarkMappingsDirty();
            MarkCacheDirty();
        }
        return;
    }

    const FString MappingType = GetStringField(Data, TEXT("type"));
    if (MappingType != TEXT("surface-uv") && MappingType != TEXT("surface-projection"))
    {
        FRshipContentMappingState Removed;
        if (Mappings.RemoveAndCopyValue(Id, Removed))
        {
            DeleteTargetForPath(BuildMappingTargetId(Id));
            MarkMappingsDirty();
            MarkCacheDirty();
        }
        return;
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

    FRshipContentMappingState& Stored = Mappings.FindOrAdd(Id);
    Stored = State;

    RegisterMappingTarget(Stored);
    EmitMappingState(Stored);
    MarkMappingsDirty();
    MarkCacheDirty();
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
}

void URshipContentMappingManager::MarkCacheDirty()
{
    bCacheDirty = true;
}

UWorld* URshipContentMappingManager::GetBestWorld() const
{
    if (LastValidWorld.IsValid())
    {
        return LastValidWorld.Get();
    }

    if (Subsystem)
    {
        if (UWorld* SubsystemWorld = Subsystem->GetWorld())
        {
            LastValidWorld = SubsystemWorld;
            return SubsystemWorld;
        }
    }

    if (!GEngine)
    {
        return nullptr;
    }

    const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();
    for (const FWorldContext& Context : Contexts)
    {
        UWorld* World = Context.World();
        if (!World)
        {
            continue;
        }

        if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
        {
            LastValidWorld = World;
            return World;
        }
    }

    for (const FWorldContext& Context : Contexts)
    {
        UWorld* World = Context.World();
        if (!World)
        {
            continue;
        }

        if (Context.WorldType == EWorldType::Editor || Context.WorldType == EWorldType::EditorPreview)
        {
            LastValidWorld = World;
            return World;
        }
    }

    for (const FWorldContext& Context : Contexts)
    {
        if (UWorld* World = Context.World())
        {
            LastValidWorld = World;
            return World;
        }
    }

    return nullptr;
}

void URshipContentMappingManager::ResolveRenderContext(FRshipRenderContextState& ContextState)
{
    ContextState.LastError.Empty();
    ContextState.ResolvedTexture = nullptr;

    if (!ContextState.bEnabled)
    {
        if (ARshipCameraActor* CameraActor = ContextState.CameraActor.Get())
        {
            CameraActor->bEnableSceneCapture = false;
            if (CameraActor->SceneCapture)
            {
                CameraActor->SceneCapture->bCaptureEveryFrame = false;
            }
        }
        return;
    }

    if (ContextState.SourceType == TEXT("camera"))
    {
        if (ContextState.CameraId.IsEmpty())
        {
            ContextState.LastError = TEXT("CameraId not set");
            return;
        }

        UWorld* World = nullptr;
        if (ARshipCameraActor* ExistingCamera = ContextState.CameraActor.Get())
        {
            World = ExistingCamera->GetWorld();
        }
        if (!World)
        {
            World = GetBestWorld();
        }
        if (!World)
        {
            ContextState.LastError = TEXT("World not available");
            return;
        }

        ARshipCameraActor* CameraActor = ContextState.CameraActor.Get();
        if (!CameraActor)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = FName(*FString::Printf(TEXT("RshipContentMappingCam_%s"), *ContextState.Id));
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            CameraActor = World->SpawnActor<ARshipCameraActor>(SpawnParams);
            if (CameraActor)
            {
                CameraActor->SetActorHiddenInGame(true);
            }
        }

        if (!CameraActor)
        {
            ContextState.LastError = TEXT("Failed to spawn camera actor");
            return;
        }

        CameraActor->CameraId = ContextState.CameraId;
        CameraActor->bEnableSceneCapture = true;
        CameraActor->bShowFrustumVisualization = false;

        if (CameraActor->SceneCapture)
        {
            CameraActor->SceneCapture->bCaptureEveryFrame = true;
            CameraActor->SceneCapture->bCaptureOnMovement = false;

            if (ContextState.CaptureMode == TEXT("SceneColorHDR"))
            {
                CameraActor->SceneCapture->CaptureSource = SCS_SceneColorHDR;
            }
            else if (ContextState.CaptureMode == TEXT("RawSceneColor"))
            {
                CameraActor->SceneCapture->CaptureSource = SCS_SceneColorHDR;
            }
            else
            {
                CameraActor->SceneCapture->CaptureSource = SCS_FinalColorLDR;
            }
        }

        if (CameraActor->CaptureRenderTarget)
        {
            const int32 Width = ContextState.Width > 0 ? ContextState.Width : CameraActor->CaptureRenderTarget->SizeX;
            const int32 Height = ContextState.Height > 0 ? ContextState.Height : CameraActor->CaptureRenderTarget->SizeY;
            if (CameraActor->CaptureRenderTarget->SizeX != Width || CameraActor->CaptureRenderTarget->SizeY != Height)
            {
                CameraActor->CaptureRenderTarget->InitAutoFormat(Width, Height);
                CameraActor->CaptureRenderTarget->UpdateResourceImmediate();
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
        }

        ContextState.CameraActor = CameraActor;
        ContextState.ResolvedTexture = CameraActor->CaptureRenderTarget;
        return;
    }

    if (ContextState.SourceType == TEXT("asset-store"))
    {
        if (ContextState.AssetId.IsEmpty())
        {
            ContextState.LastError = TEXT("AssetId not set");
            return;
        }

        if (TWeakObjectPtr<UTexture2D>* Cached = AssetTextureCache.Find(ContextState.AssetId))
        {
            if (Cached->IsValid())
            {
                ContextState.ResolvedTexture = Cached->Get();
                return;
            }
        }

        const FString CachedPath = GetAssetCachePathForId(ContextState.AssetId);
        if (IFileManager::Get().FileExists(*CachedPath))
        {
            UTexture2D* CachedTexture = LoadTextureFromFile(CachedPath);
            if (CachedTexture)
            {
                AssetTextureCache.Add(ContextState.AssetId, CachedTexture);
                ContextState.ResolvedTexture = CachedTexture;
                return;
            }
        }

        RequestAssetDownload(ContextState.AssetId);
        ContextState.LastError = TEXT("Asset downloading");
        return;
    }

    ContextState.LastError = TEXT("Unsupported sourceType");
}

void URshipContentMappingManager::ResolveMappingSurface(FRshipMappingSurfaceState& SurfaceState)
{
    SurfaceState.LastError.Empty();

    if (!Subsystem)
    {
        SurfaceState.LastError = TEXT("Subsystem not ready");
        return;
    }

    if (SurfaceState.TargetId.IsEmpty())
    {
        SurfaceState.LastError = TEXT("TargetId not set");
        return;
    }

    FString TargetId = SurfaceState.TargetId.TrimStartAndEnd();
    if (!TargetId.Contains(TEXT(":")))
    {
        const FString ServiceId = Subsystem->GetServiceId();
        if (!ServiceId.IsEmpty())
        {
            TargetId = ServiceId + TEXT(":") + TargetId;
        }
    }

    URshipTargetComponent* TargetComponent = Subsystem->FindTargetComponent(TargetId);
    if (!TargetComponent)
    {
        SurfaceState.LastError = TEXT("Target component not found");
        return;
    }

    SurfaceState.TargetId = TargetId;

    AActor* Owner = TargetComponent->GetOwner();
    if (!Owner)
    {
        SurfaceState.LastError = TEXT("Target actor not found");
        return;
    }

    FString MeshNameOverride;
    TArray<int32> SlotOverride;
    int32 UVOverride = -1;

    if (URshipContentMappingTargetComponent* OverrideComp = Owner->FindComponentByClass<URshipContentMappingTargetComponent>())
    {
        MeshNameOverride = OverrideComp->MeshComponentNameOverride;
        SlotOverride = OverrideComp->MaterialSlotsOverride;
        UVOverride = OverrideComp->UVChannelOverride;
    }

    const FString DesiredMeshName = !MeshNameOverride.IsEmpty() ? MeshNameOverride : SurfaceState.MeshComponentName;

    TArray<UMeshComponent*> MeshComponents;
    Owner->GetComponents(MeshComponents);

    UMeshComponent* SelectedMesh = nullptr;
    if (!DesiredMeshName.IsEmpty())
    {
        for (UMeshComponent* Mesh : MeshComponents)
        {
            if (Mesh && Mesh->GetName() == DesiredMeshName)
            {
                SelectedMesh = Mesh;
                break;
            }
        }
    }

    if (!SelectedMesh && MeshComponents.Num() > 0)
    {
        SelectedMesh = MeshComponents[0];
    }

    if (!SelectedMesh)
    {
        SurfaceState.LastError = TEXT("No mesh component found");
        return;
    }

    SurfaceState.MeshComponent = SelectedMesh;

    if (UVOverride >= 0)
    {
        SurfaceState.UVChannel = UVOverride;
    }

    if (SlotOverride.Num() > 0)
    {
        SurfaceState.MaterialSlots = SlotOverride;
    }

    if (SurfaceState.MaterialSlots.Num() == 0)
    {
        const int32 SlotCount = SelectedMesh->GetNumMaterials();
        for (int32 Slot = 0; Slot < SlotCount; ++Slot)
        {
            SurfaceState.MaterialSlots.Add(Slot);
        }
    }
}

void URshipContentMappingManager::RebuildMappings()
{
    for (auto& Pair : MappingSurfaces)
    {
        RestoreSurfaceMaterials(Pair.Value);
        ResolveMappingSurface(Pair.Value);
    }

    for (auto& Pair : RenderContexts)
    {
        ResolveRenderContext(Pair.Value);
    }

    for (auto& MappingPair : Mappings)
    {
        FRshipContentMappingState& MappingState = MappingPair.Value;
        MappingState.LastError.Empty();

        if (!MappingState.bEnabled)
        {
            continue;
        }

        const FRshipRenderContextState* ContextState = nullptr;
        if (!MappingState.ContextId.IsEmpty())
        {
            ContextState = RenderContexts.Find(MappingState.ContextId);
            if (!ContextState)
            {
                MappingState.LastError = TEXT("Render context not found");
            }
        }
        else
        {
            MappingState.LastError = TEXT("Render context not set");
        }

        if (MappingState.SurfaceIds.Num() == 0 && MappingState.LastError.IsEmpty())
        {
            MappingState.LastError = TEXT("No mapping surfaces assigned");
        }

        for (const FString& SurfaceId : MappingState.SurfaceIds)
        {
            FRshipMappingSurfaceState* SurfaceState = MappingSurfaces.Find(SurfaceId);
            if (SurfaceState && SurfaceState->bEnabled)
            {
                ApplyMappingToSurface(MappingState, *SurfaceState, ContextState);
            }
            else if (MappingState.LastError.IsEmpty())
            {
                MappingState.LastError = TEXT("Mapping surface not found");
            }
        }

        EmitMappingState(MappingState);
    }
}

void URshipContentMappingManager::RestoreSurfaceMaterials(FRshipMappingSurfaceState& SurfaceState)
{
    UMeshComponent* Mesh = SurfaceState.MeshComponent.Get();
    if (!Mesh)
    {
        SurfaceState.MaterialInstances.Empty();
        SurfaceState.OriginalMaterials.Empty();
        return;
    }

    for (const auto& Pair : SurfaceState.OriginalMaterials)
    {
        if (Pair.Value)
        {
            Mesh->SetMaterial(Pair.Key, Pair.Value);
        }
    }

    SurfaceState.MaterialInstances.Empty();
}

void URshipContentMappingManager::ApplyMappingToSurface(
    const FRshipContentMappingState& MappingState,
    FRshipMappingSurfaceState& SurfaceState,
    const FRshipRenderContextState* ContextState)
{
    UMeshComponent* Mesh = SurfaceState.MeshComponent.Get();
    if (!Mesh)
    {
        SurfaceState.LastError = TEXT("Mesh component not resolved");
        return;
    }

    UMaterialInterface* BaseMaterial = ContentMappingMaterial;
    if (!BaseMaterial)
    {
        // Fall back to existing material slot
        BaseMaterial = Mesh->GetMaterial(0);
    }

    const int32 SlotCount = Mesh->GetNumMaterials();
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
            UMaterialInterface* SlotBase = BaseMaterial ? BaseMaterial : Mesh->GetMaterial(SlotIndex);
            MID = UMaterialInstanceDynamic::Create(SlotBase, Mesh);
            SurfaceState.MaterialInstances.Add(SlotIndex, MID);
            Mesh->SetMaterial(SlotIndex, MID);
        }

        ApplyMaterialParameters(MID, MappingState, SurfaceState, ContextState);

        // Give mappings a slight tint to visualize assignment if no texture
        if (MID)
        {
            if (!ContextState)
            {
                MID->SetVectorParameterValue(ParamPreviewTint, FLinearColor(0.0f, 1.0f, 1.0f, 1.0f));
            }
            else if (!ContextState->ResolvedTexture)
            {
                MID->SetVectorParameterValue(ParamPreviewTint, FLinearColor(1.0f, 0.8f, 0.2f, 1.0f));
            }
        }
    }
}

void URshipContentMappingManager::ApplyMaterialParameters(
    UMaterialInstanceDynamic* MID,
    const FRshipContentMappingState& MappingState,
    const FRshipMappingSurfaceState& SurfaceState,
    const FRshipRenderContextState* ContextState)
{
    if (!MID)
    {
        return;
    }

    const float Opacity = MappingState.bEnabled ? MappingState.Opacity : 0.0f;
    MID->SetScalarParameterValue(ParamOpacity, Opacity);
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

    if (ContextState && ContextState->ResolvedTexture)
    {
        MID->SetTextureParameterValue(ParamContextTexture, ContextState->ResolvedTexture);
    }
    else
    {
        MID->SetTextureParameterValue(ParamContextTexture, nullptr);
    }

    if (MappingState.Type == TEXT("surface-uv"))
    {
        MID->SetScalarParameterValue(ParamMappingMode, 0.0f);
        MID->SetScalarParameterValue(ParamProjectionType, 0.0f);

        float ScaleU = 1.0f;
        float ScaleV = 1.0f;
        float OffsetU = 0.0f;
        float OffsetV = 0.0f;
        float Rotation = 0.0f;
        float PivotU = 0.5f;
        float PivotV = 0.5f;

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
        }

        OffsetU = OffsetU - PivotU + 0.5f;
        OffsetV = OffsetV - PivotV + 0.5f;

        MID->SetVectorParameterValue(
            ParamUVTransform,
            FLinearColor(ScaleU, ScaleV, OffsetU, OffsetV));
        MID->SetScalarParameterValue(ParamUVRotation, Rotation);
        return;
    }

    if (MappingState.Type == TEXT("surface-projection"))
    {
        MID->SetScalarParameterValue(ParamMappingMode, 1.0f);

        FString ProjectionType = TEXT("perspective");
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

        float ProjectionTypeIndex = 0.0f;
        if (ProjectionType == TEXT("cylindrical"))
        {
            ProjectionTypeIndex = 1.0f;
        }
        else if (ProjectionType == TEXT("planar"))
        {
            ProjectionTypeIndex = 2.0f;
        }

        MID->SetScalarParameterValue(ParamProjectionType, ProjectionTypeIndex);

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

        FMatrix Projection = FMatrix::Identity;
        Projection.M[0][0] = 1.0f / (TanHalfFov * SafeAspect);
        Projection.M[1][1] = 1.0f / TanHalfFov;
        Projection.M[2][2] = SafeFar / (SafeFar - SafeNear);
        Projection.M[2][3] = 1.0f;
        Projection.M[3][2] = (-SafeNear * SafeFar) / (SafeFar - SafeNear);
        Projection.M[3][3] = 0.0f;

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
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setTargetId")));
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
    RegisterAction(TEXT("setTargetId"));
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
    Json->SetBoolField(TEXT("enabled"), ContextState.bEnabled);
    Json->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    return Json;
}

TSharedPtr<FJsonObject> URshipContentMappingManager::BuildMappingSurfaceJson(const FRshipMappingSurfaceState& SurfaceState) const
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("id"), SurfaceState.Id);
    Json->SetStringField(TEXT("name"), SurfaceState.Name);
    Json->SetStringField(TEXT("projectId"), SurfaceState.ProjectId);
    Json->SetStringField(TEXT("targetId"), SurfaceState.TargetId);
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
    Json->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    return Json;
}

TSharedPtr<FJsonObject> URshipContentMappingManager::BuildMappingJson(const FRshipContentMappingState& MappingState) const
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("id"), MappingState.Id);
    Json->SetStringField(TEXT("name"), MappingState.Name);
    Json->SetStringField(TEXT("projectId"), MappingState.ProjectId);
    Json->SetStringField(TEXT("type"), MappingState.Type);
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
    else if (ActionName == TEXT("setTargetId"))
    {
        SurfaceState->TargetId = GetStringField(Data, TEXT("targetId"), SurfaceState->TargetId);
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
        }
    }
    else if (ActionName == TEXT("setUVTransform"))
    {
        if (!MappingState->Config.IsValid())
        {
            MappingState->Config = MakeShared<FJsonObject>();
        }
        if (Data->HasTypedField<EJson::Object>(TEXT("uvTransform")))
        {
            MappingState->Config->SetObjectField(TEXT("uvTransform"), Data->GetObjectField(TEXT("uvTransform")));
        }
    }
    else
    {
        bHandled = false;
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

void URshipContentMappingManager::BuildFallbackMaterial()
{
    ContentMappingMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
    UE_LOG(LogRshipExec, Warning, TEXT("ContentMapping material missing; using default material as fallback."));
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
            if (Pair.Value.AssetId == AssetId)
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
        if (Pair.Value.AssetId == AssetId)
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
