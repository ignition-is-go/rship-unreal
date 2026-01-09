// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

RSHIPSPATIALAUDIORUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogRshipSpatialAudio, Log, All);

/**
 * Runtime module for Rship Spatial Audio system.
 * Provides spatial rendering, loudspeaker management, and DSP processing.
 */
class FRshipSpatialAudioRuntimeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.
	 * Beware of calling this during shutdown - module may be unloaded.
	 */
	static inline FRshipSpatialAudioRuntimeModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FRshipSpatialAudioRuntimeModule>("RshipSpatialAudioRuntime");
	}

	/**
	 * Checks if this module is loaded and ready.
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("RshipSpatialAudioRuntime");
	}
};
