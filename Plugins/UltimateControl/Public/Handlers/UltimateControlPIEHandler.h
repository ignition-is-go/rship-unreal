// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for Play-In-Editor control methods
 */
class ULTIMATECONTROL_API FUltimateControlPIEHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlPIEHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// pie.play - Start Play In Editor
	bool HandlePlay(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// pie.stop - Stop Play In Editor
	bool HandleStop(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// pie.pause - Pause/Resume PIE
	bool HandlePause(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// pie.getState - Get PIE state
	bool HandleGetState(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// pie.simulate - Start Simulate In Editor
	bool HandleSimulate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// pie.eject - Eject from player (during PIE)
	bool HandleEject(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// pie.possess - Possess a pawn during PIE
	bool HandlePossess(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);
};
