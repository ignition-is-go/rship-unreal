// Copyright Rocketship. All Rights Reserved.

#include "SRshipStatusPanel.h"
#include "RshipStatusPanelStyle.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipSettings.h"

#if RSHIP_EDITOR_HAS_2110
#include "Rship2110.h"  // For SMPTE 2110 status
#include "Rship2110Subsystem.h"
#include "Rship2110Settings.h"
#endif

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SHeaderRow.h"
#include "EditorStyleSet.h"
#include "ISettingsModule.h"
#include "Engine/Engine.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

#define LOCTEXT_NAMESPACE "SRshipStatusPanel"

void SRshipStatusPanel::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        .Padding(8.0f)
        [
            SNew(SVerticalBox)

            // Connection Section
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                BuildConnectionSection()
            ]

            // Separator
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SSeparator)
            ]

            // Sync Timing Section
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 8.0f)
            [
                BuildSyncTimingSection()
            ]

            // Separator
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SSeparator)
            ]

            // Targets Section
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0.0f, 8.0f, 0.0f, 8.0f)
            [
                BuildTargetsSection()
            ]

            // Separator
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SSeparator)
            ]

            // Diagnostics Section
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 8.0f)
            [
                BuildDiagnosticsSection()
            ]

            // Separator
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SSeparator)
            ]

#if RSHIP_EDITOR_HAS_2110
            // SMPTE 2110 Section
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [
                Build2110Section()
            ]
#endif
        ]
    ];

    // Initial data load
    RefreshTargetList();
    UpdateConnectionStatus();
    UpdateDiagnostics();
    UpdateSyncSettings();
#if RSHIP_EDITOR_HAS_2110
    Update2110Status();
#endif
}

SRshipStatusPanel::~SRshipStatusPanel()
{
}

void SRshipStatusPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    RefreshTimer += InDeltaTime;
    if (RefreshTimer >= RefreshInterval)
    {
        RefreshTimer = 0.0f;
        UpdateConnectionStatus();
        UpdateDiagnostics();
        UpdateSyncSettings();
#if RSHIP_EDITOR_HAS_2110
        Update2110Status();
#endif
        RefreshTargetList();
    }
}

URshipSubsystem* SRshipStatusPanel::GetSubsystem() const
{
    if (GEngine)
    {
        return GEngine->GetEngineSubsystem<URshipSubsystem>();
    }
    return nullptr;
}

TSharedRef<SWidget> SRshipStatusPanel::BuildConnectionSection()
{
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    FString InitialAddress = Settings ? Settings->rshipHostAddress : TEXT("localhost");
    int32 InitialPort = Settings ? Settings->rshipServerPort : 5155;

    return SNew(SVerticalBox)

        // Header with status indicator
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SHorizontalBox)

            // Status indicator (colored circle)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(StatusIndicator, SImage)
                .Image(FRshipStatusPanelStyle::Get().GetBrush("Rship.Status.Disconnected"))
            ]

            // Title
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ConnectionTitle", "Connection"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNullWidget::NullWidget
            ]

            // Status text
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(ConnectionStatusText, STextBlock)
                .Text(LOCTEXT("StatusDisconnected", "Disconnected"))
            ]
        ]

        // Server address row
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ServerLabel", "Server:"))
                .MinDesiredWidth(60.0f)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SAssignNew(ServerAddressBox, SEditableTextBox)
                .Text(FText::FromString(InitialAddress))
                .HintText(LOCTEXT("ServerAddressHint", "hostname or IP"))
                .OnTextCommitted(this, &SRshipStatusPanel::OnServerAddressCommitted)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("PortSeparator", ":"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(60.0f)
                [
                    SAssignNew(ServerPortBox, SEditableTextBox)
                    .Text(FText::FromString(FString::FromInt(InitialPort)))
                    .HintText(LOCTEXT("PortHint", "port"))
                    .OnTextCommitted(this, &SRshipStatusPanel::OnServerPortCommitted)
                ]
            ]
        ]

        // Buttons row
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("ReconnectButton", "Reconnect"))
                .OnClicked(this, &SRshipStatusPanel::OnReconnectClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("SettingsButton", "Settings..."))
                .OnClicked(this, &SRshipStatusPanel::OnSettingsClicked)
            ]
        ];
}

TSharedRef<SWidget> SRshipStatusPanel::BuildTargetsSection()
{
    return SNew(SVerticalBox)

        // Header
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("TargetsTitle", "Targets"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNullWidget::NullWidget
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("RefreshButton", "Refresh"))
                .OnClicked(this, &SRshipStatusPanel::OnRefreshTargetsClicked)
            ]
        ]

        // Target list
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(4.0f)
            [
                SAssignNew(TargetListView, SListView<TSharedPtr<FRshipTargetListItem>>)
                .ListItemsSource(&TargetItems)
                .OnGenerateRow(this, &SRshipStatusPanel::GenerateTargetRow)
                .OnSelectionChanged(this, &SRshipStatusPanel::OnTargetSelectionChanged)
                .SelectionMode(ESelectionMode::Single)
                .HeaderRow(
                    SNew(SHeaderRow)
                    + SHeaderRow::Column("Status")
                    .DefaultLabel(LOCTEXT("StatusColumn", ""))
                    .FixedWidth(24.0f)
                    + SHeaderRow::Column("Name")
                    .DefaultLabel(LOCTEXT("NameColumn", "Name"))
                    .FillWidth(1.0f)
                    + SHeaderRow::Column("Type")
                    .DefaultLabel(LOCTEXT("TypeColumn", "Type"))
                    .FixedWidth(80.0f)
                    + SHeaderRow::Column("Emitters")
                    .DefaultLabel(LOCTEXT("EmittersColumn", "E"))
                    .FixedWidth(30.0f)
                    .HAlignCell(HAlign_Center)
                    + SHeaderRow::Column("Actions")
                    .DefaultLabel(LOCTEXT("ActionsColumn", "A"))
                    .FixedWidth(30.0f)
                    .HAlignCell(HAlign_Center)
                )
            ]
        ];
}

TSharedRef<SWidget> SRshipStatusPanel::BuildDiagnosticsSection()
{
    return SNew(SVerticalBox)

        // Header
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("DiagnosticsTitle", "Diagnostics"))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
        ]

        // Stats grid
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            // Left column
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 2.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("QueueLabel", "Queue: "))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(QueueLengthText, STextBlock)
                        .Text(LOCTEXT("QueueDefault", "0 msgs"))
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 2.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("MessagesLabel", "Msg/s: "))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(MessageRateText, STextBlock)
                        .Text(LOCTEXT("MessagesDefault", "0"))
                    ]
                ]
            ]

            // Right column
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 2.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("BytesLabel", "KB/s: "))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(ByteRateText, STextBlock)
                        .Text(LOCTEXT("BytesDefault", "0"))
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 2.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("DroppedLabel", "Dropped: "))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(DroppedText, STextBlock)
                        .Text(LOCTEXT("DroppedDefault", "0"))
                    ]
                ]
            ]
        ]

        // Backoff status
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SAssignNew(BackoffText, STextBlock)
            .Text(LOCTEXT("BackoffNone", ""))
            .ColorAndOpacity(FLinearColor(0.9f, 0.5f, 0.0f, 1.0f))
        ];
}

