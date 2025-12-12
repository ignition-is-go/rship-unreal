// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlLevelHandler.h"
#include "UltimateControl.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Selection.h"
#include "ScopedTransaction.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/PropertyPortFlags.h"

FUltimateControlLevelHandler::FUltimateControlLevelHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// Level methods
	RegisterMethod(
		TEXT("level.getCurrent"),
		TEXT("Get information about the currently loaded level"),
		TEXT("Level"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleGetCurrent));

	RegisterMethod(
		TEXT("level.open"),
		TEXT("Open a level by path"),
		TEXT("Level"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleOpen));

	RegisterMethod(
		TEXT("level.save"),
		TEXT("Save the current level"),
		TEXT("Level"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleSave));

	RegisterMethod(
		TEXT("level.list"),
		TEXT("List all level assets in the project"),
		TEXT("Level"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleList));

	RegisterMethod(
		TEXT("level.getStreamingLevels"),
		TEXT("Get streaming levels in the current world"),
		TEXT("Level"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleGetStreamingLevels));

	// Actor methods
	RegisterMethod(
		TEXT("actor.list"),
		TEXT("List actors in the current level with optional filtering"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleListActors));

	RegisterMethod(
		TEXT("actor.get"),
		TEXT("Get detailed information about a specific actor"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleGetActor));

	RegisterMethod(
		TEXT("actor.spawn"),
		TEXT("Spawn a new actor in the level"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleSpawnActor));

	RegisterMethod(
		TEXT("actor.destroy"),
		TEXT("Destroy an actor from the level"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleDestroyActor),
		/* bIsDangerous */ true);

	RegisterMethod(
		TEXT("actor.setTransform"),
		TEXT("Set an actor's transform (location, rotation, scale)"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleSetTransform));

	RegisterMethod(
		TEXT("actor.getTransform"),
		TEXT("Get an actor's transform"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleGetTransform));

	RegisterMethod(
		TEXT("actor.setProperty"),
		TEXT("Set a property value on an actor"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleSetActorProperty));

	RegisterMethod(
		TEXT("actor.getProperty"),
		TEXT("Get a property value from an actor"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleGetActorProperty));

	RegisterMethod(
		TEXT("actor.getComponents"),
		TEXT("Get all components on an actor"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleGetComponents));

	RegisterMethod(
		TEXT("actor.addComponent"),
		TEXT("Add a new component to an actor"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleAddComponent));

	RegisterMethod(
		TEXT("actor.callFunction"),
		TEXT("Call a function on an actor"),
		TEXT("Actor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleCallFunction));

	// Selection methods
	RegisterMethod(
		TEXT("selection.get"),
		TEXT("Get currently selected actors"),
		TEXT("Selection"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleGetSelection));

	RegisterMethod(
		TEXT("selection.set"),
		TEXT("Set the selected actors"),
		TEXT("Selection"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleSetSelection));

	RegisterMethod(
		TEXT("selection.focus"),
		TEXT("Focus the viewport on the current selection"),
		TEXT("Selection"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLevelHandler::HandleFocusSelection));
}

AActor* FUltimateControlLevelHandler::FindActor(const FString& Identifier, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("No editor world available"));
		return nullptr;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();

	// Try to find by name first
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && (Actor->GetName() == Identifier || Actor->GetActorLabel() == Identifier))
		{
			return Actor;
		}
	}

	// Try by path
	AActor* Actor = FindObject<AActor>(World->GetCurrentLevel(), *Identifier);
	if (Actor)
	{
		return Actor;
	}

	OutError = UUltimateControlSubsystem::MakeError(
		EJsonRpcError::NotFound,
		FString::Printf(TEXT("Actor not found: %s"), *Identifier));
	return nullptr;
}

TSharedPtr<FJsonObject> FUltimateControlLevelHandler::ActorToJson(AActor* Actor, bool bIncludeComponents)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("name"), Actor->GetName());
	Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
	Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
	Obj->SetStringField(TEXT("path"), Actor->GetPathName());
	Obj->SetBoolField(TEXT("isHidden"), Actor->IsHidden());
	Obj->SetBoolField(TEXT("isEditorOnly"), Actor->IsEditorOnly());
	Obj->SetBoolField(TEXT("isSelectable"), Actor->IsSelectable());

	// Transform
	Obj->SetObjectField(TEXT("transform"), TransformToJson(Actor->GetActorTransform()));

	// Folder path
	Obj->SetStringField(TEXT("folderPath"), Actor->GetFolderPath().ToString());

	// Tags
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& Tag : Actor->Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	Obj->SetArrayField(TEXT("tags"), TagsArray);

	if (bIncludeComponents)
	{
		TArray<TSharedPtr<FJsonValue>> ComponentsArray;
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (Component)
			{
				TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
				CompObj->SetStringField(TEXT("name"), Component->GetName());
				CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());

				if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
				{
					CompObj->SetObjectField(TEXT("relativeTransform"), TransformToJson(SceneComp->GetRelativeTransform()));
					CompObj->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
				}

				ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
			}
		}
		Obj->SetArrayField(TEXT("components"), ComponentsArray);
	}

	return Obj;
}

