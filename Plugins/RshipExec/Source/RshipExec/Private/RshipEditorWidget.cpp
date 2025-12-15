// Rship Editor Widget Implementation

#include "RshipEditorWidget.h"

#if WITH_EDITOR

#include "RshipSubsystem.h"
#include "RshipHealthMonitor.h"
#include "RshipFixtureManager.h"
#include "RshipSceneConverter.h"
#include "RshipDMXOutput.h"
#include "RshipOSCBridge.h"
#include "RshipLiveLinkSource.h"
#include "RshipPulseReceiver.h"
#include "CineCameraActor.h"
#include "EngineUtils.h"

#if RSHIP_HAS_NDI
#include "RshipNDIStreamComponent.h"
#include "RshipNDIStreamTypes.h"
#endif

#if RSHIP_HAS_COLOR_MANAGEMENT
#include "RshipColorManagementSubsystem.h"
#include "RshipColorConfig.h"
#endif

#include "Engine/Engine.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Styling/AppStyle.h"
#include "Logging/LogMacros.h"

#define LOCTEXT_NAMESPACE "RshipDashboard"

DEFINE_LOG_CATEGORY_STATIC(LogRshipDashboard, Log, All);

// ============================================================================
// TAB SPAWNER
// ============================================================================

const FName FRshipDashboardTab::TabId = FName("RshipDashboard");

void FRshipDashboardTab::RegisterTabSpawner()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId, FOnSpawnTab::CreateStatic(&FRshipDashboardTab::SpawnTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Rship Dashboard"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Monitor and control rship integration"))
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FRshipDashboardTab::UnregisterTabSpawner()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
}

TSharedRef<SDockTab> FRshipDashboardTab::SpawnTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipDashboardWidget)
        ];
}

// ============================================================================
// MAIN DASHBOARD WIDGET
// ============================================================================

void SRshipDashboardWidget::Construct(const FArguments& InArgs)
{
    // Get subsystem
    if (GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    }

    UpdateTimer = 0.0f;
    bIsConnected = false;
    QueueLength = 0;
    QueuePressure = 0.0f;
    MessagesSentPerSecond = 0;
    TargetCount = 0;
    FixtureCount = 0;

#if RSHIP_HAS_NDI
    NDIStreamCount = 0;
    NDIActiveStreamCount = 0;
    NDITotalReceivers = 0;
#endif

#if RSHIP_HAS_COLOR_MANAGEMENT
    CurrentExposureMode = 1;  // Auto
    CurrentManualEV = 0.0f;
    CurrentExposureBias = 0.0f;
    CurrentColorSpace = 1;  // Rec709
    bCurrentHDREnabled = false;
    bCurrentSyncToViewport = true;
#endif

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.0f)
        [
            SNew(SScrollBox)
            +SScrollBox::Slot()
            [
                SNew(SVerticalBox)

                // Connection Panel
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    BuildConnectionPanel()
                ]

                // Stats Panel
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    BuildStatsPanel()
                ]

                // Quick Actions
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    BuildQuickActionsPanel()
                ]

#if RSHIP_HAS_NDI
                // NDI Streaming
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    BuildNDIPanel()
                ]
#endif

#if RSHIP_HAS_COLOR_MANAGEMENT
                // Color Management
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    BuildColorManagementPanel()
                ]
#endif

                // Fixtures
                +SVerticalBox::Slot()
                .FillHeight(0.4f)
                .Padding(0, 0, 0, 8)
                [
                    BuildFixturePanel()
                ]

                // Pulse Log
                +SVerticalBox::Slot()
                .FillHeight(0.3f)
                [
                    BuildPulseLogPanel()
                ]
            ]
        ]
    ];

    // Initial data refresh
    RefreshData();
}

SRshipDashboardWidget::~SRshipDashboardWidget()
{
}

void SRshipDashboardWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    UpdateTimer += InDeltaTime;
    if (UpdateTimer >= UpdateInterval)
    {
        UpdateTimer = 0.0f;
        RefreshData();
    }
}

TSharedRef<SWidget> SRshipDashboardWidget::BuildConnectionPanel()
{
    return SNew(SExpandableArea)
        .AreaTitle(LOCTEXT("ConnectionTitle", "Connection"))
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            SNew(SVerticalBox)

            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("StatusLabel", "Status: "))
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SAssignNew(ConnectionStatusText, STextBlock)
                    .Text(LOCTEXT("Disconnected", "Disconnected"))
                    .ColorAndOpacity(FSlateColor(FLinearColor::Red))
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Reconnect", "Reconnect"))
                    .OnClicked(this, &SRshipDashboardWidget::OnReconnectClicked)
                ]
            ]
        ];
}

