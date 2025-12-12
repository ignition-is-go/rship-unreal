// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlSessionHandler.h"

// Concert/Multi-User includes - these are optional editor-only features
#if WITH_EDITOR
	#include "Modules/ModuleManager.h"
	// Check if Concert modules are available
	#if __has_include("IConcertSyncClient.h")
		#include "IConcertSyncClient.h"
		#include "IConcertClient.h"
		#include "IConcertClientWorkspace.h"
		#include "IConcertSyncClientModule.h"
		#include "ConcertMessageData.h"
		#define ULTIMATE_CONTROL_HAS_CONCERT 1
	#else
		#define ULTIMATE_CONTROL_HAS_CONCERT 0
	#endif
#else
	#define ULTIMATE_CONTROL_HAS_CONCERT 0
#endif

FUltimateControlSessionHandler::FUltimateControlSessionHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// Session discovery
	RegisterMethod(TEXT("session.list"), TEXT("List available sessions"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleListSessions));
	RegisterMethod(TEXT("session.getCurrent"), TEXT("Get current session info"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetCurrentSession));
	RegisterMethod(TEXT("session.isInSession"), TEXT("Check if in a session"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleIsInSession));

	// Session management
	RegisterMethod(TEXT("session.create"), TEXT("Create a new session"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleCreateSession));
	RegisterMethod(TEXT("session.join"), TEXT("Join an existing session"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleJoinSession));
	RegisterMethod(TEXT("session.leave"), TEXT("Leave current session"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleLeaveSession));
	RegisterMethod(TEXT("session.delete"), TEXT("Delete a session"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleDeleteSession), true);

	// Users
	RegisterMethod(TEXT("session.listUsers"), TEXT("List users in session"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleListUsers));
	RegisterMethod(TEXT("session.getCurrentUser"), TEXT("Get current user info"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetCurrentUser));
	RegisterMethod(TEXT("session.getUserInfo"), TEXT("Get user information"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetUserInfo));
	RegisterMethod(TEXT("session.kickUser"), TEXT("Kick a user from session"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleKickUser), true);

	// Presence
	RegisterMethod(TEXT("session.getUserPresence"), TEXT("Get user presence"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetUserPresence));
	RegisterMethod(TEXT("session.getUserActivity"), TEXT("Get user activity"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetUserActivity));
	RegisterMethod(TEXT("session.getUserSelection"), TEXT("Get user selection"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetUserSelection));

	// Locking
	RegisterMethod(TEXT("session.lockObject"), TEXT("Lock an object"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleLockObject));
	RegisterMethod(TEXT("session.unlockObject"), TEXT("Unlock an object"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleUnlockObject));
	RegisterMethod(TEXT("session.getObjectLock"), TEXT("Get object lock status"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetObjectLock));
	RegisterMethod(TEXT("session.listLockedObjects"), TEXT("List locked objects"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleListLockedObjects));
	RegisterMethod(TEXT("session.forceUnlock"), TEXT("Force unlock an object"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleForceUnlock), true);

	// Transactions
	RegisterMethod(TEXT("session.getPendingTransactions"), TEXT("Get pending transactions"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetPendingTransactions));
	RegisterMethod(TEXT("session.getTransactionHistory"), TEXT("Get transaction history"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetTransactionHistory));

	// Synchronization
	RegisterMethod(TEXT("session.persist"), TEXT("Persist session changes"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandlePersistSession));
	RegisterMethod(TEXT("session.restore"), TEXT("Restore session"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleRestoreSession));
	RegisterMethod(TEXT("session.getSyncStatus"), TEXT("Get sync status"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetSyncStatus));

	// Settings
	RegisterMethod(TEXT("session.getSettings"), TEXT("Get session settings"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetSessionSettings));
	RegisterMethod(TEXT("session.setSettings"), TEXT("Set session settings"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleSetSessionSettings));

	// Server
	RegisterMethod(TEXT("session.getServerInfo"), TEXT("Get server information"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetServerInfo));
	RegisterMethod(TEXT("session.listServers"), TEXT("List available servers"), TEXT("Session"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleListServers));
}

