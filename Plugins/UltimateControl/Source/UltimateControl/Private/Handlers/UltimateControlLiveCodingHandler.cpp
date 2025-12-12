// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlLiveCodingHandler.h"
#include "Modules/ModuleManager.h"

// Live Coding module is optional - check if header exists
#if __has_include("ILiveCodingModule.h")
	#include "ILiveCodingModule.h"
	#define ULTIMATE_CONTROL_HAS_LIVE_CODING 1
#else
	#define ULTIMATE_CONTROL_HAS_LIVE_CODING 0
#endif

// Hot Reload was removed in UE 5.6 - Live Coding is the replacement
// Disable hot reload functionality in modern UE versions
#define ULTIMATE_CONTROL_HAS_HOT_RELOAD 0

FUltimateControlLiveCodingHandler::FUltimateControlLiveCodingHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// Live Coding
	RegisterMethod(TEXT("liveCoding.isEnabled"), TEXT("Check if live coding is enabled"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleIsLiveCodingEnabled));
	RegisterMethod(TEXT("liveCoding.enable"), TEXT("Enable live coding"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleEnableLiveCoding));
	RegisterMethod(TEXT("liveCoding.disable"), TEXT("Disable live coding"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleDisableLiveCoding));
	RegisterMethod(TEXT("liveCoding.start"), TEXT("Start live coding session"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleStartLiveCoding));

	// Compilation
	RegisterMethod(TEXT("liveCoding.compile"), TEXT("Trigger compilation"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleCompile));
	RegisterMethod(TEXT("liveCoding.getCompileStatus"), TEXT("Get compilation status"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetCompileStatus));
	RegisterMethod(TEXT("liveCoding.cancelCompile"), TEXT("Cancel ongoing compilation"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleCancelCompile));

	// Hot Reload
	RegisterMethod(TEXT("hotReload.reload"), TEXT("Trigger hot reload"), TEXT("HotReload"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleHotReload));
	RegisterMethod(TEXT("hotReload.canReload"), TEXT("Check if hot reload is available"), TEXT("HotReload"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleCanHotReload));

	// Module information
	RegisterMethod(TEXT("module.list"), TEXT("List loaded modules"), TEXT("Modules"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleListModules));
	RegisterMethod(TEXT("module.getInfo"), TEXT("Get module information"), TEXT("Modules"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetModuleInfo));
	RegisterMethod(TEXT("module.isLoaded"), TEXT("Check if module is loaded"), TEXT("Modules"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleIsModuleLoaded));

	// Patch information
	RegisterMethod(TEXT("liveCoding.getPendingPatches"), TEXT("Get pending patches"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetPendingPatches));
	RegisterMethod(TEXT("liveCoding.getAppliedPatches"), TEXT("Get applied patches"), TEXT("LiveCoding"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetAppliedPatches));

	// Build settings
	RegisterMethod(TEXT("build.getConfiguration"), TEXT("Get build configuration"), TEXT("Build"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetBuildConfiguration));
	RegisterMethod(TEXT("build.getCompilerSettings"), TEXT("Get compiler settings"), TEXT("Build"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetCompilerSettings));

	// Project files
	RegisterMethod(TEXT("project.generateFiles"), TEXT("Generate project files"), TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGenerateProjectFiles));
	RegisterMethod(TEXT("project.refreshFiles"), TEXT("Refresh project files"), TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleRefreshProjectFiles));

	// Compile errors
	RegisterMethod(TEXT("compile.getErrors"), TEXT("Get compilation errors"), TEXT("Compile"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetCompileErrors));
	RegisterMethod(TEXT("compile.getWarnings"), TEXT("Get compilation warnings"), TEXT("Compile"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetCompileWarnings));
}

TSharedPtr<FJsonObject> FUltimateControlLiveCodingHandler::ModuleToJson(const FModuleStatus& ModuleStatus)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("name"), ModuleStatus.Name);
	Json->SetStringField(TEXT("filePath"), ModuleStatus.FilePath);
	Json->SetBoolField(TEXT("isLoaded"), ModuleStatus.bIsLoaded);
	Json->SetBoolField(TEXT("isGameModule"), ModuleStatus.bIsGameModule);
	return Json;
}

