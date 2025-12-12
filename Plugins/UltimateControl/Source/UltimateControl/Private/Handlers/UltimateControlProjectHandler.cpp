// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlProjectHandler.h"
#include "UltimateControl.h"

#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"
// TODO(nf): GameProjectGenerationModule.h may not exist or has moved in UE 5.6
// #include "GameProjectGenerationModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "UObject/UObjectIterator.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"

FUltimateControlProjectHandler::FUltimateControlProjectHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// Register all project methods
	RegisterMethod(
		TEXT("project.getInfo"),
		TEXT("Get information about the current Unreal Engine project"),
		TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlProjectHandler::HandleGetInfo));

	RegisterMethod(
		TEXT("project.getConfig"),
		TEXT("Get project configuration settings"),
		TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlProjectHandler::HandleGetConfig));

	RegisterMethod(
		TEXT("project.listPlugins"),
		TEXT("List all enabled plugins in the project"),
		TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlProjectHandler::HandleListPlugins));

	RegisterMethod(
		TEXT("project.getModules"),
		TEXT("Get list of project modules"),
		TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlProjectHandler::HandleGetModules));

	RegisterMethod(
		TEXT("project.save"),
		TEXT("Save all dirty (modified) packages"),
		TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlProjectHandler::HandleSave));

	RegisterMethod(
		TEXT("project.getDirtyPackages"),
		TEXT("Get list of packages with unsaved changes"),
		TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlProjectHandler::HandleGetDirtyPackages));

	RegisterMethod(
		TEXT("project.compileBlueprints"),
		TEXT("Recompile all blueprints in the project"),
		TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlProjectHandler::HandleCompileBlueprints));

	RegisterMethod(
		TEXT("project.getRecentFiles"),
		TEXT("Get list of recently opened files"),
		TEXT("Project"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlProjectHandler::HandleGetRecentFiles));
}

bool FUltimateControlProjectHandler::HandleGetInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();

	// Project paths
	InfoObj->SetStringField(TEXT("projectName"), FApp::GetProjectName());
	InfoObj->SetStringField(TEXT("projectDir"), FPaths::ProjectDir());
	InfoObj->SetStringField(TEXT("projectFile"), FPaths::GetProjectFilePath());
	InfoObj->SetStringField(TEXT("contentDir"), FPaths::ProjectContentDir());
	InfoObj->SetStringField(TEXT("savedDir"), FPaths::ProjectSavedDir());
	InfoObj->SetStringField(TEXT("configDir"), FPaths::ProjectConfigDir());
	InfoObj->SetStringField(TEXT("pluginsDir"), FPaths::ProjectPluginsDir());

	// Engine info
	InfoObj->SetStringField(TEXT("engineDir"), FPaths::EngineDir());
	InfoObj->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
	InfoObj->SetNumberField(TEXT("engineMajorVersion"), FEngineVersion::Current().GetMajor());
	InfoObj->SetNumberField(TEXT("engineMinorVersion"), FEngineVersion::Current().GetMinor());
	InfoObj->SetNumberField(TEXT("enginePatchVersion"), FEngineVersion::Current().GetPatch());

	// Platform info
	InfoObj->SetStringField(TEXT("platform"), FPlatformProperties::IniPlatformName());
	InfoObj->SetStringField(TEXT("platformName"), FPlatformProperties::PlatformName());
	InfoObj->SetBoolField(TEXT("isEditor"), GIsEditor);
	InfoObj->SetBoolField(TEXT("isGame"), !GIsEditor);
	InfoObj->SetBoolField(TEXT("isDebugBuild"), UE_BUILD_DEBUG != 0);
	InfoObj->SetBoolField(TEXT("isDevelopmentBuild"), UE_BUILD_DEVELOPMENT != 0);
	InfoObj->SetBoolField(TEXT("isShippingBuild"), UE_BUILD_SHIPPING != 0);

	// Current state
	if (GEditor)
	{
		InfoObj->SetBoolField(TEXT("isPlayInEditor"), GEditor->IsPlaySessionInProgress());
		InfoObj->SetBoolField(TEXT("isSimulating"), GEditor->bIsSimulatingInEditor);
	}

	// Try to load project descriptor for more info
	FProjectDescriptor ProjectDesc;
	FText OutFailReason;
	if (ProjectDesc.Load(FPaths::GetProjectFilePath(), OutFailReason))
	{
		InfoObj->SetStringField(TEXT("description"), ProjectDesc.Description);
		InfoObj->SetStringField(TEXT("category"), ProjectDesc.Category);
		InfoObj->SetBoolField(TEXT("isEnterprise"), ProjectDesc.bIsEnterpriseProject);

		// Target platforms
		TArray<TSharedPtr<FJsonValue>> PlatformsArray;
		for (const FString& Platform : ProjectDesc.TargetPlatforms)
		{
			PlatformsArray.Add(MakeShared<FJsonValueString>(Platform));
		}
		InfoObj->SetArrayField(TEXT("targetPlatforms"), PlatformsArray);
	}

	OutResult = MakeShared<FJsonValueObject>(InfoObj);
	return true;
}

