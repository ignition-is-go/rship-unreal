// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"

class URshipSubsystem;
class URshipTargetComponent;
class AActor;
class Action;
class SVerticalBox;
class FJsonObject;
struct FInstancedPropertyBag;
class FInstancePropertyBagStructureDataProvider;
class ISinglePropertyView;

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

/** Maps one property bag field to the original schema field path/type */
struct FRshipActionFieldBinding
{
    FName BagPropertyName;
    TArray<FString> FieldPath;
    FString ParamType;
    bool bIsVector3 = false;
    FString VectorXName;
    FString VectorYName;
    FString VectorZName;
};

/** Runtime UI state for one invokable action */
struct FRshipActionEntryState
{
    FString ActionId;
    FString ActionName;
    Action* ActionPtr = nullptr;
    TArray<FRshipActionFieldBinding> FieldBindings;
    TSharedPtr<FInstancedPropertyBag> ParameterBag;
    TSharedPtr<FInstancePropertyBagStructureDataProvider> BagDataProvider;
    TArray<TSharedPtr<ISinglePropertyView>> FieldViews;
    TSharedPtr<class STextBlock> ResultText;
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

    // Server address editing
    void OnServerAddressCommitted(const FText& NewText, ETextCommit::Type CommitType);
    void OnServerPortCommitted(const FText& NewText, ETextCommit::Type CommitType);

    // Target list
    TSharedRef<ITableRow> GenerateTargetRow(TSharedPtr<FRshipTargetListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    void OnTargetSelectionChanged(TSharedPtr<FRshipTargetListItem> Item, ESelectInfo::Type SelectInfo);

    // Editor selection sync
    void OnEditorSelectionChanged(UObject* Object);
    void SyncSelectionFromOutliner();

    // Build UI sections
    TSharedRef<SWidget> BuildConnectionSection();
    TSharedRef<SWidget> BuildTargetsSection();
    TSharedRef<SWidget> BuildActionsSection();
    TSharedRef<SWidget> BuildDiagnosticsSection();
    void RefreshActionsSection();
    FReply OnExecuteActionClicked(TSharedPtr<FRshipActionEntryState> ActionEntry);
    void OnActionExpansionChanged(bool bIsExpanded, FString ActionId);
    bool BuildActionPayload(const TSharedPtr<FRshipActionEntryState>& ActionEntry, TSharedPtr<FJsonObject>& OutPayload, FString& OutError) const;
    void AddSchemaFieldsRecursive(
        const TSharedPtr<FJsonObject>& ParamSchema,
        const TArray<FString>& FieldPath,
        const TSharedPtr<FRshipActionEntryState>& Entry,
        TSet<FName>& UsedBagNames);

#if RSHIP_EDITOR_HAS_2110
    TSharedRef<SWidget> Build2110Section();
    void Update2110Status();
#endif

    // Data
    TArray<TSharedPtr<FRshipTargetListItem>> TargetItems;
    TSharedPtr<SListView<TSharedPtr<FRshipTargetListItem>>> TargetListView;
    TWeakObjectPtr<URshipTargetComponent> SelectedTargetComponent;
    TWeakObjectPtr<AActor> SelectedTargetOwner;
    FString SelectedTargetId;
    TArray<TSharedPtr<FRshipActionEntryState>> ActionEntries;
    TSharedPtr<SVerticalBox> ActionsListBox;
    TMap<FString, bool> ActionExpansionState;

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
    TSharedPtr<STextBlock> BackoffText;

#if RSHIP_EDITOR_HAS_2110
    // 2110 status text blocks
    TSharedPtr<STextBlock> RivermaxStatusText;
    TSharedPtr<STextBlock> PTPStatusText;
    TSharedPtr<STextBlock> IPMXStatusText;
    TSharedPtr<STextBlock> GPUDirectStatusText;
    TSharedPtr<STextBlock> NetworkStatusText;
#endif

    // Refresh timer
    float RefreshTimer = 0.0f;
    static constexpr float RefreshInterval = 0.5f;  // Update every 0.5 seconds

    // Editor selection delegate handle
    FDelegateHandle SelectionChangedHandle;
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
    void OnTargetIdCommitted(const FText& NewText, ETextCommit::Type CommitType);

    TSharedPtr<FRshipTargetListItem> Item;
};
