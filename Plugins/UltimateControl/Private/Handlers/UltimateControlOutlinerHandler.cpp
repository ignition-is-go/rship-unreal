// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlOutlinerHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Layers/LayersSubsystem.h"
#include "ActorGroupingUtils.h"
#include "Editor/GroupActor.h"
#include "ActorFolder.h"
#include "LevelInstance/LevelInstanceSubsystem.h"

void FUltimateControlOutlinerHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	// Hierarchy
	Methods.Add(TEXT("outliner.getHierarchy"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetHierarchy));
	Methods.Add(TEXT("outliner.getActorHierarchy"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorHierarchy));
	Methods.Add(TEXT("outliner.getParent"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetParent));
	Methods.Add(TEXT("outliner.setParent"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetParent));
	Methods.Add(TEXT("outliner.detachFromParent"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleDetachFromParent));
	Methods.Add(TEXT("outliner.getChildren"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetChildren));
	Methods.Add(TEXT("outliner.getAllDescendants"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetAllDescendants));

	// Folders
	Methods.Add(TEXT("outliner.listFolders"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleListFolders));
	Methods.Add(TEXT("outliner.createFolder"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleCreateFolder));
	Methods.Add(TEXT("outliner.deleteFolder"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleDeleteFolder));
	Methods.Add(TEXT("outliner.renameFolder"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleRenameFolder));
	Methods.Add(TEXT("outliner.getActorFolder"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorFolder));
	Methods.Add(TEXT("outliner.setActorFolder"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorFolder));
	Methods.Add(TEXT("outliner.getActorsInFolder"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorsInFolder));

	// Labels and naming
	Methods.Add(TEXT("outliner.getActorLabel"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorLabel));
	Methods.Add(TEXT("outliner.setActorLabel"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorLabel));

	// Visibility
	Methods.Add(TEXT("outliner.getActorHiddenInEditor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorHiddenInEditor));
	Methods.Add(TEXT("outliner.setActorHiddenInEditor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorHiddenInEditor));
	Methods.Add(TEXT("outliner.getActorHiddenInGame"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorHiddenInGame));
	Methods.Add(TEXT("outliner.setActorHiddenInGame"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorHiddenInGame));

	// Locking
	Methods.Add(TEXT("outliner.getActorLocked"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorLocked));
	Methods.Add(TEXT("outliner.setActorLocked"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorLocked));

	// Tags
	Methods.Add(TEXT("outliner.getActorTags"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorTags));
	Methods.Add(TEXT("outliner.addActorTag"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleAddActorTag));
	Methods.Add(TEXT("outliner.removeActorTag"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleRemoveActorTag));
	Methods.Add(TEXT("outliner.findActorsByTag"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleFindActorsByTag));

	// Layers
	Methods.Add(TEXT("layer.list"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleListLayers));
	Methods.Add(TEXT("layer.create"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleCreateLayer));
	Methods.Add(TEXT("layer.delete"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleDeleteLayer));
	Methods.Add(TEXT("layer.getActorLayers"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorLayers));
	Methods.Add(TEXT("layer.addActor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleAddActorToLayer));
	Methods.Add(TEXT("layer.removeActor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleRemoveActorFromLayer));
	Methods.Add(TEXT("layer.setVisibility"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetLayerVisibility));

	// Grouping
	Methods.Add(TEXT("group.groupActors"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGroupActors));
	Methods.Add(TEXT("group.ungroupActors"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleUngroupActors));
	Methods.Add(TEXT("group.getMembers"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetGroupMembers));
	Methods.Add(TEXT("group.lock"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleLockGroup));
	Methods.Add(TEXT("group.unlock"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleUnlockGroup));

	// Filtering/Search
	Methods.Add(TEXT("outliner.search"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSearchActors));
	Methods.Add(TEXT("outliner.filterByClass"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleFilterActorsByClass));
}

TSharedPtr<FJsonObject> FUltimateControlOutlinerHandler::ActorHierarchyToJson(AActor* Actor, bool bRecursive)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Actor)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Actor->GetActorLabel());
	Json->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
	Json->SetBoolField(TEXT("hiddenInEditor"), Actor->IsTemporarilyHiddenInEditor());
	Json->SetBoolField(TEXT("hiddenInGame"), Actor->IsHidden());

	// Attached children
	TArray<AActor*> AttachedActors;
	Actor->GetAttachedActors(AttachedActors);

	if (bRecursive && AttachedActors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (AActor* Child : AttachedActors)
		{
			ChildrenArray.Add(MakeShared<FJsonValueObject>(ActorHierarchyToJson(Child, true)));
		}
		Json->SetArrayField(TEXT("children"), ChildrenArray);
	}
	else
	{
		Json->SetNumberField(TEXT("childCount"), AttachedActors.Num());
	}

	return Json;
}

void FUltimateControlOutlinerHandler::GetAllChildActors(AActor* Parent, TArray<AActor*>& OutChildren, bool bRecursive)
{
	if (!Parent)
	{
		return;
	}

	TArray<AActor*> DirectChildren;
	Parent->GetAttachedActors(DirectChildren);

	for (AActor* Child : DirectChildren)
	{
		OutChildren.Add(Child);
		if (bRecursive)
		{
			GetAllChildActors(Child, OutChildren, true);
		}
	}
}

bool FUltimateControlOutlinerHandler::HandleGetHierarchy(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> RootActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->GetAttachParentActor()) // Skip attached actors
		{
			continue;
		}

		RootActorsArray.Add(MakeShared<FJsonValueObject>(ActorHierarchyToJson(Actor, true)));
	}

	Result = MakeShared<FJsonValueArray>(RootActorsArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorHierarchy(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(ActorHierarchyToJson(FoundActor, true));
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetParent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	TSharedPtr<FJsonObject> ParentJson = MakeShared<FJsonObject>();

	AActor* Parent = FoundActor->GetAttachParentActor();
	if (Parent)
	{
		ParentJson->SetBoolField(TEXT("hasParent"), true);
		ParentJson->SetStringField(TEXT("parentName"), Parent->GetActorLabel());
		ParentJson->SetStringField(TEXT("parentClass"), Parent->GetClass()->GetName());
	}
	else
	{
		ParentJson->SetBoolField(TEXT("hasParent"), false);
	}

	Result = MakeShared<FJsonValueObject>(ParentJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleSetParent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	FString ParentName = Params->GetStringField(TEXT("parentName"));

	if (ActorName.IsEmpty() || ParentName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName and parentName parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* ChildActor = nullptr;
	AActor* ParentActor = nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetActorLabel() == ActorName) ChildActor = Actor;
		if (Actor->GetActorLabel() == ParentName) ParentActor = Actor;
	}

	if (!ChildActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Child actor not found: %s"), *ActorName));
		return true;
	}

	if (!ParentActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Parent actor not found: %s"), *ParentName));
		return true;
	}

	ChildActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleDetachFromParent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	FoundActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetChildren(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	TArray<AActor*> AttachedActors;
	FoundActor->GetAttachedActors(AttachedActors);

	TArray<TSharedPtr<FJsonValue>> ChildrenArray;
	for (AActor* Child : AttachedActors)
	{
		TSharedPtr<FJsonObject> ChildJson = MakeShared<FJsonObject>();
		ChildJson->SetStringField(TEXT("name"), Child->GetActorLabel());
		ChildJson->SetStringField(TEXT("class"), Child->GetClass()->GetName());
		ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildJson));
	}

	Result = MakeShared<FJsonValueArray>(ChildrenArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetAllDescendants(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	TArray<AActor*> AllDescendants;
	GetAllChildActors(FoundActor, AllDescendants, true);

	TArray<TSharedPtr<FJsonValue>> DescendantsArray;
	for (AActor* Descendant : AllDescendants)
	{
		TSharedPtr<FJsonObject> DescJson = MakeShared<FJsonObject>();
		DescJson->SetStringField(TEXT("name"), Descendant->GetActorLabel());
		DescJson->SetStringField(TEXT("class"), Descendant->GetClass()->GetName());
		DescendantsArray.Add(MakeShared<FJsonValueObject>(DescJson));
	}

	Result = MakeShared<FJsonValueArray>(DescendantsArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleListFolders(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TSet<FName> Folders;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor)
		{
			FName FolderPath = Actor->GetFolderPath();
			if (!FolderPath.IsNone())
			{
				Folders.Add(FolderPath);
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> FoldersArray;
	for (const FName& Folder : Folders)
	{
		FoldersArray.Add(MakeShared<FJsonValueString>(Folder.ToString()));
	}

	Result = MakeShared<FJsonValueArray>(FoldersArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleCreateFolder(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FolderPath = Params->GetStringField(TEXT("path"));
	if (FolderPath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("path parameter required"));
		return true;
	}

	// Folders are implicitly created when an actor is assigned to them
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetStringField(TEXT("path"), FolderPath);
	ResultJson->SetStringField(TEXT("message"), TEXT("Folder will be created when an actor is assigned to it"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleDeleteFolder(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FolderPath = Params->GetStringField(TEXT("path"));
	if (FolderPath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("path parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	// Remove all actors from the folder
	FName FolderName(*FolderPath);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetFolderPath() == FolderName)
		{
			Actor->SetFolderPath(NAME_None);
		}
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleRenameFolder(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString OldPath = Params->GetStringField(TEXT("oldPath"));
	FString NewPath = Params->GetStringField(TEXT("newPath"));

	if (OldPath.IsEmpty() || NewPath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("oldPath and newPath parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	FName OldFolderName(*OldPath);
	FName NewFolderName(*NewPath);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetFolderPath() == OldFolderName)
		{
			Actor->SetFolderPath(NewFolderName);
		}
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorFolder(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	TSharedPtr<FJsonObject> FolderJson = MakeShared<FJsonObject>();
	FName FolderPath = FoundActor->GetFolderPath();
	FolderJson->SetStringField(TEXT("folder"), FolderPath.IsNone() ? TEXT("") : FolderPath.ToString());
	FolderJson->SetBoolField(TEXT("hasFolder"), !FolderPath.IsNone());

	Result = MakeShared<FJsonValueObject>(FolderJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleSetActorFolder(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	FString FolderPath = Params->GetStringField(TEXT("folder"));

	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	FoundActor->SetFolderPath(FolderPath.IsEmpty() ? NAME_None : FName(*FolderPath));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorsInFolder(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FolderPath = Params->GetStringField(TEXT("folder"));
	if (FolderPath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("folder parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	FName FolderName(*FolderPath);
	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetFolderPath() == FolderName)
		{
			TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
			ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
			ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		}
	}

	Result = MakeShared<FJsonValueArray>(ActorsArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorLabel(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName || (*It)->GetName() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	Result = MakeShared<FJsonValueString>(FoundActor->GetActorLabel());
	return true;
}

bool FUltimateControlOutlinerHandler::HandleSetActorLabel(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	FString NewLabel = Params->GetStringField(TEXT("label"));

	if (ActorName.IsEmpty() || NewLabel.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName and label parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	FoundActor->SetActorLabel(NewLabel);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorHiddenInEditor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	Result = MakeShared<FJsonValueBoolean>(FoundActor->IsTemporarilyHiddenInEditor());
	return true;
}

bool FUltimateControlOutlinerHandler::HandleSetActorHiddenInEditor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	bool bHidden = Params->GetBoolField(TEXT("hidden"));

	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	FoundActor->SetIsTemporarilyHiddenInEditor(bHidden);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorHiddenInGame(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	Result = MakeShared<FJsonValueBoolean>(FoundActor->IsHidden());
	return true;
}

bool FUltimateControlOutlinerHandler::HandleSetActorHiddenInGame(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	bool bHidden = Params->GetBoolField(TEXT("hidden"));

	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	FoundActor->SetActorHiddenInGame(bHidden);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorLocked(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	Result = MakeShared<FJsonValueBoolean>(FoundActor->IsLockLocation());
	return true;
}

bool FUltimateControlOutlinerHandler::HandleSetActorLocked(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	bool bLocked = Params->GetBoolField(TEXT("locked"));

	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	FoundActor->SetLockLocation(bLocked);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorTags(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& Tag : FoundActor->Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}

	Result = MakeShared<FJsonValueArray>(TagsArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleAddActorTag(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	FString Tag = Params->GetStringField(TEXT("tag"));

	if (ActorName.IsEmpty() || Tag.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName and tag parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	FoundActor->Tags.AddUnique(FName(*Tag));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleRemoveActorTag(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	FString Tag = Params->GetStringField(TEXT("tag"));

	if (ActorName.IsEmpty() || Tag.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName and tag parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	FoundActor->Tags.Remove(FName(*Tag));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleFindActorsByTag(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Tag = Params->GetStringField(TEXT("tag"));
	if (Tag.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("tag parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	FName TagName(*Tag);
	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->Tags.Contains(TagName))
		{
			TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
			ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
			ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		}
	}

	Result = MakeShared<FJsonValueArray>(ActorsArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleListLayers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (!LayersSubsystem)
	{
		Error = CreateError(-32603, TEXT("Layers subsystem not available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> LayersArray;

	TArray<FName> AllLayers;
	LayersSubsystem->AddAllLayerNamesTo(AllLayers);

	for (const FName& LayerName : AllLayers)
	{
		TSharedPtr<FJsonObject> LayerJson = MakeShared<FJsonObject>();
		LayerJson->SetStringField(TEXT("name"), LayerName.ToString());
		LayerJson->SetBoolField(TEXT("visible"), LayersSubsystem->IsLayerVisible(LayerName));
		LayersArray.Add(MakeShared<FJsonValueObject>(LayerJson));
	}

	Result = MakeShared<FJsonValueArray>(LayersArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleCreateLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	if (LayerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("name parameter required"));
		return true;
	}

	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (!LayersSubsystem)
	{
		Error = CreateError(-32603, TEXT("Layers subsystem not available"));
		return true;
	}

	LayersSubsystem->CreateLayer(FName(*LayerName));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleDeleteLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("name"));
	if (LayerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("name parameter required"));
		return true;
	}

	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (!LayersSubsystem)
	{
		Error = CreateError(-32603, TEXT("Layers subsystem not available"));
		return true;
	}

	LayersSubsystem->DeleteLayer(FName(*LayerName));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetActorLayers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	if (ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("actorName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> LayersArray;
	for (const FName& Layer : FoundActor->Layers)
	{
		LayersArray.Add(MakeShared<FJsonValueString>(Layer.ToString()));
	}

	Result = MakeShared<FJsonValueArray>(LayersArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleAddActorToLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
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

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (LayersSubsystem)
	{
		LayersSubsystem->AddActorToLayer(FoundActor, FName(*LayerName));
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleRemoveActorFromLayer(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
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

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (LayersSubsystem)
	{
		LayersSubsystem->RemoveActorFromLayer(FoundActor, FName(*LayerName));
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleSetLayerVisibility(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayerName = Params->GetStringField(TEXT("layerName"));
	bool bVisible = Params->GetBoolField(TEXT("visible"));

	if (LayerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("layerName parameter required"));
		return true;
	}

	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (!LayersSubsystem)
	{
		Error = CreateError(-32603, TEXT("Layers subsystem not available"));
		return true;
	}

	LayersSubsystem->SetLayerVisibility(FName(*LayerName), bVisible);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGroupActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	const TArray<TSharedPtr<FJsonValue>>* ActorNames;
	if (!Params->TryGetArrayField(TEXT("actorNames"), ActorNames))
	{
		Error = CreateError(-32602, TEXT("actorNames array parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<AActor*> ActorsToGroup;
	for (const TSharedPtr<FJsonValue>& Value : *ActorNames)
	{
		FString Name = Value->AsString();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if ((*It)->GetActorLabel() == Name)
			{
				ActorsToGroup.Add(*It);
				break;
			}
		}
	}

	if (ActorsToGroup.Num() < 2)
	{
		Error = CreateError(-32602, TEXT("At least 2 actors required for grouping"));
		return true;
	}

	// Create group
	AGroupActor* GroupActor = UActorGroupingUtils::Get()->GroupActors(ActorsToGroup);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), GroupActor != nullptr);
	if (GroupActor)
	{
		ResultJson->SetStringField(TEXT("groupName"), GroupActor->GetActorLabel());
	}

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleUngroupActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString GroupName = Params->GetStringField(TEXT("groupName"));
	if (GroupName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("groupName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AGroupActor* GroupActor = nullptr;
	for (TActorIterator<AGroupActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == GroupName)
		{
			GroupActor = *It;
			break;
		}
	}

	if (!GroupActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Group not found: %s"), *GroupName));
		return true;
	}

	UActorGroupingUtils::Get()->UngroupActors({GroupActor});

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleGetGroupMembers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString GroupName = Params->GetStringField(TEXT("groupName"));
	if (GroupName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("groupName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AGroupActor* GroupActor = nullptr;
	for (TActorIterator<AGroupActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == GroupName)
		{
			GroupActor = *It;
			break;
		}
	}

	if (!GroupActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Group not found: %s"), *GroupName));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> MembersArray;

	TArray<AActor*> GroupedActors;
	GroupActor->GetGroupActors(GroupedActors);

	for (AActor* Actor : GroupedActors)
	{
		if (Actor)
		{
			TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
			ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
			ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			MembersArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		}
	}

	Result = MakeShared<FJsonValueArray>(MembersArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleLockGroup(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString GroupName = Params->GetStringField(TEXT("groupName"));
	if (GroupName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("groupName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AGroupActor* GroupActor = nullptr;
	for (TActorIterator<AGroupActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == GroupName)
		{
			GroupActor = *It;
			break;
		}
	}

	if (!GroupActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Group not found: %s"), *GroupName));
		return true;
	}

	GroupActor->Lock();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleUnlockGroup(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString GroupName = Params->GetStringField(TEXT("groupName"));
	if (GroupName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("groupName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	AGroupActor* GroupActor = nullptr;
	for (TActorIterator<AGroupActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == GroupName)
		{
			GroupActor = *It;
			break;
		}
	}

	if (!GroupActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Group not found: %s"), *GroupName));
		return true;
	}

	GroupActor->Unlock();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleSearchActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Query = Params->GetStringField(TEXT("query"));
	if (Query.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("query parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		FString Label = Actor->GetActorLabel();
		FString ClassName = Actor->GetClass()->GetName();

		if (Label.Contains(Query, ESearchCase::IgnoreCase) || ClassName.Contains(Query, ESearchCase::IgnoreCase))
		{
			TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
			ActorJson->SetStringField(TEXT("name"), Label);
			ActorJson->SetStringField(TEXT("class"), ClassName);
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		}
	}

	Result = MakeShared<FJsonValueArray>(ActorsArray);
	return true;
}

bool FUltimateControlOutlinerHandler::HandleFilterActorsByClass(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ClassName = Params->GetStringField(TEXT("className"));
	if (ClassName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("className parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		FString ActorClassName = Actor->GetClass()->GetName();
		if (ActorClassName.Contains(ClassName, ESearchCase::IgnoreCase))
		{
			TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
			ActorJson->SetStringField(TEXT("name"), Actor->GetActorLabel());
			ActorJson->SetStringField(TEXT("class"), ActorClassName);
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		}
	}

	Result = MakeShared<FJsonValueArray>(ActorsArray);
	return true;
}