bool FUltimateControlProjectHandler::HandleGetConfig(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString ConfigSection = GetOptionalString(Params, TEXT("section"), TEXT(""));
	FString ConfigKey = GetOptionalString(Params, TEXT("key"), TEXT(""));
	FString ConfigFile = GetOptionalString(Params, TEXT("file"), TEXT("DefaultGame"));

	TSharedPtr<FJsonObject> ConfigObj = MakeShared<FJsonObject>();

	FString ConfigPath = FPaths::ProjectConfigDir() / ConfigFile + TEXT(".ini");
	if (!FPaths::FileExists(ConfigPath))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Config file not found: %s"), *ConfigPath));
		return false;
	}

	if (!ConfigSection.IsEmpty() && !ConfigKey.IsEmpty())
	{
		// Get specific value
		FString Value;
		if (GConfig->GetString(*ConfigSection, *ConfigKey, Value, ConfigPath))
		{
			ConfigObj->SetStringField(TEXT("value"), Value);
		}
		else
		{
			ConfigObj->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
		}
	}
	else if (!ConfigSection.IsEmpty())
	{
		// Get all keys in section
		TArray<FString> Keys;
		GConfig->GetSection(*ConfigSection, Keys, ConfigPath);

		TSharedPtr<FJsonObject> SectionObj = MakeShared<FJsonObject>();
		for (const FString& KeyValue : Keys)
		{
			FString Key, Value;
			if (KeyValue.Split(TEXT("="), &Key, &Value))
			{
				SectionObj->SetStringField(Key, Value);
			}
		}
		ConfigObj->SetObjectField(TEXT("section"), SectionObj);
	}
	else
	{
		// Get all sections
		TArray<FString> Sections;
		GConfig->GetSectionNames(ConfigPath, Sections);

		TArray<TSharedPtr<FJsonValue>> SectionsArray;
		for (const FString& Section : Sections)
		{
			SectionsArray.Add(MakeShared<FJsonValueString>(Section));
		}
		ConfigObj->SetArrayField(TEXT("sections"), SectionsArray);
	}

	ConfigObj->SetStringField(TEXT("file"), ConfigPath);
	OutResult = MakeShared<FJsonValueObject>(ConfigObj);
	return true;
}

bool FUltimateControlProjectHandler::HandleListPlugins(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	bool bEnabledOnly = GetOptionalBool(Params, TEXT("enabledOnly"), true);
	FString Category = GetOptionalString(Params, TEXT("category"), TEXT(""));

	TArray<TSharedPtr<FJsonValue>> PluginsArray;

	IPluginManager& PluginManager = IPluginManager::Get();
	TArray<TSharedRef<IPlugin>> Plugins = PluginManager.GetDiscoveredPlugins();

	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		if (bEnabledOnly && !Plugin->IsEnabled())
		{
			continue;
		}

		const FPluginDescriptor& Desc = Plugin->GetDescriptor();

		if (!Category.IsEmpty() && Desc.Category != Category)
		{
			continue;
		}

		TSharedPtr<FJsonObject> PluginObj = MakeShared<FJsonObject>();
		PluginObj->SetStringField(TEXT("name"), Plugin->GetName());
		PluginObj->SetStringField(TEXT("friendlyName"), Desc.FriendlyName);
		PluginObj->SetStringField(TEXT("description"), Desc.Description);
		PluginObj->SetStringField(TEXT("category"), Desc.Category);
		PluginObj->SetStringField(TEXT("version"), Desc.VersionName);
		PluginObj->SetStringField(TEXT("createdBy"), Desc.CreatedBy);
		PluginObj->SetBoolField(TEXT("enabled"), Plugin->IsEnabled());
		PluginObj->SetBoolField(TEXT("canContainContent"), Desc.bCanContainContent);
		PluginObj->SetBoolField(TEXT("isBetaVersion"), Desc.bIsBetaVersion);
		PluginObj->SetBoolField(TEXT("installed"), Desc.bInstalled);

		// Modules
		TArray<TSharedPtr<FJsonValue>> ModulesArray;
		for (const FModuleDescriptor& Module : Desc.Modules)
		{
			TSharedPtr<FJsonObject> ModuleObj = MakeShared<FJsonObject>();
			ModuleObj->SetStringField(TEXT("name"), Module.Name.ToString());
			ModuleObj->SetStringField(TEXT("type"), StaticEnum<EHostType::Type>()->GetNameStringByValue(static_cast<int64>(Module.Type)));
			ModuleObj->SetStringField(TEXT("loadingPhase"), StaticEnum<ELoadingPhase::Type>()->GetNameStringByValue(static_cast<int64>(Module.LoadingPhase)));
			ModulesArray.Add(MakeShared<FJsonValueObject>(ModuleObj));
		}
		PluginObj->SetArrayField(TEXT("modules"), ModulesArray);

		PluginsArray.Add(MakeShared<FJsonValueObject>(PluginObj));
	}

	OutResult = MakeShared<FJsonValueArray>(PluginsArray);
	return true;
}

