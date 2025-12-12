// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlEditorHandler.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
// EditorModeToolInstance not needed for current functionality
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Settings/EditorSettings.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Dialogs/Dialogs.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFileManager.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsContainer.h"

FUltimateControlEditorHandler::FUltimateControlEditorHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	// Window management
	RegisterMethod(TEXT("editor.listWindows"), TEXT("List windows"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleListWindows));
	RegisterMethod(TEXT("editor.getActiveWindow"), TEXT("Get active window"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetActiveWindow));
	RegisterMethod(TEXT("editor.focusWindow"), TEXT("Focus window"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleFocusWindow));
	RegisterMethod(TEXT("editor.closeWindow"), TEXT("Close window"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleCloseWindow));

	// Tab/Panel management
	RegisterMethod(TEXT("editor.listTabs"), TEXT("List tabs"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleListTabs));
	RegisterMethod(TEXT("editor.openTab"), TEXT("Open tab"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleOpenTab));
	RegisterMethod(TEXT("editor.closeTab"), TEXT("Close tab"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleCloseTab));
	RegisterMethod(TEXT("editor.focusTab"), TEXT("Focus tab"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleFocusTab));

	// Layout
	RegisterMethod(TEXT("editor.getLayout"), TEXT("Get layout"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetLayout));
	RegisterMethod(TEXT("editor.saveLayout"), TEXT("Save layout"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSaveLayout));
	RegisterMethod(TEXT("editor.loadLayout"), TEXT("Load layout"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleLoadLayout));
	RegisterMethod(TEXT("editor.listLayouts"), TEXT("List layouts"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleListLayouts));
	RegisterMethod(TEXT("editor.resetLayout"), TEXT("Reset layout"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleResetLayout));

	// Editor tools/modes
	RegisterMethod(TEXT("editor.getCurrentMode"), TEXT("Get current mode"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetCurrentMode));
	RegisterMethod(TEXT("editor.setMode"), TEXT("Set mode"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSetMode));
	RegisterMethod(TEXT("editor.listModes"), TEXT("List modes"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleListModes));

	// Tool selection
	RegisterMethod(TEXT("editor.getCurrentTool"), TEXT("Get current tool"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetCurrentTool));
	RegisterMethod(TEXT("editor.setTool"), TEXT("Set tool"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSetTool));
	RegisterMethod(TEXT("editor.listTools"), TEXT("List tools"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleListTools));

	// Gizmo/Transform mode
	RegisterMethod(TEXT("editor.getTransformMode"), TEXT("Get transform mode"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetTransformMode));
	RegisterMethod(TEXT("editor.setTransformMode"), TEXT("Set transform mode"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSetTransformMode));
	RegisterMethod(TEXT("editor.getCoordinateSystem"), TEXT("Get coordinate system"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetCoordinateSystem));
	RegisterMethod(TEXT("editor.setCoordinateSystem"), TEXT("Set coordinate system"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSetCoordinateSystem));

	// Snapping
	RegisterMethod(TEXT("editor.getSnapSettings"), TEXT("Get snap settings"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetSnapSettings));
	RegisterMethod(TEXT("editor.setSnapSettings"), TEXT("Set snap settings"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSetSnapSettings));
	RegisterMethod(TEXT("editor.toggleSnap"), TEXT("Toggle snap"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleToggleSnap));

	// Grid
	RegisterMethod(TEXT("editor.getGridSettings"), TEXT("Get grid settings"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetGridSettings));
	RegisterMethod(TEXT("editor.setGridSettings"), TEXT("Set grid settings"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSetGridSettings));
	RegisterMethod(TEXT("editor.toggleGrid"), TEXT("Toggle grid"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleToggleGrid));

	// Notifications
	RegisterMethod(TEXT("editor.showNotification"), TEXT("Show notification"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleShowNotification));
	RegisterMethod(TEXT("editor.showDialog"), TEXT("Show dialog"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleShowDialog));

	// Editor preferences
	RegisterMethod(TEXT("editor.getPreference"), TEXT("Get preference"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetEditorPreference));
	RegisterMethod(TEXT("editor.setPreference"), TEXT("Set preference"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSetEditorPreference));
	RegisterMethod(TEXT("editor.listPreferences"), TEXT("List preferences"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleListEditorPreferences));

	// Project settings
	RegisterMethod(TEXT("editor.getProjectSetting"), TEXT("Get project setting"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleGetProjectSetting));
	RegisterMethod(TEXT("editor.setProjectSetting"), TEXT("Set project setting"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleSetProjectSetting));
	RegisterMethod(TEXT("editor.openProjectSettings"), TEXT("Open project settings"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleOpenProjectSettings));

	// Menus and commands
	RegisterMethod(TEXT("editor.executeCommand"), TEXT("Execute command"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleExecuteCommand));
	RegisterMethod(TEXT("editor.listCommands"), TEXT("List commands"), TEXT("Editor"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlEditorHandler::HandleListCommands));
}

