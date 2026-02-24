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
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SHeaderRow.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#if __has_include("InstancedPropertyBagStructureDataProvider.h")
#include "InstancedPropertyBagStructureDataProvider.h"
#elif __has_include("InstancePropertyBagStructureDataProvider.h")
#include "InstancePropertyBagStructureDataProvider.h"
#else
#error "Property bag structure data provider header not found"
#endif
#include "StructUtils/PropertyBag.h"
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
            .Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(2.0f)
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
    // If list selection is temporarily cleared (e.g. after an action mutates editor state),
    // keep using our last known selected component so selection is stable across refreshes.
    if (!SelectedComponent.IsValid() && SelectedTargetComponent.IsValid())
    {
        SelectedComponent = SelectedTargetComponent;
    }
    // If the selected component instance was recreated, recover by owner actor.
    if (!SelectedComponent.IsValid() && SelectedTargetOwner.IsValid())
    {
        for (auto& Pair : *Subsystem->TargetComponents)
        {
            URshipTargetComponent* Candidate = Pair.Value;
            if (Candidate && Candidate->IsValidLowLevel() && Candidate->GetOwner() == SelectedTargetOwner.Get())
            {
                SelectedComponent = Candidate;
                break;
            }
        }
    }
    // Final fallback: recover selection by target ID across component/world recreation.
    if (!SelectedComponent.IsValid() && !SelectedTargetId.IsEmpty())
    {
        for (auto& Pair : *Subsystem->TargetComponents)
        {
            URshipTargetComponent* Candidate = Pair.Value;
            if (Candidate && Candidate->IsValidLowLevel() && Candidate->targetName == SelectedTargetId)
            {
                SelectedComponent = Candidate;
                break;
            }
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
        bool bSelectionRestored = false;
        if (SelectedComponent.IsValid())
        {
            for (const auto& Item : TargetItems)
            {
                if (Item.IsValid() && Item->Component.Get() == SelectedComponent.Get())
                {
                    TargetListView->SetSelection(Item, ESelectInfo::Direct);
                    TargetListView->RequestScrollIntoView(Item);
                    bSelectionRestored = true;
                    break;
                }
            }
        }
        // If component instance changed, restore by target id.
        if (!bSelectionRestored && !SelectedTargetId.IsEmpty())
        {
            for (const auto& Item : TargetItems)
            {
                if (Item.IsValid() && Item->TargetId == SelectedTargetId)
                {
                    TargetListView->SetSelection(Item, ESelectInfo::Direct);
                    TargetListView->RequestScrollIntoView(Item);
                    SelectedComponent = Item->Component;
                    bSelectionRestored = true;
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
        SelectedTargetOwner = SelectedComponent->GetOwner();
        SelectedTargetId = SelectedComponent->targetName;
    }
    else if (!TargetItems.ContainsByPredicate([this](const TSharedPtr<FRshipTargetListItem>& Item)
        {
            return Item.IsValid() && Item->Component.Get() == SelectedTargetComponent.Get();
        }))
    {
        bShouldRefreshActions = SelectedTargetComponent.IsValid();
        SelectedTargetComponent.Reset();
        // Keep owner fallback; do not clear SelectedTargetOwner here.
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
        SelectedTargetOwner = Item->Component->GetOwner();
        SelectedTargetId = Item->TargetId;
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
        SelectedTargetOwner.Reset();
        // Keep SelectedTargetId so selection can recover across editor state transitions.
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
                SelectedTargetOwner = TargetComp->GetOwner();
                SelectedTargetId = TargetComp->targetName;
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
        Entry->ParameterBag = MakeShared<FInstancedPropertyBag>();
        ActionEntries.Add(Entry);

        TSet<FName> UsedBagNames;

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
                AddSchemaFieldsRecursive(*ParamSchemaPtr, RootPath, Entry, UsedBagNames);
            }
        }

        TSharedPtr<SVerticalBox> CardBody;
        const FString ExpansionKey = SelectedTargetId + TEXT("::") + Entry->ActionId;
        const bool bInitiallyExpanded = ActionExpansionState.FindRef(ExpansionKey);

        ActionsListBox->AddSlot()
        .AutoHeight()
        .Padding(1.0f, 0.0f, 1.0f, 2.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(0.0f)
            [
                SNew(SExpandableArea)
                .AreaTitle(FText::FromString(Entry->ActionName.IsEmpty() ? Entry->ActionId : Entry->ActionName))
                .InitiallyCollapsed(!bInitiallyExpanded)
                .HeaderPadding(FMargin(6.0f, 3.0f))
                .BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
                .BodyBorderImage(FAppStyle::GetBrush("DetailsView.CollapsedCategory"))
                .AreaTitleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
                .OnAreaExpansionChanged(this, &SRshipStatusPanel::OnActionExpansionChanged, Entry->ActionId)
                .BodyContent()
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("NoBorder"))
                    .Padding(FMargin(6.0f, 4.0f, 6.0f, 6.0f))
                    [
                        SAssignNew(CardBody, SVerticalBox)
                    ]
                ]
            ]
        ];

        if (!CardBody.IsValid())
        {
            continue;
        }

        if (Entry->FieldBindings.Num() > 0)
        {
            FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
            Entry->BagDataProvider = MakeShared<FInstancePropertyBagStructureDataProvider>(*Entry->ParameterBag);

            for (const FRshipActionFieldBinding& Binding : Entry->FieldBindings)
            {
                if (Binding.bIsVector3)
                {
                    const FRshipActionFieldBinding BindingCopy = Binding;
                    const FString LabelText = FString::Join(Binding.FieldPath, TEXT("."));

                    auto GetVectorValue = [Entry, BindingCopy]() -> FVector
                    {
                        if (!Entry.IsValid() || !Entry->ParameterBag.IsValid())
                        {
                            return FVector::ZeroVector;
                        }
                        const TValueOrError<FVector*, EPropertyBagResult> Result =
                            Entry->ParameterBag->GetValueStruct<FVector>(BindingCopy.BagPropertyName);
                        return (Result.IsValid() && Result.GetValue() != nullptr) ? *Result.GetValue() : FVector::ZeroVector;
                    };

                    auto SetVectorComponent = [Entry, BindingCopy](int32 Axis, float NewValue)
                    {
                        if (!Entry.IsValid() || !Entry->ParameterBag.IsValid())
                        {
                            return;
                        }

                        const TValueOrError<FVector*, EPropertyBagResult> CurrentResult =
                            Entry->ParameterBag->GetValueStruct<FVector>(BindingCopy.BagPropertyName);
                        FVector Value = (CurrentResult.IsValid() && CurrentResult.GetValue() != nullptr)
                            ? *CurrentResult.GetValue()
                            : FVector::ZeroVector;

                        if (Axis == 0) { Value.X = NewValue; }
                        else if (Axis == 1) { Value.Y = NewValue; }
                        else { Value.Z = NewValue; }

                        Entry->ParameterBag->SetValueStruct(BindingCopy.BagPropertyName, Value);
                    };

                    CardBody->AddSlot()
                    .AutoHeight()
                    .Padding(0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(LabelText))
                            .MinDesiredWidth(150.0f)
                            .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SVectorInputBox)
                            .bColorAxisLabels(true)
                            .X_Lambda([GetVectorValue]() { return TOptional<float>(GetVectorValue().X); })
                            .Y_Lambda([GetVectorValue]() { return TOptional<float>(GetVectorValue().Y); })
                            .Z_Lambda([GetVectorValue]() { return TOptional<float>(GetVectorValue().Z); })
                            .OnXChanged_Lambda([SetVectorComponent](float V) { SetVectorComponent(0, V); })
                            .OnYChanged_Lambda([SetVectorComponent](float V) { SetVectorComponent(1, V); })
                            .OnZChanged_Lambda([SetVectorComponent](float V) { SetVectorComponent(2, V); })
                            .OnXCommitted_Lambda([SetVectorComponent](float V, ETextCommit::Type) { SetVectorComponent(0, V); })
                            .OnYCommitted_Lambda([SetVectorComponent](float V, ETextCommit::Type) { SetVectorComponent(1, V); })
                            .OnZCommitted_Lambda([SetVectorComponent](float V, ETextCommit::Type) { SetVectorComponent(2, V); })
                        ]
                    ];
                    continue;
                }

                FSinglePropertyParams SinglePropertyParams;
                SinglePropertyParams.bHideResetToDefault = true;
                SinglePropertyParams.bHideAssetThumbnail = true;
                SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Left;
                SinglePropertyParams.NameOverride = FText::FromString(FString::Join(Binding.FieldPath, TEXT(".")));

                TSharedPtr<ISinglePropertyView> SinglePropertyView =
                    PropertyEditorModule.CreateSingleProperty(Entry->BagDataProvider, Binding.BagPropertyName, SinglePropertyParams);
                if (!SinglePropertyView.IsValid() || !SinglePropertyView->HasValidProperty())
                {
                    continue;
                }

                Entry->FieldViews.Add(SinglePropertyView);
                CardBody->AddSlot()
                .AutoHeight()
                .Padding(0.0f)
                [
                    SinglePropertyView.ToSharedRef()
                ];
            }
        }
        else
        {
            CardBody->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 2.0f, 0.0f, 6.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ActionNoParams", "No parameters"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ];
        }

        CardBody->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f, 0.0f, 4.0f)
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