bool FUltimateControlProjectHandler::HandleGetModules(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	TArray<TSharedPtr<FJsonValue>> ModulesArray;

	// Get loaded modules
	TArray<FModuleStatus> ModuleStatuses;
	FModuleManager::Get().QueryModules(ModuleStatuses);

	for (const FModuleStatus& Status : ModuleStatuses)
	{
		TSharedPtr<FJsonObject> ModuleObj = MakeShared<FJsonObject>();
		ModuleObj->SetStringField(TEXT("name"), Status.Name);
		ModuleObj->SetStringField(TEXT("filePath"), Status.FilePath);
		ModuleObj->SetBoolField(TEXT("isLoaded"), Status.bIsLoaded);
		ModuleObj->SetBoolField(TEXT("isGameModule"), Status.bIsGameModule);

		ModulesArray.Add(MakeShared<FJsonValueObject>(ModuleObj));
	}

	OutResult = MakeShared<FJsonValueArray>(ModulesArray);
	return true;
}

bool FUltimateControlProjectHandler::HandleSave(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	bool bPromptUser = GetOptionalBool(Params, TEXT("prompt"), false);

	bool bSuccess = false;
	if (bPromptUser)
	{
		bSuccess = FEditorFileUtils::SaveDirtyPackages(
			/* bPromptUserToSave */ true,
			/* bSaveMapPackages */ true,
			/* bSaveContentPackages */ true);
	}
	else
	{
		bSuccess = FEditorFileUtils::SaveDirtyPackages(
			/* bPromptUserToSave */ false,
			/* bSaveMapPackages */ true,
			/* bSaveContentPackages */ true);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bSuccess);
	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlProjectHandler::HandleGetDirtyPackages(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	TArray<TSharedPtr<FJsonValue>> PackagesArray;

	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyPackages(DirtyPackages);

	for (UPackage* Package : DirtyPackages)
	{
		if (Package)
		{
			TSharedPtr<FJsonObject> PkgObj = MakeShared<FJsonObject>();
			PkgObj->SetStringField(TEXT("name"), Package->GetName());
			PkgObj->SetStringField(TEXT("fileName"), Package->FileName.ToString());
			PkgObj->SetBoolField(TEXT("isMap"), Package->ContainsMap());
			PackagesArray.Add(MakeShared<FJsonValueObject>(PkgObj));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(PackagesArray);
	return true;
}

bool FUltimateControlProjectHandler::HandleCompileBlueprints(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	bool bCompileAll = GetOptionalBool(Params, TEXT("all"), true);
	FString BlueprintPath = GetOptionalString(Params, TEXT("path"), TEXT(""));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	int32 CompiledCount = 0;
	int32 ErrorCount = 0;
	TArray<TSharedPtr<FJsonValue>> ErrorsArray;

	if (!BlueprintPath.IsEmpty())
	{
		// Compile specific blueprint
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		if (Blueprint)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);
			CompiledCount = 1;
			if (Blueprint->Status == BS_Error)
			{
				ErrorCount = 1;
				TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
				ErrorObj->SetStringField(TEXT("blueprint"), BlueprintPath);
				ErrorObj->SetStringField(TEXT("message"), TEXT("Blueprint has errors"));
				ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrorObj));
			}
		}
		else
		{
			OutError = UUltimateControlSubsystem::MakeError(
				EJsonRpcError::NotFound,
				FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
			return false;
		}
	}
	else
	{
		// Compile all blueprints
		for (TObjectIterator<UBlueprint> It; It; ++It)
		{
			UBlueprint* Blueprint = *It;
			if (Blueprint && !Blueprint->IsPendingKill())
			{
				FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);
				CompiledCount++;
				if (Blueprint->Status == BS_Error)
				{
					ErrorCount++;
					TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
					ErrorObj->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
					ErrorObj->SetStringField(TEXT("message"), TEXT("Blueprint has errors"));
					ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrorObj));
				}
			}
		}
	}

	ResultObj->SetNumberField(TEXT("compiledCount"), CompiledCount);
	ResultObj->SetNumberField(TEXT("errorCount"), ErrorCount);
	ResultObj->SetArrayField(TEXT("errors"), ErrorsArray);
	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlProjectHandler::HandleGetRecentFiles(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	int32 MaxCount = GetOptionalInt(Params, TEXT("maxCount"), 20);

	TArray<TSharedPtr<FJsonValue>> FilesArray;

	// Get recent levels
	TArray<FString> RecentlyOpenedMapsList;
	if (GConfig->GetArray(TEXT("LevelEditor"), TEXT("RecentlyOpenedMapsList"), RecentlyOpenedMapsList, GEditorPerProjectIni))
	{
		for (int32 i = 0; i < FMath::Min(RecentlyOpenedMapsList.Num(), MaxCount); ++i)
		{
			TSharedPtr<FJsonObject> FileObj = MakeShared<FJsonObject>();
			FileObj->SetStringField(TEXT("path"), RecentlyOpenedMapsList[i]);
			FileObj->SetStringField(TEXT("type"), TEXT("Level"));
			FilesArray.Add(MakeShared<FJsonValueObject>(FileObj));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(FilesArray);
	return true;
}