bool FUltimateControlLevelHandler::HandleGetCurrent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("No editor world available"));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();

	TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
	LevelObj->SetStringField(TEXT("name"), World->GetMapName());
	LevelObj->SetStringField(TEXT("path"), World->GetPathName());

	// Count actors
	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ActorCount++;
	}
	LevelObj->SetNumberField(TEXT("actorCount"), ActorCount);

	// World settings
	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();
		SettingsObj->SetBoolField(TEXT("enableWorldBoundsChecks"), WorldSettings->bEnableWorldBoundsChecks);
		SettingsObj->SetBoolField(TEXT("enableWorldComposition"), WorldSettings->bEnableWorldComposition);
		LevelObj->SetObjectField(TEXT("worldSettings"), SettingsObj);
	}

	OutResult = MakeShared<FJsonValueObject>(LevelObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleOpen(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	bool bPromptSave = GetOptionalBool(Params, TEXT("promptSave"), true);

	// Convert asset path to map name
	FString MapName = Path;
	if (MapName.StartsWith(TEXT("/Game/")))
	{
		// Already in the correct format
	}
	else
	{
		MapName = TEXT("/Game/") + MapName;
	}

	// Load the map
	bool bSuccess = FEditorFileUtils::LoadMap(MapName, bPromptSave, /* bShowProgress */ false);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bSuccess);
	if (bSuccess && GEditor && GEditor->GetEditorWorldContext().World())
	{
		ResultObj->SetStringField(TEXT("loadedLevel"), GEditor->GetEditorWorldContext().World()->GetMapName());
	}

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleSave(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("No editor world available"));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	bool bSuccess = FEditorFileUtils::SaveLevel(World->GetCurrentLevel());

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bSuccess);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleList(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path = GetOptionalString(Params, TEXT("path"), TEXT("/Game"));
	bool bRecursive = GetOptionalBool(Params, TEXT("recursive"), true);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = bRecursive;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	TArray<TSharedPtr<FJsonValue>> LevelsArray;
	for (const FAssetData& Asset : AssetList)
	{
		TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
		LevelObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		LevelObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		LevelsArray.Add(MakeShared<FJsonValueObject>(LevelObj));
	}

	OutResult = MakeShared<FJsonValueArray>(LevelsArray);
	return true;
}

bool FUltimateControlLevelHandler::HandleGetStreamingLevels(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("No editor world available"));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	TArray<TSharedPtr<FJsonValue>> LevelsArray;

	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (StreamingLevel)
		{
			TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
			LevelObj->SetStringField(TEXT("packageName"), StreamingLevel->GetWorldAssetPackageName());
			LevelObj->SetBoolField(TEXT("isLoaded"), StreamingLevel->IsLevelLoaded());
			LevelObj->SetBoolField(TEXT("isVisible"), StreamingLevel->IsLevelVisible());
			LevelObj->SetBoolField(TEXT("shouldBeLoaded"), StreamingLevel->ShouldBeLoaded());
			LevelObj->SetBoolField(TEXT("shouldBeVisible"), StreamingLevel->ShouldBeVisible());
			LevelsArray.Add(MakeShared<FJsonValueObject>(LevelObj));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(LevelsArray);
	return true;
}

