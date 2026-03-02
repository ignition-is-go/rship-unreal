// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

/**
 * LiveLink subject item for the list view
 */
struct FRshipLiveLinkSubjectItem
{
	FName SubjectName;
	FString Role;           // Transform, Camera, Light, etc.
	bool bIsFromRship;      // True if this subject comes from rship
	bool bIsPublishedToRship; // True if we're publishing this to rship
	FString RshipEmitterId; // Mapped emitter ID (if publishing)
	FString Status;         // Active, Inactive, Stale

	FRshipLiveLinkSubjectItem()
		: bIsFromRship(false)
		, bIsPublishedToRship(false)
	{}
};

/**
 * LiveLink panel for managing LiveLink subjects and rship integration
 *
 * Features:
 * - View all LiveLink subjects (from rship and other sources)
 * - Configure bidirectional mode (Consume/Publish/Both)
 * - Map LiveLink subjects to rship emitters
 * - Monitor subject status and frame rates
 */
class SRshipLiveLinkPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipLiveLinkPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// UI Section builders
	TSharedRef<SWidget> BuildModeSection();
	TSharedRef<SWidget> BuildSourcesSection();
	TSharedRef<SWidget> BuildSubjectsSection();
	TSharedRef<SWidget> BuildMappingSection();
	TSharedRef<SWidget> BuildStatusSection();

	// List view callbacks
	TSharedRef<ITableRow> OnGenerateSubjectRow(TSharedPtr<FRshipLiveLinkSubjectItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSubjectSelectionChanged(TSharedPtr<FRshipLiveLinkSubjectItem> Item, ESelectInfo::Type SelectInfo);

	// Button callbacks
	FReply OnRefreshClicked();
	FReply OnPublishSelectedClicked();
	FReply OnStopPublishingClicked();
	FReply OnMapToEmitterClicked();

	// Mode change callback
	void OnModeChanged(int32 NewMode);

	// Refresh data from subsystem
	void RefreshSubjectList();
	void RefreshStatus();

	// Cached UI elements for updates
	TSharedPtr<STextBlock> ModeDescriptionText;
	TSharedPtr<STextBlock> ConnectionStatusText;
	TSharedPtr<STextBlock> SubjectCountText;
	TSharedPtr<STextBlock> FrameRateText;
	TSharedPtr<STextBlock> SelectedSubjectText;
	TSharedPtr<SEditableTextBox> EmitterIdInput;

	// Subject list
	TArray<TSharedPtr<FRshipLiveLinkSubjectItem>> SubjectItems;
	TSharedPtr<SListView<TSharedPtr<FRshipLiveLinkSubjectItem>>> SubjectListView;
	TSharedPtr<FRshipLiveLinkSubjectItem> SelectedSubject;

	// Current mode: 0=Consume, 1=Publish, 2=Bidirectional
	int32 CurrentMode;

	// Refresh timing
	float TimeSinceLastRefresh;
	static constexpr float RefreshInterval = 0.5f; // 2Hz refresh for status
};

/**
 * Row widget for LiveLink subject list
 */
class SRshipLiveLinkSubjectRow : public SMultiColumnTableRow<TSharedPtr<FRshipLiveLinkSubjectItem>>
{
public:
	SLATE_BEGIN_ARGS(SRshipLiveLinkSubjectRow) {}
		SLATE_ARGUMENT(TSharedPtr<FRshipLiveLinkSubjectItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FRshipLiveLinkSubjectItem> Item;
};
