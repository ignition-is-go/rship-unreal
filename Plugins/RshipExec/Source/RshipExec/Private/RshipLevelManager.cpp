// Rocketship Level Manager Implementation

#include "RshipLevelManager.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"

URshipLevelManager::URshipLevelManager()
{
}

void URshipLevelManager::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;

	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("RshipLevelManager: Cannot initialize without subsystem"));
		return;
	}

	UWorld* World = Subsystem->GetWorld();
	if (World)
	{
		// Bind to level events
		LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &URshipLevelManager::OnLevelAdded);
		LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &URshipLevelManager::OnLevelRemoved);

		// Process any already-loaded levels
		if (World->PersistentLevel)
		{
			ProcessedLevels.Add(World->PersistentLevel);
		}

		for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			if (StreamingLevel && StreamingLevel->GetLoadedLevel())
			{
				ProcessedLevels.Add(StreamingLevel->GetLoadedLevel());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Initialized"));
}

void URshipLevelManager::Shutdown()
{
	// Unbind from events
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);

	ProcessedLevels.Empty();
	Subsystem = nullptr;

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Shutdown"));
}

// ============================================================================
// LEVEL QUERIES
// ============================================================================

TArray<FRshipLevelInfo> URshipLevelManager::GetAllLevels()
{
	TArray<FRshipLevelInfo> Result;

	if (!Subsystem) return Result;

	UWorld* World = Subsystem->GetWorld();
	if (!World) return Result;

	// Persistent level
	if (World->PersistentLevel)
	{
		FRshipLevelInfo Info;
		Info.LevelName = World->GetOutermost()->GetName();
		Info.DisplayName = FPackageName::GetShortName(Info.LevelName);
		Info.bIsLoaded = true;
		Info.bIsVisible = true;
		Info.bIsPersistent = true;

		// Count targets
		TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(Info.LevelName);
		Info.TargetCount = Targets.Num();

		Result.Add(Info);
	}

	// Streaming levels
	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel) continue;

		FRshipLevelInfo Info;
		Info.LevelName = StreamingLevel->GetWorldAssetPackageName();
		Info.DisplayName = FPackageName::GetShortName(Info.LevelName);
		Info.bIsLoaded = StreamingLevel->IsLevelLoaded();
		Info.bIsVisible = StreamingLevel->IsLevelVisible();
		Info.bIsPersistent = false;

		if (Info.bIsLoaded)
		{
			TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(Info.LevelName);
			Info.TargetCount = Targets.Num();
		}

		Result.Add(Info);
	}

	return Result;
}

FRshipLevelInfo URshipLevelManager::GetLevelInfo(const FString& LevelName)
{
	FRshipLevelInfo Info;
	Info.LevelName = LevelName;
	Info.DisplayName = FPackageName::GetShortName(LevelName);

	if (!Subsystem) return Info;

	UWorld* World = Subsystem->GetWorld();
	if (!World) return Info;

	// Check if it's the persistent level
	FString PersistentName = World->GetOutermost()->GetName();
	if (PersistentName.Contains(LevelName) || LevelName.Contains(PersistentName))
	{
		Info.bIsPersistent = true;
		Info.bIsLoaded = true;
		Info.bIsVisible = true;

		TArray<URshipTargetComponent*> Targets = GetTargetsInPersistentLevel();
		Info.TargetCount = Targets.Num();
		return Info;
	}

	// Check streaming levels
	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel) continue;

		FString StreamingName = StreamingLevel->GetWorldAssetPackageName();
		if (StreamingName.Contains(LevelName) || LevelName.Contains(StreamingName))
		{
			Info.bIsLoaded = StreamingLevel->IsLevelLoaded();
			Info.bIsVisible = StreamingLevel->IsLevelVisible();

			if (Info.bIsLoaded)
			{
				TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(StreamingName);
				Info.TargetCount = Targets.Num();
			}
			break;
		}
	}

	return Info;
}

TArray<URshipTargetComponent*> URshipLevelManager::GetTargetsInLevel(const FString& LevelName)
{
	TArray<URshipTargetComponent*> Result;

	if (!Subsystem || !Subsystem->TargetComponents) return Result;

	FString ShortName = GetLevelShortName(LevelName);

	for (auto& Pair : *Subsystem->TargetComponents)
	{
		URshipTargetComponent* Comp = Pair.Value;
		if (!Comp || !Comp->GetOwner()) continue;

		ULevel* OwnerLevel = Comp->GetOwner()->GetLevel();
		if (!OwnerLevel) continue;

		FString OwnerLevelName = OwnerLevel->GetOutermost()->GetName();
		FString OwnerShortName = GetLevelShortName(OwnerLevelName);

		if (OwnerShortName == ShortName || OwnerLevelName.Contains(LevelName) || LevelName.Contains(OwnerLevelName))
		{
			Result.Add(Comp);
		}
	}

	return Result;
}

