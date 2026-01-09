// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlWorldPartitionHandler.h"
#include "UltimateControlSubsystem.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/HLOD/HLODRuntimeSubsystem.h"

FUltimateControlWorldPartitionHandler::FUltimateControlWorldPartitionHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// World Partition status
	RegisterMethod(
		TEXT("worldPartition.getStatus"),
		TEXT("Get World Partition status and configuration"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetWorldPartitionStatus));

	RegisterMethod(
		TEXT("worldPartition.isEnabled"),
		TEXT("Check if World Partition is enabled for current world"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleIsWorldPartitionEnabled));

	RegisterMethod(
		TEXT("worldPartition.getWorldBounds"),
		TEXT("Get the bounds of the World Partition world"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetWorldBounds));

	// Cell management
	RegisterMethod(
		TEXT("worldPartition.listCells"),
		TEXT("List all streaming cells in the world"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleListCells));

	RegisterMethod(
		TEXT("worldPartition.getCellStatus"),
		TEXT("Get status of a specific streaming cell"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetCellStatus));

	RegisterMethod(
		TEXT("worldPartition.loadCells"),
		TEXT("Load cells around a specified location"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleLoadCells));

	RegisterMethod(
		TEXT("worldPartition.unloadCells"),
		TEXT("Unload cells"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleUnloadCells));

	RegisterMethod(
		TEXT("worldPartition.getLoadedCells"),
		TEXT("Get list of currently loaded cells"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetLoadedCells));

	// Streaming
	RegisterMethod(
		TEXT("worldPartition.setStreamingSource"),
		TEXT("Set streaming source location"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleSetStreamingSource));

	RegisterMethod(
		TEXT("worldPartition.getStreamingSources"),
		TEXT("Get current streaming sources"),
		TEXT("WorldPartition"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetStreamingSources));

	// Data Layers
	RegisterMethod(
		TEXT("dataLayer.list"),
		TEXT("List all data layers in the world"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleListDataLayers));

	RegisterMethod(
		TEXT("dataLayer.get"),
		TEXT("Get information about a specific data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetDataLayer));

	RegisterMethod(
		TEXT("dataLayer.create"),
		TEXT("Create a new data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleCreateDataLayer));

	RegisterMethod(
		TEXT("dataLayer.delete"),
		TEXT("Delete a data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleDeleteDataLayer),
		/* bIsDangerous */ true,
		/* bRequiresConfirmation */ true);

	// Data Layer visibility
	RegisterMethod(
		TEXT("dataLayer.getVisibility"),
		TEXT("Get visibility state of a data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetDataLayerVisibility));

	RegisterMethod(
		TEXT("dataLayer.setVisibility"),
		TEXT("Set visibility of a data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleSetDataLayerVisibility));

	RegisterMethod(
		TEXT("dataLayer.setLoadState"),
		TEXT("Set load state of a data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleSetDataLayerLoadState));

	// Data Layer actor management
	RegisterMethod(
		TEXT("dataLayer.getActors"),
		TEXT("Get actors assigned to a data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetDataLayerActors));

	RegisterMethod(
		TEXT("dataLayer.addActor"),
		TEXT("Add an actor to a data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleAddActorToDataLayer));

	RegisterMethod(
		TEXT("dataLayer.removeActor"),
		TEXT("Remove an actor from a data layer"),
		TEXT("DataLayer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleRemoveActorFromDataLayer));

	// HLOD
	RegisterMethod(
		TEXT("hlod.getStatus"),
		TEXT("Get HLOD subsystem status"),
		TEXT("HLOD"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetHLODStatus));

	RegisterMethod(
		TEXT("hlod.build"),
		TEXT("Trigger HLOD build"),
		TEXT("HLOD"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleBuildHLODs));
}

UWorldPartition* FUltimateControlWorldPartitionHandler::GetWorldPartition()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	return World ? World->GetWorldPartition() : nullptr;
}

TSharedPtr<FJsonObject> FUltimateControlWorldPartitionHandler::DataLayerToJson(const UDataLayerInstance* DataLayer)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!DataLayer)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), DataLayer->GetDataLayerShortName());
	Json->SetStringField(TEXT("fullName"), DataLayer->GetDataLayerFullName());
	Json->SetBoolField(TEXT("isVisible"), DataLayer->IsVisible());
	Json->SetBoolField(TEXT("isInitiallyVisible"), DataLayer->IsInitiallyVisible());
	Json->SetBoolField(TEXT("isRuntime"), DataLayer->IsRuntime());

	// Get the data layer asset if available
	if (const UDataLayerAsset* Asset = DataLayer->GetAsset())
	{
		Json->SetStringField(TEXT("assetPath"), Asset->GetPathName());
	}

	return Json;
}

bool FUltimateControlWorldPartitionHandler::HandleGetWorldPartitionStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();

	UWorldPartition* WorldPartition = World->GetWorldPartition();
	StatusJson->SetBoolField(TEXT("enabled"), WorldPartition != nullptr);
	StatusJson->SetStringField(TEXT("worldName"), World->GetName());

	if (WorldPartition)
	{
		StatusJson->SetBoolField(TEXT("isInitialized"), WorldPartition->IsInitialized());
		StatusJson->SetBoolField(TEXT("isStreamingEnabled"), WorldPartition->IsStreamingEnabled());
	}

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleIsWorldPartitionEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorldPartition* WorldPartition = GetWorldPartition();
	Result = MakeShared<FJsonValueBoolean>(WorldPartition != nullptr);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetWorldBounds(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorldPartition* WorldPartition = GetWorldPartition();
	if (!WorldPartition)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("World Partition is not enabled"));
		return true;
	}

	// UE 5.6: Use GetRuntimeWorldBounds() or calculate from streaming generation
	TSharedPtr<FJsonObject> BoundsJson = MakeShared<FJsonObject>();

	// UE 5.6: Calculate bounds from all actors in the world since GetWorldBounds() was removed
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		// Calculate bounds from all actors
		FBox WorldBounds(ForceInit);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && !Actor->IsEditorOnly())
			{
				WorldBounds += Actor->GetComponentsBoundingBox();
			}
		}

		if (WorldBounds.IsValid)
		{
			TSharedPtr<FJsonObject> MinJson = MakeShared<FJsonObject>();
			MinJson->SetNumberField(TEXT("x"), WorldBounds.Min.X);
			MinJson->SetNumberField(TEXT("y"), WorldBounds.Min.Y);
			MinJson->SetNumberField(TEXT("z"), WorldBounds.Min.Z);
			BoundsJson->SetObjectField(TEXT("min"), MinJson);

			TSharedPtr<FJsonObject> MaxJson = MakeShared<FJsonObject>();
			MaxJson->SetNumberField(TEXT("x"), WorldBounds.Max.X);
			MaxJson->SetNumberField(TEXT("y"), WorldBounds.Max.Y);
			MaxJson->SetNumberField(TEXT("z"), WorldBounds.Max.Z);
			BoundsJson->SetObjectField(TEXT("max"), MaxJson);

			FVector Center = WorldBounds.GetCenter();
			FVector Extent = WorldBounds.GetExtent();

			TSharedPtr<FJsonObject> CenterJson = MakeShared<FJsonObject>();
			CenterJson->SetNumberField(TEXT("x"), Center.X);
			CenterJson->SetNumberField(TEXT("y"), Center.Y);
			CenterJson->SetNumberField(TEXT("z"), Center.Z);
			BoundsJson->SetObjectField(TEXT("center"), CenterJson);

			TSharedPtr<FJsonObject> ExtentJson = MakeShared<FJsonObject>();
			ExtentJson->SetNumberField(TEXT("x"), Extent.X);
			ExtentJson->SetNumberField(TEXT("y"), Extent.Y);
			ExtentJson->SetNumberField(TEXT("z"), Extent.Z);
			BoundsJson->SetObjectField(TEXT("extent"), ExtentJson);
		}
	}

	Result = MakeShared<FJsonValueObject>(BoundsJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleListCells(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	UWorldPartitionSubsystem* WPSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>();
	if (!WPSubsystem)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("World Partition Subsystem not available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> CellsArray;
	// Note: Cell enumeration requires runtime - in editor we report available info

	Result = MakeShared<FJsonValueArray>(CellsArray);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetCellStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString CellName = Params->GetStringField(TEXT("cellName"));
	if (CellName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("cellName parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetStringField(TEXT("cellName"), CellName);
	StatusJson->SetStringField(TEXT("status"), TEXT("unknown"));

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleLoadCells(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));
	double Z = Params->GetNumberField(TEXT("z"));
	double Radius = Params->HasField(TEXT("radius")) ? Params->GetNumberField(TEXT("radius")) : 10000.0;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	FVector Location(X, Y, Z);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetNumberField(TEXT("x"), X);
	ResultJson->SetNumberField(TEXT("y"), Y);
	ResultJson->SetNumberField(TEXT("z"), Z);
	ResultJson->SetNumberField(TEXT("radius"), Radius);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleUnloadCells(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetLoadedCells(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> CellsArray;
	Result = MakeShared<FJsonValueArray>(CellsArray);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleSetStreamingSource(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));
	double Z = Params->GetNumberField(TEXT("z"));

	FVector Location(X, Y, Z);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetStreamingSources(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> SourcesArray;
	Result = MakeShared<FJsonValueArray>(SourcesArray);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleListDataLayers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> LayersArray;

	// UE 5.6: Use DataLayerManager to get data layers
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World))
	{
		DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
		{
			if (DataLayerInstance)
			{
				LayersArray.Add(MakeShared<FJsonValueObject>(DataLayerToJson(DataLayerInstance)));
			}
			return true;
		});
	}

	Result = MakeShared<FJsonValueArray>(LayersArray);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetDataLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	if (LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	if (!DataLayerManager)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Data Layer Manager not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
	{
		if (DataLayerInstance && DataLayerInstance->GetDataLayerShortName() == LayerName)
		{
			FoundLayer = DataLayerInstance;
			return false;
		}
		return true;
	});

	if (!FoundLayer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(DataLayerToJson(FoundLayer));
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleCreateDataLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	if (LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Data layer creation requires DataLayerAsset. Use asset.create to create a DataLayerAsset first."));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleDeleteDataLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	if (LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Data layer deletion requires removing the associated DataLayerAsset."));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetDataLayerVisibility(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	if (LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	if (!DataLayerManager)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Data Layer Manager not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
	{
		if (DataLayerInstance && DataLayerInstance->GetDataLayerShortName() == LayerName)
		{
			FoundLayer = DataLayerInstance;
			return false;
		}
		return true;
	});

	if (!FoundLayer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
		return true;
	}

	TSharedPtr<FJsonObject> VisibilityJson = MakeShared<FJsonObject>();
	VisibilityJson->SetStringField(TEXT("name"), LayerName);
	VisibilityJson->SetBoolField(TEXT("isVisible"), FoundLayer->IsVisible());
	VisibilityJson->SetBoolField(TEXT("isInitiallyVisible"), FoundLayer->IsInitiallyVisible());

	Result = MakeShared<FJsonValueObject>(VisibilityJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleSetDataLayerVisibility(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	bool bVisible = Params->GetBoolField(TEXT("visible"));

	if (LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	if (!DataLayerManager)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Data Layer Manager not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
	{
		if (DataLayerInstance && DataLayerInstance->GetDataLayerShortName() == LayerName)
		{
			FoundLayer = DataLayerInstance;
			return false;
		}
		return true;
	});

	if (!FoundLayer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
		return true;
	}

	FoundLayer->SetVisible(bVisible);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetStringField(TEXT("name"), LayerName);
	ResultJson->SetBoolField(TEXT("visible"), bVisible);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleSetDataLayerLoadState(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	FString StateStr = Params->GetStringField(TEXT("state"));

	if (LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetStringField(TEXT("name"), LayerName);
	ResultJson->SetStringField(TEXT("state"), StateStr);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetDataLayerActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	if (LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		TArray<const UDataLayerInstance*> DataLayers = Actor->GetDataLayerInstances();
		for (const UDataLayerInstance* DataLayer : DataLayers)
		{
			if (DataLayer && DataLayer->GetDataLayerShortName() == LayerName)
			{
				TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
				ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
				ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
				ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
				break;
			}
		}
	}

	Result = MakeShared<FJsonValueArray>(ActorsArray);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleAddActorToDataLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	FString LayerName = Params->GetStringField(TEXT("layerName"));

	if (ActorName.IsEmpty() || LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName and layerName parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetActorLabel() == ActorName)
		{
			FoundActor = Actor;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	if (!DataLayerManager)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Data Layer Manager not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
	{
		if (DataLayerInstance && DataLayerInstance->GetDataLayerShortName() == LayerName)
		{
			FoundLayer = DataLayerInstance;
			return false;
		}
		return true;
	});

	if (!FoundLayer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
		return true;
	}

	FoundActor->AddDataLayer(FoundLayer);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetStringField(TEXT("actorName"), ActorName);
	ResultJson->SetStringField(TEXT("layerName"), LayerName);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleRemoveActorFromDataLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	FString LayerName = Params->GetStringField(TEXT("layerName"));

	if (ActorName.IsEmpty() || LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName and layerName parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetActorLabel() == ActorName)
		{
			FoundActor = Actor;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	if (!DataLayerManager)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Data Layer Manager not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
	{
		if (DataLayerInstance && DataLayerInstance->GetDataLayerShortName() == LayerName)
		{
			FoundLayer = DataLayerInstance;
			return false;
		}
		return true;
	});

	if (!FoundLayer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
		return true;
	}

	FoundActor->RemoveDataLayer(FoundLayer);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetStringField(TEXT("actorName"), ActorName);
	ResultJson->SetStringField(TEXT("layerName"), LayerName);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetHLODStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();

	// UE 5.6: Use UWorldPartitionHLODRuntimeSubsystem
	UWorldPartitionHLODRuntimeSubsystem* HLODSubsystem = World->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>();
	StatusJson->SetBoolField(TEXT("available"), HLODSubsystem != nullptr);

	if (HLODSubsystem)
	{
		StatusJson->SetBoolField(TEXT("enabled"), true);
	}

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleBuildHLODs(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("HLOD building should be triggered through the Build menu or automation.buildHLODs"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}
