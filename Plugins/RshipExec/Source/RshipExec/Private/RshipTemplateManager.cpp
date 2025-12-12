// Rocketship Template Manager Implementation

#include "RshipTemplateManager.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipTargetGroup.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

URshipTemplateManager::URshipTemplateManager()
{
}

void URshipTemplateManager::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	UE_LOG(LogTemp, Log, TEXT("RshipTemplateManager: Initialized"));
}

void URshipTemplateManager::Shutdown()
{
	AutoNameCounters.Empty();
	Subsystem = nullptr;
	UE_LOG(LogTemp, Log, TEXT("RshipTemplateManager: Shutdown"));
}

// ============================================================================
// TEMPLATE CREATION
// ============================================================================

FRshipTargetTemplate URshipTemplateManager::CreateTemplate(const FString& Name, const FString& Description)
{
	FRshipTargetTemplate Template;
	Template.TemplateId = GenerateTemplateId();
	Template.DisplayName = Name;
	Template.Description = Description;
	Template.CreatedAt = FDateTime::Now();
	Template.ModifiedAt = FDateTime::Now();

	return Template;
}

FRshipTargetTemplate URshipTemplateManager::CreateTemplateFromTarget(const FString& Name, URshipTargetComponent* SourceTarget)
{
	FRshipTargetTemplate Template = CreateTemplate(Name);

	if (SourceTarget)
	{
		Template.Tags = SourceTarget->Tags;
		Template.GroupIds = SourceTarget->GroupIds;

		// Extract prefix/suffix from target name if possible
		FString TargetName = SourceTarget->targetName;

		// Try to detect numeric suffix
		int32 LastDigitIndex = TargetName.Len() - 1;
		while (LastDigitIndex >= 0 && FChar::IsDigit(TargetName[LastDigitIndex]))
		{
			LastDigitIndex--;
		}

		if (LastDigitIndex < TargetName.Len() - 1 && LastDigitIndex >= 0)
		{
			Template.NamePrefix = TargetName.Left(LastDigitIndex + 1);
			Template.bAutoGenerateName = true;
		}

		UE_LOG(LogTemp, Log, TEXT("RshipTemplates: Created template '%s' from target '%s'"),
			*Name, *TargetName);
	}

	return Template;
}

FRshipTargetTemplate URshipTemplateManager::CreateTemplateFromTargets(const FString& Name, const TArray<URshipTargetComponent*>& SourceTargets)
{
	FRshipTargetTemplate Template = CreateTemplate(Name);

	if (SourceTargets.Num() == 0)
	{
		return Template;
	}

	// Find common tags across all targets
	TSet<FString> CommonTags;
	bool bFirstTarget = true;

	for (URshipTargetComponent* Target : SourceTargets)
	{
		if (!Target) continue;

		TSet<FString> TargetTags;
		for (const FString& Tag : Target->Tags)
		{
			TargetTags.Add(Tag.ToLower().TrimStartAndEnd());
		}

		if (bFirstTarget)
		{
			CommonTags = TargetTags;
			bFirstTarget = false;
		}
		else
		{
			CommonTags = CommonTags.Intersect(TargetTags);
		}
	}

	for (const FString& Tag : CommonTags)
	{
		Template.Tags.Add(Tag);
	}

	// Find common groups
	TSet<FString> CommonGroups;
	bFirstTarget = true;

	for (URshipTargetComponent* Target : SourceTargets)
	{
		if (!Target) continue;

		TSet<FString> TargetGroups;
		for (const FString& GroupId : Target->GroupIds)
		{
			TargetGroups.Add(GroupId);
		}

		if (bFirstTarget)
		{
			CommonGroups = TargetGroups;
			bFirstTarget = false;
		}
		else
		{
			CommonGroups = CommonGroups.Intersect(TargetGroups);
		}
	}

	for (const FString& GroupId : CommonGroups)
	{
		Template.GroupIds.Add(GroupId);
	}

	UE_LOG(LogTemp, Log, TEXT("RshipTemplates: Created template '%s' from %d targets (%d common tags, %d common groups)"),
		*Name, SourceTargets.Num(), Template.Tags.Num(), Template.GroupIds.Num());

	return Template;
}

// ============================================================================
// TEMPLATE APPLICATION
// ============================================================================

