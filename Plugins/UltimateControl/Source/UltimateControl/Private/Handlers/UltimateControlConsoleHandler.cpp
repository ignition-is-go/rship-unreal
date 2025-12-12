// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlConsoleHandler.h"
#include "UltimateControlSubsystem.h"
#include "UltimateControl.h"

#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Editor.h"

FUltimateControlConsoleHandler::FUltimateControlConsoleHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(
		TEXT("console.execute"),
		TEXT("Execute a console command"),
		TEXT("Console"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlConsoleHandler::HandleExecute),
		/* bIsDangerous */ true);

	RegisterMethod(
		TEXT("console.getVariable"),
		TEXT("Get the value of a console variable"),
		TEXT("Console"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlConsoleHandler::HandleGetVariable));

	RegisterMethod(
		TEXT("console.setVariable"),
		TEXT("Set the value of a console variable"),
		TEXT("Console"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlConsoleHandler::HandleSetVariable));

	RegisterMethod(
		TEXT("console.listVariables"),
		TEXT("List console variables matching a filter"),
		TEXT("Console"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlConsoleHandler::HandleListVariables));

	RegisterMethod(
		TEXT("console.listCommands"),
		TEXT("List available console commands"),
		TEXT("Console"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlConsoleHandler::HandleListCommands));
}

bool FUltimateControlConsoleHandler::HandleExecute(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Command;
	if (!RequireString(Params, TEXT("command"), Command, OutError))
	{
		return false;
	}

	// Validate command isn't dangerous
	// Block some potentially dangerous commands
	TArray<FString> BlockedCommands = {
		TEXT("exit"),
		TEXT("quit"),
		TEXT("crash"),
		TEXT("debug crash"),
	};

	FString CommandLower = Command.ToLower();
	for (const FString& Blocked : BlockedCommands)
	{
		if (CommandLower.StartsWith(Blocked))
		{
			OutError = UUltimateControlSubsystem::MakeError(
				EJsonRpcError::OperationFailed,
				FString::Printf(TEXT("Command '%s' is blocked for safety reasons"), *Command));
			return false;
		}
	}

	// Execute the command
	bool bSuccess = false;
	if (GEngine)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		bSuccess = GEngine->Exec(World, *Command);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bSuccess);
	ResultObj->SetStringField(TEXT("command"), Command);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlConsoleHandler::HandleGetVariable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString VariableName;
	if (!RequireString(Params, TEXT("name"), VariableName, OutError))
	{
		return false;
	}

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*VariableName);
	if (!CVar)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Console variable not found: %s"), *VariableName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), VariableName);
	ResultObj->SetStringField(TEXT("value"), CVar->GetString());
	ResultObj->SetStringField(TEXT("help"), CVar->GetHelp());

	// Determine type
	FString TypeStr = TEXT("String");
	if (CVar->IsVariableInt())
	{
		TypeStr = TEXT("Int");
		ResultObj->SetNumberField(TEXT("intValue"), CVar->GetInt());
	}
	else if (CVar->IsVariableFloat())
	{
		TypeStr = TEXT("Float");
		ResultObj->SetNumberField(TEXT("floatValue"), CVar->GetFloat());
	}
	else if (CVar->IsVariableBool())
	{
		TypeStr = TEXT("Bool");
		ResultObj->SetBoolField(TEXT("boolValue"), CVar->GetBool());
	}
	ResultObj->SetStringField(TEXT("type"), TypeStr);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlConsoleHandler::HandleSetVariable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString VariableName;
	FString Value;

	if (!RequireString(Params, TEXT("name"), VariableName, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("value"), Value, OutError))
	{
		return false;
	}

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*VariableName);
	if (!CVar)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Console variable not found: %s"), *VariableName));
		return false;
	}

	// Check if it's read-only
	if (CVar->TestFlags(ECVF_ReadOnly))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			FString::Printf(TEXT("Console variable is read-only: %s"), *VariableName));
		return false;
	}

	CVar->Set(*Value, ECVF_SetByCode);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), VariableName);
	ResultObj->SetStringField(TEXT("newValue"), CVar->GetString());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlConsoleHandler::HandleListVariables(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Filter = GetOptionalString(Params, TEXT("filter"), TEXT(""));
	int32 Limit = GetOptionalInt(Params, TEXT("limit"), 100);

	TArray<TSharedPtr<FJsonValue>> VariablesArray;

	// Iterate console variables
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda([&](const TCHAR* Name, IConsoleObject* Obj)
		{
			if (VariablesArray.Num() >= Limit)
			{
				return;
			}

			FString NameStr = Name;
			if (!Filter.IsEmpty() && !NameStr.Contains(Filter))
			{
				return;
			}

			IConsoleVariable* CVar = Obj->AsVariable();
			if (CVar)
			{
				TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
				VarObj->SetStringField(TEXT("name"), NameStr);
				VarObj->SetStringField(TEXT("value"), CVar->GetString());
				VarObj->SetStringField(TEXT("help"), CVar->GetHelp());
				VarObj->SetBoolField(TEXT("readOnly"), CVar->TestFlags(ECVF_ReadOnly));

				VariablesArray.Add(MakeShared<FJsonValueObject>(VarObj));
			}
		}),
		TEXT("")
	);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("variables"), VariablesArray);
	ResultObj->SetNumberField(TEXT("count"), VariablesArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlConsoleHandler::HandleListCommands(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Filter = GetOptionalString(Params, TEXT("filter"), TEXT(""));
	int32 Limit = GetOptionalInt(Params, TEXT("limit"), 100);

	TArray<TSharedPtr<FJsonValue>> CommandsArray;

	// Iterate console commands
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda([&](const TCHAR* Name, IConsoleObject* Obj)
		{
			if (CommandsArray.Num() >= Limit)
			{
				return;
			}

			FString NameStr = Name;
			if (!Filter.IsEmpty() && !NameStr.Contains(Filter))
			{
				return;
			}

			IConsoleCommand* Command = Obj->AsCommand();
			if (Command)
			{
				TSharedPtr<FJsonObject> CmdObj = MakeShared<FJsonObject>();
				CmdObj->SetStringField(TEXT("name"), NameStr);
				CmdObj->SetStringField(TEXT("help"), Command->GetHelp());

				CommandsArray.Add(MakeShared<FJsonValueObject>(CmdObj));
			}
		}),
		TEXT("")
	);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("commands"), CommandsArray);
	ResultObj->SetNumberField(TEXT("count"), CommandsArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
