#include "RshipMappingEditor.h"
#include "SRshipContentMappingPanel.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FRshipMappingEditorModule"

static const FName RshipContentMappingPanelTabName("RshipContentMappingPanel");

void FRshipMappingEditorModule::StartupModule()
{
	RegisterContentMappingPanel();
}

void FRshipMappingEditorModule::ShutdownModule()
{
	UnregisterContentMappingPanel();
}

void FRshipMappingEditorModule::RegisterContentMappingPanel()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		RshipContentMappingPanelTabName,
		FOnSpawnTab::CreateRaw(this, &FRshipMappingEditorModule::SpawnContentMappingPanelTab))
		.SetDisplayName(LOCTEXT("RshipContentMappingPanelTabTitle", "Rship Content Mapping"))
		.SetTooltipText(LOCTEXT("RshipContentMappingPanelTooltip", "Open Rocketship Content Mapping Panel"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());
}

void FRshipMappingEditorModule::UnregisterContentMappingPanel()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipContentMappingPanelTabName);
}

TSharedRef<SDockTab> FRshipMappingEditorModule::SpawnContentMappingPanelTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SRshipContentMappingPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipMappingEditorModule, RshipMappingEditor)
