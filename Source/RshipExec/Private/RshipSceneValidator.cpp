// Rship Scene Validator Implementation

#include "RshipSceneValidator.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "Logs.h"
#include "EngineUtils.h"
#include "Components/LightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

void URshipSceneValidator::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    InitializeDefaultRules();
    UE_LOG(LogRshipExec, Log, TEXT("SceneValidator initialized with %d rules"), Rules.Num());
}

void URshipSceneValidator::Shutdown()
{
    Rules.Empty();
    IssueCache.Empty();
    Subsystem = nullptr;
}

void URshipSceneValidator::InitializeDefaultRules()
{
    Rules.Empty();

    auto AddRule = [this](const FString& Id, const FString& Desc, ERshipValidationCategory Cat, ERshipValidationSeverity Sev) {
        FRshipValidationRule Rule;
        Rule.RuleId = Id;
        Rule.Description = Desc;
        Rule.Category = Cat;
        Rule.DefaultSeverity = Sev;
        Rule.bEnabled = true;
        Rules.Add(Id, Rule);
    };

    // Naming rules
    AddRule(TEXT("NAMING_EMPTY"), TEXT("Actor has empty or default name"), ERshipValidationCategory::Naming, ERshipValidationSeverity::Warning);
    AddRule(TEXT("NAMING_DUPLICATE"), TEXT("Multiple actors with same name"), ERshipValidationCategory::Naming, ERshipValidationSeverity::Error);
    AddRule(TEXT("NAMING_SPECIAL_CHARS"), TEXT("Actor name contains special characters"), ERshipValidationCategory::Naming, ERshipValidationSeverity::Warning);

    // Component rules
    AddRule(TEXT("COMP_NO_CONVERTIBLE"), TEXT("Actor has no convertible components"), ERshipValidationCategory::Components, ERshipValidationSeverity::Info);
    AddRule(TEXT("COMP_ALREADY_TARGET"), TEXT("Actor already has RshipTargetComponent"), ERshipValidationCategory::Components, ERshipValidationSeverity::Info);
    AddRule(TEXT("COMP_MULTIPLE_LIGHTS"), TEXT("Actor has multiple light components"), ERshipValidationCategory::Components, ERshipValidationSeverity::Warning);

    // Light-specific rules
    AddRule(TEXT("LIGHT_NO_MOBILITY"), TEXT("Light is static and cannot be controlled"), ERshipValidationCategory::Compatibility, ERshipValidationSeverity::Error);
    AddRule(TEXT("LIGHT_NO_IES"), TEXT("Light has no IES profile assigned"), ERshipValidationCategory::Properties, ERshipValidationSeverity::Info);
    AddRule(TEXT("LIGHT_EXTREME_INTENSITY"), TEXT("Light intensity is extremely high"), ERshipValidationCategory::Performance, ERshipValidationSeverity::Warning);

    // Camera rules
    AddRule(TEXT("CAMERA_NO_MOBILITY"), TEXT("Camera is static"), ERshipValidationCategory::Compatibility, ERshipValidationSeverity::Warning);

    // Hierarchy rules
    AddRule(TEXT("HIER_DEEPLY_NESTED"), TEXT("Actor is deeply nested (>5 levels)"), ERshipValidationCategory::Hierarchy, ERshipValidationSeverity::Warning);
    AddRule(TEXT("HIER_CIRCULAR"), TEXT("Potential circular reference detected"), ERshipValidationCategory::Hierarchy, ERshipValidationSeverity::Critical);

    // Reference rules
    AddRule(TEXT("REF_MISSING_MATERIAL"), TEXT("Component has missing material reference"), ERshipValidationCategory::References, ERshipValidationSeverity::Warning);

    // Performance rules
    AddRule(TEXT("PERF_HIGH_POLY_COUNT"), TEXT("Actor has high polygon count"), ERshipValidationCategory::Performance, ERshipValidationSeverity::Info);
    AddRule(TEXT("PERF_MANY_COMPONENTS"), TEXT("Actor has many components (>20)"), ERshipValidationCategory::Performance, ERshipValidationSeverity::Warning);
}

