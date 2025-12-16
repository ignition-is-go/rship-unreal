// Copyright Rocketship. All Rights Reserved.

#include "RshipAssetStoreClient.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "TimerManager.h"
#include "Engine/World.h"

URshipAssetStoreClient::URshipAssetStoreClient()
	: bIsConnected(false)
	, ReconnectAttempts(0)
{
}

URshipAssetStoreClient::~URshipAssetStoreClient()
{
	Disconnect();
}

// ============================================================================
// Connection
// ============================================================================

void URshipAssetStoreClient::Connect(const FString& InServerUrl)
{
	if (bIsConnected)
	{
		Disconnect();
	}

	ServerUrl = InServerUrl;
	ReconnectAttempts = 0;

	// Build WebSocket URL
	FString WsUrl = ServerUrl;
	WsUrl = WsUrl.Replace(TEXT("http://"), TEXT("ws://"));
	WsUrl = WsUrl.Replace(TEXT("https://"), TEXT("wss://"));
	WsUrl += TEXT("/ws");

	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Connecting to %s"), *WsUrl);

	// Create WebSocket
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(WsUrl);

	// Bind handlers
	WebSocket->OnConnected().AddUObject(this, &URshipAssetStoreClient::OnWebSocketConnected);
	WebSocket->OnConnectionError().AddUObject(this, &URshipAssetStoreClient::OnWebSocketConnectionError);
	WebSocket->OnClosed().AddUObject(this, &URshipAssetStoreClient::OnWebSocketClosed);
	WebSocket->OnMessage().AddUObject(this, &URshipAssetStoreClient::OnWebSocketMessage);

	// Connect
	WebSocket->Connect();
}

void URshipAssetStoreClient::Disconnect()
{
	if (WebSocket.IsValid())
	{
		WebSocket->Close();
		WebSocket.Reset();
	}

	bIsConnected = false;
	CancelAllDownloads();

	// Clear reconnect timer
	if (UWorld* World = GEngine->GetCurrentPlayWorld())
	{
		World->GetTimerManager().ClearTimer(ReconnectTimerHandle);
	}
}

bool URshipAssetStoreClient::IsConnected() const
{
	return bIsConnected && WebSocket.IsValid() && WebSocket->IsConnected();
}

// ============================================================================
// WebSocket Handlers
// ============================================================================

void URshipAssetStoreClient::OnWebSocketConnected()
{
	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Connected to asset store"));
	bIsConnected = true;
	ReconnectAttempts = 0;

	OnConnected.Broadcast();
	OnConnectedNative.Broadcast();

	// Request initial asset list
	RequestAssetList();
}

void URshipAssetStoreClient::OnWebSocketConnectionError(const FString& Error)
{
	UE_LOG(LogTemp, Error, TEXT("RshipAssetStoreClient: Connection error: %s"), *Error);
	bIsConnected = false;

	OnError.Broadcast(Error);
	OnErrorNative.Broadcast(Error);
	AttemptReconnect();
}

void URshipAssetStoreClient::OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Connection closed (code=%d, reason=%s)"), StatusCode, *Reason);
	bIsConnected = false;

	OnDisconnected.Broadcast(Reason);
	OnDisconnectedNative.Broadcast(Reason);

	if (!bWasClean)
	{
		AttemptReconnect();
	}
}

void URshipAssetStoreClient::OnWebSocketMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipAssetStoreClient: Failed to parse message: %s"), *Message);
		return;
	}

	FString MessageType;
	if (!JsonObject->TryGetStringField(TEXT("type"), MessageType))
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipAssetStoreClient: Message missing 'type' field"));
		return;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	JsonObject->TryGetObjectField(TEXT("data"), DataObject);

	if (MessageType == TEXT("ObjectList") || MessageType == TEXT("ObjectListUpdated"))
	{
		if (DataObject)
		{
			ProcessObjectList(*DataObject);
		}
	}
	else if (MessageType == TEXT("ObjectAdded"))
	{
		if (DataObject)
		{
			ProcessObjectAdded(*DataObject);
		}
	}
	else if (MessageType == TEXT("ObjectRemoved"))
	{
		if (DataObject)
		{
			ProcessObjectRemoved(*DataObject);
		}
	}
	else if (MessageType == TEXT("Ping"))
	{
		// Respond with Pong
		if (WebSocket.IsValid() && WebSocket->IsConnected())
		{
			WebSocket->Send(TEXT("{\"type\":\"Pong\"}"));
		}
	}
	else if (MessageType == TEXT("Error"))
	{
		FString ErrorMessage;
		if (DataObject && (*DataObject)->TryGetStringField(TEXT("message"), ErrorMessage))
		{
			OnError.Broadcast(ErrorMessage);
			OnErrorNative.Broadcast(ErrorMessage);
		}
	}
}

