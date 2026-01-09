// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlSourceControlHandler.h"
#include "UltimateControlSubsystem.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"

FUltimateControlSourceControlHandler::FUltimateControlSourceControlHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// Provider status
	RegisterMethod(TEXT("sourceControl.getProviderStatus"), TEXT("Get provider status"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleGetProviderStatus));
	RegisterMethod(TEXT("sourceControl.getProviderName"), TEXT("Get provider name"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleGetProviderName));
	RegisterMethod(TEXT("sourceControl.isEnabled"), TEXT("Is enabled"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleIsEnabled));
	RegisterMethod(TEXT("sourceControl.connect"), TEXT("Connect"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleConnect));

	// File status
	RegisterMethod(TEXT("sourceControl.getFileStatus"), TEXT("Get file status"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleGetFileStatus));
	RegisterMethod(TEXT("sourceControl.getFilesStatus"), TEXT("Get files status"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleGetFilesStatus));
	RegisterMethod(TEXT("sourceControl.refreshStatus"), TEXT("Refresh status"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleRefreshStatus));

	// Basic operations
	RegisterMethod(TEXT("sourceControl.checkOut"), TEXT("Check out"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleCheckOut));
	RegisterMethod(TEXT("sourceControl.checkIn"), TEXT("Check in"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleCheckIn));
	RegisterMethod(TEXT("sourceControl.revert"), TEXT("Revert"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleRevert));
	RegisterMethod(TEXT("sourceControl.sync"), TEXT("Sync"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleSync));

	// Add/Delete
	RegisterMethod(TEXT("sourceControl.markForAdd"), TEXT("Mark for add"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleMarkForAdd));
	RegisterMethod(TEXT("sourceControl.markForDelete"), TEXT("Mark for delete"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleMarkForDelete));
	RegisterMethod(TEXT("sourceControl.move"), TEXT("Move"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleMove));

	// History
	RegisterMethod(TEXT("sourceControl.getHistory"), TEXT("Get history"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleGetHistory));
	RegisterMethod(TEXT("sourceControl.diff"), TEXT("Diff"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleDiff));
	RegisterMethod(TEXT("sourceControl.annotate"), TEXT("Annotate"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleAnnotate));

	// Changelists
	RegisterMethod(TEXT("sourceControl.listChangelists"), TEXT("List changelists"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleListChangelists));
	RegisterMethod(TEXT("sourceControl.getChangelist"), TEXT("Get changelist"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleGetChangelist));
	RegisterMethod(TEXT("sourceControl.createChangelist"), TEXT("Create changelist"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleCreateChangelist));
	RegisterMethod(TEXT("sourceControl.deleteChangelist"), TEXT("Delete changelist"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleDeleteChangelist));
	RegisterMethod(TEXT("sourceControl.moveToChangelist"), TEXT("Move to changelist"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleMoveToChangelist));
	RegisterMethod(TEXT("sourceControl.submitChangelist"), TEXT("Submit changelist"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleSubmitChangelist));

	// Shelving
	RegisterMethod(TEXT("sourceControl.shelve"), TEXT("Shelve"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleShelve));
	RegisterMethod(TEXT("sourceControl.unshelve"), TEXT("Unshelve"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleUnshelve));
	RegisterMethod(TEXT("sourceControl.deleteShelved"), TEXT("Delete shelved"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleDeleteShelved));

	// Branches
	RegisterMethod(TEXT("sourceControl.listBranches"), TEXT("List branches"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleListBranches));
	RegisterMethod(TEXT("sourceControl.getCurrentBranch"), TEXT("Get current branch"), TEXT("SourceControl"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSourceControlHandler::HandleGetCurrentBranch));
}

ISourceControlProvider* FUltimateControlSourceControlHandler::GetProvider()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (SourceControlModule.IsEnabled())
	{
		return &SourceControlModule.GetProvider();
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FUltimateControlSourceControlHandler::FileStateToJson(const FString& FilePath)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Json->SetStringField(TEXT("status"), TEXT("NoProvider"));
		return Json;
	}

	FSourceControlStatePtr State = Provider->GetState(FilePath, EStateCacheUsage::Use);
	if (State.IsValid())
	{
		Json->SetStringField(TEXT("path"), FilePath);
		Json->SetBoolField(TEXT("isCheckedOut"), State->IsCheckedOut());
		Json->SetBoolField(TEXT("isCheckedOutOther"), State->IsCheckedOutOther());
		Json->SetBoolField(TEXT("isAdded"), State->IsAdded());
		Json->SetBoolField(TEXT("isDeleted"), State->IsDeleted());
		Json->SetBoolField(TEXT("isModified"), State->IsModified());
		Json->SetBoolField(TEXT("isConflicted"), State->IsConflicted());
		Json->SetBoolField(TEXT("canCheckIn"), State->CanCheckIn());
		Json->SetBoolField(TEXT("canCheckOut"), State->CanCheckout());
		Json->SetBoolField(TEXT("canRevert"), State->CanRevert());
		Json->SetBoolField(TEXT("isSourceControlled"), State->IsSourceControlled());
		Json->SetBoolField(TEXT("isCurrent"), State->IsCurrent());

		if (State->IsCheckedOutOther())
		{
			Json->SetStringField(TEXT("checkedOutBy"), State->GetOtherUserBranchCheckedOuts());
		}
	}

	return Json;
}

