// Rocketship Preset Manager Implementation

#include "RshipPresetManager.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipTargetGroup.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

URshipPresetManager::URshipPresetManager()
{
}

void URshipPresetManager::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	UE_LOG(LogTemp, Log, TEXT("RshipPresetManager: Initialized"));
}

void URshipPresetManager::Shutdown()
{
	StopInterpolation();
	EmitterValueCache.Empty();
	Subsystem = nullptr;
	UE_LOG(LogTemp, Log, TEXT("RshipPresetManager: Shutdown"));
}

// ============================================================================
// CAPTURE
// ============================================================================

FRshipPreset URshipPresetManager::CapturePreset(const FString& Name, const TArray<URshipTargetComponent*>& Targets)
{
	FRshipPreset Preset;
	Preset.PresetId = GeneratePresetId();
	Preset.DisplayName = Name;
	Preset.CreatedAt = FDateTime::Now();
	Preset.ModifiedAt = FDateTime::Now();

	for (URshipTargetComponent* Target : Targets)
	{
		if (!Target || !Target->TargetData) continue;

		// Capture each emitter on this target
		for (const auto& EmitterPair : Target->TargetData->GetEmitters())
		{
			FString EmitterId = EmitterPair.Key;
			FString EmitterName = EmitterPair.Value->GetName();

			// Get cached value
			FString CacheKey = Target->targetName + TEXT(":") + EmitterName;
			TSharedPtr<FJsonObject>* CachedValue = EmitterValueCache.Find(CacheKey);

			if (CachedValue && CachedValue->IsValid())
			{
				FRshipEmitterSnapshot Snapshot;
				Snapshot.EmitterId = EmitterId;
				Snapshot.TargetId = Target->targetName;
				Snapshot.EmitterName = EmitterName;
				Snapshot.CapturedAt = FDateTime::Now();

				// Serialize JSON to string
				FString JsonString;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
				FJsonSerializer::Serialize(CachedValue->ToSharedRef(), Writer);
				Snapshot.ValuesJson = JsonString;

				Preset.Snapshots.Add(Snapshot);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipPresets: Captured preset '%s' with %d emitter snapshots"),
		*Name, Preset.Snapshots.Num());

	return Preset;
}

FRshipPreset URshipPresetManager::CapturePresetByTag(const FString& Name, const FString& Tag)
{
	if (!Subsystem) return FRshipPreset();

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	if (!GroupManager) return FRshipPreset();

	TArray<URshipTargetComponent*> Targets = GroupManager->GetTargetsByTag(Tag);
	return CapturePreset(Name, Targets);
}

FRshipPreset URshipPresetManager::CapturePresetByGroup(const FString& Name, const FString& GroupId)
{
	if (!Subsystem) return FRshipPreset();

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	if (!GroupManager) return FRshipPreset();

	TArray<URshipTargetComponent*> Targets = GroupManager->GetTargetsByGroup(GroupId);
	return CapturePreset(Name, Targets);
}

FRshipPreset URshipPresetManager::CapturePresetAll(const FString& Name)
{
	if (!Subsystem || !Subsystem->TargetComponents) return FRshipPreset();

	TArray<URshipTargetComponent*> Targets;
	for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
	{
		if (Comp) Targets.Add(Comp);
	}

	return CapturePreset(Name, Targets);
}

// ============================================================================
// RECALL
// ============================================================================

void URshipPresetManager::RecallPreset(const FRshipPreset& Preset)
{
	StopInterpolation();

	for (const FRshipEmitterSnapshot& Snapshot : Preset.Snapshots)
	{
		ApplySnapshot(Snapshot);
	}

	OnPresetRecalled.Broadcast(Preset.PresetId);

	UE_LOG(LogTemp, Log, TEXT("RshipPresets: Recalled preset '%s' (%d snapshots)"),
		*Preset.DisplayName, Preset.Snapshots.Num());
}

void URshipPresetManager::RecallPresetById(const FString& PresetId)
{
	FRshipPreset Preset;
	if (GetPreset(PresetId, Preset))
	{
		RecallPreset(Preset);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipPresets: Preset '%s' not found"), *PresetId);
	}
}

void URshipPresetManager::RecallPresetWithFade(const FRshipPreset& Preset, float FadeTimeSeconds)
{
	if (FadeTimeSeconds <= 0.0f)
	{
		RecallPreset(Preset);
		return;
	}

	// Capture current state as "from" preset
	TArray<URshipTargetComponent*> TargetsToCapture;
	if (Subsystem && Subsystem->TargetComponents)
	{
		for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
		{
			if (Comp) TargetsToCapture.Add(Comp);
		}
	}

	InterpolationFromPreset = CapturePreset(TEXT("__interpolation_from__"), TargetsToCapture);
	InterpolationToPreset = Preset;

	// Start interpolation
	bIsInterpolating = true;
	InterpolationProgress = 0.0f;
	InterpolationDuration = FadeTimeSeconds;
	InterpolationElapsed = 0.0f;

	// Start timer
	if (Subsystem)
	{
		if (UWorld* World = Subsystem->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InterpolationTimerHandle,
				this,
				&URshipPresetManager::TickInterpolation,
				0.033f,  // ~30Hz
				true
			);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipPresets: Starting fade to '%s' over %.2fs"),
		*Preset.DisplayName, FadeTimeSeconds);
}

void URshipPresetManager::CrossfadePresets(const FRshipPreset& FromPreset, const FRshipPreset& ToPreset, float DurationSeconds)
{
	if (DurationSeconds <= 0.0f)
	{
		RecallPreset(ToPreset);
		return;
	}

	InterpolationFromPreset = FromPreset;
	InterpolationToPreset = ToPreset;

	bIsInterpolating = true;
	InterpolationProgress = 0.0f;
	InterpolationDuration = DurationSeconds;
	InterpolationElapsed = 0.0f;

	// Start timer
	if (Subsystem)
	{
		if (UWorld* World = Subsystem->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InterpolationTimerHandle,
				this,
				&URshipPresetManager::TickInterpolation,
				0.033f,
				true
			);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipPresets: Crossfading from '%s' to '%s' over %.2fs"),
		*FromPreset.DisplayName, *ToPreset.DisplayName, DurationSeconds);
}

void URshipPresetManager::StopInterpolation()
{
	if (bIsInterpolating)
	{
		bIsInterpolating = false;

		if (Subsystem)
		{
			if (UWorld* World = Subsystem->GetWorld())
			{
				World->GetTimerManager().ClearTimer(InterpolationTimerHandle);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("RshipPresets: Interpolation stopped at %.0f%%"),
			InterpolationProgress * 100.0f);
	}
}

// ============================================================================
// PRESET MANAGEMENT
// ============================================================================

void URshipPresetManager::SavePreset(const FRshipPreset& Preset)
{
	FRshipPreset SavedPreset = Preset;
	SavedPreset.ModifiedAt = FDateTime::Now();

	Presets.Add(Preset.PresetId, SavedPreset);

	UE_LOG(LogTemp, Log, TEXT("RshipPresets: Saved preset '%s' (ID: %s)"),
		*Preset.DisplayName, *Preset.PresetId);
}

bool URshipPresetManager::DeletePreset(const FString& PresetId)
{
	if (Presets.Remove(PresetId) > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("RshipPresets: Deleted preset '%s'"), *PresetId);
		return true;
	}
	return false;
}

bool URshipPresetManager::GetPreset(const FString& PresetId, FRshipPreset& OutPreset) const
{
	const FRshipPreset* Found = Presets.Find(PresetId);
	if (Found)
	{
		OutPreset = *Found;
		return true;
	}
	return false;
}

TArray<FRshipPreset> URshipPresetManager::GetAllPresets() const
{
	TArray<FRshipPreset> Result;
	Presets.GenerateValueArray(Result);

	// Sort by name
	Result.Sort([](const FRshipPreset& A, const FRshipPreset& B) {
		return A.DisplayName < B.DisplayName;
	});

	return Result;
}

TArray<FRshipPreset> URshipPresetManager::GetPresetsByTag(const FString& Tag) const
{
	TArray<FRshipPreset> Result;

	FString NormalizedTag = Tag.ToLower().TrimStartAndEnd();

	for (const auto& Pair : Presets)
	{
		for (const FString& PresetTag : Pair.Value.Tags)
		{
			if (PresetTag.ToLower().TrimStartAndEnd() == NormalizedTag)
			{
				Result.Add(Pair.Value);
				break;
			}
		}
	}

	return Result;
}

bool URshipPresetManager::UpdatePresetMetadata(const FString& PresetId, const FString& NewName, const FString& NewDescription, const TArray<FString>& NewTags)
{
	FRshipPreset* Preset = Presets.Find(PresetId);
	if (!Preset)
	{
		return false;
	}

	Preset->DisplayName = NewName;
	Preset->Description = NewDescription;
	Preset->Tags = NewTags;
	Preset->ModifiedAt = FDateTime::Now();

	return true;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

FString URshipPresetManager::GetPresetsSaveFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("Rship") / TEXT("Presets.json");
}

bool URshipPresetManager::SavePresetsToFile()
{
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	RootObject->SetNumberField(TEXT("version"), 1);

	TArray<TSharedPtr<FJsonValue>> PresetsArray;
	for (const auto& Pair : Presets)
	{
		const FRshipPreset& Preset = Pair.Value;

		TSharedPtr<FJsonObject> PresetObj = MakeShareable(new FJsonObject);
		PresetObj->SetStringField(TEXT("presetId"), Preset.PresetId);
		PresetObj->SetStringField(TEXT("displayName"), Preset.DisplayName);
		PresetObj->SetStringField(TEXT("description"), Preset.Description);

		// Tags
		TArray<TSharedPtr<FJsonValue>> TagsArray;
		for (const FString& Tag : Preset.Tags)
		{
			TagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
		}
		PresetObj->SetArrayField(TEXT("tags"), TagsArray);

		// Snapshots
		TArray<TSharedPtr<FJsonValue>> SnapshotsArray;
		for (const FRshipEmitterSnapshot& Snapshot : Preset.Snapshots)
		{
			TSharedPtr<FJsonObject> SnapObj = MakeShareable(new FJsonObject);
			SnapObj->SetStringField(TEXT("emitterId"), Snapshot.EmitterId);
			SnapObj->SetStringField(TEXT("targetId"), Snapshot.TargetId);
			SnapObj->SetStringField(TEXT("emitterName"), Snapshot.EmitterName);
			SnapObj->SetStringField(TEXT("valuesJson"), Snapshot.ValuesJson);
			SnapshotsArray.Add(MakeShareable(new FJsonValueObject(SnapObj)));
		}
		PresetObj->SetArrayField(TEXT("snapshots"), SnapshotsArray);

		PresetsArray.Add(MakeShareable(new FJsonValueObject(PresetObj)));
	}
	RootObject->SetArrayField(TEXT("presets"), PresetsArray);

	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	FString FilePath = GetPresetsSaveFilePath();
	FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	if (FFileHelper::SaveStringToFile(OutputString, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("RshipPresets: Saved %d presets to %s"), Presets.Num(), *FilePath);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("RshipPresets: Failed to save presets to %s"), *FilePath);
	return false;
}

bool URshipPresetManager::LoadPresetsFromFile()
{
	FString FilePath = GetPresetsSaveFilePath();
	FString JsonString;

	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("RshipPresets: No saved presets file found at %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RshipPresets: Failed to parse presets JSON"));
		return false;
	}

	Presets.Empty();

	const TArray<TSharedPtr<FJsonValue>>* PresetsArray;
	if (!RootObject->TryGetArrayField(TEXT("presets"), PresetsArray))
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& PresetValue : *PresetsArray)
	{
		TSharedPtr<FJsonObject> PresetObj = PresetValue->AsObject();
		if (!PresetObj.IsValid()) continue;

		FRshipPreset Preset;
		Preset.PresetId = PresetObj->GetStringField(TEXT("presetId"));
		Preset.DisplayName = PresetObj->GetStringField(TEXT("displayName"));
		Preset.Description = PresetObj->GetStringField(TEXT("description"));

		// Tags
		const TArray<TSharedPtr<FJsonValue>>* TagsArray;
		if (PresetObj->TryGetArrayField(TEXT("tags"), TagsArray))
		{
			for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
			{
				Preset.Tags.Add(TagValue->AsString());
			}
		}

		// Snapshots
		const TArray<TSharedPtr<FJsonValue>>* SnapshotsArray;
		if (PresetObj->TryGetArrayField(TEXT("snapshots"), SnapshotsArray))
		{
			for (const TSharedPtr<FJsonValue>& SnapValue : *SnapshotsArray)
			{
				TSharedPtr<FJsonObject> SnapObj = SnapValue->AsObject();
				if (!SnapObj.IsValid()) continue;

				FRshipEmitterSnapshot Snapshot;
				Snapshot.EmitterId = SnapObj->GetStringField(TEXT("emitterId"));
				Snapshot.TargetId = SnapObj->GetStringField(TEXT("targetId"));
				Snapshot.EmitterName = SnapObj->GetStringField(TEXT("emitterName"));
				Snapshot.ValuesJson = SnapObj->GetStringField(TEXT("valuesJson"));

				Preset.Snapshots.Add(Snapshot);
			}
		}

		// Update ID counter
		if (Preset.PresetId.StartsWith(TEXT("preset_")))
		{
			FString NumPart = Preset.PresetId.Mid(7);
			int32 UnderscorePos;
			if (NumPart.FindChar('_', UnderscorePos))
			{
				NumPart = NumPart.Left(UnderscorePos);
				int32 IdNum = FCString::Atoi(*NumPart);
				PresetIdCounter = FMath::Max(PresetIdCounter, IdNum);
			}
		}

		Presets.Add(Preset.PresetId, Preset);
	}

	UE_LOG(LogTemp, Log, TEXT("RshipPresets: Loaded %d presets from %s"), Presets.Num(), *FilePath);
	return true;
}

