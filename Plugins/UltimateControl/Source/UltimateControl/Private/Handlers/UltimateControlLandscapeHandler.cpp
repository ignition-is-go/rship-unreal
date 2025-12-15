// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlLandscapeHandler.h"
#include "UltimateControlSubsystem.h"
#include "UltimateControlVersion.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeEdit.h"
#include "Materials/MaterialInterface.h"

FUltimateControlLandscapeHandler::FUltimateControlLandscapeHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// Landscape listing and info
	RegisterMethod(
		TEXT("landscape.list"),
		TEXT("List all landscapes in the world"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleListLandscapes));

	RegisterMethod(
		TEXT("landscape.get"),
		TEXT("Get information about a specific landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetLandscape));

	RegisterMethod(
		TEXT("landscape.getBounds"),
		TEXT("Get the bounding box of a landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetLandscapeBounds));

	RegisterMethod(
		TEXT("landscape.getResolution"),
		TEXT("Get the resolution settings of a landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetLandscapeResolution));

	// Height data
	RegisterMethod(
		TEXT("landscape.getHeightAtLocation"),
		TEXT("Get the terrain height at a specific XY location"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetHeightAtLocation));

	RegisterMethod(
		TEXT("landscape.getHeightRange"),
		TEXT("Get the min/max height range of a landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetHeightRange));

	RegisterMethod(
		TEXT("landscape.exportHeightmap"),
		TEXT("Export heightmap to a file"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleExportHeightmap));

	RegisterMethod(
		TEXT("landscape.importHeightmap"),
		TEXT("Import heightmap from a file"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleImportHeightmap));

	// Height editing
	RegisterMethod(
		TEXT("landscape.setHeightAtLocation"),
		TEXT("Set the terrain height at a location"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleSetHeightAtLocation));

	RegisterMethod(
		TEXT("landscape.smoothHeight"),
		TEXT("Smooth terrain height in an area"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleSmoothHeight));

	RegisterMethod(
		TEXT("landscape.flattenHeight"),
		TEXT("Flatten terrain to a specified height"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleFlattenHeight));

	RegisterMethod(
		TEXT("landscape.rampHeight"),
		TEXT("Create a height ramp between two points"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleRampHeight));

	// Layers
	RegisterMethod(
		TEXT("landscape.listLayers"),
		TEXT("List all paint layers on a landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleListLandscapeLayers));

	RegisterMethod(
		TEXT("landscape.getLayerInfo"),
		TEXT("Get information about a specific landscape layer"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetLayerInfo));

	RegisterMethod(
		TEXT("landscape.addLayer"),
		TEXT("Add a new paint layer to a landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleAddLandscapeLayer));

	RegisterMethod(
		TEXT("landscape.removeLayer"),
		TEXT("Remove a paint layer from a landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleRemoveLandscapeLayer));

	// Layer painting
	RegisterMethod(
		TEXT("landscape.getLayerWeightAtLocation"),
		TEXT("Get layer weights at a specific location"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetLayerWeightAtLocation));

	RegisterMethod(
		TEXT("landscape.paintLayer"),
		TEXT("Paint a layer at a location"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandlePaintLayer));

	RegisterMethod(
		TEXT("landscape.exportWeightmap"),
		TEXT("Export layer weightmap to a file"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleExportWeightmap));

	RegisterMethod(
		TEXT("landscape.importWeightmap"),
		TEXT("Import layer weightmap from a file"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleImportWeightmap));

	// Landscape material
	RegisterMethod(
		TEXT("landscape.getMaterial"),
		TEXT("Get the material assigned to a landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetLandscapeMaterial));

	RegisterMethod(
		TEXT("landscape.setMaterial"),
		TEXT("Set the material on a landscape"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleSetLandscapeMaterial));

	// Landscape components
	RegisterMethod(
		TEXT("landscape.listComponents"),
		TEXT("List all landscape components"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleListLandscapeComponents));

	RegisterMethod(
		TEXT("landscape.getComponentInfo"),
		TEXT("Get information about a specific landscape component"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetLandscapeComponentInfo));

	// LOD and optimization
	RegisterMethod(
		TEXT("landscape.getLODSettings"),
		TEXT("Get landscape LOD settings"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleGetLandscapeLODSettings));

	RegisterMethod(
		TEXT("landscape.setLODSettings"),
		TEXT("Set landscape LOD settings"),
		TEXT("Landscape"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLandscapeHandler::HandleSetLandscapeLODSettings));
}

TSharedPtr<FJsonObject> FUltimateControlLandscapeHandler::LandscapeToJson(ALandscapeProxy* Landscape)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Landscape)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Landscape->GetActorLabel());
	Json->SetStringField(TEXT("class"), Landscape->GetClass()->GetName());

	// Get bounds
	FBox Bounds = Landscape->GetComponentsBoundingBox();
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
	Json->SetObjectField(TEXT("bounds"), BoundsJson);

	// Component count
	TArray<ULandscapeComponent*> Components;
	Landscape->GetComponents<ULandscapeComponent>(Components);
	Json->SetNumberField(TEXT("componentCount"), Components.Num());

	// Material
	if (UMaterialInterface* Material = Landscape->GetLandscapeMaterial())
	{
		Json->SetStringField(TEXT("material"), Material->GetPathName());
	}

	return Json;
}

TSharedPtr<FJsonObject> FUltimateControlLandscapeHandler::LayerInfoToJson(ULandscapeLayerInfoObject* LayerInfo)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!LayerInfo)
	{
		return Json;
	}

#if ULTIMATE_CONTROL_UE_5_7_OR_LATER
	// UE 5.7+: LayerName is private, bNoWeightBlend uses getter
	Json->SetStringField(TEXT("name"), LayerInfo->GetLayerName().ToString());
	Json->SetStringField(TEXT("path"), LayerInfo->GetPathName());
	Json->SetBoolField(TEXT("noWeightBlend"), LayerInfo->IsNoWeightBlend());
#else
	// UE 5.6 and earlier: Direct property access
	Json->SetStringField(TEXT("name"), LayerInfo->LayerName.ToString());
	Json->SetStringField(TEXT("path"), LayerInfo->GetPathName());
	Json->SetBoolField(TEXT("noWeightBlend"), LayerInfo->bNoWeightBlend);
#endif

	return Json;
}

ALandscapeProxy* FUltimateControlLandscapeHandler::FindLandscape(const FString& LandscapeName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		if (Landscape && Landscape->GetActorLabel() == LandscapeName)
		{
			return Landscape;
		}
	}

	return nullptr;
}

bool FUltimateControlLandscapeHandler::HandleListLandscapes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> LandscapesArray;

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		if (Landscape)
		{
			LandscapesArray.Add(MakeShared<FJsonValueObject>(LandscapeToJson(Landscape)));
		}
	}

	Result = MakeShared<FJsonValueArray>(LandscapesArray);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetLandscape(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(LandscapeToJson(Landscape));
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetLandscapeBounds(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	FBox Bounds = Landscape->GetComponentsBoundingBox();

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

	FVector Size = Bounds.GetSize();
	TSharedPtr<FJsonObject> SizeJson = MakeShared<FJsonObject>();
	SizeJson->SetNumberField(TEXT("x"), Size.X);
	SizeJson->SetNumberField(TEXT("y"), Size.Y);
	SizeJson->SetNumberField(TEXT("z"), Size.Z);
	BoundsJson->SetObjectField(TEXT("size"), SizeJson);

	Result = MakeShared<FJsonValueObject>(BoundsJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetLandscapeResolution(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	TSharedPtr<FJsonObject> ResolutionJson = MakeShared<FJsonObject>();

	// Get component size
	ResolutionJson->SetNumberField(TEXT("componentSizeQuads"), Landscape->ComponentSizeQuads);
	ResolutionJson->SetNumberField(TEXT("subsectionSizeQuads"), Landscape->SubsectionSizeQuads);
	ResolutionJson->SetNumberField(TEXT("numSubsections"), Landscape->NumSubsections);

	// Count components
	TArray<ULandscapeComponent*> Components;
	Landscape->GetComponents<ULandscapeComponent>(Components);
	ResolutionJson->SetNumberField(TEXT("componentCount"), Components.Num());

	Result = MakeShared<FJsonValueObject>(ResolutionJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetHeightAtLocation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));

	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	// Get the landscape info for height sampling
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Could not get landscape info"));
		return true;
	}

	// Sample height at location using a line trace
	UWorld* World = Landscape->GetWorld();
	FHitResult HitResult;
	FVector Start(X, Y, 100000.0f);
	FVector End(X, Y, -100000.0f);

	if (World->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic))
	{
		if (HitResult.GetActor() == Landscape)
		{
			TSharedPtr<FJsonObject> HeightJson = MakeShared<FJsonObject>();
			HeightJson->SetNumberField(TEXT("x"), X);
			HeightJson->SetNumberField(TEXT("y"), Y);
			HeightJson->SetNumberField(TEXT("height"), HitResult.Location.Z);
			HeightJson->SetBoolField(TEXT("valid"), true);
			Result = MakeShared<FJsonValueObject>(HeightJson);
			return true;
		}
	}

	TSharedPtr<FJsonObject> HeightJson = MakeShared<FJsonObject>();
	HeightJson->SetNumberField(TEXT("x"), X);
	HeightJson->SetNumberField(TEXT("y"), Y);
	HeightJson->SetBoolField(TEXT("valid"), false);
	Result = MakeShared<FJsonValueObject>(HeightJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetHeightRange(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	FBox Bounds = Landscape->GetComponentsBoundingBox();

	TSharedPtr<FJsonObject> RangeJson = MakeShared<FJsonObject>();
	RangeJson->SetNumberField(TEXT("minHeight"), Bounds.Min.Z);
	RangeJson->SetNumberField(TEXT("maxHeight"), Bounds.Max.Z);
	RangeJson->SetNumberField(TEXT("heightRange"), Bounds.Max.Z - Bounds.Min.Z);

	Result = MakeShared<FJsonValueObject>(RangeJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleExportHeightmap(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString FilePath = Params->GetStringField(TEXT("filePath"));

	if (Name.IsEmpty() || FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name and filePath parameters required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	// Heightmap export requires landscape editor functionality
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Heightmap export requires LandscapeEditorUtils which needs editor mode active"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleImportHeightmap(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString FilePath = Params->GetStringField(TEXT("filePath"));

	if (Name.IsEmpty() || FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name and filePath parameters required"));
		return true;
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Heightmap import requires LandscapeEditorUtils which needs editor mode active"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleSetHeightAtLocation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));

	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Height modification requires landscape edit mode to be active"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleSmoothHeight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Smoothing requires landscape edit mode"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleFlattenHeight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Flattening requires landscape edit mode"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleRampHeight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Ramping requires landscape edit mode"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleListLandscapeLayers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> LayersArray;

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (LandscapeInfo)
	{
		for (const FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
		{
			TSharedPtr<FJsonObject> LayerJson = MakeShared<FJsonObject>();
			LayerJson->SetStringField(TEXT("name"), Layer.GetLayerName().ToString());
			if (Layer.LayerInfoObj)
			{
				LayerJson->SetStringField(TEXT("layerInfoPath"), Layer.LayerInfoObj->GetPathName());
#if ULTIMATE_CONTROL_UE_5_7_OR_LATER
				LayerJson->SetBoolField(TEXT("noWeightBlend"), Layer.LayerInfoObj->IsNoWeightBlend());
#else
				LayerJson->SetBoolField(TEXT("noWeightBlend"), Layer.LayerInfoObj->bNoWeightBlend);
#endif
			}
			LayersArray.Add(MakeShared<FJsonValueObject>(LayerJson));
		}
	}

	Result = MakeShared<FJsonValueArray>(LayersArray);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetLayerInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString LayerName = Params->GetStringField(TEXT("layerName"));

	if (Name.IsEmpty() || LayerName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name and layerName parameters required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Could not get landscape info"));
		return true;
	}

	for (const FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
	{
		if (Layer.GetLayerName().ToString() == LayerName)
		{
			TSharedPtr<FJsonObject> LayerJson = MakeShared<FJsonObject>();
			LayerJson->SetStringField(TEXT("name"), Layer.GetLayerName().ToString());
			if (Layer.LayerInfoObj)
			{
				LayerJson->SetStringField(TEXT("layerInfoPath"), Layer.LayerInfoObj->GetPathName());
#if ULTIMATE_CONTROL_UE_5_7_OR_LATER
				LayerJson->SetBoolField(TEXT("noWeightBlend"), Layer.LayerInfoObj->IsNoWeightBlend());
#else
				LayerJson->SetBoolField(TEXT("noWeightBlend"), Layer.LayerInfoObj->bNoWeightBlend);
#endif
			}
			Result = MakeShared<FJsonValueObject>(LayerJson);
			return true;
		}
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Layer not found: %s"), *LayerName));
	return true;
}

bool FUltimateControlLandscapeHandler::HandleAddLandscapeLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Adding layers requires creating a LandscapeLayerInfoObject asset"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleRemoveLandscapeLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Removing layers requires landscape edit mode"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetLayerWeightAtLocation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));

	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	TSharedPtr<FJsonObject> WeightsJson = MakeShared<FJsonObject>();
	WeightsJson->SetNumberField(TEXT("x"), X);
	WeightsJson->SetNumberField(TEXT("y"), Y);

	// Layer weight sampling requires FLandscapeEditDataInterface
	TArray<TSharedPtr<FJsonValue>> WeightsArray;
	WeightsJson->SetArrayField(TEXT("weights"), WeightsArray);

	Result = MakeShared<FJsonValueObject>(WeightsJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandlePaintLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Layer painting requires landscape edit mode"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleExportWeightmap(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Weightmap export requires LandscapeEditorUtils"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleImportWeightmap(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Weightmap import requires LandscapeEditorUtils"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetLandscapeMaterial(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	TSharedPtr<FJsonObject> MaterialJson = MakeShared<FJsonObject>();

	UMaterialInterface* Material = Landscape->GetLandscapeMaterial();
	if (Material)
	{
		MaterialJson->SetStringField(TEXT("material"), Material->GetPathName());
		MaterialJson->SetStringField(TEXT("materialName"), Material->GetName());
	}
	else
	{
		MaterialJson->SetStringField(TEXT("material"), TEXT(""));
	}

	Result = MakeShared<FJsonValueObject>(MaterialJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleSetLandscapeMaterial(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString MaterialPath = Params->GetStringField(TEXT("materialPath"));

	if (Name.IsEmpty() || MaterialPath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name and materialPath parameters required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Material)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
		return true;
	}

	Landscape->LandscapeMaterial = Material;
	Landscape->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleListLandscapeComponents(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ComponentsArray;

	TArray<ULandscapeComponent*> Components;
	Landscape->GetComponents<ULandscapeComponent>(Components);

	for (ULandscapeComponent* Component : Components)
	{
		if (!Component) continue;

		TSharedPtr<FJsonObject> CompJson = MakeShared<FJsonObject>();
		CompJson->SetStringField(TEXT("name"), Component->GetName());
		CompJson->SetNumberField(TEXT("sectionBaseX"), Component->SectionBaseX);
		CompJson->SetNumberField(TEXT("sectionBaseY"), Component->SectionBaseY);
		CompJson->SetNumberField(TEXT("componentSizeQuads"), Component->ComponentSizeQuads);

		FVector Location = Component->GetComponentLocation();
		TSharedPtr<FJsonObject> LocJson = MakeShared<FJsonObject>();
		LocJson->SetNumberField(TEXT("x"), Location.X);
		LocJson->SetNumberField(TEXT("y"), Location.Y);
		LocJson->SetNumberField(TEXT("z"), Location.Z);
		CompJson->SetObjectField(TEXT("location"), LocJson);

		ComponentsArray.Add(MakeShared<FJsonValueObject>(CompJson));
	}

	Result = MakeShared<FJsonValueArray>(ComponentsArray);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetLandscapeComponentInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	FString ComponentName = Params->GetStringField(TEXT("componentName"));

	if (Name.IsEmpty() || ComponentName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name and componentName parameters required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	TArray<ULandscapeComponent*> Components;
	Landscape->GetComponents<ULandscapeComponent>(Components);

	for (ULandscapeComponent* Component : Components)
	{
		if (Component && Component->GetName() == ComponentName)
		{
			TSharedPtr<FJsonObject> CompJson = MakeShared<FJsonObject>();
			CompJson->SetStringField(TEXT("name"), Component->GetName());
			CompJson->SetNumberField(TEXT("sectionBaseX"), Component->SectionBaseX);
			CompJson->SetNumberField(TEXT("sectionBaseY"), Component->SectionBaseY);
			CompJson->SetNumberField(TEXT("componentSizeQuads"), Component->ComponentSizeQuads);
			CompJson->SetNumberField(TEXT("subsectionSizeQuads"), Component->SubsectionSizeQuads);
			CompJson->SetNumberField(TEXT("numSubsections"), Component->NumSubsections);

			FVector Location = Component->GetComponentLocation();
			TSharedPtr<FJsonObject> LocJson = MakeShared<FJsonObject>();
			LocJson->SetNumberField(TEXT("x"), Location.X);
			LocJson->SetNumberField(TEXT("y"), Location.Y);
			LocJson->SetNumberField(TEXT("z"), Location.Z);
			CompJson->SetObjectField(TEXT("location"), LocJson);

			FBox Bounds = Component->Bounds.GetBox();
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
			CompJson->SetObjectField(TEXT("bounds"), BoundsJson);

			Result = MakeShared<FJsonValueObject>(CompJson);
			return true;
		}
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	return true;
}

bool FUltimateControlLandscapeHandler::HandleGetLandscapeLODSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	TSharedPtr<FJsonObject> LODJson = MakeShared<FJsonObject>();
	LODJson->SetNumberField(TEXT("staticLightingLOD"), Landscape->StaticLightingLOD);
	LODJson->SetNumberField(TEXT("lodDistributionSetting"), Landscape->LODDistributionSetting);
	// Note: LODFalloff was removed in UE 5.6

	Result = MakeShared<FJsonValueObject>(LODJson);
	return true;
}

bool FUltimateControlLandscapeHandler::HandleSetLandscapeLODSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Name = Params->GetStringField(TEXT("name"));
	if (Name.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ALandscapeProxy* Landscape = FindLandscape(Name);
	if (!Landscape)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Landscape not found: %s"), *Name));
		return true;
	}

	if (Params->HasField(TEXT("staticLightingLOD")))
	{
		Landscape->StaticLightingLOD = static_cast<int32>(Params->GetIntegerField(TEXT("staticLightingLOD")));
	}

	if (Params->HasField(TEXT("lodDistributionSetting")))
	{
		Landscape->LODDistributionSetting = static_cast<float>(Params->GetNumberField(TEXT("lodDistributionSetting")));
	}

	Landscape->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}
