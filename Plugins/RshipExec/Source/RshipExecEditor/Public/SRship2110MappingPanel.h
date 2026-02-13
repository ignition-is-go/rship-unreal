// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class URship2110Subsystem;
class URshipSubsystem;
class URshipContentMappingManager;
class URshipSettings;
class SEditableTextBox;

struct FRship2110MappingStreamItem
{
	FString StreamId;
	FString StateText;
	FLinearColor StateColor;
	bool bIsRunning = false;
	bool bStreamMissing = false;
	FString Resolution;
	FString FrameRate;
	FString ColorFormat;
	FString BitDepth;
	FString CaptureSource;
	FString Destination;
	FString BoundContextId;
	FString BoundContextName;
	bool bHasCaptureRect = false;
	FIntRect BoundCaptureRect;
	FString BoundCaptureText;
	int64 FramesSent = 0;
	int64 FramesDropped = 0;
	int64 LateFrames = 0;
	double BitrateMbps = 0.0;
};

struct FRship2110RenderContextItem
{
	FString ContextId;
	FString Name;
	FString SourceType;
	FString Resolution;
	FString CameraId;
	bool bEnabled = false;
	bool bHasRenderTarget = false;
	int32 BoundStreamCount = 0;
	bool bBound = false;
	int32 Width = 0;
	int32 Height = 0;
	FString LastError;
};

class SRship2110MappingPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRship2110MappingPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// UI construction
	TSharedRef<SWidget> BuildOverviewSection();
	TSharedRef<SWidget> BuildStreamListSection();
	TSharedRef<SWidget> BuildContextListSection();
	TSharedRef<SWidget> BuildBindingSection();
	TSharedRef<SWidget> BuildSelectionDetailsSection();
	TSharedRef<SWidget> BuildUserGuideSection();

	// Data refresh
	void RefreshPanel();
	void RefreshSubsystemState();
	void RefreshStreams();
	void RefreshContexts();
	void ReconcileSelection();
	void UpdateSummaries();
	void UpdateSelectionDetails();
	void UpdateBindingInputsFromSelection();
	bool ParseCropField(const TSharedPtr<SEditableTextBox>& TextBox, int32& OutValue) const;
	bool GetBindCaptureRect(FIntRect& OutRect) const;

	// Action callbacks
	void OnRefreshClicked();
	FReply OnBindClicked();
	FReply OnUnbindClicked();
	FReply OnStartStreamClicked();
	FReply OnStopStreamClicked();
	FReply OnResetStatsClicked();

	// Rows
	TSharedRef<ITableRow> OnGenerateStreamRow(TSharedPtr<FRship2110MappingStreamItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnStreamSelectionChanged(TSharedPtr<FRship2110MappingStreamItem> Item, ESelectInfo::Type SelectInfo);

	TSharedRef<ITableRow> OnGenerateContextRow(TSharedPtr<FRship2110RenderContextItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnContextSelectionChanged(TSharedPtr<FRship2110RenderContextItem> Item, ESelectInfo::Type SelectInfo);

	// Subsystem helpers
	URship2110Subsystem* Get2110Subsystem() const;
	URshipSubsystem* GetRshipSubsystem() const;
	URshipContentMappingManager* GetContentMappingManager() const;
	bool IsContentMappingAvailable() const;
	bool Is2110RuntimeAvailable() const;
	FText FormatStreamNotReadyText() const;
	FText FormatContextUnavailableText() const;
	FText StreamStateText(const FString& State) const;

	// Action state helpers
	bool CanBind() const;
	bool CanUnbind() const;
	bool CanStart() const;
	bool CanStop() const;

	// Output state caching
	TMap<FString, int32> BoundContextCounts;

	// Overview section refs
	TSharedPtr<STextBlock> ModuleStatusText;
	TSharedPtr<STextBlock> ContentMappingStatusText;
	TSharedPtr<STextBlock> StreamSummaryText;
	TSharedPtr<STextBlock> ContextSummaryText;
	TSharedPtr<STextBlock> BindingSummaryText;

	// Stream list + state
	TArray<TSharedPtr<FRship2110MappingStreamItem>> StreamItems;
	TSharedPtr<SListView<TSharedPtr<FRship2110MappingStreamItem>>> StreamListView;
	TSharedPtr<FRship2110MappingStreamItem> SelectedStream;
	TSharedPtr<STextBlock> SelectedStreamText;
	TSharedPtr<STextBlock> SelectedStreamFormatText;
	TSharedPtr<STextBlock> SelectedStreamStatsText;
	TSharedPtr<STextBlock> SelectedStreamBindingText;

	// Context list + state
	TArray<TSharedPtr<FRship2110RenderContextItem>> ContextItems;
	TSharedPtr<SListView<TSharedPtr<FRship2110RenderContextItem>>> ContextListView;
	TSharedPtr<FRship2110RenderContextItem> SelectedContext;
	TSharedPtr<STextBlock> SelectedContextText;
	TSharedPtr<STextBlock> SelectedContextDetailsText;
	TSharedPtr<SEditableTextBox> CaptureXText;
	TSharedPtr<SEditableTextBox> CaptureYText;
	TSharedPtr<SEditableTextBox> CaptureWText;
	TSharedPtr<SEditableTextBox> CaptureHText;

	// Binding status/feedback
	TSharedPtr<STextBlock> BindingStatusText;

	float TimeSinceLastRefresh = 0.0f;
	static constexpr float RefreshInterval = 0.25f;
};
