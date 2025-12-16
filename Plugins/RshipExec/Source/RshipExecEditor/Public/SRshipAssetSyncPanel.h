// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "RshipAssetStoreClient.h"  // For ERshipAssetType and URshipAssetStoreClient

// Note: ERshipAssetType is defined in RshipAssetStoreClient.h as a UENUM

/**
 * Asset item for the sync list
 */
struct FRshipAssetItem
{
	FString AssetId;
	FString FileName;
	ERshipAssetType AssetType;
	int64 FileSize;
	FDateTime LastModified;
	bool bIsCached;        // True if local copy exists
	bool bNeedsSync;       // True if server version is newer
	bool bIsDownloading;   // True if currently downloading
	float DownloadProgress; // 0.0 - 1.0

	FRshipAssetItem()
		: AssetType(ERshipAssetType::Other)
		, FileSize(0)
		, bIsCached(false)
		, bNeedsSync(false)
		, bIsDownloading(false)
		, DownloadProgress(0.0f)
	{}

	FString GetAssetTypeString() const
	{
		switch (AssetType)
		{
		case ERshipAssetType::GDTF: return TEXT("GDTF");
		case ERshipAssetType::MVR: return TEXT("MVR");
		case ERshipAssetType::IES: return TEXT("IES");
		default: return TEXT("Other");
		}
	}

	FString GetFileSizeString() const
	{
		if (FileSize < 1024) return FString::Printf(TEXT("%lld B"), FileSize);
		if (FileSize < 1024 * 1024) return FString::Printf(TEXT("%.1f KB"), FileSize / 1024.0f);
		return FString::Printf(TEXT("%.1f MB"), FileSize / (1024.0f * 1024.0f));
	}
};

/**
 * Asset sync panel for managing GDTF/MVR/IES files from rship asset store
 *
 * Features:
 * - View available assets on rship server
 * - Download/sync assets to local cache
 * - Monitor download progress
 * - Filter by asset type
 * - Clear local cache
 */
class SRshipAssetSyncPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipAssetSyncPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// UI Section builders
	TSharedRef<SWidget> BuildConnectionSection();
	TSharedRef<SWidget> BuildFilterSection();
	TSharedRef<SWidget> BuildAssetListSection();
	TSharedRef<SWidget> BuildActionsSection();
	TSharedRef<SWidget> BuildCacheSection();

	// List view callbacks
	TSharedRef<ITableRow> OnGenerateAssetRow(TSharedPtr<FRshipAssetItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnAssetSelectionChanged(TSharedPtr<FRshipAssetItem> Item, ESelectInfo::Type SelectInfo);

	// Filter callbacks
	void OnFilterChanged(int32 NewFilter);

	// Button callbacks
	FReply OnConnectClicked();
	FReply OnRefreshClicked();
	FReply OnDownloadSelectedClicked();
	FReply OnSyncAllClicked();
	FReply OnCancelDownloadsClicked();
	FReply OnClearCacheClicked();
	FReply OnOpenCacheFolderClicked();

	// Data operations
	void RefreshAssetList();
	void RefreshStatus();
	void StartDownload(TSharedPtr<FRshipAssetItem> Item);
	void CancelDownload(TSharedPtr<FRshipAssetItem> Item);

	// Helper
	ERshipAssetType GetAssetTypeFromFileName(const FString& FileName) const;
	FString GetCachePath() const;

	// Cached UI elements
	TSharedPtr<STextBlock> ConnectionStatusText;
	TSharedPtr<STextBlock> ServerUrlText;
	TSharedPtr<STextBlock> AssetCountText;
	TSharedPtr<STextBlock> CacheSizeText;
	TSharedPtr<STextBlock> SelectedAssetText;
	TSharedPtr<SProgressBar> OverallProgressBar;
	TSharedPtr<SEditableTextBox> ServerUrlInput;

	// Asset list
	TArray<TSharedPtr<FRshipAssetItem>> AllAssetItems;
	TArray<TSharedPtr<FRshipAssetItem>> FilteredAssetItems;
	TSharedPtr<SListView<TSharedPtr<FRshipAssetItem>>> AssetListView;
	TSharedPtr<FRshipAssetItem> SelectedAsset;

	// Current filter: 0=All, 1=GDTF, 2=MVR, 3=IES
	int32 CurrentFilter;

	// Connection state
	bool bIsConnected;
	FString CurrentServerUrl;

	// Download tracking
	int32 ActiveDownloads;
	int32 TotalDownloads;

	// Refresh timing
	float TimeSinceLastRefresh;
	static constexpr float RefreshInterval = 2.0f; // 0.5Hz refresh

	// Asset store client
	TWeakObjectPtr<URshipAssetStoreClient> AssetStoreClient;

	// Event handlers for asset store client
	void OnAssetStoreConnected();
	void OnAssetStoreDisconnected(const FString& Reason);
	void OnAssetStoreError(const FString& ErrorMessage);
	void OnAssetListReceived(const TArray<struct FRshipAssetInfo>& Assets);
	void OnAssetDownloadComplete(const FString& ObjectKey, const FString& LocalPath);
	void OnAssetDownloadFailed(const FString& ObjectKey, const FString& ErrorMessage);
	void OnAssetDownloadProgressUpdate(const struct FRshipDownloadProgress& Progress);
};

/**
 * Row widget for asset list
 */
class SRshipAssetRow : public SMultiColumnTableRow<TSharedPtr<FRshipAssetItem>>
{
public:
	SLATE_BEGIN_ARGS(SRshipAssetRow) {}
		SLATE_ARGUMENT(TSharedPtr<FRshipAssetItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FRshipAssetItem> Item;
};
