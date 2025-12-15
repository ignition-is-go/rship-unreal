// Copyright Lucid. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRshipColor, Log, All);

class FRshipColorManagementModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the singleton instance of this module. */
	static FRshipColorManagementModule& Get();

	/** Check if this module is loaded and available. */
	static bool IsAvailable();
};
