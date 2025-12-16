// Copyright Rocketship. All Rights Reserved.

#include "SRshipAssetSyncPanel.h"
#include "RshipSubsystem.h"
#include "RshipAssetStoreClient.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#endif

#define LOCTEXT_NAMESPACE "SRshipAssetSyncPanel"

void SRshipAssetSyncPanel::Construct(const FArguments& InArgs)
{
	CurrentFilter = 0;
	bIsConnected = false;
	ActiveDownloads = 0;
	TotalDownloads = 0;
	TimeSinceLastRefresh = 0.0f;
	CurrentServerUrl = TEXT("http://localhost:3100");

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Connection Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildConnectionSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Filter Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildFilterSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Asset List
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0, 0, 0, 8)
			[
				BuildAssetListSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Actions Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildActionsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Cache Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildCacheSection()
			]
		]
	];

	// Initial status
	RefreshStatus();
}

void SRshipAssetSyncPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		TimeSinceLastRefresh = 0.0f;
		RefreshStatus();
	}
}

TSharedRef<SWidget> SRshipAssetSyncPanel::BuildConnectionSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConnectionLabel", "Asset Store Connection"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ServerLabel", "Server:"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(ServerUrlInput, SEditableTextBox)
				.Text(FText::FromString(CurrentServerUrl))
				.HintText(LOCTEXT("ServerUrlHint", "http://localhost:3100"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text_Lambda([this]()
				{
					return bIsConnected ? LOCTEXT("DisconnectBtn", "Disconnect") : LOCTEXT("ConnectBtn", "Connect");
				})
				.OnClicked(this, &SRshipAssetSyncPanel::OnConnectClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Check"))
					.ColorAndOpacity_Lambda([this]()
					{
						return bIsConnected ? FLinearColor::Green : FLinearColor::Red;
					})
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(ConnectionStatusText, STextBlock)
					.Text(LOCTEXT("StatusDisconnected", "Not connected to asset store"))
				]
			]
		];
}

TSharedRef<SWidget> SRshipAssetSyncPanel::BuildFilterSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FilterLabel", "Asset Type Filter"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSegmentedControl<int32>)
			.OnValueChanged(this, &SRshipAssetSyncPanel::OnFilterChanged)
			+ SSegmentedControl<int32>::Slot(0)
			.Text(LOCTEXT("FilterAll", "All"))
			+ SSegmentedControl<int32>::Slot(1)
			.Text(LOCTEXT("FilterGDTF", "GDTF"))
			.ToolTip(LOCTEXT("GDTFTooltip", "Fixture device profiles"))
			+ SSegmentedControl<int32>::Slot(2)
			.Text(LOCTEXT("FilterMVR", "MVR"))
			.ToolTip(LOCTEXT("MVRTooltip", "Virtual rig scene files"))
			+ SSegmentedControl<int32>::Slot(3)
			.Text(LOCTEXT("FilterIES", "IES"))
			.ToolTip(LOCTEXT("IESTooltip", "Light photometric profiles"))
		];
}

TSharedRef<SWidget> SRshipAssetSyncPanel::BuildAssetListSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetsLabel", "Available Assets"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(AssetCountText, STextBlock)
				.Text(LOCTEXT("AssetCount", "0 assets"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshBtn", "Refresh"))
				.OnClicked(this, &SRshipAssetSyncPanel::OnRefreshClicked)
				.IsEnabled_Lambda([this]() { return bIsConnected; })
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0, 4, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(AssetListView, SListView<TSharedPtr<FRshipAssetItem>>)
				.ListItemsSource(&FilteredAssetItems)
				.OnGenerateRow(this, &SRshipAssetSyncPanel::OnGenerateAssetRow)
				.OnSelectionChanged(this, &SRshipAssetSyncPanel::OnAssetSelectionChanged)
				.SelectionMode(ESelectionMode::Multi)
				.HeaderRow
				(
					SNew(SHeaderRow)

					+ SHeaderRow::Column("Name")
					.DefaultLabel(LOCTEXT("ColName", "File Name"))
					.FillWidth(0.35f)

					+ SHeaderRow::Column("Type")
					.DefaultLabel(LOCTEXT("ColType", "Type"))
					.FillWidth(0.1f)

					+ SHeaderRow::Column("Size")
					.DefaultLabel(LOCTEXT("ColSize", "Size"))
					.FillWidth(0.12f)

					+ SHeaderRow::Column("Modified")
					.DefaultLabel(LOCTEXT("ColModified", "Modified"))
					.FillWidth(0.18f)

					+ SHeaderRow::Column("Status")
					.DefaultLabel(LOCTEXT("ColStatus", "Status"))
					.FillWidth(0.25f)
				)
			]
		];
}