void SRshipStatusPanel::OnActionExpansionChanged(bool bIsExpanded, FString ActionId)
{
    if (ActionId.IsEmpty())
    {
        return;
    }

    const FString ExpansionKey = SelectedTargetId + TEXT("::") + ActionId;
    ActionExpansionState.Add(ExpansionKey, bIsExpanded);
}

namespace
{
FName MakeUniqueBagFieldName(const TArray<FString>& FieldPath, TSet<FName>& UsedBagNames)
{
    FString BaseName = FString::Join(FieldPath, TEXT("_"));
    if (BaseName.IsEmpty())
    {
        BaseName = TEXT("Param");
    }

    FName Candidate(*BaseName);
    int32 Suffix = 2;
    while (UsedBagNames.Contains(Candidate))
    {
        Candidate = FName(*FString::Printf(TEXT("%s_%d"), *BaseName, Suffix++));
    }

    UsedBagNames.Add(Candidate);
    return Candidate;
}
}

void SRshipStatusPanel::AddSchemaFieldsRecursive(
    const TSharedPtr<FJsonObject>& ParamSchema,
    const TArray<FString>& FieldPath,
    const TSharedPtr<FRshipActionEntryState>& Entry,
    TSet<FName>& UsedBagNames)
{
    if (!ParamSchema.IsValid() || !Entry.IsValid() || !Entry->ParameterBag.IsValid() || FieldPath.Num() == 0)
    {
        return;
    }

    FString ParamType = TEXT("string");
    ParamSchema->TryGetStringField(TEXT("type"), ParamType);

    if (ParamType == TEXT("object"))
    {
        auto TryBuildVector3Binding = [](const TSharedPtr<FJsonObject>& ObjectSchema, FString& OutX, FString& OutY, FString& OutZ) -> bool
        {
            const TSharedPtr<FJsonObject>* ChildPropsPtr = nullptr;
            if (!ObjectSchema.IsValid() ||
                !ObjectSchema->TryGetObjectField(TEXT("properties"), ChildPropsPtr) ||
                !ChildPropsPtr || !(*ChildPropsPtr).IsValid())
            {
                return false;
            }

            const TSharedPtr<FJsonObject>& Props = *ChildPropsPtr;
            TArray<FString> Keys;
            Props->Values.GetKeys(Keys);
            if (Keys.Num() != 3)
            {
                return false;
            }

            auto FindKeyIgnoreCase = [&Keys](const TCHAR* Expected) -> FString
            {
                for (const FString& Key : Keys)
                {
                    if (Key.Equals(Expected, ESearchCase::IgnoreCase))
                    {
                        return Key;
                    }
                }
                return FString();
            };

            const FString XKey = FindKeyIgnoreCase(TEXT("x"));
            const FString YKey = FindKeyIgnoreCase(TEXT("y"));
            const FString ZKey = FindKeyIgnoreCase(TEXT("z"));
            if (XKey.IsEmpty() || YKey.IsEmpty() || ZKey.IsEmpty())
            {
                return false;
            }

            const FString AxisKeys[3] = { XKey, YKey, ZKey };
            for (const FString& AxisKey : AxisKeys)
            {
                const TSharedPtr<FJsonObject>* AxisSchemaPtr = nullptr;
                if (!Props->TryGetObjectField(AxisKey, AxisSchemaPtr) || !AxisSchemaPtr || !(*AxisSchemaPtr).IsValid())
                {
                    return false;
                }

                FString AxisType;
                if (!(*AxisSchemaPtr)->TryGetStringField(TEXT("type"), AxisType) || AxisType != TEXT("number"))
                {
                    return false;
                }
            }

            OutX = XKey;
            OutY = YKey;
            OutZ = ZKey;
            return true;
        };

        FString XKey;
        FString YKey;
        FString ZKey;
        if (TryBuildVector3Binding(ParamSchema, XKey, YKey, ZKey))
        {
            const FName BagFieldName = MakeUniqueBagFieldName(FieldPath, UsedBagNames);
            Entry->ParameterBag->AddProperty(BagFieldName, EPropertyBagPropertyType::Struct, TBaseStructure<FVector>::Get());
            Entry->ParameterBag->SetValueStruct(BagFieldName, FVector::ZeroVector);

            FRshipActionFieldBinding& Binding = Entry->FieldBindings.AddDefaulted_GetRef();
            Binding.BagPropertyName = BagFieldName;
            Binding.FieldPath = FieldPath;
            Binding.ParamType = TEXT("number");
            Binding.bIsVector3 = true;
            Binding.VectorXName = XKey;
            Binding.VectorYName = YKey;
            Binding.VectorZName = ZKey;
            return;
        }

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
                AddSchemaFieldsRecursive(*ChildSchemaPtr, ChildPath, Entry, UsedBagNames);
            }
        }
        return;
    }

    EPropertyBagPropertyType BagType = EPropertyBagPropertyType::String;
    if (ParamType == TEXT("boolean"))
    {
        BagType = EPropertyBagPropertyType::Bool;
    }
    else if (ParamType == TEXT("number"))
    {
        BagType = EPropertyBagPropertyType::Double;
    }

    const FName BagFieldName = MakeUniqueBagFieldName(FieldPath, UsedBagNames);

    Entry->ParameterBag->AddProperty(BagFieldName, BagType, nullptr);

    if (BagType == EPropertyBagPropertyType::Bool)
    {
        Entry->ParameterBag->SetValueBool(BagFieldName, false);
    }
    else if (BagType == EPropertyBagPropertyType::Double)
    {
        Entry->ParameterBag->SetValueDouble(BagFieldName, 0.0);
    }
    else
    {
        Entry->ParameterBag->SetValueString(BagFieldName, FString());
    }

    FRshipActionFieldBinding& Binding = Entry->FieldBindings.AddDefaulted_GetRef();
    Binding.BagPropertyName = BagFieldName;
    Binding.FieldPath = FieldPath;
    Binding.ParamType = ParamType;
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

    if (!ActionEntry->ParameterBag.IsValid())
    {
        OutError = TEXT("Invalid action parameter state");
        return false;
    }

    for (const FRshipActionFieldBinding& Field : ActionEntry->FieldBindings)
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
            OutError = FString::Printf(TEXT("Failed to build payload path for '%s'"), *LeafName);
            return false;
        }

        if (Field.bIsVector3)
        {
            const TValueOrError<FVector*, EPropertyBagResult> VecResult = ActionEntry->ParameterBag->GetValueStruct<FVector>(Field.BagPropertyName);
            if (!VecResult.IsValid() || VecResult.GetValue() == nullptr)
            {
                OutError = FString::Printf(TEXT("Missing vector value for '%s'"), *LeafName);
                return false;
            }

            const FVector* Vec = VecResult.GetValue();
            TSharedPtr<FJsonObject> VectorObject = MakeShared<FJsonObject>();
            VectorObject->SetNumberField(Field.VectorXName, Vec->X);
            VectorObject->SetNumberField(Field.VectorYName, Vec->Y);
            VectorObject->SetNumberField(Field.VectorZName, Vec->Z);
            ParentObject->SetObjectField(LeafName, VectorObject);
            continue;
        }

        if (Field.ParamType == TEXT("boolean"))
        {
            const TValueOrError<bool, EPropertyBagResult> BoolResult = ActionEntry->ParameterBag->GetValueBool(Field.BagPropertyName);
            if (!BoolResult.HasValue())
            {
                OutError = FString::Printf(TEXT("Missing boolean value for '%s'"), *LeafName);
                return false;
            }
            ParentObject->SetBoolField(LeafName, BoolResult.GetValue());
        }
        else if (Field.ParamType == TEXT("number"))
        {
            const TValueOrError<double, EPropertyBagResult> NumberResult = ActionEntry->ParameterBag->GetValueDouble(Field.BagPropertyName);
            if (!NumberResult.HasValue())
            {
                OutError = FString::Printf(TEXT("Missing number value for '%s'"), *LeafName);
                return false;
            }

            ParentObject->SetNumberField(LeafName, NumberResult.GetValue());
        }
        else
        {
            const TValueOrError<FString, EPropertyBagResult> StringResult = ActionEntry->ParameterBag->GetValueString(Field.BagPropertyName);
            if (!StringResult.HasValue())
            {
                OutError = FString::Printf(TEXT("Missing text value for '%s'"), *LeafName);
                return false;
            }

            ParentObject->SetStringField(LeafName, StringResult.GetValue());
        }
    }

    return true;
}