TSharedRef<SWidget> SRshipStatusPanel::BuildSyncTimingSection()
{
    const URshipSubsystem* Subsystem = GetSubsystem();
    const float InitialControlSyncRate = Subsystem ? Subsystem->GetControlSyncRateHz() : 60.0f;
    const int32 InitialLeadFrames = Subsystem ? Subsystem->GetInboundApplyLeadFrames() : 1;
#if RSHIP_EDITOR_HAS_2110
    const URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    const float InitialClusterSyncRate = Subsystem2110 ? Subsystem2110->GetClusterSyncRateHz() : 60.0f;
    const int32 InitialSubsteps = Subsystem2110 ? Subsystem2110->GetLocalRenderSubsteps() : 1;
    const int32 InitialMaxCatchupSteps = Subsystem2110 ? Subsystem2110->GetMaxSyncCatchupSteps() : 4;
    const FString ActiveDomain = Subsystem2110 ? Subsystem2110->GetActiveSyncDomainId() : FString(TEXT("default"));
    const float InitialSyncDomainRate = Subsystem2110 && !ActiveDomain.IsEmpty() ? Subsystem2110->GetSyncDomainRateHz(ActiveDomain) : InitialClusterSyncRate;
#endif

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("SyncTimingTitle", "Sync Timing"))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(STextBlock)
            .WrapTextAt(900.0f)
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f, 1.0f))
            .Text(LOCTEXT("SyncTimingSummaryHint",
                "Deterministic control sync (control + cluster rate) should remain consistent across nodes in one domain. "
                "Local render substeps increase this node's output cadence only."))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("CommonSyncPresetsLabel", "Preset (control + cluster):"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Preset30", "30"))
                .OnClicked_Lambda([this]()
                {
                    return OnApplySyncPresetClicked(30.0f);
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Preset60", "60"))
                .OnClicked_Lambda([this]()
                {
                    return OnApplySyncPresetClicked(60.0f);
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("Preset120", "120"))
                .OnClicked_Lambda([this]()
                {
                    return OnApplySyncPresetClicked(120.0f);
                })
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ControlRateLabel", "Control sync rate (Hz):"))
                .MinDesiredWidth(150.0f)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(ControlSyncRateInput, SEditableTextBox)
                .Text(FText::AsNumber(InitialControlSyncRate))
                .HintText(LOCTEXT("ControlRateHint", "e.g. 60"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("ApplyControlSyncRate", "Apply"))
                .OnClicked(this, &SRshipStatusPanel::OnApplyControlSyncRateClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(ControlSyncRateValueText, STextBlock)
                .Text(LOCTEXT("ControlRateValueLoading", "current: ..."))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("LeadFramesLabel", "Inbound lead frames:"))
                .MinDesiredWidth(150.0f)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(InboundLeadFramesInput, SEditableTextBox)
                .Text(FText::AsNumber(InitialLeadFrames))
                .HintText(LOCTEXT("LeadFramesHint", "integer >= 1"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("ApplyLeadFrames", "Apply"))
                .OnClicked(this, &SRshipStatusPanel::OnApplyInboundLeadFramesClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(InboundLeadFramesValueText, STextBlock)
                .Text(LOCTEXT("LeadFramesValueLoading", "current: ..."))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SAssignNew(SyncTimingStatusText, STextBlock)
            .Text(LOCTEXT("SyncTimingStatusInit", "Ready"))
            .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SAssignNew(SyncTimingSummaryText, STextBlock)
            .Text(LOCTEXT("SyncTimingSummaryInit", "Local output target: not available"))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 10.0f, 0.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("RolloutTitle", "Rollout & Deployment"))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("SaveTimingDefaults", "Save Timing Defaults"))
                .OnClicked(this, &SRshipStatusPanel::OnSaveTimingDefaultsClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("CopyRolloutCommands", "Copy Runtime Commands"))
                .OnClicked(this, &SRshipStatusPanel::OnCopyRolloutCommandsClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("CopyRolloutStartup", "Copy Startup Snippet"))
                .OnClicked(this, &SRshipStatusPanel::OnCopyStartupRolloutSnippetClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("CopyRolloutIni", "Copy Ini Defaults"))
                .OnClicked(this, &SRshipStatusPanel::OnCopyIniRolloutSnippetClicked)
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("RolloutCommandsHeading", "Runtime command bundle (copy + run on remote nodes):"))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f, 1.0f))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SAssignNew(RolloutCommandText, STextBlock)
            .WrapTextAt(900.0f)
            .Text(LOCTEXT("RolloutCommandsDefault", "Press \"Copy Runtime Commands\" to build a node rollout payload."))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("StartupSnippetHeading", "Startup snippet (for -ExecCmds):"))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f, 1.0f))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SAssignNew(StartupRolloutText, STextBlock)
            .WrapTextAt(900.0f)
            .Text(LOCTEXT("StartupSnippetDefault", "Press \"Copy Startup Snippet\" to build launch args."))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("IniSnippetHeading", "Ini defaults snippet:"))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f, 1.0f))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SAssignNew(IniRolloutText, STextBlock)
            .WrapTextAt(900.0f)
            .Text(LOCTEXT("IniSnippetDefault", "Press \"Copy Ini Defaults\" to generate the config text block."))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
        ]