TSharedRef<SWidget> SRshipDashboardWidget::BuildStatsPanel()
{
    return SNew(SExpandableArea)
        .AreaTitle(LOCTEXT("StatsTitle", "Statistics"))
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            SNew(SVerticalBox)

            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .FillWidth(0.5f)
                [
                    SNew(SVerticalBox)

                    +SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock).Text(LOCTEXT("QueueLabel", "Queue: "))
                        ]
                        +SHorizontalBox::Slot().AutoWidth()
                        [
                            SAssignNew(QueueStatusText, STextBlock).Text(LOCTEXT("QueueEmpty", "0 (0%)"))
                        ]
                    ]

                    +SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock).Text(LOCTEXT("ThroughputLabel", "Msgs/sec: "))
                        ]
                        +SHorizontalBox::Slot().AutoWidth()
                        [
                            SAssignNew(ThroughputText, STextBlock).Text(LOCTEXT("ThroughputZero", "0"))
                        ]
                    ]
                ]

                +SHorizontalBox::Slot()
                .FillWidth(0.5f)
                [
                    SNew(SVerticalBox)

                    +SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock).Text(LOCTEXT("TargetsLabel", "Targets: "))
                        ]
                        +SHorizontalBox::Slot().AutoWidth()
                        [
                            SAssignNew(TargetCountText, STextBlock).Text(LOCTEXT("TargetsZero", "0"))
                        ]
                    ]

                    +SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock).Text(LOCTEXT("FixturesLabel", "Fixtures: "))
                        ]
                        +SHorizontalBox::Slot().AutoWidth()
                        [
                            SAssignNew(FixtureCountText, STextBlock).Text(LOCTEXT("FixturesZero", "0"))
                        ]
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SRshipDashboardWidget::BuildQuickActionsPanel()
{
    return SNew(SExpandableArea)
        .AreaTitle(LOCTEXT("QuickActionsTitle", "Quick Actions"))
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            SNew(SVerticalBox)

            // Row 1: Scene
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(2)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("DiscoverScene", "Discover Scene"))
                    .OnClicked(this, &SRshipDashboardWidget::OnDiscoverSceneClicked)
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(2)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ConvertLights", "Convert Lights"))
                    .OnClicked(this, &SRshipDashboardWidget::OnConvertLightsClicked)
                ]
            ]

            // Row 2: DMX
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(2)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Blackout", "Blackout"))
                    .OnClicked(this, &SRshipDashboardWidget::OnBlackoutClicked)
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(2)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ReleaseBlackout", "Release"))
                    .OnClicked(this, &SRshipDashboardWidget::OnReleaseBlackoutClicked)
                ]
            ]

            // Row 3: Integrations
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(2)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("StartOSC", "Start OSC Server"))
                    .OnClicked(this, &SRshipDashboardWidget::OnStartOSCClicked)
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(2)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("StartLiveLink", "Start Live Link"))
                    .OnClicked(this, &SRshipDashboardWidget::OnStartLiveLinkClicked)
                ]
            ]

            // Master Dimmer
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text(LOCTEXT("MasterDimmer", "Master: "))
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SSlider)
                    .Value(1.0f)
                    .OnValueChanged(this, &SRshipDashboardWidget::OnMasterDimmerChanged)
                ]
            ]
        ];
}

TSharedRef<SWidget> SRshipDashboardWidget::BuildFixturePanel()
{
    return SNew(SExpandableArea)
        .AreaTitle(LOCTEXT("FixturesTitle", "Fixtures"))
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
            [
                SAssignNew(FixtureListView, SListView<TSharedPtr<FRshipDashboardFixtureItem>>)
                .ListItemsSource(&FixtureItems)
                .OnGenerateRow(this, &SRshipDashboardWidget::GenerateFixtureRow)
                .HeaderRow
                (
                    SNew(SHeaderRow)
                    +SHeaderRow::Column("Name")
                    .DefaultLabel(LOCTEXT("NameColumn", "Name"))
                    .FillWidth(0.3f)

                    +SHeaderRow::Column("Type")
                    .DefaultLabel(LOCTEXT("TypeColumn", "Type"))
                    .FillWidth(0.2f)

                    +SHeaderRow::Column("Intensity")
                    .DefaultLabel(LOCTEXT("IntensityColumn", "Intensity"))
                    .FillWidth(0.2f)

                    +SHeaderRow::Column("Color")
                    .DefaultLabel(LOCTEXT("ColorColumn", "Color"))
                    .FillWidth(0.2f)

                    +SHeaderRow::Column("Status")
                    .DefaultLabel(LOCTEXT("StatusColumn", "Status"))
                    .FillWidth(0.1f)
                )
            ]
        ];
}

