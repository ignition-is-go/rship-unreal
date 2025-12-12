// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlAIHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AIModule/Classes/AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "GameFramework/Pawn.h"
#include "AssetRegistry/AssetRegistryModule.h"

void FUltimateControlAIHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	// Navigation mesh
	Methods.Add(TEXT("navigation.build"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleBuildNavigation));
	Methods.Add(TEXT("navigation.rebuild"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleRebuildNavigation));
	Methods.Add(TEXT("navigation.getStatus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetNavigationStatus));
	Methods.Add(TEXT("navigation.clear"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleClearNavigation));

	// Path finding
	Methods.Add(TEXT("navigation.findPath"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleFindPath));
	Methods.Add(TEXT("navigation.testPath"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleTestPath));
	Methods.Add(TEXT("navigation.getRandomReachablePoint"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetRandomReachablePoint));
	Methods.Add(TEXT("navigation.projectToNavigation"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleProjectToNavigation));
	Methods.Add(TEXT("navigation.isNavigable"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleIsNavigable));

	// AI Controllers
	Methods.Add(TEXT("ai.listControllers"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleListAIControllers));
	Methods.Add(TEXT("ai.getController"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetAIController));
	Methods.Add(TEXT("ai.spawnController"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleSpawnAIController));

	// Movement control
	Methods.Add(TEXT("ai.moveToLocation"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleMoveToLocation));
	Methods.Add(TEXT("ai.moveToActor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleMoveToActor));
	Methods.Add(TEXT("ai.stopMovement"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleStopMovement));
	Methods.Add(TEXT("ai.getMovementStatus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetMovementStatus));
	Methods.Add(TEXT("ai.pauseMovement"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandlePauseMovement));
	Methods.Add(TEXT("ai.resumeMovement"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleResumeMovement));

	// Behavior Trees
	Methods.Add(TEXT("behaviorTree.list"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleListBehaviorTrees));
	Methods.Add(TEXT("behaviorTree.get"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetBehaviorTree));
	Methods.Add(TEXT("behaviorTree.run"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleRunBehaviorTree));
	Methods.Add(TEXT("behaviorTree.stop"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleStopBehaviorTree));
	Methods.Add(TEXT("behaviorTree.pause"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandlePauseBehaviorTree));
	Methods.Add(TEXT("behaviorTree.resume"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleResumeBehaviorTree));

	// Blackboard
	Methods.Add(TEXT("blackboard.getValue"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetBlackboardValue));
	Methods.Add(TEXT("blackboard.setValue"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleSetBlackboardValue));
	Methods.Add(TEXT("blackboard.listKeys"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleListBlackboardKeys));
	Methods.Add(TEXT("blackboard.clear"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleClearBlackboard));

	// Perception
	Methods.Add(TEXT("ai.getPerceivedActors"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetPerceivedActors));
	Methods.Add(TEXT("ai.getPerceptionInfo"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetPerceptionInfo));

	// Focus
	Methods.Add(TEXT("ai.setFocus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleSetFocus));
	Methods.Add(TEXT("ai.clearFocus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleClearFocus));
	Methods.Add(TEXT("ai.getFocus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAIHandler::HandleGetFocus));
}

AAIController* FUltimateControlAIHandler::FindAIController(const FString& ControllerName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AAIController> It(World); It; ++It)
	{
		AAIController* Controller = *It;
		if (Controller && Controller->GetName() == ControllerName)
		{
			return Controller;
		}
	}

	return nullptr;
}

UBlackboardComponent* FUltimateControlAIHandler::GetBlackboard(AAIController* Controller)
{
	if (!Controller)
	{
		return nullptr;
	}
	return Controller->GetBlackboardComponent();
}

TSharedPtr<FJsonObject> FUltimateControlAIHandler::AIControllerToJson(AAIController* Controller)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Controller)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Controller->GetName());
	Json->SetStringField(TEXT("class"), Controller->GetClass()->GetName());

	if (APawn* Pawn = Controller->GetPawn())
	{
		Json->SetStringField(TEXT("pawnName"), Pawn->GetActorLabel());
		Json->SetStringField(TEXT("pawnClass"), Pawn->GetClass()->GetName());
	}

	// Movement status
	if (UPathFollowingComponent* PathComp = Controller->GetPathFollowingComponent())
	{
		EPathFollowingStatus::Type Status = PathComp->GetStatus();
		FString StatusStr;
		switch (Status)
		{
			case EPathFollowingStatus::Idle: StatusStr = TEXT("Idle"); break;
			case EPathFollowingStatus::Waiting: StatusStr = TEXT("Waiting"); break;
			case EPathFollowingStatus::Paused: StatusStr = TEXT("Paused"); break;
			case EPathFollowingStatus::Moving: StatusStr = TEXT("Moving"); break;
			default: StatusStr = TEXT("Unknown"); break;
		}
		Json->SetStringField(TEXT("movementStatus"), StatusStr);
	}

	// Behavior tree
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(Controller->GetBrainComponent()))
	{
		if (UBehaviorTree* BT = BTComp->GetCurrentTree())
		{
			Json->SetStringField(TEXT("behaviorTree"), BT->GetName());
		}
		Json->SetBoolField(TEXT("behaviorTreeRunning"), BTComp->IsRunning());
		Json->SetBoolField(TEXT("behaviorTreePaused"), BTComp->IsPaused());
	}

	return Json;
}

TSharedPtr<FJsonObject> FUltimateControlAIHandler::BehaviorTreeToJson(UBehaviorTree* BehaviorTree)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!BehaviorTree)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), BehaviorTree->GetName());
	Json->SetStringField(TEXT("path"), BehaviorTree->GetPathName());

	if (BehaviorTree->BlackboardAsset)
	{
		Json->SetStringField(TEXT("blackboardAsset"), BehaviorTree->BlackboardAsset->GetName());
	}

	return Json;
}

TSharedPtr<FJsonObject> FUltimateControlAIHandler::PathToJson(const TArray<FVector>& PathPoints)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> PointsArray;
	for (const FVector& Point : PathPoints)
	{
		TSharedPtr<FJsonObject> PointJson = MakeShared<FJsonObject>();
		PointJson->SetNumberField(TEXT("x"), Point.X);
		PointJson->SetNumberField(TEXT("y"), Point.Y);
		PointJson->SetNumberField(TEXT("z"), Point.Z);
		PointsArray.Add(MakeShared<FJsonValueObject>(PointJson));
	}

	Json->SetArrayField(TEXT("points"), PointsArray);
	Json->SetNumberField(TEXT("pointCount"), PathPoints.Num());

	if (PathPoints.Num() >= 2)
	{
		float TotalLength = 0.0f;
		for (int32 i = 1; i < PathPoints.Num(); i++)
		{
			TotalLength += FVector::Dist(PathPoints[i - 1], PathPoints[i]);
		}
		Json->SetNumberField(TEXT("length"), TotalLength);
	}

	return Json;
}

bool FUltimateControlAIHandler::HandleBuildNavigation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		Error = CreateError(-32603, TEXT("Navigation system not available"));
		return true;
	}

	NavSys->Build();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetStringField(TEXT("message"), TEXT("Navigation build initiated"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleRebuildNavigation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	return HandleBuildNavigation(Params, Result, Error);
}

bool FUltimateControlAIHandler::HandleGetNavigationStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("available"), NavSys != nullptr);

	if (NavSys)
	{
		StatusJson->SetBoolField(TEXT("isNavigationBuildingNow"), NavSys->IsNavigationBuildingNow());
		StatusJson->SetBoolField(TEXT("isNavigationBuildingLocked"), NavSys->IsNavigationBuildingLocked());

		// Count nav mesh actors
		int32 NavMeshCount = 0;
		for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
		{
			NavMeshCount++;
		}
		StatusJson->SetNumberField(TEXT("navMeshCount"), NavMeshCount);
	}

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlAIHandler::HandleClearNavigation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		Error = CreateError(-32603, TEXT("Navigation system not available"));
		return true;
	}

	// Clear all nav data
	for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
	{
		ARecastNavMesh* NavMesh = *It;
		if (NavMesh)
		{
			NavMesh->RebuildAll();
		}
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleFindPath(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	double StartX = Params->GetNumberField(TEXT("startX"));
	double StartY = Params->GetNumberField(TEXT("startY"));
	double StartZ = Params->GetNumberField(TEXT("startZ"));
	double EndX = Params->GetNumberField(TEXT("endX"));
	double EndY = Params->GetNumberField(TEXT("endY"));
	double EndZ = Params->GetNumberField(TEXT("endZ"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		Error = CreateError(-32603, TEXT("Navigation system not available"));
		return true;
	}

	FVector Start(StartX, StartY, StartZ);
	FVector End(EndX, EndY, EndZ);

	FPathFindingQuery Query(nullptr, *NavSys->GetDefaultNavDataInstance(), Start, End);
	FPathFindingResult PathResult = NavSys->FindPathSync(Query);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), PathResult.IsSuccessful());

	if (PathResult.IsSuccessful() && PathResult.Path.IsValid())
	{
		TArray<FVector> PathPoints;
		for (const FNavPathPoint& PathPoint : PathResult.Path->GetPathPoints())
		{
			PathPoints.Add(PathPoint.Location);
		}
		ResultJson->SetObjectField(TEXT("path"), PathToJson(PathPoints));
	}

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleTestPath(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	double StartX = Params->GetNumberField(TEXT("startX"));
	double StartY = Params->GetNumberField(TEXT("startY"));
	double StartZ = Params->GetNumberField(TEXT("startZ"));
	double EndX = Params->GetNumberField(TEXT("endX"));
	double EndY = Params->GetNumberField(TEXT("endY"));
	double EndZ = Params->GetNumberField(TEXT("endZ"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		Error = CreateError(-32603, TEXT("Navigation system not available"));
		return true;
	}

	FVector Start(StartX, StartY, StartZ);
	FVector End(EndX, EndY, EndZ);

	FPathFindingQuery Query(nullptr, *NavSys->GetDefaultNavDataInstance(), Start, End);
	ENavigationQueryResult::Type QueryResult = NavSys->TestPathSync(Query);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("reachable"), QueryResult == ENavigationQueryResult::Success);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleGetRandomReachablePoint(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	double OriginX = Params->GetNumberField(TEXT("originX"));
	double OriginY = Params->GetNumberField(TEXT("originY"));
	double OriginZ = Params->GetNumberField(TEXT("originZ"));
	double Radius = Params->GetNumberField(TEXT("radius"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		Error = CreateError(-32603, TEXT("Navigation system not available"));
		return true;
	}

	FVector Origin(OriginX, OriginY, OriginZ);
	FNavLocation RandomLocation;

	bool bFound = NavSys->GetRandomReachablePointInRadius(Origin, Radius, RandomLocation);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("found"), bFound);

	if (bFound)
	{
		TSharedPtr<FJsonObject> PointJson = MakeShared<FJsonObject>();
		PointJson->SetNumberField(TEXT("x"), RandomLocation.Location.X);
		PointJson->SetNumberField(TEXT("y"), RandomLocation.Location.Y);
		PointJson->SetNumberField(TEXT("z"), RandomLocation.Location.Z);
		ResultJson->SetObjectField(TEXT("point"), PointJson);
	}

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleProjectToNavigation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));
	double Z = Params->GetNumberField(TEXT("z"));
	double QueryExtent = Params->HasField(TEXT("queryExtent")) ? Params->GetNumberField(TEXT("queryExtent")) : 100.0;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		Error = CreateError(-32603, TEXT("Navigation system not available"));
		return true;
	}

	FVector Point(X, Y, Z);
	FNavLocation ProjectedLocation;

	bool bProjected = NavSys->ProjectPointToNavigation(Point, ProjectedLocation, FVector(QueryExtent));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("projected"), bProjected);

	if (bProjected)
	{
		TSharedPtr<FJsonObject> PointJson = MakeShared<FJsonObject>();
		PointJson->SetNumberField(TEXT("x"), ProjectedLocation.Location.X);
		PointJson->SetNumberField(TEXT("y"), ProjectedLocation.Location.Y);
		PointJson->SetNumberField(TEXT("z"), ProjectedLocation.Location.Z);
		ResultJson->SetObjectField(TEXT("point"), PointJson);
	}

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleIsNavigable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));
	double Z = Params->GetNumberField(TEXT("z"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		Error = CreateError(-32603, TEXT("Navigation system not available"));
		return true;
	}

	FVector Point(X, Y, Z);
	FNavLocation NavLocation;

	bool bNavigable = NavSys->ProjectPointToNavigation(Point, NavLocation, FVector(50.0f));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("navigable"), bNavigable);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleListAIControllers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ControllersArray;

	for (TActorIterator<AAIController> It(World); It; ++It)
	{
		AAIController* Controller = *It;
		if (Controller)
		{
			ControllersArray.Add(MakeShared<FJsonValueObject>(AIControllerToJson(Controller)));
		}
	}

	Result = MakeShared<FJsonValueArray>(ControllersArray);
	return true;
}