void URshipAssetStoreClient::AttemptReconnect()
{
	if (ReconnectAttempts >= MaxReconnectAttempts)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipAssetStoreClient: Max reconnect attempts reached"));
		return;
	}

	ReconnectAttempts++;
	float Delay = FMath::Pow(2.0f, ReconnectAttempts - 1); // Exponential backoff

	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Reconnecting in %.1f seconds (attempt %d/%d)"),
		Delay, ReconnectAttempts, MaxReconnectAttempts);

	if (UWorld* World = GEngine->GetCurrentPlayWorld())
	{
		World->GetTimerManager().SetTimer(
			ReconnectTimerHandle,
			[this]()
			{
				Connect(ServerUrl);
			},
			Delay,
			false);
	}
}

// ============================================================================
// Message Processing
// ============================================================================

void URshipAssetStoreClient::ProcessObjectList(TSharedPtr<FJsonObject> Data)
{
	FScopeLock Lock(&CacheLock);

	CachedAssets.Empty();
	AssetMap.Empty();

	const TArray<TSharedPtr<FJsonValue>>* ObjectsArray = nullptr;
	if (!Data->TryGetArrayField(TEXT("objects"), ObjectsArray))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *ObjectsArray)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!Value->TryGetObject(ObjectPtr))
		{
			continue;
		}

		FRshipAssetInfo Info;
		(*ObjectPtr)->TryGetStringField(TEXT("object_key"), Info.ObjectKey);
		(*ObjectPtr)->TryGetStringField(TEXT("content_type"), Info.ContentType);
		(*ObjectPtr)->TryGetStringField(TEXT("e_tag"), Info.ETag);

		int64 Size = 0;
		if ((*ObjectPtr)->TryGetNumberField(TEXT("size"), Size))
		{
			Info.FileSize = Size;
		}

		// Extract filename from object key
		Info.FileName = FPaths::GetCleanFilename(Info.ObjectKey);
		Info.AssetType = GetAssetTypeFromFileName(Info.FileName);

		// Parse last modified timestamp
		FString LastModifiedStr;
		if ((*ObjectPtr)->TryGetStringField(TEXT("last_modified"), LastModifiedStr))
		{
			FDateTime::ParseIso8601(*LastModifiedStr, Info.LastModified);
		}

		CachedAssets.Add(Info);
		AssetMap.Add(Info.ObjectKey, Info);
	}

	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Received %d assets"), CachedAssets.Num());
	OnAssetListReceived.Broadcast(CachedAssets);
	OnAssetListReceivedNative.Broadcast(CachedAssets);
}

void URshipAssetStoreClient::ProcessObjectAdded(TSharedPtr<FJsonObject> Data)
{
	FRshipAssetInfo Info;
	Data->TryGetStringField(TEXT("object_key"), Info.ObjectKey);
	Data->TryGetStringField(TEXT("content_type"), Info.ContentType);
	Data->TryGetStringField(TEXT("e_tag"), Info.ETag);

	int64 Size = 0;
	if (Data->TryGetNumberField(TEXT("size"), Size))
	{
		Info.FileSize = Size;
	}

	Info.FileName = FPaths::GetCleanFilename(Info.ObjectKey);
	Info.AssetType = GetAssetTypeFromFileName(Info.FileName);

	FString LastModifiedStr;
	if (Data->TryGetStringField(TEXT("last_modified"), LastModifiedStr))
	{
		FDateTime::ParseIso8601(*LastModifiedStr, Info.LastModified);
	}

	{
		FScopeLock Lock(&CacheLock);
		CachedAssets.Add(Info);
		AssetMap.Add(Info.ObjectKey, Info);
	}

	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Asset added: %s"), *Info.ObjectKey);
	OnAssetAdded.Broadcast(Info);
}

void URshipAssetStoreClient::ProcessObjectRemoved(TSharedPtr<FJsonObject> Data)
{
	FString ObjectKey;
	if (!Data->TryGetStringField(TEXT("object_key"), ObjectKey))
	{
		return;
	}

	{
		FScopeLock Lock(&CacheLock);
		AssetMap.Remove(ObjectKey);
		CachedAssets.RemoveAll([&ObjectKey](const FRshipAssetInfo& Info)
		{
			return Info.ObjectKey == ObjectKey;
		});
	}

	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Asset removed: %s"), *ObjectKey);
	OnAssetRemoved.Broadcast(ObjectKey);
}

// ============================================================================
// Asset Listing
// ============================================================================

void URshipAssetStoreClient::RequestAssetList()
{
	if (!WebSocket.IsValid() || !WebSocket->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipAssetStoreClient: Cannot request asset list - not connected"));
		return;
	}

	WebSocket->Send(TEXT("{\"type\":\"ListObjectsRequest\"}"));
}

