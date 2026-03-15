// Minimal asset-store client for the mapping plugin.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RshipAssetStoreClient.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAssetDownloadCompleteNative, const FString& /*ObjectKey*/, const FString& /*LocalPath*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAssetDownloadFailedNative, const FString& /*ObjectKey*/, const FString& /*ErrorMessage*/);

UCLASS()
class RSHIPMAPPING_API URshipAssetStoreClient : public UObject
{
	GENERATED_BODY()

public:
	void Connect(const FString& InServerUrl);
	void Disconnect();
	void DownloadAsset(const FString& ObjectKey, bool bForceRedownload = false);

	FOnAssetDownloadCompleteNative OnDownloadCompleteNative;
	FOnAssetDownloadFailedNative OnDownloadFailedNative;

private:
	FString ServerUrl;
};
