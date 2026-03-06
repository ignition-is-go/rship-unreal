// Copyright Rocketship. All Rights Reserved.

#include "RshipExecEditor.h"
#include "SRshipStatusPanel.h"
#include "SRshipNDIPanel.h"
#if RSHIP_EDITOR_HAS_2110
#include "SRship2110MappingPanel.h"
#endif
#include "RshipStatusPanelStyle.h"
#include "RshipStatusPanelCommands.h"
#include "RshipSubsystem.h"
#include "RshipActorRegistrationComponent.h"
#include "RshipTargetIdOutlinerColumn.h"

#include "LevelEditor.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "ClassViewerModule.h"
#include "Kismet2/SClassPickerDialog.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"

#define LOCTEXT_NAMESPACE "FRshipExecEditorModule"

static const FName RshipStatusPanelTabName("RshipStatusPanel");
static const FName RshipNDIPanelTabName("RshipNDIPanel");
static const FName RshipToolbarEntryName("RshipStatusToolbarButton");

void FRshipExecEditorModule::StartupModule()
{
    // Initialize style
    FRshipStatusPanelStyle::Initialize();
    FRshipStatusPanelStyle::ReloadTextures();

    // Initialize commands
    FRshipStatusPanelCommands::Register();

    FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
    SceneOutlinerModule.RegisterDefaultColumnType<FRshipTargetIdOutlinerColumn>(
        FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Invisible, 12, FCreateSceneOutlinerColumn(), true, TOptional<float>(), TAttribute<FText>(LOCTEXT("RshipTargetIdColumnDisplayName", "Rship Target ID"))));

    PluginCommands = MakeShareable(new FUICommandList);

    // Register panels
    RegisterStatusPanel();
    RegisterNDIPanel();
#if RSHIP_EDITOR_HAS_2110
    Register2110MappingPanel();
#endif

    // Register menus after ToolMenus is ready
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRshipExecEditorModule::RegisterMenus));

    bLastToolbarConnectedState = false;
    ToolbarStatusTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FRshipExecEditorModule::OnToolbarStatusTick),
        0.25f
    );
}

void FRshipExecEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    if (ToolbarStatusTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ToolbarStatusTickerHandle);
        ToolbarStatusTickerHandle.Reset();
    }

    if (FModuleManager::Get().IsModuleLoaded("SceneOutliner"))
    {
        FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::GetModuleChecked<FSceneOutlinerModule>("SceneOutliner");
        SceneOutlinerModule.UnRegisterColumnType<FRshipTargetIdOutlinerColumn>();
    }

    FRshipStatusPanelCommands::Unregister();
    FRshipStatusPanelStyle::Shutdown();

    UnregisterStatusPanel();
    UnregisterNDIPanel();
#if RSHIP_EDITOR_HAS_2110
    Unregister2110MappingPanel();
#endif
}

FRshipExecEditorModule& FRshipExecEditorModule::Get()
{
    return FModuleManager::LoadModuleChecked<FRshipExecEditorModule>("RshipExecEditor");
}

void FRshipExecEditorModule::RegisterStatusPanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipStatusPanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnStatusPanelTab))
        .SetDisplayName(LOCTEXT("RshipStatusPanelTabTitle", "Rocketship"))
        .SetTooltipText(LOCTEXT("RshipStatusPanelTooltip", "Open Rocketship Status Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterStatusPanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipStatusPanelTabName);
}

void FRshipExecEditorModule::RegisterNDIPanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipNDIPanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnNDIPanelTab))
        .SetDisplayName(LOCTEXT("RshipNDIPanelTabTitle", "Rship NDI"))
        .SetTooltipText(LOCTEXT("RshipNDIPanelTooltip", "Open Rocketship NDI Streaming Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterNDIPanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipNDIPanelTabName);
}

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnNDIPanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipNDIPanel)
        ];
}

