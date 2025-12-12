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

FUltimateControlOutlinerHandler::FUltimateControlOutlinerHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// Hierarchy
	RegisterMethod(TEXT("outliner.getHierarchy"), TEXT("Get full actor hierarchy"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetHierarchy));
	RegisterMethod(TEXT("outliner.getActorHierarchy"), TEXT("Get actor's child hierarchy"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorHierarchy));
	RegisterMethod(TEXT("outliner.getParent"), TEXT("Get actor's parent"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetParent));
	RegisterMethod(TEXT("outliner.setParent"), TEXT("Set actor's parent"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetParent));
	RegisterMethod(TEXT("outliner.detachFromParent"), TEXT("Detach actor from parent"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleDetachFromParent));
	RegisterMethod(TEXT("outliner.getChildren"), TEXT("Get actor's children"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetChildren));
	RegisterMethod(TEXT("outliner.getAllDescendants"), TEXT("Get all descendants of actor"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetAllDescendants));

	// Folders
	RegisterMethod(TEXT("outliner.listFolders"), TEXT("List outliner folders"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleListFolders));
	RegisterMethod(TEXT("outliner.createFolder"), TEXT("Create outliner folder"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleCreateFolder));
	RegisterMethod(TEXT("outliner.deleteFolder"), TEXT("Delete outliner folder"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleDeleteFolder));
	RegisterMethod(TEXT("outliner.renameFolder"), TEXT("Rename outliner folder"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleRenameFolder));
	RegisterMethod(TEXT("outliner.getActorFolder"), TEXT("Get actor's folder"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorFolder));
	RegisterMethod(TEXT("outliner.setActorFolder"), TEXT("Set actor's folder"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorFolder));
	RegisterMethod(TEXT("outliner.getActorsInFolder"), TEXT("Get actors in folder"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorsInFolder));

	// Labels and naming
	RegisterMethod(TEXT("outliner.getActorLabel"), TEXT("Get actor's display label"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorLabel));
	RegisterMethod(TEXT("outliner.setActorLabel"), TEXT("Set actor's display label"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorLabel));

	// Visibility
	RegisterMethod(TEXT("outliner.getActorHiddenInEditor"), TEXT("Get actor hidden in editor state"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorHiddenInEditor));
	RegisterMethod(TEXT("outliner.setActorHiddenInEditor"), TEXT("Set actor hidden in editor state"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorHiddenInEditor));
	RegisterMethod(TEXT("outliner.getActorHiddenInGame"), TEXT("Get actor hidden in game state"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorHiddenInGame));
	RegisterMethod(TEXT("outliner.setActorHiddenInGame"), TEXT("Set actor hidden in game state"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorHiddenInGame));

	// Locking
	RegisterMethod(TEXT("outliner.getActorLocked"), TEXT("Get actor locked state"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorLocked));
	RegisterMethod(TEXT("outliner.setActorLocked"), TEXT("Set actor locked state"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetActorLocked));

	// Tags
	RegisterMethod(TEXT("outliner.getActorTags"), TEXT("Get actor's tags"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorTags));
	RegisterMethod(TEXT("outliner.addActorTag"), TEXT("Add tag to actor"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleAddActorTag));
	RegisterMethod(TEXT("outliner.removeActorTag"), TEXT("Remove tag from actor"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleRemoveActorTag));
	RegisterMethod(TEXT("outliner.findActorsByTag"), TEXT("Find actors with tag"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleFindActorsByTag));

	// Layers
	RegisterMethod(TEXT("layer.list"), TEXT("List editor layers"), TEXT("Layer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleListLayers));
	RegisterMethod(TEXT("layer.create"), TEXT("Create editor layer"), TEXT("Layer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleCreateLayer));
	RegisterMethod(TEXT("layer.delete"), TEXT("Delete editor layer"), TEXT("Layer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleDeleteLayer));
	RegisterMethod(TEXT("layer.getActorLayers"), TEXT("Get actor's layers"), TEXT("Layer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetActorLayers));
	RegisterMethod(TEXT("layer.addActor"), TEXT("Add actor to layer"), TEXT("Layer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleAddActorToLayer));
	RegisterMethod(TEXT("layer.removeActor"), TEXT("Remove actor from layer"), TEXT("Layer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleRemoveActorFromLayer));
	RegisterMethod(TEXT("layer.setVisibility"), TEXT("Set layer visibility"), TEXT("Layer"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSetLayerVisibility));

	// Grouping
	RegisterMethod(TEXT("group.groupActors"), TEXT("Group actors together"), TEXT("Group"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGroupActors));
	RegisterMethod(TEXT("group.ungroupActors"), TEXT("Ungroup actors"), TEXT("Group"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleUngroupActors));
	RegisterMethod(TEXT("group.getMembers"), TEXT("Get group members"), TEXT("Group"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleGetGroupMembers));
	RegisterMethod(TEXT("group.lock"), TEXT("Lock actor group"), TEXT("Group"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleLockGroup));
	RegisterMethod(TEXT("group.unlock"), TEXT("Unlock actor group"), TEXT("Group"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleUnlockGroup));

	// Filtering/Search
	RegisterMethod(TEXT("outliner.search"), TEXT("Search actors by name/class"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleSearchActors));
	RegisterMethod(TEXT("outliner.filterByClass"), TEXT("Filter actors by class"), TEXT("Outliner"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlOutlinerHandler::HandleFilterActorsByClass));
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
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName and parentName parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Child actor not found: %s"), *ActorName));
		return true;
	}

	if (!ParentActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Parent actor not found: %s"), *ParentName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("path parameter required"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("path parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("oldPath and newPath parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("folder parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName || (*It)->GetName() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName and label parameters required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName and tag parameters required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName and tag parameters required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("tag parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Layers subsystem not available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> LayersArray;

	TArray<FName> AllLayers;
	LayersSubsystem->AddAllLayerNamesTo(AllLayers);

	for (const FName& LayerName : AllLayers)
	{
		TSharedPtr<FJsonObject> LayerJson = MakeShared<FJsonObject>();
		LayerJson->SetStringField(TEXT("name"), LayerName.ToString());
		// Note: Layer visibility API changed significantly in UE 5.6
		// Just report the layer exists; visibility tracking requires per-actor checks
		LayerJson->SetBoolField(TEXT("visible"), true);
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (!LayersSubsystem)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Layers subsystem not available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (!LayersSubsystem)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Layers subsystem not available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorName parameter required"));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		if ((*It)->GetActorLabel() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("layerName parameter required"));
		return true;
	}

	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	if (!LayersSubsystem)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Layers subsystem not available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("actorNames array parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("At least 2 actors required for grouping"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("groupName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Group not found: %s"), *GroupName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("groupName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Group not found: %s"), *GroupName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("groupName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Group not found: %s"), *GroupName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("groupName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Group not found: %s"), *GroupName));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("query parameter required"));
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
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("className parameter required"));
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