void URshipTemplateManager::ApplyTemplate(const FRshipTargetTemplate& Template, URshipTargetComponent* Target, bool bMergeTags)
{
	if (!Target)
	{
		return;
	}

	// Apply tags
	if (bMergeTags)
	{
		// Merge tags
		for (const FString& Tag : Template.Tags)
		{
			if (!Target->HasTag(Tag))
			{
				Target->Tags.Add(Tag);
			}
		}
	}
	else
	{
		// Replace tags
		Target->Tags = Template.Tags;
	}

	// Apply groups
	if (Subsystem)
	{
		URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
		if (GroupManager)
		{
			for (const FString& GroupId : Template.GroupIds)
			{
				GroupManager->AddTargetToGroup(Target, GroupId);
			}
		}
	}

	// Apply auto-generated name if enabled
	if (Template.bAutoGenerateName)
	{
		Target->targetName = GenerateTargetName(Template);
	}
	else if (!Template.NamePrefix.IsEmpty() || !Template.NameSuffix.IsEmpty())
	{
		// Apply prefix/suffix to existing name
		FString NewName = Template.NamePrefix + Target->targetName + Template.NameSuffix;
		Target->targetName = NewName;
	}

	// Update use count
	FRshipTargetTemplate* SavedTemplate = Templates.Find(Template.TemplateId);
	if (SavedTemplate)
	{
		SavedTemplate->UseCount++;
		SavedTemplate->ModifiedAt = FDateTime::Now();
	}

	OnTemplateApplied.Broadcast(Template.TemplateId, Target);

	UE_LOG(LogTemp, Verbose, TEXT("RshipTemplates: Applied template '%s' to target '%s'"),
		*Template.DisplayName, *Target->targetName);
}

int32 URshipTemplateManager::ApplyTemplateToTargets(const FRshipTargetTemplate& Template, const TArray<URshipTargetComponent*>& Targets, bool bMergeTags)
{
	int32 Count = 0;
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			ApplyTemplate(Template, Target, bMergeTags);
			Count++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipTemplates: Applied template '%s' to %d targets"),
		*Template.DisplayName, Count);

	return Count;
}

void URshipTemplateManager::ApplyTemplateById(const FString& TemplateId, URshipTargetComponent* Target, bool bMergeTags)
{
	FRshipTargetTemplate Template;
	if (GetTemplate(TemplateId, Template))
	{
		ApplyTemplate(Template, Target, bMergeTags);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTemplates: Template '%s' not found"), *TemplateId);
	}
}

int32 URshipTemplateManager::ApplyTemplateToTaggedTargets(const FString& TemplateId, const FString& Tag, bool bMergeTags)
{
	if (!Subsystem) return 0;

	FRshipTargetTemplate Template;
	if (!GetTemplate(TemplateId, Template))
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTemplates: Template '%s' not found"), *TemplateId);
		return 0;
	}

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	if (!GroupManager) return 0;

	TArray<URshipTargetComponent*> Targets = GroupManager->GetTargetsByTag(Tag);
	return ApplyTemplateToTargets(Template, Targets, bMergeTags);
}

// ============================================================================
// TEMPLATE MANAGEMENT
// ============================================================================

void URshipTemplateManager::SaveTemplate(const FRshipTargetTemplate& Template)
{
	FRshipTargetTemplate SavedTemplate = Template;
	SavedTemplate.ModifiedAt = FDateTime::Now();

	Templates.Add(Template.TemplateId, SavedTemplate);

	UE_LOG(LogTemp, Log, TEXT("RshipTemplates: Saved template '%s' (ID: %s)"),
		*Template.DisplayName, *Template.TemplateId);
}

bool URshipTemplateManager::DeleteTemplate(const FString& TemplateId)
{
	if (Templates.Remove(TemplateId) > 0)
	{
		AutoNameCounters.Remove(TemplateId);
		UE_LOG(LogTemp, Log, TEXT("RshipTemplates: Deleted template '%s'"), *TemplateId);
		return true;
	}
	return false;
}

bool URshipTemplateManager::GetTemplate(const FString& TemplateId, FRshipTargetTemplate& OutTemplate) const
{
	const FRshipTargetTemplate* Found = Templates.Find(TemplateId);
	if (Found)
	{
		OutTemplate = *Found;
		return true;
	}
	return false;
}

TArray<FRshipTargetTemplate> URshipTemplateManager::GetAllTemplates() const
{
	TArray<FRshipTargetTemplate> Result;
	Templates.GenerateValueArray(Result);

	// Sort by category then name
	Result.Sort([](const FRshipTargetTemplate& A, const FRshipTargetTemplate& B) {
		if (A.Category != B.Category)
		{
			return A.Category < B.Category;
		}
		return A.DisplayName < B.DisplayName;
	});

	return Result;
}

