// Rship Editor Transform Sync Implementation

#include "RshipEditorTransformSync.h"
#include "RshipSubsystem.h"
#include "RshipSceneConverter.h"
#include "RshipFixtureManager.h"
#include "RshipCameraManager.h"
#include "Logs.h"
#include "Engine/World.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditor.h"
#include "Selection.h"
#endif

void URshipEditorTransformSync::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

#if WITH_EDITOR
    BindEditorEvents();
#endif

    UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync initialized"));
}

void URshipEditorTransformSync::Shutdown()
{
#if WITH_EDITOR
    UnbindEditorEvents();
#endif

    // Clear periodic timer
    if (Subsystem)
    {
        if (UWorld* World = Subsystem->GetWorld())
        {
            World->GetTimerManager().ClearTimer(PeriodicSyncTimerHandle);
        }
    }

    TrackedActors.Empty();
    Subsystem = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync shutdown"));
}

// ============================================================================
// SYNC CONFIGURATION
// ============================================================================

void URshipEditorTransformSync::SetSyncMode(ERshipTransformSyncMode Mode)
{
    if (SyncMode == Mode)
    {
        return;
    }

    ERshipTransformSyncMode OldMode = SyncMode;
    SyncMode = Mode;

    // Handle periodic timer
    if (Subsystem)
    {
        if (UWorld* World = Subsystem->GetWorld())
        {
            if (Mode == ERshipTransformSyncMode::Periodic)
            {
                // Start periodic timer
                World->GetTimerManager().SetTimer(
                    PeriodicSyncTimerHandle,
                    this,
                    &URshipEditorTransformSync::OnPeriodicSyncTimer,
                    SyncIntervalSeconds,
                    true
                );
            }
            else if (OldMode == ERshipTransformSyncMode::Periodic)
            {
                // Stop periodic timer
                World->GetTimerManager().ClearTimer(PeriodicSyncTimerHandle);
            }
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync: Sync mode set to %d"), (int32)Mode);
}

void URshipEditorTransformSync::SetSyncInterval(float IntervalSeconds)
{
    SyncIntervalSeconds = FMath::Max(0.1f, IntervalSeconds);

    // Update timer if periodic mode is active
    if (SyncMode == ERshipTransformSyncMode::Periodic && Subsystem)
    {
        if (UWorld* World = Subsystem->GetWorld())
        {
            World->GetTimerManager().ClearTimer(PeriodicSyncTimerHandle);
            World->GetTimerManager().SetTimer(
                PeriodicSyncTimerHandle,
                this,
                &URshipEditorTransformSync::OnPeriodicSyncTimer,
                SyncIntervalSeconds,
                true
            );
        }
    }
}

// ============================================================================
// ACTOR TRACKING
// ============================================================================

void URshipEditorTransformSync::TrackActor(AActor* Actor, const FString& EntityId, bool bIsFixture)
{
    if (!Actor || EntityId.IsEmpty())
    {
        return;
    }

    FRshipTrackedActor TrackedInfo;
    TrackedInfo.Actor = Actor;
    TrackedInfo.EntityId = EntityId;
    TrackedInfo.LastSyncedPosition = Actor->GetActorLocation();
    TrackedInfo.LastSyncedRotation = Actor->GetActorRotation();
    TrackedInfo.bHasPendingChanges = false;
    TrackedInfo.bIsFixture = bIsFixture;

    TrackedActors.Add(Actor, TrackedInfo);

    UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync: Now tracking actor '%s' -> entity '%s'"),
        *Actor->GetName(), *EntityId);
}

void URshipEditorTransformSync::UntrackActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    if (TrackedActors.Remove(Actor) > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync: Stopped tracking actor '%s'"),
            *Actor->GetName());
    }
}

void URshipEditorTransformSync::UntrackAllActors()
{
    int32 Count = TrackedActors.Num();
    TrackedActors.Empty();

    UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync: Stopped tracking %d actors"), Count);
}

