// Copyright Rocketship. All Rights Reserved.

#include "SRshipStatusPanel.h"
#include "RshipStatusPanelStyle.h"
#include "RshipSubsystem.h"
#include "RshipSettings.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "SRshipStatusPanel"

namespace
{
    static FText ConnectionStateToText(uint8 StateValue)
    {
        switch (static_cast<ERshipConnectionState>(StateValue))
        {
        case ERshipConnectionState::Connecting:
            return LOCTEXT("ConnectionStateConnecting", "Connecting");
        case ERshipConnectionState::Connected:
            return LOCTEXT("ConnectionStateConnected", "Connected");
        case ERshipConnectionState::Reconnecting:
            return LOCTEXT("ConnectionStateReconnecting", "Reconnecting");
        case ERshipConnectionState::BackingOff:
            return LOCTEXT("ConnectionStateBackingOff", "Backing Off");
        case ERshipConnectionState::Disconnected:
        default:
            return LOCTEXT("ConnectionStateDisconnected", "Disconnected");
        }
    }
}

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

            // Diagnostics Section
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 8.0f)
            [
                BuildDiagnosticsSection()
            ]
        ]
    ];

    // Initial data load
    UpdateConnectionStatus();
    UpdateDiagnostics();
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


        // Prominent local-only banner when remote communication is disabled
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SBorder)
            .Visibility(this, &SRshipStatusPanel::GetRemoteOffBannerVisibility)
            .Padding(8.0f)
            .BorderBackgroundColor(FLinearColor(0.75f, 0.1f, 0.1f, 1.0f))
            [
                SNew(STextBlock)
                .Text(LOCTEXT("RemoteOffBanner", "REMOTE OFF  -  LOCAL ACTIONS ONLY"))
                .ColorAndOpacity(FLinearColor::White)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            ]
        ]

        // Global remote communication toggle
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("RemoteToggleLabel", "Remote Server:"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(RemoteToggleCheckBox, SCheckBox)
                .OnCheckStateChanged(this, &SRshipStatusPanel::OnRemoteToggleChanged)
                .IsChecked(this, &SRshipStatusPanel::GetRemoteToggleState)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return IsRemoteControlsEnabled()
                            ? LOCTEXT("RemoteOnLabel", "ON")
                            : LOCTEXT("RemoteOffLabel", "OFF");
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    .ColorAndOpacity_Lambda([this]()
                    {
                        return IsRemoteControlsEnabled()
                            ? FLinearColor(0.1f, 0.7f, 0.1f, 1.0f)
                            : FLinearColor(0.9f, 0.1f, 0.1f, 1.0f);
                    })
                ]
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
                .IsEnabled(this, &SRshipStatusPanel::IsRemoteControlsEnabled)
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
                    .IsEnabled(this, &SRshipStatusPanel::IsRemoteControlsEnabled)
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
                .IsEnabled(this, &SRshipStatusPanel::IsRemoteControlsEnabled)
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
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            SAssignNew(SyncStatusText, STextBlock)
            .Text(LOCTEXT("SyncStatusDefault", "Sync: idle"))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SAssignNew(TargetSyncText, STextBlock)
            .Text(LOCTEXT("TargetSyncDefault", "Targets: 0 local / 0 remote"))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SAssignNew(ActionSyncText, STextBlock)
            .Text(LOCTEXT("ActionSyncDefault", "Actions: 0 local / 0 remote"))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SAssignNew(EmitterSyncText, STextBlock)
            .Text(LOCTEXT("EmitterSyncDefault", "Emitters: 0 local / 0 remote"))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SAssignNew(StatusSyncText, STextBlock)
            .Text(LOCTEXT("StatusSyncDefault", "Status: 0 local / 0 remote"))
        ];
}

ECheckBoxState SRshipStatusPanel::GetRemoteToggleState() const
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return ECheckBoxState::Checked;
    }
    return Subsystem->IsRemoteCommunicationEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SRshipStatusPanel::OnRemoteToggleChanged(ECheckBoxState NewState)
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return;
    }

    const bool bEnableRemote = (NewState == ECheckBoxState::Checked);
    Subsystem->SetRemoteCommunicationEnabled(bEnableRemote);
    UpdateConnectionStatus();
    UpdateDiagnostics();
}

EVisibility SRshipStatusPanel::GetRemoteOffBannerVisibility() const
{
    return IsRemoteControlsEnabled() ? EVisibility::Collapsed : EVisibility::Visible;
}

