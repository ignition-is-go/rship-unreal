// Rocketship Target Group Management
// Provides organization of targets via tags and groups for large-scale projects

#pragma once

#include "CoreMinimal.h"
#include "RshipTargetGroup.generated.h"

// Forward declarations
class URshipTargetComponent;

/**
 * Represents a logical grouping of targets.
 * Groups can be manually populated or auto-populated via patterns.
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipTargetGroup
{
	GENERATED_BODY()

	/** Unique identifier for this group */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Groups")
	FString GroupId;

	/** User-facing display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Groups")
	FString DisplayName;

	/** Color for visual identification in editor and UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Groups")
	FLinearColor Color = FLinearColor::White;

	/** Target IDs that belong to this group */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Groups")
	TArray<FString> TargetIds;

	/** Tags associated with this group */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Groups")
	TArray<FString> Tags;

	/** If true, automatically add targets matching the pattern */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Groups")
	bool bAutoPopulate = false;

	/** Wildcard pattern for auto-population (e.g., "stage-*-lights") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Groups", meta = (EditCondition = "bAutoPopulate"))
	FString AutoPopulatePattern;

	/** When this group was created */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Groups")
	FDateTime CreatedAt;

	/** Last modification time */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Groups")
	FDateTime ModifiedAt;

	FRshipTargetGroup()
		: CreatedAt(FDateTime::Now())
		, ModifiedAt(FDateTime::Now())
	{
	}

	bool IsValid() const
	{
		return !GroupId.IsEmpty() && !DisplayName.IsEmpty();
	}
};

