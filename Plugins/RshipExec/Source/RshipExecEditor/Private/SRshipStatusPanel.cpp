// Copyright Rocketship. All Rights Reserved.

#include "SRshipStatusPanel.h"
#include "RshipStatusPanelStyle.h"
#include "RshipSubsystem.h"
#include "RshipActorRegistrationComponent.h"
#include "RshipSettings.h"
#include "Misc/Crc.h"

#if RSHIP_EDITOR_HAS_2110
#include "Rship2110.h"  // For SMPTE 2110 status
#include "Rship2110Subsystem.h"
#include "Rship2110Settings.h"
#endif

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SHeaderRow.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "ISceneOutliner.h"
#include "ISceneOutlinerMode.h"
#include "ISceneOutlinerHierarchy.h"
#include "ISceneOutlinerTreeItem.h"
#include "ISceneOutlinerColumn.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerFwd.h"
#include "SSceneOutliner.h"
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
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Editor.h"
#include "Selection.h"
#include "Dom/JsonObject.h"
#include "Algo/Sort.h"

#define LOCTEXT_NAMESPACE "SRshipStatusPanel"

namespace RshipStatusOutliner
{
    static uint64 MakeStableItemId(const FString& FullTargetId)
    {
        // Two independent 32-bit hashes combined into a stable 64-bit tree item ID.
        const uint32 H1 = GetTypeHash(FullTargetId);
        const uint32 H2 = FCrc::StrCrc32(*FullTargetId);
        return (static_cast<uint64>(H1) << 32) | static_cast<uint64>(H2);
    }

    struct FTargetTreeItem : ISceneOutlinerTreeItem
    {
        explicit FTargetTreeItem(const TSharedPtr<FRshipTargetListItem>& InItem)
            : ISceneOutlinerTreeItem(Type)
            , Item(InItem)
            , StableId(Item.IsValid() ? MakeStableItemId(Item->FullTargetId) : 0)
        {
        }

        static const FSceneOutlinerTreeItemType Type;

        virtual bool IsValid() const override
        {
            return Item.IsValid();
        }

        virtual FSceneOutlinerTreeItemID GetID() const override
        {
            return FSceneOutlinerTreeItemID(StableId);
        }

        virtual FString GetDisplayString() const override
        {
            return Item.IsValid() ? Item->DisplayName : FString();
        }

        virtual bool CanInteract() const override
        {
            return true;
        }

        virtual TSharedRef<SWidget> GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow) override
        {
            return SNew(STextBlock)
                .Text(Item.IsValid() ? FText::FromString(Item->DisplayName) : FText::GetEmpty())
                .HighlightText(Outliner.GetFilterHighlightText());
        }

        TSharedPtr<FRshipTargetListItem> Item;

