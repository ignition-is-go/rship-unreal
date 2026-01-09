// Copyright Epic Games, Inc. All Rights Reserved.

#include "RshipExec.h"
#include "ISettingsModule.h"
#include "RshipSettings.h"
#include "RshipSubsystem.h"

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

	// After hot reload, reinitialize the subsystem's tickers and connections
	// GEngine may not exist yet on initial load, but will exist after hot reload
	if (GEngine)
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			// Re-run initialization to set up tickers that were cleared before hot reload
			UE_LOG(LogTemp, Log, TEXT("RshipExec: Re-initializing subsystem after hot reload"));
			Subsystem->ReinitializeAfterHotReload();
		}
	}

// Dashboard panel now registered in RshipExecEditor module (SRshipStatusPanel)
}

void FRshipExecModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// CRITICAL: Clean up the subsystem before module unload (especially for live coding)
	// This prevents crashes when the new module loads with stale ticker delegates
	if (GEngine)
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			Subsystem->PrepareForHotReload();
		}
	}

	if (ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Rship Exec");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipExecModule, RshipExec)