bool FUltimateControlLevelHandler::HandleListActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("No editor world available"));
		return false;
	}

	FString ClassFilter = GetOptionalString(Params, TEXT("class"), TEXT(""));
	FString TagFilter = GetOptionalString(Params, TEXT("tag"), TEXT(""));
	bool bIncludeHidden = GetOptionalBool(Params, TEXT("includeHidden"), true);
	int32 Limit = GetOptionalInt(Params, TEXT("limit"), 1000);

	UWorld* World = GEditor->GetEditorWorldContext().World();
	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (ActorsArray.Num() >= Limit)
		{
			break;
		}

		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Apply filters
		if (!bIncludeHidden && Actor->IsHidden())
		{
			continue;
		}

		if (!ClassFilter.IsEmpty())
		{
			if (!Actor->GetClass()->GetName().Contains(ClassFilter))
			{
				continue;
			}
		}

		if (!TagFilter.IsEmpty())
		{
			bool bHasTag = false;
			for (const FName& Tag : Actor->Tags)
			{
				if (Tag.ToString() == TagFilter)
				{
					bHasTag = true;
					break;
				}
			}
			if (!bHasTag)
			{
				continue;
			}
		}

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor, false)));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("actors"), ActorsArray);
	ResultObj->SetNumberField(TEXT("count"), ActorsArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleGetActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	OutResult = MakeShared<FJsonValueObject>(ActorToJson(Actor, /* bIncludeComponents */ true));
	return true;
}

bool FUltimateControlLevelHandler::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("No editor world available"));
		return false;
	}

	FString ClassName;
	if (!RequireString(Params, TEXT("class"), ClassName, OutError))
	{
		return false;
	}

	FString ActorName = GetOptionalString(Params, TEXT("name"), TEXT(""));

	// Parse transform
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;

	const TSharedPtr<FJsonObject>* LocationObj;
	if (Params->TryGetObjectField(TEXT("location"), LocationObj))
	{
		Location = JsonToVector(*LocationObj);
	}

	const TSharedPtr<FJsonObject>* RotationObj;
	if (Params->TryGetObjectField(TEXT("rotation"), RotationObj))
	{
		Rotation = JsonToRotator(*RotationObj);
	}

	// Find the class
	UClass* ActorClass = FindObject<UClass>(nullptr, *ClassName);
	if (!ActorClass)
	{
		ActorClass = LoadObject<UClass>(nullptr, *ClassName);
	}
	if (!ActorClass)
	{
		// Try common actor classes
		if (ClassName == TEXT("StaticMeshActor"))
		{
			ActorClass = AStaticMeshActor::StaticClass();
		}
		else if (ClassName == TEXT("PointLight"))
		{
			ActorClass = APointLight::StaticClass();
		}
		else if (ClassName == TEXT("SpotLight"))
		{
			ActorClass = ASpotLight::StaticClass();
		}
		else if (ClassName == TEXT("DirectionalLight"))
		{
			ActorClass = ADirectionalLight::StaticClass();
		}
		else if (ClassName == TEXT("CameraActor"))
		{
			ActorClass = ACameraActor::StaticClass();
		}
	}

	if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			FString::Printf(TEXT("Invalid actor class: %s"), *ClassName));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	if (!ActorName.IsEmpty())
	{
		SpawnParams.Name = FName(*ActorName);
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);

	if (!NewActor)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			TEXT("Failed to spawn actor"));
		return false;
	}

	// Apply label if specified
	if (!ActorName.IsEmpty())
	{
		NewActor->SetActorLabel(ActorName);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), NewActor->GetName());
	ResultObj->SetStringField(TEXT("path"), NewActor->GetPathName());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleDestroyActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	bool bSuccess = Actor->Destroy();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bSuccess);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleSetTransform(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	// Get current transform to use as defaults
	FVector Location = Actor->GetActorLocation();
	FRotator Rotation = Actor->GetActorRotation();
	FVector Scale = Actor->GetActorScale3D();

	const TSharedPtr<FJsonObject>* LocationObj;
	if (Params->TryGetObjectField(TEXT("location"), LocationObj))
	{
		Location = JsonToVector(*LocationObj);
	}

	const TSharedPtr<FJsonObject>* RotationObj;
	if (Params->TryGetObjectField(TEXT("rotation"), RotationObj))
	{
		Rotation = JsonToRotator(*RotationObj);
	}

	const TSharedPtr<FJsonObject>* ScaleObj;
	if (Params->TryGetObjectField(TEXT("scale"), ScaleObj))
	{
		Scale = JsonToVector(*ScaleObj);
	}

	// Create transaction for undo
	FScopedTransaction Transaction(FText::FromString(TEXT("Set Actor Transform")));
	Actor->Modify();

	Actor->SetActorLocation(Location);
	Actor->SetActorRotation(Rotation);
	Actor->SetActorScale3D(Scale);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetObjectField(TEXT("transform"), TransformToJson(Actor->GetActorTransform()));

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleGetTransform(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	OutResult = MakeShared<FJsonValueObject>(TransformToJson(Actor->GetActorTransform()));
	return true;
}

bool FUltimateControlLevelHandler::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	FString PropertyName;
	FString ValueStr;

	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("property"), PropertyName, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("value"), ValueStr, OutError))
	{
		return false;
	}

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Property not found: %s"), *PropertyName));
		return false;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);

	FScopedTransaction Transaction(FText::FromString(TEXT("Set Actor Property")));
	Actor->Modify();

	const TCHAR* ImportBuffer = *ValueStr;
	Property->ImportText_Direct(ImportBuffer, ValuePtr, Actor, PPF_None);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleGetActorProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	FString PropertyName;

	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("property"), PropertyName, OutError))
	{
		return false;
	}

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Property not found: %s"), *PropertyName));
		return false;
	}

	FString ValueStr;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
	Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, Actor, PPF_None);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("property"), PropertyName);
	ResultObj->SetStringField(TEXT("value"), ValueStr);
	ResultObj->SetStringField(TEXT("type"), Property->GetCPPType());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleGetComponents(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component)
		{
			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"), Component->GetName());
			CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
			CompObj->SetBoolField(TEXT("isActive"), Component->IsActive());

			if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
			{
				CompObj->SetObjectField(TEXT("relativeTransform"), TransformToJson(SceneComp->GetRelativeTransform()));
				CompObj->SetObjectField(TEXT("worldTransform"), TransformToJson(SceneComp->GetComponentTransform()));
				CompObj->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
			}

			ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(ComponentsArray);
	return true;
}