TArray<FRshipTargetTemplate> URshipTemplateManager::GetTemplatesByCategory(const FString& Category) const
{
	TArray<FRshipTargetTemplate> Result;

	FString NormalizedCategory = Category.ToLower().TrimStartAndEnd();

	for (const auto& Pair : Templates)
	{
		if (Pair.Value.Category.ToLower().TrimStartAndEnd() == NormalizedCategory)
		{
			Result.Add(Pair.Value);
		}
	}

	// Sort by name
	Result.Sort([](const FRshipTargetTemplate& A, const FRshipTargetTemplate& B) {
		return A.DisplayName < B.DisplayName;
	});

	return Result;
}

TArray<FString> URshipTemplateManager::GetAllCategories() const
{
	TSet<FString> Categories;

	for (const auto& Pair : Templates)
	{
		if (!Pair.Value.Category.IsEmpty())
		{
			Categories.Add(Pair.Value.Category);
		}
	}

	TArray<FString> Result = Categories.Array();
	Result.Sort();
	return Result;
}

bool URshipTemplateManager::UpdateTemplate(const FRshipTargetTemplate& Template)
{
	FRshipTargetTemplate* Existing = Templates.Find(Template.TemplateId);
	if (!Existing)
	{
		return false;
	}

	// Preserve use count and creation date
	int32 UseCount = Existing->UseCount;
	FDateTime CreatedAt = Existing->CreatedAt;

	*Existing = Template;
	Existing->UseCount = UseCount;
	Existing->CreatedAt = CreatedAt;
	Existing->ModifiedAt = FDateTime::Now();

	return true;
}

FRshipTargetTemplate URshipTemplateManager::DuplicateTemplate(const FString& SourceTemplateId, const FString& NewName)
{
	FRshipTargetTemplate SourceTemplate;
	if (!GetTemplate(SourceTemplateId, SourceTemplate))
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTemplates: Source template '%s' not found"), *SourceTemplateId);
		return FRshipTargetTemplate();
	}

	FRshipTargetTemplate NewTemplate = SourceTemplate;
	NewTemplate.TemplateId = GenerateTemplateId();
	NewTemplate.DisplayName = NewName;
	NewTemplate.CreatedAt = FDateTime::Now();
	NewTemplate.ModifiedAt = FDateTime::Now();
	NewTemplate.UseCount = 0;

	UE_LOG(LogTemp, Log, TEXT("RshipTemplates: Duplicated template '%s' as '%s'"),
		*SourceTemplate.DisplayName, *NewName);

	return NewTemplate;
}

// ============================================================================
// AUTO-NAMING
// ============================================================================

FString URshipTemplateManager::GenerateTargetName(const FRshipTargetTemplate& Template)
{
	int32& Counter = AutoNameCounters.FindOrAdd(Template.TemplateId);
	Counter++;

	FString Name = Template.NamePrefix;
	Name += FString::Printf(TEXT("%03d"), Counter);
	Name += Template.NameSuffix;

	return Name;
}

void URshipTemplateManager::ResetAutoNameCounter(const FString& TemplateId)
{
	AutoNameCounters.Remove(TemplateId);
}

// ============================================================================
// PERSISTENCE
// ============================================================================

FString URshipTemplateManager::GetTemplatesSaveFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("Rship") / TEXT("Templates.json");
}

bool URshipTemplateManager::SaveTemplatesToFile()
{
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	RootObject->SetNumberField(TEXT("version"), 1);

	TArray<TSharedPtr<FJsonValue>> TemplatesArray;
	for (const auto& Pair : Templates)
	{
		const FRshipTargetTemplate& Template = Pair.Value;

		TSharedPtr<FJsonObject> TemplateObj = MakeShareable(new FJsonObject);
		TemplateObj->SetStringField(TEXT("templateId"), Template.TemplateId);
		TemplateObj->SetStringField(TEXT("displayName"), Template.DisplayName);
		TemplateObj->SetStringField(TEXT("description"), Template.Description);
		TemplateObj->SetStringField(TEXT("category"), Template.Category);
		TemplateObj->SetStringField(TEXT("namePrefix"), Template.NamePrefix);
		TemplateObj->SetStringField(TEXT("nameSuffix"), Template.NameSuffix);
		TemplateObj->SetBoolField(TEXT("autoGenerateName"), Template.bAutoGenerateName);
		TemplateObj->SetNumberField(TEXT("useCount"), Template.UseCount);

		// Tags
		TArray<TSharedPtr<FJsonValue>> TagsArray;
		for (const FString& Tag : Template.Tags)
		{
			TagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
		}
		TemplateObj->SetArrayField(TEXT("tags"), TagsArray);

		// Groups
		TArray<TSharedPtr<FJsonValue>> GroupsArray;
		for (const FString& GroupId : Template.GroupIds)
		{
			GroupsArray.Add(MakeShareable(new FJsonValueString(GroupId)));
		}
		TemplateObj->SetArrayField(TEXT("groupIds"), GroupsArray);

		TemplatesArray.Add(MakeShareable(new FJsonValueObject(TemplateObj)));
	}
	RootObject->SetArrayField(TEXT("templates"), TemplatesArray);

	// Save auto-name counters
	TSharedPtr<FJsonObject> CountersObj = MakeShareable(new FJsonObject);
	for (const auto& Pair : AutoNameCounters)
	{
		CountersObj->SetNumberField(Pair.Key, Pair.Value);
	}
	RootObject->SetObjectField(TEXT("autoNameCounters"), CountersObj);

	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	FString FilePath = GetTemplatesSaveFilePath();
	FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	if (FFileHelper::SaveStringToFile(OutputString, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("RshipTemplates: Saved %d templates to %s"), Templates.Num(), *FilePath);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("RshipTemplates: Failed to save templates to %s"), *FilePath);
	return false;
}