void FRshipExecEditorModule::Register2110MappingPanel()
{
#if RSHIP_EDITOR_HAS_2110
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(Rship2110MappingPanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::Spawn2110MappingPanelTab))
        .SetDisplayName(LOCTEXT("Rship2110MappingPanelTabTitle", "Rship 2110 Mapping"))
        .SetTooltipText(LOCTEXT("Rship2110MappingPanelTooltip", "Open Rocketship SMPTE 2110 Mapping Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
#endif
}

void FRshipExecEditorModule::Unregister2110MappingPanel()
{
#if RSHIP_EDITOR_HAS_2110
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Rship2110MappingPanelTabName);
#endif
}

TSharedRef<SDockTab> FRshipExecEditorModule::Spawn2110MappingPanelTab(const FSpawnTabArgs& Args)
{
#if RSHIP_EDITOR_HAS_2110
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRship2110MappingPanel)
        ];
#else
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock).Text(LOCTEXT("Rship2110MappingUnavailable", "Rship 2110 plugin is not available."))
            ]
        ];
#endif
}

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnStatusPanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipStatusPanel)
        ];
}

void FRshipExecEditorModule::UpdateToolbarStatusIcon(bool bConnected)
{
    UToolMenus* ToolMenus = UToolMenus::Get();
    if (!ToolMenus)
    {
        return;
    }

    UToolMenu* ToolbarMenu = ToolMenus->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
    if (!ToolbarMenu)
    {
        return;
    }

    FToolMenuSection* Section = ToolbarMenu->FindSection("PluginTools");
    if (!Section)
    {
        return;
    }

    FToolMenuEntry* Entry = Section->FindEntry(RshipToolbarEntryName);
    if (!Entry)
    {
        return;
    }

    Entry->Icon = FSlateIcon(
        FRshipStatusPanelStyle::GetStyleSetName(),
        bConnected ? "Rship.StatusPanel.ToolbarIcon.Connected" : "Rship.StatusPanel.ToolbarIcon.Disconnected"
    );

    ToolMenus->RefreshAllWidgets();
}

bool FRshipExecEditorModule::OnToolbarStatusTick(float DeltaTime)
{
    URshipSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
    const bool bConnected = Subsystem && Subsystem->IsConnected();

    if (bConnected != bLastToolbarConnectedState)
    {
        bLastToolbarConnectedState = bConnected;
        UpdateToolbarStatusIcon(bConnected);
    }

    return true;
}

void FRshipExecEditorModule::RegisterMenus()
{
    // Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
    FToolMenuOwnerScoped OwnerScoped(this);

    // Add to Window menu
    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
        FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
        Section.AddMenuEntryWithCommandList(
            FRshipStatusPanelCommands::Get().OpenStatusPanel,
            PluginCommands,
            LOCTEXT("RshipStatusPanelMenuLabel", "Rocketship"),
            LOCTEXT("RshipStatusPanelMenuTooltip", "Open the Rocketship Status Panel"),
            FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon")
        );
    }

    // Add toolbar button
    {
        UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
        FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");

        FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(
            FRshipStatusPanelCommands::Get().OpenStatusPanel,
            LOCTEXT("RshipToolbarButton", "Rship"),
            LOCTEXT("RshipToolbarTooltip", "Open Rocketship Status Panel"),
            FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.ToolbarIcon.Disconnected")
        ));
        Entry.SetCommandList(PluginCommands);
        Entry.Name = RshipToolbarEntryName;
    }

    // Bind the command
    PluginCommands->MapAction(
        FRshipStatusPanelCommands::Get().OpenStatusPanel,
        FExecuteAction::CreateLambda([]()
        {
            FGlobalTabmanager::Get()->TryInvokeTab(RshipStatusPanelTabName);
        }),
        FCanExecuteAction());

    RegisterActorContextMenu();
}

