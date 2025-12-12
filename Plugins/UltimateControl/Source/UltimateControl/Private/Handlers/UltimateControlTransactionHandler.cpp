// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlTransactionHandler.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UltimateControlTransactionHandler"

FUltimateControlTransactionHandler::FUltimateControlTransactionHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(TEXT("transaction.undo"), TEXT("Undo"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleUndo));
	RegisterMethod(TEXT("transaction.redo"), TEXT("Redo"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleRedo));
	RegisterMethod(TEXT("transaction.getUndoHistory"), TEXT("Get undo history"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleGetUndoHistory));
	RegisterMethod(TEXT("transaction.getRedoHistory"), TEXT("Get redo history"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleGetRedoHistory));
	RegisterMethod(TEXT("transaction.clearHistory"), TEXT("Clear history"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleClearHistory));
	RegisterMethod(TEXT("transaction.canUndo"), TEXT("Can undo"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleCanUndo));
	RegisterMethod(TEXT("transaction.canRedo"), TEXT("Can redo"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleCanRedo));
	RegisterMethod(TEXT("transaction.begin"), TEXT("Begin transaction"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleBeginTransaction));
	RegisterMethod(TEXT("transaction.end"), TEXT("End transaction"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleEndTransaction));
	RegisterMethod(TEXT("transaction.cancel"), TEXT("Cancel transaction"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleCancelTransaction));
	RegisterMethod(TEXT("transaction.isInTransaction"), TEXT("Is in transaction"), TEXT("Transaction"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlTransactionHandler::HandleIsInTransaction));
}