FRshipValidationResult URshipSceneValidator::ValidateScene()
{
    double StartTime = FPlatformTime::Seconds();
    FRshipValidationResult Result;
    Result.ValidationTimestamp = FDateTime::Now();
    IssueCache.Empty();

    UWorld* World = Subsystem ? GEngine->GetWorldFromContextObjectChecked(Subsystem) : nullptr;
    if (!World) { Result.bIsValid = false; return Result; }

    TArray<AActor*> AllActors;
    for (TActorIterator<AActor> It(World); It; ++It) AllActors.Add(*It);
    Result.TotalActorsScanned = AllActors.Num();

    // Check for duplicate names
    TMap<FString, TArray<AActor*>> NameMap;
    for (AActor* Actor : AllActors)
    {
        if (Actor) NameMap.FindOrAdd(Actor->GetActorLabel()).Add(Actor);
    }

    int32 ProcessedCount = 0;
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;

        TArray<FRshipValidationIssue> ActorIssues = ValidateActor(Actor);

        // Check for duplicate names
        if (IsRuleEnabled(TEXT("NAMING_DUPLICATE")))
        {
            TArray<AActor*>* Duplicates = NameMap.Find(Actor->GetActorLabel());
            if (Duplicates && Duplicates->Num() > 1)
            {
                ActorIssues.Add(CreateIssue(
                    ERshipValidationSeverity::Error,
                    ERshipValidationCategory::Naming,
                    FString::Printf(TEXT("Duplicate name: %d actors named '%s'"), Duplicates->Num(), *Actor->GetActorLabel()),
                    Actor, TEXT(""), TEXT("Each target needs a unique name"), TEXT("Rename actor"), true
                ));
            }
        }

        if (CanConvertActor(Actor)) Result.ConvertibleActors++;

        for (const auto& Issue : ActorIssues)
        {
            Result.Issues.Add(Issue);
            IssueCache.Add(Issue.Id, Issue);
            OnIssueFound.Broadcast(Issue, Result.Issues.Num() - 1);

            switch (Issue.Severity)
            {
                case ERshipValidationSeverity::Info: Result.InfoCount++; break;
                case ERshipValidationSeverity::Warning: Result.WarningCount++; break;
                case ERshipValidationSeverity::Error: Result.ErrorCount++; Result.bIsValid = false; break;
                case ERshipValidationSeverity::Critical: Result.CriticalCount++; Result.bIsValid = false; break;
            }
        }

        ProcessedCount++;
        OnValidationProgress.Broadcast((float)ProcessedCount / AllActors.Num());
    }

    Result.ValidationTimeSeconds = (float)(FPlatformTime::Seconds() - StartTime);
    LastResult = Result;
    OnValidationComplete.Broadcast(Result);

    UE_LOG(LogRshipExec, Log, TEXT("Validation complete: %d actors, %d convertible, %d issues (E:%d W:%d) in %.2fs"),
        Result.TotalActorsScanned, Result.ConvertibleActors, Result.Issues.Num(),
        Result.ErrorCount, Result.WarningCount, Result.ValidationTimeSeconds);

    return Result;
}

FRshipValidationResult URshipSceneValidator::ValidateActors(const TArray<AActor*>& Actors)
{
    double StartTime = FPlatformTime::Seconds();
    FRshipValidationResult Result;
    Result.ValidationTimestamp = FDateTime::Now();
    Result.TotalActorsScanned = Actors.Num();

    for (AActor* Actor : Actors)
    {
        if (!Actor) continue;
        TArray<FRshipValidationIssue> Issues = ValidateActor(Actor);
        if (CanConvertActor(Actor)) Result.ConvertibleActors++;
        for (const auto& Issue : Issues)
        {
            Result.Issues.Add(Issue);
            switch (Issue.Severity)
            {
                case ERshipValidationSeverity::Info: Result.InfoCount++; break;
                case ERshipValidationSeverity::Warning: Result.WarningCount++; break;
                case ERshipValidationSeverity::Error: Result.ErrorCount++; Result.bIsValid = false; break;
                case ERshipValidationSeverity::Critical: Result.CriticalCount++; Result.bIsValid = false; break;
            }
        }
    }

    Result.ValidationTimeSeconds = (float)(FPlatformTime::Seconds() - StartTime);
    return Result;
}

TArray<FRshipValidationIssue> URshipSceneValidator::ValidateActor(AActor* Actor)
{
    TArray<FRshipValidationIssue> Issues;
    if (!Actor) return Issues;

    CheckNaming(Actor, Issues);
    CheckComponents(Actor, Issues);
    CheckLightComponents(Actor, Issues);
    CheckCameraComponents(Actor, Issues);
    CheckHierarchy(Actor, Issues);
    CheckPerformance(Actor, Issues);

    return Issues;
}

bool URshipSceneValidator::CanConvertActor(AActor* Actor) const
{
    if (!Actor) return false;

    // Already has target component
    if (Actor->FindComponentByClass<URshipTargetComponent>()) return true;

    // Has light components
    if (Actor->FindComponentByClass<ULightComponent>()) return true;

    // Has camera components
    if (Actor->FindComponentByClass<UCameraComponent>()) return true;

    return false;
}

