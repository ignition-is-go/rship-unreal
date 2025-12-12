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
class ACineCameraActor;

#if RSHIP_HAS_NDI
class URshipNDIStreamComponent;
#endif

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

#if RSHIP_HAS_NDI
/** Info for displaying an NDI stream in the list */
struct FRshipDashboardNDIItem
{
    /** The NDI stream component */
    TWeakObjectPtr<URshipNDIStreamComponent> Component;
    /** Owning camera actor */
    TWeakObjectPtr<ACineCameraActor> CameraActor;
    /** Stream name */
    FString StreamName;
    /** Camera name */
    FString CameraName;
    /** Resolution string (e.g., "7680x4320") */
    FString Resolution;
    /** Current FPS */
    float CurrentFPS;
    /** Target FPS */
    int32 TargetFPS;
    /** Connected receivers */
    int32 Receivers;
    /** Bandwidth in Mbps */
    float BandwidthMbps;
    /** Total frames sent */
    int64 FramesSent;
    /** Dropped frames */
    int64 DroppedFrames;
    /** Stream state (0=Stopped, 1=Starting, 2=Streaming, 3=Error) */
    int32 State;
    /** Is NDI sender available */
    bool bSenderAvailable;
};
#endif

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

#if RSHIP_HAS_NDI
    TArray<TSharedPtr<FRshipDashboardNDIItem>> NDIItems;
    int32 NDIStreamCount;
    int32 NDIActiveStreamCount;
    int32 NDITotalReceivers;
#endif

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

#if RSHIP_HAS_NDI
    TSharedPtr<SListView<TSharedPtr<FRshipDashboardNDIItem>>> NDIListView;
    TSharedPtr<STextBlock> NDIStreamCountText;
    TSharedPtr<STextBlock> NDIReceiverCountText;
    TSharedPtr<STextBlock> NDISenderStatusText;
#endif

    // ========================================================================
    // UI BUILDERS
    // ========================================================================

    TSharedRef<SWidget> BuildConnectionPanel();
    TSharedRef<SWidget> BuildStatsPanel();
    TSharedRef<SWidget> BuildFixturePanel();
    TSharedRef<SWidget> BuildPulseLogPanel();
    TSharedRef<SWidget> BuildQuickActionsPanel();

#if RSHIP_HAS_NDI
    TSharedRef<SWidget> BuildNDIPanel();
#endif

    // List view generators
    TSharedRef<ITableRow> GenerateFixtureRow(TSharedPtr<FRshipDashboardFixtureItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    TSharedRef<ITableRow> GeneratePulseRow(TSharedPtr<FRshipDashboardPulseItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

#if RSHIP_HAS_NDI
    TSharedRef<ITableRow> GenerateNDIRow(TSharedPtr<FRshipDashboardNDIItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
#endif

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

#if RSHIP_HAS_NDI
    FReply OnNDIStartAllClicked();
    FReply OnNDIStopAllClicked();
    void OnNDIStreamStartStopClicked(TSharedPtr<FRshipDashboardNDIItem> Item);
#endif

    // ========================================================================
    // DATA UPDATE
    // ========================================================================

    void RefreshData();
    void RefreshFixtureList();
    void AddPulseLogEntry(const FString& EmitterId, const FString& Data);

#if RSHIP_HAS_NDI
    void RefreshNDIList();
#endif
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

#if RSHIP_HAS_NDI
// ============================================================================
// NDI STREAM ROW WIDGET
// ============================================================================

class SRshipNDIRowWidget : public SMultiColumnTableRow<TSharedPtr<FRshipDashboardNDIItem>>
{
public:
    SLATE_BEGIN_ARGS(SRshipNDIRowWidget) {}
        SLATE_EVENT(FSimpleDelegate, OnStartStopClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FRshipDashboardNDIItem> InItem);

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FRshipDashboardNDIItem> Item;
    FSimpleDelegate OnStartStopClicked;

    FReply HandleStartStopClicked();
};
#endif

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