TArray<URshipTargetComponent*> URshipLevelManager::GetTargetsInPersistentLevel()
{
	TArray<URshipTargetComponent*> Result;

	if (!Subsystem || !Subsystem->TargetComponents) return Result;

	UWorld* World = Subsystem->GetWorld();
	if (!World || !World->PersistentLevel) return Result;

	for (auto& Pair : *Subsystem->TargetComponents)
	{
		URshipTargetComponent* Comp = Pair.Value;
		if (!Comp || !Comp->GetOwner()) continue;

		if (Comp->GetOwner()->GetLevel() == World->PersistentLevel)
		{
			Result.Add(Comp);
		}
	}

	return Result;
}

TArray<URshipTargetComponent*> URshipLevelManager::GetTargetsInStreamingLevels()
{
	TArray<URshipTargetComponent*> Result;

	if (!Subsystem || !Subsystem->TargetComponents) return Result;

	UWorld* World = Subsystem->GetWorld();
	if (!World) return Result;

	for (auto& Pair : *Subsystem->TargetComponents)
	{
		URshipTargetComponent* Comp = Pair.Value;
		if (!Comp || !Comp->GetOwner()) continue;

		ULevel* OwnerLevel = Comp->GetOwner()->GetLevel();
		if (OwnerLevel && OwnerLevel != World->PersistentLevel)
		{
			Result.Add(Comp);
		}
	}

	return Result;
}

FString URshipLevelManager::GetTargetLevel(URshipTargetComponent* Target)
{
	if (!Target || !Target->GetOwner()) return FString();

	ULevel* Level = Target->GetOwner()->GetLevel();
	if (!Level) return FString();

	return Level->GetOutermost()->GetName();
}

bool URshipLevelManager::IsLevelLoaded(const FString& LevelName)
{
	FRshipLevelInfo Info = GetLevelInfo(LevelName);
	return Info.bIsLoaded;
}

bool URshipLevelManager::IsLevelVisible(const FString& LevelName)
{
	FRshipLevelInfo Info = GetLevelInfo(LevelName);
	return Info.bIsVisible;
}

// ============================================================================
// LEVEL ACTIONS
// ============================================================================

int32 URshipLevelManager::ReregisterTargetsInLevel(const FString& LevelName)
{
	TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(LevelName);

	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			Target->Register();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Re-registered %d targets in level '%s'"),
		Targets.Num(), *LevelName);

	return Targets.Num();
}