TSharedRef<SWidget> SRshipDashboardWidget::BuildPulseLogPanel()
{
    return SNew(SExpandableArea)
        .AreaTitle(LOCTEXT("PulseLogTitle", "Pulse Activity"))
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            SNew(SVerticalBox)

            +SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SButton)
                .Text(LOCTEXT("ClearLog", "Clear"))
                .OnClicked(this, &SRshipDashboardWidget::OnClearPulseLogClicked)
            ]

            +SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
                [
                    SAssignNew(PulseLogView, SListView<TSharedPtr<FRshipDashboardPulseItem>>)
                    .ListItemsSource(&PulseItems)
                    .OnGenerateRow(this, &SRshipDashboardWidget::GeneratePulseRow)
                    .HeaderRow
                    (
                        SNew(SHeaderRow)
                        +SHeaderRow::Column("Time")
                        .DefaultLabel(LOCTEXT("TimeColumn", "Time"))
                        .FillWidth(0.2f)

                        +SHeaderRow::Column("Emitter")
                        .DefaultLabel(LOCTEXT("EmitterColumn", "Emitter"))
                        .FillWidth(0.4f)

                        +SHeaderRow::Column("Data")
                        .DefaultLabel(LOCTEXT("DataColumn", "Data"))
                        .FillWidth(0.4f)
                    )
                ]
            ]
        ];
}

TSharedRef<ITableRow> SRshipDashboardWidget::GenerateFixtureRow(TSharedPtr<FRshipDashboardFixtureItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SRshipFixtureRowWidget, OwnerTable, Item);
}

TSharedRef<ITableRow> SRshipDashboardWidget::GeneratePulseRow(TSharedPtr<FRshipDashboardPulseItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SRshipPulseRowWidget, OwnerTable, Item);
}

// ============================================================================
// ACTIONS
// ============================================================================

