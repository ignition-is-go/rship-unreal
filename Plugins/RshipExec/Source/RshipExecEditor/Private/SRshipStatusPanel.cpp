// Copyright Rocketship. All Rights Reserved.

#include "SRshipStatusPanel.h"
#include "RshipStatusPanelStyle.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipSettings.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SHeaderRow.h"
#include "EditorStyleSet.h"
#include "ISettingsModule.h"
#include "Engine/Engine.h"

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
            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [
                BuildDiagnosticsSection()
            ]
        ]
    ];

    // Initial data load
    RefreshTargetList();
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
                    .Text(FText::AsNumber(InitialPort))
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

    for (URshipTargetComponent* Component : *Subsystem->TargetComponents)
    {
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