// ============================================================================
// EMITTER VALUE CACHE
// ============================================================================

void URshipPresetManager::CacheEmitterValue(const FString& TargetId, const FString& EmitterId, TSharedPtr<FJsonObject> Values)
{
	FString CacheKey = TargetId + TEXT(":") + EmitterId;
	EmitterValueCache.Add(CacheKey, Values);
}

TSharedPtr<FJsonObject> URshipPresetManager::GetCachedEmitterValue(const FString& TargetId, const FString& EmitterId) const
{
	FString CacheKey = TargetId + TEXT(":") + EmitterId;
	const TSharedPtr<FJsonObject>* Found = EmitterValueCache.Find(CacheKey);
	return Found ? *Found : nullptr;
}

// ============================================================================
// INTERNAL
// ============================================================================

FString URshipPresetManager::GeneratePresetId() const
{
	return FString::Printf(TEXT("preset_%d_%s"), ++const_cast<URshipPresetManager*>(this)->PresetIdCounter,
		*FGuid::NewGuid().ToString(EGuidFormats::Short));
}

void URshipPresetManager::ApplySnapshot(const FRshipEmitterSnapshot& Snapshot)
{
	if (!Subsystem || !Snapshot.IsValid()) return;

	// Parse the JSON values
	TSharedPtr<FJsonObject> Values;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Snapshot.ValuesJson);
	if (!FJsonSerializer::Deserialize(Reader, Values) || !Values.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipPresets: Failed to parse snapshot values for %s"), *Snapshot.EmitterId);
		return;
	}

	// Find the target and trigger an action to set these values
	// This requires the target to have actions that can set the emitter values
	// For now, we'll update the cache and log
	CacheEmitterValue(Snapshot.TargetId, Snapshot.EmitterName, Values);

	// TODO: Actually push the values to the target via actions
	// This would require knowing the action schema and calling the appropriate setters
	UE_LOG(LogTemp, Verbose, TEXT("RshipPresets: Applied snapshot for %s:%s"),
		*Snapshot.TargetId, *Snapshot.EmitterName);
}