void FRshipExecEditorModule::RegisterActorContextMenu()
{
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
    if (!Menu)
    {
        return;
    }

    FToolMenuSection& Section = Menu->FindOrAddSection("RSHIP");
    Section.Label = LOCTEXT("RshipActorContextSectionLabel", "RSHIP");

    Section.AddMenuEntry(
        "RshipAddRegistrationComponent",
        LOCTEXT("RshipAddRegistrationComponentLabel", "Add Rship Actor Registration Component"),
        LOCTEXT("RshipAddRegistrationComponentTooltip", "Add RshipActorRegistrationComponent to all selected actors."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateRaw(this, &FRshipExecEditorModule::AddRshipRegistrationToSelectedActors),
            FCanExecuteAction::CreateRaw(this, &FRshipExecEditorModule::CanAddRshipRegistrationToSelectedActors))
    );

    Section.AddMenuEntry(
        "RshipAddComponentByClass",
        LOCTEXT("RshipAddComponentByClassLabel", "Add Component..."),
        LOCTEXT("RshipAddComponentByClassTooltip", "Search for a component class and add it to all selected actors."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateRaw(this, &FRshipExecEditorModule::AddComponentClassToSelectedActors),
            FCanExecuteAction::CreateRaw(this, &FRshipExecEditorModule::CanAddComponentClassToSelectedActors))
    );

    Section.AddMenuEntry(
        "RshipRemoveComponentByClass",
        LOCTEXT("RshipRemoveComponentByClassLabel", "Remove Component..."),
        LOCTEXT("RshipRemoveComponentByClassTooltip", "Search for a component class and remove it from all selected actors."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateRaw(this, &FRshipExecEditorModule::RemoveComponentClassFromSelectedActors),
            FCanExecuteAction::CreateRaw(this, &FRshipExecEditorModule::CanRemoveComponentClassFromSelectedActors))
    );
}

void FRshipExecEditorModule::AddRshipRegistrationToSelectedActors()
{
    AddComponentToSelectedActors(URshipActorRegistrationComponent::StaticClass(), true);
}

bool FRshipExecEditorModule::CanAddRshipRegistrationToSelectedActors() const
{
    return HasEligibleSelectedActors(URshipActorRegistrationComponent::StaticClass(), true);
}

void FRshipExecEditorModule::AddComponentClassToSelectedActors()
{
    FClassViewerInitializationOptions Options;
    Options.Mode = EClassViewerMode::ClassPicker;
    Options.DisplayMode = EClassViewerDisplayMode::TreeView;
    Options.bShowObjectRootClass = false;
    Options.bShowNoneOption = false;
    Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::Dynamic;

    UClass* PickedClass = nullptr;
    const bool bPicked = SClassPickerDialog::PickClass(
        LOCTEXT("RshipPickComponentClassTitle", "Pick Component Class"),
        Options,
        PickedClass,
        UActorComponent::StaticClass());

    if (!bPicked || !PickedClass || !PickedClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return;
    }

    AddComponentToSelectedActors(PickedClass, true);
}

bool FRshipExecEditorModule::CanAddComponentClassToSelectedActors() const
{
    return HasEligibleSelectedActors(UActorComponent::StaticClass(), false);
}

void FRshipExecEditorModule::RemoveComponentClassFromSelectedActors()
{
    FClassViewerInitializationOptions Options;
    Options.Mode = EClassViewerMode::ClassPicker;
    Options.DisplayMode = EClassViewerDisplayMode::TreeView;
    Options.bShowObjectRootClass = false;
    Options.bShowNoneOption = false;
    Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::Dynamic;

    UClass* PickedClass = nullptr;
    const bool bPicked = SClassPickerDialog::PickClass(
        LOCTEXT("RshipPickComponentClassToRemoveTitle", "Pick Component Class To Remove"),
        Options,
        PickedClass,
        UActorComponent::StaticClass());

    if (!bPicked || !PickedClass || !PickedClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return;
    }

    RemoveComponentFromSelectedActors(PickedClass);
}

bool FRshipExecEditorModule::CanRemoveComponentClassFromSelectedActors() const
{
    return HasEligibleSelectedActors(UActorComponent::StaticClass(), false);
}