bool URshipTemplateManager::LoadTemplatesFromFile()
{
	FString FilePath = GetTemplatesSaveFilePath();
	FString JsonString;

	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("RshipTemplates: No saved templates file found at %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RshipTemplates: Failed to parse templates JSON"));
		return false;
	}

	Templates.Empty();
	AutoNameCounters.Empty();

	// Load templates
	const TArray<TSharedPtr<FJsonValue>>* TemplatesArray;
	if (RootObject->TryGetArrayField(TEXT("templates"), TemplatesArray))
	{
		for (const TSharedPtr<FJsonValue>& TemplateValue : *TemplatesArray)
		{
			TSharedPtr<FJsonObject> TemplateObj = TemplateValue->AsObject();
			if (!TemplateObj.IsValid()) continue;

			FRshipTargetTemplate Template;
			Template.TemplateId = TemplateObj->GetStringField(TEXT("templateId"));
			Template.DisplayName = TemplateObj->GetStringField(TEXT("displayName"));
			Template.Description = TemplateObj->GetStringField(TEXT("description"));
			Template.Category = TemplateObj->GetStringField(TEXT("category"));
			Template.NamePrefix = TemplateObj->GetStringField(TEXT("namePrefix"));
			Template.NameSuffix = TemplateObj->GetStringField(TEXT("nameSuffix"));
			Template.bAutoGenerateName = TemplateObj->GetBoolField(TEXT("autoGenerateName"));
			Template.UseCount = TemplateObj->GetIntegerField(TEXT("useCount"));

			// Tags
			const TArray<TSharedPtr<FJsonValue>>* TagsArray;
			if (TemplateObj->TryGetArrayField(TEXT("tags"), TagsArray))
			{
				for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
				{
					Template.Tags.Add(TagValue->AsString());
				}
			}

			// Groups
			const TArray<TSharedPtr<FJsonValue>>* GroupsArray;
			if (TemplateObj->TryGetArrayField(TEXT("groupIds"), GroupsArray))
			{
				for (const TSharedPtr<FJsonValue>& GroupValue : *GroupsArray)
				{
					Template.GroupIds.Add(GroupValue->AsString());
				}
			}

			// Update ID counter
			if (Template.TemplateId.StartsWith(TEXT("template_")))
			{
				FString NumPart = Template.TemplateId.Mid(9);
				int32 UnderscorePos;
				if (NumPart.FindChar('_', UnderscorePos))
				{
					NumPart = NumPart.Left(UnderscorePos);
					int32 IdNum = FCString::Atoi(*NumPart);
					TemplateIdCounter = FMath::Max(TemplateIdCounter, IdNum);
				}
			}

			Templates.Add(Template.TemplateId, Template);
		}
	}

	// Load auto-name counters
	const TSharedPtr<FJsonObject>* CountersObj;
	if (RootObject->TryGetObjectField(TEXT("autoNameCounters"), CountersObj))
	{
		for (const auto& Pair : (*CountersObj)->Values)
		{
			AutoNameCounters.Add(Pair.Key, static_cast<int32>(Pair.Value->AsNumber()));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipTemplates: Loaded %d templates from %s"), Templates.Num(), *FilePath);
	return true;
}

// ============================================================================
// INTERNAL
// ============================================================================

FString URshipTemplateManager::GenerateTemplateId() const
{
	return FString::Printf(TEXT("template_%d_%s"), ++const_cast<URshipTemplateManager*>(this)->TemplateIdCounter,
		*FGuid::NewGuid().ToString(EGuidFormats::Short));
}