FReply SRshipStatusPanel::OnExecuteActionClicked(TSharedPtr<FRshipActionEntryState> ActionEntry)
{
    URshipTargetComponent* TargetComponent = SelectedTargetComponent.Get();
    if (!TargetComponent || !TargetComponent->TargetData || !TargetComponent->GetOwner() || !ActionEntry.IsValid())
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

    bool bSuccess = false;
    const FString TargetId = TargetComponent->TargetData ? TargetComponent->TargetData->GetId() : FString();
    URshipSubsystem* Subsystem = GetSubsystem();
#if WITH_EDITOR
    // Allow Blueprint/script action execution while not in PIE/Simulate.
    if (GEditor && GEditor->PlayWorld == nullptr)
    {
        FEditorScriptExecutionGuard ScriptGuard;
        bSuccess = (Subsystem && !TargetId.IsEmpty())
            ? Subsystem->ExecuteTargetAction(TargetId, ActionEntry->ActionId, Payload.ToSharedRef())
            : TargetComponent->TargetData->TakeAction(TargetComponent->GetOwner(), ActionEntry->ActionId, Payload.ToSharedRef());
        GEditor->RedrawAllViewports(false);
    }
    else
#endif
    {
        bSuccess = (Subsystem && !TargetId.IsEmpty())
            ? Subsystem->ExecuteTargetAction(TargetId, ActionEntry->ActionId, Payload.ToSharedRef())
            : TargetComponent->TargetData->TakeAction(TargetComponent->GetOwner(), ActionEntry->ActionId, Payload.ToSharedRef());
    }

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