FReply SRshipDashboardWidget::OnReconnectClicked()
{
    if (Subsystem)
    {
        Subsystem->Reconnect();
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnDiscoverSceneClicked()
{
    if (Subsystem)
    {
        if (URshipSceneConverter* Converter = Subsystem->GetSceneConverter())
        {
            FRshipDiscoveryOptions Options;
            int32 Count = Converter->DiscoverScene(Options);
            UE_LOG(LogRshipDashboard, Log, TEXT("Discovered %d items"), Count);
        }
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnConvertLightsClicked()
{
    if (Subsystem)
    {
        if (URshipSceneConverter* Converter = Subsystem->GetSceneConverter())
        {
            FRshipConversionOptions Options;
            TArray<FRshipConversionResult> Results;
            int32 Count = Converter->ConvertAllLightsValidated(Options, Results);
            UE_LOG(LogRshipDashboard, Log, TEXT("Converted %d lights"), Count);
        }
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnBlackoutClicked()
{
    if (Subsystem)
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->Blackout();
        }
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnReleaseBlackoutClicked()
{
    if (Subsystem)
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->ReleaseBlackout();
        }
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnClearPulseLogClicked()
{
    PulseItems.Empty();
    if (PulseLogView.IsValid())
    {
        PulseLogView->RequestListRefresh();
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnStartOSCClicked()
{
    if (Subsystem)
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            if (OSC->IsServerRunning())
            {
                OSC->StopServer();
            }
            else
            {
                OSC->StartServer(8000);
            }
        }
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnStartLiveLinkClicked()
{
    if (Subsystem)
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            if (LL->IsSourceActive())
            {
                LL->StopSource();
            }
            else
            {
                LL->StartSource();
            }
        }
    }
    return FReply::Handled();
}

void SRshipDashboardWidget::OnMasterDimmerChanged(float NewValue)
{
    if (Subsystem)
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->SetMasterDimmer(NewValue);
        }
    }
}

// ============================================================================
// DATA UPDATE
// ============================================================================

void SRshipDashboardWidget::RefreshData()
{
    if (!Subsystem) return;

    // Update connection status
    bIsConnected = Subsystem->IsConnected();
    if (ConnectionStatusText.IsValid())
    {
        if (bIsConnected)
        {
            ConnectionStatusText->SetText(LOCTEXT("Connected", "Connected"));
            ConnectionStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
        }
        else
        {
            ConnectionStatusText->SetText(LOCTEXT("Disconnected", "Disconnected"));
            ConnectionStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
        }
    }

    // Update queue stats
    QueueLength = Subsystem->GetQueueLength();
    QueuePressure = Subsystem->GetQueuePressure();
    if (QueueStatusText.IsValid())
    {
        QueueStatusText->SetText(FText::Format(
            LOCTEXT("QueueFormat", "{0} ({1}%)"),
            FText::AsNumber(QueueLength),
            FText::AsNumber(FMath::RoundToInt(QueuePressure * 100))
        ));
    }

    // Update throughput
    MessagesSentPerSecond = Subsystem->GetMessagesSentPerSecond();
    if (ThroughputText.IsValid())
    {
        ThroughputText->SetText(FText::AsNumber(MessagesSentPerSecond));
    }

    // Update target count
    if (Subsystem->TargetComponents)
    {
        TargetCount = Subsystem->TargetComponents->Num();
    }
    if (TargetCountText.IsValid())
    {
        TargetCountText->SetText(FText::AsNumber(TargetCount));
    }

    // Update fixture count
    if (URshipFixtureManager* FM = Subsystem->GetFixtureManager())
    {
        FixtureCount = FM->GetAllFixtures().Num();
    }
    if (FixtureCountText.IsValid())
    {
        FixtureCountText->SetText(FText::AsNumber(FixtureCount));
    }

    RefreshFixtureList();

#if RSHIP_HAS_NDI
    RefreshNDIList();
#endif

#if RSHIP_HAS_COLOR_MANAGEMENT
    RefreshColorData();
#endif
}

void SRshipDashboardWidget::RefreshFixtureList()
{
    if (!Subsystem) return;

    URshipFixtureManager* FM = Subsystem->GetFixtureManager();
    if (!FM) return;

    TArray<FRshipFixtureInfo> Fixtures = FM->GetAllFixtures();

    FixtureItems.Empty();
    for (const FRshipFixtureInfo& Fixture : Fixtures)
    {
        TSharedPtr<FRshipDashboardFixtureItem> Item = MakeShared<FRshipDashboardFixtureItem>();
        Item->Id = Fixture.Id;
        Item->Name = Fixture.Name;
        Item->Type = Fixture.FixtureTypeId;
        Item->Intensity = 0.0f;  // Runtime state not available from FRshipFixtureInfo
        Item->Color = FLinearColor::White;  // Runtime state not available from FRshipFixtureInfo
        Item->bOnline = true;
        FixtureItems.Add(Item);
    }

    if (FixtureListView.IsValid())
    {
        FixtureListView->RequestListRefresh();
    }
}

void SRshipDashboardWidget::AddPulseLogEntry(const FString& EmitterId, const FString& Data)
{
    TSharedPtr<FRshipDashboardPulseItem> Item = MakeShared<FRshipDashboardPulseItem>();
    Item->EmitterId = EmitterId;
    Item->Data = Data;
    Item->Time = FPlatformTime::Seconds();

    FDateTime Now = FDateTime::Now();
    Item->Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"), Now.GetHour(), Now.GetMinute(), Now.GetSecond());

    // Add to front, limit size
    PulseItems.Insert(Item, 0);
    if (PulseItems.Num() > 100)
    {
        PulseItems.SetNum(100);
    }

    if (PulseLogView.IsValid())
    {
        PulseLogView->RequestListRefresh();
    }
}

// ============================================================================
// FIXTURE ROW WIDGET
// ============================================================================

void SRshipFixtureRowWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FRshipDashboardFixtureItem> InItem)
{
    Item = InItem;
    SMultiColumnTableRow<TSharedPtr<FRshipDashboardFixtureItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipFixtureRowWidget::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    if (ColumnName == "Name")
    {
        return SNew(STextBlock).Text(FText::FromString(Item->Name));
    }
    else if (ColumnName == "Type")
    {
        return SNew(STextBlock).Text(FText::FromString(Item->Type));
    }
    else if (ColumnName == "Intensity")
    {
        return SNew(STextBlock).Text(FText::Format(LOCTEXT("IntensityFmt", "{0}%"), FText::AsNumber(FMath::RoundToInt(Item->Intensity * 100))));
    }
    else if (ColumnName == "Color")
    {
        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(Item->Color)
            .Padding(FMargin(8, 2));
    }
    else if (ColumnName == "Status")
    {
        return SNew(STextBlock)
            .Text(Item->bOnline ? LOCTEXT("Online", "ON") : LOCTEXT("Offline", "OFF"))
            .ColorAndOpacity(Item->bOnline ? FSlateColor(FLinearColor::Green) : FSlateColor(FLinearColor::Red));
    }

    return SNullWidget::NullWidget;
}

// ============================================================================
// PULSE ROW WIDGET
// ============================================================================

void SRshipPulseRowWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FRshipDashboardPulseItem> InItem)
{
    Item = InItem;
    SMultiColumnTableRow<TSharedPtr<FRshipDashboardPulseItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipPulseRowWidget::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    if (ColumnName == "Time")
    {
        return SNew(STextBlock).Text(FText::FromString(Item->Timestamp));
    }
    else if (ColumnName == "Emitter")
    {
        return SNew(STextBlock).Text(FText::FromString(Item->EmitterId));
    }
    else if (ColumnName == "Data")
    {
        return SNew(STextBlock).Text(FText::FromString(Item->Data));
    }

    return SNullWidget::NullWidget;
}

// ============================================================================
// NDI STREAMING
// ============================================================================

#if RSHIP_HAS_NDI

TSharedRef<SWidget> SRshipDashboardWidget::BuildNDIPanel()
{
    return SNew(SExpandableArea)
        .AreaTitle(LOCTEXT("NDITitle", "NDI Streaming"))
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            SNew(SVerticalBox)

            // Status bar
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("NDISenderLabel", "Sender: "))
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0, 0, 16, 0)
                [
                    SAssignNew(NDISenderStatusText, STextBlock)
                    .Text(LOCTEXT("NDISenderUnavailable", "Unavailable"))
                    .ColorAndOpacity(FSlateColor(FLinearColor::Red))
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("NDIStreamsLabel", "Streams: "))
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0, 0, 16, 0)
                [
                    SAssignNew(NDIStreamCountText, STextBlock)
                    .Text(LOCTEXT("NDIStreamsZero", "0 / 0"))
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("NDIReceiversLabel", "Receivers: "))
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SAssignNew(NDIReceiverCountText, STextBlock)
                    .Text(LOCTEXT("NDIReceiversZero", "0"))
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("NDIStartAll", "Start All"))
                    .OnClicked(this, &SRshipDashboardWidget::OnNDIStartAllClicked)
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("NDIStopAll", "Stop All"))
                    .OnClicked(this, &SRshipDashboardWidget::OnNDIStopAllClicked)
                ]
            ]

            // Stream list
            +SVerticalBox::Slot()
            .AutoHeight()
            .MaxHeight(200.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
                [
                    SAssignNew(NDIListView, SListView<TSharedPtr<FRshipDashboardNDIItem>>)
                    .ListItemsSource(&NDIItems)
                    .OnGenerateRow(this, &SRshipDashboardWidget::GenerateNDIRow)
                    .HeaderRow
                    (
                        SNew(SHeaderRow)
                        +SHeaderRow::Column("Camera")
                        .DefaultLabel(LOCTEXT("NDICameraColumn", "Camera"))
                        .FillWidth(0.2f)

                        +SHeaderRow::Column("Stream")
                        .DefaultLabel(LOCTEXT("NDIStreamColumn", "Stream Name"))
                        .FillWidth(0.2f)

                        +SHeaderRow::Column("Resolution")
                        .DefaultLabel(LOCTEXT("NDIResColumn", "Resolution"))
                        .FillWidth(0.15f)

                        +SHeaderRow::Column("FPS")
                        .DefaultLabel(LOCTEXT("NDIFPSColumn", "FPS"))
                        .FillWidth(0.1f)

                        +SHeaderRow::Column("Receivers")
                        .DefaultLabel(LOCTEXT("NDIRxColumn", "Rx"))
                        .FillWidth(0.08f)

                        +SHeaderRow::Column("Status")
                        .DefaultLabel(LOCTEXT("NDIStatusColumn", "Status"))
                        .FillWidth(0.12f)

                        +SHeaderRow::Column("Action")
                        .DefaultLabel(LOCTEXT("NDIActionColumn", ""))
                        .FillWidth(0.15f)
                    )
                ]
            ]
        ];
}

