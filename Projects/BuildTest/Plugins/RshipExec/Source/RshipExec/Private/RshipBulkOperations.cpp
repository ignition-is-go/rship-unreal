// Rocketship Bulk Operations Implementation

#include "RshipBulkOperations.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipTargetGroup.h"
#include "Engine/Engine.h"

// Static selection storage
static TSet<TWeakObjectPtr<URshipTargetComponent>> GRshipSelectedTargets;

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

TSet<TWeakObjectPtr<URshipTargetComponent>>& URshipBulkOperations::GetSelectionSet()
{
	// Clean up any invalid weak pointers
	for (auto It = GRshipSelectedTargets.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	return GRshipSelectedTargets;
}

URshipSubsystem* URshipBulkOperations::GetSubsystem()
{
	if (GEngine)
	{
		return GEngine->GetEngineSubsystem<URshipSubsystem>();
	}
	return nullptr;
}

void URshipBulkOperations::NotifySelectionChanged()
{
	// In the future, this could broadcast a delegate
	UE_LOG(LogTemp, Verbose, TEXT("RshipBulk: Selection changed, %d targets selected"), GetSelectionSet().Num());
}

// ============================================================================
// SELECTION MANAGEMENT
// ============================================================================

void URshipBulkOperations::SelectTargets(const TArray<URshipTargetComponent*>& Targets)
{
	GetSelectionSet().Empty();
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			GetSelectionSet().Add(Target);
		}
	}
	NotifySelectionChanged();
}

void URshipBulkOperations::SelectTargetsByTag(const FString& Tag)
{
	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	if (!GroupManager)
	{
		return;
	}

	TArray<URshipTargetComponent*> Targets = GroupManager->GetTargetsByTag(Tag);
	SelectTargets(Targets);
}

void URshipBulkOperations::SelectTargetsByGroup(const FString& GroupId)
{
	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	if (!GroupManager)
	{
		return;
	}

	TArray<URshipTargetComponent*> Targets = GroupManager->GetTargetsByGroup(GroupId);
	SelectTargets(Targets);
}

void URshipBulkOperations::SelectTargetsByPattern(const FString& WildcardPattern)
{
	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	if (!GroupManager)
	{
		return;
	}

	TArray<URshipTargetComponent*> Targets = GroupManager->GetTargetsByPattern(WildcardPattern);
	SelectTargets(Targets);
}

void URshipBulkOperations::AddToSelection(const TArray<URshipTargetComponent*>& Targets)
{
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			GetSelectionSet().Add(Target);
		}
	}
	NotifySelectionChanged();
}

void URshipBulkOperations::RemoveFromSelection(const TArray<URshipTargetComponent*>& Targets)
{
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			GetSelectionSet().Remove(Target);
		}
	}
	NotifySelectionChanged();
}

TArray<URshipTargetComponent*> URshipBulkOperations::GetSelectedTargets()
{
	TArray<URshipTargetComponent*> Result;
	for (const TWeakObjectPtr<URshipTargetComponent>& WeakTarget : GetSelectionSet())
	{
		if (WeakTarget.IsValid())
		{
			Result.Add(WeakTarget.Get());
		}
	}
	return Result;
}

int32 URshipBulkOperations::GetSelectionCount()
{
	return GetSelectionSet().Num();
}

bool URshipBulkOperations::HasSelection()
{
	return GetSelectionSet().Num() > 0;
}

void URshipBulkOperations::ClearSelection()
{
	GetSelectionSet().Empty();
	NotifySelectionChanged();
}

void URshipBulkOperations::SelectAll()
{
	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->TargetComponents)
	{
		return;
	}

	GetSelectionSet().Empty();
	for (auto& Pair : *Subsystem->TargetComponents)
	{
		if (Pair.Value)
		{
			GetSelectionSet().Add(Pair.Value);
		}
	}
	NotifySelectionChanged();
}

void URshipBulkOperations::InvertSelection()
{
	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->TargetComponents)
	{
		return;
	}

	TSet<TWeakObjectPtr<URshipTargetComponent>> NewSelection;
	for (auto& Pair : *Subsystem->TargetComponents)
	{
		if (Pair.Value && !GetSelectionSet().Contains(Pair.Value))
		{
			NewSelection.Add(Pair.Value);
		}
	}
	GetSelectionSet() = NewSelection;
	NotifySelectionChanged();
}