    private:
        const uint64 StableId;
    };
    const FSceneOutlinerTreeItemType FTargetTreeItem::Type(&ISceneOutlinerTreeItem::Type);

    static const FRshipTargetListItem* GetTargetItem(const ISceneOutlinerTreeItem& Item)
    {
        const FTargetTreeItem* TargetItem = Item.CastTo<FTargetTreeItem>();
        return (TargetItem && TargetItem->Item.IsValid()) ? TargetItem->Item.Get() : nullptr;
    }

    static TSharedPtr<FRshipTargetListItem> GetTargetItem(FSceneOutlinerTreeItemPtr Item)
    {
        if (!Item.IsValid())
        {
            return nullptr;
        }

        FTargetTreeItem* TargetItem = Item->CastTo<FTargetTreeItem>();
        return TargetItem ? TargetItem->Item : nullptr;
    }

    class FTargetIdColumn : public ISceneOutlinerColumn
    {
    public:
        explicit FTargetIdColumn(ISceneOutliner& Outliner)
            : SceneOutlinerWeak(StaticCastSharedRef<ISceneOutliner>(Outliner.AsShared()))
        {
        }

        static FName GetID()
        {
            static const FName Id(TEXT("RshipStatus.TargetId"));
            return Id;
        }

        virtual FName GetColumnID() override { return GetID(); }

        virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override
        {
            return SHeaderRow::Column(GetColumnID())
                .DefaultLabel(LOCTEXT("StatusPanelTargetIdColumn", "TargetId"))
                .FillWidth(0.5f);
        }

        virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override
        {
            const FRshipTargetListItem* Item = GetTargetItem(*TreeItem);
            return SNew(STextBlock).Text(Item ? FText::FromString(Item->TargetId) : FText::GetEmpty());
        }

    private:
        TWeakPtr<ISceneOutliner> SceneOutlinerWeak;
    };

    class FTypeColumn : public ISceneOutlinerColumn
    {
    public:
        explicit FTypeColumn(ISceneOutliner& Outliner) {}
        static FName GetID()
        {
            static const FName Id(TEXT("RshipStatus.Type"));
            return Id;
        }
        virtual FName GetColumnID() override { return GetID(); }
        virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override
        {
            return SHeaderRow::Column(GetColumnID()).DefaultLabel(LOCTEXT("StatusPanelTypeColumn", "Type")).FixedWidth(90.0f);
        }
        virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override
        {
            const FRshipTargetListItem* Item = GetTargetItem(*TreeItem);
            return SNew(STextBlock).Text(Item ? FText::FromString(Item->TargetType) : FText::GetEmpty());
        }
    };

    class FEmitterCountColumn : public ISceneOutlinerColumn
    {
    public:
        explicit FEmitterCountColumn(ISceneOutliner& Outliner) {}
        static FName GetID()
        {
            static const FName Id(TEXT("RshipStatus.Emitters"));
            return Id;
        }
        virtual FName GetColumnID() override { return GetID(); }
        virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override
        {
            return SHeaderRow::Column(GetColumnID()).DefaultLabel(LOCTEXT("StatusPanelEmittersColumn", "Emitters")).FixedWidth(70.0f);
        }
        virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override
        {
            const FRshipTargetListItem* Item = GetTargetItem(*TreeItem);
            return SNew(STextBlock).Text(Item ? FText::AsNumber(Item->EmitterCount) : FText::GetEmpty());
        }
    };

    class FActionCountColumn : public ISceneOutlinerColumn
    {
    public:
        explicit FActionCountColumn(ISceneOutliner& Outliner) {}
        static FName GetID()
        {
            static const FName Id(TEXT("RshipStatus.Actions"));
            return Id;
        }
        virtual FName GetColumnID() override { return GetID(); }
        virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override
        {
            return SHeaderRow::Column(GetColumnID()).DefaultLabel(LOCTEXT("StatusPanelActionsColumn", "Actions")).FixedWidth(70.0f);
        }
        virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override
        {
            const FRshipTargetListItem* Item = GetTargetItem(*TreeItem);
            return SNew(STextBlock).Text(Item ? FText::AsNumber(Item->ActionCount) : FText::GetEmpty());
        }
    };

    class FTargetHierarchy : public ISceneOutlinerHierarchy
    {
    public:
        FTargetHierarchy(
            ISceneOutlinerMode* InMode,
            TFunction<const TArray<TSharedPtr<FRshipTargetListItem>>&()> InItemsProvider)
            : ISceneOutlinerHierarchy(InMode)
            , ItemsProvider(MoveTemp(InItemsProvider))
        {
        }

        virtual void CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const override
        {
            for (const TSharedPtr<FRshipTargetListItem>& Item : ItemsProvider())
            {
                if (Item.IsValid())
                {
                    OutItems.Add(Mode->CreateItemFor<FTargetTreeItem>(Item, true));
                }
            }
        }

        virtual void CreateChildren(const FSceneOutlinerTreeItemPtr& Item, TArray<FSceneOutlinerTreeItemPtr>& OutChildren) const override
        {
        }

        virtual FSceneOutlinerTreeItemPtr FindOrCreateParentItem(
            const ISceneOutlinerTreeItem& Item,
            const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items,
            bool bCreate = false) override
        {
            const FRshipTargetListItem* Child = GetTargetItem(Item);
            if (!Child)
            {
                return nullptr;
            }

            for (const FString& ParentId : Child->ParentTargetIds)
            {
                for (const TPair<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Pair : Items)
                {
                    const FRshipTargetListItem* Candidate = GetTargetItem(*Pair.Value);
                    if (Candidate && Candidate->FullTargetId == ParentId)
                    {
                        return Pair.Value;
                    }
                }
            }

            return nullptr;
        }

    private:
        TFunction<const TArray<TSharedPtr<FRshipTargetListItem>>&()> ItemsProvider;
    };

    class FTargetMode : public ISceneOutlinerMode
    {
    public:
        FTargetMode(
            SSceneOutliner* InOutliner,
            TFunction<const TArray<TSharedPtr<FRshipTargetListItem>>&()> InItemsProvider,
            TFunction<void(TSharedPtr<FRshipTargetListItem>, ESelectInfo::Type)> InSelectionChanged)
            : ISceneOutlinerMode(InOutliner)
            , ItemsProvider(MoveTemp(InItemsProvider))
            , SelectionChanged(MoveTemp(InSelectionChanged))
        {
        }

        virtual void Rebuild() override
        {
            Hierarchy = CreateHierarchy();
        }

        virtual void OnItemSelectionChanged(
            FSceneOutlinerTreeItemPtr Item,
            ESelectInfo::Type SelectionType,
            const FSceneOutlinerItemSelection& Selection) override
        {
            SelectionChanged(GetTargetItem(Item), SelectionType);
        }

        virtual bool IsInteractive() const override { return true; }
        virtual bool SupportsKeyboardFocus() const override { return true; }
        virtual ESelectionMode::Type GetSelectionMode() const override { return ESelectionMode::Single; }
        virtual int32 GetTypeSortPriority(const ISceneOutlinerTreeItem& Item) const override { return 0; }

    protected:
        virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override
        {
            return MakeUnique<FTargetHierarchy>(this, ItemsProvider);
        }

    private:
        TFunction<const TArray<TSharedPtr<FRshipTargetListItem>>&()> ItemsProvider;
        TFunction<void(TSharedPtr<FRshipTargetListItem>, ESelectInfo::Type)> SelectionChanged;
    };
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
    RefreshActionsSection();
    UpdateConnectionStatus();
    UpdateDiagnostics();
    UpdateSyncSettings();
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

    TryApplyPendingTargetSelection();

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