FString FUltimateControlSourceControlHandler::SourceControlStateToString(ESourceControlStatus Status)
{
	switch (Status)
	{
		case ESourceControlStatus::Unknown: return TEXT("Unknown");
		case ESourceControlStatus::NotInDepot: return TEXT("NotInDepot");
		case ESourceControlStatus::NotCurrent: return TEXT("NotCurrent");
		case ESourceControlStatus::CheckedOutOther: return TEXT("CheckedOutOther");
		case ESourceControlStatus::OpenForAdd: return TEXT("OpenForAdd");
		case ESourceControlStatus::Deleted: return TEXT("Deleted");
		case ESourceControlStatus::MarkedForDelete: return TEXT("MarkedForDelete");
		case ESourceControlStatus::Branched: return TEXT("Branched");
		case ESourceControlStatus::Ignored: return TEXT("Ignored");
		default: return TEXT("Unknown");
	}
}

bool FUltimateControlSourceControlHandler::HandleGetProviderStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("enabled"), SourceControlModule.IsEnabled());

	if (SourceControlModule.IsEnabled())
	{
		ISourceControlProvider& Provider = SourceControlModule.GetProvider();
		StatusJson->SetStringField(TEXT("providerName"), Provider.GetName().ToString());
		StatusJson->SetBoolField(TEXT("isAvailable"), Provider.IsAvailable());
	}

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleGetProviderName(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Result = MakeShared<FJsonValueString>(TEXT("None"));
		return true;
	}

	Result = MakeShared<FJsonValueString>(Provider->GetName().ToString());
	return true;
}

bool FUltimateControlSourceControlHandler::HandleIsEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	Result = MakeShared<FJsonValueBoolean>(SourceControlModule.IsEnabled());
	return true;
}

bool FUltimateControlSourceControlHandler::HandleConnect(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	ECommandResult::Type ConnectResult = Provider->Login(FString(), EConcurrency::Synchronous);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), ConnectResult == ECommandResult::Succeeded);
	ResultJson->SetBoolField(TEXT("isAvailable"), Provider->IsAvailable());

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleGetFileStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(FileStateToJson(FilePath));
	return true;
}