#if RSHIP_EDITOR_HAS_2110
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("2110ClusterRateLabel", "2110 cluster rate (Hz):"))
                .MinDesiredWidth(150.0f)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(ClusterSyncRateInput, SEditableTextBox)
                .Text(FText::AsNumber(InitialClusterSyncRate))
                .HintText(LOCTEXT("2110ClusterRateHint", "e.g. 60"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Apply2110ClusterRate", "Apply"))
                .OnClicked(this, &SRshipStatusPanel::OnApplyClusterSyncRateClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(ClusterSyncRateValueText, STextBlock)
                .Text(LOCTEXT("2110ClusterRateValueLoading", "current: ..."))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("2110SubstepsLabel", "Local render substeps:"))
                .MinDesiredWidth(150.0f)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(LocalRenderSubstepsInput, SEditableTextBox)
                .Text(FText::AsNumber(InitialSubsteps))
                .HintText(LOCTEXT("2110SubstepsHint", "integer >= 1"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Apply2110Substeps", "Apply"))
                .OnClicked(this, &SRshipStatusPanel::OnApplyRenderSubstepsClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(LocalRenderSubstepsValueText, STextBlock)
                .Text(LOCTEXT("2110SubstepsValueLoading", "current: ..."))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SubstepsPresetsLabel", "Local substeps preset:"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("SubstepsPreset1", "1"))
                .OnClicked_Lambda([this]()
                {
                    return OnApplyRenderSubstepsPresetClicked(1);
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("SubstepsPreset2", "2"))
                .OnClicked_Lambda([this]()
                {
                    return OnApplyRenderSubstepsPresetClicked(2);
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("SubstepsPreset4", "4"))
                .OnClicked_Lambda([this]()
                {
                    return OnApplyRenderSubstepsPresetClicked(4);
                })
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("2110CatchupLabel", "Max catch-up steps:"))
                .MinDesiredWidth(150.0f)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(MaxSyncCatchupStepsInput, SEditableTextBox)
                .Text(FText::AsNumber(InitialMaxCatchupSteps))
                .HintText(LOCTEXT("2110CatchupHint", "integer >= 1"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Apply2110Catchup", "Apply"))
                .OnClicked(this, &SRshipStatusPanel::OnApplyCatchupStepsClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(MaxSyncCatchupStepsValueText, STextBlock)
                .Text(LOCTEXT("2110CatchupValueLoading", "current: ..."))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ActiveSyncDomainLabel", "Active sync domain:"))
                .MinDesiredWidth(150.0f)
            ]

            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SAssignNew(ActiveSyncDomainCombo, SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&SyncDomainOptions)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> InDomain)
                    {
                        return SNew(STextBlock).Text(InDomain.IsValid() ? FText::FromString(*InDomain) : FText::GetEmpty());
                    })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
                    {
                        SelectedSyncDomainOption = NewSelection;
                    })
                    .Content()
                    [
                        SNew(STextBlock)
                        .Text(this, &SRshipStatusPanel::GetActiveSyncDomainOptionText)
                    ]
                ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Apply2110Domain", "Apply"))
                .OnClicked(this, &SRshipStatusPanel::OnApplyActiveSyncDomainClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(ActiveSyncDomainValueText, STextBlock)
                .Text(LOCTEXT("2110DomainValueLoading", "current: ..."))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("DomainRateLabel", "Domain rate (Hz):"))
                .MinDesiredWidth(150.0f)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(SyncDomainRateCombo, SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&SyncDomainOptions)
                .OnGenerateWidget_Lambda([](TSharedPtr<FString> InDomain)
                {
                    return SNew(STextBlock).Text(InDomain.IsValid() ? FText::FromString(*InDomain) : FText::GetEmpty());
                })
                .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
                {
                    SelectedSyncDomainRateOption = NewSelection;
                })
                .Content()
                [
                    SNew(STextBlock)
                    .Text(this, &SRshipStatusPanel::GetSyncDomainRateOptionText)
                ]
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(SyncDomainRateInput, SEditableTextBox)
                .Text(FText::AsNumber(InitialSyncDomainRate))
                .HintText(LOCTEXT("2110DomainRateHint", "e.g. 60"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Apply2110DomainRate", "Apply"))
                .OnClicked(this, &SRshipStatusPanel::OnApplySyncDomainRateClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(SyncDomainRateValueText, STextBlock)
                .Text(LOCTEXT("2110DomainRateValueLoading", "current: ..."))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
            ]
        ]
#endif
    ;
}

void SRshipStatusPanel::RefreshTargetList()
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem || !Subsystem->TargetComponents)
    {
        TargetItems.Empty();
        if (TargetListView.IsValid())
        {
            TargetListView->RequestListRefresh();
        }
        return;
    }

    // Build new items list
    TArray<TSharedPtr<FRshipTargetListItem>> NewItems;

    for (auto& Pair : *Subsystem->TargetComponents)
    {
        URshipTargetComponent* Component = Pair.Value;
        if (Component && Component->IsValidLowLevel())
        {
            TSharedPtr<FRshipTargetListItem> Item = MakeShareable(new FRshipTargetListItem());
            Item->TargetId = Component->targetName;
            Item->DisplayName = Component->GetOwner() ? Component->GetOwner()->GetActorLabel() : Component->targetName;

            // Get type from tags if available, otherwise use "Target"
            Item->TargetType = Component->Tags.Num() > 0 ? Component->Tags[0] : TEXT("Target");
            Item->bIsOnline = true;  // If registered, it's online

            // Get counts from TargetData if available
            if (Component->TargetData)
            {
                Item->EmitterCount = Component->TargetData->GetEmitters().Num();
                Item->ActionCount = Component->TargetData->GetActions().Num();
            }

            Item->Component = Component;
            NewItems.Add(Item);
        }
    }

    // Sort by name
    NewItems.Sort([](const TSharedPtr<FRshipTargetListItem>& A, const TSharedPtr<FRshipTargetListItem>& B)
    {
        return A->DisplayName < B->DisplayName;
    });

    TargetItems = MoveTemp(NewItems);

    if (TargetListView.IsValid())
    {
        TargetListView->RequestListRefresh();
    }
}

void SRshipStatusPanel::UpdateConnectionStatus()
{
    URshipSubsystem* Subsystem = GetSubsystem();

    if (!Subsystem)
    {
        if (ConnectionStatusText.IsValid())
        {
            ConnectionStatusText->SetText(LOCTEXT("StatusNoSubsystem", "No Subsystem"));
        }
        if (StatusIndicator.IsValid())
        {
            StatusIndicator->SetImage(FRshipStatusPanelStyle::Get().GetBrush("Rship.Status.Disconnected"));
        }
        return;
    }

    bool bConnected = Subsystem->IsConnected();
    bool bBackingOff = Subsystem->IsRateLimiterBackingOff();

    FText StatusText;
    FName BrushName;

    if (bConnected)
    {
        StatusText = LOCTEXT("StatusConnected", "Connected");
        BrushName = "Rship.Status.Connected";
    }
    else if (bBackingOff)
    {
        StatusText = FText::Format(LOCTEXT("StatusBackingOffFmt", "Backing off ({0}s)"),
            FText::AsNumber(FMath::CeilToInt(Subsystem->GetBackoffRemaining())));
        BrushName = "Rship.Status.BackingOff";
    }
    else
    {
        StatusText = LOCTEXT("StatusDisconnected", "Disconnected");
        BrushName = "Rship.Status.Disconnected";
    }

    if (ConnectionStatusText.IsValid())
    {
        ConnectionStatusText->SetText(StatusText);
    }
    if (StatusIndicator.IsValid())
    {
        StatusIndicator->SetImage(FRshipStatusPanelStyle::Get().GetBrush(BrushName));
    }
}

void SRshipStatusPanel::UpdateDiagnostics()
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return;
    }

    if (QueueLengthText.IsValid())
    {
        QueueLengthText->SetText(FText::Format(LOCTEXT("QueueFmt", "{0} msgs ({1}%)"),
            FText::AsNumber(Subsystem->GetQueueLength()),
            FText::AsNumber(FMath::RoundToInt(Subsystem->GetQueuePressure() * 100.0f))));
    }

    if (MessageRateText.IsValid())
    {
        MessageRateText->SetText(FText::AsNumber(Subsystem->GetMessagesSentPerSecond()));
    }

    if (ByteRateText.IsValid())
    {
        float KBps = Subsystem->GetBytesSentPerSecond() / 1024.0f;
        ByteRateText->SetText(FText::Format(LOCTEXT("KBpsFmt", "{0}"), FText::AsNumber(FMath::RoundToInt(KBps))));
    }

    if (DroppedText.IsValid())
    {
        DroppedText->SetText(FText::AsNumber(Subsystem->GetMessagesDropped()));
    }

    if (BackoffText.IsValid())
    {
        if (Subsystem->IsRateLimiterBackingOff())
        {
            BackoffText->SetText(FText::Format(LOCTEXT("BackoffFmt", "Rate limited - backing off {0}s"),
                FText::AsNumber(FMath::CeilToInt(Subsystem->GetBackoffRemaining()))));
        }
        else
        {
            BackoffText->SetText(FText::GetEmpty());
        }
    }
}

