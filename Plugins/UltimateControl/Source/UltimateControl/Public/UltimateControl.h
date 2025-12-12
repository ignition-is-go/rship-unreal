// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUltimateControl, Log, All);

class FUltimateControlModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Get the singleton instance of this module
	 */
	static FUltimateControlModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUltimateControlModule>("UltimateControl");
	}

	/**
	 * Check if the module is loaded
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UltimateControl");
	}
};
