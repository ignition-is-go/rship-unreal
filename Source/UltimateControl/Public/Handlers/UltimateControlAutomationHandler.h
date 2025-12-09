// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for automation, testing, and build methods
 */
class ULTIMATECONTROL_API FUltimateControlAutomationHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlAutomationHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// automation.listTests - List available automation tests
	bool HandleListTests(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// automation.runTests - Run automation tests
	bool HandleRunTests(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// automation.getTestResults - Get test results
	bool HandleGetTestResults(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// build.cook - Cook content for a platform
	bool HandleCook(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// build.package - Package the project
	bool HandlePackage(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// build.getStatus - Get build status
	bool HandleGetStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// build.runUAT - Run Unreal Automation Tool command
	bool HandleRunUAT(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);
};