TSharedRef<SWidget> SRshipAssetSyncPanel::BuildActionsSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ActionsLabel", "Sync Actions"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectedLabel", "Selected:"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SelectedAssetText, STextBlock)
				.Text(LOCTEXT("NoneSelected", "(none)"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("DownloadSelectedBtn", "Download Selected"))
				.OnClicked(this, &SRshipAssetSyncPanel::OnDownloadSelectedClicked)
				.IsEnabled_Lambda([this]() { return bIsConnected && SelectedAsset.IsValid(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("SyncAllBtn", "Sync All"))
				.ToolTipText(LOCTEXT("SyncAllTooltip", "Download all assets that need updates"))
				.OnClicked(this, &SRshipAssetSyncPanel::OnSyncAllClicked)
				.IsEnabled_Lambda([this]() { return bIsConnected && AllAssetItems.Num() > 0; })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("CancelBtn", "Cancel Downloads"))
				.OnClicked(this, &SRshipAssetSyncPanel::OnCancelDownloadsClicked)
				.IsEnabled_Lambda([this]() { return ActiveDownloads > 0; })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ProgressLabel", "Overall Progress:"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(OverallProgressBar, SProgressBar)
				.Percent(0.0f)
			]
		];
}

TSharedRef<SWidget> SRshipAssetSyncPanel::BuildCacheSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CacheLabel", "Local Cache"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CacheSizeLabel", "Cache Size:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(CacheSizeText, STextBlock)
						.Text(LOCTEXT("CacheSizeUnknown", "Unknown"))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CachePathLabel", "Location:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(FText::FromString(GetCachePath()))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenCacheBtn", "Open Cache Folder"))
				.OnClicked(this, &SRshipAssetSyncPanel::OnOpenCacheFolderClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearCacheBtn", "Clear Cache"))
				.OnClicked(this, &SRshipAssetSyncPanel::OnClearCacheClicked)
			]
		];
}

TSharedRef<ITableRow> SRshipAssetSyncPanel::OnGenerateAssetRow(TSharedPtr<FRshipAssetItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRshipAssetRow, OwnerTable)
		.Item(Item);
}

void SRshipAssetSyncPanel::OnAssetSelectionChanged(TSharedPtr<FRshipAssetItem> Item, ESelectInfo::Type SelectInfo)
{
	SelectedAsset = Item;

	if (AssetListView.IsValid())
	{
		TArray<TSharedPtr<FRshipAssetItem>> SelectedItems = AssetListView->GetSelectedItems();
		if (SelectedItems.Num() > 1)
		{
			SelectedAssetText->SetText(FText::Format(LOCTEXT("MultipleSelected", "{0} items selected"), SelectedItems.Num()));
		}
		else if (Item.IsValid())
		{
			SelectedAssetText->SetText(FText::FromString(Item->FileName));
		}
		else
		{
			SelectedAssetText->SetText(LOCTEXT("NoneSelected", "(none)"));
		}
	}
}

void SRshipAssetSyncPanel::OnFilterChanged(int32 NewFilter)
{
	CurrentFilter = NewFilter;

	FilteredAssetItems.Empty();
	for (const auto& Item : AllAssetItems)
	{
		bool bInclude = false;
		switch (CurrentFilter)
		{
		case 0: bInclude = true; break; // All
		case 1: bInclude = (Item->AssetType == ERshipAssetType::GDTF); break;
		case 2: bInclude = (Item->AssetType == ERshipAssetType::MVR); break;
		case 3: bInclude = (Item->AssetType == ERshipAssetType::IES); break;
		}

		if (bInclude)
		{
			FilteredAssetItems.Add(Item);
		}
	}

	// Update count
	if (AssetCountText.IsValid())
	{
		AssetCountText->SetText(FText::Format(
			LOCTEXT("AssetCountFmt", "{0} {0}|plural(one=asset,other=assets)"),
			FilteredAssetItems.Num()));
	}

	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}
}