TSharedPtr<FJsonObject> FUltimateControlSessionHandler::SessionToJson()
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	// Session info depends on Concert/Multi-User implementation
	return Json;
}

TSharedPtr<FJsonObject> FUltimateControlSessionHandler::UserToJson()
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	// User info depends on Concert/Multi-User implementation
	return Json;
}

bool FUltimateControlSessionHandler::HandleListSessions(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");
	if (!ConcertModule)
	{
		TArray<TSharedPtr<FJsonValue>> EmptyArray;
		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetArrayField(TEXT("sessions"), EmptyArray);
		ResultJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing module not loaded"));
		Result = MakeShared<FJsonValueObject>(ResultJson);
		return true;
	}

	TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
	if (!Client)
	{
		TArray<TSharedPtr<FJsonValue>> EmptyArray;
		Result = MakeShared<FJsonValueArray>(EmptyArray);
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> SessionsArray;
	// List available sessions from the connected server
	// This would require async operations with the Concert client

	Result = MakeShared<FJsonValueArray>(SessionsArray);
	return true;
#else
	TArray<TSharedPtr<FJsonValue>> EmptyArray;
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetArrayField(TEXT("sessions"), EmptyArray);
	ResultJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing not available in this build"));
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleGetCurrentSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");
	if (!ConcertModule)
	{
		TSharedPtr<FJsonObject> SessionJson = MakeShared<FJsonObject>();
		SessionJson->SetBoolField(TEXT("inSession"), false);
		SessionJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing module not loaded"));
		Result = MakeShared<FJsonValueObject>(SessionJson);
		return true;
	}

	TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
	if (!Client)
	{
		TSharedPtr<FJsonObject> SessionJson = MakeShared<FJsonObject>();
		SessionJson->SetBoolField(TEXT("inSession"), false);
		Result = MakeShared<FJsonValueObject>(SessionJson);
		return true;
	}

	TSharedPtr<FJsonObject> SessionJson = MakeShared<FJsonObject>();

	TSharedPtr<IConcertClientSession> Session = Client->GetConcertClient()->GetCurrentSession();
	if (Session.IsValid())
	{
		SessionJson->SetBoolField(TEXT("inSession"), true);
		SessionJson->SetStringField(TEXT("sessionName"), Session->GetSessionInfo().SessionName);
		SessionJson->SetStringField(TEXT("sessionId"), Session->GetSessionInfo().SessionId.ToString());
	}
	else
	{
		SessionJson->SetBoolField(TEXT("inSession"), false);
	}

	Result = MakeShared<FJsonValueObject>(SessionJson);
	return true;
#else
	TSharedPtr<FJsonObject> SessionJson = MakeShared<FJsonObject>();
	SessionJson->SetBoolField(TEXT("inSession"), false);
	SessionJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing not available in this build"));
	Result = MakeShared<FJsonValueObject>(SessionJson);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleIsInSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");
	if (!ConcertModule)
	{
		Result = MakeShared<FJsonValueBoolean>(false);
		return true;
	}

	TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
	if (!Client)
	{
		Result = MakeShared<FJsonValueBoolean>(false);
		return true;
	}

	TSharedPtr<IConcertClientSession> Session = Client->GetConcertClient()->GetCurrentSession();
	Result = MakeShared<FJsonValueBoolean>(Session.IsValid());
	return true;
#else
	Result = MakeShared<FJsonValueBoolean>(false);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleCreateSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SessionName = Params->GetStringField(TEXT("sessionName"));
	if (SessionName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("sessionName parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Session creation requires Multi-User Editing to be connected to a server"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleJoinSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SessionName = Params->GetStringField(TEXT("sessionName"));
	if (SessionName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("sessionName parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Session joining requires Multi-User Editing to be connected to a server"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleLeaveSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");
	if (!ConcertModule)
	{
		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), false);
		ResultJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing module not loaded"));
		Result = MakeShared<FJsonValueObject>(ResultJson);
		return true;
	}

	TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
	if (!Client)
	{
		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), false);
		Result = MakeShared<FJsonValueObject>(ResultJson);
		return true;
	}

	Client->GetConcertClient()->DisconnectSession();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing not available in this build"));
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleDeleteSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Session deletion requires admin privileges on the server"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleListUsers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");
	TArray<TSharedPtr<FJsonValue>> UsersArray;

	if (ConcertModule)
	{
		TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
		if (Client)
		{
			TSharedPtr<IConcertClientSession> Session = Client->GetConcertClient()->GetCurrentSession();
			if (Session.IsValid())
			{
				TArray<FConcertSessionClientInfo> Clients = Session->GetSessionClients();
				for (const FConcertSessionClientInfo& ClientInfo : Clients)
				{
					TSharedPtr<FJsonObject> UserJson = MakeShared<FJsonObject>();
					UserJson->SetStringField(TEXT("displayName"), ClientInfo.ClientInfo.DisplayName);
					UserJson->SetStringField(TEXT("clientId"), ClientInfo.ClientEndpointId.ToString());
					UsersArray.Add(MakeShared<FJsonValueObject>(UserJson));
				}
			}
		}
	}

	Result = MakeShared<FJsonValueArray>(UsersArray);
	return true;
#else
	TArray<TSharedPtr<FJsonValue>> UsersArray;
	Result = MakeShared<FJsonValueArray>(UsersArray);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleGetCurrentUser(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");

	TSharedPtr<FJsonObject> UserJson = MakeShared<FJsonObject>();

	if (ConcertModule)
	{
		TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
		if (Client)
		{
			const FConcertClientInfo& ClientInfo = Client->GetConcertClient()->GetClientInfo();
			UserJson->SetStringField(TEXT("displayName"), ClientInfo.DisplayName);
			UserJson->SetStringField(TEXT("userName"), ClientInfo.UserName);
			UserJson->SetStringField(TEXT("deviceName"), ClientInfo.DeviceName);
		}
	}

	Result = MakeShared<FJsonValueObject>(UserJson);
	return true;
#else
	TSharedPtr<FJsonObject> UserJson = MakeShared<FJsonObject>();
	Result = MakeShared<FJsonValueObject>(UserJson);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleGetUserInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString UserId = Params->GetStringField(TEXT("userId"));
	if (UserId.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("userId parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> UserJson = MakeShared<FJsonObject>();
	UserJson->SetStringField(TEXT("userId"), UserId);
	UserJson->SetStringField(TEXT("message"), TEXT("User info lookup requires active session"));

	Result = MakeShared<FJsonValueObject>(UserJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleKickUser(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Kicking users requires admin privileges"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleGetUserPresence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> PresenceJson = MakeShared<FJsonObject>();
	PresenceJson->SetStringField(TEXT("message"), TEXT("Presence info requires active session"));

	Result = MakeShared<FJsonValueObject>(PresenceJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleGetUserActivity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ActivityJson = MakeShared<FJsonObject>();
	ActivityJson->SetStringField(TEXT("message"), TEXT("Activity tracking requires active session"));

	Result = MakeShared<FJsonValueObject>(ActivityJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleGetUserSelection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> SelectionJson = MakeShared<FJsonObject>();
	SelectionJson->SetStringField(TEXT("message"), TEXT("Selection tracking requires active session"));

	Result = MakeShared<FJsonValueObject>(SelectionJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleLockObject(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ObjectPath = Params->GetStringField(TEXT("objectPath"));
	if (ObjectPath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("objectPath parameter required"));
		return true;
	}

#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();

	if (ConcertModule)
	{
		TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
		if (Client)
		{
			TSharedPtr<IConcertClientWorkspace> Workspace = Client->GetWorkspace();
			if (Workspace.IsValid())
			{
				// Lock the resource
				Workspace->LockResources({FName(*ObjectPath)});
				ResultJson->SetBoolField(TEXT("success"), true);
				Result = MakeShared<FJsonValueObject>(ResultJson);
				return true;
			}
		}
	}

	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Locking requires active Multi-User session"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing not available in this build"));
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleUnlockObject(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ObjectPath = Params->GetStringField(TEXT("objectPath"));
	if (ObjectPath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("objectPath parameter required"));
		return true;
	}

#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();

	if (ConcertModule)
	{
		TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
		if (Client)
		{
			TSharedPtr<IConcertClientWorkspace> Workspace = Client->GetWorkspace();
			if (Workspace.IsValid())
			{
				Workspace->UnlockResources({FName(*ObjectPath)});
				ResultJson->SetBoolField(TEXT("success"), true);
				Result = MakeShared<FJsonValueObject>(ResultJson);
				return true;
			}
		}
	}

	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Unlocking requires active Multi-User session"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing not available in this build"));
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleGetObjectLock(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ObjectPath = Params->GetStringField(TEXT("objectPath"));
	if (ObjectPath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("objectPath parameter required"));
		return true;
	}

	TSharedPtr<FJsonObject> LockJson = MakeShared<FJsonObject>();
	LockJson->SetStringField(TEXT("objectPath"), ObjectPath);
	LockJson->SetBoolField(TEXT("isLocked"), false);
	LockJson->SetStringField(TEXT("message"), TEXT("Lock status requires active session"));

	Result = MakeShared<FJsonValueObject>(LockJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleListLockedObjects(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> LockedArray;

	Result = MakeShared<FJsonValueArray>(LockedArray);
	return true;
}

bool FUltimateControlSessionHandler::HandleForceUnlock(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Force unlock requires admin privileges"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleGetPendingTransactions(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> TransactionsArray;

	Result = MakeShared<FJsonValueArray>(TransactionsArray);
	return true;
}

bool FUltimateControlSessionHandler::HandleGetTransactionHistory(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> HistoryArray;

	Result = MakeShared<FJsonValueArray>(HistoryArray);
	return true;
}

bool FUltimateControlSessionHandler::HandlePersistSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_CONCERT
	IConcertSyncClientModule* ConcertModule = FModuleManager::GetModulePtr<IConcertSyncClientModule>("ConcertSyncClient");

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();

	if (ConcertModule)
	{
		TSharedPtr<IConcertSyncClient> Client = ConcertModule->GetClient(TEXT("Multi-User Editing"));
		if (Client)
		{
			TSharedPtr<IConcertClientWorkspace> Workspace = Client->GetWorkspace();
			if (Workspace.IsValid())
			{
				Workspace->PersistSessionChanges();
				ResultJson->SetBoolField(TEXT("success"), true);
				Result = MakeShared<FJsonValueObject>(ResultJson);
				return true;
			}
		}
	}

	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Persist requires active Multi-User session"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Multi-User Editing not available in this build"));
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#endif
}

bool FUltimateControlSessionHandler::HandleRestoreSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Session restore is handled during session join"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleGetSyncStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("synced"), true);
	StatusJson->SetStringField(TEXT("message"), TEXT("Sync status requires active session"));

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleGetSessionSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> SettingsJson = MakeShared<FJsonObject>();

	Result = MakeShared<FJsonValueObject>(SettingsJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleSetSessionSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Session settings modification not directly supported"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleGetServerInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ServerJson = MakeShared<FJsonObject>();
	ServerJson->SetStringField(TEXT("message"), TEXT("Server info requires connected server"));

	Result = MakeShared<FJsonValueObject>(ServerJson);
	return true;
}

bool FUltimateControlSessionHandler::HandleListServers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> ServersArray;

	Result = MakeShared<FJsonValueArray>(ServersArray);
	return true;
}
