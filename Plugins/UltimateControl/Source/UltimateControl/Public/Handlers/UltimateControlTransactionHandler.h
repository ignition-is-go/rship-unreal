// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for undo/redo transaction operations
 */
class ULTIMATECONTROL_API FUltimateControlTransactionHandler : public FUltimateControlHandlerBase
{
public:
	using FUltimateControlHandlerBase::FUltimateControlHandlerBase;

private:
	// Undo/Redo operations
	bool HandleUndo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleRedo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Transaction history
	bool HandleGetUndoHistory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetRedoHistory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleClearHistory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Transaction state
	bool HandleCanUndo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleCanRedo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Transaction management
	bool HandleBeginTransaction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleEndTransaction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleCancelTransaction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleIsInTransaction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Transaction tracking
	int32 ActiveTransactionIndex = INDEX_NONE;
};