FReply SRshipAssetSyncPanel::OnConnectClicked()
{
	if (bIsConnected)
	{
		// Disconnect
		if (AssetStoreClient.IsValid())
		{
			AssetStoreClient->Disconnect();
		}

		bIsConnected = false;
		AllAssetItems.Empty();
		FilteredAssetItems.Empty();

		if (ConnectionStatusText.IsValid())
		{
			ConnectionStatusText->SetText(LOCTEXT("StatusDisconnected", "Not connected to asset store"));
		}

		if (AssetListView.IsValid())
		{
			AssetListView->RequestListRefresh();
		}
	}
	else
	{
		// Connect
		CurrentServerUrl = ServerUrlInput->GetText().ToString();

		// Create or get asset store client
		if (!AssetStoreClient.IsValid())
		{
			AssetStoreClient = NewObject<URshipAssetStoreClient>();
			AssetStoreClient->AddToRoot(); // Prevent GC

			// Bind event handlers
			AssetStoreClient->OnConnected.AddDynamic(this, &SRshipAssetSyncPanel::OnAssetStoreConnected);
			AssetStoreClient->OnDisconnected.AddDynamic(this, &SRshipAssetSyncPanel::OnAssetStoreDisconnected);
			AssetStoreClient->OnError.AddDynamic(this, &SRshipAssetSyncPanel::OnAssetStoreError);
			AssetStoreClient->OnAssetListReceived.AddDynamic(this, &SRshipAssetSyncPanel::OnAssetListReceived);
			AssetStoreClient->OnDownloadComplete.AddDynamic(this, &SRshipAssetSyncPanel::OnAssetDownloadComplete);
			AssetStoreClient->OnDownloadFailed.AddDynamic(this, &SRshipAssetSyncPanel::OnAssetDownloadFailed);
			AssetStoreClient->OnDownloadProgress.AddDynamic(this, &SRshipAssetSyncPanel::OnAssetDownloadProgressUpdate);
		}

		if (ConnectionStatusText.IsValid())
		{
			ConnectionStatusText->SetText(FText::Format(
				LOCTEXT("StatusConnecting", "Connecting to {0}..."),
				FText::FromString(CurrentServerUrl)));
		}

		AssetStoreClient->Connect(CurrentServerUrl);
	}

	return FReply::Handled();
}

FReply SRshipAssetSyncPanel::OnRefreshClicked()
{
	RefreshAssetList();
	return FReply::Handled();
}

FReply SRshipAssetSyncPanel::OnDownloadSelectedClicked()
{
	if (AssetListView.IsValid())
	{
		TArray<TSharedPtr<FRshipAssetItem>> SelectedItems = AssetListView->GetSelectedItems();
		for (auto& Item : SelectedItems)
		{
			if (!Item->bIsCached || Item->bNeedsSync)
			{
				StartDownload(Item);
			}
		}
	}
	return FReply::Handled();
}

FReply SRshipAssetSyncPanel::OnSyncAllClicked()
{
	for (auto& Item : AllAssetItems)
	{
		if (!Item->bIsCached || Item->bNeedsSync)
		{
			StartDownload(Item);
		}
	}
	return FReply::Handled();
}

FReply SRshipAssetSyncPanel::OnCancelDownloadsClicked()
{
	for (auto& Item : AllAssetItems)
	{
		if (Item->bIsDownloading)
		{
			CancelDownload(Item);
		}
	}
	return FReply::Handled();
}

FReply SRshipAssetSyncPanel::OnClearCacheClicked()
{
	FString CachePath = GetCachePath();
	IFileManager::Get().DeleteDirectory(*CachePath, false, true);

	// Reset cached status
	for (auto& Item : AllAssetItems)
	{
		Item->bIsCached = false;
		Item->bNeedsSync = true;
	}

	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}

	RefreshStatus();
	return FReply::Handled();
}

FReply SRshipAssetSyncPanel::OnOpenCacheFolderClicked()
{
	FString CachePath = GetCachePath();
	FPlatformProcess::ExploreFolder(*CachePath);
	return FReply::Handled();
}