TArray<FRshipAssetInfo> URshipAssetStoreClient::GetAssetsByType(ERshipAssetType Type) const
{
	FScopeLock Lock(&CacheLock);

	TArray<FRshipAssetInfo> FilteredAssets;
	for (const FRshipAssetInfo& Info : CachedAssets)
	{
		if (Info.AssetType == Type)
		{
			FilteredAssets.Add(Info);
		}
	}
	return FilteredAssets;
}

// ============================================================================
// Downloads
// ============================================================================

void URshipAssetStoreClient::DownloadAsset(const FString& ObjectKey, bool bForceRedownload)
{
	// Check if already cached
	if (!bForceRedownload && IsAssetCached(ObjectKey))
	{
		FString LocalPath = GetCachedAssetPath(ObjectKey);
		OnDownloadComplete.Broadcast(ObjectKey, LocalPath);
		OnDownloadCompleteNative.Broadcast(ObjectKey, LocalPath);
		return;
	}

	// Check if already downloading
	if (ActiveDownloads.Contains(ObjectKey))
	{
		return;
	}

	// Build download URL
	// The asset store uses /assets/:asset_id/download.http endpoint
	FString EncodedKey = FGenericPlatformHttp::UrlEncode(ObjectKey);
	FString DownloadUrl = FString::Printf(TEXT("%s/assets/%s/download.http"), *ServerUrl, *EncodedKey);

	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Downloading %s"), *ObjectKey);

	// Create HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(DownloadUrl);
	Request->SetVerb(TEXT("GET"));

	// Bind completion callback
	Request->OnProcessRequestComplete().BindUObject(this, &URshipAssetStoreClient::OnDownloadRequestComplete, ObjectKey);

	// Track and start
	ActiveDownloads.Add(ObjectKey, Request);
	Request->ProcessRequest();
}

void URshipAssetStoreClient::DownloadAssets(const TArray<FString>& ObjectKeys, bool bForceRedownload)
{
	for (const FString& Key : ObjectKeys)
	{
		DownloadAsset(Key, bForceRedownload);
	}
}

void URshipAssetStoreClient::CancelDownload(const FString& ObjectKey)
{
	TSharedPtr<IHttpRequest>* RequestPtr = ActiveDownloads.Find(ObjectKey);
	if (RequestPtr && RequestPtr->IsValid())
	{
		(*RequestPtr)->CancelRequest();
	}
	ActiveDownloads.Remove(ObjectKey);
}

void URshipAssetStoreClient::CancelAllDownloads()
{
	for (auto& Pair : ActiveDownloads)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->CancelRequest();
		}
	}
	ActiveDownloads.Empty();
}