TSharedRef<SWidget> SRshipStatusPanel::BuildTargetsSection()
{
    if (!TargetSceneOutliner.IsValid())
    {
        FSceneOutlinerInitializationOptions InitOptions;
        InitOptions.bShowHeaderRow = true;
        InitOptions.bShowSearchBox = true;
        InitOptions.bShowCreateNewFolder = false;
        InitOptions.bCanSelectGeneratedColumns = true;
        InitOptions.OutlinerIdentifier = TEXT("RshipStatusPanelTargets");
        InitOptions.PrimaryColumnName = FSceneOutlinerBuiltInColumnTypes::Label();

        InitOptions.ColumnMap.Add(
            FSceneOutlinerBuiltInColumnTypes::Label(),
            FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));
        InitOptions.ColumnMap.Add(
            RshipStatusOutliner::FTargetIdColumn::GetID(),
            FSceneOutlinerColumnInfo(
                ESceneOutlinerColumnVisibility::Visible,
                1,
                FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& Outliner)
                {
                    return MakeShared<RshipStatusOutliner::FTargetIdColumn>(Outliner);
                }),
                true,
                TOptional<float>(),
                TAttribute<FText>(LOCTEXT("StatusPanelTargetIdColumnMenuLabel", "TargetId"))));
        InitOptions.ColumnMap.Add(
            RshipStatusOutliner::FTypeColumn::GetID(),
            FSceneOutlinerColumnInfo(
                ESceneOutlinerColumnVisibility::Visible,
                2,
                FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& Outliner)
                {
                    return MakeShared<RshipStatusOutliner::FTypeColumn>(Outliner);
                }),
                true,
                TOptional<float>(),
                TAttribute<FText>(LOCTEXT("StatusPanelTypeColumnMenuLabel", "Type"))));
        InitOptions.ColumnMap.Add(
            RshipStatusOutliner::FEmitterCountColumn::GetID(),
            FSceneOutlinerColumnInfo(
                ESceneOutlinerColumnVisibility::Visible,
                3,
                FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& Outliner)
                {
                    return MakeShared<RshipStatusOutliner::FEmitterCountColumn>(Outliner);
                }),
                true,
                TOptional<float>(),
                TAttribute<FText>(LOCTEXT("StatusPanelEmittersColumnMenuLabel", "Emitters"))));
        InitOptions.ColumnMap.Add(
            RshipStatusOutliner::FActionCountColumn::GetID(),
            FSceneOutlinerColumnInfo(
                ESceneOutlinerColumnVisibility::Visible,
                4,
                FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& Outliner)
                {
                    return MakeShared<RshipStatusOutliner::FActionCountColumn>(Outliner);
                }),
                true,
                TOptional<float>(),
                TAttribute<FText>(LOCTEXT("StatusPanelActionsColumnMenuLabel", "Actions"))));

        InitOptions.ModeFactory = FCreateSceneOutlinerMode::CreateLambda(
            [this](SSceneOutliner* OutlinerWidget)
            {
                return new RshipStatusOutliner::FTargetMode(
                    OutlinerWidget,
                    [this]() -> const TArray<TSharedPtr<FRshipTargetListItem>>& { return TargetItems; },
                    [this](TSharedPtr<FRshipTargetListItem> SelectedItem, ESelectInfo::Type SelectInfo)
                    {
                        OnTargetSelectionChanged(SelectedItem, SelectInfo);
                    });
            });

        FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
        TargetSceneOutliner = SceneOutlinerModule.CreateSceneOutliner(InitOptions);
    }

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

        ]

        // Target list
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(4.0f)
            [
                TargetSceneOutliner.IsValid()
                    ? TargetSceneOutliner.ToSharedRef()
                    : SNullWidget::NullWidget
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
                SAssignNew(ActionsListBox, SVerticalBox)
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

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 2.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("InboundFrameLabel", "Inbound frame: "))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(InboundFrameCounterText, STextBlock)
                        .Text(LOCTEXT("InboundFrameDefault", "0"))
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
                        .Text(LOCTEXT("NextApplyLabel", "Next planned apply: "))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(InboundNextApplyFrameText, STextBlock)
                        .Text(LOCTEXT("NextApplyDefault", "0"))
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
                        .Text(LOCTEXT("QueuedFrameSpanLabel", "Queued frame span: "))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(InboundQueuedFrameSpanText, STextBlock)
                        .Text(LOCTEXT("QueuedFrameSpanDefault", "n/a"))
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

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 2.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ExactDroppedLabel", "Exact dropped: "))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(ExactDroppedText, STextBlock)
                        .Text(LOCTEXT("ExactDroppedDefault", "0"))
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
    const bool InitialRequireExactFrame = Subsystem ? Subsystem->IsInboundRequireExactFrame() : false;
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
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("RequireExactFrameLabel", "Inbound require exact frame:"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(InboundRequireExactFrameCheckBox, SCheckBox)
                .IsChecked(InitialRequireExactFrame ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged(this, &SRshipStatusPanel::OnRequireExactFrameChanged)
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
            [
                SNew(SButton)
                .Text(LOCTEXT("CopyRolloutIni", "Copy Timing Profile"))
                .OnClicked(this, &SRshipStatusPanel::OnCopyIniRolloutSnippetClicked)
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("IniSnippetHeading", "Resolved timing profile (for deployment):"))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f, 1.0f))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SAssignNew(IniRolloutText, STextBlock)
            .WrapTextAt(900.0f)
            .Text(LOCTEXT("IniSnippetDefault", "Click \"Save Timing Defaults\" to persist current values, then copy profile for node deployment."))
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
    if (!Subsystem)
    {
        TargetItems.Empty();
        RootTargetItems.Empty();
        PendingSelectionTargetId.Empty();
        if (TargetSceneOutliner.IsValid())
        {
            TargetSceneOutliner->FullRefresh();
        }
        return;
    }

    // Preserve selection across refreshes.
    TWeakObjectPtr<URshipActorRegistrationComponent> SelectedComponent = SelectedTargetComponent;
    // If the selected component instance was recreated, recover by owner actor.
    if (!SelectedComponent.IsValid() && SelectedTargetOwner.IsValid() && Subsystem->TargetComponents)
    {
        for (auto& Pair : *Subsystem->TargetComponents)
        {
            URshipActorRegistrationComponent* Candidate = Pair.Value;
            if (Candidate && Candidate->IsValidLowLevel() && Candidate->GetOwner() == SelectedTargetOwner.Get())
            {
                SelectedComponent = Candidate;
                break;
            }
        }
    }
    // Final fallback: recover selection by target ID across component/world recreation.
    if (!SelectedComponent.IsValid() && !SelectedTargetId.IsEmpty() && Subsystem->TargetComponents)
    {
        for (auto& Pair : *Subsystem->TargetComponents)
        {
            URshipActorRegistrationComponent* Candidate = Pair.Value;
            if (Candidate && Candidate->IsValidLowLevel() && Candidate->TargetData &&
                (Candidate->TargetData->GetId() == SelectedTargetId || Candidate->targetName == SelectedTargetId))
            {
                SelectedComponent = Candidate;
                break;
            }
        }
    }

    // Build new flat items list from all managed targets (includes automation subtargets).
    TArray<TSharedPtr<FRshipTargetListItem>> NewItems;
    TMap<FString, TSharedPtr<FRshipTargetListItem>> ItemsByFullId;

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

    TArray<FRshipManagedTargetView> ManagedTargets;
    Subsystem->GetManagedTargetsSnapshot(ManagedTargets);

    for (const FRshipManagedTargetView& ManagedTarget : ManagedTargets)
    {
        URshipActorRegistrationComponent* Component = ManagedTarget.BoundTargetComponent.Get();
        if (ManagedTarget.bBoundToComponent && !IsValid(Component))
        {
            continue;
        }

        ERshipTargetViewMode ComponentMode = ERshipTargetViewMode::Editor;
        if (Component)
        {
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
        }

        const FString FullTargetId = ManagedTarget.Id;
        if (FullTargetId.IsEmpty())
        {
            continue;
        }

        TSharedPtr<FRshipTargetListItem> Item = MakeShared<FRshipTargetListItem>();
        Item->FullTargetId = FullTargetId;
        Item->TargetId = Component ? Component->targetName : FullTargetId;
        Item->DisplayName = !ManagedTarget.Name.IsEmpty() ? ManagedTarget.Name : Item->TargetId;
        Item->TargetType = Component
            ? (Component->Tags.Num() > 0 ? Component->Tags[0] : TEXT("Target"))
            : (ManagedTarget.ParentTargetIds.Num() > 0 ? TEXT("Subtarget") : TEXT("Target"));
        Item->bIsOnline = true;
        Item->EmitterCount = ManagedTarget.EmitterCount;
        Item->ActionCount = ManagedTarget.ActionCount;
        Item->ParentTargetIds = ManagedTarget.ParentTargetIds;
        Item->Component = Component;

        // Add suffix for PIE/Simulate instances
        if (Component)
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

        NewItems.Add(Item);
        ItemsByFullId.Add(FullTargetId, Item);
    }

    // Sort by name for deterministic roots/children ordering.
    NewItems.Sort([](const TSharedPtr<FRshipTargetListItem>& A, const TSharedPtr<FRshipTargetListItem>& B)
    {
        return A->DisplayName < B->DisplayName;
    });

    for (const TSharedPtr<FRshipTargetListItem>& Item : NewItems)
    {
        if (Item.IsValid())
        {
            Item->Children.Reset();
        }
    }

    TArray<TSharedPtr<FRshipTargetListItem>> NewRootItems;
    for (const TSharedPtr<FRshipTargetListItem>& Item : NewItems)
    {
        if (!Item.IsValid())
        {
            continue;
        }

        TSharedPtr<FRshipTargetListItem> ParentItem;
        for (const FString& ParentId : Item->ParentTargetIds)
        {
            const TSharedPtr<FRshipTargetListItem>* FoundParent = ItemsByFullId.Find(ParentId);
            if (FoundParent && FoundParent->IsValid() && FoundParent->Get() != Item.Get())
            {
                ParentItem = *FoundParent;
                break;
            }
        }

        if (ParentItem.IsValid())
        {
            ParentItem->Children.Add(Item);
        }
        else
        {
            NewRootItems.Add(Item);
        }
    }

    TFunction<void(TArray<TSharedPtr<FRshipTargetListItem>>&)> SortItemsRecursive;
    SortItemsRecursive = [&SortItemsRecursive](TArray<TSharedPtr<FRshipTargetListItem>>& Items)
    {
        Items.Sort([](const TSharedPtr<FRshipTargetListItem>& A, const TSharedPtr<FRshipTargetListItem>& B)
        {
            return A->DisplayName < B->DisplayName;
        });

        for (const TSharedPtr<FRshipTargetListItem>& Item : Items)
        {
            if (Item.IsValid() && Item->Children.Num() > 0)
            {
                SortItemsRecursive(Item->Children);
            }
        }
    };

    SortItemsRecursive(NewRootItems);

    TargetItems = MoveTemp(NewItems);
    RootTargetItems = MoveTemp(NewRootItems);

    TSharedPtr<FRshipTargetListItem> RestoredSelectionItem = nullptr;
    if (SelectedComponent.IsValid())
    {
        for (const TSharedPtr<FRshipTargetListItem>& Item : TargetItems)
        {
            if (Item.IsValid() && Item->Component.Get() == SelectedComponent.Get())
            {
                RestoredSelectionItem = Item;
                break;
            }
        }
    }
    if (!RestoredSelectionItem.IsValid() && !SelectedTargetId.IsEmpty())
    {
        for (const TSharedPtr<FRshipTargetListItem>& Item : TargetItems)
        {
            if (Item.IsValid() && (Item->FullTargetId == SelectedTargetId || Item->TargetId == SelectedTargetId))
            {
                RestoredSelectionItem = Item;
                SelectedComponent = Item->Component;
                break;
            }
        }
    }

    if (TargetSceneOutliner.IsValid())
    {
        TargetSceneOutliner->FullRefresh();
        if (RestoredSelectionItem.IsValid())
        {
            PendingSelectionTargetId = RestoredSelectionItem->FullTargetId;
            TryApplyPendingTargetSelection();
        }
    }

    bool bShouldRefreshActions = false;

    if (RestoredSelectionItem.IsValid())
    {
        const TWeakObjectPtr<URshipActorRegistrationComponent> ResolvedComponent = ResolveOwningComponentForTargetItem(RestoredSelectionItem);
        const AActor* ResolvedOwner = ResolvedComponent.IsValid() ? ResolvedComponent->GetOwner() : nullptr;
        bShouldRefreshActions =
            SelectedTargetComponent.Get() != ResolvedComponent.Get() ||
            SelectedTargetOwner.Get() != ResolvedOwner ||
            SelectedTargetId != RestoredSelectionItem->FullTargetId;

        SelectedTargetComponent = ResolvedComponent;
        SelectedTargetOwner = const_cast<AActor*>(ResolvedOwner);
        SelectedTargetId = RestoredSelectionItem->FullTargetId;
    }
    else if (!SelectedTargetId.IsEmpty() && !FindTargetItemByFullTargetId(SelectedTargetId).IsValid())
    {
        bShouldRefreshActions = SelectedTargetComponent.IsValid();
        SelectedTargetComponent.Reset();
        SelectedTargetOwner.Reset();
        SelectedTargetId.Empty();
    }

    if (bShouldRefreshActions)
    {
        RefreshActionsSection();
    }
}