void SRshipAssetSyncPanel::RefreshAssetList()
{
	if (bIsConnected && AssetStoreClient.IsValid())
	{
		// Request asset list from server
		AssetStoreClient->RequestAssetList();
	}
	else
	{
		// Clear and refresh UI
		AllAssetItems.Empty();
		OnFilterChanged(CurrentFilter);
	}
}

void SRshipAssetSyncPanel::RefreshStatus()
{
	// Calculate cache size
	FString CachePath = GetCachePath();
	int64 TotalSize = 0;

	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> Files;
	FileManager.FindFilesRecursive(Files, *CachePath, TEXT("*"), true, false);

	for (const FString& File : Files)
	{
		TotalSize += FileManager.FileSize(*File);
	}

	if (CacheSizeText.IsValid())
	{
		if (TotalSize < 1024)
		{
			CacheSizeText->SetText(FText::Format(LOCTEXT("CacheSizeBytes", "{0} B"), TotalSize));
		}
		else if (TotalSize < 1024 * 1024)
		{
			CacheSizeText->SetText(FText::Format(LOCTEXT("CacheSizeKB", "{0} KB"), TotalSize / 1024));
		}
		else
		{
			CacheSizeText->SetText(FText::Format(LOCTEXT("CacheSizeMB", "{0} MB"), TotalSize / (1024 * 1024)));
		}
	}

	// Update progress bar
	if (OverallProgressBar.IsValid())
	{
		float Progress = (TotalDownloads > 0) ? (float)(TotalDownloads - ActiveDownloads) / TotalDownloads : 0.0f;
		OverallProgressBar->SetPercent(Progress);
	}
}

void SRshipAssetSyncPanel::StartDownload(TSharedPtr<FRshipAssetItem> Item)
{
	if (!Item.IsValid() || Item->bIsDownloading) return;
	if (!AssetStoreClient.IsValid()) return;

	Item->bIsDownloading = true;
	Item->DownloadProgress = 0.0f;
	ActiveDownloads++;
	TotalDownloads++;

	// Download via asset store client
	AssetStoreClient->DownloadAsset(Item->AssetId);

	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}
}

void SRshipAssetSyncPanel::CancelDownload(TSharedPtr<FRshipAssetItem> Item)
{
	if (!Item.IsValid() || !Item->bIsDownloading) return;

	if (AssetStoreClient.IsValid())
	{
		AssetStoreClient->CancelDownload(Item->AssetId);
	}

	Item->bIsDownloading = false;
	Item->DownloadProgress = 0.0f;
	ActiveDownloads--;

	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}
}

ERshipAssetType SRshipAssetSyncPanel::GetAssetTypeFromFileName(const FString& FileName) const
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

FString SRshipAssetSyncPanel::GetCachePath() const
{
	return FPaths::ProjectSavedDir() / TEXT("RshipAssets");
}

// ============================================================================
// SRshipAssetRow
// ============================================================================

void SRshipAssetRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<TSharedPtr<FRshipAssetItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipAssetRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == "Name")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->FileName))
			];
	}
	else if (ColumnName == "Type")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->GetAssetTypeString()))
			];
	}
	else if (ColumnName == "Size")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->GetFileSizeString()))
			];
	}
	else if (ColumnName == "Modified")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::AsDate(Item->LastModified))
			];
	}
	else if (ColumnName == "Status")
	{
		if (Item->bIsDownloading)
		{
			return SNew(SBox)
				.Padding(FMargin(4, 2))
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SProgressBar)
						.Percent(Item->DownloadProgress)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, 0, 0, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("DownloadPercent", "{0}%"),
							FMath::RoundToInt(Item->DownloadProgress * 100)))
					]
				];
		}
		else
		{
			FText StatusText;
			FLinearColor StatusColor;

			if (Item->bIsCached && !Item->bNeedsSync)
			{
				StatusText = LOCTEXT("StatusCached", "Cached");
				StatusColor = FLinearColor::Green;
			}
			else if (Item->bNeedsSync)
			{
				StatusText = LOCTEXT("StatusNeedsSync", "Update Available");
				StatusColor = FLinearColor::Yellow;
			}
			else
			{
				StatusText = LOCTEXT("StatusNotCached", "Not Downloaded");
				StatusColor = FLinearColor::Gray;
			}

			return SNew(SBox)
				.Padding(FMargin(4, 2))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(StatusText)
					.ColorAndOpacity(StatusColor)
				];
		}
	}

	return SNullWidget::NullWidget;
}

