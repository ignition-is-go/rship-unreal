// Rocketship Data Layer Manager
// Handles Data Layer awareness for World Partition workflows
// Automatically registers/unregisters targets as Data Layers load/unload

#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "RshipDataLayerManager.generated.h"

// Forward declarations
class URshipSubsystem;
class URshipTargetComponent;
class UDataLayerInstance;
class UDataLayerAsset;
class UDataLayerSubsystem;

/**
 * Information about a Data Layer and its targets
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDataLayerInfo
{
	GENERATED_BODY()

	/** Name of the Data Layer */
	UPROPERTY(BlueprintReadOnly, Category = "DataLayer")
	FString DataLayerName;

	/** Asset name (if using Data Layer Assets) */
	UPROPERTY(BlueprintReadOnly, Category = "DataLayer")
	FString DataLayerAssetName;

	/** Current runtime state */
	UPROPERTY(BlueprintReadOnly, Category = "DataLayer")
	EDataLayerRuntimeState RuntimeState = EDataLayerRuntimeState::Unloaded;

	/** Whether this Data Layer is loaded */
	UPROPERTY(BlueprintReadOnly, Category = "DataLayer")
	bool bIsLoaded = false;

	/** Whether this Data Layer is activated (visible) */
	UPROPERTY(BlueprintReadOnly, Category = "DataLayer")
	bool bIsActivated = false;

	/** Number of targets in this Data Layer */
	UPROPERTY(BlueprintReadOnly, Category = "DataLayer")
	int32 TargetCount = 0;

	/** Debug color from the Data Layer */
	UPROPERTY(BlueprintReadOnly, Category = "DataLayer")
	FColor DebugColor = FColor::White;
};

/**
 * Delegates for Data Layer events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipDataLayerStateChanged, const FString&, DataLayerName, EDataLayerRuntimeState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipDataLayerTargetsRegistered, const FString&, DataLayerName, int32, TargetCount);

/**
 * Manages Data Layer awareness for targets.
 * Automatically handles target registration when Data Layers load/unload.
 * Access via URshipSubsystem::GetDataLayerManager()
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipDataLayerManager : public UObject
{
	GENERATED_BODY()

public:
	URshipDataLayerManager();

	/** Initialize with reference to subsystem */
	void Initialize(URshipSubsystem* InSubsystem);

	/** Shutdown and unbind from Data Layer events */
	void Shutdown();

	// ========================================================================
	// DATA LAYER QUERIES
	// ========================================================================

	/** Get all Data Layers in the world with target info */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	TArray<FRshipDataLayerInfo> GetAllDataLayers();

	/** Get info about a specific Data Layer */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	FRshipDataLayerInfo GetDataLayerInfo(const FString& DataLayerName);

	/** Get all targets in a specific Data Layer */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	TArray<URshipTargetComponent*> GetTargetsInDataLayer(const FString& DataLayerName);

	/** Get all targets in Data Layers matching a pattern */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	TArray<URshipTargetComponent*> GetTargetsByDataLayerPattern(const FString& WildcardPattern);

	/** Get the Data Layers a target belongs to */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	TArray<FString> GetTargetDataLayers(URshipTargetComponent* Target);

	/** Check if a Data Layer is currently loaded */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	bool IsDataLayerLoaded(const FString& DataLayerName);

	/** Check if a Data Layer is currently activated */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	bool IsDataLayerActivated(const FString& DataLayerName);

	/** Get current runtime state of a Data Layer */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	EDataLayerRuntimeState GetDataLayerState(const FString& DataLayerName);

	// ========================================================================
	// DATA LAYER ACTIONS
	// ========================================================================

	/** Re-register all targets in a specific Data Layer */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	int32 ReregisterTargetsInDataLayer(const FString& DataLayerName);

	/** Add a tag to all targets in a Data Layer */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	int32 AddTagToDataLayerTargets(const FString& DataLayerName, const FString& Tag);

	/** Remove a tag from all targets in a Data Layer */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	int32 RemoveTagFromDataLayerTargets(const FString& DataLayerName, const FString& Tag);

	/** Add all targets in a Data Layer to a group */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	int32 AddDataLayerTargetsToGroup(const FString& DataLayerName, const FString& GroupId);

	// ========================================================================
	// AUTO-TAGGING
	// ========================================================================

	/** Enable automatic Data Layer tagging (adds Data Layer name as tag to all targets) */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	void SetAutoDataLayerTagging(bool bEnabled);

	/** Check if auto Data Layer tagging is enabled */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	bool IsAutoDataLayerTaggingEnabled() const { return bAutoDataLayerTagging; }

	/** Set the prefix used for auto Data Layer tags (default: "datalayer:") */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	void SetAutoDataLayerTagPrefix(const FString& Prefix);

	/** Get the auto Data Layer tag prefix */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	FString GetAutoDataLayerTagPrefix() const { return AutoDataLayerTagPrefix; }

	// ========================================================================
	// AUTO-GROUPING
	// ========================================================================

	/** Enable automatic group creation per Data Layer */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	void SetAutoDataLayerGrouping(bool bEnabled);

	/** Check if auto Data Layer grouping is enabled */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	bool IsAutoDataLayerGroupingEnabled() const { return bAutoDataLayerGrouping; }

	/** Create groups for all current Data Layers */
	UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
	int32 CreateGroupsForAllDataLayers();

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when a Data Layer's runtime state changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|DataLayers|Events")
	FOnRshipDataLayerStateChanged OnDataLayerStateChanged;

	/** Fired when targets are registered from a Data Layer */
	UPROPERTY(BlueprintAssignable, Category = "Rship|DataLayers|Events")
	FOnRshipDataLayerTargetsRegistered OnDataLayerTargetsRegistered;

private:
	/** Handle Data Layer runtime state change */
	void OnDataLayerRuntimeStateChanged(const UDataLayerInstance* DataLayer, EDataLayerRuntimeState NewState);

	/** Register all targets in a Data Layer */
	void RegisterDataLayerTargets(const UDataLayerInstance* DataLayer);

	/** Unregister all targets in a Data Layer */
	void UnregisterDataLayerTargets(const UDataLayerInstance* DataLayer);

	/** Apply auto Data Layer tag to a target */
	void ApplyAutoDataLayerTag(URshipTargetComponent* Target, const FString& DataLayerName);

	/** Remove auto Data Layer tags from a target */
	void RemoveAutoDataLayerTags(URshipTargetComponent* Target);

	/** Get all targets that belong to a specific Data Layer instance */
	TArray<URshipTargetComponent*> GetTargetsForDataLayerInstance(const UDataLayerInstance* DataLayer);

	/** Find Data Layer instance by name */
	const UDataLayerInstance* FindDataLayerByName(const FString& DataLayerName) const;

	/** Get the Data Layer subsystem */
	UDataLayerSubsystem* GetDataLayerSubsystem() const;

	/** Reference to subsystem */
	UPROPERTY()
	URshipSubsystem* Subsystem;

	/** Whether auto Data Layer tagging is enabled */
	bool bAutoDataLayerTagging = false;

	/** Prefix for auto Data Layer tags */
	FString AutoDataLayerTagPrefix = TEXT("datalayer:");

	/** Whether auto Data Layer grouping is enabled */
	bool bAutoDataLayerGrouping = false;

	/** Cache of Data Layer states */
	TMap<FName, EDataLayerRuntimeState> DataLayerStates;

	/** Delegate handle for state changes */
	FDelegateHandle DataLayerStateChangedHandle;
};
