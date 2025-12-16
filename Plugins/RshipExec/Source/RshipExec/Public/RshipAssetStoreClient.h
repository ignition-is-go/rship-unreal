// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "RshipAssetStoreClient.generated.h"

class IWebSocket;

/**
 * Asset type enumeration
 */
UENUM(BlueprintType)
enum class ERshipAssetType : uint8
{
	GDTF    UMETA(DisplayName = "GDTF"),
	MVR     UMETA(DisplayName = "MVR"),
	IES     UMETA(DisplayName = "IES"),
	Other   UMETA(DisplayName = "Other")
};

/**
 * Asset information from the asset store
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipAssetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	FString ObjectKey;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	FString FileName;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	ERshipAssetType AssetType;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	int64 FileSize;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	FString ContentType;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	FDateTime LastModified;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	FString ETag;

	FRshipAssetInfo()
		: AssetType(ERshipAssetType::Other)
		, FileSize(0)
	{}
};

/**
 * Download progress information
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDownloadProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	FString ObjectKey;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	int64 BytesReceived;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	int64 TotalBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|AssetStore")
	float Progress; // 0.0 - 1.0

	FRshipDownloadProgress()
		: BytesReceived(0)
		, TotalBytes(0)
		, Progress(0.0f)
	{}
};

// Dynamic delegates (for Blueprint)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAssetStoreConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetStoreDisconnected, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetStoreError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetListReceived, const TArray<FRshipAssetInfo>&, Assets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetAdded, const FRshipAssetInfo&, Asset);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetRemoved, const FString&, ObjectKey);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAssetDownloadComplete, const FString&, ObjectKey, const FString&, LocalPath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAssetDownloadFailed, const FString&, ObjectKey, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetDownloadProgress, const FRshipDownloadProgress&, Progress);

// Native delegates (for C++ binding without UObject requirement)
DECLARE_MULTICAST_DELEGATE(FOnAssetStoreConnectedNative);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetStoreDisconnectedNative, const FString& /*Reason*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetStoreErrorNative, const FString& /*ErrorMessage*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetListReceivedNative, const TArray<FRshipAssetInfo>& /*Assets*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAssetDownloadCompleteNative, const FString& /*ObjectKey*/, const FString& /*LocalPath*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAssetDownloadFailedNative, const FString& /*ObjectKey*/, const FString& /*ErrorMessage*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetDownloadProgressNative, const FRshipDownloadProgress& /*Progress*/);

