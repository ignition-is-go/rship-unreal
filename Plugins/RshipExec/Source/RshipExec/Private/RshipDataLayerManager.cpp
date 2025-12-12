// Rocketship Data Layer Manager Implementation

#include "RshipDataLayerManager.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipTargetGroup.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

URshipDataLayerManager::URshipDataLayerManager()
{
}

void URshipDataLayerManager::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;

	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("RshipDataLayerManager: Cannot initialize without subsystem"));
		return;
	}

	// Bind to Data Layer state changes
	UDataLayerSubsystem* DataLayerSS = GetDataLayerSubsystem();
	if (DataLayerSS)
	{
		// UE 5.6 uses OnDataLayerInstanceRuntimeStateChanged delegate
		DataLayerStateChangedHandle = DataLayerSS->OnDataLayerInstanceRuntimeStateChanged().AddUObject(
			this, &URshipDataLayerManager::OnDataLayerRuntimeStateChanged);
	}

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Initialized"));
}

void URshipDataLayerManager::Shutdown()
{
	// Unbind from Data Layer events
	UDataLayerSubsystem* DataLayerSS = GetDataLayerSubsystem();
	if (DataLayerSS && DataLayerStateChangedHandle.IsValid())
	{
		DataLayerSS->OnDataLayerInstanceRuntimeStateChanged().Remove(DataLayerStateChangedHandle);
	}
	DataLayerStateChangedHandle.Reset();

	DataLayerStates.Empty();
	Subsystem = nullptr;

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Shutdown"));
}

// ============================================================================
// DATA LAYER QUERIES
// ============================================================================

TArray<FRshipDataLayerInfo> URshipDataLayerManager::GetAllDataLayers()
{
	TArray<FRshipDataLayerInfo> Result;

	if (!Subsystem) return Result;

	UDataLayerSubsystem* DataLayerSS = GetDataLayerSubsystem();
	if (!DataLayerSS) return Result;

	// Collect unique data layers from all target actors
	TSet<const UDataLayerInstance*> FoundDataLayers;

	if (Subsystem->TargetComponents)
	{
		for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
		{
			if (!Comp || !Comp->GetOwner()) continue;

			TArray<const UDataLayerInstance*> ActorDataLayers = Comp->GetOwner()->GetDataLayerInstances();
			for (const UDataLayerInstance* DataLayer : ActorDataLayers)
			{
				if (DataLayer)
				{
					FoundDataLayers.Add(DataLayer);
				}
			}
		}
	}

	// Also check World Partition's data layers if available
	UWorld* World = Subsystem->GetWorld();
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor) continue;

			TArray<const UDataLayerInstance*> ActorDataLayers = Actor->GetDataLayerInstances();
			for (const UDataLayerInstance* DataLayer : ActorDataLayers)
			{
				if (DataLayer)
				{
					FoundDataLayers.Add(DataLayer);
				}
			}
		}
	}

	// Build info for each unique data layer
	for (const UDataLayerInstance* DataLayer : FoundDataLayers)
	{
		FRshipDataLayerInfo Info;
		Info.DataLayerName = DataLayer->GetDataLayerShortName();
		Info.RuntimeState = DataLayer->GetRuntimeState();
		Info.bIsLoaded = Info.RuntimeState == EDataLayerRuntimeState::Loaded ||
		                 Info.RuntimeState == EDataLayerRuntimeState::Activated;
		Info.bIsActivated = Info.RuntimeState == EDataLayerRuntimeState::Activated;
		Info.DebugColor = DataLayer->GetDebugColor();

		if (const UDataLayerAsset* Asset = DataLayer->GetAsset())
		{
			Info.DataLayerAssetName = Asset->GetName();
		}

		TArray<URshipTargetComponent*> Targets = GetTargetsForDataLayerInstance(DataLayer);
		Info.TargetCount = Targets.Num();

		Result.Add(Info);
	}

	return Result;
}