bool FUltimateControlLevelHandler::HandleAddComponent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	FString ComponentClass;
	FString ComponentName;

	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("class"), ComponentClass, OutError))
	{
		return false;
	}

	ComponentName = GetOptionalString(Params, TEXT("name"), TEXT(""));

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	UClass* CompClass = FindObject<UClass>(nullptr, *ComponentClass);
	if (!CompClass)
	{
		CompClass = LoadObject<UClass>(nullptr, *ComponentClass);
	}

	if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			FString::Printf(TEXT("Invalid component class: %s"), *ComponentClass));
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("Add Component")));
	Actor->Modify();

	UActorComponent* NewComponent = NewObject<UActorComponent>(Actor, CompClass, ComponentName.IsEmpty() ? NAME_None : FName(*ComponentName));
	if (!NewComponent)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("Failed to create component"));
		return false;
	}

	Actor->AddInstanceComponent(NewComponent);
	NewComponent->RegisterComponent();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), NewComponent->GetName());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleCallFunction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Identifier;
	FString FunctionName;

	if (!RequireString(Params, TEXT("actor"), Identifier, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("function"), FunctionName, OutError))
	{
		return false;
	}

	AActor* Actor = FindActor(Identifier, OutError);
	if (!Actor)
	{
		return false;
	}

	UFunction* Function = Actor->FindFunction(FName(*FunctionName));
	if (!Function)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Function not found: %s"), *FunctionName));
		return false;
	}

	// For now, only support functions with no parameters
	Actor->ProcessEvent(Function, nullptr);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleGetSelection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	TArray<TSharedPtr<FJsonValue>> SelectedArray;

	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		for (int32 i = 0; i < Selection->Num(); ++i)
		{
			AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
			if (Actor)
			{
				SelectedArray.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
			}
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("selected"), SelectedArray);
	ResultObj->SetNumberField(TEXT("count"), SelectedArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleSetSelection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	TArray<TSharedPtr<FJsonValue>> ActorNames = GetOptionalArray(Params, TEXT("actors"));
	bool bAddToSelection = GetOptionalBool(Params, TEXT("add"), false);

	if (!GEditor)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("Editor not available"));
		return false;
	}

	if (!bAddToSelection)
	{
		GEditor->SelectNone(false, true, false);
	}

	int32 SelectedCount = 0;
	for (const TSharedPtr<FJsonValue>& Value : ActorNames)
	{
		FString ActorName = Value->AsString();
		TSharedPtr<FJsonObject> Error;
		AActor* Actor = FindActor(ActorName, Error);
		if (Actor)
		{
			GEditor->SelectActor(Actor, true, true, true);
			SelectedCount++;
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetNumberField(TEXT("selectedCount"), SelectedCount);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLevelHandler::HandleFocusSelection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (GEditor)
	{
		AActor* SelectedActor = GEditor->GetSelectedActors()->GetTop<AActor>();
		if (SelectedActor)
		{
			// UE 5.6: MoveViewportCamerasToActor takes a reference or array, not a pointer
			TArray<AActor*> ActorsToFocus;
			ActorsToFocus.Add(SelectedActor);
			GEditor->MoveViewportCamerasToActor(ActorsToFocus, true);
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