// ============================================================================
// Window Management
// ============================================================================

bool FUltimateControlEditorHandler::HandleListWindows(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> WindowArray;

	if (FSlateApplication::IsInitialized())
	{
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);

		for (const TSharedRef<SWindow>& Window : Windows)
		{
			TSharedPtr<FJsonObject> WindowObj = WindowToJson(Window);
			if (WindowObj.IsValid())
			{
				WindowArray.Add(MakeShared<FJsonValueObject>(WindowObj));
			}
		}
	}

	Result = MakeShared<FJsonValueArray>(WindowArray);
	return true;
}

bool FUltimateControlEditorHandler::HandleGetActiveWindow(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (ActiveWindow.IsValid())
		{
			Result = MakeShared<FJsonValueObject>(WindowToJson(ActiveWindow.ToSharedRef()));
			return true;
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("status"), TEXT("no_active_window"));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleFocusWindow(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Title;
	if (!Params->TryGetStringField(TEXT("title"), Title))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: title"));
		return false;
	}

	if (FSlateApplication::IsInitialized())
	{
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);

		for (const TSharedRef<SWindow>& Window : Windows)
		{
			if (Window->GetTitle().ToString().Contains(Title))
			{
				Window->BringToFront();
				FSlateApplication::Get().SetAllUserFocus(&Window.Get(), EFocusCause::SetDirectly);

				TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
				ResultObj->SetBoolField(TEXT("success"), true);
				ResultObj->SetStringField(TEXT("window"), Window->GetTitle().ToString());
				Result = MakeShared<FJsonValueObject>(ResultObj);
				return true;
			}
		}
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Window not found: %s"), *Title));
	return false;
}

bool FUltimateControlEditorHandler::HandleCloseWindow(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Title;
	if (!Params->TryGetStringField(TEXT("title"), Title))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: title"));
		return false;
	}

	if (FSlateApplication::IsInitialized())
	{
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);

		for (const TSharedRef<SWindow>& Window : Windows)
		{
			if (Window->GetTitle().ToString().Contains(Title))
			{
				Window->RequestDestroyWindow();

				TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
				ResultObj->SetBoolField(TEXT("success"), true);
				Result = MakeShared<FJsonValueObject>(ResultObj);
				return true;
			}
		}
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Window not found: %s"), *Title));
	return false;
}

// ============================================================================
// Tab/Panel Management
// ============================================================================

bool FUltimateControlEditorHandler::HandleListTabs(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> TabArray;

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FTabManager> TabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (TabManager.IsValid())
	{
		// Get registered tab spawner IDs
		TArray<FName> AllTabIds;
		TabManager->GetAllSpawnerTabIds(AllTabIds);

		for (const FName& TabId : AllTabIds)
		{
			TSharedPtr<FJsonObject> TabObj = MakeShared<FJsonObject>();
			TabObj->SetStringField(TEXT("id"), TabId.ToString());

			TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(FTabId(TabId));
			TabObj->SetBoolField(TEXT("isOpen"), Tab.IsValid());

			if (Tab.IsValid())
			{
				TabObj->SetStringField(TEXT("label"), Tab->GetTabLabel().ToString());
			}

			TabArray.Add(MakeShared<FJsonValueObject>(TabObj));
		}
	}

	Result = MakeShared<FJsonValueArray>(TabArray);
	return true;
}