TArray<FRshipTrackedActor> URshipEditorTransformSync::GetTrackedActors() const
{
    TArray<FRshipTrackedActor> Result;
    for (const auto& Pair : TrackedActors)
    {
        if (Pair.Key.IsValid())
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

bool URshipEditorTransformSync::IsActorTracked(AActor* Actor) const
{
    return Actor && TrackedActors.Contains(Actor);
}

// ============================================================================
// MANUAL SYNC
// ============================================================================

int32 URshipEditorTransformSync::SyncAllPendingChanges()
{
    int32 SyncCount = 0;

    for (auto& Pair : TrackedActors)
    {
        if (Pair.Value.bHasPendingChanges && Pair.Key.IsValid())
        {
            if (SyncActorInternal(Pair.Value))
            {
                SyncCount++;
            }
        }
    }

    if (SyncCount > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync: Synced %d actors"), SyncCount);
        OnTransformsSynced.Broadcast(SyncCount);
    }

    return SyncCount;
}

bool URshipEditorTransformSync::SyncActor(AActor* Actor)
{
    if (!Actor)
    {
        return false;
    }

    FRshipTrackedActor* TrackedInfo = TrackedActors.Find(Actor);
    if (!TrackedInfo)
    {
        return false;
    }

    return SyncActorInternal(*TrackedInfo);
}

int32 URshipEditorTransformSync::CheckForChanges()
{
    int32 ChangeCount = 0;

    for (auto& Pair : TrackedActors)
    {
        if (Pair.Key.IsValid())
        {
            CheckActorForChanges(Pair.Value);
            if (Pair.Value.bHasPendingChanges)
            {
                ChangeCount++;
            }
        }
    }

    return ChangeCount;
}

int32 URshipEditorTransformSync::GetPendingChangeCount() const
{
    int32 Count = 0;
    for (const auto& Pair : TrackedActors)
    {
        if (Pair.Value.bHasPendingChanges)
        {
            Count++;
        }
    }
    return Count;
}

// ============================================================================
// AUTO-TRACK FROM SCENE CONVERTER
// ============================================================================

int32 URshipEditorTransformSync::TrackConvertedActors()
{
    if (!Subsystem)
    {
        return 0;
    }

    URshipSceneConverter* Converter = Subsystem->GetSceneConverter();
    if (!Converter)
    {
        return 0;
    }

    int32 TrackCount = 0;

    // Get discovered lights and cameras
    TArray<FRshipDiscoveredLight> Lights = Converter->GetDiscoveredLights();
    TArray<FRshipDiscoveredCamera> Cameras = Converter->GetDiscoveredCameras();

    // Track converted lights
    for (const FRshipDiscoveredLight& Light : Lights)
    {
        if (Light.bAlreadyConverted && Light.OwnerActor && !Light.ExistingFixtureId.IsEmpty())
        {
            if (!IsActorTracked(Light.OwnerActor))
            {
                TrackActor(Light.OwnerActor, Light.ExistingFixtureId, true);
                TrackCount++;
            }
        }
    }

    // Track converted cameras
    for (const FRshipDiscoveredCamera& Camera : Cameras)
    {
        if (Camera.bAlreadyConverted && Camera.CameraActor && !Camera.ExistingCameraId.IsEmpty())
        {
            if (!IsActorTracked(Camera.CameraActor))
            {
                TrackActor(Camera.CameraActor, Camera.ExistingCameraId, false);
                TrackCount++;
            }
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync: Auto-tracked %d converted actors"), TrackCount);

    return TrackCount;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

bool URshipEditorTransformSync::SyncActorInternal(FRshipTrackedActor& TrackedInfo)
{
    if (!Subsystem || !TrackedInfo.Actor)
    {
        return false;
    }

    AActor* Actor = TrackedInfo.Actor;
    FVector Position = Actor->GetActorLocation() * PositionScale;
    FRotator Rotation = Actor->GetActorRotation();

    bool bSuccess = false;

    if (TrackedInfo.bIsFixture)
    {
        URshipFixtureManager* FixtureManager = Subsystem->GetFixtureManager();
        if (FixtureManager)
        {
            bSuccess = FixtureManager->UpdateFixturePosition(TrackedInfo.EntityId, Position, Rotation);
        }
    }
    else
    {
        URshipCameraManager* CameraManager = Subsystem->GetCameraManager();
        if (CameraManager)
        {
            bSuccess = CameraManager->UpdateCameraPosition(TrackedInfo.EntityId, Position, Rotation);
        }
    }

    if (bSuccess)
    {
        TrackedInfo.LastSyncedPosition = Actor->GetActorLocation();
        TrackedInfo.LastSyncedRotation = Actor->GetActorRotation();
        TrackedInfo.bHasPendingChanges = false;

        UE_LOG(LogRshipExec, Verbose, TEXT("EditorTransformSync: Synced '%s' to entity '%s'"),
            *Actor->GetName(), *TrackedInfo.EntityId);
    }

    return bSuccess;
}

void URshipEditorTransformSync::OnPeriodicSyncTimer()
{
    // Check for changes and sync
    CheckForChanges();
    SyncAllPendingChanges();
}

void URshipEditorTransformSync::CheckActorForChanges(FRshipTrackedActor& TrackedInfo)
{
    if (!TrackedInfo.Actor)
    {
        return;
    }

    if (HasActorMoved(TrackedInfo))
    {
        if (!TrackedInfo.bHasPendingChanges)
        {
            TrackedInfo.bHasPendingChanges = true;
            OnActorTransformChanged.Broadcast(TrackedInfo.Actor, TrackedInfo.EntityId);
        }
    }
}

bool URshipEditorTransformSync::HasActorMoved(const FRshipTrackedActor& TrackedInfo) const
{
    if (!TrackedInfo.Actor)
    {
        return false;
    }

    AActor* Actor = TrackedInfo.Actor;
    FVector CurrentPos = Actor->GetActorLocation();
    FRotator CurrentRot = Actor->GetActorRotation();

    float PosDelta = FVector::DistSquared(CurrentPos, TrackedInfo.LastSyncedPosition);
    float RotDelta = FMath::Abs((CurrentRot - TrackedInfo.LastSyncedRotation).GetNormalized().GetManhattanDistance(FRotator::ZeroRotator));

    // Check if position or rotation changed beyond threshold
    return (PosDelta > MovementThreshold * MovementThreshold) || (RotDelta > 0.1f);
}

// ============================================================================
// EDITOR CALLBACKS (WITH_EDITOR only)
// ============================================================================

#if WITH_EDITOR
void URshipEditorTransformSync::BindEditorEvents()
{
    if (!GEditor)
    {
        return;
    }

    // Bind to actor moved event
    ActorMovedHandle = GEditor->OnActorMoved().AddUObject(this, &URshipEditorTransformSync::OnActorMoved);

    // Bind to selection changed
    SelectionChangedHandle = USelection::SelectionChangedEvent.AddUObject(this, &URshipEditorTransformSync::OnEditorSelectionChanged);

    UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync: Bound to editor events"));
}

void URshipEditorTransformSync::UnbindEditorEvents()
{
    if (GEditor && ActorMovedHandle.IsValid())
    {
        GEditor->OnActorMoved().Remove(ActorMovedHandle);
    }
    ActorMovedHandle.Reset();

    if (SelectionChangedHandle.IsValid())
    {
        USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
    }
    SelectionChangedHandle.Reset();
}

void URshipEditorTransformSync::OnActorMoved(AActor* Actor)
{
    if (!Actor || SyncMode == ERshipTransformSyncMode::Disabled)
    {
        return;
    }

    FRshipTrackedActor* TrackedInfo = TrackedActors.Find(Actor);
    if (!TrackedInfo)
    {
        return;
    }

    // Check if actually moved
    if (!HasActorMoved(*TrackedInfo))
    {
        return;
    }

    TrackedInfo->bHasPendingChanges = true;
    OnActorTransformChanged.Broadcast(Actor, TrackedInfo->EntityId);

    // In real-time mode, sync immediately
    if (SyncMode == ERshipTransformSyncMode::RealTime)
    {
        SyncActorInternal(*TrackedInfo);
        OnTransformsSynced.Broadcast(1);
    }
}

void URshipEditorTransformSync::OnEditorSelectionChanged(UObject* Object)
{
    if (SyncMode != ERshipTransformSyncMode::OnDeselect)
    {
        return;
    }

    // Get currently selected actors
    TSet<TWeakObjectPtr<AActor>> CurrentlySelectedActors;

    if (GEditor)
    {
        USelection* Selection = GEditor->GetSelectedActors();
        if (Selection)
        {
            for (FSelectionIterator It(*Selection); It; ++It)
            {
                if (AActor* Actor = Cast<AActor>(*It))
                {
                    CurrentlySelectedActors.Add(Actor);
                }
            }
        }
    }

    // Find actors that were deselected
    TArray<AActor*> DeselectedActors;
    for (const TWeakObjectPtr<AActor>& PrevActor : PreviouslySelectedActors)
    {
        if (PrevActor.IsValid() && !CurrentlySelectedActors.Contains(PrevActor))
        {
            DeselectedActors.Add(PrevActor.Get());
        }
    }

    // Sync deselected tracked actors that have pending changes
    int32 SyncCount = 0;
    for (AActor* Actor : DeselectedActors)
    {
        FRshipTrackedActor* TrackedInfo = TrackedActors.Find(Actor);
        if (TrackedInfo && TrackedInfo->bHasPendingChanges)
        {
            if (SyncActorInternal(*TrackedInfo))
            {
                SyncCount++;
            }
        }
    }

    if (SyncCount > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync: Synced %d actors on deselect"), SyncCount);
        OnTransformsSynced.Broadcast(SyncCount);
    }

    // Update previous selection
    PreviouslySelectedActors = CurrentlySelectedActors;
}
#endif
