// Copyright Epic Games, Inc. All Rights Reserved.

#include "RshipExec.h"
#include "ISettingsModule.h"
#include "RshipSettings.h"
#include "Logs.h"

// Old RshipEditorWidget.h dashboard deprecated - using RshipExecEditor's SRshipStatusPanel instead

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

// Dashboard panel now registered in RshipExecEditor module (SRshipStatusPanel)

#if RSHIP_HAS_DISPLAY_RUST
	UE_LOG(LogRshipExec, Log, TEXT("Rship display Rust runtime available (RSHIP_HAS_DISPLAY_RUST=1)"));
#else
	UE_LOG(LogRshipExec, Warning, TEXT("Rship display Rust runtime not found (RSHIP_HAS_DISPLAY_RUST=0)"));
	UE_LOG(LogRshipExec, Warning, TEXT("Build optional runtime at Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display"));
#endif
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
