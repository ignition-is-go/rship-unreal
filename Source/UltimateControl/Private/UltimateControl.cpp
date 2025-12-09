// Copyright Rocketship. All Rights Reserved.

#include "UltimateControl.h"
#include "UltimateControlSettings.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif

#define LOCTEXT_NAMESPACE "FUltimateControlModule"

DEFINE_LOG_CATEGORY(LogUltimateControl);

void FUltimateControlModule::StartupModule()
{
	UE_LOG(LogUltimateControl, Log, TEXT("UltimateControl module starting up..."));

#if WITH_EDITOR
	// Register settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Project",
			"Plugins",
			"UltimateControl",
			LOCTEXT("RuntimeSettingsName", "Ultimate Control"),
			LOCTEXT("RuntimeSettingsDescription", "Configure the Ultimate Control HTTP API server"),
			GetMutableDefault<UUltimateControlSettings>()
		);
	}
#endif

	UE_LOG(LogUltimateControl, Log, TEXT("UltimateControl module started successfully"));
}

void FUltimateControlModule::ShutdownModule()
{
	UE_LOG(LogUltimateControl, Log, TEXT("UltimateControl module shutting down..."));

#if WITH_EDITOR
	// Unregister settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "UltimateControl");
	}
#endif

	UE_LOG(LogUltimateControl, Log, TEXT("UltimateControl module shut down successfully"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUltimateControlModule, UltimateControl)