bool FUltimateControlAIHandler::HandleGetAIController(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("name"));
	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("name parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(AIControllerToJson(Controller));
	return true;
}

bool FUltimateControlAIHandler::HandleSpawnAIController(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString PawnName = Params->GetStringField(TEXT("pawnName"));
	FString ControllerClass = Params->HasField(TEXT("controllerClass")) ? Params->GetStringField(TEXT("controllerClass")) : TEXT("AIController");

	if (PawnName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("pawnName parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	// Find the pawn
	APawn* TargetPawn = nullptr;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == PawnName)
		{
			TargetPawn = *It;
			break;
		}
	}

	if (!TargetPawn)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Pawn not found: %s"), *PawnName));
		return true;
	}

	// Spawn AI controller
	AAIController* Controller = World->SpawnActor<AAIController>();
	if (Controller)
	{
		Controller->Possess(TargetPawn);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetObjectField(TEXT("controller"), AIControllerToJson(Controller));
		Result = MakeShared<FJsonValueObject>(ResultJson);
	}
	else
	{
		Error = CreateError(-32603, TEXT("Failed to spawn AI controller"));
	}

	return true;
}

bool FUltimateControlAIHandler::HandleMoveToLocation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));
	double Z = Params->GetNumberField(TEXT("z"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	FVector Destination(X, Y, Z);
	EPathFollowingRequestResult::Type RequestResult = Controller->MoveToLocation(Destination);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), RequestResult == EPathFollowingRequestResult::RequestSuccessful);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleMoveToActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));
	FString TargetActorName = Params->GetStringField(TEXT("targetActorName"));

	if (ControllerName.IsEmpty() || TargetActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName and targetActorName parameters required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* TargetActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == TargetActorName)
		{
			TargetActor = *It;
			break;
		}
	}

	if (!TargetActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Target actor not found: %s"), *TargetActorName));
		return true;
	}

	EPathFollowingRequestResult::Type RequestResult = Controller->MoveToActor(TargetActor);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), RequestResult == EPathFollowingRequestResult::RequestSuccessful);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleStopMovement(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	Controller->StopMovement();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleGetMovementStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();

	if (UPathFollowingComponent* PathComp = Controller->GetPathFollowingComponent())
	{
		EPathFollowingStatus::Type Status = PathComp->GetStatus();
		FString StatusStr;
		switch (Status)
		{
			case EPathFollowingStatus::Idle: StatusStr = TEXT("Idle"); break;
			case EPathFollowingStatus::Waiting: StatusStr = TEXT("Waiting"); break;
			case EPathFollowingStatus::Paused: StatusStr = TEXT("Paused"); break;
			case EPathFollowingStatus::Moving: StatusStr = TEXT("Moving"); break;
			default: StatusStr = TEXT("Unknown"); break;
		}
		StatusJson->SetStringField(TEXT("status"), StatusStr);
	}

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlAIHandler::HandlePauseMovement(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	if (UPathFollowingComponent* PathComp = Controller->GetPathFollowingComponent())
	{
		PathComp->PauseMove();
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleResumeMovement(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	if (UPathFollowingComponent* PathComp = Controller->GetPathFollowingComponent())
	{
		PathComp->ResumeMove();
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleListBehaviorTrees(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(UBehaviorTree::StaticClass()->GetClassPathName(), AssetDataList);

	TArray<TSharedPtr<FJsonValue>> BTsArray;

	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> BTJson = MakeShared<FJsonObject>();
		BTJson->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		BTJson->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		BTsArray.Add(MakeShared<FJsonValueObject>(BTJson));
	}

	Result = MakeShared<FJsonValueArray>(BTsArray);
	return true;
}

bool FUltimateControlAIHandler::HandleGetBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path = Params->GetStringField(TEXT("path"));
	if (Path.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("path parameter required"));
		return true;
	}

	UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *Path);
	if (!BT)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Behavior tree not found: %s"), *Path));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(BehaviorTreeToJson(BT));
	return true;
}

bool FUltimateControlAIHandler::HandleRunBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));
	FString TreePath = Params->GetStringField(TEXT("treePath"));

	if (ControllerName.IsEmpty() || TreePath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName and treePath parameters required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *TreePath);
	if (!BT)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Behavior tree not found: %s"), *TreePath));
		return true;
	}

	bool bSuccess = Controller->RunBehaviorTree(BT);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), bSuccess);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleStopBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(Controller->GetBrainComponent()))
	{
		BTComp->StopTree();
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandlePauseBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(Controller->GetBrainComponent()))
	{
		BTComp->PauseLogic(TEXT("Paused via JSON-RPC"));
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleResumeBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(Controller->GetBrainComponent()))
	{
		BTComp->ResumeLogic(TEXT("Resumed via JSON-RPC"));
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleGetBlackboardValue(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));
	FString KeyName = Params->GetStringField(TEXT("keyName"));

	if (ControllerName.IsEmpty() || KeyName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName and keyName parameters required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UBlackboardComponent* BB = GetBlackboard(Controller);
	if (!BB)
	{
		Error = CreateError(-32603, TEXT("No blackboard component found"));
		return true;
	}

	TSharedPtr<FJsonObject> ValueJson = MakeShared<FJsonObject>();
	ValueJson->SetStringField(TEXT("keyName"), KeyName);

	// Try to get the value as different types
	FBlackboard::FKey KeyID = BB->GetKeyID(FName(*KeyName));
	if (KeyID != FBlackboard::InvalidKey)
	{
		// Get key type and value
		ValueJson->SetBoolField(TEXT("exists"), true);
	}
	else
	{
		ValueJson->SetBoolField(TEXT("exists"), false);
	}

	Result = MakeShared<FJsonValueObject>(ValueJson);
	return true;
}

