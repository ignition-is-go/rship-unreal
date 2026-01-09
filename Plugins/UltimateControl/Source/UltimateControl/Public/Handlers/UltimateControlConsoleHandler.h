// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for console command execution
 */
class ULTIMATECONTROL_API FUltimateControlConsoleHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlConsoleHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// console.execute - Execute a console command
	bool HandleExecute(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// console.getVariable - Get a console variable value
	bool HandleGetVariable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// console.setVariable - Set a console variable value
	bool HandleSetVariable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// console.listVariables - List console variables
	bool HandleListVariables(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// console.listCommands - List available console commands
	bool HandleListCommands(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);
};
