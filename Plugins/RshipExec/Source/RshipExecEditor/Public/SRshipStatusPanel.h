// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ActionProxy.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"

class URshipSubsystem;
class URshipActorRegistrationComponent;
class AActor;
class SVerticalBox;
class FJsonObject;
struct FInstancedPropertyBag;
class FInstancePropertyBagStructureDataProvider;
class ISinglePropertyView;

/** Row data for the target list */
struct FRshipTargetListItem
{
    FString FullTargetId;
    FString TargetId;
    FString DisplayName;
    FString TargetType;
    bool bIsOnline;
    int32 EmitterCount;
    int32 ActionCount;
    TArray<FString> ParentTargetIds;
    TArray<TSharedPtr<FRshipTargetListItem>> Children;
    TWeakObjectPtr<URshipActorRegistrationComponent> Component;

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
    FRshipActionProxy ActionBinding;
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

    // Global remote communication toggle
    ECheckBoxState GetRemoteToggleState() const;
    void OnRemoteToggleChanged(ECheckBoxState NewState);
    EVisibility GetRemoteOffBannerVisibility() const;
    bool IsRemoteControlsEnabled() const;

    void OnTargetSelectionChanged(TSharedPtr<FRshipTargetListItem> Item, ESelectInfo::Type SelectInfo);

    // Editor selection sync
    void OnEditorSelectionChanged(UObject* Object);
    void SyncSelectionFromOutliner();
    void TryApplyPendingTargetSelection();
    TSharedPtr<FRshipTargetListItem> FindTargetItemByFullTargetId(const FString& FullTargetId) const;
    TWeakObjectPtr<URshipActorRegistrationComponent> ResolveOwningComponentForTargetItem(TSharedPtr<FRshipTargetListItem> Item) const;

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

    // Data
    TArray<TSharedPtr<FRshipTargetListItem>> TargetItems;
    TArray<TSharedPtr<FRshipTargetListItem>> RootTargetItems;
    TSharedPtr<class ISceneOutliner> TargetSceneOutliner;
    TWeakObjectPtr<URshipActorRegistrationComponent> SelectedTargetComponent;
    TWeakObjectPtr<AActor> SelectedTargetOwner;
    FString SelectedTargetId;
    FString PendingSelectionTargetId;
    bool bSyncingToEditorSelection = false;
    bool bSyncingFromEditorSelection = false;
    TArray<TSharedPtr<FRshipActionEntryState>> ActionEntries;
    TSharedPtr<SVerticalBox> ActionsListBox;
    TMap<FString, bool> ActionExpansionState;

    // Cached UI elements for updates
    TSharedPtr<STextBlock> ConnectionStatusText;
    TSharedPtr<SImage> StatusIndicator;
    TSharedPtr<SEditableTextBox> ServerAddressBox;
    TSharedPtr<SEditableTextBox> ServerPortBox;
    TSharedPtr<class SCheckBox> RemoteToggleCheckBox;

    // Diagnostics text blocks
    TSharedPtr<STextBlock> QueueLengthText;
    TSharedPtr<STextBlock> MessageRateText;
    TSharedPtr<STextBlock> ByteRateText;
    TSharedPtr<STextBlock> DroppedText;
    TSharedPtr<STextBlock> BackoffText;

    // Refresh timer
    float RefreshTimer = 0.0f;
    static constexpr float RefreshInterval = 0.5f;  // Update every 0.5 seconds

    // Editor selection delegate handle
    FDelegateHandle SelectionChangedHandle;
};