bool FUltimateControlSourceControlHandler::HandleGetFilesStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	const TArray<TSharedPtr<FJsonValue>>* FilePaths;
	if (!Params->TryGetArrayField(TEXT("filePaths"), FilePaths))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePaths array parameter required"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> StatusArray;
	for (const TSharedPtr<FJsonValue>& Value : *FilePaths)
	{
		FString FilePath = Value->AsString();
		StatusArray.Add(MakeShared<FJsonValueObject>(FileStateToJson(FilePath)));
	}

	Result = MakeShared<FJsonValueArray>(StatusArray);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleRefreshStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TArray<FString> Files;
	Files.Add(FilePath);

	ECommandResult::Type UpdateResult = Provider->Execute(ISourceControlOperation::Create<FUpdateStatus>(), Files);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), UpdateResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleCheckOut(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TArray<FString> Files;
	Files.Add(FilePath);

	ECommandResult::Type CheckOutResult = Provider->Execute(ISourceControlOperation::Create<FCheckOut>(), Files);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), CheckOutResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleCheckIn(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	FString Description = Params->GetStringField(TEXT("description"));

	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TArray<FString> Files;
	Files.Add(FilePath);

	TSharedRef<FCheckIn> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
	CheckInOperation->SetDescription(FText::FromString(Description));

	ECommandResult::Type CheckInResult = Provider->Execute(CheckInOperation, Files);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), CheckInResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleRevert(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TArray<FString> Files;
	Files.Add(FilePath);

	ECommandResult::Type RevertResult = Provider->Execute(ISourceControlOperation::Create<FRevert>(), Files);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), RevertResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleSync(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TArray<FString> Files;
	Files.Add(FilePath);

	ECommandResult::Type SyncResult = Provider->Execute(ISourceControlOperation::Create<FSync>(), Files);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), SyncResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleMarkForAdd(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TArray<FString> Files;
	Files.Add(FilePath);

	ECommandResult::Type AddResult = Provider->Execute(ISourceControlOperation::Create<FMarkForAdd>(), Files);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), AddResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleMarkForDelete(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TArray<FString> Files;
	Files.Add(FilePath);

	ECommandResult::Type DeleteResult = Provider->Execute(ISourceControlOperation::Create<FDelete>(), Files);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), DeleteResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleMove(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SourcePath = Params->GetStringField(TEXT("sourcePath"));
	FString DestPath = Params->GetStringField(TEXT("destPath"));

	if (SourcePath.IsEmpty() || DestPath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("sourcePath and destPath parameters required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TSharedRef<FCopy> CopyOperation = ISourceControlOperation::Create<FCopy>();
	CopyOperation->SetDestination(DestPath);

	TArray<FString> Files;
	Files.Add(SourcePath);

	ECommandResult::Type MoveResult = Provider->Execute(CopyOperation, Files);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), MoveResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleGetHistory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TSharedRef<FUpdateStatus> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory(true);

	TArray<FString> Files;
	Files.Add(FilePath);

	ECommandResult::Type HistoryResult = Provider->Execute(UpdateStatusOperation, Files);

	TArray<TSharedPtr<FJsonValue>> HistoryArray;

	if (HistoryResult == ECommandResult::Succeeded)
	{
		FSourceControlStatePtr State = Provider->GetState(FilePath, EStateCacheUsage::Use);
		if (State.IsValid())
		{
			for (int32 i = 0; i < State->GetHistorySize(); i++)
			{
				TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = State->GetHistoryItem(i);
				if (Revision.IsValid())
				{
					TSharedPtr<FJsonObject> RevJson = MakeShared<FJsonObject>();
					RevJson->SetStringField(TEXT("revision"), Revision->GetRevision());
					RevJson->SetStringField(TEXT("user"), Revision->GetUserName());
					RevJson->SetStringField(TEXT("description"), Revision->GetDescription());
					RevJson->SetStringField(TEXT("date"), Revision->GetDate().ToString());
					RevJson->SetNumberField(TEXT("changelistNumber"), Revision->GetCheckInIdentifier());
					HistoryArray.Add(MakeShared<FJsonValueObject>(RevJson));
				}
			}
		}
	}

	Result = MakeShared<FJsonValueArray>(HistoryArray);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleDiff(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FilePath = Params->GetStringField(TEXT("filePath"));
	if (FilePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("filePath parameter required"));
		return true;
	}

	// Diff operation typically opens an external diff tool
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Diff operation requires external diff tool. Use the editor's diff functionality."));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleAnnotate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Annotate not directly available. Use provider-specific commands."));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleListChangelists(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TArray<FSourceControlChangelistRef> Changelists = Provider->GetChangelists(EStateCacheUsage::Use);

	TArray<TSharedPtr<FJsonValue>> ChangelistsArray;
	for (const FSourceControlChangelistRef& Changelist : Changelists)
	{
		TSharedPtr<FJsonObject> CLJson = MakeShared<FJsonObject>();
		// Changelist details depend on the provider
		ChangelistsArray.Add(MakeShared<FJsonValueObject>(CLJson));
	}

	Result = MakeShared<FJsonValueArray>(ChangelistsArray);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleGetChangelist(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 ChangelistNumber = static_cast<int32>(Params->GetIntegerField(TEXT("changelist")));

	TSharedPtr<FJsonObject> CLJson = MakeShared<FJsonObject>();
	CLJson->SetNumberField(TEXT("changelist"), ChangelistNumber);
	CLJson->SetStringField(TEXT("message"), TEXT("Changelist details depend on the source control provider"));

	Result = MakeShared<FJsonValueObject>(CLJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleCreateChangelist(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Description = Params->GetStringField(TEXT("description"));

	ISourceControlProvider* Provider = GetProvider();
	if (!Provider)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Source control is not enabled"));
		return true;
	}

	TSharedRef<FNewChangelist> NewChangelistOperation = ISourceControlOperation::Create<FNewChangelist>();
	NewChangelistOperation->SetDescription(FText::FromString(Description));

	ECommandResult::Type CreateResult = Provider->Execute(NewChangelistOperation, TArray<FString>());

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), CreateResult == ECommandResult::Succeeded);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleDeleteChangelist(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Changelist deletion depends on provider. Use provider-specific commands."));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleMoveToChangelist(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Move to changelist depends on provider"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleSubmitChangelist(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Submit changelist through CheckIn operation"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleShelve(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Shelving depends on provider support"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleUnshelve(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Unshelving depends on provider support"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleDeleteShelved(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Delete shelved depends on provider support"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleListBranches(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> BranchesArray;

	// Branch listing depends on the source control provider
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("message"), TEXT("Branch listing depends on provider. Git, SVN, and Perforce have different branch concepts."));

	Result = MakeShared<FJsonValueArray>(BranchesArray);
	return true;
}

bool FUltimateControlSourceControlHandler::HandleGetCurrentBranch(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> BranchJson = MakeShared<FJsonObject>();
	BranchJson->SetStringField(TEXT("message"), TEXT("Current branch depends on provider"));

	// For Git, we could try to get the branch name
	// For Perforce, streams would be the equivalent

	Result = MakeShared<FJsonValueObject>(BranchJson);
	return true;
}
