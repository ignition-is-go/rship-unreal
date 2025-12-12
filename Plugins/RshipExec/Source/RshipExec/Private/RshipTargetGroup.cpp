// Rocketship Target Group Management Implementation

#include "RshipTargetGroup.h"
#include "RshipTargetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

URshipTargetGroupManager::URshipTargetGroupManager()
{
}

// ============================================================================
// GROUP CRUD OPERATIONS
// ============================================================================

FRshipTargetGroup URshipTargetGroupManager::CreateGroup(const FString& DisplayName, FLinearColor Color)
{
	FRshipTargetGroup NewGroup;
	NewGroup.GroupId = GenerateGroupId();
	NewGroup.DisplayName = DisplayName;
	NewGroup.Color = Color;
	NewGroup.CreatedAt = FDateTime::Now();
	NewGroup.ModifiedAt = FDateTime::Now();

	Groups.Add(NewGroup.GroupId, NewGroup);

	OnGroupChanged.Broadcast(NewGroup.GroupId);

	UE_LOG(LogTemp, Log, TEXT("RshipGroups: Created group '%s' (ID: %s)"), *DisplayName, *NewGroup.GroupId);
	return NewGroup;
}

bool URshipTargetGroupManager::DeleteGroup(const FString& GroupId)
{
	FRshipTargetGroup* Group = Groups.Find(GroupId);
	if (!Group)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipGroups: Cannot delete group '%s' - not found"), *GroupId);
		return false;
	}

	// Remove from reverse index for all member targets
	for (const FString& TargetId : Group->TargetIds)
	{
		UpdateTargetToGroupsIndex(TargetId, GroupId, false);
	}

	Groups.Remove(GroupId);

	OnGroupChanged.Broadcast(GroupId);

	UE_LOG(LogTemp, Log, TEXT("RshipGroups: Deleted group '%s'"), *GroupId);
	return true;
}

bool URshipTargetGroupManager::GetGroup(const FString& GroupId, FRshipTargetGroup& OutGroup) const
{
	const FRshipTargetGroup* Group = Groups.Find(GroupId);
	if (Group)
	{
		OutGroup = *Group;
		return true;
	}
	return false;
}

bool URshipTargetGroupManager::UpdateGroup(const FRshipTargetGroup& Group)
{
	if (!Groups.Contains(Group.GroupId))
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipGroups: Cannot update group '%s' - not found"), *Group.GroupId);
		return false;
	}

	FRshipTargetGroup UpdatedGroup = Group;
	UpdatedGroup.ModifiedAt = FDateTime::Now();
	Groups.Add(Group.GroupId, UpdatedGroup);

	OnGroupChanged.Broadcast(Group.GroupId);

	return true;
}

TArray<FRshipTargetGroup> URshipTargetGroupManager::GetAllGroups() const
{
	TArray<FRshipTargetGroup> Result;
	Groups.GenerateValueArray(Result);
	return Result;
}

// ============================================================================
// GROUP MEMBERSHIP OPERATIONS
// ============================================================================

bool URshipTargetGroupManager::AddTargetToGroup(const FString& TargetId, const FString& GroupId)
{
	FRshipTargetGroup* Group = Groups.Find(GroupId);
	if (!Group)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipGroups: Cannot add target to group '%s' - group not found"), *GroupId);
		return false;
	}

	// Check if already a member (idempotent)
	if (Group->TargetIds.Contains(TargetId))
	{
		return true;
	}

	Group->TargetIds.Add(TargetId);
	Group->ModifiedAt = FDateTime::Now();

	UpdateTargetToGroupsIndex(TargetId, GroupId, true);

	OnGroupChanged.Broadcast(GroupId);

	return true;
}

bool URshipTargetGroupManager::RemoveTargetFromGroup(const FString& TargetId, const FString& GroupId)
{
	FRshipTargetGroup* Group = Groups.Find(GroupId);
	if (!Group)
	{
		return false;
	}

	if (Group->TargetIds.Remove(TargetId) > 0)
	{
		Group->ModifiedAt = FDateTime::Now();
		UpdateTargetToGroupsIndex(TargetId, GroupId, false);
		OnGroupChanged.Broadcast(GroupId);
		return true;
	}

	return false;
}