/**
 * Client for connecting to the Rocketship Asset Store
 *
 * Provides WebSocket-based real-time asset listing and HTTP-based downloads
 * for GDTF, MVR, IES, and other asset types.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipAssetStoreClient : public UObject
{
	GENERATED_BODY()

public:
	URshipAssetStoreClient();
	virtual ~URshipAssetStoreClient();

	// ========================================================================
	// Connection
	// ========================================================================

	/**
	 * Connect to the asset store server
	 * @param ServerUrl Base URL of the asset store (e.g., "http://localhost:3100")
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void Connect(const FString& ServerUrl);

	/**
	 * Disconnect from the asset store
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void Disconnect();

	/**
	 * Check if connected to the asset store
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	bool IsConnected() const;

	/**
	 * Get the current server URL
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	FString GetServerUrl() const { return ServerUrl; }

	// ========================================================================
	// Asset Listing
	// ========================================================================

	/**
	 * Request the full list of assets from the server
	 * Results delivered via OnAssetListReceived delegate
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void RequestAssetList();

	/**
	 * Get the cached asset list (from last request)
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	TArray<FRshipAssetInfo> GetCachedAssetList() const { return CachedAssets; }

	/**
	 * Get assets filtered by type
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	TArray<FRshipAssetInfo> GetAssetsByType(ERshipAssetType Type) const;

	// ========================================================================
	// Downloads
	// ========================================================================

	/**
	 * Download an asset to the local cache
	 * @param ObjectKey The asset's object key (path) on the server
	 * @param bForceRedownload If true, download even if already cached
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void DownloadAsset(const FString& ObjectKey, bool bForceRedownload = false);

	/**
	 * Download multiple assets
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void DownloadAssets(const TArray<FString>& ObjectKeys, bool bForceRedownload = false);

	/**
	 * Cancel a pending download
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void CancelDownload(const FString& ObjectKey);

	/**
	 * Cancel all pending downloads
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void CancelAllDownloads();

	/**
	 * Get the number of active downloads
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	int32 GetActiveDownloadCount() const { return ActiveDownloads.Num(); }

	// ========================================================================
	// Cache Management
	// ========================================================================

	/**
	 * Check if an asset is cached locally
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	bool IsAssetCached(const FString& ObjectKey) const;

	/**
	 * Get the local path for a cached asset
	 * @return Empty string if not cached
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	FString GetCachedAssetPath(const FString& ObjectKey) const;

	/**
	 * Get the cache directory path
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	FString GetCacheDirectory() const;

	/**
	 * Get the total size of the cache in bytes
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|AssetStore")
	int64 GetCacheSize() const;

	/**
	 * Clear the entire cache
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void ClearCache();

	/**
	 * Remove a single cached asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void RemoveCachedAsset(const FString& ObjectKey);

	// ========================================================================
	// Sync Operations
	// ========================================================================

	/**
	 * Sync all GDTF files from the asset store
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void SyncGDTFLibrary();

	/**
	 * Sync all MVR files from the asset store
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void SyncMVRFiles();

	/**
	 * Sync all IES profiles from the asset store
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void SyncIESProfiles();

	/**
	 * Sync all assets of a specific type
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|AssetStore")
	void SyncAssetsByType(ERshipAssetType Type);

	// ========================================================================
	// Events
	// ========================================================================

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetStoreConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetStoreDisconnected OnDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetStoreError OnError;

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetListReceived OnAssetListReceived;

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetAdded OnAssetAdded;

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetRemoved OnAssetRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetDownloadComplete OnDownloadComplete;

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetDownloadFailed OnDownloadFailed;

	UPROPERTY(BlueprintAssignable, Category = "Rship|AssetStore")
	FOnAssetDownloadProgress OnDownloadProgress;

	// Native delegates for C++ binding (Slate widgets, etc.)
	FOnAssetStoreConnectedNative OnConnectedNative;
	FOnAssetStoreDisconnectedNative OnDisconnectedNative;
	FOnAssetStoreErrorNative OnErrorNative;
	FOnAssetListReceivedNative OnAssetListReceivedNative;
	FOnAssetDownloadCompleteNative OnDownloadCompleteNative;
	FOnAssetDownloadFailedNative OnDownloadFailedNative;
	FOnAssetDownloadProgressNative OnDownloadProgressNative;

private:
	// WebSocket handlers
	void OnWebSocketConnected();
	void OnWebSocketConnectionError(const FString& Error);
	void OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnWebSocketMessage(const FString& Message);

	// HTTP download handlers
	void OnDownloadRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString ObjectKey);
	void HandleDownloadProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived, FString ObjectKey);

	// Message processing
	void ProcessObjectList(TSharedPtr<FJsonObject> Data);
	void ProcessObjectAdded(TSharedPtr<FJsonObject> Data);
	void ProcessObjectRemoved(TSharedPtr<FJsonObject> Data);

	// Helpers
	ERshipAssetType GetAssetTypeFromFileName(const FString& FileName) const;
	FString ObjectKeyToLocalPath(const FString& ObjectKey) const;
	void EnsureCacheDirectoryExists() const;

	// WebSocket connection
	TSharedPtr<IWebSocket> WebSocket;
	FString ServerUrl;
	bool bIsConnected;

	// Reconnection
	FTimerHandle ReconnectTimerHandle;
	int32 ReconnectAttempts;
	static constexpr int32 MaxReconnectAttempts = 5;
	void AttemptReconnect();

	// Asset cache
	TArray<FRshipAssetInfo> CachedAssets;
	TMap<FString, FRshipAssetInfo> AssetMap; // ObjectKey -> Info

	// Active downloads
	TMap<FString, TSharedPtr<IHttpRequest>> ActiveDownloads;

	// Cache metadata
	mutable FCriticalSection CacheLock;
};
