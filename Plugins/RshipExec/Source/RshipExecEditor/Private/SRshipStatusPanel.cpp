// Copyright Rocketship. All Rights Reserved.

#include "SRshipStatusPanel.h"
#include "RshipStatusPanelStyle.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipSettings.h"
#include "Action.h"

#if RSHIP_EDITOR_HAS_2110
#include "Rship2110.h"  // For SMPTE 2110 status
#endif

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Styling/AppStyle.h"
#include "EditorStyleSet.h"
#include "ISettingsModule.h"
#include "Engine/Engine.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Editor.h"
#include "Selection.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Dom/JsonObject.h"
#include "Misc/DefaultValueHelper.h"

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
    RefreshActionsSection();
    UpdateConnectionStatus();
    UpdateDiagnostics();
#if RSHIP_EDITOR_HAS_2110
    Update2110Status();
#endif

    // Bind to editor selection changes to sync outliner selection with target list
    if (GEditor)
    {
        SelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw(
            this, &SRshipStatusPanel::OnEditorSelectionChanged);
    }
}

SRshipStatusPanel::~SRshipStatusPanel()
{
    // Unbind from editor selection
    if (SelectionChangedHandle.IsValid())
    {
        USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
    }
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
                    + SHeaderRow::Column("TargetId")
                    .DefaultLabel(LOCTEXT("TargetIdColumn", "Target ID"))
                    .FillWidth(0.5f)
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
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            BuildActionsSection()
        ];
}