FRshipDataLayerInfo URshipDataLayerManager::GetDataLayerInfo(const FString& DataLayerName)
{
	FRshipDataLayerInfo Info;
	Info.DataLayerName = DataLayerName;

	const UDataLayerInstance* DataLayer = FindDataLayerByName(DataLayerName);
	if (!DataLayer)
	{
		return Info;
	}

	Info.RuntimeState = DataLayer->GetRuntimeState();
	Info.bIsLoaded = Info.RuntimeState == EDataLayerRuntimeState::Loaded ||
	                 Info.RuntimeState == EDataLayerRuntimeState::Activated;
	Info.bIsActivated = Info.RuntimeState == EDataLayerRuntimeState::Activated;
	Info.DebugColor = DataLayer->GetDebugColor();

	if (const UDataLayerAsset* Asset = DataLayer->GetAsset())
	{
		Info.DataLayerAssetName = Asset->GetName();
	}

	TArray<URshipTargetComponent*> Targets = GetTargetsForDataLayerInstance(DataLayer);
	Info.TargetCount = Targets.Num();

	return Info;
}

TArray<URshipTargetComponent*> URshipDataLayerManager::GetTargetsInDataLayer(const FString& DataLayerName)
{
	const UDataLayerInstance* DataLayer = FindDataLayerByName(DataLayerName);
	if (!DataLayer)
	{
		return TArray<URshipTargetComponent*>();
	}

	return GetTargetsForDataLayerInstance(DataLayer);
}

TArray<URshipTargetComponent*> URshipDataLayerManager::GetTargetsByDataLayerPattern(const FString& WildcardPattern)
{
	TArray<URshipTargetComponent*> Result;

	if (!Subsystem || !Subsystem->TargetComponents) return Result;

	// Convert wildcard pattern to regex-like matching
	// Simple wildcard: * matches any characters
	FString Pattern = WildcardPattern.ToLower().Replace(TEXT("*"), TEXT(""));

	for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
	{
		if (!Comp || !Comp->GetOwner()) continue;

		TArray<const UDataLayerInstance*> ActorDataLayers = Comp->GetOwner()->GetDataLayerInstances();
		for (const UDataLayerInstance* DataLayer : ActorDataLayers)
		{
			if (!DataLayer) continue;

			FString LayerName = DataLayer->GetDataLayerShortName().ToLower();

			// Check if pattern matches (simple contains for now)
			bool bMatches = false;
			if (WildcardPattern.StartsWith(TEXT("*")) && WildcardPattern.EndsWith(TEXT("*")))
			{
				// *pattern* = contains
				bMatches = LayerName.Contains(Pattern);
			}
			else if (WildcardPattern.StartsWith(TEXT("*")))
			{
				// *pattern = ends with
				bMatches = LayerName.EndsWith(Pattern);
			}
			else if (WildcardPattern.EndsWith(TEXT("*")))
			{
				// pattern* = starts with
				bMatches = LayerName.StartsWith(Pattern);
			}
			else
			{
				// Exact match
				bMatches = (LayerName == WildcardPattern.ToLower());
			}

			if (bMatches)
			{
				Result.AddUnique(Comp);
				break;  // Found a matching layer, no need to check more
			}
		}
	}

	return Result;
}

TArray<FString> URshipDataLayerManager::GetTargetDataLayers(URshipTargetComponent* Target)
{
	TArray<FString> Result;

	if (!Target || !Target->GetOwner())
	{
		return Result;
	}

	UDataLayerSubsystem* DataLayerSS = GetDataLayerSubsystem();
	if (!DataLayerSS)
	{
		return Result;
	}

	AActor* Owner = Target->GetOwner();

	// Get Data Layers for this actor
	TArray<const UDataLayerInstance*> DataLayers = Owner->GetDataLayerInstances();
	for (const UDataLayerInstance* DataLayer : DataLayers)
	{
		if (DataLayer)
		{
			Result.Add(DataLayer->GetDataLayerShortName());
		}
	}

	return Result;
}

