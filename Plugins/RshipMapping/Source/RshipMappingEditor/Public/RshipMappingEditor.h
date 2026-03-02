#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FRshipMappingEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterContentMappingPanel();
	void UnregisterContentMappingPanel();
	TSharedRef<class SDockTab> SpawnContentMappingPanelTab(const class FSpawnTabArgs& Args);
};
