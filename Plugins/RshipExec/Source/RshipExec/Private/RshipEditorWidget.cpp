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
#include "Engine/Engine.h"
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
                .ItemHeight(24)
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
                    .ItemHeight(20)
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

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