bool URshipDataLayerManager::IsDataLayerLoaded(const FString& DataLayerName)
{
	EDataLayerRuntimeState State = GetDataLayerState(DataLayerName);
	return State == EDataLayerRuntimeState::Loaded || State == EDataLayerRuntimeState::Activated;
}

bool URshipDataLayerManager::IsDataLayerActivated(const FString& DataLayerName)
{
	return GetDataLayerState(DataLayerName) == EDataLayerRuntimeState::Activated;
}

EDataLayerRuntimeState URshipDataLayerManager::GetDataLayerState(const FString& DataLayerName)
{
	const UDataLayerInstance* DataLayer = FindDataLayerByName(DataLayerName);
	if (DataLayer)
	{
		return DataLayer->GetRuntimeState();
	}
	return EDataLayerRuntimeState::Unloaded;
}

// ============================================================================
// DATA LAYER ACTIONS
// ============================================================================

int32 URshipDataLayerManager::ReregisterTargetsInDataLayer(const FString& DataLayerName)
{
	TArray<URshipTargetComponent*> Targets = GetTargetsInDataLayer(DataLayerName);

	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			Target->Register();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Re-registered %d targets in Data Layer '%s'"),
		Targets.Num(), *DataLayerName);

	return Targets.Num();
}

int32 URshipDataLayerManager::AddTagToDataLayerTargets(const FString& DataLayerName, const FString& Tag)
{
	TArray<URshipTargetComponent*> Targets = GetTargetsInDataLayer(DataLayerName);
	int32 Count = 0;

	for (URshipTargetComponent* Target : Targets)
	{
		if (Target && !Target->HasTag(Tag))
		{
			Target->Tags.Add(Tag);
			Count++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Added tag '%s' to %d targets in Data Layer '%s'"),
		*Tag, Count, *DataLayerName);

	return Count;
}

int32 URshipDataLayerManager::RemoveTagFromDataLayerTargets(const FString& DataLayerName, const FString& Tag)
{
	TArray<URshipTargetComponent*> Targets = GetTargetsInDataLayer(DataLayerName);
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

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Removed tag '%s' from %d targets in Data Layer '%s'"),
		*Tag, Count, *DataLayerName);

	return Count;
}

int32 URshipDataLayerManager::AddDataLayerTargetsToGroup(const FString& DataLayerName, const FString& GroupId)
{
	if (!Subsystem) return 0;

	TArray<URshipTargetComponent*> Targets = GetTargetsInDataLayer(DataLayerName);

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	if (!GroupManager) return 0;

	int32 Count = 0;
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			GroupManager->AddTargetToGroup(Target->targetName, GroupId);
			Count++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Added %d targets from Data Layer '%s' to group '%s'"),
		Count, *DataLayerName, *GroupId);

	return Count;
}

// ============================================================================
// AUTO-TAGGING
// ============================================================================

