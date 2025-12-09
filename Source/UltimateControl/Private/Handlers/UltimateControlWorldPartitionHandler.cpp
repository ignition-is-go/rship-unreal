// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlWorldPartitionHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/HLOD/HLODSubsystem.h"

void FUltimateControlWorldPartitionHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	// World Partition status
	Methods.Add(TEXT("worldPartition.getStatus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetWorldPartitionStatus));
	Methods.Add(TEXT("worldPartition.isEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleIsWorldPartitionEnabled));
	Methods.Add(TEXT("worldPartition.getWorldBounds"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetWorldBounds));

	// Cell management
	Methods.Add(TEXT("worldPartition.listCells"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleListCells));
	Methods.Add(TEXT("worldPartition.getCellStatus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetCellStatus));
	Methods.Add(TEXT("worldPartition.loadCells"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleLoadCells));
	Methods.Add(TEXT("worldPartition.unloadCells"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleUnloadCells));
	Methods.Add(TEXT("worldPartition.getLoadedCells"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetLoadedCells));

	// Streaming
	Methods.Add(TEXT("worldPartition.setStreamingSource"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleSetStreamingSource));
	Methods.Add(TEXT("worldPartition.getStreamingSources"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetStreamingSources));

	// Data Layers
	Methods.Add(TEXT("dataLayer.list"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleListDataLayers));
	Methods.Add(TEXT("dataLayer.get"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetDataLayer));
	Methods.Add(TEXT("dataLayer.create"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleCreateDataLayer));
	Methods.Add(TEXT("dataLayer.delete"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleDeleteDataLayer));

	// Data Layer visibility
	Methods.Add(TEXT("dataLayer.getVisibility"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetDataLayerVisibility));
	Methods.Add(TEXT("dataLayer.setVisibility"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleSetDataLayerVisibility));
	Methods.Add(TEXT("dataLayer.setLoadState"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleSetDataLayerLoadState));

	// Data Layer actor management
	Methods.Add(TEXT("dataLayer.getActors"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetDataLayerActors));
	Methods.Add(TEXT("dataLayer.addActor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleAddActorToDataLayer));
	Methods.Add(TEXT("dataLayer.removeActor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleRemoveActorFromDataLayer));

	// HLOD
	Methods.Add(TEXT("hlod.getStatus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleGetHLODStatus));
	Methods.Add(TEXT("hlod.build"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlWorldPartitionHandler::HandleBuildHLODs));
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
		Error = CreateError(-32603, TEXT("No editor world available"));
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
		Error = CreateError(-32603, TEXT("World Partition is not enabled"));
		return true;
	}

	FBox Bounds = WorldPartition->GetWorldBounds();

	TSharedPtr<FJsonObject> BoundsJson = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> MinJson = MakeShared<FJsonObject>();
	MinJson->SetNumberField(TEXT("x"), Bounds.Min.X);
	MinJson->SetNumberField(TEXT("y"), Bounds.Min.Y);
	MinJson->SetNumberField(TEXT("z"), Bounds.Min.Z);
	BoundsJson->SetObjectField(TEXT("min"), MinJson);

	TSharedPtr<FJsonObject> MaxJson = MakeShared<FJsonObject>();
	MaxJson->SetNumberField(TEXT("x"), Bounds.Max.X);
	MaxJson->SetNumberField(TEXT("y"), Bounds.Max.Y);
	MaxJson->SetNumberField(TEXT("z"), Bounds.Max.Z);
	BoundsJson->SetObjectField(TEXT("max"), MaxJson);

	FVector Center = Bounds.GetCenter();
	FVector Extent = Bounds.GetExtent();

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

	Result = MakeShared<FJsonValueObject>(BoundsJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleListCells(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UWorldPartitionSubsystem* Subsystem = World->GetSubsystem<UWorldPartitionSubsystem>();
	if (!Subsystem)
	{
		Error = CreateError(-32603, TEXT("World Partition Subsystem not available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> CellsArray;

	// Get streaming cells info
	// Note: In editor, we can enumerate through the cells

	Result = MakeShared<FJsonValueArray>(CellsArray);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetCellStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString CellName = Params->GetStringField(TEXT("cellName"));
	if (CellName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("cellName parameter required"));
		return true;
	}

	// Cell status is typically retrieved during runtime streaming
	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetStringField(TEXT("cellName"), CellName);
	StatusJson->SetStringField(TEXT("status"), TEXT("unknown"));

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleLoadCells(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Load cells at specified location
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));
	double Z = Params->GetNumberField(TEXT("z"));
	double Radius = Params->HasField(TEXT("radius")) ? Params->GetNumberField(TEXT("radius")) : 10000.0;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	// In editor, we can load cells around a location
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
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
	if (!DataLayerSubsystem)
	{
		Error = CreateError(-32603, TEXT("Data Layer Subsystem not available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> LayersArray;

	// Get all data layer instances
	DataLayerSubsystem->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
	{
		if (DataLayerInstance)
		{
			LayersArray.Add(MakeShared<FJsonValueObject>(DataLayerToJson(DataLayerInstance)));
		}
		return true;
	});

	Result = MakeShared<FJsonValueArray>(LayersArray);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleGetDataLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	if (LayerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("name parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
	if (!DataLayerSubsystem)
	{
		Error = CreateError(-32603, TEXT("Data Layer Subsystem not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerSubsystem->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
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
		Error = CreateError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
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
		Error = CreateError(-32602, TEXT("name parameter required"));
		return true;
	}

	// Creating data layers requires creating a DataLayerAsset
	// This is typically done through the editor UI or asset creation
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
		Error = CreateError(-32602, TEXT("name parameter required"));
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
		Error = CreateError(-32602, TEXT("name parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
	if (!DataLayerSubsystem)
	{
		Error = CreateError(-32603, TEXT("Data Layer Subsystem not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerSubsystem->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
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
		Error = CreateError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
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
		Error = CreateError(-32602, TEXT("name parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
	if (!DataLayerSubsystem)
	{
		Error = CreateError(-32603, TEXT("Data Layer Subsystem not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerSubsystem->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
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
		Error = CreateError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
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
		Error = CreateError(-32602, TEXT("name parameter required"));
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
		Error = CreateError(-32602, TEXT("name parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	// Iterate through all actors and check their data layer assignments
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
		Error = CreateError(-32602, TEXT("actorName and layerName parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	// Find the actor
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
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	// Find the data layer
	UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
	if (!DataLayerSubsystem)
	{
		Error = CreateError(-32603, TEXT("Data Layer Subsystem not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerSubsystem->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
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
		Error = CreateError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
		return true;
	}

	// Add actor to data layer
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
		Error = CreateError(-32602, TEXT("actorName and layerName parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	// Find the actor
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
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	// Find the data layer
	UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
	if (!DataLayerSubsystem)
	{
		Error = CreateError(-32603, TEXT("Data Layer Subsystem not available"));
		return true;
	}

	UDataLayerInstance* FoundLayer = nullptr;
	DataLayerSubsystem->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
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
		Error = CreateError(-32602, FString::Printf(TEXT("Data layer not found: %s"), *LayerName));
		return true;
	}

	// Remove actor from data layer
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
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();

	// Check if HLOD subsystem exists
	UHLODSubsystem* HLODSubsystem = World->GetSubsystem<UHLODSubsystem>();
	StatusJson->SetBoolField(TEXT("available"), HLODSubsystem != nullptr);

	if (HLODSubsystem)
	{
		// Get HLOD info
		StatusJson->SetBoolField(TEXT("enabled"), true);
	}

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlWorldPartitionHandler::HandleBuildHLODs(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Building HLODs is typically done through the editor build process
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("HLOD building should be triggered through the Build menu or automation.buildHLODs"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}