bool FUltimateControlLiveCodingHandler::HandleIsLiveCodingEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("moduleLoaded"), LiveCoding != nullptr);

	if (LiveCoding)
	{
		StatusJson->SetBoolField(TEXT("enabled"), LiveCoding->IsEnabledForSession());
		StatusJson->SetBoolField(TEXT("enabledByDefault"), LiveCoding->IsEnabledByDefault());
	}

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
#else
	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("moduleLoaded"), false);
	StatusJson->SetBoolField(TEXT("enabled"), false);
	StatusJson->SetStringField(TEXT("message"), TEXT("Live Coding not available in this build"));
	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
#endif
}

bool FUltimateControlLiveCodingHandler::HandleEnableLiveCoding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	LiveCoding->EnableForSession(true);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetBoolField(TEXT("enabled"), LiveCoding->IsEnabledForSession());

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding not available in this build"));
	return true;
#endif
}

bool FUltimateControlLiveCodingHandler::HandleDisableLiveCoding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	LiveCoding->EnableForSession(false);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetBoolField(TEXT("enabled"), LiveCoding->IsEnabledForSession());

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding not available in this build"));
	return true;
#endif
}

bool FUltimateControlLiveCodingHandler::HandleStartLiveCoding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	if (!LiveCoding->IsEnabledForSession())
	{
		LiveCoding->EnableForSession(true);
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding not available in this build"));
	return true;
#endif
}

bool FUltimateControlLiveCodingHandler::HandleCompile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	if (!LiveCoding->IsEnabledForSession())
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding is not enabled for this session"));
		return true;
	}

	// Trigger live coding compile
	bool bStarted = LiveCoding->Compile();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("started"), bStarted);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding not available in this build"));
	return true;
#endif
}

bool FUltimateControlLiveCodingHandler::HandleGetCompileStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();

	if (LiveCoding)
	{
		StatusJson->SetBoolField(TEXT("isCompiling"), LiveCoding->IsCompiling());
	}
	else
	{
		StatusJson->SetBoolField(TEXT("isCompiling"), false);
	}

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
#else
	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("isCompiling"), false);
	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
#endif
}

bool FUltimateControlLiveCodingHandler::HandleCancelCompile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	// There's no direct cancel API, but we can report status
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Compile cancellation not directly supported"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
#else
	Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Live Coding not available in this build"));
	return true;
#endif
}

