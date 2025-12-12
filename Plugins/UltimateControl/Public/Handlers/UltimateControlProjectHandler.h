// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for project-related JSON-RPC methods
 */
class ULTIMATECONTROL_API FUltimateControlProjectHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlProjectHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// project.getInfo - Get project information
	bool HandleGetInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// project.getConfig - Get project configuration
	bool HandleGetConfig(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// project.listPlugins - List enabled plugins
	bool HandleListPlugins(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// project.getModules - List project modules
	bool HandleGetModules(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// project.save - Save all dirty packages
	bool HandleSave(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// project.getDirtyPackages - Get list of unsaved packages
	bool HandleGetDirtyPackages(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// project.compileBlueprints - Recompile all blueprints
	bool HandleCompileBlueprints(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// project.getRecentFiles - Get recently opened files
	bool HandleGetRecentFiles(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);
};