void URshipPresetManager::ApplyInterpolatedSnapshot(const FRshipEmitterSnapshot& From, const FRshipEmitterSnapshot& To, float Alpha)
{
	if (!From.IsValid() || !To.IsValid()) return;

	// Parse both JSON values
	TSharedPtr<FJsonObject> FromValues, ToValues;
	TSharedRef<TJsonReader<>> FromReader = TJsonReaderFactory<>::Create(From.ValuesJson);
	TSharedRef<TJsonReader<>> ToReader = TJsonReaderFactory<>::Create(To.ValuesJson);

	if (!FJsonSerializer::Deserialize(FromReader, FromValues) || !FromValues.IsValid()) return;
	if (!FJsonSerializer::Deserialize(ToReader, ToValues) || !ToValues.IsValid()) return;

	// Interpolate
	TSharedPtr<FJsonObject> Interpolated = LerpJsonObjects(FromValues, ToValues, Alpha);
	if (Interpolated.IsValid())
	{
		CacheEmitterValue(To.TargetId, To.EmitterName, Interpolated);
		// TODO: Actually push values
	}
}

TSharedPtr<FJsonObject> URshipPresetManager::LerpJsonObjects(TSharedPtr<FJsonObject> A, TSharedPtr<FJsonObject> B, float Alpha)
{
	if (!A.IsValid()) return B;
	if (!B.IsValid()) return A;

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	// Iterate through B's fields and lerp where possible
	for (const auto& Pair : B->Values)
	{
		FString FieldName = Pair.Key;
		TSharedPtr<FJsonValue> BValue = Pair.Value;
		TSharedPtr<FJsonValue> AValue = A->TryGetField(FieldName);

		if (!AValue.IsValid())
		{
			// Field only in B, use B at Alpha > 0.5
			if (Alpha > 0.5f)
			{
				Result->SetField(FieldName, BValue);
			}
			continue;
		}

		// Both have the field, try to lerp
		if (BValue->Type == EJson::Number && AValue->Type == EJson::Number)
		{
			// Numeric lerp
			double ANum = AValue->AsNumber();
			double BNum = BValue->AsNumber();
			double Lerped = FMath::Lerp(ANum, BNum, static_cast<double>(Alpha));
			Result->SetNumberField(FieldName, Lerped);
		}
		else if (BValue->Type == EJson::Boolean && AValue->Type == EJson::Boolean)
		{
			// Boolean snap at 50%
			bool Value = (Alpha > 0.5f) ? BValue->AsBool() : AValue->AsBool();
			Result->SetBoolField(FieldName, Value);
		}
		else if (BValue->Type == EJson::String && AValue->Type == EJson::String)
		{
			// String snap at 50%
			FString Value = (Alpha > 0.5f) ? BValue->AsString() : AValue->AsString();
			Result->SetStringField(FieldName, Value);
		}
		else
		{
			// Can't interpolate, use target value at Alpha > 0.5
			Result->SetField(FieldName, (Alpha > 0.5f) ? BValue : AValue);
		}
	}

	// Add any fields only in A
	for (const auto& Pair : A->Values)
	{
		if (!B->HasField(Pair.Key) && Alpha < 0.5f)
		{
			Result->SetField(Pair.Key, Pair.Value);
		}
	}

	return Result;
}