// ============================================================================
// BULK TAG OPERATIONS
// ============================================================================

int32 URshipBulkOperations::BulkAddTag(const FString& Tag)
{
	return BulkAddTagToTargets(GetSelectedTargets(), Tag);
}

int32 URshipBulkOperations::BulkAddTagToTargets(const TArray<URshipTargetComponent*>& Targets, const FString& Tag)
{
	if (Tag.IsEmpty())
	{
		return 0;
	}

	URshipSubsystem* Subsystem = GetSubsystem();
	URshipTargetGroupManager* GroupManager = Subsystem ? Subsystem->GetGroupManager() : nullptr;

	int32 ModifiedCount = 0;
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target && !Target->HasTag(Tag))
		{
			if (GroupManager)
			{
				GroupManager->AddTagToTarget(Target, Tag);
			}
			else
			{
				Target->Tags.Add(Tag);
			}
			ModifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Added tag '%s' to %d targets"), *Tag, ModifiedCount);
	return ModifiedCount;
}

int32 URshipBulkOperations::BulkRemoveTag(const FString& Tag)
{
	return BulkRemoveTagFromTargets(GetSelectedTargets(), Tag);
}

int32 URshipBulkOperations::BulkRemoveTagFromTargets(const TArray<URshipTargetComponent*>& Targets, const FString& Tag)
{
	if (Tag.IsEmpty())
	{
		return 0;
	}

	URshipSubsystem* Subsystem = GetSubsystem();
	URshipTargetGroupManager* GroupManager = Subsystem ? Subsystem->GetGroupManager() : nullptr;

	int32 ModifiedCount = 0;
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target && Target->HasTag(Tag))
		{
			if (GroupManager)
			{
				GroupManager->RemoveTagFromTarget(Target, Tag);
			}
			else
			{
				Target->Tags.Remove(Tag);
			}
			ModifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Removed tag '%s' from %d targets"), *Tag, ModifiedCount);
	return ModifiedCount;
}

