// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ComponentVisualizer.h"

/**
 * Editor module for Rship Spatial Audio system.
 * Provides speaker layout editor, routing matrix, DSP controls, and visualization.
 */
class FRshipSpatialAudioEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.
	 */
	static inline FRshipSpatialAudioEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FRshipSpatialAudioEditorModule>("RshipSpatialAudioEditor");
	}

	/**
	 * Checks if this module is loaded and ready.
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("RshipSpatialAudioEditor");
	}

private:
	void RegisterMenus();
	void UnregisterMenus();

	TSharedPtr<class FUICommandList> PluginCommands;

	/** Registered component visualizers */
	TArray<TSharedPtr<FComponentVisualizer>> RegisteredVisualizers;
};
