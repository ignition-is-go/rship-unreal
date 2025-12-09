// Rocketship Bulk Operations
// Provides bulk editing capabilities for managing many targets at once

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RshipBulkOperations.generated.h"

// Forward declarations
class URshipTargetComponent;
class URshipSubsystem;

/**
 * Configuration snapshot for copying between targets
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipTargetConfig
{
	GENERATED_BODY()

	/** The target name (optional, may be empty if not copying name) */
	UPROPERTY(BlueprintReadWrite, Category = "Rship|Config")
	FString TargetName;

	/** Tags assigned to the target */
	UPROPERTY(BlueprintReadWrite, Category = "Rship|Config")
	TArray<FString> Tags;

	/** Groups the target belongs to */
	UPROPERTY(BlueprintReadWrite, Category = "Rship|Config")
	TArray<FString> GroupIds;

	/** Source target this config was copied from */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Config")
	FString SourceTargetId;

	/** When this config was captured */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Config")
	FDateTime CapturedAt;

	FRshipTargetConfig()
		: CapturedAt(FDateTime::Now())
	{
	}

	bool IsValid() const
	{
		return !SourceTargetId.IsEmpty();
	}
};

/**
 * Delegate for selection changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipSelectionChanged);

/**
 * Static function library for bulk operations on rship targets.
 * All functions are BlueprintCallable for use in Editor Utility Widgets.
 */
UCLASS()
class RSHIPEXEC_API URshipBulkOperations : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========================================================================
	// SELECTION MANAGEMENT
	// ========================================================================

	/** Select multiple targets at once */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void SelectTargets(const TArray<URshipTargetComponent*>& Targets);

	/** Select all targets with a specific tag */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void SelectTargetsByTag(const FString& Tag);

	/** Select all targets in a group */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void SelectTargetsByGroup(const FString& GroupId);

	/** Select targets matching a wildcard pattern */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void SelectTargetsByPattern(const FString& WildcardPattern);

	/** Add targets to the current selection */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void AddToSelection(const TArray<URshipTargetComponent*>& Targets);

	/** Remove targets from the current selection */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void RemoveFromSelection(const TArray<URshipTargetComponent*>& Targets);

	/** Get all currently selected targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static TArray<URshipTargetComponent*> GetSelectedTargets();

	/** Get the number of selected targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static int32 GetSelectionCount();

	/** Check if any targets are selected */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static bool HasSelection();

	/** Clear all selected targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void ClearSelection();

	/** Select all registered targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void SelectAll();

	/** Invert the current selection */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Selection")
	static void InvertSelection();

	// ========================================================================
	// BULK TAG OPERATIONS
	// ========================================================================

	/** Add a tag to all selected targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Tags")
	static int32 BulkAddTag(const FString& Tag);

	/** Add a tag to specific targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Tags")
	static int32 BulkAddTagToTargets(const TArray<URshipTargetComponent*>& Targets, const FString& Tag);

	/** Remove a tag from all selected targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Tags")
	static int32 BulkRemoveTag(const FString& Tag);

	/** Remove a tag from specific targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Tags")
	static int32 BulkRemoveTagFromTargets(const TArray<URshipTargetComponent*>& Targets, const FString& Tag);

	/** Replace one tag with another on all selected targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Tags")
	static int32 BulkReplaceTag(const FString& OldTag, const FString& NewTag);

	/** Clear all tags from selected targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Tags")
	static int32 BulkClearTags();

	// ========================================================================
	// BULK GROUP OPERATIONS
	// ========================================================================

	/** Add all selected targets to a group */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Groups")
	static int32 BulkAddToGroup(const FString& GroupId);

	/** Remove all selected targets from a group */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|Groups")
	static int32 BulkRemoveFromGroup(const FString& GroupId);

	// ========================================================================
	// BULK STATE OPERATIONS
	// ========================================================================

	/** Enable/disable all selected target components */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|State")
	static int32 BulkSetEnabled(bool bEnabled);

	/** Re-register all selected targets with the server */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|State")
	static int32 BulkReregister();

	// ========================================================================
	// COPY/PASTE CONFIGURATION
	// ========================================================================

	/** Copy configuration from a target */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|CopyPaste")
	static FRshipTargetConfig CopyTargetConfig(URshipTargetComponent* Source);

	/** Paste configuration to all selected targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|CopyPaste")
	static int32 PasteTargetConfig(const FRshipTargetConfig& Config, bool bPasteTags = true, bool bPasteGroups = true);

	/** Paste configuration to specific targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|CopyPaste")
	static int32 PasteTargetConfigToTargets(const TArray<URshipTargetComponent*>& Targets, const FRshipTargetConfig& Config, bool bPasteTags = true, bool bPasteGroups = true);

	// ========================================================================
	// FIND AND REPLACE
	// ========================================================================

	/** Find and replace in target names (returns number of targets modified) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|FindReplace")
	static int32 FindAndReplaceInTargetNames(const FString& Find, const FString& Replace, bool bCaseSensitive = false);

	/** Find and replace in tags across all targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Bulk|FindReplace")
	static int32 FindAndReplaceInTags(const FString& Find, const FString& Replace, bool bCaseSensitive = false);

	// ========================================================================
	// UTILITY
	// ========================================================================

	/** Get targets that match a filter predicate (for advanced filtering in C++) */
	static TArray<URshipTargetComponent*> FilterTargets(TFunction<bool(URshipTargetComponent*)> Predicate);

	/** Get the rship subsystem (internal helper) */
	static URshipSubsystem* GetSubsystem();

private:
	/** Internal selection state - stored on the subsystem to persist across calls */
	static TSet<TWeakObjectPtr<URshipTargetComponent>>& GetSelectionSet();

	/** Broadcast selection change event */
	static void NotifySelectionChanged();
};
