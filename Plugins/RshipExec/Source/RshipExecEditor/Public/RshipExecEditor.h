// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

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

    /** Register the NDI panel tab spawner */
    void RegisterNDIPanel();

    /** Unregister the NDI panel tab spawner */
    void UnregisterNDIPanel();

private:
    void RegisterActorContextMenu();
    void AddRshipRegistrationToSelectedActors();
    bool CanAddRshipRegistrationToSelectedActors() const;
    void AddComponentClassToSelectedActors();
    bool CanAddComponentClassToSelectedActors() const;
    void RemoveComponentClassFromSelectedActors();
    bool CanRemoveComponentClassFromSelectedActors() const;
    bool HasEligibleSelectedActors(TSubclassOf<class UActorComponent> ComponentClass, bool bSkipIfAlreadyPresent = true) const;
    int32 AddComponentToSelectedActors(TSubclassOf<class UActorComponent> ComponentClass, bool bSkipIfAlreadyPresent = true) const;
    int32 RemoveComponentFromSelectedActors(TSubclassOf<class UActorComponent> ComponentClass) const;

    /** Handle for the status panel tab spawner */
    TSharedPtr<class FUICommandList> PluginCommands;

    /** Spawn the status panel tab */
    TSharedRef<class SDockTab> SpawnStatusPanelTab(const class FSpawnTabArgs& Args);

    /** Spawn the NDI panel tab */
    TSharedRef<class SDockTab> SpawnNDIPanelTab(const class FSpawnTabArgs& Args);

    /** Register the 2110 mapping panel tab spawner */
    void Register2110MappingPanel();

    /** Unregister the 2110 mapping panel tab spawner */
    void Unregister2110MappingPanel();

    /** Spawn the 2110 mapping panel tab */
    TSharedRef<class SDockTab> Spawn2110MappingPanelTab(const class FSpawnTabArgs& Args);

    /** Register menu extensions */
    void UpdateToolbarStatusIcon(bool bConnected);
    bool OnToolbarStatusTick(float DeltaTime);

    FTSTicker::FDelegateHandle ToolbarStatusTickerHandle;
    bool bLastToolbarConnectedState = false;

    void RegisterMenus();
};
