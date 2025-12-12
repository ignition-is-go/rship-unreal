// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FRshipExecEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** Get the module instance */
    static FRshipExecEditorModule& Get();

    /** Register the status panel tab spawner */
    void RegisterStatusPanel();

    /** Unregister the status panel tab spawner */
    void UnregisterStatusPanel();

private:
    /** Handle for the status panel tab spawner */
    TSharedPtr<class FUICommandList> PluginCommands;

    /** Spawn the status panel tab */
    TSharedRef<class SDockTab> SpawnStatusPanelTab(const class FSpawnTabArgs& Args);

    /** Register menu extensions */
    void RegisterMenus();
};
