// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// Forward declarations
class URshipNDIStreamComponent;

/**
 * NDI stream item for the list view
 */
struct FRshipNDIStreamItem
{
	/** The NDI stream component */
	TWeakObjectPtr<URshipNDIStreamComponent> Component;

	/** Actor label for display */
	FString ActorLabel;

	/** Stream name */
	FString StreamName;

	/** Resolution string (e.g., "1920x1080") */
	FString Resolution;

	/** Current state as string */
	FString StateString;

	/** Is currently streaming */
	bool bIsStreaming = false;

	/** Current FPS */
	float CurrentFPS = 0.0f;

	/** Connected receiver count */
	int32 ReceiverCount = 0;

	/** Bandwidth in Mbps */
	float BandwidthMbps = 0.0f;

	/** Total frames sent */
	int64 TotalFramesSent = 0;

	/** Dropped frames */
	int64 DroppedFrames = 0;
};

/**
 * NDI panel for managing NDI streams from CineCameras
 *
 * Features:
 * - View all NDI stream components in the level
 * - Start/stop individual streams
 * - Monitor streaming statistics (FPS, bandwidth, receivers)
 * - Quick configuration access
 * - Bulk start/stop all streams
 */
class SRshipNDIPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipNDIPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// UI Section builders
	TSharedRef<SWidget> BuildOverviewSection();
	TSharedRef<SWidget> BuildStreamListSection();
	TSharedRef<SWidget> BuildSelectedStreamSection();
	TSharedRef<SWidget> BuildBulkActionsSection();

	// List view callbacks
	TSharedRef<ITableRow> OnGenerateStreamRow(TSharedPtr<FRshipNDIStreamItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnStreamSelectionChanged(TSharedPtr<FRshipNDIStreamItem> Item, ESelectInfo::Type SelectInfo);

	// Button callbacks
	FReply OnRefreshStreamsClicked();
	FReply OnStartSelectedClicked();
	FReply OnStopSelectedClicked();
	FReply OnStartAllClicked();
	FReply OnStopAllClicked();
	FReply OnFocusCameraClicked();

	// Data refresh
	void RefreshStreamList();
	void UpdateStreamStats();

	// Helpers
	FLinearColor GetStateColor(bool bIsStreaming, bool bHasError) const;
	FText GetStateText(bool bIsStreaming, bool bHasError) const;

	// Cached UI elements
	TSharedPtr<STextBlock> TotalStreamsText;
	TSharedPtr<STextBlock> ActiveStreamsText;
	TSharedPtr<STextBlock> TotalReceiversText;
	TSharedPtr<STextBlock> NDIAvailableText;

	// Selected stream details
	TSharedPtr<STextBlock> SelectedStreamNameText;
	TSharedPtr<STextBlock> SelectedResolutionText;
	TSharedPtr<STextBlock> SelectedFrameRateText;
	TSharedPtr<STextBlock> SelectedBandwidthText;
	TSharedPtr<STextBlock> SelectedFramesSentText;
	TSharedPtr<STextBlock> SelectedDroppedFramesText;
	TSharedPtr<STextBlock> SelectedReceiversText;
	TSharedPtr<STextBlock> SelectedVRAMText;

	// Stream list
	TArray<TSharedPtr<FRshipNDIStreamItem>> StreamItems;
	TSharedPtr<SListView<TSharedPtr<FRshipNDIStreamItem>>> StreamListView;
	TSharedPtr<FRshipNDIStreamItem> SelectedStream;

	// Refresh timing
	float TimeSinceLastRefresh;
	static constexpr float RefreshInterval = 0.25f; // 4Hz refresh for responsive stats
};

/**
 * Row widget for NDI stream list
 */
class SRshipNDIStreamRow : public SMultiColumnTableRow<TSharedPtr<FRshipNDIStreamItem>>
{
public:
	SLATE_BEGIN_ARGS(SRshipNDIStreamRow) {}
		SLATE_ARGUMENT(TSharedPtr<FRshipNDIStreamItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FRshipNDIStreamItem> Item;
};
