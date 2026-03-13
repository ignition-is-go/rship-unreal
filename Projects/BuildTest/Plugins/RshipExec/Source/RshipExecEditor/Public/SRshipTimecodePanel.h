// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "RshipTimecodeSync.h"

class URshipSubsystem;
class URshipTimecodeSync;

/** Row data for the cue point list */
struct FRshipCuePointListItem
{
    FString Id;
    FString Name;
    FTimecode Timecode;
    FLinearColor Color;
    bool bEnabled;
    bool bFired;

    FRshipCuePointListItem()
        : bEnabled(true)
        , bFired(false)
    {}

    FRshipCuePointListItem(const FRshipCuePoint& CuePoint)
        : Id(CuePoint.Id)
        , Name(CuePoint.Name)
        , Timecode(CuePoint.Timecode)
        , Color(CuePoint.Color)
        , bEnabled(CuePoint.bEnabled)
        , bFired(CuePoint.bFired)
    {}
};

/**
 * Rocketship Timecode Panel widget.
 * Shows timecode status, playback controls, source selection, and cue points.
 */
class SRshipTimecodePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRshipTimecodePanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SRshipTimecodePanel();

    // SWidget interface
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    // UI update helpers
    void UpdateTimecodeDisplay();
    void UpdateSourceStatus();
    void RefreshCuePointList();

    // Get services
    URshipSubsystem* GetSubsystem() const;
    URshipTimecodeSync* GetTimecodeSync() const;

    // Playback control callbacks
    FReply OnPlayClicked();
    FReply OnPauseClicked();
    FReply OnStopClicked();
    FReply OnStepForwardClicked();
    FReply OnStepBackwardClicked();
    FReply OnJumpToNextCueClicked();
    FReply OnJumpToPrevCueClicked();

    // Source selection
    void OnSourceChanged(TSharedPtr<FString> NewSource, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> GenerateSourceComboItem(TSharedPtr<FString> InItem);
    FText GetCurrentSourceText() const;

    // Mode selection (bidirectional)
    void OnModeChanged(TSharedPtr<FString> NewMode, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> GenerateModeComboItem(TSharedPtr<FString> InItem);
    FText GetCurrentModeText() const;

    // Cue point list
    TSharedRef<ITableRow> GenerateCuePointRow(TSharedPtr<FRshipCuePointListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    void OnCuePointSelectionChanged(TSharedPtr<FRshipCuePointListItem> Item, ESelectInfo::Type SelectInfo);
    FReply OnAddCuePointClicked();
    FReply OnRemoveCuePointClicked();
    FReply OnClearCuePointsClicked();

    // Test pulse injection (for testing without server)
    FReply OnInjectTestTimecodeClicked();

    // Build UI sections
    TSharedRef<SWidget> BuildTimecodeDisplaySection();
    TSharedRef<SWidget> BuildPlaybackControlSection();
    TSharedRef<SWidget> BuildSourceSection();
    TSharedRef<SWidget> BuildCuePointsSection();
    TSharedRef<SWidget> BuildTestSection();

    // Cached UI elements for updates
    TSharedPtr<STextBlock> TimecodeText;
    TSharedPtr<STextBlock> FrameNumberText;
    TSharedPtr<STextBlock> ElapsedTimeText;
    TSharedPtr<STextBlock> StateText;
    TSharedPtr<STextBlock> SyncStatusText;
    TSharedPtr<STextBlock> SyncOffsetText;
    TSharedPtr<STextBlock> FrameRateText;
    TSharedPtr<SImage> SyncIndicator;
    TSharedPtr<SImage> PlaybackIndicator;

    // Source selection
    TArray<TSharedPtr<FString>> SourceOptions;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> SourceComboBox;
    ERshipTimecodeSource CurrentSource = ERshipTimecodeSource::Internal;

    // Mode selection (bidirectional)
    TArray<TSharedPtr<FString>> ModeOptions;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ModeComboBox;
    ERshipTimecodeMode CurrentMode = ERshipTimecodeMode::Receive;
    TSharedPtr<STextBlock> ModeStatusText;

    // Cue point list
    TArray<TSharedPtr<FRshipCuePointListItem>> CuePointItems;
    TSharedPtr<SListView<TSharedPtr<FRshipCuePointListItem>>> CuePointListView;
    TSharedPtr<FRshipCuePointListItem> SelectedCuePoint;

    // Refresh timer
    float RefreshTimer = 0.0f;
    static constexpr float RefreshInterval = 0.033f;  // ~30fps for smooth timecode display
};

/**
 * Row widget for cue point list items
 */
class SRshipCuePointRow : public SMultiColumnTableRow<TSharedPtr<FRshipCuePointListItem>>
{
public:
    SLATE_BEGIN_ARGS(SRshipCuePointRow) {}
        SLATE_ARGUMENT(TSharedPtr<FRshipCuePointListItem>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FRshipCuePointListItem> Item;
};
