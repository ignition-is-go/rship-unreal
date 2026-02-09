// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogConcertHostIdentity, Log, All);

class FConcertHostIdentityModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Deterministic color from a hostname string (case-insensitive) */
	static FLinearColor ColorFromHostname(const FString& Hostname);
};