TArray<FString> URshipTargetGroupManager::GetGroupsForTarget(const FString& TargetId) const
{
	TArray<FString> Result;
	const TSet<FString>* GroupIds = TargetToGroups.Find(TargetId);
	if (GroupIds)
	{
		Result = GroupIds->Array();
	}
	return Result;
}

// ============================================================================
// TAG OPERATIONS
// ============================================================================

void URshipTargetGroupManager::AddTagToTarget(URshipTargetComponent* Target, const FString& Tag)
{
	if (!Target || Tag.IsEmpty())
	{
		return;
	}

	FString NormalizedTag = NormalizeTag(Tag);
	if (NormalizedTag.IsEmpty())
	{
		return;
	}

	// Check if target already has this tag
	if (Target->Tags.Contains(NormalizedTag))
	{
		return; // Idempotent
	}

	Target->Tags.Add(NormalizedTag);

	// Update reverse index
	TSet<FString>& Targets = TagToTargets.FindOrAdd(NormalizedTag);
	Targets.Add(Target->targetName);

	OnTargetTagsChanged.Broadcast(Target, Target->Tags);
}

void URshipTargetGroupManager::RemoveTagFromTarget(URshipTargetComponent* Target, const FString& Tag)
{
	if (!Target || Tag.IsEmpty())
	{
		return;
	}

	FString NormalizedTag = NormalizeTag(Tag);

	if (Target->Tags.Remove(NormalizedTag) > 0)
	{
		// Update reverse index
		TSet<FString>* Targets = TagToTargets.Find(NormalizedTag);
		if (Targets)
		{
			Targets->Remove(Target->targetName);
			if (Targets->Num() == 0)
			{
				TagToTargets.Remove(NormalizedTag);
			}
		}

		OnTargetTagsChanged.Broadcast(Target, Target->Tags);
	}
}

TArray<FString> URshipTargetGroupManager::GetAllTags() const
{
	TArray<FString> Result;
	TagToTargets.GenerateKeyArray(Result);
	Result.Sort();
	return Result;
}

bool URshipTargetGroupManager::TagExists(const FString& Tag) const
{
	return TagToTargets.Contains(NormalizeTag(Tag));
}

// ============================================================================
// QUERY OPERATIONS
// ============================================================================

TArray<URshipTargetComponent*> URshipTargetGroupManager::GetTargetsByTag(const FString& Tag) const
{
	TArray<URshipTargetComponent*> Result;
	FString NormalizedTag = NormalizeTag(Tag);

	const TSet<FString>* TargetIds = TagToTargets.Find(NormalizedTag);
	if (!TargetIds)
	{
		return Result;
	}

	for (const FString& TargetId : *TargetIds)
	{
		const TWeakObjectPtr<URshipTargetComponent>* WeakPtr = RegisteredTargets.Find(TargetId);
		if (WeakPtr && WeakPtr->IsValid())
		{
			Result.Add(WeakPtr->Get());
		}
	}

	return Result;
}

TArray<URshipTargetComponent*> URshipTargetGroupManager::GetTargetsByGroup(const FString& GroupId) const
{
	TArray<URshipTargetComponent*> Result;

	const FRshipTargetGroup* Group = Groups.Find(GroupId);
	if (!Group)
	{
		return Result;
	}

	for (const FString& TargetId : Group->TargetIds)
	{
		const TWeakObjectPtr<URshipTargetComponent>* WeakPtr = RegisteredTargets.Find(TargetId);
		if (WeakPtr && WeakPtr->IsValid())
		{
			Result.Add(WeakPtr->Get());
		}
	}

	return Result;
}

TArray<URshipTargetComponent*> URshipTargetGroupManager::GetTargetsByPattern(const FString& WildcardPattern) const
{
	TArray<URshipTargetComponent*> Result;

	for (const auto& Pair : RegisteredTargets)
	{
		if (Pair.Value.IsValid() && MatchesWildcard(Pair.Key, WildcardPattern))
		{
			Result.Add(Pair.Value.Get());
		}
	}

	return Result;
}