bool FUltimateControlLiveCodingHandler::HandleHotReload(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Hot Reload was removed in UE 5.6 - Live Coding is the replacement
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Hot reload was removed in UE 5.6. Use Live Coding instead (liveCoding.compile)."));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleCanHotReload(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Hot Reload was removed in UE 5.6 - Live Coding is the replacement
	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("canHotReload"), false);
	StatusJson->SetStringField(TEXT("reason"), TEXT("Hot reload was removed in UE 5.6. Use Live Coding instead."));

	Result = MakeShared<FJsonValueObject>(StatusJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleListModules(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	bool bGameModulesOnly = Params->HasField(TEXT("gameModulesOnly")) ? Params->GetBoolField(TEXT("gameModulesOnly")) : false;

	TArray<FModuleStatus> ModuleStatuses;
	FModuleManager::Get().QueryModules(ModuleStatuses);

	TArray<TSharedPtr<FJsonValue>> ModulesArray;

	for (const FModuleStatus& Status : ModuleStatuses)
	{
		if (bGameModulesOnly && !Status.bIsGameModule)
		{
			continue;
		}

		ModulesArray.Add(MakeShared<FJsonValueObject>(ModuleToJson(Status)));
	}

	Result = MakeShared<FJsonValueArray>(ModulesArray);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGetModuleInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ModuleName = Params->GetStringField(TEXT("moduleName"));
	if (ModuleName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("moduleName parameter required"));
		return true;
	}

	TArray<FModuleStatus> ModuleStatuses;
	FModuleManager::Get().QueryModules(ModuleStatuses);

	for (const FModuleStatus& Status : ModuleStatuses)
	{
		if (Status.Name == ModuleName)
		{
			Result = MakeShared<FJsonValueObject>(ModuleToJson(Status));
			return true;
		}
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Module not found: %s"), *ModuleName));
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleIsModuleLoaded(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ModuleName = Params->GetStringField(TEXT("moduleName"));
	if (ModuleName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("moduleName parameter required"));
		return true;
	}

	bool bIsLoaded = FModuleManager::Get().IsModuleLoaded(FName(*ModuleName));
	Result = MakeShared<FJsonValueBoolean>(bIsLoaded);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGetPendingPatches(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if ULTIMATE_CONTROL_HAS_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);

	TSharedPtr<FJsonObject> PatchesJson = MakeShared<FJsonObject>();

	if (LiveCoding)
	{
		PatchesJson->SetBoolField(TEXT("hasPendingPatches"), LiveCoding->HasPendingPatch());
	}
	else
	{
		PatchesJson->SetBoolField(TEXT("hasPendingPatches"), false);
	}

	Result = MakeShared<FJsonValueObject>(PatchesJson);
	return true;
#else
	TSharedPtr<FJsonObject> PatchesJson = MakeShared<FJsonObject>();
	PatchesJson->SetBoolField(TEXT("hasPendingPatches"), false);
	Result = MakeShared<FJsonValueObject>(PatchesJson);
	return true;
#endif
}

bool FUltimateControlLiveCodingHandler::HandleGetAppliedPatches(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> PatchesJson = MakeShared<FJsonObject>();
	PatchesJson->SetStringField(TEXT("message"), TEXT("Applied patches tracking not directly exposed"));

	Result = MakeShared<FJsonValueObject>(PatchesJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGetBuildConfiguration(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ConfigJson = MakeShared<FJsonObject>();

#if UE_BUILD_DEBUG
	ConfigJson->SetStringField(TEXT("configuration"), TEXT("Debug"));
#elif UE_BUILD_DEVELOPMENT
	ConfigJson->SetStringField(TEXT("configuration"), TEXT("Development"));
#elif UE_BUILD_SHIPPING
	ConfigJson->SetStringField(TEXT("configuration"), TEXT("Shipping"));
#elif UE_BUILD_TEST
	ConfigJson->SetStringField(TEXT("configuration"), TEXT("Test"));
#else
	ConfigJson->SetStringField(TEXT("configuration"), TEXT("Unknown"));
#endif

#if WITH_EDITOR
	ConfigJson->SetBoolField(TEXT("withEditor"), true);
#else
	ConfigJson->SetBoolField(TEXT("withEditor"), false);
#endif

#if WITH_HOT_RELOAD
	ConfigJson->SetBoolField(TEXT("hotReloadSupported"), true);
#else
	ConfigJson->SetBoolField(TEXT("hotReloadSupported"), false);
#endif

	Result = MakeShared<FJsonValueObject>(ConfigJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGetCompilerSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> CompilerJson = MakeShared<FJsonObject>();

#if PLATFORM_WINDOWS
	CompilerJson->SetStringField(TEXT("platform"), TEXT("Windows"));
	CompilerJson->SetStringField(TEXT("compiler"), TEXT("MSVC"));
#elif PLATFORM_MAC
	CompilerJson->SetStringField(TEXT("platform"), TEXT("Mac"));
	CompilerJson->SetStringField(TEXT("compiler"), TEXT("Clang"));
#elif PLATFORM_LINUX
	CompilerJson->SetStringField(TEXT("platform"), TEXT("Linux"));
	CompilerJson->SetStringField(TEXT("compiler"), TEXT("GCC/Clang"));
#else
	CompilerJson->SetStringField(TEXT("platform"), TEXT("Unknown"));
#endif

	Result = MakeShared<FJsonValueObject>(CompilerJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGenerateProjectFiles(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Project file generation is typically done through the editor menu or command line
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Use File > Generate Visual Studio Project Files in the editor, or run GenerateProjectFiles.bat"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleRefreshProjectFiles(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Refresh is similar to generate
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Use File > Refresh Visual Studio Project in the editor"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGetCompileErrors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Compile errors are typically shown in the Output Log
	TArray<TSharedPtr<FJsonValue>> ErrorsArray;

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetArrayField(TEXT("errors"), ErrorsArray);
	ResultJson->SetStringField(TEXT("message"), TEXT("Check Output Log for compile errors"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGetCompileWarnings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> WarningsArray;

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetArrayField(TEXT("warnings"), WarningsArray);
	ResultJson->SetStringField(TEXT("message"), TEXT("Check Output Log for compile warnings"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}