void URshipPresetManager::TickInterpolation()
{
	if (!bIsInterpolating) return;

	InterpolationElapsed += 0.033f;  // Approximate dt
	InterpolationProgress = FMath::Clamp(InterpolationElapsed / InterpolationDuration, 0.0f, 1.0f);

	// Build a map of "To" snapshots by emitter ID for quick lookup
	TMap<FString, const FRshipEmitterSnapshot*> ToSnapshots;
	for (const FRshipEmitterSnapshot& Snap : InterpolationToPreset.Snapshots)
	{
		FString Key = Snap.TargetId + TEXT(":") + Snap.EmitterName;
		ToSnapshots.Add(Key, &Snap);
	}

	// Interpolate each snapshot
	for (const FRshipEmitterSnapshot& FromSnap : InterpolationFromPreset.Snapshots)
	{
		FString Key = FromSnap.TargetId + TEXT(":") + FromSnap.EmitterName;
		const FRshipEmitterSnapshot** ToSnap = ToSnapshots.Find(Key);

		if (ToSnap && *ToSnap)
		{
			ApplyInterpolatedSnapshot(FromSnap, **ToSnap, InterpolationProgress);
		}
	}

	// Broadcast progress
	OnPresetInterpolating.Broadcast(InterpolationProgress, InterpolationToPreset.PresetId);

	// Check if done
	if (InterpolationProgress >= 1.0f)
	{
		bIsInterpolating = false;

		if (Subsystem)
		{
			if (UWorld* World = Subsystem->GetWorld())
			{
				World->GetTimerManager().ClearTimer(InterpolationTimerHandle);
			}
		}

		OnPresetRecalled.Broadcast(InterpolationToPreset.PresetId);

		UE_LOG(LogTemp, Log, TEXT("RshipPresets: Interpolation complete to '%s'"),
			*InterpolationToPreset.DisplayName);
	}
}
