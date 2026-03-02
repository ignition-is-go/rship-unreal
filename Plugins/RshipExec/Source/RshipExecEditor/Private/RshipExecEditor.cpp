// Copyright Rocketship. All Rights Reserved.

#include "RshipExecEditor.h"
#include "SRshipStatusPanel.h"
#include "SRshipNDIPanel.h"
#include "RshipStatusPanelStyle.h"
#include "RshipStatusPanelCommands.h"
#include "RshipSubsystem.h"

#include "LevelEditor.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Engine/Engine.h"

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

    PluginCommands = MakeShareable(new FUICommandList);

    // Register panels
    RegisterStatusPanel();
    RegisterNDIPanel();

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

    FRshipStatusPanelCommands::Unregister();
    FRshipStatusPanelStyle::Shutdown();

    UnregisterStatusPanel();
    UnregisterNDIPanel();
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
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipExecEditorModule, RshipExecEditor)