int32 URshipLevelManager::SetLevelTargetsOffline(const FString& LevelName)
{
	TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(LevelName);

	if (Subsystem)
	{
		for (URshipTargetComponent* Target : Targets)
		{
			if (Target && Target->TargetData)
			{
				// Send offline status to rship server
				Subsystem->SendTargetStatus(Target->TargetData, false);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Set %d targets offline in level '%s'"),
		Targets.Num(), *LevelName);

	return Targets.Num();
}

int32 URshipLevelManager::AddTagToLevelTargets(const FString& LevelName, const FString& Tag)
{
	TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(LevelName);
	int32 Count = 0;

	for (URshipTargetComponent* Target : Targets)
	{
		if (Target && !Target->HasTag(Tag))
		{
			Target->Tags.Add(Tag);
			Count++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Added tag '%s' to %d targets in level '%s'"),
		*Tag, Count, *LevelName);

	return Count;
}

int32 URshipLevelManager::RemoveTagFromLevelTargets(const FString& LevelName, const FString& Tag)
{
	TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(LevelName);
	int32 Count = 0;

	FString NormalizedTag = Tag.ToLower().TrimStartAndEnd();

	for (URshipTargetComponent* Target : Targets)
	{
		if (!Target) continue;

		for (int32 i = Target->Tags.Num() - 1; i >= 0; i--)
		{
			if (Target->Tags[i].ToLower().TrimStartAndEnd() == NormalizedTag)
			{
				Target->Tags.RemoveAt(i);
				Count++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Removed tag '%s' from %d targets in level '%s'"),
		*Tag, Count, *LevelName);

	return Count;
}

// ============================================================================
// AUTO-TAGGING
// ============================================================================

void URshipLevelManager::SetAutoLevelTagging(bool bEnabled)
{
	if (bAutoLevelTagging == bEnabled) return;

	bAutoLevelTagging = bEnabled;

	if (!Subsystem || !Subsystem->TargetComponents) return;

	if (bEnabled)
	{
		// Apply level tags to all existing targets
		for (auto& Pair : *Subsystem->TargetComponents)
		{
			URshipTargetComponent* Comp = Pair.Value;
			if (Comp)
			{
				FString LevelName = GetTargetLevel(Comp);
				if (!LevelName.IsEmpty())
				{
					ApplyAutoLevelTag(Comp, GetLevelShortName(LevelName));
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Auto level tagging enabled"));
	}
	else
	{
		// Remove level tags from all targets
		for (auto& Pair : *Subsystem->TargetComponents)
		{
			if (Pair.Value)
			{
				RemoveAutoLevelTag(Pair.Value);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Auto level tagging disabled"));
	}
}

void URshipLevelManager::SetAutoLevelTagPrefix(const FString& Prefix)
{
	if (AutoLevelTagPrefix == Prefix) return;

	// Remove old tags and apply new ones if enabled
	if (bAutoLevelTagging && Subsystem && Subsystem->TargetComponents)
	{
		for (auto& Pair : *Subsystem->TargetComponents)
		{
			if (Pair.Value)
			{
				RemoveAutoLevelTag(Pair.Value);
			}
		}

		AutoLevelTagPrefix = Prefix;

		for (auto& Pair : *Subsystem->TargetComponents)
		{
			URshipTargetComponent* Comp = Pair.Value;
			if (Comp)
			{
				FString LevelName = GetTargetLevel(Comp);
				if (!LevelName.IsEmpty())
				{
					ApplyAutoLevelTag(Comp, GetLevelShortName(LevelName));
				}
			}
		}
	}
	else
	{
		AutoLevelTagPrefix = Prefix;
	}
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void URshipLevelManager::OnLevelAdded(ULevel* Level, UWorld* World)
{
	if (!Level || !World || !Subsystem) return;

	// Verify this is our world
	if (World != Subsystem->GetWorld()) return;

	// Skip if already processed
	if (ProcessedLevels.Contains(Level)) return;

	ProcessedLevels.Add(Level);

	FString LevelName = Level->GetOutermost()->GetName();
	FString ShortName = GetLevelShortName(LevelName);

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Level added - %s"), *ShortName);

	// Register targets after a short delay to ensure components are ready
	RegisterLevelTargets(Level);

	// Count targets
	TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(LevelName);
	int32 Count = Targets.Num();

	OnLevelLoaded.Broadcast(ShortName, Count);
}

void URshipLevelManager::OnLevelRemoved(ULevel* Level, UWorld* World)
{
	if (!Level || !World || !Subsystem) return;

	// Verify this is our world
	if (World != Subsystem->GetWorld()) return;

	FString LevelName = Level->GetOutermost()->GetName();
	FString ShortName = GetLevelShortName(LevelName);

	// Count targets before removal
	TArray<URshipTargetComponent*> Targets = GetTargetsInLevel(LevelName);
	int32 Count = Targets.Num();

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Level removed - %s (%d targets)"), *ShortName, Count);

	// Unregister targets
	UnregisterLevelTargets(Level);

	ProcessedLevels.Remove(Level);

	OnLevelUnloaded.Broadcast(ShortName, Count);
}

void URshipLevelManager::OnLevelVisibilityChange(UWorld* World, const ULevelStreaming* LevelStreaming, bool bIsVisible)
{
	if (!LevelStreaming || !World || !Subsystem) return;

	// Verify this is our world
	if (World != Subsystem->GetWorld()) return;

	FString LevelName = LevelStreaming->GetWorldAssetPackageName();
	FString ShortName = GetLevelShortName(LevelName);

	UE_LOG(LogTemp, Log, TEXT("RshipLevelManager: Level visibility changed - %s, visible=%d"),
		*ShortName, bIsVisible);

	OnLevelVisibilityChanged.Broadcast(ShortName, bIsVisible);
}

// ============================================================================
// INTERNAL
// ============================================================================

void URshipLevelManager::RegisterLevelTargets(ULevel* Level)
{
	if (!Level || !Subsystem || !Subsystem->TargetComponents) return;

	FString LevelName = Level->GetOutermost()->GetName();
	FString ShortName = GetLevelShortName(LevelName);

	for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
	{
		if (!Comp || !Comp->GetOwner()) continue;

		if (Comp->GetOwner()->GetLevel() == Level)
		{
			// Apply auto level tag if enabled
			if (bAutoLevelTagging)
			{
				ApplyAutoLevelTag(Comp, ShortName);
			}

			// Re-register with server
			Comp->Register();
		}
	}
}

void URshipLevelManager::UnregisterLevelTargets(ULevel* Level)
{
	if (!Level || !Subsystem || !Subsystem->TargetComponents) return;

	for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
	{
		if (!Comp || !Comp->GetOwner()) continue;

		if (Comp->GetOwner()->GetLevel() == Level)
		{
			// Remove auto level tag
			if (bAutoLevelTagging)
			{
				RemoveAutoLevelTag(Comp);
			}

			// TODO: Implement unregister from server
			// URshipTargetComponent doesn't have an Unregister method yet
			// Comp->Unregister();
		}
	}
}

FString URshipLevelManager::GetLevelShortName(const FString& LevelPath) const
{
	// Extract just the level name from the full path
	// e.g., "/Game/Maps/MyLevel" -> "MyLevel"
	return FPackageName::GetShortName(LevelPath);
}

void URshipLevelManager::ApplyAutoLevelTag(URshipTargetComponent* Target, const FString& LevelName)
{
	if (!Target) return;

	FString LevelTag = AutoLevelTagPrefix + LevelName;

	if (!Target->HasTag(LevelTag))
	{
		Target->Tags.Add(LevelTag);
	}
}

void URshipLevelManager::RemoveAutoLevelTag(URshipTargetComponent* Target)
{
	if (!Target) return;

	// Remove any tags starting with the auto prefix
	for (int32 i = Target->Tags.Num() - 1; i >= 0; i--)
	{
		if (Target->Tags[i].StartsWith(AutoLevelTagPrefix))
		{
			Target->Tags.RemoveAt(i);
		}
	}
}
