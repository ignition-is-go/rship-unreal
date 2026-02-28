// Rship Editor Transform Sync
// Watches for actor movements in the editor and syncs positions to rship server

#pragma once

#include "CoreMinimal.h"
#include "RshipEditorTransformSync.generated.h"

class URshipSubsystem;
class URshipSceneConverter;

/**
 * Transform sync mode for converted actors
 */
UENUM(BlueprintType)
enum class ERshipTransformSyncMode : uint8
{
    /** No automatic sync - manual only */
    Disabled,

    /** Sync on actor selection change (batch sync when deselected) */
    OnDeselect,

    /** Real-time sync as actors are moved (higher network traffic) */
    RealTime,

    /** Periodic sync at configurable interval */
    Periodic
};

/**
 * Info about a tracked actor for transform sync
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipTrackedActor
{
    GENERATED_BODY()

    /** Reference to the actor */
    UPROPERTY()
    TObjectPtr<AActor> Actor;

    /** The rship entity ID (fixture or camera) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|TransformSync")
    FString EntityId;

    /** Last synced position */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|TransformSync")
    FVector LastSyncedPosition = FVector::ZeroVector;

    /** Last synced rotation */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|TransformSync")
    FRotator LastSyncedRotation = FRotator::ZeroRotator;

    /** Whether this actor has pending changes to sync */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|TransformSync")
    bool bHasPendingChanges = false;

    /** Is this a fixture (vs camera) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|TransformSync")
    bool bIsFixture = true;
};

/**
 * Delegate fired when transforms are synced
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransformsSynced, int32, SyncCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActorTransformChanged, AActor*, Actor, const FString&, EntityId);

/**
 * Manages automatic synchronization of actor transforms to rship server.
 * Watches for editor movements of converted actors and syncs position/rotation changes.
 * Only functional in Editor builds.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipEditorTransformSync : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize with reference to subsystem */
    void Initialize(URshipSubsystem* InSubsystem);

    /** Shutdown and stop tracking */
    void Shutdown();

    // ========================================================================
    // SYNC CONFIGURATION
    // ========================================================================

    /** Set the transform sync mode */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    void SetSyncMode(ERshipTransformSyncMode Mode);

    /** Get the current sync mode */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    ERshipTransformSyncMode GetSyncMode() const { return SyncMode; }

    /** Set the periodic sync interval (only used in Periodic mode) */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    void SetSyncInterval(float IntervalSeconds);

    /** Get the sync interval */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    float GetSyncInterval() const { return SyncIntervalSeconds; }

    /** Set the position scale factor (UE units to rship meters) */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    void SetPositionScale(float Scale) { PositionScale = Scale; }

    /** Get the position scale factor */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    float GetPositionScale() const { return PositionScale; }

    /** Set the minimum movement threshold for sync (in UE units) */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    void SetMovementThreshold(float Threshold) { MovementThreshold = Threshold; }

    // ========================================================================
    // ACTOR TRACKING
    // ========================================================================

    /**
     * Start tracking an actor for transform sync
     * @param Actor The actor to track
     * @param EntityId The rship entity ID (fixture or camera)
     * @param bIsFixture Whether this is a fixture (vs camera)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    void TrackActor(AActor* Actor, const FString& EntityId, bool bIsFixture = true);

    /**
     * Stop tracking an actor
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    void UntrackActor(AActor* Actor);

    /**
     * Stop tracking all actors
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    void UntrackAllActors();

    /**
     * Get all tracked actors
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    TArray<FRshipTrackedActor> GetTrackedActors() const;

    /**
     * Check if an actor is being tracked
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    bool IsActorTracked(AActor* Actor) const;

    /**
     * Get number of tracked actors
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    int32 GetTrackedActorCount() const { return TrackedActors.Num(); }

    // ========================================================================
    // MANUAL SYNC
    // ========================================================================

    /**
     * Manually sync all pending changes now
     * @return Number of actors synced
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    int32 SyncAllPendingChanges();

    /**
     * Manually sync a specific actor
     * @return Whether sync was performed
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    bool SyncActor(AActor* Actor);

    /**
     * Check all tracked actors for changes and mark pending
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    int32 CheckForChanges();

    /**
     * Get number of actors with pending changes
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    int32 GetPendingChangeCount() const;

    // ========================================================================
    // AUTO-TRACK FROM SCENE CONVERTER
    // ========================================================================

    /**
     * Automatically track all actors that have been converted via SceneConverter
     * Call this after running scene conversion to enable auto-sync
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|TransformSync")
    int32 TrackConvertedActors();

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when transforms are synced to server */
    UPROPERTY(BlueprintAssignable, Category = "Rship|TransformSync")
    FOnTransformsSynced OnTransformsSynced;

    /** Fired when a tracked actor's transform changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|TransformSync")
    FOnActorTransformChanged OnActorTransformChanged;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Sync configuration
    ERshipTransformSyncMode SyncMode = ERshipTransformSyncMode::Disabled;
    float SyncIntervalSeconds = 1.0f;
    float PositionScale = 0.01f;  // UE cm to meters
    float MovementThreshold = 1.0f;  // 1 cm minimum movement

    // Tracked actors
    TMap<TWeakObjectPtr<AActor>, FRshipTrackedActor> TrackedActors;

    // Timer for periodic sync
    FTimerHandle PeriodicSyncTimerHandle;

#if WITH_EDITOR
    // Editor callbacks
    void OnActorMoved(AActor* Actor);
    void OnEditorSelectionChanged(UObject* Object);

    // Bind/unbind editor events
    void BindEditorEvents();
    void UnbindEditorEvents();

    FDelegateHandle ActorMovedHandle;
    FDelegateHandle SelectionChangedHandle;

    // Track which actors are currently selected (for OnDeselect mode)
    TSet<TWeakObjectPtr<AActor>> PreviouslySelectedActors;
#endif

    // Internal sync
    bool SyncActorInternal(FRshipTrackedActor& TrackedInfo);
    void OnPeriodicSyncTimer();
    void CheckActorForChanges(FRshipTrackedActor& TrackedInfo);
    bool HasActorMoved(const FRshipTrackedActor& TrackedInfo) const;
};
