// Copyright Rocketship. All Rights Reserved.

#include "RshipExecEditor.h"
#include "SRshipStatusPanel.h"
#include "SRshipTimecodePanel.h"
#include "SRshipLiveLinkPanel.h"
#include "SRshipMaterialPanel.h"
#include "SRshipAssetSyncPanel.h"
#include "SRshipFixturePanel.h"
#include "SRshipTestPanel.h"
#include "SRshipNDIPanel.h"
#include "SRshipContentMappingPanel.h"
#if RSHIP_EDITOR_HAS_2110
#include "SRship2110MappingPanel.h"
#endif
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
static const FName RshipTimecodePanelTabName("RshipTimecodePanel");
static const FName RshipLiveLinkPanelTabName("RshipLiveLinkPanel");
static const FName RshipMaterialPanelTabName("RshipMaterialPanel");
static const FName RshipAssetSyncPanelTabName("RshipAssetSyncPanel");
static const FName RshipFixturePanelTabName("RshipFixturePanel");
static const FName RshipTestPanelTabName("RshipTestPanel");
static const FName RshipNDIPanelTabName("RshipNDIPanel");
static const FName RshipContentMappingPanelTabName("RshipContentMappingPanel");
static const FName Rship2110MappingPanelTabName("Rship2110MappingPanel");

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
    RegisterTimecodePanel();
    RegisterLiveLinkPanel();
    RegisterMaterialPanel();
    RegisterAssetSyncPanel();
    RegisterFixturePanel();
    RegisterTestPanel();
    RegisterNDIPanel();
    RegisterContentMappingPanel();
#if RSHIP_EDITOR_HAS_2110
    Register2110MappingPanel();
#endif

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
    UnregisterTimecodePanel();
    UnregisterLiveLinkPanel();
    UnregisterMaterialPanel();
    UnregisterAssetSyncPanel();
    UnregisterFixturePanel();
    UnregisterTestPanel();
    UnregisterNDIPanel();
    UnregisterContentMappingPanel();
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

void FRshipExecEditorModule::RegisterTimecodePanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipTimecodePanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnTimecodePanelTab))
        .SetDisplayName(LOCTEXT("RshipTimecodePanelTabTitle", "Rship Timecode"))
        .SetTooltipText(LOCTEXT("RshipTimecodePanelTooltip", "Open Rocketship Timecode Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterTimecodePanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipTimecodePanelTabName);
}

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnTimecodePanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipTimecodePanel)
        ];
}

void FRshipExecEditorModule::RegisterLiveLinkPanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipLiveLinkPanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnLiveLinkPanelTab))
        .SetDisplayName(LOCTEXT("RshipLiveLinkPanelTabTitle", "Rship LiveLink"))
        .SetTooltipText(LOCTEXT("RshipLiveLinkPanelTooltip", "Open Rocketship LiveLink Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterLiveLinkPanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipLiveLinkPanelTabName);
}

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnLiveLinkPanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipLiveLinkPanel)
        ];
}

void FRshipExecEditorModule::RegisterMaterialPanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipMaterialPanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnMaterialPanelTab))
        .SetDisplayName(LOCTEXT("RshipMaterialPanelTabTitle", "Rship Materials"))
        .SetTooltipText(LOCTEXT("RshipMaterialPanelTooltip", "Open Rocketship Material Binding Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterMaterialPanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipMaterialPanelTabName);
}

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnMaterialPanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipMaterialPanel)
        ];
}

void FRshipExecEditorModule::RegisterAssetSyncPanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipAssetSyncPanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnAssetSyncPanelTab))
        .SetDisplayName(LOCTEXT("RshipAssetSyncPanelTabTitle", "Rship Assets"))
        .SetTooltipText(LOCTEXT("RshipAssetSyncPanelTooltip", "Open Rocketship Asset Sync Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterAssetSyncPanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipAssetSyncPanelTabName);
}

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnAssetSyncPanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipAssetSyncPanel)
        ];
}

void FRshipExecEditorModule::RegisterFixturePanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipFixturePanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnFixturePanelTab))
        .SetDisplayName(LOCTEXT("RshipFixturePanelTabTitle", "Rship Fixtures"))
        .SetTooltipText(LOCTEXT("RshipFixturePanelTooltip", "Open Rocketship Fixture Library Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterFixturePanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipFixturePanelTabName);
}

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnFixturePanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipFixturePanel)
        ];
}

void FRshipExecEditorModule::RegisterTestPanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipTestPanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnTestPanelTab))
        .SetDisplayName(LOCTEXT("RshipTestPanelTabTitle", "Rship Testing"))
        .SetTooltipText(LOCTEXT("RshipTestPanelTooltip", "Open Rocketship Testing & Validation Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterTestPanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipTestPanelTabName);
}

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnTestPanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipTestPanel)
        ];
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

void FRshipExecEditorModule::RegisterContentMappingPanel()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RshipContentMappingPanelTabName,
        FOnSpawnTab::CreateRaw(this, &FRshipExecEditorModule::SpawnContentMappingPanelTab))
        .SetDisplayName(LOCTEXT("RshipContentMappingPanelTabTitle", "Rship Content Mapping"))
        .SetTooltipText(LOCTEXT("RshipContentMappingPanelTooltip", "Open Rocketship Content Mapping Panel"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FRshipStatusPanelStyle::GetStyleSetName(), "Rship.StatusPanel.TabIcon"));
}

void FRshipExecEditorModule::UnregisterContentMappingPanel()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipContentMappingPanelTabName);
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

TSharedRef<SDockTab> FRshipExecEditorModule::SpawnContentMappingPanelTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SRshipContentMappingPanel)
        ];
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
