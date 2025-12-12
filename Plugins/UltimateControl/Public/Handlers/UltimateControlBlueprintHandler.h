// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * Handler for Blueprint-related JSON-RPC methods
 */
class ULTIMATECONTROL_API FUltimateControlBlueprintHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlBlueprintHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// blueprint.list - List all blueprints
	bool HandleList(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.get - Get blueprint details
	bool HandleGet(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.getGraphs - Get all graphs in a blueprint
	bool HandleGetGraphs(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.getNodes - Get nodes in a graph
	bool HandleGetNodes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.getVariables - Get blueprint variables
	bool HandleGetVariables(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.getFunctions - Get blueprint functions
	bool HandleGetFunctions(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.getEventDispatchers - Get event dispatchers
	bool HandleGetEventDispatchers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.compile - Compile a blueprint
	bool HandleCompile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.create - Create a new blueprint
	bool HandleCreate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.addVariable - Add a variable to blueprint
	bool HandleAddVariable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.addFunction - Add a function to blueprint
	bool HandleAddFunction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.addNode - Add a node to a graph
	bool HandleAddNode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.connectPins - Connect two pins
	bool HandleConnectPins(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// blueprint.deleteNode - Delete a node from graph
	bool HandleDeleteNode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// Helper methods
	TSharedPtr<FJsonObject> BlueprintToJson(UBlueprint* Blueprint);
	TSharedPtr<FJsonObject> GraphToJson(UEdGraph* Graph);
	TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node);
	UBlueprint* LoadBlueprint(const FString& Path, TSharedPtr<FJsonObject>& OutError);
};