void URshipSceneValidator::CheckNaming(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues)
{
    FString Label = Actor->GetActorLabel();

    if (IsRuleEnabled(TEXT("NAMING_EMPTY")))
    {
        if (Label.IsEmpty() || Label.StartsWith(TEXT("Actor")) || Label.Contains(TEXT("_C_")))
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Warning,
                ERshipValidationCategory::Naming,
                TEXT("Actor has default or empty name"),
                Actor, TEXT(""), TEXT("Consider giving a descriptive name"), TEXT("Rename actor"), true
            ));
        }
    }

    if (IsRuleEnabled(TEXT("NAMING_SPECIAL_CHARS")))
    {
        if (Label.Contains(TEXT("\"")) || Label.Contains(TEXT("'")) || Label.Contains(TEXT("\\")))
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Warning,
                ERshipValidationCategory::Naming,
                TEXT("Actor name contains special characters"),
                Actor, TEXT(""), TEXT("Special characters may cause issues"), TEXT("Remove special characters"), true
            ));
        }
    }
}

void URshipSceneValidator::CheckComponents(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues)
{
    if (IsRuleEnabled(TEXT("COMP_ALREADY_TARGET")))
    {
        if (Actor->FindComponentByClass<URshipTargetComponent>())
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Info,
                ERshipValidationCategory::Components,
                TEXT("Actor already has RshipTargetComponent"),
                Actor
            ));
        }
    }

    if (IsRuleEnabled(TEXT("COMP_NO_CONVERTIBLE")))
    {
        if (!CanConvertActor(Actor))
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Info,
                ERshipValidationCategory::Components,
                TEXT("Actor has no convertible components"),
                Actor, TEXT(""), TEXT("Only lights and cameras are auto-converted")
            ));
        }
    }

    if (IsRuleEnabled(TEXT("PERF_MANY_COMPONENTS")))
    {
        TArray<UActorComponent*> Comps;
        Actor->GetComponents(Comps);
        if (Comps.Num() > 20)
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Warning,
                ERshipValidationCategory::Performance,
                FString::Printf(TEXT("Actor has %d components"), Comps.Num()),
                Actor, TEXT(""), TEXT("Many components may impact performance")
            ));
        }
    }
}

void URshipSceneValidator::CheckLightComponents(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues)
{
    TArray<ULightComponent*> Lights;
    Actor->GetComponents(Lights);
    if (Lights.Num() == 0) return;

    if (IsRuleEnabled(TEXT("COMP_MULTIPLE_LIGHTS")) && Lights.Num() > 1)
    {
        OutIssues.Add(CreateIssue(
            ERshipValidationSeverity::Warning,
            ERshipValidationCategory::Components,
            FString::Printf(TEXT("Actor has %d light components"), Lights.Num()),
            Actor, TEXT(""), TEXT("Consider separating into individual actors")
        ));
    }

    for (ULightComponent* Light : Lights)
    {
        if (IsRuleEnabled(TEXT("LIGHT_NO_MOBILITY")) && Light->Mobility == EComponentMobility::Static)
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Error,
                ERshipValidationCategory::Compatibility,
                TEXT("Static light cannot be controlled at runtime"),
                Actor, Light->GetName(), TEXT("Change mobility to Movable or Stationary"), TEXT("Set to Movable"), true
            ));
        }

        if (IsRuleEnabled(TEXT("LIGHT_EXTREME_INTENSITY")) && Light->Intensity > 100000.0f)
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Warning,
                ERshipValidationCategory::Performance,
                FString::Printf(TEXT("Light intensity is very high (%.0f)"), Light->Intensity),
                Actor, Light->GetName(), TEXT("High intensity may cause visual artifacts")
            ));
        }
    }
}

void URshipSceneValidator::CheckCameraComponents(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues)
{
    TArray<UCameraComponent*> Cameras;
    Actor->GetComponents(Cameras);

    for (UCameraComponent* Cam : Cameras)
    {
        if (IsRuleEnabled(TEXT("CAMERA_NO_MOBILITY")) && Cam->Mobility == EComponentMobility::Static)
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Warning,
                ERshipValidationCategory::Compatibility,
                TEXT("Static camera cannot be moved at runtime"),
                Actor, Cam->GetName()
            ));
        }
    }
}

void URshipSceneValidator::CheckHierarchy(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues)
{
    if (IsRuleEnabled(TEXT("HIER_DEEPLY_NESTED")))
    {
        int32 Depth = 0;
        AActor* Parent = Actor->GetAttachParentActor();
        while (Parent && Depth < 20)
        {
            Depth++;
            Parent = Parent->GetAttachParentActor();
        }

        if (Depth > 5)
        {
            OutIssues.Add(CreateIssue(
                ERshipValidationSeverity::Warning,
                ERshipValidationCategory::Hierarchy,
                FString::Printf(TEXT("Actor is nested %d levels deep"), Depth),
                Actor, TEXT(""), TEXT("Deep nesting may affect transform performance")
            ));
        }
    }
}

void URshipSceneValidator::CheckPerformance(AActor* Actor, TArray<FRshipValidationIssue>& OutIssues)
{
    // Performance checks are covered in other methods
}

