// Copyright Rocketship. All Rights Reserved.

#include "RshipExecEditor.h"
#include "SRshipStatusPanel.h"
#include "RshipStatusPanelStyle.h"
#include "RshipStatusPanelCommands.h"

#include "LevelEditor.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"

#define LOCTEXT_NAMESPACE "FRshipExecEditorModule"

static const FName RshipStatusPanelTabName("RshipStatusPanel");

void FRshipExecEditorModule::StartupModule()
{
    // Initialize style
    FRshipStatusPanelStyle::Initialize();
    FRshipStatusPanelStyle::ReloadTextures();

    // Initialize commands
    FRshipStatusPanelCommands::Register();

    PluginCommands = MakeShareable(new FUICommandList);

    // Register the status panel
    RegisterStatusPanel();

    // Register menus after ToolMenus is ready
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRshipExecEditorModule::RegisterMenus));
}

void FRshipExecEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    FRshipStatusPanelCommands::Unregister();
    FRshipStatusPanelStyle::Shutdown();

    UnregisterStatusPanel();
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

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnStatusPanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipStatusPanel)
        ];
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
            FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.ToolbarIcon")
        ));
        Entry.SetCommandList(PluginCommands);
    }

    // Bind the command
    PluginCommands->MapAction(
        FRshipStatusPanelCommands::Get().OpenStatusPanel,
        FExecuteAction::CreateLambda([]()
        {
            FGlobalTabmanager::Get()->TryInvokeTab(RshipStatusPanelTabName);
        }),
        FCanExecuteAction());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipExecEditorModule, RshipExecEditor)