/**
 * Delegate for group changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRshipGroupChanged, const FString&, GroupId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipTargetTagsChanged, URshipTargetComponent*, Target, const TArray<FString>&, Tags);

/**
 * Manages target groups and tags for organizing large numbers of targets.
 * Provides fast lookup by tag, group, or wildcard pattern.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipTargetGroupManager : public UObject
{
	GENERATED_BODY()

public:
	URshipTargetGroupManager();

	// ========================================================================
	// GROUP CRUD OPERATIONS
	// ========================================================================

	/** Create a new group with the given name and color */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	FRshipTargetGroup CreateGroup(const FString& DisplayName, FLinearColor Color = FLinearColor::White);

	/** Delete a group by ID */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	bool DeleteGroup(const FString& GroupId);

	/** Get a group by ID */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	bool GetGroup(const FString& GroupId, FRshipTargetGroup& OutGroup) const;

	/** Update an existing group */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	bool UpdateGroup(const FRshipTargetGroup& Group);

	/** Get all groups */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	TArray<FRshipTargetGroup> GetAllGroups() const;

	// ========================================================================
	// GROUP MEMBERSHIP OPERATIONS
	// ========================================================================

	/** Add a target to a group by ID */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	bool AddTargetToGroup(const FString& TargetId, const FString& GroupId);

	/** Remove a target from a group */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	bool RemoveTargetFromGroup(const FString& TargetId, const FString& GroupId);

	/** Get all group IDs that a target belongs to */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	TArray<FString> GetGroupsForTarget(const FString& TargetId) const;

	// ========================================================================
	// TAG OPERATIONS
	// ========================================================================

	/** Add a tag to a target component */
	UFUNCTION(BlueprintCallable, Category = "Rship|Tags")
	void AddTagToTarget(URshipTargetComponent* Target, const FString& Tag);

	/** Remove a tag from a target component */
	UFUNCTION(BlueprintCallable, Category = "Rship|Tags")
	void RemoveTagFromTarget(URshipTargetComponent* Target, const FString& Tag);

	/** Get all unique tags in use */
	UFUNCTION(BlueprintCallable, Category = "Rship|Tags")
	TArray<FString> GetAllTags() const;

	/** Check if a tag exists anywhere */
	UFUNCTION(BlueprintCallable, Category = "Rship|Tags")
	bool TagExists(const FString& Tag) const;

	// ========================================================================
	// QUERY OPERATIONS
	// ========================================================================

	/** Get all target components with a specific tag */
	UFUNCTION(BlueprintCallable, Category = "Rship|Query")
	TArray<URshipTargetComponent*> GetTargetsByTag(const FString& Tag) const;

	/** Get all target components in a group */
	UFUNCTION(BlueprintCallable, Category = "Rship|Query")
	TArray<URshipTargetComponent*> GetTargetsByGroup(const FString& GroupId) const;

	/** Get targets matching a wildcard pattern (e.g., "stage-*-lights") */
	UFUNCTION(BlueprintCallable, Category = "Rship|Query")
	TArray<URshipTargetComponent*> GetTargetsByPattern(const FString& WildcardPattern) const;

	/** Get targets with multiple tags (AND logic) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Query")
	TArray<URshipTargetComponent*> GetTargetsByTags(const TArray<FString>& Tags) const;

	/** Get targets with any of the given tags (OR logic) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Query")
	TArray<URshipTargetComponent*> GetTargetsByAnyTag(const TArray<FString>& Tags) const;

	// ========================================================================
	// AUTO-GROUPING HELPERS
	// ========================================================================

	/** Create a group containing all instances of an actor class */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	FRshipTargetGroup CreateGroupFromActorClass(TSubclassOf<AActor> ActorClass, const FString& GroupName = TEXT(""));

	/** Create a group from targets within a radius */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	FRshipTargetGroup CreateGroupFromProximity(FVector Center, float Radius, const FString& GroupName = TEXT(""));

	// ========================================================================
	// INTERNAL INDEX MANAGEMENT
	// ========================================================================

	/** Register a target component (called when target registers) */
	void RegisterTarget(URshipTargetComponent* Target);

	/** Unregister a target component (called when target unregisters) */
	void UnregisterTarget(URshipTargetComponent* Target);

	/** Rebuild all indices (call after bulk changes) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
	void RebuildIndices();

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Called when a group is created, modified, or deleted */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Events")
	FOnRshipGroupChanged OnGroupChanged;

	/** Called when a target's tags change */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Events")
	FOnRshipTargetTagsChanged OnTargetTagsChanged;

	// ========================================================================
	// PERSISTENCE
	// ========================================================================

	/** Save all groups to JSON file in Saved/Rship directory */
	UFUNCTION(BlueprintCallable, Category = "Rship|Persistence")
	bool SaveGroupsToFile();

	/** Load groups from JSON file */
	UFUNCTION(BlueprintCallable, Category = "Rship|Persistence")
	bool LoadGroupsFromFile();

	/** Export all groups to a JSON string */
	UFUNCTION(BlueprintCallable, Category = "Rship|Persistence")
	FString ExportGroupsToJson() const;

	/** Import groups from a JSON string */
	UFUNCTION(BlueprintCallable, Category = "Rship|Persistence")
	bool ImportGroupsFromJson(const FString& JsonString);

	/** Get the path where groups are saved */
	UFUNCTION(BlueprintCallable, Category = "Rship|Persistence")
	static FString GetGroupsSaveFilePath();

private:
	/** Generate a unique group ID */
	FString GenerateGroupId() const;

	/** Update reverse indices after group membership change */
	void UpdateTargetToGroupsIndex(const FString& TargetId, const FString& GroupId, bool bAdd);

	/** Check if a target ID matches a wildcard pattern */
	static bool MatchesWildcard(const FString& TargetId, const FString& Pattern);

	/** Normalize a tag (lowercase, trimmed) */
	static FString NormalizeTag(const FString& Tag);

	/** All groups indexed by ID */
	UPROPERTY()
	TMap<FString, FRshipTargetGroup> Groups;

	/** Reverse index: Tag -> Set of target IDs */
	TMap<FString, TSet<FString>> TagToTargets;

	/** Reverse index: Target ID -> Set of Group IDs */
	TMap<FString, TSet<FString>> TargetToGroups;

	/** All registered target components (weak references to avoid preventing GC) */
	TMap<FString, TWeakObjectPtr<URshipTargetComponent>> RegisteredTargets;

	/** Counter for generating unique group IDs */
	int32 GroupIdCounter = 0;
};
