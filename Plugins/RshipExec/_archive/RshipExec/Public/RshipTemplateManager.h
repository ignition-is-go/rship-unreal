// Rocketship Template Manager
// Create and apply reusable target configuration templates

#pragma once

#include "CoreMinimal.h"
#include "RshipTemplateManager.generated.h"

// Forward declarations
class URshipSubsystem;
class URshipActorRegistrationComponent;

/**
 * A template for target configuration that can be applied to new or existing targets
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipTargetTemplate
{
	GENERATED_BODY()

	/** Unique identifier for this template */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString TemplateId;

	/** User-facing display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString DisplayName;

	/** Optional description */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString Description;

	/** Tags to apply to targets using this template */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	TArray<FString> Tags;

	/** Groups to add targets to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	TArray<FString> GroupIds;

	/** Name prefix for auto-naming targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString NamePrefix;

	/** Name suffix for auto-naming targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString NameSuffix;

	/** Whether to auto-generate sequential names */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	bool bAutoGenerateName = false;

	/** Category for organizing templates in UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FString Category;

	/** When this template was created */
	UPROPERTY(BlueprintReadOnly, Category = "Template")
	FDateTime CreatedAt;

	/** When this template was last modified */
	UPROPERTY(BlueprintReadOnly, Category = "Template")
	FDateTime ModifiedAt;

	/** Number of targets created from this template */
	UPROPERTY(BlueprintReadOnly, Category = "Template")
	int32 UseCount = 0;

	FRshipTargetTemplate()
		: CreatedAt(FDateTime::Now())
		, ModifiedAt(FDateTime::Now())
	{
	}

	bool IsValid() const { return !TemplateId.IsEmpty() && !DisplayName.IsEmpty(); }
};

/**
 * Delegate for template events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipTemplateApplied, const FString&, TemplateId, URshipActorRegistrationComponent*, Target);

/**
 * Manages target configuration templates.
 * Access via URshipSubsystem::GetTemplateManager()
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipTemplateManager : public UObject
{
	GENERATED_BODY()

public:
	URshipTemplateManager();

	/** Initialize with reference to subsystem */
	void Initialize(URshipSubsystem* InSubsystem);

	/** Shutdown */
	void Shutdown();

	// ========================================================================
	// TEMPLATE CREATION
	// ========================================================================

	/** Create a new template from scratch */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	FRshipTargetTemplate CreateTemplate(const FString& Name, const FString& Description = TEXT(""));

	/** Create a template from an existing target's configuration */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	FRshipTargetTemplate CreateTemplateFromTarget(const FString& Name, URshipActorRegistrationComponent* SourceTarget);

	/** Create a template from multiple targets (merges common tags/groups) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	FRshipTargetTemplate CreateTemplateFromTargets(const FString& Name, const TArray<URshipActorRegistrationComponent*>& SourceTargets);

	// ========================================================================
	// TEMPLATE APPLICATION
	// ========================================================================

	/** Apply a template to a single target */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	void ApplyTemplate(const FRshipTargetTemplate& Template, URshipActorRegistrationComponent* Target, bool bMergeTags = true);

	/** Apply a template to multiple targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	int32 ApplyTemplateToTargets(const FRshipTargetTemplate& Template, const TArray<URshipActorRegistrationComponent*>& Targets, bool bMergeTags = true);

	/** Apply a template by ID to a target */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	void ApplyTemplateById(const FString& TemplateId, URshipActorRegistrationComponent* Target, bool bMergeTags = true);

	/** Apply a template to all targets with a specific tag */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	int32 ApplyTemplateToTaggedTargets(const FString& TemplateId, const FString& Tag, bool bMergeTags = true);

	// ========================================================================
	// TEMPLATE MANAGEMENT
	// ========================================================================

	/** Save a template */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	void SaveTemplate(const FRshipTargetTemplate& Template);

	/** Delete a template by ID */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	bool DeleteTemplate(const FString& TemplateId);

	/** Get a template by ID */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	bool GetTemplate(const FString& TemplateId, FRshipTargetTemplate& OutTemplate) const;

	/** Get all saved templates */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	TArray<FRshipTargetTemplate> GetAllTemplates() const;

	/** Get templates in a specific category */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	TArray<FRshipTargetTemplate> GetTemplatesByCategory(const FString& Category) const;

	/** Get all unique categories */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	TArray<FString> GetAllCategories() const;

	/** Update template metadata */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	bool UpdateTemplate(const FRshipTargetTemplate& Template);

	/** Duplicate a template with a new name */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	FRshipTargetTemplate DuplicateTemplate(const FString& SourceTemplateId, const FString& NewName);

	// ========================================================================
	// AUTO-NAMING
	// ========================================================================

	/** Generate a name for a target using template's naming rules */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	FString GenerateTargetName(const FRshipTargetTemplate& Template);

	/** Reset the auto-name counter for a template */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	void ResetAutoNameCounter(const FString& TemplateId);

	// ========================================================================
	// PERSISTENCE
	// ========================================================================

	/** Save all templates to file */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	bool SaveTemplatesToFile();

	/** Load templates from file */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	bool LoadTemplatesFromFile();

	/** Get the path where templates are saved */
	UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
	static FString GetTemplatesSaveFilePath();

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when a template is applied to a target */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Templates|Events")
	FOnRshipTemplateApplied OnTemplateApplied;

private:
	/** Generate a unique template ID */
	FString GenerateTemplateId() const;

	/** Reference to subsystem */
	UPROPERTY()
	URshipSubsystem* Subsystem;

	/** All saved templates */
	UPROPERTY()
	TMap<FString, FRshipTargetTemplate> Templates;

	/** Auto-name counters per template */
	TMap<FString, int32> AutoNameCounters;

	/** Counter for generating unique IDs */
	int32 TemplateIdCounter = 0;
};