bool FUltimateControlAIHandler::HandleSetBlackboardValue(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));
	FString KeyName = Params->GetStringField(TEXT("keyName"));

	if (ControllerName.IsEmpty() || KeyName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName and keyName parameters required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UBlackboardComponent* BB = GetBlackboard(Controller);
	if (!BB)
	{
		Error = CreateError(-32603, TEXT("No blackboard component found"));
		return true;
	}

	// Set value based on type provided
	if (Params->HasField(TEXT("floatValue")))
	{
		BB->SetValueAsFloat(FName(*KeyName), static_cast<float>(Params->GetNumberField(TEXT("floatValue"))));
	}
	else if (Params->HasField(TEXT("intValue")))
	{
		BB->SetValueAsInt(FName(*KeyName), static_cast<int32>(Params->GetIntegerField(TEXT("intValue"))));
	}
	else if (Params->HasField(TEXT("boolValue")))
	{
		BB->SetValueAsBool(FName(*KeyName), Params->GetBoolField(TEXT("boolValue")));
	}
	else if (Params->HasField(TEXT("stringValue")))
	{
		BB->SetValueAsString(FName(*KeyName), Params->GetStringField(TEXT("stringValue")));
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleListBlackboardKeys(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UBlackboardComponent* BB = GetBlackboard(Controller);
	if (!BB)
	{
		Error = CreateError(-32603, TEXT("No blackboard component found"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> KeysArray;

	if (const UBlackboardData* BBData = BB->GetBlackboardAsset())
	{
		for (const FBlackboardEntry& Key : BBData->Keys)
		{
			TSharedPtr<FJsonObject> KeyJson = MakeShared<FJsonObject>();
			KeyJson->SetStringField(TEXT("name"), Key.EntryName.ToString());
			if (Key.KeyType)
			{
				KeyJson->SetStringField(TEXT("type"), Key.KeyType->GetClass()->GetName());
			}
			KeysArray.Add(MakeShared<FJsonValueObject>(KeyJson));
		}
	}

	Result = MakeShared<FJsonValueArray>(KeysArray);
	return true;
}

bool FUltimateControlAIHandler::HandleClearBlackboard(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UBlackboardComponent* BB = GetBlackboard(Controller);
	if (!BB)
	{
		Error = CreateError(-32603, TEXT("No blackboard component found"));
		return true;
	}

	BB->ClearValue(NAME_None); // This clears all values

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleGetPerceivedActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UAIPerceptionComponent* PerceptionComp = Controller->GetAIPerceptionComponent();
	if (!PerceptionComp)
	{
		Error = CreateError(-32603, TEXT("No perception component found"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);

	for (AActor* Actor : PerceivedActors)
	{
		if (Actor)
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

bool FUltimateControlAIHandler::HandleGetPerceptionInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UAIPerceptionComponent* PerceptionComp = Controller->GetAIPerceptionComponent();

	TSharedPtr<FJsonObject> InfoJson = MakeShared<FJsonObject>();
	InfoJson->SetBoolField(TEXT("hasPerceptionComponent"), PerceptionComp != nullptr);

	if (PerceptionComp)
	{
		TArray<AActor*> PerceivedActors;
		PerceptionComp->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);
		InfoJson->SetNumberField(TEXT("perceivedActorCount"), PerceivedActors.Num());
	}

	Result = MakeShared<FJsonValueObject>(InfoJson);
	return true;
}

bool FUltimateControlAIHandler::HandleSetFocus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));
	FString TargetActorName = Params->GetStringField(TEXT("targetActorName"));

	if (ControllerName.IsEmpty() || TargetActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName and targetActorName parameters required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* TargetActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == TargetActorName)
		{
			TargetActor = *It;
			break;
		}
	}

	if (!TargetActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Target actor not found: %s"), *TargetActorName));
		return true;
	}

	Controller->SetFocus(TargetActor);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleClearFocus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	Controller->ClearFocus(EAIFocusPriority::Gameplay);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlAIHandler::HandleGetFocus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ControllerName = Params->GetStringField(TEXT("controllerName"));

	if (ControllerName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("controllerName parameter required"));
		return true;
	}

	AAIController* Controller = FindAIController(ControllerName);
	if (!Controller)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("AI Controller not found: %s"), *ControllerName));
		return true;
	}

	TSharedPtr<FJsonObject> FocusJson = MakeShared<FJsonObject>();

	AActor* FocusActor = Controller->GetFocusActor();
	if (FocusActor)
	{
		FocusJson->SetBoolField(TEXT("hasFocus"), true);
		FocusJson->SetStringField(TEXT("focusActorName"), FocusActor->GetActorLabel());
	}
	else
	{
		FocusJson->SetBoolField(TEXT("hasFocus"), false);
	}

	FVector FocalPoint = Controller->GetFocalPoint();
	TSharedPtr<FJsonObject> PointJson = MakeShared<FJsonObject>();
	PointJson->SetNumberField(TEXT("x"), FocalPoint.X);
	PointJson->SetNumberField(TEXT("y"), FocalPoint.Y);
	PointJson->SetNumberField(TEXT("z"), FocalPoint.Z);
	FocusJson->SetObjectField(TEXT("focalPoint"), PointJson);

	Result = MakeShared<FJsonValueObject>(FocusJson);
	return true;
}
