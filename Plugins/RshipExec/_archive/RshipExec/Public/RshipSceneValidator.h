// Rship Scene Validator
// Validates UE scenes before conversion to rship targets

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameFramework/Actor.h"
#include "RshipSceneValidator.generated.h"

class URshipSubsystem;

// Severity of validation issues
UENUM(BlueprintType)
enum class ERshipValidationSeverity : uint8
{
    Info        UMETA(DisplayName = "Info"),       // Informational, no action needed
    Warning     UMETA(DisplayName = "Warning"),    // May cause issues, review recommended
    Error       UMETA(DisplayName = "Error"),      // Will prevent conversion
    Critical    UMETA(DisplayName = "Critical")    // Serious issue, must fix
};

// Category of validation issue
UENUM(BlueprintType)
enum class ERshipValidationCategory : uint8
{
    Naming          UMETA(DisplayName = "Naming"),           // Actor/component naming issues
    Hierarchy       UMETA(DisplayName = "Hierarchy"),        // Parent/child relationship issues
    Components      UMETA(DisplayName = "Components"),       // Missing or invalid components
    Properties      UMETA(DisplayName = "Properties"),       // Property value issues
    References      UMETA(DisplayName = "References"),       // Missing or broken references
    Performance     UMETA(DisplayName = "Performance"),      // Performance concerns
    Compatibility   UMETA(DisplayName = "Compatibility"),    // Rship compatibility issues
    DataLayers      UMETA(DisplayName = "DataLayers")        // World Partition data layer issues
};

// Single validation issue
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipValidationIssue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    ERshipValidationSeverity Severity = ERshipValidationSeverity::Warning;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    ERshipValidationCategory Category = ERshipValidationCategory::Components;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    FString Details;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    TObjectPtr<AActor> AffectedActor;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    FString AffectedComponentName;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    FString SuggestedFix;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    bool bCanAutoFix = false;
};

// Validation result for entire scene
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipValidationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    bool bIsValid = true;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    int32 TotalActorsScanned = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    int32 ConvertibleActors = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    int32 InfoCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    int32 WarningCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    int32 ErrorCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    int32 CriticalCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    TArray<FRshipValidationIssue> Issues;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    float ValidationTimeSeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Validation")
    FDateTime ValidationTimestamp;
};

// Validation rule configuration
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipValidationRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Validation")
    FString RuleId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Validation")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Validation")
    ERshipValidationCategory Category = ERshipValidationCategory::Components;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Validation")
    ERshipValidationSeverity DefaultSeverity = ERshipValidationSeverity::Warning;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Validation")
    bool bEnabled = true;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnValidationComplete, const FRshipValidationResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnValidationProgress, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIssueFound, const FRshipValidationIssue&, Issue, int32, IssueIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAutoFixApplied, const FString&, IssueId, bool, bSuccess);

/**
 * Scene validator for pre-conversion checks
 * Scans UE scenes and identifies potential issues before converting to rship targets
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipSceneValidator : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /** Validate entire scene */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    FRshipValidationResult ValidateScene();

    /** Validate specific actors */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    FRshipValidationResult ValidateActors(const TArray<AActor*>& Actors);

    /** Validate a single actor */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    TArray<FRshipValidationIssue> ValidateActor(AActor* Actor);

    /** Quick check if actor can be converted (no detailed issues) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    bool CanConvertActor(AActor* Actor) const;

    /** Get last validation result */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    FRshipValidationResult GetLastResult() const { return LastResult; }

    // ========================================================================
    // RULE MANAGEMENT
    // ========================================================================

    /** Get all validation rules */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    TArray<FRshipValidationRule> GetAllRules() const;

    /** Enable/disable a specific rule */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    void SetRuleEnabled(const FString& RuleId, bool bEnabled);

    /** Check if a rule is enabled */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    bool IsRuleEnabled(const FString& RuleId) const;

    /** Reset all rules to defaults */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    void ResetRulesToDefaults();

    // ========================================================================
    // AUTO-FIX
    // ========================================================================

    /** Attempt to auto-fix a specific issue */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    bool TryAutoFix(const FString& IssueId);

    /** Auto-fix all fixable issues */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    int32 AutoFixAll();

    /** Get issues that can be auto-fixed */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    TArray<FRshipValidationIssue> GetAutoFixableIssues() const;

    // ========================================================================
    // FILTERING
    // ========================================================================

    /** Get issues by severity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    TArray<FRshipValidationIssue> GetIssuesBySeverity(ERshipValidationSeverity Severity) const;

    /** Get issues by category */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    TArray<FRshipValidationIssue> GetIssuesByCategory(ERshipValidationCategory Category) const;

    /** Get issues for specific actor */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    TArray<FRshipValidationIssue> GetIssuesForActor(AActor* Actor) const;

    // ========================================================================
    // DELEGATES
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Rship|Validation")
    FOnValidationComplete OnValidationComplete;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Validation")
    FOnValidationProgress OnValidationProgress;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Validation")
    FOnIssueFound OnIssueFound;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Validation")
    FOnAutoFixApplied OnAutoFixApplied;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    FRshipValidationResult LastResult;
    TMap<FString, FRshipValidationRule> Rules;
    TMap<FString, FRshipValidationIssue> IssueCache;

    void InitializeDefaultRules();

    // Validation checks
    void CheckNaming(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues);
    void CheckComponents(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues);
    void CheckLightComponents(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues);
    void CheckCameraComponents(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues);
    void CheckHierarchy(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues);
    void CheckReferences(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues);
    void CheckPerformance(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues);
    void CheckDataLayers(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues);

    // Auto-fix implementations
    bool FixNamingIssue(const FRshipValidationIssue& Issue);
    bool FixComponentIssue(const FRshipValidationIssue& Issue);

    FRshipValidationIssue CreateIssue(
        ERshipValidationSeverity Severity,
        ERshipValidationCategory Category,
        const FString& Message,
        AActor* Actor = nullptr,
        const FString& ComponentName = TEXT(""),
        const FString& Details = TEXT(""),
        const FString& SuggestedFix = TEXT(""),
        bool bCanAutoFix = false
    );
};