void SRshipStatusPanel::TryApplyPendingTargetSelection()
{
    if (!TargetSceneOutliner.IsValid() || PendingSelectionTargetId.IsEmpty())
    {
        return;
    }

    const FSceneOutlinerTreeItemID PendingId(RshipStatusOutliner::MakeStableItemId(PendingSelectionTargetId));
    FSceneOutlinerTreeItemPtr PendingItem = TargetSceneOutliner->GetTreeItem(PendingId, true);
    if (!PendingItem.IsValid())
    {
        return;
    }

    TargetSceneOutliner->SetSelection([&PendingId](ISceneOutlinerTreeItem& TreeItem)
    {
        return TreeItem.GetID() == PendingId;
    });
    TargetSceneOutliner->FrameItem(PendingId);

    if (const TSharedPtr<FRshipTargetListItem> SelectedItem = FindTargetItemByFullTargetId(PendingSelectionTargetId))
    {
        OnTargetSelectionChanged(SelectedItem, ESelectInfo::Direct);
    }

    PendingSelectionTargetId.Empty();
}

TSharedPtr<FRshipTargetListItem> SRshipStatusPanel::FindTargetItemByFullTargetId(const FString& FullTargetId) const
{
    if (FullTargetId.IsEmpty())
    {
        return nullptr;
    }

    for (const TSharedPtr<FRshipTargetListItem>& Item : TargetItems)
    {
        if (Item.IsValid() && Item->FullTargetId == FullTargetId)
        {
            return Item;
        }
    }

    return nullptr;
}

