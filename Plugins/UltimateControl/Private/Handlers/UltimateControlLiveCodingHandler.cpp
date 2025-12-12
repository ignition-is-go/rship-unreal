// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlLiveCodingHandler.h"
#include "ILiveCodingModule.h"
#include "IHotReload.h"
#include "Modules/ModuleManager.h"
#include "Misc/HotReloadInterface.h"

void FUltimateControlLiveCodingHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	// Live Coding
	Methods.Add(TEXT("liveCoding.isEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleIsLiveCodingEnabled));
	Methods.Add(TEXT("liveCoding.enable"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleEnableLiveCoding));
	Methods.Add(TEXT("liveCoding.disable"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleDisableLiveCoding));
	Methods.Add(TEXT("liveCoding.start"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleStartLiveCoding));

	// Compilation
	Methods.Add(TEXT("liveCoding.compile"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleCompile));
	Methods.Add(TEXT("liveCoding.getCompileStatus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetCompileStatus));
	Methods.Add(TEXT("liveCoding.cancelCompile"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleCancelCompile));

	// Hot Reload
	Methods.Add(TEXT("hotReload.reload"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleHotReload));
	Methods.Add(TEXT("hotReload.canReload"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleCanHotReload));

	// Module information
	Methods.Add(TEXT("module.list"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleListModules));
	Methods.Add(TEXT("module.getInfo"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetModuleInfo));
	Methods.Add(TEXT("module.isLoaded"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleIsModuleLoaded));

	// Patch information
	Methods.Add(TEXT("liveCoding.getPendingPatches"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetPendingPatches));
	Methods.Add(TEXT("liveCoding.getAppliedPatches"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetAppliedPatches));

	// Build settings
	Methods.Add(TEXT("build.getConfiguration"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetBuildConfiguration));
	Methods.Add(TEXT("build.getCompilerSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetCompilerSettings));

	// Project files
	Methods.Add(TEXT("project.generateFiles"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGenerateProjectFiles));
	Methods.Add(TEXT("project.refreshFiles"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleRefreshProjectFiles));

	// Compile errors
	Methods.Add(TEXT("compile.getErrors"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetCompileErrors));
	Methods.Add(TEXT("compile.getWarnings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLiveCodingHandler::HandleGetCompileWarnings));
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
}

bool FUltimateControlLiveCodingHandler::HandleEnableLiveCoding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = CreateError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	LiveCoding->EnableForSession(true);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetBoolField(TEXT("enabled"), LiveCoding->IsEnabledForSession());

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleDisableLiveCoding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = CreateError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	LiveCoding->EnableForSession(false);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetBoolField(TEXT("enabled"), LiveCoding->IsEnabledForSession());

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleStartLiveCoding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = CreateError(-32603, TEXT("Live Coding module not loaded"));
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
}

bool FUltimateControlLiveCodingHandler::HandleCompile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = CreateError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	if (!LiveCoding->IsEnabledForSession())
	{
		Error = CreateError(-32603, TEXT("Live Coding is not enabled for this session"));
		return true;
	}

	// Trigger live coding compile
	bool bStarted = LiveCoding->Compile();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("started"), bStarted);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGetCompileStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
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
}

bool FUltimateControlLiveCodingHandler::HandleCancelCompile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		Error = CreateError(-32603, TEXT("Live Coding module not loaded"));
		return true;
	}

	// There's no direct cancel API, but we can report status
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Compile cancellation not directly supported"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleHotReload(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if WITH_HOT_RELOAD
	IHotReloadInterface& HotReload = IHotReloadInterface::GetHotReloadInterface();

	// Check if we can hot reload
	if (!HotReload.IsCurrentlyCompiling())
	{
		// Trigger hot reload
		HotReload.DoHotReloadFromEditor(EHotReloadFlags::None);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("message"), TEXT("Hot reload triggered"));

		Result = MakeShared<FJsonValueObject>(ResultJson);
	}
	else
	{
		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), false);
		ResultJson->SetStringField(TEXT("message"), TEXT("Compilation in progress"));

		Result = MakeShared<FJsonValueObject>(ResultJson);
	}
#else
	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), false);
	ResultJson->SetStringField(TEXT("message"), TEXT("Hot reload not available in this build"));

	Result = MakeShared<FJsonValueObject>(ResultJson);
#endif

	return true;
}

bool FUltimateControlLiveCodingHandler::HandleCanHotReload(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
#if WITH_HOT_RELOAD
	IHotReloadInterface& HotReload = IHotReloadInterface::GetHotReloadInterface();

	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("canHotReload"), true);
	StatusJson->SetBoolField(TEXT("isCompiling"), HotReload.IsCurrentlyCompiling());

	Result = MakeShared<FJsonValueObject>(StatusJson);
#else
	TSharedPtr<FJsonObject> StatusJson = MakeShared<FJsonObject>();
	StatusJson->SetBoolField(TEXT("canHotReload"), false);
	StatusJson->SetStringField(TEXT("reason"), TEXT("Hot reload not available"));

	Result = MakeShared<FJsonValueObject>(StatusJson);
#endif

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
		Error = CreateError(-32602, TEXT("moduleName parameter required"));
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

	Error = CreateError(-32602, FString::Printf(TEXT("Module not found: %s"), *ModuleName));
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleIsModuleLoaded(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ModuleName = Params->GetStringField(TEXT("moduleName"));
	if (ModuleName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("moduleName parameter required"));
		return true;
	}

	bool bIsLoaded = FModuleManager::Get().IsModuleLoaded(FName(*ModuleName));
	Result = MakeShared<FJsonValueBoolean>(bIsLoaded);
	return true;
}

bool FUltimateControlLiveCodingHandler::HandleGetPendingPatches(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
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
