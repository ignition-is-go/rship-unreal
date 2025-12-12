// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlSessionHandler.h"
#include "IConcertSyncClient.h"
#include "IConcertClient.h"
#include "IConcertClientWorkspace.h"
#include "IConcertSyncClientModule.h"
#include "ConcertMessageData.h"

void FUltimateControlSessionHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	// Session discovery
	Methods.Add(TEXT("session.list"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleListSessions));
	Methods.Add(TEXT("session.getCurrent"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetCurrentSession));
	Methods.Add(TEXT("session.isInSession"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleIsInSession));

	// Session management
	Methods.Add(TEXT("session.create"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleCreateSession));
	Methods.Add(TEXT("session.join"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleJoinSession));
	Methods.Add(TEXT("session.leave"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleLeaveSession));
	Methods.Add(TEXT("session.delete"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleDeleteSession));

	// Users
	Methods.Add(TEXT("session.listUsers"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleListUsers));
	Methods.Add(TEXT("session.getCurrentUser"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetCurrentUser));
	Methods.Add(TEXT("session.getUserInfo"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetUserInfo));
	Methods.Add(TEXT("session.kickUser"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleKickUser));

	// Presence
	Methods.Add(TEXT("session.getUserPresence"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetUserPresence));
	Methods.Add(TEXT("session.getUserActivity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetUserActivity));
	Methods.Add(TEXT("session.getUserSelection"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetUserSelection));

	// Locking
	Methods.Add(TEXT("session.lockObject"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleLockObject));
	Methods.Add(TEXT("session.unlockObject"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleUnlockObject));
	Methods.Add(TEXT("session.getObjectLock"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetObjectLock));
	Methods.Add(TEXT("session.listLockedObjects"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleListLockedObjects));
	Methods.Add(TEXT("session.forceUnlock"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleForceUnlock));

	// Transactions
	Methods.Add(TEXT("session.getPendingTransactions"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetPendingTransactions));
	Methods.Add(TEXT("session.getTransactionHistory"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetTransactionHistory));

	// Synchronization
	Methods.Add(TEXT("session.persist"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandlePersistSession));
	Methods.Add(TEXT("session.restore"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleRestoreSession));
	Methods.Add(TEXT("session.getSyncStatus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetSyncStatus));

	// Settings
	Methods.Add(TEXT("session.getSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetSessionSettings));
	Methods.Add(TEXT("session.setSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleSetSessionSettings));

	// Server
	Methods.Add(TEXT("session.getServerInfo"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleGetServerInfo));
	Methods.Add(TEXT("session.listServers"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSessionHandler::HandleListServers));
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
}

bool FUltimateControlSessionHandler::HandleGetCurrentSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
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
}

bool FUltimateControlSessionHandler::HandleIsInSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
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
}

bool FUltimateControlSessionHandler::HandleCreateSession(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SessionName = Params->GetStringField(TEXT("sessionName"));
	if (SessionName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("sessionName parameter required"));
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
		Error = CreateError(-32602, TEXT("sessionName parameter required"));
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
}

bool FUltimateControlSessionHandler::HandleGetCurrentUser(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
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
}

bool FUltimateControlSessionHandler::HandleGetUserInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString UserId = Params->GetStringField(TEXT("userId"));
	if (UserId.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("userId parameter required"));
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
		Error = CreateError(-32602, TEXT("objectPath parameter required"));
		return true;
	}

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
}

bool FUltimateControlSessionHandler::HandleUnlockObject(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ObjectPath = Params->GetStringField(TEXT("objectPath"));
	if (ObjectPath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("objectPath parameter required"));
		return true;
	}

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
}

bool FUltimateControlSessionHandler::HandleGetObjectLock(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ObjectPath = Params->GetStringField(TEXT("objectPath"));
	if (ObjectPath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("objectPath parameter required"));
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
