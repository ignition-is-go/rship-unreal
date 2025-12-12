// Rship Editor Widget
// Dashboard UI for monitoring and controlling rship integration

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Styling/AppStyle.h"

class URshipSubsystem;
class URshipHealthMonitor;
class URshipFixtureManager;

// ============================================================================
// DASHBOARD DATA STRUCTURES
// ============================================================================

/** Info for displaying a fixture in the list */
struct FRshipDashboardFixtureItem
{
    FString Id;
    FString Name;
    FString Type;
    float Intensity;
    FLinearColor Color;
    bool bOnline;
};

/** Info for displaying a pulse in the activity log */
struct FRshipDashboardPulseItem
{
    FString EmitterId;
    FString Timestamp;
    FString Data;
    double Time;
};

// ============================================================================
// MAIN DASHBOARD WIDGET
// ============================================================================

/**
 * Main editor widget for rship dashboard.
 * Provides monitoring, control, and quick access to rship features.
 */
class RSHIPEXEC_API SRshipDashboardWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRshipDashboardWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SRshipDashboardWidget();

    // Tick for updates
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    // Subsystem reference
    URshipSubsystem* Subsystem;

    // Update timer
    float UpdateTimer;
    static constexpr float UpdateInterval = 0.25f;

    // ========================================================================
    // UI STATE
    // ========================================================================

    bool bIsConnected;
    int32 QueueLength;
    float QueuePressure;
    int32 MessagesSentPerSecond;
    int32 TargetCount;
    int32 FixtureCount;

    TArray<TSharedPtr<FRshipDashboardFixtureItem>> FixtureItems;
    TArray<TSharedPtr<FRshipDashboardPulseItem>> PulseItems;

    // ========================================================================
    // UI WIDGETS
    // ========================================================================

    TSharedPtr<STextBlock> ConnectionStatusText;
    TSharedPtr<STextBlock> QueueStatusText;
    TSharedPtr<STextBlock> ThroughputText;
    TSharedPtr<STextBlock> TargetCountText;
    TSharedPtr<STextBlock> FixtureCountText;

    TSharedPtr<SListView<TSharedPtr<FRshipDashboardFixtureItem>>> FixtureListView;
    TSharedPtr<SListView<TSharedPtr<FRshipDashboardPulseItem>>> PulseLogView;

    // ========================================================================
    // UI BUILDERS
    // ========================================================================

    TSharedRef<SWidget> BuildConnectionPanel();
    TSharedRef<SWidget> BuildStatsPanel();
    TSharedRef<SWidget> BuildFixturePanel();
    TSharedRef<SWidget> BuildPulseLogPanel();
    TSharedRef<SWidget> BuildQuickActionsPanel();

    // List view generators
    TSharedRef<ITableRow> GenerateFixtureRow(TSharedPtr<FRshipDashboardFixtureItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    TSharedRef<ITableRow> GeneratePulseRow(TSharedPtr<FRshipDashboardPulseItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

    // ========================================================================
    // ACTIONS
    // ========================================================================

    FReply OnReconnectClicked();
    FReply OnDiscoverSceneClicked();
    FReply OnConvertLightsClicked();
    FReply OnBlackoutClicked();
    FReply OnReleaseBlackoutClicked();
    FReply OnClearPulseLogClicked();
    FReply OnStartOSCClicked();
    FReply OnStartLiveLinkClicked();

    void OnMasterDimmerChanged(float NewValue);

    // ========================================================================
    // DATA UPDATE
    // ========================================================================

    void RefreshData();
    void RefreshFixtureList();
    void AddPulseLogEntry(const FString& EmitterId, const FString& Data);
};

// ============================================================================
// FIXTURE ROW WIDGET
// ============================================================================

class SRshipFixtureRowWidget : public SMultiColumnTableRow<TSharedPtr<FRshipDashboardFixtureItem>>
{
public:
    SLATE_BEGIN_ARGS(SRshipFixtureRowWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FRshipDashboardFixtureItem> InItem);

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FRshipDashboardFixtureItem> Item;
};

// ============================================================================
// PULSE LOG ROW WIDGET
// ============================================================================

class SRshipPulseRowWidget : public SMultiColumnTableRow<TSharedPtr<FRshipDashboardPulseItem>>
{
public:
    SLATE_BEGIN_ARGS(SRshipPulseRowWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FRshipDashboardPulseItem> InItem);

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FRshipDashboardPulseItem> Item;
};

// ============================================================================
// TAB SPAWNER
// ============================================================================

/** Helper class for spawning the dashboard tab */
class RSHIPEXEC_API FRshipDashboardTab
{
public:
    static const FName TabId;

    static void RegisterTabSpawner();
    static void UnregisterTabSpawner();

    static TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
};

#endif // WITH_EDITOR
