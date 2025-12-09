// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for profiling and logging methods
 */
class ULTIMATECONTROL_API FUltimateControlProfilingHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlProfilingHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// profiling.getStats - Get engine stats
	bool HandleGetStats(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// profiling.getMemory - Get memory usage
	bool HandleGetMemory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// profiling.startTrace - Start profiling trace
	bool HandleStartTrace(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// profiling.stopTrace - Stop profiling trace
	bool HandleStopTrace(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// logging.getLogs - Get recent log messages
	bool HandleGetLogs(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// logging.getCategories - Get log categories
	bool HandleGetCategories(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// logging.setVerbosity - Set log verbosity
	bool HandleSetVerbosity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);
};