TArray<URshipTargetComponent*> URshipTargetGroupManager::GetTargetsByTags(const TArray<FString>& Tags) const
{
	if (Tags.Num() == 0)
	{
		return TArray<URshipTargetComponent*>();
	}

	// Start with targets matching the first tag
	TSet<FString> MatchingTargetIds;
	FString FirstTag = NormalizeTag(Tags[0]);
	const TSet<FString>* FirstSet = TagToTargets.Find(FirstTag);
	if (!FirstSet)
	{
		return TArray<URshipTargetComponent*>();
	}
	MatchingTargetIds = *FirstSet;

	// Intersect with remaining tags
	for (int32 i = 1; i < Tags.Num(); ++i)
	{
		FString Tag = NormalizeTag(Tags[i]);
		const TSet<FString>* TagSet = TagToTargets.Find(Tag);
		if (!TagSet)
		{
			return TArray<URshipTargetComponent*>(); // No targets have all tags
		}
		MatchingTargetIds = MatchingTargetIds.Intersect(*TagSet);
		if (MatchingTargetIds.Num() == 0)
		{
			return TArray<URshipTargetComponent*>();
		}
	}

	// Convert IDs to components
	TArray<URshipTargetComponent*> Result;
	for (const FString& TargetId : MatchingTargetIds)
	{
		const TWeakObjectPtr<URshipTargetComponent>* WeakPtr = RegisteredTargets.Find(TargetId);
		if (WeakPtr && WeakPtr->IsValid())
		{
			Result.Add(WeakPtr->Get());
		}
	}

	return Result;
}

TArray<URshipTargetComponent*> URshipTargetGroupManager::GetTargetsByAnyTag(const TArray<FString>& Tags) const
{
	TSet<FString> MatchingTargetIds;

	for (const FString& Tag : Tags)
	{
		FString NormalizedTag = NormalizeTag(Tag);
		const TSet<FString>* TagSet = TagToTargets.Find(NormalizedTag);
		if (TagSet)
		{
			MatchingTargetIds.Append(*TagSet);
		}
	}

	// Convert IDs to components
	TArray<URshipTargetComponent*> Result;
	for (const FString& TargetId : MatchingTargetIds)
	{
		const TWeakObjectPtr<URshipTargetComponent>* WeakPtr = RegisteredTargets.Find(TargetId);
		if (WeakPtr && WeakPtr->IsValid())
		{
			Result.Add(WeakPtr->Get());
		}
	}

	return Result;
}

// ============================================================================
// AUTO-GROUPING HELPERS
// ============================================================================

