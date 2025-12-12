// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handlers/UltimateControlHandlerBase.h"

class AAIController;
class UBehaviorTree;
class UBlackboardComponent;
class UNavigationSystemV1;
class ANavigationData;

/**
 * Handler for AI and Navigation operations
 */
class ULTIMATECONTROL_API FUltimateControlAIHandler : public FUltimateControlHandlerBase
{
public:
	using FUltimateControlHandlerBase::FUltimateControlHandlerBase;

private:
	// Navigation mesh
	bool HandleBuildNavigation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleRebuildNavigation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetNavigationStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleClearNavigation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Path finding
	bool HandleFindPath(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleTestPath(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetRandomReachablePoint(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleProjectToNavigation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleIsNavigable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// AI Controllers
	bool HandleListAIControllers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetAIController(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSpawnAIController(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Movement control
	bool HandleMoveToLocation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleMoveToActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleStopMovement(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetMovementStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandlePauseMovement(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleResumeMovement(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Behavior Trees
	bool HandleListBehaviorTrees(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleRunBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleStopBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandlePauseBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleResumeBehaviorTree(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Blackboard
	bool HandleGetBlackboardValue(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetBlackboardValue(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleListBlackboardKeys(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleClearBlackboard(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Perception
	bool HandleGetPerceivedActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetPerceptionInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Focus
	bool HandleSetFocus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleClearFocus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetFocus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Helper methods
	AAIController* FindAIController(const FString& ControllerName);
	UBlackboardComponent* GetBlackboard(AAIController* Controller);
	TSharedPtr<FJsonObject> AIControllerToJson(AAIController* Controller);
	TSharedPtr<FJsonObject> BehaviorTreeToJson(UBehaviorTree* BehaviorTree);
	TSharedPtr<FJsonObject> PathToJson(const TArray<FVector>& PathPoints);
};