void URshipDataLayerManager::SetAutoDataLayerTagging(bool bEnabled)
{
	if (bAutoDataLayerTagging == bEnabled) return;

	bAutoDataLayerTagging = bEnabled;

	if (!Subsystem || !Subsystem->TargetComponents) return;

	if (bEnabled)
	{
		// Apply Data Layer tags to all existing targets
		for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
		{
			if (Comp)
			{
				TArray<FString> DataLayers = GetTargetDataLayers(Comp);
				for (const FString& LayerName : DataLayers)
				{
					ApplyAutoDataLayerTag(Comp, LayerName);
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Auto Data Layer tagging enabled"));
	}
	else
	{
		// Remove Data Layer tags from all targets
		for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
		{
			if (Comp)
			{
				RemoveAutoDataLayerTags(Comp);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Auto Data Layer tagging disabled"));
	}
}

void URshipDataLayerManager::SetAutoDataLayerTagPrefix(const FString& Prefix)
{
	if (AutoDataLayerTagPrefix == Prefix) return;

	// Remove old tags and apply new ones if enabled
	if (bAutoDataLayerTagging && Subsystem && Subsystem->TargetComponents)
	{
		for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
		{
			if (Comp)
			{
				RemoveAutoDataLayerTags(Comp);
			}
		}

		AutoDataLayerTagPrefix = Prefix;

		for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
		{
			if (Comp)
			{
				TArray<FString> DataLayers = GetTargetDataLayers(Comp);
				for (const FString& LayerName : DataLayers)
				{
					ApplyAutoDataLayerTag(Comp, LayerName);
				}
			}
		}
	}
	else
	{
		AutoDataLayerTagPrefix = Prefix;
	}
}

// ============================================================================
// AUTO-GROUPING
// ============================================================================

void URshipDataLayerManager::SetAutoDataLayerGrouping(bool bEnabled)
{
	if (bAutoDataLayerGrouping == bEnabled) return;

	bAutoDataLayerGrouping = bEnabled;

	if (bEnabled)
	{
		CreateGroupsForAllDataLayers();
		UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Auto Data Layer grouping enabled"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Auto Data Layer grouping disabled"));
	}
}

int32 URshipDataLayerManager::CreateGroupsForAllDataLayers()
{
	if (!Subsystem) return 0;

	URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
	if (!GroupManager) return 0;

	// Get all data layers and create a group for each
	TArray<FRshipDataLayerInfo> AllDataLayers = GetAllDataLayers();
	int32 Count = 0;

	for (const FRshipDataLayerInfo& Info : AllDataLayers)
	{
		FString GroupId = TEXT("DataLayer_") + Info.DataLayerName;

		// Create group if it doesn't exist
		FRshipTargetGroup Group;
		if (!GroupManager->GetGroup(GroupId, Group))
		{
			Group.GroupId = GroupId;
			Group.DisplayName = Info.DataLayerName;
			Group.Description = FString::Printf(TEXT("Auto-created group for Data Layer '%s'"), *Info.DataLayerName);
			GroupManager->CreateGroup(Group);
		}

		// Add all targets from this data layer to the group
		TArray<URshipTargetComponent*> Targets = GetTargetsInDataLayer(Info.DataLayerName);
		for (URshipTargetComponent* Target : Targets)
		{
			if (Target)
			{
				GroupManager->AddTargetToGroup(Target->targetName, GroupId);
			}
		}

		Count++;
	}

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Created groups for %d Data Layers"), Count);
	return Count;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void URshipDataLayerManager::OnDataLayerRuntimeStateChanged(const UDataLayerInstance* DataLayer, EDataLayerRuntimeState NewState)
{
	if (!DataLayer) return;

	FString DataLayerName = DataLayer->GetDataLayerShortName();
	EDataLayerRuntimeState OldState = DataLayerStates.FindRef(DataLayer->GetDataLayerFName());

	// Update cache
	DataLayerStates.Add(DataLayer->GetDataLayerFName(), NewState);

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Data Layer '%s' state changed from %d to %d"),
		*DataLayerName, static_cast<int32>(OldState), static_cast<int32>(NewState));

	// Handle state transitions
	bool bWasLoaded = OldState == EDataLayerRuntimeState::Loaded || OldState == EDataLayerRuntimeState::Activated;
	bool bIsLoaded = NewState == EDataLayerRuntimeState::Loaded || NewState == EDataLayerRuntimeState::Activated;

	if (!bWasLoaded && bIsLoaded)
	{
		// Data Layer just loaded
		RegisterDataLayerTargets(DataLayer);
	}
	else if (bWasLoaded && !bIsLoaded)
	{
		// Data Layer just unloaded
		UnregisterDataLayerTargets(DataLayer);
	}

	// Broadcast event
	OnDataLayerStateChanged.Broadcast(DataLayerName, NewState);
}

void URshipDataLayerManager::RegisterDataLayerTargets(const UDataLayerInstance* DataLayer)
{
	if (!DataLayer || !Subsystem) return;

	FString DataLayerName = DataLayer->GetDataLayerShortName();
	TArray<URshipTargetComponent*> Targets = GetTargetsForDataLayerInstance(DataLayer);

	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			// Apply auto tags if enabled
			if (bAutoDataLayerTagging)
			{
				ApplyAutoDataLayerTag(Target, DataLayerName);
			}

			// Add to auto group if enabled
			if (bAutoDataLayerGrouping)
			{
				FString GroupId = TEXT("DataLayer_") + DataLayerName;
				URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager();
				if (GroupManager)
				{
					GroupManager->AddTargetToGroup(Target->targetName, GroupId);
				}
			}

			// Register with server
			Target->Register();
		}
	}

	OnDataLayerTargetsRegistered.Broadcast(DataLayerName, Targets.Num());

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Registered %d targets from Data Layer '%s'"),
		Targets.Num(), *DataLayerName);
}

