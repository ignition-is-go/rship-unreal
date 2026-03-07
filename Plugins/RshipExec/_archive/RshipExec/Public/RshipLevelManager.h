// Rocketship Level Manager
// Handles level streaming awareness and target lifecycle per level

#pragma once

#include "CoreMinimal.h"
#include "Engine/LevelStreaming.h"
#include "RshipLevelManager.generated.h"

// Forward declarations
class URshipSubsystem;
class URshipActorRegistrationComponent;
class ULevel;
class UWorld;

/**
 * Information about targets in a specific level
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipLevelInfo
{
	GENERATED_BODY()

	/** Name of the level (package name) */
	UPROPERTY(BlueprintReadOnly, Category = "Level")
	FString LevelName;

	/** Display name for UI */
	UPROPERTY(BlueprintReadOnly, Category = "Level")
	FString DisplayName;

	/** Whether the level is currently loaded */
	UPROPERTY(BlueprintReadOnly, Category = "Level")
	bool bIsLoaded = false;

	/** Whether the level is currently visible */
	UPROPERTY(BlueprintReadOnly, Category = "Level")
	bool bIsVisible = false;

	/** Whether this is the persistent level */
	UPROPERTY(BlueprintReadOnly, Category = "Level")
	bool bIsPersistent = false;

	/** Number of targets in this level (when loaded) */
	UPROPERTY(BlueprintReadOnly, Category = "Level")
	int32 TargetCount = 0;

	/** Number of active targets in this level */
	UPROPERTY(BlueprintReadOnly, Category = "Level")
	int32 ActiveTargetCount = 0;
};

/**
 * Delegate for level events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipLevelLoaded, const FString&, LevelName, int32, TargetCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipLevelUnloaded, const FString&, LevelName, int32, TargetCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipLevelVisibilityChanged, const FString&, LevelName, bool, bIsVisible);

/**
 * Manages level streaming awareness for targets.
 * Access via URshipSubsystem::GetLevelManager()
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipLevelManager : public UObject
{
	GENERATED_BODY()

public:
	URshipLevelManager();

	/** Initialize with reference to subsystem */
	void Initialize(URshipSubsystem* InSubsystem);

	/** Shutdown and unbind from world events */
	void Shutdown();

	// ========================================================================
	// LEVEL QUERIES
	// ========================================================================

	/** Get all levels in the world with target info */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	TArray<FRshipLevelInfo> GetAllLevels();

	/** Get info about a specific level */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	FRshipLevelInfo GetLevelInfo(const FString& LevelName);

	/** Get all targets in a specific level */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	TArray<URshipActorRegistrationComponent*> GetTargetsInLevel(const FString& LevelName);

	/** Get all targets in the persistent level */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	TArray<URshipActorRegistrationComponent*> GetTargetsInPersistentLevel();

	/** Get all targets in currently loaded streaming levels */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	TArray<URshipActorRegistrationComponent*> GetTargetsInStreamingLevels();

	/** Get the level a target belongs to */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	FString GetTargetLevel(URshipActorRegistrationComponent* Target);

	/** Check if a level is currently loaded */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	bool IsLevelLoaded(const FString& LevelName);

	/** Check if a level is currently visible */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	bool IsLevelVisible(const FString& LevelName);

	// ========================================================================
	// LEVEL ACTIONS
	// ========================================================================

	/** Re-register all targets in a specific level */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	int32 ReregisterTargetsInLevel(const FString& LevelName);

	/** Send offline status for all targets in a level (before unloading) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	int32 SetLevelTargetsOffline(const FString& LevelName);

	/** Add a tag to all targets in a level */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	int32 AddTagToLevelTargets(const FString& LevelName, const FString& Tag);

	/** Remove a tag from all targets in a level */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	int32 RemoveTagFromLevelTargets(const FString& LevelName, const FString& Tag);

	// ========================================================================
	// AUTO-TAGGING
	// ========================================================================

	/** Enable automatic level tagging (adds level name as tag to all targets) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	void SetAutoLevelTagging(bool bEnabled);

	/** Check if auto level tagging is enabled */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	bool IsAutoLevelTaggingEnabled() const { return bAutoLevelTagging; }

	/** Set the prefix used for auto level tags (default: "level:") */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	void SetAutoLevelTagPrefix(const FString& Prefix);

	/** Get the auto level tag prefix */
	UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
	FString GetAutoLevelTagPrefix() const { return AutoLevelTagPrefix; }

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when a level is loaded and its targets are registered */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Levels|Events")
	FOnRshipLevelLoaded OnLevelLoaded;

	/** Fired when a level is about to unload */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Levels|Events")
	FOnRshipLevelUnloaded OnLevelUnloaded;

	/** Fired when a level's visibility changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Levels|Events")
	FOnRshipLevelVisibilityChanged OnLevelVisibilityChanged;

private:
	/** Handle level added to world */
	void OnLevelAdded(ULevel* Level, UWorld* World);

	/** Handle level removed from world */
	void OnLevelRemoved(ULevel* Level, UWorld* World);

	/** Handle streaming level visibility change */
	void OnLevelVisibilityChange(UWorld* World, const ULevelStreaming* LevelStreaming, bool bIsVisible);

	/** Register all targets in a level */
	void RegisterLevelTargets(ULevel* Level);

	/** Unregister all targets in a level */
	void UnregisterLevelTargets(ULevel* Level);

	/** Get short name from full level path */
	FString GetLevelShortName(const FString& LevelPath) const;

	/** Apply auto level tag to a target */
	void ApplyAutoLevelTag(URshipActorRegistrationComponent* Target, const FString& LevelName);

	/** Remove auto level tag from a target */
	void RemoveAutoLevelTag(URshipActorRegistrationComponent* Target);

	/** Reference to subsystem */
	UPROPERTY()
	URshipSubsystem* Subsystem;

	/** Whether auto level tagging is enabled */
	bool bAutoLevelTagging = false;

	/** Prefix for auto level tags */
	FString AutoLevelTagPrefix = TEXT("level:");

	/** Track which levels we've processed */
	TSet<TWeakObjectPtr<ULevel>> ProcessedLevels;

	/** Delegate handles for unbinding */
	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;
};