int32 URshipBulkOperations::BulkReplaceTag(const FString& OldTag, const FString& NewTag)
{
	if (OldTag.IsEmpty() || NewTag.IsEmpty())
	{
		return 0;
	}

	URshipSubsystem* Subsystem = GetSubsystem();
	URshipTargetGroupManager* GroupManager = Subsystem ? Subsystem->GetGroupManager() : nullptr;

	int32 ModifiedCount = 0;
	TArray<URshipTargetComponent*> SelectedTargets = GetSelectedTargets();

	for (URshipTargetComponent* Target : SelectedTargets)
	{
		if (Target && Target->HasTag(OldTag))
		{
			if (GroupManager)
			{
				GroupManager->RemoveTagFromTarget(Target, OldTag);
				GroupManager->AddTagToTarget(Target, NewTag);
			}
			else
			{
				Target->Tags.Remove(OldTag);
				Target->Tags.AddUnique(NewTag);
			}
			ModifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Replaced tag '%s' with '%s' on %d targets"), *OldTag, *NewTag, ModifiedCount);
	return ModifiedCount;
}

int32 URshipBulkOperations::BulkClearTags()
{
	URshipSubsystem* Subsystem = GetSubsystem();
	URshipTargetGroupManager* GroupManager = Subsystem ? Subsystem->GetGroupManager() : nullptr;

	int32 ModifiedCount = 0;
	TArray<URshipTargetComponent*> SelectedTargets = GetSelectedTargets();

	for (URshipTargetComponent* Target : SelectedTargets)
	{
		if (Target && Target->Tags.Num() > 0)
		{
			if (GroupManager)
			{
				// Remove each tag through manager to keep indices in sync
				TArray<FString> TagsCopy = Target->Tags;
				for (const FString& Tag : TagsCopy)
				{
					GroupManager->RemoveTagFromTarget(Target, Tag);
				}
			}
			else
			{
				Target->Tags.Empty();
			}
			ModifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Cleared tags from %d targets"), ModifiedCount);
	return ModifiedCount;
}

// ============================================================================
// BULK GROUP OPERATIONS
// ============================================================================

int32 URshipBulkOperations::BulkAddToGroup(const FString& GroupId)
{
	if (GroupId.IsEmpty())
	{
		return 0;
	}

	URshipSubsystem* Subsystem = GetSubsystem();
	URshipTargetGroupManager* GroupManager = Subsystem ? Subsystem->GetGroupManager() : nullptr;
	if (!GroupManager)
	{
		return 0;
	}

	int32 ModifiedCount = 0;
	TArray<URshipTargetComponent*> SelectedTargets = GetSelectedTargets();

	for (URshipTargetComponent* Target : SelectedTargets)
	{
		if (Target)
		{
			if (GroupManager->AddTargetToGroup(Target->targetName, GroupId))
			{
				ModifiedCount++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Added %d targets to group '%s'"), ModifiedCount, *GroupId);
	return ModifiedCount;
}

int32 URshipBulkOperations::BulkRemoveFromGroup(const FString& GroupId)
{
	if (GroupId.IsEmpty())
	{
		return 0;
	}

	URshipSubsystem* Subsystem = GetSubsystem();
	URshipTargetGroupManager* GroupManager = Subsystem ? Subsystem->GetGroupManager() : nullptr;
	if (!GroupManager)
	{
		return 0;
	}

	int32 ModifiedCount = 0;
	TArray<URshipTargetComponent*> SelectedTargets = GetSelectedTargets();

	for (URshipTargetComponent* Target : SelectedTargets)
	{
		if (Target)
		{
			if (GroupManager->RemoveTargetFromGroup(Target->targetName, GroupId))
			{
				ModifiedCount++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Removed %d targets from group '%s'"), ModifiedCount, *GroupId);
	return ModifiedCount;
}

// ============================================================================
// BULK STATE OPERATIONS
// ============================================================================

int32 URshipBulkOperations::BulkSetEnabled(bool bEnabled)
{
	int32 ModifiedCount = 0;
	TArray<URshipTargetComponent*> SelectedTargets = GetSelectedTargets();

	for (URshipTargetComponent* Target : SelectedTargets)
	{
		if (Target && Target->IsActive() != bEnabled)
		{
			Target->SetActive(bEnabled);
			ModifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Set %d targets to %s"), ModifiedCount, bEnabled ? TEXT("enabled") : TEXT("disabled"));
	return ModifiedCount;
}

int32 URshipBulkOperations::BulkReregister()
{
	int32 ModifiedCount = 0;
	TArray<URshipTargetComponent*> SelectedTargets = GetSelectedTargets();

	for (URshipTargetComponent* Target : SelectedTargets)
	{
		if (Target)
		{
			Target->Register();
			ModifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Re-registered %d targets"), ModifiedCount);
	return ModifiedCount;
}

// ============================================================================
// COPY/PASTE CONFIGURATION
// ============================================================================

FRshipTargetConfig URshipBulkOperations::CopyTargetConfig(URshipTargetComponent* Source)
{
	FRshipTargetConfig Config;

	if (!Source)
	{
		return Config;
	}

	Config.TargetName = Source->targetName;
	Config.Tags = Source->Tags;
	Config.GroupIds = Source->GroupIds;
	Config.SourceTargetId = Source->targetName;
	Config.CapturedAt = FDateTime::Now();

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Copied config from target '%s' (%d tags, %d groups)"),
		*Source->targetName, Config.Tags.Num(), Config.GroupIds.Num());

	return Config;
}

int32 URshipBulkOperations::PasteTargetConfig(const FRshipTargetConfig& Config, bool bPasteTags, bool bPasteGroups)
{
	return PasteTargetConfigToTargets(GetSelectedTargets(), Config, bPasteTags, bPasteGroups);
}

int32 URshipBulkOperations::PasteTargetConfigToTargets(const TArray<URshipTargetComponent*>& Targets, const FRshipTargetConfig& Config, bool bPasteTags, bool bPasteGroups)
{
	if (!Config.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipBulk: Cannot paste invalid config"));
		return 0;
	}

	URshipSubsystem* Subsystem = GetSubsystem();
	URshipTargetGroupManager* GroupManager = Subsystem ? Subsystem->GetGroupManager() : nullptr;

	int32 ModifiedCount = 0;

	for (URshipTargetComponent* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}

		bool bModified = false;

		// Paste tags
		if (bPasteTags)
		{
			// Clear existing tags
			if (GroupManager)
			{
				TArray<FString> OldTags = Target->Tags;
				for (const FString& Tag : OldTags)
				{
					GroupManager->RemoveTagFromTarget(Target, Tag);
				}
				// Add new tags
				for (const FString& Tag : Config.Tags)
				{
					GroupManager->AddTagToTarget(Target, Tag);
				}
			}
			else
			{
				Target->Tags = Config.Tags;
			}
			bModified = true;
		}

		// Paste groups
		if (bPasteGroups && GroupManager)
		{
			// Remove from current groups
			TArray<FString> CurrentGroups = GroupManager->GetGroupsForTarget(Target->targetName);
			for (const FString& GroupId : CurrentGroups)
			{
				GroupManager->RemoveTargetFromGroup(Target->targetName, GroupId);
			}
			// Add to new groups
			for (const FString& GroupId : Config.GroupIds)
			{
				GroupManager->AddTargetToGroup(Target->targetName, GroupId);
			}
			bModified = true;
		}

		if (bModified)
		{
			ModifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Pasted config to %d targets (tags=%d, groups=%d)"),
		ModifiedCount, bPasteTags ? 1 : 0, bPasteGroups ? 1 : 0);

	return ModifiedCount;
}

// ============================================================================
// FIND AND REPLACE
// ============================================================================

int32 URshipBulkOperations::FindAndReplaceInTargetNames(const FString& Find, const FString& Replace, bool bCaseSensitive)
{
	if (Find.IsEmpty())
	{
		return 0;
	}

	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->TargetComponents)
	{
		return 0;
	}

	int32 ModifiedCount = 0;
	ESearchCase::Type SearchCase = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;

	for (auto& Pair : *Subsystem->TargetComponents)
	{
		URshipTargetComponent* Target = Pair.Value;
		if (!Target)
		{
			continue;
		}

		FString OldName = Target->targetName;
		FString NewName = OldName.Replace(*Find, *Replace, SearchCase);

		if (NewName != OldName)
		{
			Target->targetName = NewName;
			ModifiedCount++;
			UE_LOG(LogTemp, Verbose, TEXT("RshipBulk: Renamed '%s' -> '%s'"), *OldName, *NewName);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Find/replace in names: '%s' -> '%s', modified %d targets"),
		*Find, *Replace, ModifiedCount);

	return ModifiedCount;
}

int32 URshipBulkOperations::FindAndReplaceInTags(const FString& Find, const FString& Replace, bool bCaseSensitive)
{
	if (Find.IsEmpty())
	{
		return 0;
	}

	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->TargetComponents)
	{
		return 0;
	}

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	ESearchCase::Type SearchCase = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;

	int32 ModifiedCount = 0;

	for (auto& Pair : *Subsystem->TargetComponents)
	{
		URshipTargetComponent* Target = Pair.Value;
		if (!Target)
		{
			continue;
		}

		bool bTargetModified = false;
		TArray<FString> OldTags = Target->Tags;

		for (const FString& OldTag : OldTags)
		{
			FString NewTag = OldTag.Replace(*Find, *Replace, SearchCase);

			if (NewTag != OldTag)
			{
				if (GroupManager)
				{
					GroupManager->RemoveTagFromTarget(Target, OldTag);
					GroupManager->AddTagToTarget(Target, NewTag);
				}
				else
				{
					Target->Tags.Remove(OldTag);
					Target->Tags.AddUnique(NewTag);
				}
				bTargetModified = true;
			}
		}

		if (bTargetModified)
		{
			ModifiedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipBulk: Find/replace in tags: '%s' -> '%s', modified %d targets"),
		*Find, *Replace, ModifiedCount);

	return ModifiedCount;
}

// ============================================================================
// UTILITY
// ============================================================================

TArray<URshipTargetComponent*> URshipBulkOperations::FilterTargets(TFunction<bool(URshipTargetComponent*)> Predicate)
{
	TArray<URshipTargetComponent*> Result;

	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->TargetComponents)
	{
		return Result;
	}

	for (auto& Pair : *Subsystem->TargetComponents)
	{
		if (Pair.Value && Predicate(Pair.Value))
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}