void SRshipStatusPanel::UpdateSyncSettings()
{
    URshipSubsystem* MainSubsystem = GetSubsystem();

    if (ControlSyncRateValueText.IsValid())
    {
        if (MainSubsystem)
        {
            ControlSyncRateValueText->SetText(FText::Format(
                LOCTEXT("ControlSyncRateValueFmt", "current: {0} Hz"),
                FText::AsNumber(MainSubsystem->GetControlSyncRateHz())));
            if (!ControlSyncRateInput.IsValid() || ControlSyncRateInput->GetText().IsEmpty())
            {
                ControlSyncRateInput->SetText(FText::AsNumber(MainSubsystem->GetControlSyncRateHz()));
            }
        }
        else
        {
            ControlSyncRateValueText->SetText(LOCTEXT("ControlSyncUnavailable", "current: n/a"));
        }
    }

    if (InboundLeadFramesValueText.IsValid())
    {
        if (URshipSubsystem* Subsystem = GetSubsystem())
        {
            InboundLeadFramesValueText->SetText(FText::Format(
                LOCTEXT("LeadFramesValueFmt", "current: {0}"),
                FText::AsNumber(Subsystem->GetInboundApplyLeadFrames())));
        }
        else
        {
            InboundLeadFramesValueText->SetText(LOCTEXT("LeadFramesUnavailable", "current: n/a"));
        }
    }

#if RSHIP_EDITOR_HAS_2110
    URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    const bool b2110Available = FRship2110Module::IsAvailable();
    const float ClusterSyncRate = Subsystem2110 && b2110Available ? Subsystem2110->GetClusterSyncRateHz() : 0.0f;
    const int32 LocalSubsteps = Subsystem2110 && b2110Available ? FMath::Max(1, Subsystem2110->GetLocalRenderSubsteps()) : 0;
    const float LocalOutputRate = ClusterSyncRate * static_cast<float>(LocalSubsteps);
    const bool bRatesAligned = MainSubsystem && b2110Available &&
        FMath::IsNearlyEqual(MainSubsystem->GetControlSyncRateHz(), ClusterSyncRate, 0.001f);

    if (ClusterSyncRateValueText.IsValid())
    {
        if (b2110Available && Subsystem2110)
        {
            ClusterSyncRateValueText->SetText(FText::Format(
                LOCTEXT("2110ClusterSyncValueFmt", "current: {0} Hz"),
                FText::AsNumber(Subsystem2110->GetClusterSyncRateHz())));
            if (ClusterSyncRateInput.IsValid() && ClusterSyncRateInput->GetText().IsEmpty())
            {
                ClusterSyncRateInput->SetText(FText::AsNumber(Subsystem2110->GetClusterSyncRateHz()));
            }
        }
        else
        {
            ClusterSyncRateValueText->SetText(LOCTEXT("2110ValueUnavailable", "current: n/a"));
        }
    }

    if (LocalRenderSubstepsValueText.IsValid())
    {
        if (b2110Available && Subsystem2110)
        {
            LocalRenderSubstepsValueText->SetText(FText::Format(
                LOCTEXT("2110SubstepsValueFmt", "current: {0}"),
                FText::AsNumber(Subsystem2110->GetLocalRenderSubsteps())));
            if (LocalRenderSubstepsInput.IsValid() && LocalRenderSubstepsInput->GetText().IsEmpty())
            {
                LocalRenderSubstepsInput->SetText(FText::AsNumber(Subsystem2110->GetLocalRenderSubsteps()));
            }
        }
        else
        {
            LocalRenderSubstepsValueText->SetText(LOCTEXT("2110SubstepsUnavailable", "current: n/a"));
        }
    }

    if (MaxSyncCatchupStepsValueText.IsValid())
    {
        if (b2110Available && Subsystem2110)
        {
            MaxSyncCatchupStepsValueText->SetText(FText::Format(
                LOCTEXT("2110CatchupValueFmt", "current: {0}"),
                FText::AsNumber(Subsystem2110->GetMaxSyncCatchupSteps())));
            if (MaxSyncCatchupStepsInput.IsValid() && MaxSyncCatchupStepsInput->GetText().IsEmpty())
            {
                MaxSyncCatchupStepsInput->SetText(FText::AsNumber(Subsystem2110->GetMaxSyncCatchupSteps()));
            }
        }
        else
        {
            MaxSyncCatchupStepsValueText->SetText(LOCTEXT("2110CatchupUnavailable", "current: n/a"));
        }
    }

    if (ActiveSyncDomainValueText.IsValid())
    {
        if (b2110Available && Subsystem2110)
        {
            ActiveSyncDomainValueText->SetText(FText::Format(
                LOCTEXT("2110ActiveDomainValueFmt", "current: {0}"),
                FText::FromString(Subsystem2110->GetActiveSyncDomainId())));
        }
        else
        {
            ActiveSyncDomainValueText->SetText(LOCTEXT("2110ActiveDomainUnavailable", "current: n/a"));
        }
    }

    if (SyncDomainRateValueText.IsValid())
    {
        const FString TargetDomainId = GetDisplaySyncDomainId(SelectedSyncDomainRateOption);
        if (b2110Available && Subsystem2110 && !TargetDomainId.IsEmpty())
        {
            const float TargetRate = Subsystem2110->GetSyncDomainRateHz(TargetDomainId);
            if (TargetRate > 0.0f)
            {
                SyncDomainRateValueText->SetText(FText::Format(
                    LOCTEXT("2110DomainRateValueFmt", "current: {0} Hz"),
                    FText::AsNumber(TargetRate)));

                if (SyncDomainRateInput.IsValid() && SyncDomainRateInput->GetText().IsEmpty())
                {
                    SyncDomainRateInput->SetText(FText::AsNumber(TargetRate));
                }
            }
            else
            {
                SyncDomainRateValueText->SetText(LOCTEXT("2110DomainRateUnavailable", "current: n/a"));
            }
        }
        else
        {
            SyncDomainRateValueText->SetText(LOCTEXT("2110DomainRateUnavailable", "current: n/a"));
        }
    }

    if (SyncTimingSummaryText.IsValid())
    {
#if RSHIP_EDITOR_HAS_2110
        if (MainSubsystem && b2110Available && Subsystem2110)
        {
            if (bRatesAligned)
            {
                SyncTimingSummaryText->SetText(FText::Format(
                    LOCTEXT("SyncTimingSummaryAlignedFmt",
                        "Deterministic timeline: {0} Hz (control + cluster), local output budget: {1} Hz ({2}x from {3} substeps)."),
                    FText::AsNumber(MainSubsystem->GetControlSyncRateHz()),
                    FText::AsNumber(LocalOutputRate),
                    FText::AsNumber(LocalSubsteps),
                    FText::AsNumber(ClusterSyncRate)));
                SyncTimingSummaryText->SetColorAndOpacity(FLinearColor(0.75f, 0.98f, 0.75f, 1.0f));
            }
            else
            {
                SyncTimingSummaryText->SetText(FText::Format(
                    LOCTEXT("SyncTimingSummaryMismatchFmt",
                        "Warning: control={0} Hz, cluster={1} Hz. Keep both equal for deterministic sync across nodes; per-node local substeps adjust output only."),
                    FText::AsNumber(MainSubsystem->GetControlSyncRateHz()),
                    FText::AsNumber(ClusterSyncRate)));
                SyncTimingSummaryText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.35f, 1.0f));
            }
        }
        else if (MainSubsystem)
        {
            SyncTimingSummaryText->SetText(FText::Format(
                LOCTEXT("SyncTimingSummaryControlOnlyFmt", "Deterministic control timing: {0} Hz. SMPTE 2110 not available for local output budget."),
                FText::AsNumber(MainSubsystem->GetControlSyncRateHz())));
            SyncTimingSummaryText->SetColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));
        }
        else
        {
            SyncTimingSummaryText->SetText(LOCTEXT("SyncTimingSummaryUnavailable", "Timing summary: subsystem unavailable"));
            SyncTimingSummaryText->SetColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f));
        }
#else
        if (MainSubsystem)
        {
            SyncTimingSummaryText->SetText(FText::Format(
                LOCTEXT("SyncTimingSummaryControlOnlyNo2110Fmt", "Control timing: {0} Hz. SMPTE 2110 controls are disabled in this build."),
                FText::AsNumber(MainSubsystem->GetControlSyncRateHz())));
            SyncTimingSummaryText->SetColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));
        }
        else
        {
            SyncTimingSummaryText->SetText(LOCTEXT("SyncTimingSummaryUnavailable", "Timing summary: subsystem unavailable"));
            SyncTimingSummaryText->SetColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f));
        }
#endif
    }

    UpdateSyncDomainOptions(Subsystem2110);
#endif

    UpdateRolloutPreviews();
}

void SRshipStatusPanel::SetSyncTimingStatus(const FText& Message, const FLinearColor& Color)
{
    if (SyncTimingStatusText.IsValid())
    {
        SyncTimingStatusText->SetText(Message);
        SyncTimingStatusText->SetColorAndOpacity(Color);
    }
}

FString SRshipStatusPanel::QuoteConsoleArgument(const FString& InArgument) const
{
    FString Escaped = InArgument.Replace(TEXT("\""), TEXT("\\\""));
    const bool bNeedsQuoting = Escaped.Contains(TEXT(" "))
        || Escaped.Contains(TEXT("\t"))
        || Escaped.Contains(TEXT(";"));
    return bNeedsQuoting ? FString::Printf(TEXT("\"%s\""), *Escaped) : Escaped;
}

