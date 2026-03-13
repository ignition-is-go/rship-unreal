// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"

class URshipSubsystem;
class URship2110Subsystem;
class URshipTargetComponent;
class SCheckBox;

/** Row data for the target list */
struct FRshipTargetListItem
{
    FString TargetId;
    FString DisplayName;
    FString TargetType;
    bool bIsOnline;
    int32 EmitterCount;
    int32 ActionCount;
    TWeakObjectPtr<URshipTargetComponent> Component;

    FRshipTargetListItem()
        : bIsOnline(false)
        , EmitterCount(0)
        , ActionCount(0)
    {}
};

/**
 * Main Rocketship Status Panel widget.
 * Shows connection status, server address, targets list, and diagnostics.
 */
class SRshipStatusPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRshipStatusPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SRshipStatusPanel();

    // SWidget interface
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    // UI update helpers
    void RefreshTargetList();
    void UpdateConnectionStatus();
    void UpdateDiagnostics();

    // Get the subsystem
    URshipSubsystem* GetSubsystem() const;

    // Button callbacks
    FReply OnReconnectClicked();
    FReply OnSettingsClicked();
    FReply OnRefreshTargetsClicked();
    FReply OnApplyControlSyncRateClicked();
    FReply OnApplyInboundLeadFramesClicked();
    void OnRequireExactFrameChanged(ECheckBoxState NewState);
    void SetSyncTimingStatus(const FText& Message, const FLinearColor& Color);
    FReply OnApplySyncPresetClicked(float PresetHz);
    FReply OnApplyRenderSubstepsPresetClicked(int32 PresetSubsteps);
    FReply OnSaveTimingDefaultsClicked();
    FReply OnCopyIniRolloutSnippetClicked();
    FString BuildTimingIniSnippet() const;
    void UpdateRolloutPreviews();

    // Server address editing
    void OnServerAddressCommitted(const FText& NewText, ETextCommit::Type CommitType);
    void OnServerPortCommitted(const FText& NewText, ETextCommit::Type CommitType);

    // Target list
    TSharedRef<ITableRow> GenerateTargetRow(TSharedPtr<FRshipTargetListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    void OnTargetSelectionChanged(TSharedPtr<FRshipTargetListItem> Item, ESelectInfo::Type SelectInfo);

    // Build UI sections
    TSharedRef<SWidget> BuildConnectionSection();
    TSharedRef<SWidget> BuildTargetsSection();
    TSharedRef<SWidget> BuildDiagnosticsSection();
    TSharedRef<SWidget> BuildSyncTimingSection();

    void UpdateSyncSettings();
    bool ParsePositiveFloatInput(const FString& Input, float& OutValue) const;
    bool ParsePositiveIntInput(const FString& Input, int32& OutValue) const;

#if RSHIP_EDITOR_HAS_2110
    TSharedRef<SWidget> Build2110Section();
    FString GetDisplaySyncDomainId(const TSharedPtr<FString>& Selection) const;
    void Update2110Status();
    void UpdateSyncDomainOptions(const URship2110Subsystem* Subsystem);
    FText GetActiveSyncDomainOptionText() const;
    FText GetSyncDomainRateOptionText() const;
    FReply OnApplyClusterSyncRateClicked();
    FReply OnApplyRenderSubstepsClicked();
    FReply OnApplyCatchupStepsClicked();
    FReply OnApplyActiveSyncDomainClicked();
    FReply OnApplySyncDomainRateClicked();
#endif

    // Data
    TArray<TSharedPtr<FRshipTargetListItem>> TargetItems;
    TSharedPtr<SListView<TSharedPtr<FRshipTargetListItem>>> TargetListView;

    // Cached UI elements for updates
    TSharedPtr<STextBlock> ConnectionStatusText;
    TSharedPtr<SImage> StatusIndicator;
    TSharedPtr<SEditableTextBox> ServerAddressBox;
    TSharedPtr<SEditableTextBox> ServerPortBox;

    // Diagnostics text blocks
    TSharedPtr<STextBlock> QueueLengthText;
    TSharedPtr<STextBlock> MessageRateText;
    TSharedPtr<STextBlock> ByteRateText;
    TSharedPtr<STextBlock> DroppedText;
    TSharedPtr<STextBlock> InboundFrameCounterText;
    TSharedPtr<STextBlock> InboundNextApplyFrameText;
    TSharedPtr<STextBlock> InboundQueuedFrameSpanText;
    TSharedPtr<STextBlock> ExactDroppedText;
    TSharedPtr<STextBlock> BackoffText;
    TSharedPtr<SEditableTextBox> ControlSyncRateInput;
    TSharedPtr<SEditableTextBox> InboundLeadFramesInput;
    TSharedPtr<SCheckBox> InboundRequireExactFrameCheckBox;
    TSharedPtr<STextBlock> ControlSyncRateValueText;
    TSharedPtr<STextBlock> InboundLeadFramesValueText;
    TSharedPtr<STextBlock> SyncTimingStatusText;
    TSharedPtr<STextBlock> SyncTimingSummaryText;
    TSharedPtr<STextBlock> IniRolloutText;

#if RSHIP_EDITOR_HAS_2110
    // 2110 status text blocks
    TSharedPtr<STextBlock> RivermaxStatusText;
    TSharedPtr<STextBlock> PTPStatusText;
    TSharedPtr<STextBlock> IPMXStatusText;
    TSharedPtr<STextBlock> GPUDirectStatusText;
    TSharedPtr<STextBlock> NetworkStatusText;
    TSharedPtr<SEditableTextBox> ClusterSyncRateInput;
    TSharedPtr<SEditableTextBox> LocalRenderSubstepsInput;
    TSharedPtr<SEditableTextBox> MaxSyncCatchupStepsInput;
    TSharedPtr<STextBlock> ClusterSyncRateValueText;
    TSharedPtr<STextBlock> LocalRenderSubstepsValueText;
    TSharedPtr<STextBlock> MaxSyncCatchupStepsValueText;
    TSharedPtr<STextBlock> ActiveSyncDomainValueText;
    TSharedPtr<SEditableTextBox> SyncDomainRateInput;
    TSharedPtr<STextBlock> SyncDomainRateValueText;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ActiveSyncDomainCombo;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> SyncDomainRateCombo;
    TArray<TSharedPtr<FString>> SyncDomainOptions;
    TSharedPtr<FString> SelectedSyncDomainOption;
    TSharedPtr<FString> SelectedSyncDomainRateOption;
#endif

    // Refresh timer
    float RefreshTimer = 0.0f;
    static constexpr float RefreshInterval = 0.5f;  // Update every 0.5 seconds
};

/**
 * Row widget for target list items
 */
class SRshipTargetRow : public SMultiColumnTableRow<TSharedPtr<FRshipTargetListItem>>
{
public:
    SLATE_BEGIN_ARGS(SRshipTargetRow) {}
        SLATE_ARGUMENT(TSharedPtr<FRshipTargetListItem>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FRshipTargetListItem> Item;
};