// ============================================================================
// Asset Store Client Event Handlers
// ============================================================================

void SRshipAssetSyncPanel::OnAssetStoreConnected()
{
	bIsConnected = true;

	if (ConnectionStatusText.IsValid())
	{
		ConnectionStatusText->SetText(FText::Format(
			LOCTEXT("StatusConnected", "Connected to {0}"),
			FText::FromString(CurrentServerUrl)));
	}

	// Asset list will be received via OnAssetListReceived
}

void SRshipAssetSyncPanel::OnAssetStoreDisconnected(const FString& Reason)
{
	bIsConnected = false;

	if (ConnectionStatusText.IsValid())
	{
		ConnectionStatusText->SetText(FText::Format(
			LOCTEXT("StatusDisconnectedReason", "Disconnected: {0}"),
			FText::FromString(Reason)));
	}
}

void SRshipAssetSyncPanel::OnAssetStoreError(const FString& ErrorMessage)
{
	if (ConnectionStatusText.IsValid())
	{
		ConnectionStatusText->SetText(FText::Format(
			LOCTEXT("StatusError", "Error: {0}"),
			FText::FromString(ErrorMessage)));
	}
}

void SRshipAssetSyncPanel::OnAssetListReceived(const TArray<FRshipAssetInfo>& Assets)
{
	AllAssetItems.Empty();

	for (const FRshipAssetInfo& Info : Assets)
	{
		TSharedPtr<FRshipAssetItem> Item = MakeShared<FRshipAssetItem>();
		Item->AssetId = Info.ObjectKey;
		Item->FileName = Info.FileName;
		Item->FileSize = Info.FileSize;
		Item->LastModified = Info.LastModified;

		// Convert asset type
		switch (Info.AssetType)
		{
		case ERshipAssetType::GDTF:
			Item->AssetType = ERshipAssetType::GDTF;
			break;
		case ERshipAssetType::MVR:
			Item->AssetType = ERshipAssetType::MVR;
			break;
		case ERshipAssetType::IES:
			Item->AssetType = ERshipAssetType::IES;
			break;
		default:
			Item->AssetType = ERshipAssetType::Other;
			break;
		}

		// Check cache status
		if (AssetStoreClient.IsValid())
		{
			Item->bIsCached = AssetStoreClient->IsAssetCached(Info.ObjectKey);
		}

		AllAssetItems.Add(Item);
	}

	// Apply filter
	OnFilterChanged(CurrentFilter);
}

void SRshipAssetSyncPanel::OnAssetDownloadComplete(const FString& ObjectKey, const FString& LocalPath)
{
	// Find the item and update its status
	for (TSharedPtr<FRshipAssetItem>& Item : AllAssetItems)
	{
		if (Item->AssetId == ObjectKey)
		{
			Item->bIsDownloading = false;
			Item->bIsCached = true;
			Item->bNeedsSync = false;
			Item->DownloadProgress = 1.0f;
			break;
		}
	}

	ActiveDownloads = FMath::Max(0, ActiveDownloads - 1);

	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}

	RefreshStatus();
}

void SRshipAssetSyncPanel::OnAssetDownloadFailed(const FString& ObjectKey, const FString& ErrorMessage)
{
	// Find the item and update its status
	for (TSharedPtr<FRshipAssetItem>& Item : AllAssetItems)
	{
		if (Item->AssetId == ObjectKey)
		{
			Item->bIsDownloading = false;
			Item->DownloadProgress = 0.0f;
			break;
		}
	}

	ActiveDownloads = FMath::Max(0, ActiveDownloads - 1);

	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}

	RefreshStatus();
}

void SRshipAssetSyncPanel::OnAssetDownloadProgressUpdate(const FRshipDownloadProgress& Progress)
{
	// Find the item and update its progress
	for (TSharedPtr<FRshipAssetItem>& Item : AllAssetItems)
	{
		if (Item->AssetId == Progress.ObjectKey)
		{
			Item->DownloadProgress = Progress.Progress;
			break;
		}
	}

	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
