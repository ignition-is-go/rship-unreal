// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlPIEHandler.h"
#include "UltimateControl.h"

#include "Editor.h"
#include "LevelEditor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

FUltimateControlPIEHandler::FUltimateControlPIEHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(
		TEXT("pie.play"),
		TEXT("Start Play In Editor session"),
		TEXT("PIE"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPIEHandler::HandlePlay));

	RegisterMethod(
		TEXT("pie.stop"),
		TEXT("Stop the current Play In Editor session"),
		TEXT("PIE"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPIEHandler::HandleStop));

	RegisterMethod(
		TEXT("pie.pause"),
		TEXT("Pause or resume the Play In Editor session"),
		TEXT("PIE"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPIEHandler::HandlePause));

	RegisterMethod(
		TEXT("pie.getState"),
		TEXT("Get the current Play In Editor state"),
		TEXT("PIE"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPIEHandler::HandleGetState));

	RegisterMethod(
		TEXT("pie.simulate"),
		TEXT("Start Simulate In Editor mode"),
		TEXT("PIE"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPIEHandler::HandleSimulate));

	RegisterMethod(
		TEXT("pie.eject"),
		TEXT("Eject from the player controller during PIE"),
		TEXT("PIE"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPIEHandler::HandleEject));

	RegisterMethod(
		TEXT("pie.possess"),
		TEXT("Possess a pawn during PIE"),
		TEXT("PIE"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPIEHandler::HandlePossess));
}

bool FUltimateControlPIEHandler::HandlePlay(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("Editor not available"));
		return false;
	}

	if (GEditor->IsPlaySessionInProgress())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("Play session already in progress"));
		return false;
	}

	// Configure play settings
	FString PlayMode = GetOptionalString(Params, TEXT("mode"), TEXT("SelectedViewport"));

	FRequestPlaySessionParams SessionParams;

	if (PlayMode == TEXT("MobilePreview"))
	{
		SessionParams.DestinationSlateViewport = nullptr;
		SessionParams.WorldType = EPlaySessionWorldType::PlayInEditor;
	}
	else if (PlayMode == TEXT("NewWindow"))
	{
		SessionParams.DestinationSlateViewport = nullptr;
		SessionParams.WorldType = EPlaySessionWorldType::PlayInEditor;
	}
	else // SelectedViewport
	{
		SessionParams.WorldType = EPlaySessionWorldType::PlayInEditor;
	}

	// Start play session
	GEditor->RequestPlaySession(SessionParams);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("mode"), PlayMode);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPIEHandler::HandleStop(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("Editor not available"));
		return false;
	}

	if (!GEditor->IsPlaySessionInProgress())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("No play session in progress"));
		return false;
	}

	GEditor->RequestEndPlayMap();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPIEHandler::HandlePause(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("Editor not available"));
		return false;
	}

	if (!GEditor->IsPlaySessionInProgress())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("No play session in progress"));
		return false;
	}

	bool bPause = GetOptionalBool(Params, TEXT("pause"), !GEditor->IsPlaySessionPaused());

	if (bPause != GEditor->IsPlaySessionPaused())
	{
		GEditor->PlaySessionPaused();
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetBoolField(TEXT("isPaused"), GEditor->IsPlaySessionPaused());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPIEHandler::HandleGetState(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("Editor not available"));
		return false;
	}

	TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
	StateObj->SetBoolField(TEXT("isPlaying"), GEditor->IsPlaySessionInProgress());
	StateObj->SetBoolField(TEXT("isPaused"), GEditor->IsPlaySessionPaused());
	StateObj->SetBoolField(TEXT("isSimulating"), GEditor->bIsSimulatingInEditor);

	// Get the PIE world if available
	if (GEditor->IsPlaySessionInProgress())
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				UWorld* PIEWorld = Context.World();
				if (PIEWorld)
				{
					StateObj->SetStringField(TEXT("worldName"), PIEWorld->GetMapName());
					StateObj->SetNumberField(TEXT("gameTime"), PIEWorld->GetTimeSeconds());

					// Get player controller info
					APlayerController* PC = PIEWorld->GetFirstPlayerController();
					if (PC)
					{
						TSharedPtr<FJsonObject> PlayerObj = MakeShared<FJsonObject>();
						PlayerObj->SetStringField(TEXT("name"), PC->GetName());
						if (PC->GetPawn())
						{
							PlayerObj->SetStringField(TEXT("pawnName"), PC->GetPawn()->GetName());
							PlayerObj->SetObjectField(TEXT("pawnLocation"), VectorToJson(PC->GetPawn()->GetActorLocation()));
						}
						StateObj->SetObjectField(TEXT("player"), PlayerObj);
					}
				}
				break;
			}
		}
	}

	OutResult = MakeShared<FJsonValueObject>(StateObj);
	return true;
}

bool FUltimateControlPIEHandler::HandleSimulate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("Editor not available"));
		return false;
	}

	if (GEditor->IsPlaySessionInProgress())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("Play session already in progress"));
		return false;
	}

	FRequestPlaySessionParams SessionParams;
	SessionParams.WorldType = EPlaySessionWorldType::SimulateInEditor;

	GEditor->RequestPlaySession(SessionParams);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPIEHandler::HandleEject(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	if (!GEditor)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InternalError, TEXT("Editor not available"));
		return false;
	}

	if (!GEditor->IsPlaySessionInProgress())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("No play session in progress"));
		return false;
	}

	GEditor->RequestToggleBetweenPIEandSIE();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPIEHandler::HandlePossess(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString PawnName;
	if (!RequireString(Params, TEXT("pawn"), PawnName, OutError))
	{
		return false;
	}

	if (!GEditor || !GEditor->IsPlaySessionInProgress())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("No play session in progress"));
		return false;
	}

	// Find the PIE world and pawn
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			UWorld* PIEWorld = Context.World();
			if (PIEWorld)
			{
				APlayerController* PC = PIEWorld->GetFirstPlayerController();
				if (PC)
				{
					// Find the pawn by name
					for (TActorIterator<APawn> It(PIEWorld); It; ++It)
					{
						APawn* Pawn = *It;
						if (Pawn && (Pawn->GetName() == PawnName || Pawn->GetActorLabel() == PawnName))
						{
							PC->Possess(Pawn);

							TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
							ResultObj->SetBoolField(TEXT("success"), true);
							ResultObj->SetStringField(TEXT("possessedPawn"), Pawn->GetName());

							OutResult = MakeShared<FJsonValueObject>(ResultObj);
							return true;
						}
					}
				}
			}
			break;
		}
	}

	OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::NotFound, FString::Printf(TEXT("Pawn not found: %s"), *PawnName));
	return false;
}