TSharedRef<SWidget> SRshipStatusPanel::BuildActionsSection()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("ActionsTitle", "Actions"))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(6.0f)
            [
                SNew(SBox)
                .MaxDesiredHeight(260.0f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SAssignNew(ActionsListBox, SVerticalBox)
                    ]
                ]
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

    // Save currently selected component to restore after refresh
    TWeakObjectPtr<URshipTargetComponent> SelectedComponent;
    if (TargetListView.IsValid())
    {
        TArray<TSharedPtr<FRshipTargetListItem>> SelectedItems = TargetListView->GetSelectedItems();
        if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid())
        {
            SelectedComponent = SelectedItems[0]->Component;
        }
    }

    // Build new items list
    TArray<TSharedPtr<FRshipTargetListItem>> NewItems;
    TSet<URshipTargetComponent*> SeenComponents;

    enum class ERshipTargetViewMode : uint8
    {
        Editor,
        PIE,
        Simulate
    };

    const bool bIsSimulating = (GEditor && GEditor->bIsSimulatingInEditor);
    const bool bIsPIE = (GEditor && GEditor->PlayWorld != nullptr && !bIsSimulating);

    const ERshipTargetViewMode ActiveMode =
        bIsSimulating ? ERshipTargetViewMode::Simulate :
        (bIsPIE ? ERshipTargetViewMode::PIE : ERshipTargetViewMode::Editor);

    for (auto& Pair : *Subsystem->TargetComponents)
    {
        URshipTargetComponent* Component = Pair.Value;
        if (Component && Component->IsValidLowLevel() && !SeenComponents.Contains(Component))
        {
            ERshipTargetViewMode ComponentMode = ERshipTargetViewMode::Editor;
            if (AActor* Owner = Component->GetOwner())
            {
                if (UWorld* World = Owner->GetWorld())
                {
                    if (World->WorldType == EWorldType::PIE)
                    {
                        ComponentMode = bIsSimulating ? ERshipTargetViewMode::Simulate : ERshipTargetViewMode::PIE;
                    }
                    else if (World->WorldType != EWorldType::Editor &&
                             World->WorldType != EWorldType::EditorPreview)
                    {
                        // Hide non-editor/non-PIE worlds in this panel.
                        continue;
                    }
                }
            }

            // Show only targets relevant to the current editor mode.
            if (ComponentMode != ActiveMode)
            {
                continue;
            }

            SeenComponents.Add(Component);

            TSharedPtr<FRshipTargetListItem> Item = MakeShareable(new FRshipTargetListItem());
            Item->TargetId = Component->targetName;
            Item->DisplayName = Component->GetOwner() ? Component->GetOwner()->GetActorLabel() : Component->targetName;

            // Add suffix for PIE/Simulate instances
            if (AActor* Owner = Component->GetOwner())
            {
                if (Owner->GetWorld())
                {
                    if (ComponentMode == ERshipTargetViewMode::Simulate)
                    {
                        Item->DisplayName += TEXT(" (Simulate)");
                    }
                    else if (ComponentMode == ERshipTargetViewMode::PIE)
                    {
                        Item->DisplayName += TEXT(" (PIE)");
                    }
                }
            }

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

        // Restore selection if the component still exists
        if (SelectedComponent.IsValid())
        {
            for (const auto& Item : TargetItems)
            {
                if (Item.IsValid() && Item->Component.Get() == SelectedComponent.Get())
                {
                    TargetListView->SetSelection(Item, ESelectInfo::Direct);
                    break;
                }
            }
        }
    }

    bool bShouldRefreshActions = false;

    if (SelectedComponent.IsValid())
    {
        bShouldRefreshActions = (SelectedTargetComponent.Get() != SelectedComponent.Get());
        SelectedTargetComponent = SelectedComponent;
    }
    else if (!TargetItems.ContainsByPredicate([this](const TSharedPtr<FRshipTargetListItem>& Item)
        {
            return Item.IsValid() && Item->Component.Get() == SelectedTargetComponent.Get();
        }))
    {
        bShouldRefreshActions = SelectedTargetComponent.IsValid();
        SelectedTargetComponent.Reset();
    }

    if (bShouldRefreshActions)
    {
        RefreshActionsSection();
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
    // Don't respond to programmatic selection changes (prevents infinite loop)
    if (SelectInfo == ESelectInfo::Direct)
    {
        return;
    }

    if (Item.IsValid() && Item->Component.IsValid() && GEditor)
    {
        SelectedTargetComponent = Item->Component;
        RefreshActionsSection();

        // Select the actor in the outliner
        AActor* Owner = Item->Component->GetOwner();
        if (Owner)
        {
            GEditor->SelectNone(false, true, false);
            GEditor->SelectActor(Owner, true, true, true);
        }
    }
    else
    {
        SelectedTargetComponent.Reset();
        RefreshActionsSection();
    }
}

void SRshipStatusPanel::OnEditorSelectionChanged(UObject* Object)
{
    // Sync our list selection when outliner selection changes
    SyncSelectionFromOutliner();
}

void SRshipStatusPanel::SyncSelectionFromOutliner()
{
    if (!GEditor || !TargetListView.IsValid())
    {
        return;
    }

    USelection* Selection = GEditor->GetSelectedActors();
    if (!Selection || Selection->Num() == 0)
    {
        return;
    }

    // Find the first selected actor that has a target component in our list
    for (int32 i = 0; i < Selection->Num(); i++)
    {
        AActor* SelectedActor = Cast<AActor>(Selection->GetSelectedObject(i));
        if (!SelectedActor)
        {
            continue;
        }

        // Check if this actor has a target component
        URshipTargetComponent* TargetComp = SelectedActor->FindComponentByClass<URshipTargetComponent>();
        if (!TargetComp)
        {
            continue;
        }

        // Find matching item in our list
        for (const auto& Item : TargetItems)
        {
            if (Item.IsValid() && Item->Component.Get() == TargetComp)
            {
                // Select this item in the list (Direct = programmatic, won't trigger OnTargetSelectionChanged loop)
                TargetListView->SetSelection(Item, ESelectInfo::Direct);
                TargetListView->RequestScrollIntoView(Item);
                SelectedTargetComponent = TargetComp;
                RefreshActionsSection();
                return;
            }
        }
    }
}

void SRshipStatusPanel::RefreshActionsSection()
{
    if (!ActionsListBox.IsValid())
    {
        return;
    }

    ActionsListBox->ClearChildren();
    ActionEntries.Empty();

    URshipTargetComponent* TargetComponent = SelectedTargetComponent.Get();
    if (!TargetComponent || !TargetComponent->TargetData)
    {
        ActionsListBox->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("ActionsNoSelection", "Select a target to view and invoke actions."))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    TMap<FString, Action*> Actions = TargetComponent->TargetData->GetActions();
    if (Actions.Num() == 0)
    {
        ActionsListBox->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("ActionsNone", "No actions registered for this target."))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    TArray<TPair<FString, Action*>> SortedActions;
    SortedActions.Reserve(Actions.Num());
    for (const TPair<FString, Action*>& Pair : Actions)
    {
        SortedActions.Add(Pair);
    }

    SortedActions.Sort([](const TPair<FString, Action*>& A, const TPair<FString, Action*>& B)
    {
        return A.Key < B.Key;
    });

    for (const TPair<FString, Action*>& Pair : SortedActions)
    {
        if (!Pair.Value)
        {
            continue;
        }

        TSharedPtr<FRshipActionEntryState> Entry = MakeShared<FRshipActionEntryState>();
        Entry->ActionId = Pair.Key;
        Entry->ActionName = Pair.Value->GetName();
        Entry->ActionPtr = Pair.Value;
        ActionEntries.Add(Entry);

        TSharedPtr<SVerticalBox> CardBody;

        ActionsListBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
            .Padding(6.0f)
            [
                SAssignNew(CardBody, SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Entry->ActionName.IsEmpty() ? Entry->ActionId : Entry->ActionName))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
            ]
        ];

        if (!CardBody.IsValid())
        {
            continue;
        }

        const TSharedPtr<FJsonObject> Schema = Entry->ActionPtr->GetSchema();
        const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
        if (Schema.IsValid() && Schema->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && (*PropertiesPtr).IsValid())
        {
            TArray<FString> ParamNames;
            (*PropertiesPtr)->Values.GetKeys(ParamNames);
            ParamNames.Sort();

            for (const FString& ParamName : ParamNames)
            {
                const TSharedPtr<FJsonObject>* ParamSchemaPtr = nullptr;
                if (!(*PropertiesPtr)->TryGetObjectField(ParamName, ParamSchemaPtr) || !ParamSchemaPtr || !(*ParamSchemaPtr).IsValid())
                {
                    continue;
                }
                TArray<FString> RootPath;
                RootPath.Add(ParamName);
                AddSchemaFieldsRecursive(*ParamSchemaPtr, RootPath, 0, Entry, CardBody);
            }
        }

        if (Entry->Fields.Num() == 0)
        {
            CardBody->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ActionNoParams", "No parameters"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ];
        }

        CardBody->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("ActionGoButton", "GO"))
                .OnClicked(this, &SRshipStatusPanel::OnExecuteActionClicked, Entry)
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SAssignNew(Entry->ResultText, STextBlock)
                .Text(FText::GetEmpty())
            ]
        ];
    }
}