TWeakObjectPtr<URshipActorRegistrationComponent> SRshipStatusPanel::ResolveOwningComponentForTargetItem(TSharedPtr<FRshipTargetListItem> Item) const
{
    if (!Item.IsValid())
    {
        return nullptr;
    }

    if (Item->Component.IsValid())
    {
        return Item->Component;
    }

    TArray<FString> PendingParents = Item->ParentTargetIds;
    TSet<FString> Visited;
    while (PendingParents.Num() > 0)
    {
        const FString ParentId = PendingParents.Pop();
        if (ParentId.IsEmpty() || Visited.Contains(ParentId))
        {
            continue;
        }

        Visited.Add(ParentId);
        const TSharedPtr<FRshipTargetListItem> ParentItem = FindTargetItemByFullTargetId(ParentId);
        if (!ParentItem.IsValid())
        {
            continue;
        }

        if (ParentItem->Component.IsValid())
        {
            return ParentItem->Component;
        }

        PendingParents.Append(ParentItem->ParentTargetIds);
    }

    return nullptr;
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
    if (ExactDroppedText.IsValid())
    {
        ExactDroppedText->SetText(FText::AsNumber(Subsystem->GetInboundExactFrameDroppedMessages()));
    }

    if (InboundFrameCounterText.IsValid())
    {
        InboundFrameCounterText->SetText(FText::AsNumber(Subsystem->GetInboundFrameCounter()));
    }

    if (InboundNextApplyFrameText.IsValid())
    {
        InboundNextApplyFrameText->SetText(FText::AsNumber(Subsystem->GetInboundNextPlannedApplyFrame()));
    }

    if (InboundQueuedFrameSpanText.IsValid())
    {
        const int32 QueueLength = Subsystem->GetInboundQueueLength();
        if (QueueLength <= 0)
        {
            InboundQueuedFrameSpanText->SetText(LOCTEXT("QueuedFrameSpanEmpty", "n/a"));
        }
        else
        {
            const int64 Oldest = Subsystem->GetInboundQueuedOldestApplyFrame();
            const int64 Newest = Subsystem->GetInboundQueuedNewestApplyFrame();
            InboundQueuedFrameSpanText->SetText(
                FText::Format(LOCTEXT("QueuedFrameSpanFmt", "{0}..{1}"),
                    FText::AsNumber(Oldest),
                    FText::AsNumber(Newest)));
        }
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

    if (InboundRequireExactFrameCheckBox.IsValid())
    {
        InboundRequireExactFrameCheckBox->SetIsChecked(MainSubsystem && MainSubsystem->IsInboundRequireExactFrame()
            ? ECheckBoxState::Checked
            : ECheckBoxState::Unchecked);
        InboundRequireExactFrameCheckBox->SetEnabled(MainSubsystem != nullptr);
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

FString SRshipStatusPanel::BuildTimingIniSnippet() const
{
    const URshipSubsystem* MainSubsystem = GetSubsystem();
    if (!MainSubsystem)
    {
        return TEXT("[/Script/RshipExec.URshipSettings]\nControlSyncRateHz=60.0\nInboundApplyLeadFrames=1\nbInboundRequireExactFrame=false");
    }

    TArray<FString> Lines;
    Lines.Add(TEXT("[/Script/RshipExec.URshipSettings]"));
    Lines.Add(FString::Printf(TEXT("ControlSyncRateHz=%s"), *FString::SanitizeFloat(MainSubsystem->GetControlSyncRateHz(), 2)));
    Lines.Add(FString::Printf(TEXT("InboundApplyLeadFrames=%d"), MainSubsystem->GetInboundApplyLeadFrames()));
    Lines.Add(FString::Printf(TEXT("bInboundRequireExactFrame=%s"),
        MainSubsystem->IsInboundRequireExactFrame() ? TEXT("true") : TEXT("false")));

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
    if (IniRolloutText.IsValid())
    {
        IniRolloutText->SetText(FText::FromString(BuildTimingIniSnippet()));
    }
}

FReply SRshipStatusPanel::OnCopyIniRolloutSnippetClicked()
{
    const FString Snippet = BuildTimingIniSnippet();
    FPlatformApplicationMisc::ClipboardCopy(*Snippet);
    SetSyncTimingStatus(LOCTEXT("RolloutIniSnippetCopied", "Timing profile copied to clipboard."), FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
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
    Settings->bInboundRequireExactFrame = MainSubsystem->IsInboundRequireExactFrame();
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

void SRshipStatusPanel::OnRequireExactFrameChanged(ECheckBoxState NewState)
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        SetSyncTimingStatus(LOCTEXT("SyncTimingStatusExactFrameUnavailable", "Cannot update exact-frame mode: Rship subsystem unavailable."), FLinearColor(1.0f, 0.35f, 0.0f, 1.0f));
        UpdateSyncSettings();
        return;
    }

    const bool bRequireExactFrame = (NewState == ECheckBoxState::Checked);
    Subsystem->SetInboundRequireExactFrame(bRequireExactFrame);

    SetSyncTimingStatus(
        FText::Format(
            LOCTEXT("SyncTimingStatusExactFrameUpdated", "Inbound exact-frame mode {0}."),
            bRequireExactFrame ? LOCTEXT("EnabledLower", "enabled") : LOCTEXT("DisabledLower", "disabled")),
        FLinearColor(0.2f, 0.85f, 0.45f, 1.0f));
    UpdateSyncSettings();
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

void SRshipStatusPanel::OnTargetSelectionChanged(TSharedPtr<FRshipTargetListItem> Item, ESelectInfo::Type SelectInfo)
{
    PendingSelectionTargetId.Empty();

    if (Item.IsValid())
    {
        const FString PreviousTargetId = SelectedTargetId;
        const TWeakObjectPtr<URshipActorRegistrationComponent> PreviousComponent = SelectedTargetComponent;
        const TWeakObjectPtr<AActor> PreviousOwner = SelectedTargetOwner;

        const TWeakObjectPtr<URshipActorRegistrationComponent> ResolvedComponent = ResolveOwningComponentForTargetItem(Item);
        const AActor* ResolvedOwner = ResolvedComponent.IsValid() ? ResolvedComponent->GetOwner() : nullptr;
        const bool bSelectionChanged =
            PreviousComponent.Get() != ResolvedComponent.Get() ||
            PreviousOwner.Get() != ResolvedOwner ||
            PreviousTargetId != Item->FullTargetId;

        SelectedTargetId = Item->FullTargetId;
        SelectedTargetComponent = ResolvedComponent;
        SelectedTargetOwner = const_cast<AActor*>(ResolvedOwner);

        if (bSelectionChanged)
        {
            RefreshActionsSection();
        }

        // Mirror panel selection to editor actor selection (for subtargets, use owning top-level actor).
        if (!bSyncingFromEditorSelection && GEditor && SelectedTargetOwner.IsValid())
        {
            bSyncingToEditorSelection = true;
            GEditor->SelectNone(false, true, false);
            GEditor->SelectActor(SelectedTargetOwner.Get(), true, true, true);
            bSyncingToEditorSelection = false;
        }
    }
    else
    {
        SelectedTargetComponent.Reset();
        SelectedTargetOwner.Reset();
        RefreshActionsSection();
    }
}

void SRshipStatusPanel::OnEditorSelectionChanged(UObject* Object)
{
    if (bSyncingToEditorSelection)
    {
        return;
    }

    // Sync our list selection when outliner selection changes
    SyncSelectionFromOutliner();
}

void SRshipStatusPanel::SyncSelectionFromOutliner()
{
    if (!GEditor)
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
        URshipActorRegistrationComponent* TargetComp = SelectedActor->FindComponentByClass<URshipActorRegistrationComponent>();
        if (!TargetComp)
        {
            continue;
        }

        // Find matching item in our list
        for (const auto& Item : TargetItems)
        {
            if (Item.IsValid() && Item->Component.Get() == TargetComp)
            {
                PendingSelectionTargetId = Item->FullTargetId;
                bSyncingFromEditorSelection = true;
                TryApplyPendingTargetSelection();
                bSyncingFromEditorSelection = false;
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

    URshipActorRegistrationComponent* TargetComponent = SelectedTargetComponent.Get();
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

    const TMap<FString, FRshipActionProxy>& Actions = TargetComponent->TargetData->GetActions();
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

    TArray<TPair<FString, FRshipActionProxy>> SortedActions;
    SortedActions.Reserve(Actions.Num());
    for (const TPair<FString, FRshipActionProxy>& Pair : Actions)
    {
        SortedActions.Add(Pair);
    }

    SortedActions.Sort([](const TPair<FString, FRshipActionProxy>& A, const TPair<FString, FRshipActionProxy>& B)
    {
        return A.Key < B.Key;
    });

    for (const TPair<FString, FRshipActionProxy>& Pair : SortedActions)
    {
        if (!Pair.Value.IsValid())
        {
            continue;
        }

        TSharedPtr<FRshipActionEntryState> Entry = MakeShared<FRshipActionEntryState>();
        Entry->ActionId = Pair.Key;
        Entry->ActionName = Pair.Value.Name;
        Entry->ActionBinding = Pair.Value;
        Entry->ParameterBag = MakeShared<FInstancedPropertyBag>();
        ActionEntries.Add(Entry);

        TSet<FName> UsedBagNames;

        const TSharedPtr<FJsonObject> Schema = Entry->ActionBinding.GetSchema();
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
    URshipActorRegistrationComponent* TargetComponent = SelectedTargetComponent.Get();
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

#undef LOCTEXT_NAMESPACE

