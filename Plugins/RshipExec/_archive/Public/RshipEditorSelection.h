// Rocketship Editor Selection Sync
// Synchronizes Rship target selection with Unreal Editor viewport selection
// Only active in Editor builds (wrapped with WITH_EDITOR)

#pragma once

#include "CoreMinimal.h"
#include "RshipEditorSelection.generated.h"

// Forward declarations
class URshipSubsystem;
class URshipActorRegistrationComponent;

/**
 * Selection sync mode
 */
UENUM(BlueprintType)
enum class ERshipSelectionSyncMode : uint8
{
	/** No automatic sync */
	Disabled,

	/** Rship selection follows Editor selection */
	EditorToRship,

	/** Editor selection follows Rship selection */
	RshipToEditor,

	/** Bidirectional sync (last change wins) */
	Bidirectional
};

/**
 * Delegate for selection sync events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRshipEditorSelectionSynced, int32, TargetCount);

/**
 * Manages synchronization between Rship target selection and Editor viewport selection.
 * Only functional in Editor builds - in runtime builds, all methods are no-ops.
 * Access via URshipSubsystem::GetEditorSelection()
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipEditorSelection : public UObject
{
	GENERATED_BODY()

public:
	URshipEditorSelection();

	/** Initialize with reference to subsystem */
	void Initialize(URshipSubsystem* InSubsystem);

	/** Shutdown and unbind from editor events */
	void Shutdown();

	// ========================================================================
	// SYNC CONTROL
	// ========================================================================

	/** Set the selection sync mode */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	void SetSyncMode(ERshipSelectionSyncMode Mode);

	/** Get the current sync mode */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	ERshipSelectionSyncMode GetSyncMode() const { return SyncMode; }

	/** Check if selection sync is available (Editor build) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	bool IsEditorSyncAvailable() const;

	// ========================================================================
	// MANUAL SYNC
	// ========================================================================

	/** Sync Editor selection to Rship (select in Rship what's selected in Editor) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	int32 SyncEditorToRship();

	/** Sync Rship selection to Editor (select in Editor what's selected in Rship) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	int32 SyncRshipToEditor();

	/** Select actors in Editor viewport by their Rship target components */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	int32 SelectActorsInEditor(const TArray<URshipActorRegistrationComponent*>& Targets);

	/** Get Rship target components from currently selected Editor actors */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	TArray<URshipActorRegistrationComponent*> GetTargetsFromEditorSelection();

	// ========================================================================
	// VIEWPORT FOCUS
	// ========================================================================

	/** Focus the Editor viewport on selected Rship targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	void FocusOnSelectedTargets();

	/** Focus the Editor viewport on specific targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Editor|Selection")
	void FocusOnTargets(const TArray<URshipActorRegistrationComponent*>& Targets);

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when selection is synced */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Editor|Selection|Events")
	FOnRshipEditorSelectionSynced OnSelectionSynced;

private:
#if WITH_EDITOR
	/** Handle Editor selection changed */
	void OnEditorSelectionChanged(UObject* Object);

	/** Handle Rship selection changed */
	void OnRshipSelectionChanged();

	/** Bind to editor events */
	void BindEditorEvents();

	/** Unbind from editor events */
	void UnbindEditorEvents();

	/** Delegate handle for editor selection */
	FDelegateHandle EditorSelectionHandle;

	/** Prevent recursive sync */
	bool bIsSyncing = false;
#endif

	/** Reference to subsystem */
	UPROPERTY()
	URshipSubsystem* Subsystem;

	/** Current sync mode */
	ERshipSelectionSyncMode SyncMode = ERshipSelectionSyncMode::Disabled;
};

