// Copyright Lucid. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRshipNDI, Log, All);

class FRshipNDIModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Check if the Rust NDI sender library is available.
	 * If false, streaming will not work and a warning should be shown.
	 */
	static bool IsNDISenderAvailable();

	/**
	 * Get the singleton instance of this module.
	 */
	static FRshipNDIModule& Get();

	/**
	 * Check if this module is loaded and available.
	 */
	static bool IsAvailable();
};
