// Copyright Epic Games, Inc. All Rights Reserved.

#include "RshipExec.h"
#include "ISettingsModule.h"
#include "RshipSettings.h"

#define LOCTEXT_NAMESPACE "FRshipExecModule"

void FRshipExecModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	if (ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "Rship Exec",
										 LOCTEXT("RshipExecSettingsName", "Rship Exec"),
										 LOCTEXT("RshipExecSettingsDescription", "Settings for Rship Exec"),
										 GetMutableDefault<URshipSettings>());
	}
}

void FRshipExecModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Rship Exec");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipExecModule, RshipExec)