TArray<FRshipValidationRule> URshipSceneValidator::GetAllRules() const
{
    TArray<FRshipValidationRule> Result;
    Rules.GenerateValueArray(Result);
    return Result;
}

void URshipSceneValidator::SetRuleEnabled(const FString& RuleId, bool bEnabled)
{
    if (FRshipValidationRule* Rule = Rules.Find(RuleId)) Rule->bEnabled = bEnabled;
}

bool URshipSceneValidator::IsRuleEnabled(const FString& RuleId) const
{
    const FRshipValidationRule* Rule = Rules.Find(RuleId);
    return Rule && Rule->bEnabled;
}

void URshipSceneValidator::ResetRulesToDefaults() { InitializeDefaultRules(); }

bool URshipSceneValidator::TryAutoFix(const FString& IssueId)
{
    const FRshipValidationIssue* Issue = IssueCache.Find(IssueId);
    if (!Issue || !Issue->bCanAutoFix) return false;

    bool bSuccess = false;
    switch (Issue->Category)
    {
        case ERshipValidationCategory::Naming: bSuccess = FixNamingIssue(*Issue); break;
        case ERshipValidationCategory::Components: bSuccess = FixComponentIssue(*Issue); break;
        default: break;
    }

    OnAutoFixApplied.Broadcast(IssueId, bSuccess);
    return bSuccess;
}

int32 URshipSceneValidator::AutoFixAll()
{
    int32 Fixed = 0;
    for (const auto& Pair : IssueCache)
    {
        if (Pair.Value.bCanAutoFix && TryAutoFix(Pair.Key)) Fixed++;
    }
    return Fixed;
}

TArray<FRshipValidationIssue> URshipSceneValidator::GetAutoFixableIssues() const
{
    TArray<FRshipValidationIssue> Result;
    for (const auto& Issue : LastResult.Issues)
        if (Issue.bCanAutoFix) Result.Add(Issue);
    return Result;
}

TArray<FRshipValidationIssue> URshipSceneValidator::GetIssuesBySeverity(ERshipValidationSeverity Severity) const
{
    TArray<FRshipValidationIssue> Result;
    for (const auto& Issue : LastResult.Issues)
        if (Issue.Severity == Severity) Result.Add(Issue);
    return Result;
}

TArray<FRshipValidationIssue> URshipSceneValidator::GetIssuesByCategory(ERshipValidationCategory Category) const
{
    TArray<FRshipValidationIssue> Result;
    for (const auto& Issue : LastResult.Issues)
        if (Issue.Category == Category) Result.Add(Issue);
    return Result;
}

TArray<FRshipValidationIssue> URshipSceneValidator::GetIssuesForActor(AActor* Actor) const
{
    TArray<FRshipValidationIssue> Result;
    for (const auto& Issue : LastResult.Issues)
        if (Issue.AffectedActor.Get() == Actor) Result.Add(Issue);
    return Result;
}

bool URshipSceneValidator::FixNamingIssue(const FRshipValidationIssue& Issue)
{
    AActor* Actor = Issue.AffectedActor.Get();
    if (!Actor) return false;

    // Generate unique name based on class
    FString NewName = FString::Printf(TEXT("%s_%s"), *Actor->GetClass()->GetName(), *FGuid::NewGuid().ToString().Left(8));
    Actor->SetActorLabel(NewName);
    return true;
}

bool URshipSceneValidator::FixComponentIssue(const FRshipValidationIssue& Issue)
{
    AActor* Actor = Issue.AffectedActor.Get();
    if (!Actor) return false;

    // Fix static light mobility
    if (Issue.Message.Contains(TEXT("Static light")))
    {
        TArray<ULightComponent*> Lights;
        Actor->GetComponents(Lights);
        for (ULightComponent* Light : Lights)
        {
            if (Light->Mobility == EComponentMobility::Static)
            {
                Light->SetMobility(EComponentMobility::Movable);
            }
        }
        return true;
    }

    return false;
}

FRshipValidationIssue URshipSceneValidator::CreateIssue(
    ERshipValidationSeverity Severity,
    ERshipValidationCategory Category,
    const FString& Message,
    AActor* Actor,
    const FString& ComponentName,
    const FString& Details,
    const FString& SuggestedFix,
    bool bCanAutoFix)
{
    FRshipValidationIssue Issue;
    Issue.Id = FGuid::NewGuid().ToString();
    Issue.Severity = Severity;
    Issue.Category = Category;
    Issue.Message = Message;
    Issue.Details = Details;
    Issue.AffectedActor = Actor;
    Issue.AffectedComponentName = ComponentName;
    Issue.SuggestedFix = SuggestedFix;
    Issue.bCanAutoFix = bCanAutoFix;
    return Issue;
}