TSharedRef<ITableRow> SRshipDashboardWidget::GenerateNDIRow(TSharedPtr<FRshipDashboardNDIItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SRshipNDIRowWidget, OwnerTable, Item)
        .OnStartStopClicked(FSimpleDelegate::CreateSP(this, &SRshipDashboardWidget::OnNDIStreamStartStopClicked, Item));
}

void SRshipDashboardWidget::RefreshNDIList()
{
    NDIItems.Empty();
    NDIStreamCount = 0;
    NDIActiveStreamCount = 0;
    NDITotalReceivers = 0;

    // Check sender availability
    bool bSenderAvailable = URshipNDIStreamComponent::IsNDISenderAvailable();

    if (NDISenderStatusText.IsValid())
    {
        if (bSenderAvailable)
        {
            NDISenderStatusText->SetText(LOCTEXT("NDISenderAvailable", "Available"));
            NDISenderStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
        }
        else
        {
            NDISenderStatusText->SetText(LOCTEXT("NDISenderUnavailable", "Unavailable"));
            NDISenderStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
        }
    }

    // Find all CineCameraActors with NDI components
    UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
    if (!World)
    {
        World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    }

    if (World)
    {
        for (TActorIterator<ACineCameraActor> It(World); It; ++It)
        {
            ACineCameraActor* CameraActor = *It;
            if (!CameraActor) continue;

            URshipNDIStreamComponent* NDIComp = CameraActor->FindComponentByClass<URshipNDIStreamComponent>();
            if (!NDIComp) continue;

            TSharedPtr<FRshipDashboardNDIItem> Item = MakeShared<FRshipDashboardNDIItem>();
            Item->Component = NDIComp;
            Item->CameraActor = CameraActor;
            Item->CameraName = CameraActor->GetActorLabel();
            Item->StreamName = NDIComp->Config.StreamName;
            Item->Resolution = FString::Printf(TEXT("%dx%d"), NDIComp->Config.Width, NDIComp->Config.Height);
            Item->TargetFPS = NDIComp->Config.FrameRate;
            Item->bSenderAvailable = bSenderAvailable;

            // Get runtime stats
            FRshipNDIStreamStats Stats = NDIComp->GetStats();
            Item->CurrentFPS = Stats.CurrentFPS;
            Item->Receivers = Stats.ConnectedReceivers;
            Item->BandwidthMbps = Stats.BandwidthMbps;
            Item->FramesSent = Stats.TotalFramesSent;
            Item->DroppedFrames = Stats.DroppedFrames;
            Item->State = static_cast<int32>(NDIComp->GetStreamState());

            NDIItems.Add(Item);
            NDIStreamCount++;
            NDITotalReceivers += Item->Receivers;

            if (Item->State == 2) // Streaming
            {
                NDIActiveStreamCount++;
            }
        }
    }

    // Update summary texts
    if (NDIStreamCountText.IsValid())
    {
        NDIStreamCountText->SetText(FText::Format(
            LOCTEXT("NDIStreamCountFmt", "{0} / {1}"),
            FText::AsNumber(NDIActiveStreamCount),
            FText::AsNumber(NDIStreamCount)
        ));
    }

    if (NDIReceiverCountText.IsValid())
    {
        NDIReceiverCountText->SetText(FText::AsNumber(NDITotalReceivers));
    }

    if (NDIListView.IsValid())
    {
        NDIListView->RequestListRefresh();
    }
}

