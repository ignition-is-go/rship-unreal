// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlViewportHandler.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewportSettings.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"

void FUltimateControlViewportHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	Methods.Add(TEXT("viewport.list"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleListViewports));
	Methods.Add(TEXT("viewport.get"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleGetViewport));
	Methods.Add(TEXT("viewport.getCamera"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleGetCamera));
	Methods.Add(TEXT("viewport.setCamera"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleSetCamera));
	Methods.Add(TEXT("viewport.focusOnActor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleFocusOnActor));
	Methods.Add(TEXT("viewport.focusOnLocation"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleFocusOnLocation));
	Methods.Add(TEXT("viewport.getSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleGetViewportSettings));
	Methods.Add(TEXT("viewport.setSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleSetViewportSettings));
	Methods.Add(TEXT("viewport.setViewMode"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleSetViewMode));
	Methods.Add(TEXT("viewport.setRealtime"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleSetRealtime));
	Methods.Add(TEXT("viewport.takeScreenshot"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleTakeScreenshot));
	Methods.Add(TEXT("viewport.getSize"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleGetViewportSize));
	Methods.Add(TEXT("viewport.maximize"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleMaximizeViewport));
	Methods.Add(TEXT("viewport.restore"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlViewportHandler::HandleRestoreViewports));
}

FLevelEditorViewportClient* FUltimateControlViewportHandler::GetViewportClient(int32 ViewportIndex)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	if (!LevelEditor.IsValid())
	{
		return nullptr;
	}

	TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
	if (ViewportIndex >= 0 && ViewportIndex < Viewports.Num())
	{
		TSharedPtr<SLevelViewport> Viewport = Viewports[ViewportIndex];
		if (Viewport.IsValid())
		{
			return &Viewport->GetLevelViewportClient();
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> FUltimateControlViewportHandler::ViewportToJson(FLevelEditorViewportClient* ViewportClient, int32 Index)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetNumberField(TEXT("index"), Index);
	Result->SetStringField(TEXT("viewMode"), ViewModeToString(ViewportClient->GetViewMode()));
	Result->SetBoolField(TEXT("isRealtime"), ViewportClient->IsRealtime());
	Result->SetBoolField(TEXT("isPerspective"), ViewportClient->IsPerspective());

	// Camera location and rotation
	FVector Location = ViewportClient->GetViewLocation();
	FRotator Rotation = ViewportClient->GetViewRotation();

	Result->SetObjectField(TEXT("location"), VectorToJson(Location));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Rotation));

	// FOV
	Result->SetNumberField(TEXT("fov"), ViewportClient->ViewFOV);

	// Viewport type
	FString ViewportTypeName;
	switch (ViewportClient->GetViewportType())
	{
	case LVT_Perspective: ViewportTypeName = TEXT("Perspective"); break;
	case LVT_OrthoXY: ViewportTypeName = TEXT("OrthoXY"); break;
	case LVT_OrthoXZ: ViewportTypeName = TEXT("OrthoXZ"); break;
	case LVT_OrthoYZ: ViewportTypeName = TEXT("OrthoYZ"); break;
	case LVT_OrthoNegativeXY: ViewportTypeName = TEXT("OrthoNegativeXY"); break;
	case LVT_OrthoNegativeXZ: ViewportTypeName = TEXT("OrthoNegativeXZ"); break;
	case LVT_OrthoNegativeYZ: ViewportTypeName = TEXT("OrthoNegativeYZ"); break;
	case LVT_OrthoFreelook: ViewportTypeName = TEXT("OrthoFreelook"); break;
	default: ViewportTypeName = TEXT("Unknown"); break;
	}
	Result->SetStringField(TEXT("viewportType"), ViewportTypeName);

	return Result;
}

FString FUltimateControlViewportHandler::ViewModeToString(EViewModeIndex ViewMode)
{
	switch (ViewMode)
	{
	case VMI_BrushWireframe: return TEXT("BrushWireframe");
	case VMI_Wireframe: return TEXT("Wireframe");
	case VMI_Unlit: return TEXT("Unlit");
	case VMI_Lit: return TEXT("Lit");
	case VMI_Lit_DetailLighting: return TEXT("DetailLighting");
	case VMI_LightingOnly: return TEXT("LightingOnly");
	case VMI_LightComplexity: return TEXT("LightComplexity");
	case VMI_ShaderComplexity: return TEXT("ShaderComplexity");
	case VMI_StationaryLightOverlap: return TEXT("StationaryLightOverlap");
	case VMI_LightmapDensity: return TEXT("LightmapDensity");
	case VMI_ReflectionOverride: return TEXT("ReflectionOverride");
	case VMI_VisualizeBuffer: return TEXT("VisualizeBuffer");
	case VMI_CollisionPawn: return TEXT("CollisionPawn");
	case VMI_CollisionVisibility: return TEXT("CollisionVisibility");
	case VMI_PathTracing: return TEXT("PathTracing");
	case VMI_RayTracingDebug: return TEXT("RayTracingDebug");
	default: return TEXT("Lit");
	}
}

EViewModeIndex FUltimateControlViewportHandler::StringToViewMode(const FString& ViewModeStr)
{
	if (ViewModeStr == TEXT("BrushWireframe")) return VMI_BrushWireframe;
	if (ViewModeStr == TEXT("Wireframe")) return VMI_Wireframe;
	if (ViewModeStr == TEXT("Unlit")) return VMI_Unlit;
	if (ViewModeStr == TEXT("Lit")) return VMI_Lit;
	if (ViewModeStr == TEXT("DetailLighting")) return VMI_Lit_DetailLighting;
	if (ViewModeStr == TEXT("LightingOnly")) return VMI_LightingOnly;
	if (ViewModeStr == TEXT("LightComplexity")) return VMI_LightComplexity;
	if (ViewModeStr == TEXT("ShaderComplexity")) return VMI_ShaderComplexity;
	if (ViewModeStr == TEXT("StationaryLightOverlap")) return VMI_StationaryLightOverlap;
	if (ViewModeStr == TEXT("LightmapDensity")) return VMI_LightmapDensity;
	if (ViewModeStr == TEXT("ReflectionOverride")) return VMI_ReflectionOverride;
	if (ViewModeStr == TEXT("VisualizeBuffer")) return VMI_VisualizeBuffer;
	if (ViewModeStr == TEXT("CollisionPawn")) return VMI_CollisionPawn;
	if (ViewModeStr == TEXT("CollisionVisibility")) return VMI_CollisionVisibility;
	if (ViewModeStr == TEXT("PathTracing")) return VMI_PathTracing;
	if (ViewModeStr == TEXT("RayTracingDebug")) return VMI_RayTracingDebug;
	return VMI_Lit;
}

bool FUltimateControlViewportHandler::HandleListViewports(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	TArray<TSharedPtr<FJsonValue>> ViewportsArray;

	if (LevelEditor.IsValid())
	{
		TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
		for (int32 i = 0; i < Viewports.Num(); i++)
		{
			if (Viewports[i].IsValid())
			{
				FLevelEditorViewportClient& Client = Viewports[i]->GetLevelViewportClient();
				ViewportsArray.Add(MakeShared<FJsonValueObject>(ViewportToJson(&Client, i)));
			}
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("viewports"), ViewportsArray);
	ResultObj->SetNumberField(TEXT("count"), ViewportsArray.Num());

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleGetViewport(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorViewportClient* ViewportClient = GetViewportClient(Index);
	if (!ViewportClient)
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	Result = MakeShared<FJsonValueObject>(ViewportToJson(ViewportClient, Index));
	return true;
}

bool FUltimateControlViewportHandler::HandleGetCamera(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorViewportClient* ViewportClient = GetViewportClient(Index);
	if (!ViewportClient)
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	TSharedPtr<FJsonObject> CameraObj = MakeShared<FJsonObject>();
	CameraObj->SetObjectField(TEXT("location"), VectorToJson(ViewportClient->GetViewLocation()));
	CameraObj->SetObjectField(TEXT("rotation"), RotatorToJson(ViewportClient->GetViewRotation()));
	CameraObj->SetNumberField(TEXT("fov"), ViewportClient->ViewFOV);
	CameraObj->SetNumberField(TEXT("orthoZoom"), ViewportClient->GetOrthoZoom());
	CameraObj->SetBoolField(TEXT("isPerspective"), ViewportClient->IsPerspective());

	Result = MakeShared<FJsonValueObject>(CameraObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleSetCamera(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorViewportClient* ViewportClient = GetViewportClient(Index);
	if (!ViewportClient)
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	if (Params->HasField(TEXT("location")))
	{
		FVector Location = JsonToVector(Params->GetObjectField(TEXT("location")));
		ViewportClient->SetViewLocation(Location);
	}

	if (Params->HasField(TEXT("rotation")))
	{
		FRotator Rotation = JsonToRotator(Params->GetObjectField(TEXT("rotation")));
		ViewportClient->SetViewRotation(Rotation);
	}

	if (Params->HasField(TEXT("fov")))
	{
		ViewportClient->ViewFOV = Params->GetNumberField(TEXT("fov"));
	}

	if (Params->HasField(TEXT("orthoZoom")))
	{
		ViewportClient->SetOrthoZoom(Params->GetNumberField(TEXT("orthoZoom")));
	}

	ViewportClient->Invalidate();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleFocusOnActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = CreateError(-32002, TEXT("No world loaded"));
		return false;
	}

	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return false;
	}

	// Focus on the actor
	GEditor->MoveViewportCamerasToActor(*Actor, false);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleFocusOnLocation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("location")))
	{
		Error = CreateError(-32602, TEXT("Missing required parameter: location"));
		return false;
	}

	FVector Location = JsonToVector(Params->GetObjectField(TEXT("location")));

	float Distance = 500.0f;
	if (Params->HasField(TEXT("distance")))
	{
		Distance = Params->GetNumberField(TEXT("distance"));
	}

	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorViewportClient* ViewportClient = GetViewportClient(Index);
	if (!ViewportClient)
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	// Calculate camera position looking at the target
	FVector CameraLocation = Location - ViewportClient->GetViewRotation().Vector() * Distance;
	ViewportClient->SetViewLocation(CameraLocation);
	ViewportClient->Invalidate();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleGetViewportSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorViewportClient* ViewportClient = GetViewportClient(Index);
	if (!ViewportClient)
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();
	SettingsObj->SetBoolField(TEXT("realtime"), ViewportClient->IsRealtime());
	SettingsObj->SetStringField(TEXT("viewMode"), ViewModeToString(ViewportClient->GetViewMode()));
	SettingsObj->SetBoolField(TEXT("showStats"), ViewportClient->ShouldShowStats());
	SettingsObj->SetBoolField(TEXT("showFPS"), ViewportClient->ShouldShowFPS());
	SettingsObj->SetNumberField(TEXT("exposureSettings"), ViewportClient->ExposureSettings.FixedEV100);
	SettingsObj->SetNumberField(TEXT("farClipPlane"), ViewportClient->GetFarClipPlaneOverride());
	SettingsObj->SetNumberField(TEXT("cameraSpeedSetting"), ViewportClient->GetCameraSpeedSetting());

	Result = MakeShared<FJsonValueObject>(SettingsObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleSetViewportSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorViewportClient* ViewportClient = GetViewportClient(Index);
	if (!ViewportClient)
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	if (Params->HasField(TEXT("realtime")))
	{
		ViewportClient->SetRealtime(Params->GetBoolField(TEXT("realtime")));
	}

	if (Params->HasField(TEXT("viewMode")))
	{
		ViewportClient->SetViewMode(StringToViewMode(Params->GetStringField(TEXT("viewMode"))));
	}

	if (Params->HasField(TEXT("showStats")))
	{
		ViewportClient->SetShowStats(Params->GetBoolField(TEXT("showStats")));
	}

	if (Params->HasField(TEXT("exposureSettings")))
	{
		ViewportClient->ExposureSettings.FixedEV100 = Params->GetNumberField(TEXT("exposureSettings"));
	}

	if (Params->HasField(TEXT("farClipPlane")))
	{
		ViewportClient->OverrideFarClipPlane(Params->GetNumberField(TEXT("farClipPlane")));
	}

	if (Params->HasField(TEXT("cameraSpeedSetting")))
	{
		ViewportClient->SetCameraSpeedSetting(FMath::RoundToInt(Params->GetNumberField(TEXT("cameraSpeedSetting"))));
	}

	ViewportClient->Invalidate();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleSetViewMode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ViewMode;
	if (!RequireString(Params, TEXT("mode"), ViewMode, Error))
	{
		return false;
	}

	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorViewportClient* ViewportClient = GetViewportClient(Index);
	if (!ViewportClient)
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	ViewportClient->SetViewMode(StringToViewMode(ViewMode));
	ViewportClient->Invalidate();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleSetRealtime(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	bool bRealtime = true;
	if (Params->HasField(TEXT("enabled")))
	{
		bRealtime = Params->GetBoolField(TEXT("enabled"));
	}

	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorViewportClient* ViewportClient = GetViewportClient(Index);
	if (!ViewportClient)
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	ViewportClient->SetRealtime(bRealtime);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FString OutputPath;
	if (!RequireString(Params, TEXT("path"), OutputPath, Error))
	{
		return false;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	if (!LevelEditor.IsValid())
	{
		Error = CreateError(-32002, TEXT("Level editor not available"));
		return false;
	}

	TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
	if (Index < 0 || Index >= Viewports.Num() || !Viewports[Index].IsValid())
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	TSharedPtr<SLevelViewport> Viewport = Viewports[Index];
	TSharedPtr<FSceneViewport> SceneViewport = Viewport->GetSceneViewport();

	if (!SceneViewport.IsValid())
	{
		Error = CreateError(-32002, TEXT("Scene viewport not available"));
		return false;
	}

	// Get viewport size
	FIntPoint Size = SceneViewport->GetSizeXY();

	// Read pixels from viewport
	TArray<FColor> Bitmap;
	if (!SceneViewport->ReadPixels(Bitmap))
	{
		Error = CreateError(-32002, TEXT("Failed to read viewport pixels"));
		return false;
	}

	// Save to file
	TArray<uint8> CompressedBitmap;
	FImageUtils::CompressImageArray(Size.X, Size.Y, Bitmap, CompressedBitmap);

	if (!FFileHelper::SaveArrayToFile(CompressedBitmap, *OutputPath))
	{
		Error = CreateError(-32002, FString::Printf(TEXT("Failed to save screenshot to: %s"), *OutputPath));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("path"), OutputPath);
	ResultObj->SetNumberField(TEXT("width"), Size.X);
	ResultObj->SetNumberField(TEXT("height"), Size.Y);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleGetViewportSize(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	if (!LevelEditor.IsValid())
	{
		Error = CreateError(-32002, TEXT("Level editor not available"));
		return false;
	}

	TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
	if (Index < 0 || Index >= Viewports.Num() || !Viewports[Index].IsValid())
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	TSharedPtr<SLevelViewport> Viewport = Viewports[Index];
	TSharedPtr<FSceneViewport> SceneViewport = Viewport->GetSceneViewport();

	FIntPoint Size = SceneViewport.IsValid() ? SceneViewport->GetSizeXY() : FIntPoint(0, 0);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("width"), Size.X);
	ResultObj->SetNumberField(TEXT("height"), Size.Y);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleMaximizeViewport(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Index = 0;
	if (Params->HasField(TEXT("index")))
	{
		Index = FMath::RoundToInt(Params->GetNumberField(TEXT("index")));
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	if (!LevelEditor.IsValid())
	{
		Error = CreateError(-32002, TEXT("Level editor not available"));
		return false;
	}

	TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
	if (Index < 0 || Index >= Viewports.Num() || !Viewports[Index].IsValid())
	{
		Error = CreateError(-32003, TEXT("Viewport not found"));
		return false;
	}

	// Maximize viewport
	GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("VIEWPORT MAXIMIZED"));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlViewportHandler::HandleRestoreViewports(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Restore all viewports from maximized state
	GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("VIEWPORT RESTORE"));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
