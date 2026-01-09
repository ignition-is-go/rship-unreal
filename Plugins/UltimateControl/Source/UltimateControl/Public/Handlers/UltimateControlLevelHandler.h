// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for level and actor manipulation JSON-RPC methods
 */
class ULTIMATECONTROL_API FUltimateControlLevelHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlLevelHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// level.getCurrent - Get current level info
	bool HandleGetCurrent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// level.open - Open a level
	bool HandleOpen(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// level.save - Save current level
	bool HandleSave(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// level.list - List all levels in project
	bool HandleList(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// level.getStreamingLevels - Get streaming levels
	bool HandleGetStreamingLevels(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.list - List actors in level
	bool HandleListActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.get - Get actor details
	bool HandleGetActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.spawn - Spawn a new actor
	bool HandleSpawnActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.destroy - Destroy an actor
	bool HandleDestroyActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.setTransform - Set actor transform
	bool HandleSetTransform(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.getTransform - Get actor transform
	bool HandleGetTransform(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.setProperty - Set actor property
	bool HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.getProperty - Get actor property
	bool HandleGetActorProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.getComponents - Get actor components
	bool HandleGetComponents(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.addComponent - Add component to actor
	bool HandleAddComponent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// actor.callFunction - Call a function on an actor
	bool HandleCallFunction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// selection.get - Get selected actors
	bool HandleGetSelection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// selection.set - Set selected actors
	bool HandleSetSelection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// selection.focus - Focus viewport on selection
	bool HandleFocusSelection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// Helper to find actor by name or path
	AActor* FindActor(const FString& Identifier, TSharedPtr<FJsonObject>& OutError);

	// Convert actor to JSON
	TSharedPtr<FJsonObject> ActorToJson(AActor* Actor, bool bIncludeComponents = false);
};