FReply SRshipDashboardWidget::OnNDIStartAllClicked()
{
    for (const TSharedPtr<FRshipDashboardNDIItem>& Item : NDIItems)
    {
        if (Item.IsValid() && Item->Component.IsValid())
        {
            URshipNDIStreamComponent* Comp = Item->Component.Get();
            if (!Comp->IsStreaming())
            {
                Comp->StartStreaming();
            }
        }
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnNDIStopAllClicked()
{
    for (const TSharedPtr<FRshipDashboardNDIItem>& Item : NDIItems)
    {
        if (Item.IsValid() && Item->Component.IsValid())
        {
            URshipNDIStreamComponent* Comp = Item->Component.Get();
            if (Comp->IsStreaming())
            {
                Comp->StopStreaming();
            }
        }
    }
    return FReply::Handled();
}

void SRshipDashboardWidget::OnNDIStreamStartStopClicked(TSharedPtr<FRshipDashboardNDIItem> Item)
{
    if (!Item.IsValid() || !Item->Component.IsValid())
    {
        return;
    }

    URshipNDIStreamComponent* Comp = Item->Component.Get();
    if (Comp->IsStreaming())
    {
        Comp->StopStreaming();
    }
    else
    {
        Comp->StartStreaming();
    }
}

// ============================================================================
// NDI ROW WIDGET
// ============================================================================

void SRshipNDIRowWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FRshipDashboardNDIItem> InItem)
{
    Item = InItem;
    OnStartStopClicked = InArgs._OnStartStopClicked;
    SMultiColumnTableRow<TSharedPtr<FRshipDashboardNDIItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipNDIRowWidget::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    if (ColumnName == "Camera")
    {
        return SNew(STextBlock).Text(FText::FromString(Item->CameraName));
    }
    else if (ColumnName == "Stream")
    {
        return SNew(STextBlock).Text(FText::FromString(Item->StreamName));
    }
    else if (ColumnName == "Resolution")
    {
        return SNew(STextBlock).Text(FText::FromString(Item->Resolution));
    }
    else if (ColumnName == "FPS")
    {
        FText FPSText = FText::Format(
            LOCTEXT("NDIFPSFmt", "{0}/{1}"),
            FText::AsNumber(FMath::RoundToInt(Item->CurrentFPS)),
            FText::AsNumber(Item->TargetFPS)
        );
        return SNew(STextBlock).Text(FPSText);
    }
    else if (ColumnName == "Receivers")
    {
        return SNew(STextBlock).Text(FText::AsNumber(Item->Receivers));
    }
    else if (ColumnName == "Status")
    {
        FText StatusText;
        FLinearColor StatusColor;

        switch (Item->State)
        {
        case 0: // Stopped
            StatusText = LOCTEXT("NDIStopped", "Stopped");
            StatusColor = FLinearColor::Gray;
            break;
        case 1: // Starting
            StatusText = LOCTEXT("NDIStarting", "Starting");
            StatusColor = FLinearColor::Yellow;
            break;
        case 2: // Streaming
            StatusText = LOCTEXT("NDIStreaming", "Streaming");
            StatusColor = FLinearColor::Green;
            break;
        case 3: // Error
            StatusText = LOCTEXT("NDIError", "Error");
            StatusColor = FLinearColor::Red;
            break;
        default:
            StatusText = LOCTEXT("NDIUnknown", "Unknown");
            StatusColor = FLinearColor::Gray;
            break;
        }

        return SNew(STextBlock)
            .Text(StatusText)
            .ColorAndOpacity(FSlateColor(StatusColor));
    }
    else if (ColumnName == "Action")
    {
        FText ButtonText = (Item->State == 2) ? LOCTEXT("NDIStop", "Stop") : LOCTEXT("NDIStart", "Start");
        bool bEnabled = Item->bSenderAvailable && (Item->State == 0 || Item->State == 2);

        return SNew(SButton)
            .Text(ButtonText)
            .IsEnabled(bEnabled)
            .OnClicked(this, &SRshipNDIRowWidget::HandleStartStopClicked);
    }

    return SNullWidget::NullWidget;
}

FReply SRshipNDIRowWidget::HandleStartStopClicked()
{
    OnStartStopClicked.ExecuteIfBound();
    return FReply::Handled();
}

#endif // RSHIP_HAS_NDI

// ============================================================================
// COLOR MANAGEMENT PANEL
// ============================================================================

#if RSHIP_HAS_COLOR_MANAGEMENT

TSharedRef<SWidget> SRshipDashboardWidget::BuildColorManagementPanel()
{
    return SNew(SExpandableArea)
        .AreaTitle(LOCTEXT("ColorTitle", "Color Management"))
        .InitiallyCollapsed(false)
        .BodyContent()
        [
            SNew(SVerticalBox)

            // Exposure Mode Row
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ExposureModeLabel", "Exposure: "))
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(4, 0)
                [
                    SAssignNew(ExposureModeText, STextBlock)
                    .Text(LOCTEXT("ExposureAuto", "Auto"))
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ManualBtn", "Manual"))
                    .OnClicked(this, &SRshipDashboardWidget::OnExposureModeManualClicked)
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("AutoBtn", "Auto"))
                    .OnClicked(this, &SRshipDashboardWidget::OnExposureModeAutoClicked)
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("HistogramBtn", "Histogram"))
                    .OnClicked(this, &SRshipDashboardWidget::OnExposureModeHistogramClicked)
                ]
            ]

            // Manual EV Slider
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ManualEVLabel", "Manual EV: "))
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .Padding(4, 0)
                [
                    SAssignNew(ManualEVSlider, SSlider)
                    .Value(0.5f)  // -16 to +16 -> 0 to 1
                    .OnValueChanged(this, &SRshipDashboardWidget::OnManualEVChanged)
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SAssignNew(ManualEVValueText, STextBlock)
                    .Text(LOCTEXT("ManualEVValue", "0.0 EV"))
                    .MinDesiredWidth(60.0f)
                ]
            ]

            // Exposure Bias Slider
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ExposureBiasLabel", "Bias: "))
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .Padding(4, 0)
                [
                    SAssignNew(ExposureBiasSlider, SSlider)
                    .Value(0.5f)  // -4 to +4 -> 0 to 1
                    .OnValueChanged(this, &SRshipDashboardWidget::OnExposureBiasChanged)
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SAssignNew(ExposureBiasValueText, STextBlock)
                    .Text(LOCTEXT("ExposureBiasValue", "0.0 EV"))
                    .MinDesiredWidth(60.0f)
                ]
            ]

            // Color Space Row
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ColorSpaceLabel", "Color Space: "))
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(4, 0)
                [
                    SAssignNew(ColorSpaceText, STextBlock)
                    .Text(LOCTEXT("ColorSpaceRec709", "Rec.709"))
                ]
            ]

            // HDR and Viewport Sync Row
            +SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4)
            [
                SNew(SHorizontalBox)

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SAssignNew(HDREnabledCheckbox, SCheckBox)
                    .IsChecked(ECheckBoxState::Unchecked)
                    .OnCheckStateChanged(this, &SRshipDashboardWidget::OnHDREnabledChanged)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("HDREnabled", "HDR Output"))
                    ]
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(16, 0, 0, 0)
                [
                    SAssignNew(ViewportSyncCheckbox, SCheckBox)
                    .IsChecked(ECheckBoxState::Checked)
                    .OnCheckStateChanged(this, &SRshipDashboardWidget::OnViewportSyncChanged)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ViewportSync", "Sync to Viewport"))
                    ]
                ]

                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]

                +SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ApplyViewport", "Apply to Viewport"))
                    .OnClicked(this, &SRshipDashboardWidget::OnApplyToViewportClicked)
                ]
            ]
        ];
}