FRshipTargetGroup URshipTargetGroupManager::CreateGroupFromActorClass(TSubclassOf<AActor> ActorClass, const FString& GroupName)
{
	FString Name = GroupName.IsEmpty() ? ActorClass->GetName() : GroupName;
	FRshipTargetGroup NewGroup = CreateGroup(Name, FLinearColor::MakeRandomColor());

	// Find all targets on actors of this class
	for (const auto& Pair : RegisteredTargets)
	{
		if (Pair.Value.IsValid())
		{
			URshipTargetComponent* Target = Pair.Value.Get();
			if (Target->GetOwner() && Target->GetOwner()->IsA(ActorClass))
			{
				AddTargetToGroup(Pair.Key, NewGroup.GroupId);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipGroups: Created group '%s' from actor class '%s' with %d targets"),
		*Name, *ActorClass->GetName(), NewGroup.TargetIds.Num());

	return NewGroup;
}

FRshipTargetGroup URshipTargetGroupManager::CreateGroupFromProximity(FVector Center, float Radius, const FString& GroupName)
{
	FString Name = GroupName.IsEmpty() ? FString::Printf(TEXT("Proximity_%.0f"), Radius) : GroupName;
	FRshipTargetGroup NewGroup = CreateGroup(Name, FLinearColor::MakeRandomColor());

	float RadiusSq = Radius * Radius;

	for (const auto& Pair : RegisteredTargets)
	{
		if (Pair.Value.IsValid())
		{
			URshipTargetComponent* Target = Pair.Value.Get();
			if (Target->GetOwner())
			{
				float DistSq = FVector::DistSquared(Target->GetOwner()->GetActorLocation(), Center);
				if (DistSq <= RadiusSq)
				{
					AddTargetToGroup(Pair.Key, NewGroup.GroupId);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipGroups: Created proximity group '%s' (radius %.0f) with %d targets"),
		*Name, Radius, NewGroup.TargetIds.Num());

	return NewGroup;
}

// ============================================================================
// INTERNAL INDEX MANAGEMENT
// ============================================================================

void URshipTargetGroupManager::RegisterTarget(URshipTargetComponent* Target)
{
	if (!Target || Target->targetName.IsEmpty())
	{
		return;
	}

	RegisteredTargets.Add(Target->targetName, Target);

	// Index existing tags
	for (const FString& Tag : Target->Tags)
	{
		FString NormalizedTag = NormalizeTag(Tag);
		if (!NormalizedTag.IsEmpty())
		{
			TSet<FString>& Targets = TagToTargets.FindOrAdd(NormalizedTag);
			Targets.Add(Target->targetName);
		}
	}

	// Check auto-populate groups
	for (auto& Pair : Groups)
	{
		FRshipTargetGroup& Group = Pair.Value;
		if (Group.bAutoPopulate && !Group.AutoPopulatePattern.IsEmpty())
		{
			if (MatchesWildcard(Target->targetName, Group.AutoPopulatePattern))
			{
				AddTargetToGroup(Target->targetName, Group.GroupId);
			}
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("RshipGroups: Registered target '%s'"), *Target->targetName);
}

void URshipTargetGroupManager::UnregisterTarget(URshipTargetComponent* Target)
{
	if (!Target || Target->targetName.IsEmpty())
	{
		return;
	}

	// Remove from tag index
	for (const FString& Tag : Target->Tags)
	{
		FString NormalizedTag = NormalizeTag(Tag);
		TSet<FString>* Targets = TagToTargets.Find(NormalizedTag);
		if (Targets)
		{
			Targets->Remove(Target->targetName);
			if (Targets->Num() == 0)
			{
				TagToTargets.Remove(NormalizedTag);
			}
		}
	}

	// Remove from all groups
	TSet<FString>* GroupIds = TargetToGroups.Find(Target->targetName);
	if (GroupIds)
	{
		for (const FString& GroupId : *GroupIds)
		{
			FRshipTargetGroup* Group = Groups.Find(GroupId);
			if (Group)
			{
				Group->TargetIds.Remove(Target->targetName);
				Group->ModifiedAt = FDateTime::Now();
			}
		}
		TargetToGroups.Remove(Target->targetName);
	}

	RegisteredTargets.Remove(Target->targetName);

	UE_LOG(LogTemp, Verbose, TEXT("RshipGroups: Unregistered target '%s'"), *Target->targetName);
}

void URshipTargetGroupManager::RebuildIndices()
{
	TagToTargets.Empty();
	TargetToGroups.Empty();

	// Rebuild tag index
	for (const auto& Pair : RegisteredTargets)
	{
		if (Pair.Value.IsValid())
		{
			URshipTargetComponent* Target = Pair.Value.Get();
			for (const FString& Tag : Target->Tags)
			{
				FString NormalizedTag = NormalizeTag(Tag);
				if (!NormalizedTag.IsEmpty())
				{
					TSet<FString>& Targets = TagToTargets.FindOrAdd(NormalizedTag);
					Targets.Add(Target->targetName);
				}
			}
		}
	}

	// Rebuild target-to-groups index
	for (const auto& Pair : Groups)
	{
		const FRshipTargetGroup& Group = Pair.Value;
		for (const FString& TargetId : Group.TargetIds)
		{
			TSet<FString>& GroupIds = TargetToGroups.FindOrAdd(TargetId);
			GroupIds.Add(Group.GroupId);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipGroups: Rebuilt indices (%d tags, %d groups, %d targets)"),
		TagToTargets.Num(), Groups.Num(), RegisteredTargets.Num());
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

FString URshipTargetGroupManager::GenerateGroupId() const
{
	return FString::Printf(TEXT("group_%d_%s"), ++const_cast<URshipTargetGroupManager*>(this)->GroupIdCounter,
		*FGuid::NewGuid().ToString(EGuidFormats::Short));
}

void URshipTargetGroupManager::UpdateTargetToGroupsIndex(const FString& TargetId, const FString& GroupId, bool bAdd)
{
	if (bAdd)
	{
		TSet<FString>& GroupIds = TargetToGroups.FindOrAdd(TargetId);
		GroupIds.Add(GroupId);
	}
	else
	{
		TSet<FString>* GroupIds = TargetToGroups.Find(TargetId);
		if (GroupIds)
		{
			GroupIds->Remove(GroupId);
			if (GroupIds->Num() == 0)
			{
				TargetToGroups.Remove(TargetId);
			}
		}
	}
}

bool URshipTargetGroupManager::MatchesWildcard(const FString& TargetId, const FString& Pattern)
{
	// Simple wildcard matching using * for any characters
	// Convert to regex-style pattern for FString::MatchesWildcard
	return TargetId.MatchesWildcard(Pattern, ESearchCase::IgnoreCase);
}

FString URshipTargetGroupManager::NormalizeTag(const FString& Tag)
{
	// Trim whitespace, convert to lowercase, limit length
	FString Normalized = Tag.TrimStartAndEnd().ToLower();
	if (Normalized.Len() > 64)
	{
		Normalized = Normalized.Left(64);
		UE_LOG(LogTemp, Warning, TEXT("RshipGroups: Tag truncated to 64 characters: '%s'"), *Normalized);
	}
	return Normalized;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

FString URshipTargetGroupManager::GetGroupsSaveFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("Rship") / TEXT("TargetGroups.json");
}

bool URshipTargetGroupManager::SaveGroupsToFile()
{
	FString JsonString = ExportGroupsToJson();
	if (JsonString.IsEmpty())
	{
		return false;
	}

	FString FilePath = GetGroupsSaveFilePath();

	// Ensure directory exists
	FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	if (FFileHelper::SaveStringToFile(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("RshipGroups: Saved %d groups to %s"), Groups.Num(), *FilePath);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("RshipGroups: Failed to save groups to %s"), *FilePath);
	return false;
}

bool URshipTargetGroupManager::LoadGroupsFromFile()
{
	FString FilePath = GetGroupsSaveFilePath();

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("RshipGroups: No saved groups file found at %s"), *FilePath);
		return false;
	}

	if (ImportGroupsFromJson(JsonString))
	{
		UE_LOG(LogTemp, Log, TEXT("RshipGroups: Loaded %d groups from %s"), Groups.Num(), *FilePath);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("RshipGroups: Failed to load groups from %s"), *FilePath);
	return false;
}

FString URshipTargetGroupManager::ExportGroupsToJson() const
{
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

	// Version for future compatibility
	RootObject->SetNumberField(TEXT("version"), 1);

	// Export groups
	TArray<TSharedPtr<FJsonValue>> GroupsArray;
	for (const auto& Pair : Groups)
	{
		const FRshipTargetGroup& Group = Pair.Value;

		TSharedPtr<FJsonObject> GroupObject = MakeShareable(new FJsonObject);
		GroupObject->SetStringField(TEXT("groupId"), Group.GroupId);
		GroupObject->SetStringField(TEXT("displayName"), Group.DisplayName);

		// Color as array [R, G, B, A]
		TArray<TSharedPtr<FJsonValue>> ColorArray;
		ColorArray.Add(MakeShareable(new FJsonValueNumber(Group.Color.R)));
		ColorArray.Add(MakeShareable(new FJsonValueNumber(Group.Color.G)));
		ColorArray.Add(MakeShareable(new FJsonValueNumber(Group.Color.B)));
		ColorArray.Add(MakeShareable(new FJsonValueNumber(Group.Color.A)));
		GroupObject->SetArrayField(TEXT("color"), ColorArray);

		// Target IDs
		TArray<TSharedPtr<FJsonValue>> TargetIdsArray;
		for (const FString& TargetId : Group.TargetIds)
		{
			TargetIdsArray.Add(MakeShareable(new FJsonValueString(TargetId)));
		}
		GroupObject->SetArrayField(TEXT("targetIds"), TargetIdsArray);

		// Tags
		TArray<TSharedPtr<FJsonValue>> TagsArray;
		for (const FString& Tag : Group.Tags)
		{
			TagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
		}
		GroupObject->SetArrayField(TEXT("tags"), TagsArray);

		// Auto-populate settings
		GroupObject->SetBoolField(TEXT("bAutoPopulate"), Group.bAutoPopulate);
		GroupObject->SetStringField(TEXT("autoPopulatePattern"), Group.AutoPopulatePattern);

		GroupsArray.Add(MakeShareable(new FJsonValueObject(GroupObject)));
	}
	RootObject->SetArrayField(TEXT("groups"), GroupsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	return OutputString;
}

bool URshipTargetGroupManager::ImportGroupsFromJson(const FString& JsonString)
{
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RshipGroups: Failed to parse JSON"));
		return false;
	}

	// Check version
	int32 Version = RootObject->GetIntegerField(TEXT("version"));
	if (Version > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipGroups: JSON version %d is newer than supported (1), some data may be lost"), Version);
	}

	// Clear existing groups
	Groups.Empty();
	TargetToGroups.Empty();

	// Import groups
	const TArray<TSharedPtr<FJsonValue>>* GroupsArray;
	if (!RootObject->TryGetArrayField(TEXT("groups"), GroupsArray))
	{
		return true; // Empty but valid
	}

	for (const TSharedPtr<FJsonValue>& GroupValue : *GroupsArray)
	{
		TSharedPtr<FJsonObject> GroupObject = GroupValue->AsObject();
		if (!GroupObject.IsValid())
		{
			continue;
		}

		FRshipTargetGroup Group;
		Group.GroupId = GroupObject->GetStringField(TEXT("groupId"));
		Group.DisplayName = GroupObject->GetStringField(TEXT("displayName"));

		// Parse color
		const TArray<TSharedPtr<FJsonValue>>* ColorArray;
		if (GroupObject->TryGetArrayField(TEXT("color"), ColorArray) && ColorArray->Num() >= 4)
		{
			Group.Color = FLinearColor(
				(*ColorArray)[0]->AsNumber(),
				(*ColorArray)[1]->AsNumber(),
				(*ColorArray)[2]->AsNumber(),
				(*ColorArray)[3]->AsNumber()
			);
		}

		// Parse target IDs
		const TArray<TSharedPtr<FJsonValue>>* TargetIdsArray;
		if (GroupObject->TryGetArrayField(TEXT("targetIds"), TargetIdsArray))
		{
			for (const TSharedPtr<FJsonValue>& TargetIdValue : *TargetIdsArray)
			{
				FString TargetId = TargetIdValue->AsString();
				if (!TargetId.IsEmpty())
				{
					Group.TargetIds.Add(TargetId);
				}
			}
		}

		// Parse tags
		const TArray<TSharedPtr<FJsonValue>>* TagsArray;
		if (GroupObject->TryGetArrayField(TEXT("tags"), TagsArray))
		{
			for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
			{
				FString Tag = TagValue->AsString();
				if (!Tag.IsEmpty())
				{
					Group.Tags.Add(Tag);
				}
			}
		}

		// Parse auto-populate settings
		Group.bAutoPopulate = GroupObject->GetBoolField(TEXT("bAutoPopulate"));
		Group.AutoPopulatePattern = GroupObject->GetStringField(TEXT("autoPopulatePattern"));

		// Update group ID counter
		int32 IdNum = 0;
		if (Group.GroupId.StartsWith(TEXT("group_")))
		{
			FString NumPart = Group.GroupId.Mid(6);
			int32 UnderscorePos;
			if (NumPart.FindChar('_', UnderscorePos))
			{
				NumPart = NumPart.Left(UnderscorePos);
				IdNum = FCString::Atoi(*NumPart);
				GroupIdCounter = FMath::Max(GroupIdCounter, IdNum);
			}
		}

		Groups.Add(Group.GroupId, Group);

		// Update reverse index
		for (const FString& TargetId : Group.TargetIds)
		{
			TSet<FString>& GroupIds = TargetToGroups.FindOrAdd(TargetId);
			GroupIds.Add(Group.GroupId);
		}
	}

	return true;
}