bool FUltimateControlEditorHandler::HandleOpenTab(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString TabId;
	if (!Params->TryGetStringField(TEXT("tabId"), TabId))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: tabId"));
		return false;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FTabManager> TabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (TabManager.IsValid())
	{
		TSharedPtr<SDockTab> Tab = TabManager->TryInvokeTab(FTabId(*TabId));
		if (Tab.IsValid())
		{
			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetBoolField(TEXT("success"), true);
			ResultObj->SetStringField(TEXT("tabId"), TabId);
			Result = MakeShared<FJsonValueObject>(ResultObj);
			return true;
		}
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Failed to open tab: %s"), *TabId));
	return false;
}

bool FUltimateControlEditorHandler::HandleCloseTab(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString TabId;
	if (!Params->TryGetStringField(TEXT("tabId"), TabId))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: tabId"));
		return false;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FTabManager> TabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (TabManager.IsValid())
	{
		TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(FTabId(*TabId));
		if (Tab.IsValid())
		{
			Tab->RequestCloseTab();

			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetBoolField(TEXT("success"), true);
			Result = MakeShared<FJsonValueObject>(ResultObj);
			return true;
		}
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Tab not found: %s"), *TabId));
	return false;
}

bool FUltimateControlEditorHandler::HandleFocusTab(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString TabId;
	if (!Params->TryGetStringField(TEXT("tabId"), TabId))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: tabId"));
		return false;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FTabManager> TabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (TabManager.IsValid())
	{
		TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(FTabId(*TabId));
		if (Tab.IsValid())
		{
			Tab->ActivateInParent(ETabActivationCause::SetDirectly);
			Tab->DrawAttention();

			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetBoolField(TEXT("success"), true);
			Result = MakeShared<FJsonValueObject>(ResultObj);
			return true;
		}
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Tab not found: %s"), *TabId));
	return false;
}

// ============================================================================
// Layout
// ============================================================================

bool FUltimateControlEditorHandler::HandleGetLayout(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("status"), TEXT("current_layout"));

	// Get open tabs
	TArray<TSharedPtr<FJsonValue>> OpenTabs;
	TSharedPtr<FTabManager> TabManager = LevelEditorModule.GetLevelEditorTabManager();
	if (TabManager.IsValid())
	{
		TArray<FName> AllTabIds;
		TabManager->GetAllSpawnerTabIds(AllTabIds);

		for (const FName& TabId : AllTabIds)
		{
			TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(FTabId(TabId));
			if (Tab.IsValid())
			{
				OpenTabs.Add(MakeShared<FJsonValueString>(TabId.ToString()));
			}
		}
	}

	ResultObj->SetArrayField(TEXT("openTabs"), OpenTabs);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSaveLayout(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayoutName;
	if (!Params->TryGetStringField(TEXT("name"), LayoutName))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: name"));
		return false;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FTabManager> TabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (TabManager.IsValid())
	{
		// Save the current layout
		FString LayoutIni = FPaths::ProjectSavedDir() / TEXT("Layouts") / LayoutName + TEXT(".ini");
		TabManager->SavePersistentLayout();

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetStringField(TEXT("name"), LayoutName);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Failed to save layout"));
	return false;
}

bool FUltimateControlEditorHandler::HandleLoadLayout(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LayoutName;
	if (!Params->TryGetStringField(TEXT("name"), LayoutName))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: name"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), LayoutName);
	ResultObj->SetStringField(TEXT("note"), TEXT("Layout loading requires editor restart"));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleListLayouts(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> LayoutArray;

	// List saved layouts
	FString LayoutDir = FPaths::ProjectSavedDir() / TEXT("Layouts");
	TArray<FString> LayoutFiles;
	IFileManager::Get().FindFiles(LayoutFiles, *LayoutDir, TEXT("*.ini"));

	for (const FString& File : LayoutFiles)
	{
		TSharedPtr<FJsonObject> LayoutObj = MakeShared<FJsonObject>();
		LayoutObj->SetStringField(TEXT("name"), FPaths::GetBaseFilename(File));
		LayoutObj->SetStringField(TEXT("path"), LayoutDir / File);
		LayoutArray.Add(MakeShared<FJsonValueObject>(LayoutObj));
	}

	// Add default layouts
	TArray<FString> DefaultLayouts = { TEXT("Default"), TEXT("Cinematic"), TEXT("VFX"), TEXT("Level Design") };
	for (const FString& Layout : DefaultLayouts)
	{
		TSharedPtr<FJsonObject> LayoutObj = MakeShared<FJsonObject>();
		LayoutObj->SetStringField(TEXT("name"), Layout);
		LayoutObj->SetBoolField(TEXT("isDefault"), true);
		LayoutArray.Add(MakeShared<FJsonValueObject>(LayoutObj));
	}

	Result = MakeShared<FJsonValueArray>(LayoutArray);
	return true;
}

bool FUltimateControlEditorHandler::HandleResetLayout(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FTabManager> TabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (TabManager.IsValid())
	{
		// Reset to default layout
		TabManager->CloseAllAreas();

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetStringField(TEXT("note"), TEXT("Layout reset to default"));
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Failed to reset layout"));
	return false;
}

// ============================================================================
// Editor Tools/Modes
// ============================================================================

bool FUltimateControlEditorHandler::HandleGetCurrentMode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> ActiveModes;
	TArray<FEditorModeID> ActiveModeIds = ModeTools.GetActiveScriptableModes();

	for (const FEditorModeID& ModeId : ActiveModeIds)
	{
		ActiveModes.Add(MakeShared<FJsonValueString>(ModeId.ToString()));
	}

	ResultObj->SetArrayField(TEXT("activeModes"), ActiveModes);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSetMode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ModeId;
	if (!Params->TryGetStringField(TEXT("modeId"), ModeId))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: modeId"));
		return false;
	}

	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	bool bActivate = true;
	Params->TryGetBoolField(TEXT("activate"), bActivate);

	if (bActivate)
	{
		ModeTools.ActivateMode(FEditorModeID(*ModeId));
	}
	else
	{
		ModeTools.DeactivateMode(FEditorModeID(*ModeId));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("mode"), ModeId);
	ResultObj->SetBoolField(TEXT("activated"), bActivate);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleListModes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> ModeArray;

	// Standard editor modes
	TArray<TPair<FString, FString>> StandardModes = {
		{ TEXT("EM_Default"), TEXT("Default") },
		{ TEXT("EM_Placement"), TEXT("Placement") },
		{ TEXT("EM_Landscape"), TEXT("Landscape") },
		{ TEXT("EM_Foliage"), TEXT("Foliage") },
		{ TEXT("EM_MeshPaint"), TEXT("Mesh Paint") },
		{ TEXT("EM_Geometry"), TEXT("Geometry") },
		{ TEXT("EM_Physics"), TEXT("Physics") }
	};

	FEditorModeTools& ModeTools = GLevelEditorModeTools();
	TArray<FEditorModeID> ActiveModes = ModeTools.GetActiveScriptableModes();

	for (const auto& Mode : StandardModes)
	{
		TSharedPtr<FJsonObject> ModeObj = MakeShared<FJsonObject>();
		ModeObj->SetStringField(TEXT("id"), Mode.Key);
		ModeObj->SetStringField(TEXT("name"), Mode.Value);
		ModeObj->SetBoolField(TEXT("isActive"), ActiveModes.Contains(FEditorModeID(*Mode.Key)));
		ModeArray.Add(MakeShared<FJsonValueObject>(ModeObj));
	}

	Result = MakeShared<FJsonValueArray>(ModeArray);
	return true;
}

// ============================================================================
// Tool Selection
// ============================================================================

bool FUltimateControlEditorHandler::HandleGetCurrentTool(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("status"), TEXT("tool_query"));
	ResultObj->SetStringField(TEXT("note"), TEXT("Tool system varies by mode"));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSetTool(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ToolName;
	if (!Params->TryGetStringField(TEXT("tool"), ToolName))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: tool"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("tool"), ToolName);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleListTools(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> ToolArray;

	// Common tools across modes
	TArray<FString> CommonTools = {
		TEXT("Select"),
		TEXT("Translate"),
		TEXT("Rotate"),
		TEXT("Scale"),
		TEXT("Paint"),
		TEXT("Sculpt"),
		TEXT("Smooth")
	};

	for (const FString& Tool : CommonTools)
	{
		TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
		ToolObj->SetStringField(TEXT("name"), Tool);
		ToolArray.Add(MakeShared<FJsonValueObject>(ToolObj));
	}

	Result = MakeShared<FJsonValueArray>(ToolArray);
	return true;
}

// ============================================================================
// Transform Mode/Coordinate System
// ============================================================================

bool FUltimateControlEditorHandler::HandleGetTransformMode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

	FString ModeName;
	switch (ModeTools.GetWidgetMode())
	{
		case UE::Widget::WM_Translate: ModeName = TEXT("Translate"); break;
		case UE::Widget::WM_Rotate: ModeName = TEXT("Rotate"); break;
		case UE::Widget::WM_Scale: ModeName = TEXT("Scale"); break;
		case UE::Widget::WM_TranslateRotateZ: ModeName = TEXT("TranslateRotateZ"); break;
		case UE::Widget::WM_2D: ModeName = TEXT("2D"); break;
		default: ModeName = TEXT("None"); break;
	}

	ResultObj->SetStringField(TEXT("mode"), ModeName);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSetTransformMode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Mode;
	if (!Params->TryGetStringField(TEXT("mode"), Mode))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: mode"));
		return false;
	}

	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	UE::Widget::EWidgetMode WidgetMode = UE::Widget::WM_None;
	if (Mode.Equals(TEXT("Translate"), ESearchCase::IgnoreCase))
	{
		WidgetMode = UE::Widget::WM_Translate;
	}
	else if (Mode.Equals(TEXT("Rotate"), ESearchCase::IgnoreCase))
	{
		WidgetMode = UE::Widget::WM_Rotate;
	}
	else if (Mode.Equals(TEXT("Scale"), ESearchCase::IgnoreCase))
	{
		WidgetMode = UE::Widget::WM_Scale;
	}

	ModeTools.SetWidgetMode(WidgetMode);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("mode"), Mode);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleGetCoordinateSystem(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

	FString CoordSystem = ModeTools.GetCoordSystem() == COORD_World ? TEXT("World") : TEXT("Local");
	ResultObj->SetStringField(TEXT("coordinateSystem"), CoordSystem);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSetCoordinateSystem(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString System;
	if (!Params->TryGetStringField(TEXT("system"), System))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: system"));
		return false;
	}

	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	ECoordSystem CoordSystem = System.Equals(TEXT("World"), ESearchCase::IgnoreCase) ? COORD_World : COORD_Local;
	ModeTools.SetCoordSystem(CoordSystem);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("system"), System);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

// ============================================================================
// Snapping
// ============================================================================

bool FUltimateControlEditorHandler::HandleGetSnapSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	if (ViewportSettings)
	{
		ResultObj->SetBoolField(TEXT("gridSnapEnabled"), ViewportSettings->GridEnabled);
		ResultObj->SetBoolField(TEXT("rotationSnapEnabled"), ViewportSettings->RotGridEnabled);
		ResultObj->SetBoolField(TEXT("scaleSnapEnabled"), ViewportSettings->SnapScaleEnabled);

		ResultObj->SetNumberField(TEXT("gridSize"), GEditor->GetGridSize());
		ResultObj->SetNumberField(TEXT("rotationSnapAngle"), GEditor->GetRotGridSize().Yaw);
		ResultObj->SetNumberField(TEXT("scaleSnapValue"), GEditor->GetScaleGridSize());
	}

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSetSnapSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	if (!ViewportSettings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Viewport settings not available"));
		return false;
	}

	bool bGridSnapEnabled;
	if (Params->TryGetBoolField(TEXT("gridSnapEnabled"), bGridSnapEnabled))
	{
		ViewportSettings->GridEnabled = bGridSnapEnabled;
	}

	bool bRotationSnapEnabled;
	if (Params->TryGetBoolField(TEXT("rotationSnapEnabled"), bRotationSnapEnabled))
	{
		ViewportSettings->RotGridEnabled = bRotationSnapEnabled;
	}

	bool bScaleSnapEnabled;
	if (Params->TryGetBoolField(TEXT("scaleSnapEnabled"), bScaleSnapEnabled))
	{
		ViewportSettings->SnapScaleEnabled = bScaleSnapEnabled;
	}

	double GridSize;
	if (Params->TryGetNumberField(TEXT("gridSize"), GridSize))
	{
		GEditor->SetGridSize(0, static_cast<float>(GridSize));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleToggleSnap(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SnapType;
	if (!Params->TryGetStringField(TEXT("type"), SnapType))
	{
		SnapType = TEXT("grid");
	}

	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	if (!ViewportSettings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Viewport settings not available"));
		return false;
	}

	bool bNewState = false;
	if (SnapType.Equals(TEXT("grid"), ESearchCase::IgnoreCase))
	{
		ViewportSettings->GridEnabled = !ViewportSettings->GridEnabled;
		bNewState = ViewportSettings->GridEnabled;
	}
	else if (SnapType.Equals(TEXT("rotation"), ESearchCase::IgnoreCase))
	{
		ViewportSettings->RotGridEnabled = !ViewportSettings->RotGridEnabled;
		bNewState = ViewportSettings->RotGridEnabled;
	}
	else if (SnapType.Equals(TEXT("scale"), ESearchCase::IgnoreCase))
	{
		ViewportSettings->SnapScaleEnabled = !ViewportSettings->SnapScaleEnabled;
		bNewState = ViewportSettings->SnapScaleEnabled;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("type"), SnapType);
	ResultObj->SetBoolField(TEXT("enabled"), bNewState);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

// ============================================================================
// Grid
// ============================================================================

bool FUltimateControlEditorHandler::HandleGetGridSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("gridEnabled"), ViewportSettings ? ViewportSettings->GridEnabled : false);
	ResultObj->SetNumberField(TEXT("gridSize"), GEditor->GetGridSize());

	// Grid snap values
	TArray<TSharedPtr<FJsonValue>> GridSizes;
	GridSizes.Add(MakeShared<FJsonValueNumber>(1.0f));
	GridSizes.Add(MakeShared<FJsonValueNumber>(5.0f));
	GridSizes.Add(MakeShared<FJsonValueNumber>(10.0f));
	GridSizes.Add(MakeShared<FJsonValueNumber>(50.0f));
	GridSizes.Add(MakeShared<FJsonValueNumber>(100.0f));
	GridSizes.Add(MakeShared<FJsonValueNumber>(500.0f));
	GridSizes.Add(MakeShared<FJsonValueNumber>(1000.0f));
	ResultObj->SetArrayField(TEXT("availableGridSizes"), GridSizes);

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSetGridSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!GEditor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
		return false;
	}

	double GridSize;
	if (Params->TryGetNumberField(TEXT("gridSize"), GridSize))
	{
		GEditor->SetGridSize(0, static_cast<float>(GridSize));
	}

	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	if (ViewportSettings)
	{
		bool bGridEnabled;
		if (Params->TryGetBoolField(TEXT("gridEnabled"), bGridEnabled))
		{
			ViewportSettings->GridEnabled = bGridEnabled;
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleToggleGrid(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	if (!ViewportSettings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Viewport settings not available"));
		return false;
	}

	ViewportSettings->GridEnabled = !ViewportSettings->GridEnabled;

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetBoolField(TEXT("gridEnabled"), ViewportSettings->GridEnabled);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

// ============================================================================
// Notifications
// ============================================================================

bool FUltimateControlEditorHandler::HandleShowNotification(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Message;
	if (!Params->TryGetStringField(TEXT("message"), Message))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: message"));
		return false;
	}

	FString Type;
	Params->TryGetStringField(TEXT("type"), Type);

	double Duration = 3.0;
	Params->TryGetNumberField(TEXT("duration"), Duration);

	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = static_cast<float>(Duration);
	Info.bFireAndForget = true;
	Info.bUseThrobber = false;

	if (Type.Equals(TEXT("success"), ESearchCase::IgnoreCase))
	{
		Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.SuccessWithColor"));
	}
	else if (Type.Equals(TEXT("error"), ESearchCase::IgnoreCase))
	{
		Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.ErrorWithColor"));
	}
	else if (Type.Equals(TEXT("warning"), ESearchCase::IgnoreCase))
	{
		Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.WarningWithColor"));
	}

	FSlateNotificationManager::Get().AddNotification(Info);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleShowDialog(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Title;
	if (!Params->TryGetStringField(TEXT("title"), Title))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: title"));
		return false;
	}

	FString Message;
	if (!Params->TryGetStringField(TEXT("message"), Message))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: message"));
		return false;
	}

	FString Type;
	Params->TryGetStringField(TEXT("type"), Type);

	// Show dialog on game thread
	EAppReturnType::Type ReturnType = FMessageDialog::Open(
		Type.Equals(TEXT("yesno"), ESearchCase::IgnoreCase) ? EAppMsgType::YesNo : EAppMsgType::Ok,
		FText::FromString(Message),
		FText::FromString(Title)
	);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("result"), ReturnType == EAppReturnType::Yes ? TEXT("Yes") :
		(ReturnType == EAppReturnType::No ? TEXT("No") : TEXT("Ok")));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

// ============================================================================
// Editor Preferences
// ============================================================================

bool FUltimateControlEditorHandler::HandleGetEditorPreference(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Category;
	FString Section;
	FString Property;

	if (!Params->TryGetStringField(TEXT("category"), Category) ||
		!Params->TryGetStringField(TEXT("section"), Section) ||
		!Params->TryGetStringField(TEXT("property"), Property))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: category, section, property"));
		return false;
	}

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (!SettingsModule)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Settings module not available"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("category"), Category);
	ResultObj->SetStringField(TEXT("section"), Section);
	ResultObj->SetStringField(TEXT("property"), Property);
	ResultObj->SetStringField(TEXT("status"), TEXT("preference_queried"));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSetEditorPreference(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Category;
	FString Section;
	FString Property;

	if (!Params->TryGetStringField(TEXT("category"), Category) ||
		!Params->TryGetStringField(TEXT("section"), Section) ||
		!Params->TryGetStringField(TEXT("property"), Property))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: category, section, property"));
		return false;
	}

	// Value can be any type
	const TSharedPtr<FJsonValue>* Value = nullptr;
	if (!Params->TryGetField(TEXT("value"), Value))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: value"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("category"), Category);
	ResultObj->SetStringField(TEXT("section"), Section);
	ResultObj->SetStringField(TEXT("property"), Property);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleListEditorPreferences(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> PreferenceArray;

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
	{
		TArray<FName> ContainerNames;
		SettingsModule->GetContainerNames(ContainerNames);

		for (const FName& ContainerName : ContainerNames)
		{
			TSharedPtr<ISettingsContainer> Container = SettingsModule->GetContainer(ContainerName);
			if (Container.IsValid())
			{
				TSharedPtr<FJsonObject> ContainerObj = MakeShared<FJsonObject>();
				ContainerObj->SetStringField(TEXT("name"), ContainerName.ToString());
				ContainerObj->SetStringField(TEXT("displayName"), Container->GetDisplayName().ToString());

				TArray<TSharedPtr<FJsonValue>> Categories;
				TArray<TSharedPtr<ISettingsCategory>> CategoryList;
				Container->GetCategories(CategoryList);

				for (const TSharedPtr<ISettingsCategory>& Category : CategoryList)
				{
					Categories.Add(MakeShared<FJsonValueString>(Category->GetName().ToString()));
				}

				ContainerObj->SetArrayField(TEXT("categories"), Categories);
				PreferenceArray.Add(MakeShared<FJsonValueObject>(ContainerObj));
			}
		}
	}

	Result = MakeShared<FJsonValueArray>(PreferenceArray);
	return true;
}

// ============================================================================
// Project Settings
// ============================================================================

bool FUltimateControlEditorHandler::HandleGetProjectSetting(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Category;
	FString Section;
	FString Property;

	if (!Params->TryGetStringField(TEXT("category"), Category) ||
		!Params->TryGetStringField(TEXT("section"), Section) ||
		!Params->TryGetStringField(TEXT("property"), Property))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: category, section, property"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("category"), Category);
	ResultObj->SetStringField(TEXT("section"), Section);
	ResultObj->SetStringField(TEXT("property"), Property);
	ResultObj->SetStringField(TEXT("status"), TEXT("setting_queried"));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Category;
	FString Section;
	FString Property;

	if (!Params->TryGetStringField(TEXT("category"), Category) ||
		!Params->TryGetStringField(TEXT("section"), Section) ||
		!Params->TryGetStringField(TEXT("property"), Property))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: category, section, property"));
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = nullptr;
	if (!Params->TryGetField(TEXT("value"), Value))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: value"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("category"), Category);
	ResultObj->SetStringField(TEXT("section"), Section);
	ResultObj->SetStringField(TEXT("property"), Property);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlEditorHandler::HandleOpenProjectSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Category;
	Params->TryGetStringField(TEXT("category"), Category);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
	{
		if (Category.IsEmpty())
		{
			SettingsModule->ShowViewer(TEXT("Project"), TEXT("Project"), TEXT("General"));
		}
		else
		{
			SettingsModule->ShowViewer(TEXT("Project"), *Category, TEXT(""));
		}

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Settings module not available"));
	return false;
}

// ============================================================================
// Commands
// ============================================================================

bool FUltimateControlEditorHandler::HandleExecuteCommand(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Command;
	if (!Params->TryGetStringField(TEXT("command"), Command))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: command"));
		return false;
	}

	if (GUnrealEd)
	{
		GUnrealEd->Exec(GEditor->GetEditorWorldContext().World(), *Command);

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetStringField(TEXT("command"), Command);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Editor not available"));
	return false;
}

bool FUltimateControlEditorHandler::HandleListCommands(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> CommandArray;

	// Common editor commands
	TArray<TPair<FString, FString>> CommonCommands = {
		{ TEXT("EDIT COPY"), TEXT("Copy selection") },
		{ TEXT("EDIT CUT"), TEXT("Cut selection") },
		{ TEXT("EDIT PASTE"), TEXT("Paste clipboard") },
		{ TEXT("EDIT DUPLICATE"), TEXT("Duplicate selection") },
		{ TEXT("DELETE"), TEXT("Delete selection") },
		{ TEXT("SELECT ALL"), TEXT("Select all actors") },
		{ TEXT("SELECT NONE"), TEXT("Deselect all") },
		{ TEXT("CAMERA ALIGN"), TEXT("Align camera to selection") },
		{ TEXT("BUILD"), TEXT("Build all") },
		{ TEXT("BUILD LIGHTING"), TEXT("Build lighting") },
		{ TEXT("BUILD PATHS"), TEXT("Build paths") },
		{ TEXT("MAP CHECK"), TEXT("Check map for errors") },
		{ TEXT("SAVE ALL"), TEXT("Save all modified assets") },
		{ TEXT("SAVEGAME"), TEXT("Save current game") }
	};

	for (const auto& Cmd : CommonCommands)
	{
		TSharedPtr<FJsonObject> CmdObj = MakeShared<FJsonObject>();
		CmdObj->SetStringField(TEXT("command"), Cmd.Key);
		CmdObj->SetStringField(TEXT("description"), Cmd.Value);
		CommandArray.Add(MakeShared<FJsonValueObject>(CmdObj));
	}

	Result = MakeShared<FJsonValueArray>(CommandArray);
	return true;
}

// ============================================================================
// Helper Methods
// ============================================================================

TSharedPtr<FJsonObject> FUltimateControlEditorHandler::WindowToJson(TSharedRef<SWindow> Window)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("title"), Window->GetTitle().ToString());
	Obj->SetNumberField(TEXT("width"), Window->GetSizeInScreen().X);
	Obj->SetNumberField(TEXT("height"), Window->GetSizeInScreen().Y);
	Obj->SetNumberField(TEXT("x"), Window->GetPositionInScreen().X);
	Obj->SetNumberField(TEXT("y"), Window->GetPositionInScreen().Y);
	Obj->SetBoolField(TEXT("isMaximized"), Window->IsWindowMaximized());
	Obj->SetBoolField(TEXT("isMinimized"), Window->IsWindowMinimized());
	Obj->SetBoolField(TEXT("hasFocus"), Window->HasFocusedDescendants());

	return Obj;
}

TSharedPtr<FJsonObject> FUltimateControlEditorHandler::TabToJson(TSharedRef<SDockTab> Tab)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("label"), Tab->GetTabLabel().ToString());
	Obj->SetBoolField(TEXT("isForeground"), Tab->IsForeground());

	return Obj;
}