void SRshipDashboardWidget::RefreshColorData()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return;
    }

    URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>();
    if (!ColorSubsystem)
    {
        return;
    }

    FRshipColorConfig Config = ColorSubsystem->GetColorConfig();

    // Update exposure mode
    CurrentExposureMode = static_cast<int32>(Config.Exposure.Mode);
    FString ModeStr;
    switch (Config.Exposure.Mode)
    {
    case ERshipExposureMode::Manual: ModeStr = TEXT("Manual"); break;
    case ERshipExposureMode::Auto: ModeStr = TEXT("Auto"); break;
    case ERshipExposureMode::Histogram: ModeStr = TEXT("Histogram"); break;
    }
    if (ExposureModeText.IsValid())
    {
        ExposureModeText->SetText(FText::FromString(ModeStr));
    }

    // Update manual EV
    CurrentManualEV = Config.Exposure.ManualExposureEV;
    if (ManualEVSlider.IsValid())
    {
        // Map -16 to +16 -> 0 to 1
        float SliderValue = (CurrentManualEV + 16.0f) / 32.0f;
        ManualEVSlider->SetValue(SliderValue);
    }
    if (ManualEVValueText.IsValid())
    {
        ManualEVValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f EV"), CurrentManualEV)));
    }

    // Update exposure bias
    CurrentExposureBias = Config.Exposure.ExposureBias;
    if (ExposureBiasSlider.IsValid())
    {
        // Map -4 to +4 -> 0 to 1
        float SliderValue = (CurrentExposureBias + 4.0f) / 8.0f;
        ExposureBiasSlider->SetValue(SliderValue);
    }
    if (ExposureBiasValueText.IsValid())
    {
        ExposureBiasValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f EV"), CurrentExposureBias)));
    }

    // Update color space
    CurrentColorSpace = static_cast<int32>(Config.ColorSpace);
    FString ColorSpaceStr;
    switch (Config.ColorSpace)
    {
    case ERshipColorSpace::sRGB: ColorSpaceStr = TEXT("sRGB"); break;
    case ERshipColorSpace::Rec709: ColorSpaceStr = TEXT("Rec.709"); break;
    case ERshipColorSpace::Rec2020: ColorSpaceStr = TEXT("Rec.2020"); break;
    case ERshipColorSpace::DCIP3: ColorSpaceStr = TEXT("DCI-P3"); break;
    }
    if (ColorSpaceText.IsValid())
    {
        ColorSpaceText->SetText(FText::FromString(ColorSpaceStr));
    }

    // Update HDR
    bCurrentHDREnabled = Config.bEnableHDR;
    if (HDREnabledCheckbox.IsValid())
    {
        HDREnabledCheckbox->SetIsChecked(bCurrentHDREnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }

    // Update viewport sync
    bCurrentSyncToViewport = Config.bSyncExposureToViewport;
    if (ViewportSyncCheckbox.IsValid())
    {
        ViewportSyncCheckbox->SetIsChecked(bCurrentSyncToViewport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }
}

FReply SRshipDashboardWidget::OnExposureModeManualClicked()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>())
        {
            FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
            Config.Exposure.Mode = ERshipExposureMode::Manual;
            ColorSubsystem->SetColorConfig(Config);
        }
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnExposureModeAutoClicked()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>())
        {
            FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
            Config.Exposure.Mode = ERshipExposureMode::Auto;
            ColorSubsystem->SetColorConfig(Config);
        }
    }
    return FReply::Handled();
}