bool SRshipStatusPanel::IsRemoteControlsEnabled() const
{
    URshipSubsystem* Subsystem = GetSubsystem();
    return !Subsystem || Subsystem->IsRemoteCommunicationEnabled();
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

    const bool bRemoteEnabled = Subsystem->IsRemoteCommunicationEnabled();
    bool bConnected = Subsystem->IsConnected();
    bool bBackingOff = Subsystem->IsRateLimiterBackingOff();

    FText StatusText;
    FName BrushName;

    if (!bRemoteEnabled)
    {
        StatusText = LOCTEXT("StatusRemoteOff", "REMOTE OFF (Local Only)");
        BrushName = "Rship.Status.Disconnected";
    }
    else if (bConnected)
    {
        StatusText = ConnectionStateToText(Subsystem->GetConnectionStateValue());
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
        StatusText = ConnectionStateToText(Subsystem->GetConnectionStateValue());
        BrushName = "Rship.Status.Disconnected";
    }

    if (ConnectionStatusText.IsValid())
    {
        ConnectionStatusText->SetText(StatusText);
        ConnectionStatusText->SetColorAndOpacity(bRemoteEnabled ? FSlateColor::UseForeground() : FLinearColor(0.9f, 0.1f, 0.1f, 1.0f));
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

    const FText SyncReason = FText::FromString(Subsystem->GetTopologySyncReason());
    const FText SyncDetail = FText::FromString(Subsystem->GetTopologySyncDetail());
    const FText SyncAge = FText::AsNumber(FMath::RoundToInt(Subsystem->GetTopologySyncAgeSeconds() * 10.0f) / 10.0f);

    if (SyncStatusText.IsValid())
    {
        if (Subsystem->IsTopologySyncInFlight())
        {
            SyncStatusText->SetText(FText::Format(
                LOCTEXT("SyncStatusInFlight", "Syncing {0} ({1}s)  {2}"),
                SyncReason,
                SyncAge,
                SyncDetail));
            SyncStatusText->SetColorAndOpacity(FLinearColor(0.95f, 0.8f, 0.2f, 1.0f));
        }
        else if (!SyncReason.IsEmpty())
        {
            SyncStatusText->SetText(FText::Format(
                LOCTEXT("SyncStatusLast", "Last sync {0} ({1}s)  {2}"),
                SyncReason,
                SyncAge,
                SyncDetail));
            SyncStatusText->SetColorAndOpacity(FSlateColor::UseForeground());
        }
        else
        {
            SyncStatusText->SetText(LOCTEXT("SyncStatusIdle", "Sync: idle"));
            SyncStatusText->SetColorAndOpacity(FSlateColor::UseForeground());
        }
    }

    if (TargetSyncText.IsValid())
    {
        TargetSyncText->SetText(FText::Format(
            LOCTEXT("TargetSyncFmt", "Targets: {0} local / {1} remote"),
            FText::AsNumber(Subsystem->GetLocalTargetCount()),
            FText::AsNumber(Subsystem->GetRemoteTargetCount())));
    }

    if (ActionSyncText.IsValid())
    {
        ActionSyncText->SetText(FText::Format(
            LOCTEXT("ActionSyncFmt", "Actions: {0} local / {1} remote"),
            FText::AsNumber(Subsystem->GetLocalActionCount()),
            FText::AsNumber(Subsystem->GetRemoteActionCount())));
    }

    if (EmitterSyncText.IsValid())
    {
        EmitterSyncText->SetText(FText::Format(
            LOCTEXT("EmitterSyncFmt", "Emitters: {0} local / {1} remote"),
            FText::AsNumber(Subsystem->GetLocalEmitterCount()),
            FText::AsNumber(Subsystem->GetRemoteEmitterCount())));
    }

    if (StatusSyncText.IsValid())
    {
        StatusSyncText->SetText(FText::Format(
            LOCTEXT("StatusSyncFmt", "Status: {0} local / {1} remote"),
            FText::AsNumber(Subsystem->GetLocalTargetStatusCount()),
            FText::AsNumber(Subsystem->GetRemoteTargetStatusCount())));
    }
}

FReply SRshipStatusPanel::OnReconnectClicked()
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (Subsystem && Subsystem->IsRemoteCommunicationEnabled())
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

#undef LOCTEXT_NAMESPACE
