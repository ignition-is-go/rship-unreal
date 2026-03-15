#include "RshipAssetStoreClient.h"

#include "Logging/LogMacros.h"

void URshipAssetStoreClient::Connect(const FString& InServerUrl)
{
	ServerUrl = InServerUrl;
}

void URshipAssetStoreClient::Disconnect()
{
	ServerUrl.Reset();
}

void URshipAssetStoreClient::DownloadAsset(const FString& ObjectKey, bool bForceRedownload)
{
	(void)bForceRedownload;

	if (ObjectKey.IsEmpty())
	{
		OnDownloadFailedNative.Broadcast(ObjectKey, TEXT("Asset id is empty"));
		return;
	}

	if (ServerUrl.IsEmpty())
	{
		OnDownloadFailedNative.Broadcast(ObjectKey, TEXT("Asset store URL is not configured"));
		return;
	}

	OnDownloadFailedNative.Broadcast(ObjectKey, TEXT("Asset store client is not available in this branch"));
}