void SRshipStatusPanel::AddSchemaFieldsRecursive(
    const TSharedPtr<FJsonObject>& ParamSchema,
    const TArray<FString>& FieldPath,
    int32 Depth,
    const TSharedPtr<FRshipActionEntryState>& Entry,
    const TSharedPtr<SVerticalBox>& CardBody)
{
    if (!ParamSchema.IsValid() || !Entry.IsValid() || !CardBody.IsValid() || FieldPath.Num() == 0)
    {
        return;
    }

    FString ParamType = TEXT("string");
    ParamSchema->TryGetStringField(TEXT("type"), ParamType);

    if (ParamType == TEXT("object"))
    {
        const TSharedPtr<FJsonObject>* ChildPropsPtr = nullptr;
        if (ParamSchema->TryGetObjectField(TEXT("properties"), ChildPropsPtr) && ChildPropsPtr && (*ChildPropsPtr).IsValid())
        {
            TArray<FString> ChildNames;
            (*ChildPropsPtr)->Values.GetKeys(ChildNames);
            ChildNames.Sort();

            for (const FString& ChildName : ChildNames)
            {
                const TSharedPtr<FJsonObject>* ChildSchemaPtr = nullptr;
                if (!(*ChildPropsPtr)->TryGetObjectField(ChildName, ChildSchemaPtr) || !ChildSchemaPtr || !(*ChildSchemaPtr).IsValid())
                {
                    continue;
                }

                TArray<FString> ChildPath = FieldPath;
                ChildPath.Add(ChildName);
                AddSchemaFieldsRecursive(*ChildSchemaPtr, ChildPath, Depth + 1, Entry, CardBody);
            }
        }
        return;
    }

    FRshipActionFieldState& Field = Entry->Fields.AddDefaulted_GetRef();
    Field.FieldPath = FieldPath;
    Field.ParamName = FieldPath.Last();
    Field.ParamType = ParamType;
    Field.IndentDepth = Depth;

    CardBody->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 0.0f, 0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SSpacer)
                .Size(FVector2D(FMath::Max(0, Field.IndentDepth) * 12.0f, 1.0f))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Join(Field.FieldPath, TEXT("."))))
                .MinDesiredWidth(110.0f)
            ]
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            (ParamType == TEXT("boolean"))
            ? StaticCastSharedRef<SWidget>(
                SAssignNew(Field.BoolBox, SCheckBox)
            )
            : StaticCastSharedRef<SWidget>(
                SAssignNew(Field.ValueBox, SEditableTextBox)
                .HintText(
                    ParamType == TEXT("number")
                    ? LOCTEXT("ActionNumberHint", "number")
                    : LOCTEXT("ActionStringHint", "value"))
            )
        ]
    ];
}