bool FUltimateControlTransactionHandler::HandleUndo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Count = 1;
	if (Params->HasField(TEXT("count")))
	{
		Count = FMath::Max(1, FMath::RoundToInt(Params->GetNumberField(TEXT("count"))));
	}

	UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans);
	if (!TransBuffer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Transaction buffer not available"));
		return false;
	}

	int32 UndoneCount = 0;
	for (int32 i = 0; i < Count; i++)
	{
		if (TransBuffer->CanUndo())
		{
			TransBuffer->Undo();
			UndoneCount++;
		}
		else
		{
			break;
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), UndoneCount > 0);
	ResultObj->SetNumberField(TEXT("undoneCount"), UndoneCount);
	ResultObj->SetBoolField(TEXT("canUndoMore"), TransBuffer->CanUndo());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleRedo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Count = 1;
	if (Params->HasField(TEXT("count")))
	{
		Count = FMath::Max(1, FMath::RoundToInt(Params->GetNumberField(TEXT("count"))));
	}

	UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans);
	if (!TransBuffer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Transaction buffer not available"));
		return false;
	}

	int32 RedoneCount = 0;
	for (int32 i = 0; i < Count; i++)
	{
		if (TransBuffer->CanRedo())
		{
			TransBuffer->Redo();
			RedoneCount++;
		}
		else
		{
			break;
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), RedoneCount > 0);
	ResultObj->SetNumberField(TEXT("redoneCount"), RedoneCount);
	ResultObj->SetBoolField(TEXT("canRedoMore"), TransBuffer->CanRedo());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleGetUndoHistory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Limit = 50;
	if (Params->HasField(TEXT("limit")))
	{
		Limit = FMath::Clamp(FMath::RoundToInt(Params->GetNumberField(TEXT("limit"))), 1, 500);
	}

	UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans);
	if (!TransBuffer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Transaction buffer not available"));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> HistoryArray;

	int32 UndoCount = TransBuffer->GetUndoCount();
	for (int32 i = 0; i < FMath::Min(UndoCount, Limit); i++)
	{
		const FTransaction* Transaction = TransBuffer->GetTransaction(TransBuffer->GetQueueLength() - 1 - i);
		if (Transaction)
		{
			TSharedPtr<FJsonObject> TransactionObj = MakeShared<FJsonObject>();
			TransactionObj->SetNumberField(TEXT("index"), i);
			TransactionObj->SetStringField(TEXT("title"), Transaction->GetTitle().ToString());
			// UE 5.6: Context is already FString, not FName - don't call ToString()
			TransactionObj->SetStringField(TEXT("context"), Transaction->GetContext().Context);
			HistoryArray.Add(MakeShared<FJsonValueObject>(TransactionObj));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("history"), HistoryArray);
	ResultObj->SetNumberField(TEXT("count"), HistoryArray.Num());
	ResultObj->SetNumberField(TEXT("totalUndoCount"), UndoCount);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleGetRedoHistory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Limit = 50;
	if (Params->HasField(TEXT("limit")))
	{
		Limit = FMath::Clamp(FMath::RoundToInt(Params->GetNumberField(TEXT("limit"))), 1, 500);
	}

	UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans);
	if (!TransBuffer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Transaction buffer not available"));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> HistoryArray;

	// Note: GetRedoCount() may not exist in UE 5.6, calculate from queue
	int32 QueueLength = TransBuffer->GetQueueLength();
	int32 UndoCount = TransBuffer->GetUndoCount();
	int32 RedoCount = QueueLength > UndoCount ? (QueueLength - UndoCount) : 0;

	for (int32 i = 0; i < FMath::Min(RedoCount, Limit); i++)
	{
		const FTransaction* Transaction = TransBuffer->GetTransaction(TransBuffer->GetQueueLength() + i);
		if (Transaction)
		{
			TSharedPtr<FJsonObject> TransactionObj = MakeShared<FJsonObject>();
			TransactionObj->SetNumberField(TEXT("index"), i);
			TransactionObj->SetStringField(TEXT("title"), Transaction->GetTitle().ToString());
			// UE 5.6: Context is already FString, not FName - don't call ToString()
			TransactionObj->SetStringField(TEXT("context"), Transaction->GetContext().Context);
			HistoryArray.Add(MakeShared<FJsonValueObject>(TransactionObj));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("history"), HistoryArray);
	ResultObj->SetNumberField(TEXT("count"), HistoryArray.Num());
	ResultObj->SetNumberField(TEXT("totalRedoCount"), RedoCount);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleClearHistory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans);
	if (!TransBuffer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Transaction buffer not available"));
		return false;
	}

	TransBuffer->Reset(LOCTEXT("ClearHistory", "Clear History"));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleCanUndo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans);
	if (!TransBuffer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Transaction buffer not available"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("canUndo"), TransBuffer->CanUndo());
	ResultObj->SetNumberField(TEXT("undoCount"), TransBuffer->GetUndoCount());

	// Get title of next undo action if available
	if (TransBuffer->CanUndo())
	{
		const FTransaction* NextUndo = TransBuffer->GetTransaction(TransBuffer->GetQueueLength() - 1);
		if (NextUndo)
		{
			ResultObj->SetStringField(TEXT("nextUndoTitle"), NextUndo->GetTitle().ToString());
		}
	}

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleCanRedo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans);
	if (!TransBuffer)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Transaction buffer not available"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("canRedo"), TransBuffer->CanRedo());

	// Note: GetRedoCount() may not exist in UE 5.6, calculate from queue
	int32 QueueLength = TransBuffer->GetQueueLength();
	int32 UndoCount = TransBuffer->GetUndoCount();
	int32 RedoCount = QueueLength > UndoCount ? (QueueLength - UndoCount) : 0;
	ResultObj->SetNumberField(TEXT("redoCount"), RedoCount);

	// Get title of next redo action if available
	if (TransBuffer->CanRedo())
	{
		const FTransaction* NextRedo = TransBuffer->GetTransaction(TransBuffer->GetQueueLength());
		if (NextRedo)
		{
			ResultObj->SetStringField(TEXT("nextRedoTitle"), NextRedo->GetTitle().ToString());
		}
	}

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleBeginTransaction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Description = TEXT("Remote Operation");
	if (Params->HasField(TEXT("description")))
	{
		Description = Params->GetStringField(TEXT("description"));
	}

	if (ActiveTransactionIndex != INDEX_NONE)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Transaction already in progress. Call transaction.end or transaction.cancel first."));
		return false;
	}

	// Begin a new transaction
	ActiveTransactionIndex = GEditor->BeginTransaction(FText::FromString(Description));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetNumberField(TEXT("transactionIndex"), ActiveTransactionIndex);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleEndTransaction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (ActiveTransactionIndex == INDEX_NONE)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No transaction in progress. Call transaction.begin first."));
		return false;
	}

	// End the transaction
	GEditor->EndTransaction();
	int32 CompletedTransaction = ActiveTransactionIndex;
	ActiveTransactionIndex = INDEX_NONE;

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetNumberField(TEXT("completedTransactionIndex"), CompletedTransaction);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleCancelTransaction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (ActiveTransactionIndex == INDEX_NONE)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No transaction in progress. Call transaction.begin first."));
		return false;
	}

	// Cancel the transaction
	GEditor->CancelTransaction(ActiveTransactionIndex);
	ActiveTransactionIndex = INDEX_NONE;

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlTransactionHandler::HandleIsInTransaction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("inTransaction"), ActiveTransactionIndex != INDEX_NONE);
	if (ActiveTransactionIndex != INDEX_NONE)
	{
		ResultObj->SetNumberField(TEXT("transactionIndex"), ActiveTransactionIndex);
	}
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

#undef LOCTEXT_NAMESPACE