FString SRshipStatusPanel::BuildRolloutCommandBundle() const
{
    const URshipSubsystem* MainSubsystem = GetSubsystem();
    if (!MainSubsystem)
    {
        return TEXT("echo Rship subsystem unavailable");
    }

    TArray<FString> Commands;
    Commands.Add(FString::Printf(TEXT("rship.sync.rate %s"), *FString::SanitizeFloat(MainSubsystem->GetControlSyncRateHz(), 2)));
    Commands.Add(FString::Printf(TEXT("rship.sync.lead %d"), MainSubsystem->GetInboundApplyLeadFrames()));

#if RSHIP_EDITOR_HAS_2110
    URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    if (Subsystem2110 && FRship2110Module::IsAvailable())
    {
        Commands.Add(FString::Printf(TEXT("rship.cluster.timing.sync %s"), *FString::SanitizeFloat(Subsystem2110->GetClusterSyncRateHz(), 2)));
        Commands.Add(FString::Printf(TEXT("rship.cluster.timing.substeps %d"), FMath::Max(1, Subsystem2110->GetLocalRenderSubsteps()));
        Commands.Add(FString::Printf(TEXT("rship.cluster.timing.catchup %d"), FMath::Max(1, Subsystem2110->GetMaxSyncCatchupSteps()));

        const FString ActiveDomain = Subsystem2110->GetActiveSyncDomainId();
        if (!ActiveDomain.IsEmpty())
        {
            Commands.Add(FString::Printf(TEXT("rship.cluster.domain.active %s"), *QuoteConsoleArgument(ActiveDomain)));
        }

        TSet<FString> AddedDomainRates;
        TArray<FString> DomainIds = Subsystem2110->GetSyncDomainIds();
        if (DomainIds.Num() == 0 && !ActiveDomain.IsEmpty())
        {
            DomainIds.Add(ActiveDomain);
        }
        DomainIds.Sort();
        for (const FString& DomainId : DomainIds)
        {
            if (DomainId.IsEmpty() || AddedDomainRates.Contains(DomainId))
            {
                continue;
            }
            AddedDomainRates.Add(DomainId);
            const float DomainRate = Subsystem2110->GetSyncDomainRateHz(DomainId);
            if (DomainRate > 0.0f)
            {
                Commands.Add(FString::Printf(
                    TEXT("rship.cluster.domain.rate %s %s"),
                    *QuoteConsoleArgument(DomainId),
                    *FString::SanitizeFloat(DomainRate, 2)));
            }
        }
    }
#endif

    return FString::Join(Commands, TEXT("\n"));
}

FString SRshipStatusPanel::BuildStartupRolloutSnippet() const
{
    const FString RawCommands = BuildRolloutCommandBundle();
    TArray<FString> CommandLines;
    RawCommands.ParseIntoArrayLines(CommandLines);

    TArray<FString> InlineCommands;
    for (FString& CommandLine : CommandLines)
    {
        const FString TrimmedLine = CommandLine.TrimStartAndEnd();
        if (!TrimmedLine.IsEmpty())
        {
            InlineCommands.Add(TrimmedLine);
        }
    }
    if (InlineCommands.Num() == 0)
    {
        return TEXT("(-ExecCmds flag skipped; no commands)");
    }

    const FString InlineBundle = FString::Join(InlineCommands, TEXT("; "));
    return FString::Printf(TEXT("-ExecCmds=\"%s\""), *InlineBundle.Replace(TEXT("\""), TEXT("\\\"")));
}

FString SRshipStatusPanel::BuildTimingIniSnippet() const
{
    const URshipSubsystem* MainSubsystem = GetSubsystem();
    if (!MainSubsystem)
    {
        return TEXT("[/Script/RshipExec.URshipSettings]\nControlSyncRateHz=60.0\nInboundApplyLeadFrames=1");
    }

    TArray<FString> Lines;
    Lines.Add(TEXT("[/Script/RshipExec.URshipSettings]"));
    Lines.Add(FString::Printf(TEXT("ControlSyncRateHz=%s"), *FString::SanitizeFloat(MainSubsystem->GetControlSyncRateHz(), 2)));
    Lines.Add(FString::Printf(TEXT("InboundApplyLeadFrames=%d"), MainSubsystem->GetInboundApplyLeadFrames()));

#if RSHIP_EDITOR_HAS_2110
    if (FRship2110Module::IsAvailable())
    {
        const URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (Subsystem2110)
        {
            Lines.Add(TEXT(""));
            Lines.Add(TEXT("[/Script/Rship2110.URship2110Settings]"));
            Lines.Add(FString::Printf(TEXT("ClusterSyncRateHz=%s"), *FString::SanitizeFloat(Subsystem2110->GetClusterSyncRateHz(), 2)));
            Lines.Add(FString::Printf(TEXT("LocalRenderSubsteps=%d"), FMath::Max(1, Subsystem2110->GetLocalRenderSubsteps()));
            Lines.Add(FString::Printf(TEXT("MaxSyncCatchupSteps=%d"), FMath::Max(1, Subsystem2110->GetMaxSyncCatchupSteps()));
        }
    }
#endif

    return FString::Join(Lines, TEXT("\n"));
}

void SRshipStatusPanel::UpdateRolloutPreviews()
{
    if (RolloutCommandText.IsValid())
    {
        RolloutCommandText->SetText(FText::FromString(BuildRolloutCommandBundle()));
    }

    if (StartupRolloutText.IsValid())
    {
        StartupRolloutText->SetText(FText::FromString(BuildStartupRolloutSnippet()));
    }

    if (IniRolloutText.IsValid())
    {
        IniRolloutText->SetText(FText::FromString(BuildTimingIniSnippet()));
    }
}

FReply SRshipStatusPanel::OnCopyRolloutCommandsClicked()
{
    const FString Commands = BuildRolloutCommandBundle();
    FPlatformApplicationMisc::ClipboardCopy(*Commands);
    SetSyncTimingStatus(LOCTEXT("RolloutCommandsCopied", "Runtime rollout command bundle copied to clipboard."), FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
    UpdateRolloutPreviews();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnCopyStartupRolloutSnippetClicked()
{
    const FString Snippet = BuildStartupRolloutSnippet();
    FPlatformApplicationMisc::ClipboardCopy(*Snippet);
    SetSyncTimingStatus(LOCTEXT("RolloutStartupSnippetCopied", "Startup -ExecCmds snippet copied to clipboard."), FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
    UpdateRolloutPreviews();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnCopyIniRolloutSnippetClicked()
{
    const FString Snippet = BuildTimingIniSnippet();
    FPlatformApplicationMisc::ClipboardCopy(*Snippet);
    SetSyncTimingStatus(LOCTEXT("RolloutIniSnippetCopied", "Ini defaults snippet copied to clipboard."), FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
    UpdateRolloutPreviews();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnSaveTimingDefaultsClicked()
{
    URshipSubsystem* MainSubsystem = GetSubsystem();
    URshipSettings* Settings = GetMutableDefault<URshipSettings>();
    if (!MainSubsystem || !Settings)
    {
        SetSyncTimingStatus(LOCTEXT("SaveTimingDefaultsUnavailable", "Cannot save defaults: Rship subsystem/settings unavailable."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
        UpdateRolloutPreviews();
        return FReply::Handled();
    }

    bool bInvalidInput = false;
    if (ControlSyncRateInput.IsValid())
    {
        const FString ValueText = ControlSyncRateInput->GetText().ToString();
        float Value = 0.0f;
        if (!ValueText.IsEmpty())
        {
            if (ParsePositiveFloatInput(ValueText, Value))
            {
                MainSubsystem->SetControlSyncRateHz(Value);
            }
            else
            {
                bInvalidInput = true;
            }
        }
    }

    if (InboundLeadFramesInput.IsValid())
    {
        const FString ValueText = InboundLeadFramesInput->GetText().ToString();
        int32 Value = 0;
        if (!ValueText.IsEmpty())
        {
            if (ParsePositiveIntInput(ValueText, Value))
            {
                MainSubsystem->SetInboundApplyLeadFrames(Value);
            }
            else
            {
                bInvalidInput = true;
            }
        }
    }

    Settings->ControlSyncRateHz = FMath::Max(1.0f, MainSubsystem->GetControlSyncRateHz());
    Settings->InboundApplyLeadFrames = FMath::Max(1, MainSubsystem->GetInboundApplyLeadFrames());
    Settings->SaveConfig();

#if RSHIP_EDITOR_HAS_2110
    if (FRship2110Module::IsAvailable())
    {
        URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        URship2110Settings* Settings2110 = URship2110Settings::Get();
        if (Subsystem2110 && Settings2110)
        {
            if (ClusterSyncRateInput.IsValid())
            {
                const FString ValueText = ClusterSyncRateInput->GetText().ToString();
                float Value = 0.0f;
                if (!ValueText.IsEmpty())
                {
                    if (ParsePositiveFloatInput(ValueText, Value))
                    {
                        Subsystem2110->SetClusterSyncRateHz(Value);
                    }
                    else
                    {
                        bInvalidInput = true;
                    }
                }
            }

            if (LocalRenderSubstepsInput.IsValid())
            {
                const FString ValueText = LocalRenderSubstepsInput->GetText().ToString();
                int32 Value = 0;
                if (!ValueText.IsEmpty())
                {
                    if (ParsePositiveIntInput(ValueText, Value))
                    {
                        Subsystem2110->SetLocalRenderSubsteps(Value);
                    }
                    else
                    {
                        bInvalidInput = true;
                    }
                }
            }

            if (MaxSyncCatchupStepsInput.IsValid())
            {
                const FString ValueText = MaxSyncCatchupStepsInput->GetText().ToString();
                int32 Value = 0;
                if (!ValueText.IsEmpty())
                {
                    if (ParsePositiveIntInput(ValueText, Value))
                    {
                        Subsystem2110->SetMaxSyncCatchupSteps(Value);
                    }
                    else
                    {
                        bInvalidInput = true;
                    }
                }
            }

            Settings2110->ClusterSyncRateHz = FMath::Max(1.0f, Subsystem2110->GetClusterSyncRateHz());
            Settings2110->LocalRenderSubsteps = FMath::Max(1, Subsystem2110->GetLocalRenderSubsteps());
            Settings2110->MaxSyncCatchupSteps = FMath::Max(1, Subsystem2110->GetMaxSyncCatchupSteps());
            Settings2110->SaveConfig();
        }
        else if (Settings2110)
        {
            Settings2110->ClusterSyncRateHz = MainSubsystem->GetControlSyncRateHz();
            Settings2110->LocalRenderSubsteps = 1;
            Settings2110->MaxSyncCatchupSteps = 4;
            Settings2110->SaveConfig();
        }
    }
#endif

    UpdateSyncSettings();
    UpdateRolloutPreviews();

    if (bInvalidInput)
    {
        SetSyncTimingStatus(LOCTEXT("SaveTimingDefaultsInvalid", "Saved timing defaults, but some entered values were invalid."), FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
    }
    else
    {
        SetSyncTimingStatus(LOCTEXT("SaveTimingDefaultsSuccess", "Timing defaults saved to project config."), FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
    }

    return FReply::Handled();
}

bool SRshipStatusPanel::ParsePositiveFloatInput(const FString& Input, float& OutValue) const
{
    FString CleanInput = Input;
    CleanInput.TrimStartAndEndInline();
    if (!CleanInput.IsNumeric())
    {
        return false;
    }

    OutValue = FCString::Atof(*CleanInput);
    return FMath::IsFinite(OutValue) && OutValue > 0.0f;
}

bool SRshipStatusPanel::ParsePositiveIntInput(const FString& Input, int32& OutValue) const
{
    FString CleanInput = Input;
    CleanInput.TrimStartAndEndInline();
    if (!CleanInput.IsNumeric())
    {
        return false;
    }

    OutValue = FCString::Atoi(*CleanInput);
    return OutValue > 0;
}

FReply SRshipStatusPanel::OnApplyControlSyncRateClicked()
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (Subsystem && ControlSyncRateInput.IsValid())
    {
        float Value = 0.0f;
        if (ParsePositiveFloatInput(ControlSyncRateInput->GetText().ToString(), Value))
        {
            Subsystem->SetControlSyncRateHz(Value);
#if RSHIP_EDITOR_HAS_2110
            if (FRship2110Module::IsAvailable())
            {
                if (URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr)
                {
                    Subsystem2110->SetClusterSyncRateHz(Value);
                }
            }
#endif
            SetSyncTimingStatus(
                FText::Format(
                    LOCTEXT("SyncTimingStatusControlUpdated", "Control sync updated to {0} Hz."),
                    FText::AsNumber(Value)),
                FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
            UpdateSyncSettings();
            return FReply::Handled();
        }
    }

    SetSyncTimingStatus(LOCTEXT("SyncTimingStatusControlInvalid", "Invalid control sync value. Enter a positive number."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));

    UpdateSyncSettings();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnApplyInboundLeadFramesClicked()
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (Subsystem && InboundLeadFramesInput.IsValid())
    {
        int32 Value = 0;
        if (ParsePositiveIntInput(InboundLeadFramesInput->GetText().ToString(), Value))
        {
            Subsystem->SetInboundApplyLeadFrames(Value);
            SetSyncTimingStatus(
                FText::Format(
                    LOCTEXT("SyncTimingStatusLeadUpdated", "Inbound lead frames updated to {0}."),
                    FText::AsNumber(Value)),
                FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
            UpdateSyncSettings();
            return FReply::Handled();
        }
    }

    SetSyncTimingStatus(LOCTEXT("SyncTimingStatusLeadInvalid", "Invalid inbound lead value. Enter an integer >= 1."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
    UpdateSyncSettings();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnApplySyncPresetClicked(float PresetHz)
{
    if (!FMath::IsFinite(PresetHz) || PresetHz <= 0.0f)
    {
        SetSyncTimingStatus(LOCTEXT("SyncTimingStatusPresetInvalid", "Preset sync rate is invalid."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
        UpdateSyncSettings();
        return FReply::Handled();
    }

    URshipSubsystem* Subsystem = GetSubsystem();
    bool bControlUpdated = false;
    bool bClusterUpdated = false;

    if (Subsystem)
    {
        Subsystem->SetControlSyncRateHz(PresetHz);
        bControlUpdated = true;
        if (ControlSyncRateInput.IsValid())
        {
            ControlSyncRateInput->SetText(FText::AsNumber(PresetHz));
        }
    }

#if RSHIP_EDITOR_HAS_2110
    if (FRship2110Module::IsAvailable())
    {
        URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (Subsystem2110)
        {
            Subsystem2110->SetClusterSyncRateHz(PresetHz);
            bClusterUpdated = true;
            if (ClusterSyncRateInput.IsValid())
            {
                ClusterSyncRateInput->SetText(FText::AsNumber(PresetHz));
            }
        }
    }
#endif

    if (bControlUpdated)
    {
        const FText Message = bClusterUpdated
            ? FText::Format(LOCTEXT("SyncTimingStatusPresetBothUpdated", "Preset applied: control + cluster sync set to {0} Hz."), FText::AsNumber(PresetHz))
            : LOCTEXT("SyncTimingStatusPresetControlUpdated", "Preset applied to control sync only (SMPTE 2110 controls unavailable).");
        SetSyncTimingStatus(Message, FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
    }
    else
    {
        SetSyncTimingStatus(LOCTEXT("SyncTimingStatusPresetUnavailable", "Sync rate preset not applied: no subsystem available."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
    }

    UpdateSyncSettings();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnApplyRenderSubstepsPresetClicked(int32 PresetSubsteps)
{
    if (PresetSubsteps <= 0)
    {
        SetSyncTimingStatus(LOCTEXT("SyncTimingStatusSubstepsInvalid", "Substeps preset is invalid."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
        return FReply::Handled();
    }

#if RSHIP_EDITOR_HAS_2110
    if (!FRship2110Module::IsAvailable())
    {
        SetSyncTimingStatus(LOCTEXT("SyncTimingStatusSubstepsNoModule", "SMPTE 2110 is not available."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
        return FReply::Handled();
    }

    URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    if (!Subsystem2110)
    {
        SetSyncTimingStatus(LOCTEXT("SyncTimingStatusSubstepsUnavailable", "SMPTE 2110 timing not available on this node."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
        return FReply::Handled();
    }

    Subsystem2110->SetLocalRenderSubsteps(PresetSubsteps);
    if (LocalRenderSubstepsInput.IsValid())
    {
        LocalRenderSubstepsInput->SetText(FText::AsNumber(PresetSubsteps));
    }
    SetSyncTimingStatus(
        FText::Format(
            LOCTEXT("SyncTimingStatusSubstepsUpdated", "Local substeps preset applied: {0}."), FText::AsNumber(PresetSubsteps)),
        FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
    UpdateSyncSettings();
    return FReply::Handled();
#else
    SetSyncTimingStatus(LOCTEXT("SyncTimingStatusSubstepsUnavailable", "SMPTE 2110 controls are not enabled for this build."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
    return FReply::Handled();
#endif
}

#if RSHIP_EDITOR_HAS_2110
void SRshipStatusPanel::UpdateSyncDomainOptions(const URship2110Subsystem* Subsystem)
{
    SyncDomainOptions.Empty();

    FString ActiveDomain = Subsystem ? Subsystem->GetActiveSyncDomainId() : TEXT("");
    TArray<FString> DomainIds;
    if (Subsystem)
    {
        DomainIds = Subsystem->GetSyncDomainIds();
    }

    if (!ActiveDomain.IsEmpty())
    {
        bool bFoundActive = false;
        for (const FString& DomainId : DomainIds)
        {
            if (DomainId.Equals(ActiveDomain, ESearchCase::IgnoreCase))
            {
                bFoundActive = true;
                break;
            }
        }
        if (!bFoundActive)
        {
            DomainIds.Add(ActiveDomain);
        }
    }

    for (const FString& DomainId : DomainIds)
    {
        SyncDomainOptions.Add(MakeShareable(new FString(DomainId)));
    }

    TSharedPtr<FString> MatchingOption;
    if (Subsystem)
    {
        for (auto& DomainOption : SyncDomainOptions)
        {
            if (DomainOption.IsValid() && DomainOption->Equals(Subsystem->GetActiveSyncDomainId(), ESearchCase::IgnoreCase))
            {
                MatchingOption = DomainOption;
                break;
            }
        }
    }
    if (!MatchingOption.IsValid() && SyncDomainOptions.Num() > 0)
    {
        MatchingOption = SyncDomainOptions[0];
    }
    SelectedSyncDomainOption = MatchingOption;

    if (ActiveSyncDomainCombo.IsValid())
    {
        ActiveSyncDomainCombo->RefreshOptions();
        if (MatchingOption.IsValid())
        {
            ActiveSyncDomainCombo->SetSelectedItem(MatchingOption);
        }
        else
        {
            ActiveSyncDomainCombo->ClearSelection();
        }
    }

    TSharedPtr<FString> RateMatchingOption;
    const FString CurrentRateDomain = GetDisplaySyncDomainId(SelectedSyncDomainRateOption);
    for (auto& DomainOption : SyncDomainOptions)
    {
        if (DomainOption.IsValid() && DomainOption->Equals(CurrentRateDomain, ESearchCase::IgnoreCase))
        {
            RateMatchingOption = DomainOption;
            break;
        }
    }

    if (!RateMatchingOption.IsValid() && SyncDomainOptions.Num() > 0)
    {
        RateMatchingOption = SyncDomainOptions[0];
    }
    SelectedSyncDomainRateOption = RateMatchingOption;

    if (SyncDomainRateCombo.IsValid())
    {
        SyncDomainRateCombo->RefreshOptions();
        if (RateMatchingOption.IsValid())
        {
            SyncDomainRateCombo->SetSelectedItem(RateMatchingOption);
        }
        else
        {
            SyncDomainRateCombo->ClearSelection();
        }
    }
}

FText SRshipStatusPanel::GetActiveSyncDomainOptionText() const
{
    if (SelectedSyncDomainOption.IsValid())
    {
        return FText::FromString(*SelectedSyncDomainOption);
    }

    if (ActiveSyncDomainCombo.IsValid())
    {
        const TSharedPtr<FString> Selected = ActiveSyncDomainCombo->GetSelectedItem();
        if (Selected.IsValid())
        {
            return FText::FromString(*Selected);
        }
    }

    return LOCTEXT("NoSyncDomainOption", "(none)");
}

FString SRshipStatusPanel::GetDisplaySyncDomainId(const TSharedPtr<FString>& Selection) const
{
    if (Selection.IsValid())
    {
        return *Selection;
    }

    if (URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr)
    {
        return Subsystem->GetActiveSyncDomainId();
    }

    return FString();
}

FText SRshipStatusPanel::GetSyncDomainRateOptionText() const
{
    if (SelectedSyncDomainRateOption.IsValid())
    {
        return FText::FromString(*SelectedSyncDomainRateOption);
    }

    if (SyncDomainRateCombo.IsValid())
    {
        const TSharedPtr<FString> Selected = SyncDomainRateCombo->GetSelectedItem();
        if (Selected.IsValid())
        {
            return FText::FromString(*Selected);
        }
    }

    return LOCTEXT("NoSyncDomainRateOption", "(none)");
}

FReply SRshipStatusPanel::OnApplyClusterSyncRateClicked()
{
    URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    if (Subsystem2110 && ClusterSyncRateInput.IsValid())
    {
        float Value = 0.0f;
        if (ParsePositiveFloatInput(ClusterSyncRateInput->GetText().ToString(), Value))
        {
            Subsystem2110->SetClusterSyncRateHz(Value);
            SetSyncTimingStatus(
                FText::Format(LOCTEXT("SyncTimingStatusClusterRateUpdated", "Cluster sync updated to {0} Hz."), FText::AsNumber(Value)),
                FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
            UpdateSyncSettings();
            return FReply::Handled();
        }
    }

    SetSyncTimingStatus(LOCTEXT("SyncTimingStatusClusterRateInvalid", "Invalid cluster sync rate. Enter a positive number."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
    UpdateSyncSettings();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnApplyRenderSubstepsClicked()
{
    URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    if (Subsystem2110 && LocalRenderSubstepsInput.IsValid())
    {
        int32 Value = 0;
        if (ParsePositiveIntInput(LocalRenderSubstepsInput->GetText().ToString(), Value))
        {
            Subsystem2110->SetLocalRenderSubsteps(Value);
            SetSyncTimingStatus(
                FText::Format(LOCTEXT("SyncTimingStatusSubstepsValueUpdated", "Local substeps updated to {0}."), FText::AsNumber(Value)),
                FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
            UpdateSyncSettings();
            return FReply::Handled();
        }
    }

    SetSyncTimingStatus(LOCTEXT("SyncTimingStatusSubstepsValueInvalid", "Invalid local substeps value. Enter an integer >= 1."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
    UpdateSyncSettings();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnApplyCatchupStepsClicked()
{
    URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    if (Subsystem2110 && MaxSyncCatchupStepsInput.IsValid())
    {
        int32 Value = 0;
        if (ParsePositiveIntInput(MaxSyncCatchupStepsInput->GetText().ToString(), Value))
        {
            Subsystem2110->SetMaxSyncCatchupSteps(Value);
            SetSyncTimingStatus(
                FText::Format(LOCTEXT("SyncTimingStatusCatchupUpdated", "Max catch-up steps updated to {0}."), FText::AsNumber(Value)),
                FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
            UpdateSyncSettings();
            return FReply::Handled();
        }
    }

    SetSyncTimingStatus(LOCTEXT("SyncTimingStatusCatchupInvalid", "Invalid catch-up value. Enter an integer >= 1."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
    UpdateSyncSettings();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnApplyActiveSyncDomainClicked()
{
    URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    if (Subsystem2110 && SelectedSyncDomainOption.IsValid())
    {
        Subsystem2110->SetActiveSyncDomainId(*SelectedSyncDomainOption);
        SetSyncTimingStatus(
            FText::Format(LOCTEXT("SyncTimingStatusActiveDomainUpdated", "Active sync domain set to {0}."), FText::FromString(*SelectedSyncDomainOption)),
            FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
        UpdateSyncSettings();
        return FReply::Handled();
    }

    SetSyncTimingStatus(LOCTEXT("SyncTimingStatusActiveDomainInvalid", "No sync domain selected."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
    UpdateSyncSettings();
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnApplySyncDomainRateClicked()
{
    URship2110Subsystem* Subsystem2110 = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
    const FString DomainId = GetDisplaySyncDomainId(SelectedSyncDomainRateOption);
    if (Subsystem2110 && SyncDomainRateInput.IsValid() && !DomainId.IsEmpty())
    {
        float Value = 0.0f;
        if (ParsePositiveFloatInput(SyncDomainRateInput->GetText().ToString(), Value))
        {
            Subsystem2110->SetSyncDomainRateHz(DomainId, Value);
            SetSyncTimingStatus(
                FText::Format(
                    LOCTEXT("SyncTimingStatusDomainRateUpdated", "Sync domain {0} rate set to {1} Hz."),
                    FText::FromString(DomainId),
                    FText::AsNumber(Value)),
                FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
            UpdateSyncSettings();
            return FReply::Handled();
        }
    }

    SetSyncTimingStatus(LOCTEXT("SyncTimingStatusDomainRateInvalid", "Invalid domain selection or rate value."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
    UpdateSyncSettings();
    return FReply::Handled();
}
#endif

FReply SRshipStatusPanel::OnReconnectClicked()
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (Subsystem)
    {
        // Get address from text boxes
        FString Address = ServerAddressBox.IsValid() ? ServerAddressBox->GetText().ToString() : TEXT("");
        int32 Port = ServerPortBox.IsValid() ? FCString::Atoi(*ServerPortBox->GetText().ToString()) : 5155;

        if (Port <= 0 || Port > 65535)
        {
            Port = 5155;
        }

        Subsystem->ConnectTo(Address, Port);
    }
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnSettingsClicked()
{
    // Open project settings to Rocketship section
    ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
    if (SettingsModule)
    {
        SettingsModule->ShowViewer("Project", "Game", "Rocketship Settings");
    }
    return FReply::Handled();
}

FReply SRshipStatusPanel::OnRefreshTargetsClicked()
{
    RefreshTargetList();
    return FReply::Handled();
}

void SRshipStatusPanel::OnServerAddressCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnEnter)
    {
        OnReconnectClicked();
    }
}

void SRshipStatusPanel::OnServerPortCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnEnter)
    {
        OnReconnectClicked();
    }
}

TSharedRef<ITableRow> SRshipStatusPanel::GenerateTargetRow(TSharedPtr<FRshipTargetListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SRshipTargetRow, OwnerTable)
        .Item(Item);
}

void SRshipStatusPanel::OnTargetSelectionChanged(TSharedPtr<FRshipTargetListItem> Item, ESelectInfo::Type SelectInfo)
{
    if (Item.IsValid() && Item->Component.IsValid())
    {
        // Could select the actor in the editor here
        // GEditor->SelectActor(Item->Component->GetOwner(), true, true);
    }
}

#if RSHIP_EDITOR_HAS_2110
TSharedRef<SWidget> SRshipStatusPanel::Build2110Section()
{
    return SNew(SVerticalBox)

        // Header
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("2110Title", "SMPTE 2110"))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
        ]

        // Status grid
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SVerticalBox)

            // Rivermax row
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("RivermaxLabel", "Rivermax: "))
                    .MinDesiredWidth(80.0f)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(RivermaxStatusText, STextBlock)
                    .Text(LOCTEXT("RivermaxDefault", "Checking..."))
                ]
            ]

            // PTP row
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PTPLabel", "PTP: "))
                    .MinDesiredWidth(80.0f)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(PTPStatusText, STextBlock)
                    .Text(LOCTEXT("PTPDefault", "Checking..."))
                ]
            ]

            // IPMX row
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("IPMXLabel", "IPMX: "))
                    .MinDesiredWidth(80.0f)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(IPMXStatusText, STextBlock)
                    .Text(LOCTEXT("IPMXDefault", "Checking..."))
                ]
            ]

            // GPUDirect row
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("GPUDirectLabel", "GPUDirect: "))
                    .MinDesiredWidth(80.0f)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(GPUDirectStatusText, STextBlock)
                    .Text(LOCTEXT("GPUDirectDefault", "Checking..."))
                ]
            ]

            // Network row
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("NetworkLabel", "Network: "))
                    .MinDesiredWidth(80.0f)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(NetworkStatusText, STextBlock)
                    .Text(LOCTEXT("NetworkDefault", "Checking..."))
                ]
            ]
        ];
}

void SRshipStatusPanel::Update2110Status()
{
    if (!FRship2110Module::IsAvailable())
    {
        // Module not loaded
        FText NotLoadedText = LOCTEXT("2110NotLoaded", "Module not loaded");
        if (RivermaxStatusText.IsValid()) RivermaxStatusText->SetText(NotLoadedText);
        if (PTPStatusText.IsValid()) PTPStatusText->SetText(NotLoadedText);
        if (IPMXStatusText.IsValid()) IPMXStatusText->SetText(NotLoadedText);
        if (GPUDirectStatusText.IsValid()) GPUDirectStatusText->SetText(LOCTEXT("GPUDirectNotLoaded", "N/A"));
        if (NetworkStatusText.IsValid()) NetworkStatusText->SetText(LOCTEXT("NetworkNotLoaded", "N/A"));
        return;
    }

    FRship2110Module& Module = FRship2110Module::Get();

    // Rivermax status
    if (RivermaxStatusText.IsValid())
    {
        if (Module.IsRivermaxAvailable())
        {
            RivermaxStatusText->SetText(LOCTEXT("RivermaxAvailable", "Available (DLL loaded)"));
            RivermaxStatusText->SetColorAndOpacity(FLinearColor(0.0f, 0.8f, 0.0f, 1.0f));
        }
        else
        {
            RivermaxStatusText->SetText(LOCTEXT("RivermaxNotAvailable", "Not available"));
            RivermaxStatusText->SetColorAndOpacity(FLinearColor(0.8f, 0.0f, 0.0f, 1.0f));
        }
    }

    // PTP status
    if (PTPStatusText.IsValid())
    {
        if (Module.IsPTPAvailable())
        {
            PTPStatusText->SetText(LOCTEXT("PTPAvailable", "Available"));
            PTPStatusText->SetColorAndOpacity(FLinearColor(0.0f, 0.8f, 0.0f, 1.0f));
        }
        else
        {
            PTPStatusText->SetText(LOCTEXT("PTPNotAvailable", "Not available"));
            PTPStatusText->SetColorAndOpacity(FLinearColor(0.8f, 0.0f, 0.0f, 1.0f));
        }
    }

    // IPMX status
    if (IPMXStatusText.IsValid())
    {
        if (Module.IsIPMXAvailable())
        {
            IPMXStatusText->SetText(LOCTEXT("IPMXAvailable", "Available"));
            IPMXStatusText->SetColorAndOpacity(FLinearColor(0.0f, 0.8f, 0.0f, 1.0f));
        }
        else
        {
            IPMXStatusText->SetText(LOCTEXT("IPMXNotAvailable", "Not available"));
            IPMXStatusText->SetColorAndOpacity(FLinearColor(0.8f, 0.0f, 0.0f, 1.0f));
        }
    }

    // GPUDirect status (compile-time check from module)
#if RSHIP_GPUDIRECT_AVAILABLE
    if (GPUDirectStatusText.IsValid())
    {
        GPUDirectStatusText->SetText(LOCTEXT("GPUDirectAvailable", "Compiled with support"));
        GPUDirectStatusText->SetColorAndOpacity(FLinearColor(0.0f, 0.8f, 0.0f, 1.0f));
    }
#else
    if (GPUDirectStatusText.IsValid())
    {
        GPUDirectStatusText->SetText(LOCTEXT("GPUDirectNotCompiled", "Not compiled"));
        GPUDirectStatusText->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
    }
#endif

    // Network status - show network interfaces
    if (NetworkStatusText.IsValid())
    {
        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (SocketSubsystem)
        {
            TArray<TSharedPtr<FInternetAddr>> Addresses;
            if (SocketSubsystem->GetLocalAdapterAddresses(Addresses))
            {
                TArray<FString> AddrStrings;
                for (const TSharedPtr<FInternetAddr>& Addr : Addresses)
                {
                    if (Addr.IsValid())
                    {
                        FString AddrStr = Addr->ToString(false);
                        // Skip loopback and link-local
                        if (!AddrStr.StartsWith(TEXT("127.")) && !AddrStr.StartsWith(TEXT("169.254.")))
                        {
                            AddrStrings.Add(AddrStr);
                        }
                    }
                }
                if (AddrStrings.Num() > 0)
                {
                    // Show up to 3 interfaces
                    FString DisplayStr;
                    for (int32 i = 0; i < FMath::Min(3, AddrStrings.Num()); ++i)
                    {
                        if (i > 0) DisplayStr += TEXT(", ");
                        DisplayStr += AddrStrings[i];
                    }
                    if (AddrStrings.Num() > 3)
                    {
                        DisplayStr += FString::Printf(TEXT(" (+%d more)"), AddrStrings.Num() - 3);
                    }
                    NetworkStatusText->SetText(FText::FromString(DisplayStr));
                    NetworkStatusText->SetColorAndOpacity(FLinearColor::White);
                }
                else
                {
                    NetworkStatusText->SetText(LOCTEXT("NoNetworkInterfaces", "No interfaces found"));
                    NetworkStatusText->SetColorAndOpacity(FLinearColor(0.8f, 0.5f, 0.0f, 1.0f));
                }
            }
            else
            {
                NetworkStatusText->SetText(LOCTEXT("NetworkEnumFailed", "Failed to enumerate"));
                NetworkStatusText->SetColorAndOpacity(FLinearColor(0.8f, 0.0f, 0.0f, 1.0f));
            }
        }
        else
        {
            NetworkStatusText->SetText(LOCTEXT("SocketSubsystemNA", "Socket subsystem N/A"));
            NetworkStatusText->SetColorAndOpacity(FLinearColor(0.8f, 0.0f, 0.0f, 1.0f));
        }
    }
}
#endif // RSHIP_EDITOR_HAS_2110

// ============================================================================
// SRshipTargetRow
// ============================================================================

void SRshipTargetRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
    Item = InArgs._Item;

    SMultiColumnTableRow<TSharedPtr<FRshipTargetListItem>>::Construct(
        FSuperRowType::FArguments(),
        InOwnerTableView
    );
}

TSharedRef<SWidget> SRshipTargetRow::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    if (ColumnName == "Status")
    {
        return SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(SImage)
                .Image(FRshipStatusPanelStyle::Get().GetBrush(
                    Item->bIsOnline ? "Rship.Status.Connected" : "Rship.Status.Disconnected"))
            ];
    }
    else if (ColumnName == "Name")
    {
        return SNew(STextBlock)
            .Text(FText::FromString(Item->DisplayName))
            .ToolTipText(FText::FromString(Item->TargetId));
    }
    else if (ColumnName == "Type")
    {
        return SNew(STextBlock)
            .Text(FText::FromString(Item->TargetType));
    }
    else if (ColumnName == "Emitters")
    {
        return SNew(STextBlock)
            .Text(FText::AsNumber(Item->EmitterCount))
            .Justification(ETextJustify::Center);
    }
    else if (ColumnName == "Actions")
    {
        return SNew(STextBlock)
            .Text(FText::AsNumber(Item->ActionCount))
            .Justification(ETextJustify::Center);
    }

    return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