bool SRshipStatusPanel::BuildActionPayload(const TSharedPtr<FRshipActionEntryState>& ActionEntry, TSharedPtr<FJsonObject>& OutPayload, FString& OutError) const
{
    OutPayload = MakeShared<FJsonObject>();
    OutError.Empty();

    if (!ActionEntry.IsValid())
    {
        OutError = TEXT("Invalid action entry");
        return false;
    }

    auto FindOrCreateObjectForPath = [](const TSharedPtr<FJsonObject>& Root, const TArray<FString>& PathSegments) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Current = Root;
        for (const FString& Segment : PathSegments)
        {
            const TSharedPtr<FJsonObject>* ExistingChildPtr = nullptr;
            if (Current->TryGetObjectField(Segment, ExistingChildPtr) && ExistingChildPtr && (*ExistingChildPtr).IsValid())
            {
                Current = *ExistingChildPtr;
                continue;
            }

            TSharedPtr<FJsonObject> NewChild = MakeShared<FJsonObject>();
            Current->SetObjectField(Segment, NewChild);
            Current = NewChild;
        }
        return Current;
    };

    for (const FRshipActionFieldState& Field : ActionEntry->Fields)
    {
        if (Field.FieldPath.Num() == 0)
        {
            OutError = TEXT("Invalid field path");
            return false;
        }

        TArray<FString> ParentPath = Field.FieldPath;
        const FString LeafName = ParentPath.Pop();
        TSharedPtr<FJsonObject> ParentObject = FindOrCreateObjectForPath(OutPayload, ParentPath);
        if (!ParentObject.IsValid())
        {
            OutError = FString::Printf(TEXT("Failed to build payload path for '%s'"), *Field.ParamName);
            return false;
        }

        if (Field.ParamType == TEXT("boolean"))
        {
            const bool bValue = Field.BoolBox.IsValid() && (Field.BoolBox->GetCheckedState() == ECheckBoxState::Checked);
            ParentObject->SetBoolField(LeafName, bValue);
        }
        else if (Field.ParamType == TEXT("number"))
        {
            if (!Field.ValueBox.IsValid())
            {
                OutError = FString::Printf(TEXT("Missing input field for '%s'"), *Field.ParamName);
                return false;
            }

            const FString Raw = Field.ValueBox->GetText().ToString().TrimStartAndEnd();
            double NumberValue = 0.0;
            if (!Raw.IsEmpty() && !FDefaultValueHelper::ParseDouble(Raw, NumberValue))
            {
                OutError = FString::Printf(TEXT("Invalid number for '%s'"), *Field.ParamName);
                return false;
            }

            ParentObject->SetNumberField(LeafName, NumberValue);
        }
        else
        {
            if (!Field.ValueBox.IsValid())
            {
                OutError = FString::Printf(TEXT("Missing input field for '%s'"), *Field.ParamName);
                return false;
            }

            ParentObject->SetStringField(LeafName, Field.ValueBox->GetText().ToString());
        }
    }

    return true;
}

FReply SRshipStatusPanel::OnExecuteActionClicked(TSharedPtr<FRshipActionEntryState> ActionEntry)
{
    URshipTargetComponent* TargetComponent = SelectedTargetComponent.Get();
    if (!TargetComponent || !TargetComponent->TargetData || !TargetComponent->GetOwner() || !ActionEntry.IsValid() || !ActionEntry->ActionPtr)
    {
        if (ActionEntry.IsValid() && ActionEntry->ResultText.IsValid())
        {
            ActionEntry->ResultText->SetText(LOCTEXT("ActionRunInvalidState", "Unable to execute (invalid target/action)"));
            ActionEntry->ResultText->SetColorAndOpacity(FLinearColor::Red);
        }
        return FReply::Handled();
    }

    TSharedPtr<FJsonObject> Payload;
    FString Error;
    if (!BuildActionPayload(ActionEntry, Payload, Error))
    {
        if (ActionEntry->ResultText.IsValid())
        {
            ActionEntry->ResultText->SetText(FText::FromString(Error));
            ActionEntry->ResultText->SetColorAndOpacity(FLinearColor::Red);
        }
        return FReply::Handled();
    }

    const bool bSuccess = ActionEntry->ActionPtr->Take(TargetComponent->GetOwner(), Payload.ToSharedRef());
    if (ActionEntry->ResultText.IsValid())
    {
        ActionEntry->ResultText->SetText(
            bSuccess
            ? LOCTEXT("ActionRunSuccess", "Action executed locally")
            : LOCTEXT("ActionRunFail", "Action execution failed"));
        ActionEntry->ResultText->SetColorAndOpacity(bSuccess ? FLinearColor::Green : FLinearColor::Red);
    }

    return FReply::Handled();
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
            .Text(FText::FromString(Item->DisplayName));
    }
    else if (ColumnName == "TargetId")
    {
        return SNew(SInlineEditableTextBlock)
            .Text(FText::FromString(Item->TargetId))
            .OnTextCommitted(this, &SRshipTargetRow::OnTargetIdCommitted);
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

void SRshipTargetRow::OnTargetIdCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
    // Only apply on Enter or focus lost, not on cancel
    if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
    {
        FString NewTargetId = NewText.ToString().TrimStartAndEnd();

        if (!NewTargetId.IsEmpty() && Item.IsValid() && Item->Component.IsValid())
        {
            // Only update if the ID actually changed
            if (NewTargetId != Item->TargetId)
            {
                Item->Component->SetTargetId(NewTargetId);
                Item->TargetId = NewTargetId;
            }
        }
    }
}

#undef LOCTEXT_NAMESPACE