bool FRshipExecEditorModule::HasEligibleSelectedActors(TSubclassOf<UActorComponent> ComponentClass, bool bSkipIfAlreadyPresent) const
{
    if (!GEditor || !*ComponentClass)
    {
        return false;
    }

    USelection* Selection = GEditor->GetSelectedActors();
    if (!Selection || Selection->Num() == 0)
    {
        return false;
    }

    for (FSelectionIterator It(*Selection); It; ++It)
    {
        AActor* Actor = Cast<AActor>(*It);
        if (!IsValid(Actor) || Actor->IsTemplate())
        {
            continue;
        }
        if (bSkipIfAlreadyPresent && Actor->FindComponentByClass(ComponentClass))
        {
            continue;
        }

        return true;
    }

    return false;
}

int32 FRshipExecEditorModule::AddComponentToSelectedActors(TSubclassOf<UActorComponent> ComponentClass, bool bSkipIfAlreadyPresent) const
{
    if (!HasEligibleSelectedActors(ComponentClass, bSkipIfAlreadyPresent) || !GEditor)
    {
        return 0;
    }

    USelection* Selection = GEditor->GetSelectedActors();
    if (!Selection)
    {
        return 0;
    }

    const FScopedTransaction Transaction(LOCTEXT("AddComponentToSelectedActorsTx", "Add Component to Selected Actors"));
    int32 AddedCount = 0;

    for (FSelectionIterator It(*Selection); It; ++It)
    {
        AActor* Actor = Cast<AActor>(*It);
        if (!IsValid(Actor) || Actor->IsTemplate())
        {
            continue;
        }

        if (bSkipIfAlreadyPresent && Actor->FindComponentByClass(ComponentClass))
        {
            continue;
        }

        Actor->Modify();

        UActorComponent* NewComponent = NewObject<UActorComponent>(Actor, ComponentClass, NAME_None, RF_Transactional);
        if (!NewComponent)
        {
            continue;
        }

        Actor->AddInstanceComponent(NewComponent);
        NewComponent->OnComponentCreated();
        NewComponent->RegisterComponent();
        Actor->RerunConstructionScripts();
        Actor->MarkPackageDirty();
        ++AddedCount;
    }

    if (AddedCount > 0)
    {
        GEditor->NoteSelectionChange();
        GEditor->RedrawLevelEditingViewports();
    }

    return AddedCount;
}

int32 FRshipExecEditorModule::RemoveComponentFromSelectedActors(TSubclassOf<UActorComponent> ComponentClass) const
{
    if (!GEditor || !*ComponentClass)
    {
        return 0;
    }

    USelection* Selection = GEditor->GetSelectedActors();
    if (!Selection || Selection->Num() == 0)
    {
        return 0;
    }

    const FScopedTransaction Transaction(LOCTEXT("RemoveComponentFromSelectedActorsTx", "Remove Component from Selected Actors"));
    int32 RemovedCount = 0;

    for (FSelectionIterator It(*Selection); It; ++It)
    {
        AActor* Actor = Cast<AActor>(*It);
        if (!IsValid(Actor) || Actor->IsTemplate())
        {
            continue;
        }

        TArray<UActorComponent*> ComponentsToRemove;
        Actor->GetComponents(ComponentClass, ComponentsToRemove);
        if (ComponentsToRemove.Num() == 0)
        {
            continue;
        }

        Actor->Modify();

        for (UActorComponent* Component : ComponentsToRemove)
        {
            if (!IsValid(Component))
            {
                continue;
            }

            Component->Modify();
            Actor->RemoveInstanceComponent(Component);
            Component->DestroyComponent();
            ++RemovedCount;
        }

        Actor->MarkPackageDirty();
    }

    if (RemovedCount > 0)
    {
        GEditor->NoteSelectionChange();
        GEditor->RedrawLevelEditingViewports();
    }

    return RemovedCount;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipExecEditorModule, RshipExecEditor)