void URshipDataLayerManager::UnregisterDataLayerTargets(const UDataLayerInstance* DataLayer)
{
	if (!DataLayer || !Subsystem) return;

	FString DataLayerName = DataLayer->GetDataLayerShortName();
	TArray<URshipTargetComponent*> Targets = GetTargetsForDataLayerInstance(DataLayer);

	for (URshipTargetComponent* Target : Targets)
	{
		if (Target)
		{
			// Remove auto tags
			if (bAutoDataLayerTagging)
			{
				RemoveAutoDataLayerTags(Target);
			}

			// Note: URshipTargetComponent unregisters automatically via OnComponentDestroyed
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipDataLayerManager: Unregistered %d targets from Data Layer '%s'"),
		Targets.Num(), *DataLayerName);
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

void URshipDataLayerManager::ApplyAutoDataLayerTag(URshipTargetComponent* Target, const FString& DataLayerName)
{
	if (!Target) return;

	FString Tag = AutoDataLayerTagPrefix + DataLayerName;

	if (!Target->HasTag(Tag))
	{
		Target->Tags.Add(Tag);
	}
}

void URshipDataLayerManager::RemoveAutoDataLayerTags(URshipTargetComponent* Target)
{
	if (!Target) return;

	// Remove any tags starting with the auto prefix
	for (int32 i = Target->Tags.Num() - 1; i >= 0; i--)
	{
		if (Target->Tags[i].StartsWith(AutoDataLayerTagPrefix))
		{
			Target->Tags.RemoveAt(i);
		}
	}
}

TArray<URshipTargetComponent*> URshipDataLayerManager::GetTargetsForDataLayerInstance(const UDataLayerInstance* DataLayer)
{
	TArray<URshipTargetComponent*> Result;

	if (!DataLayer || !Subsystem || !Subsystem->TargetComponents)
	{
		return Result;
	}

	for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
	{
		if (!Comp || !Comp->GetOwner()) continue;

		// Check if this actor belongs to the Data Layer
		TArray<const UDataLayerInstance*> ActorDataLayers = Comp->GetOwner()->GetDataLayerInstances();
		if (ActorDataLayers.Contains(DataLayer))
		{
			Result.Add(Comp);
		}
	}

	return Result;
}

const UDataLayerInstance* URshipDataLayerManager::FindDataLayerByName(const FString& DataLayerName) const
{
	if (!Subsystem) return nullptr;

	UWorld* World = Subsystem->GetWorld();
	if (!World) return nullptr;

	// Search through all actors to find a data layer instance with matching name
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		TArray<const UDataLayerInstance*> ActorDataLayers = Actor->GetDataLayerInstances();
		for (const UDataLayerInstance* DataLayer : ActorDataLayers)
		{
			if (DataLayer)
			{
				FString LayerName = DataLayer->GetDataLayerShortName();
				if (LayerName.Equals(DataLayerName, ESearchCase::IgnoreCase) ||
				    LayerName.Contains(DataLayerName) ||
				    DataLayerName.Contains(LayerName))
				{
					return DataLayer;
				}
			}
		}
	}

	return nullptr;
}

UDataLayerSubsystem* URshipDataLayerManager::GetDataLayerSubsystem() const
{
	if (!Subsystem) return nullptr;

	UWorld* World = Subsystem->GetWorld();
	if (!World) return nullptr;

	return World->GetSubsystem<UDataLayerSubsystem>();
}