void URshipAssetStoreClient::OnDownloadRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString ObjectKey)
{
	ActiveDownloads.Remove(ObjectKey);

	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RshipAssetStoreClient: Download failed for %s"), *ObjectKey);
		OnDownloadFailed.Broadcast(ObjectKey, TEXT("Request failed"));
		OnDownloadFailedNative.Broadcast(ObjectKey, TEXT("Request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	if (ResponseCode != 200)
	{
		UE_LOG(LogTemp, Error, TEXT("RshipAssetStoreClient: Download failed for %s (HTTP %d)"), *ObjectKey, ResponseCode);
		FString ErrorMsg = FString::Printf(TEXT("HTTP %d"), ResponseCode);
		OnDownloadFailed.Broadcast(ObjectKey, ErrorMsg);
		OnDownloadFailedNative.Broadcast(ObjectKey, ErrorMsg);
		return;
	}

	// Save to cache
	FString LocalPath = ObjectKeyToLocalPath(ObjectKey);
	EnsureCacheDirectoryExists();

	// Ensure parent directory exists
	FString ParentDir = FPaths::GetPath(LocalPath);
	if (!IFileManager::Get().DirectoryExists(*ParentDir))
	{
		IFileManager::Get().MakeDirectory(*ParentDir, true);
	}

	// Write file
	const TArray<uint8>& Content = Response->GetContent();
	if (FFileHelper::SaveArrayToFile(Content, *LocalPath))
	{
		UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Downloaded %s to %s"), *ObjectKey, *LocalPath);
		OnDownloadComplete.Broadcast(ObjectKey, LocalPath);
		OnDownloadCompleteNative.Broadcast(ObjectKey, LocalPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RshipAssetStoreClient: Failed to save %s"), *LocalPath);
		OnDownloadFailed.Broadcast(ObjectKey, TEXT("Failed to save file"));
		OnDownloadFailedNative.Broadcast(ObjectKey, TEXT("Failed to save file"));
	}
}

void URshipAssetStoreClient::HandleDownloadProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived, FString ObjectKey)
{
	// Get total size from asset info if available
	int64 TotalBytes = 0;
	if (const FRshipAssetInfo* Info = AssetMap.Find(ObjectKey))
	{
		TotalBytes = Info->FileSize;
	}

	FRshipDownloadProgress Progress;
	Progress.ObjectKey = ObjectKey;
	Progress.BytesReceived = BytesReceived;
	Progress.TotalBytes = TotalBytes;
	Progress.Progress = (TotalBytes > 0) ? (float)BytesReceived / TotalBytes : 0.0f;

	OnDownloadProgress.Broadcast(Progress);
	OnDownloadProgressNative.Broadcast(Progress);
}

// ============================================================================
// Cache Management
// ============================================================================

bool URshipAssetStoreClient::IsAssetCached(const FString& ObjectKey) const
{
	FString LocalPath = ObjectKeyToLocalPath(ObjectKey);
	return IFileManager::Get().FileExists(*LocalPath);
}

FString URshipAssetStoreClient::GetCachedAssetPath(const FString& ObjectKey) const
{
	FString LocalPath = ObjectKeyToLocalPath(ObjectKey);
	if (IFileManager::Get().FileExists(*LocalPath))
	{
		return LocalPath;
	}
	return FString();
}

FString URshipAssetStoreClient::GetCacheDirectory() const
{
	return FPaths::ProjectSavedDir() / TEXT("RshipAssets");
}

int64 URshipAssetStoreClient::GetCacheSize() const
{
	int64 TotalSize = 0;
	FString CacheDir = GetCacheDirectory();

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *CacheDir, TEXT("*"), true, false);

	for (const FString& File : Files)
	{
		TotalSize += IFileManager::Get().FileSize(*File);
	}

	return TotalSize;
}

void URshipAssetStoreClient::ClearCache()
{
	FString CacheDir = GetCacheDirectory();
	IFileManager::Get().DeleteDirectory(*CacheDir, false, true);

	UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Cache cleared"));
}

void URshipAssetStoreClient::RemoveCachedAsset(const FString& ObjectKey)
{
	FString LocalPath = ObjectKeyToLocalPath(ObjectKey);
	if (IFileManager::Get().FileExists(*LocalPath))
	{
		IFileManager::Get().Delete(*LocalPath);
	}
}

// ============================================================================
// Sync Operations
// ============================================================================

void URshipAssetStoreClient::SyncGDTFLibrary()
{
	SyncAssetsByType(ERshipAssetType::GDTF);
}

void URshipAssetStoreClient::SyncMVRFiles()
{
	SyncAssetsByType(ERshipAssetType::MVR);
}

void URshipAssetStoreClient::SyncIESProfiles()
{
	SyncAssetsByType(ERshipAssetType::IES);
}

void URshipAssetStoreClient::SyncAssetsByType(ERshipAssetType Type)
{
	TArray<FRshipAssetInfo> Assets = GetAssetsByType(Type);

	TArray<FString> KeysToDownload;
	for (const FRshipAssetInfo& Info : Assets)
	{
		if (!IsAssetCached(Info.ObjectKey))
		{
			KeysToDownload.Add(Info.ObjectKey);
		}
	}

	if (KeysToDownload.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("RshipAssetStoreClient: Syncing %d %s files"),
			KeysToDownload.Num(),
			*UEnum::GetValueAsString(Type));

		DownloadAssets(KeysToDownload);
	}
}

// ============================================================================
// Helpers
// ============================================================================

ERshipAssetType URshipAssetStoreClient::GetAssetTypeFromFileName(const FString& FileName) const
{
	FString Extension = FPaths::GetExtension(FileName).ToLower();

	if (Extension == TEXT("gdtf"))
	{
		return ERshipAssetType::GDTF;
	}
	else if (Extension == TEXT("mvr"))
	{
		return ERshipAssetType::MVR;
	}
	else if (Extension == TEXT("ies"))
	{
		return ERshipAssetType::IES;
	}

	return ERshipAssetType::Other;
}

FString URshipAssetStoreClient::ObjectKeyToLocalPath(const FString& ObjectKey) const
{
	// Replace forward slashes with platform path separators
	FString LocalPath = GetCacheDirectory() / ObjectKey;
	FPaths::NormalizeFilename(LocalPath);
	return LocalPath;
}

void URshipAssetStoreClient::EnsureCacheDirectoryExists() const
{
	FString CacheDir = GetCacheDirectory();
	if (!IFileManager::Get().DirectoryExists(*CacheDir))
	{
		IFileManager::Get().MakeDirectory(*CacheDir, true);
	}
}