FReply SRshipDashboardWidget::OnExposureModeHistogramClicked()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>())
        {
            FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
            Config.Exposure.Mode = ERshipExposureMode::Histogram;
            ColorSubsystem->SetColorConfig(Config);
        }
    }
    return FReply::Handled();
}

void SRshipDashboardWidget::OnManualEVChanged(float NewValue)
{
    // Map 0 to 1 -> -16 to +16
    float EV = (NewValue * 32.0f) - 16.0f;

    if (ManualEVValueText.IsValid())
    {
        ManualEVValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f EV"), EV)));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>())
        {
            FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
            Config.Exposure.ManualExposureEV = EV;
            ColorSubsystem->SetColorConfig(Config);
        }
    }
}

void SRshipDashboardWidget::OnExposureBiasChanged(float NewValue)
{
    // Map 0 to 1 -> -4 to +4
    float Bias = (NewValue * 8.0f) - 4.0f;

    if (ExposureBiasValueText.IsValid())
    {
        ExposureBiasValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f EV"), Bias)));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>())
        {
            FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
            Config.Exposure.ExposureBias = Bias;
            ColorSubsystem->SetColorConfig(Config);
        }
    }
}

void SRshipDashboardWidget::OnHDREnabledChanged(ECheckBoxState NewState)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>())
        {
            FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
            Config.bEnableHDR = (NewState == ECheckBoxState::Checked);
            ColorSubsystem->SetColorConfig(Config);
        }
    }
}

void SRshipDashboardWidget::OnViewportSyncChanged(ECheckBoxState NewState)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>())
        {
            FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
            Config.bSyncExposureToViewport = (NewState == ECheckBoxState::Checked);
            ColorSubsystem->SetColorConfig(Config);
        }
    }
}

FReply SRshipDashboardWidget::OnApplyToViewportClicked()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>())
        {
            ColorSubsystem->ApplyToViewport();
        }
    }
    return FReply::Handled();
}

#endif // RSHIP_HAS_COLOR_MANAGEMENT

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
