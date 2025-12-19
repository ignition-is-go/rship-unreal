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

    /** Register the timecode panel tab spawner */
    void RegisterTimecodePanel();

    /** Unregister the timecode panel tab spawner */
    void UnregisterTimecodePanel();

    /** Register the LiveLink panel tab spawner */
    void RegisterLiveLinkPanel();

    /** Unregister the LiveLink panel tab spawner */
    void UnregisterLiveLinkPanel();

    /** Register the Material panel tab spawner */
    void RegisterMaterialPanel();

    /** Unregister the Material panel tab spawner */
    void UnregisterMaterialPanel();

    /** Register the Asset Sync panel tab spawner */
    void RegisterAssetSyncPanel();

    /** Unregister the Asset Sync panel tab spawner */
    void UnregisterAssetSyncPanel();

    /** Register the Fixture panel tab spawner */
    void RegisterFixturePanel();

    /** Unregister the Fixture panel tab spawner */
    void UnregisterFixturePanel();

    /** Register the Test panel tab spawner */
    void RegisterTestPanel();

    /** Unregister the Test panel tab spawner */
    void UnregisterTestPanel();

    /** Register the NDI panel tab spawner */
    void RegisterNDIPanel();

    /** Unregister the NDI panel tab spawner */
    void UnregisterNDIPanel();

private:
    /** Handle for the status panel tab spawner */
    TSharedPtr<class FUICommandList> PluginCommands;

    /** Spawn the status panel tab */
    TSharedRef<class SDockTab> SpawnStatusPanelTab(const class FSpawnTabArgs& Args);

    /** Spawn the timecode panel tab */
    TSharedRef<class SDockTab> SpawnTimecodePanelTab(const class FSpawnTabArgs& Args);

    /** Spawn the LiveLink panel tab */
    TSharedRef<class SDockTab> SpawnLiveLinkPanelTab(const class FSpawnTabArgs& Args);

    /** Spawn the Material panel tab */
    TSharedRef<class SDockTab> SpawnMaterialPanelTab(const class FSpawnTabArgs& Args);

    /** Spawn the Asset Sync panel tab */
    TSharedRef<class SDockTab> SpawnAssetSyncPanelTab(const class FSpawnTabArgs& Args);

    /** Spawn the Fixture panel tab */
    TSharedRef<class SDockTab> SpawnFixturePanelTab(const class FSpawnTabArgs& Args);

    /** Spawn the Test panel tab */
    TSharedRef<class SDockTab> SpawnTestPanelTab(const class FSpawnTabArgs& Args);

    /** Spawn the NDI panel tab */
    TSharedRef<class SDockTab> SpawnNDIPanelTab(const class FSpawnTabArgs& Args);

    /** Register menu extensions */
    void RegisterMenus();
};
