// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlRenderHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/PostProcessVolume.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Scalability.h"
#include "ShowFlags.h"
#include "Engine/RendererSettings.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"

void FUltimateControlRenderHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	// Quality settings
	Methods.Add(TEXT("render.getQualitySettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetQualitySettings));
	Methods.Add(TEXT("render.setQualitySettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetQualitySettings));
	Methods.Add(TEXT("render.getScalabilityGroups"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetScalabilityGroups));
	Methods.Add(TEXT("render.setScalabilityGroup"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetScalabilityGroup));

	// Resolution
	Methods.Add(TEXT("render.getResolution"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetResolution));
	Methods.Add(TEXT("render.setResolution"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetResolution));
	Methods.Add(TEXT("render.getResolutionScale"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetResolutionScale));
	Methods.Add(TEXT("render.setResolutionScale"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetResolutionScale));

	// Frame rate
	Methods.Add(TEXT("render.getFrameRate"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetFrameRate));
	Methods.Add(TEXT("render.setTargetFrameRate"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetTargetFrameRate));
	Methods.Add(TEXT("render.getVSyncEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetVSyncEnabled));
	Methods.Add(TEXT("render.setVSyncEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetVSyncEnabled));

	// Rendering features
	Methods.Add(TEXT("render.getRaytracingEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetRaytracingEnabled));
	Methods.Add(TEXT("render.setRaytracingEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetRaytracingEnabled));
	Methods.Add(TEXT("render.getNaniteEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetNaniteEnabled));
	Methods.Add(TEXT("render.getLumenEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetLumenEnabled));
	Methods.Add(TEXT("render.setLumenEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetLumenEnabled));
	Methods.Add(TEXT("render.getVirtualShadowMapsEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetVirtualShadowMapsEnabled));

	// Post-process volumes
	Methods.Add(TEXT("postProcess.listVolumes"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleListPostProcessVolumes));
	Methods.Add(TEXT("postProcess.getVolume"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetPostProcessVolume));
	Methods.Add(TEXT("postProcess.createVolume"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleCreatePostProcessVolume));

	// Post-process settings
	Methods.Add(TEXT("postProcess.getSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetPostProcessSettings));
	Methods.Add(TEXT("postProcess.setSetting"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetPostProcessSetting));

	// Common PP settings shortcuts
	Methods.Add(TEXT("postProcess.setBloomIntensity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetBloomIntensity));
	Methods.Add(TEXT("postProcess.setExposure"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetExposure));
	Methods.Add(TEXT("postProcess.setMotionBlur"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetMotionBlurAmount));
	Methods.Add(TEXT("postProcess.setVignette"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetVignetteIntensity));
	Methods.Add(TEXT("postProcess.setDepthOfField"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetDepthOfField));
	Methods.Add(TEXT("postProcess.setColorGrading"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetColorGrading));
	Methods.Add(TEXT("postProcess.setAmbientOcclusion"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetAmbientOcclusion));
	Methods.Add(TEXT("postProcess.setFilmGrain"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetFilmGrain));
	Methods.Add(TEXT("postProcess.setChromaticAberration"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetChromaticAberration));

	// Show flags
	Methods.Add(TEXT("render.getShowFlags"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetShowFlags));
	Methods.Add(TEXT("render.setShowFlag"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetShowFlag));
	Methods.Add(TEXT("render.listShowFlags"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleListShowFlags));

	// Fog
	Methods.Add(TEXT("render.getFogSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleGetFogSettings));
	Methods.Add(TEXT("render.setFogSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlRenderHandler::HandleSetFogSettings));
}

TSharedPtr<FJsonObject> FUltimateControlRenderHandler::PostProcessVolumeToJson(APostProcessVolume* Volume)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Volume)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Volume->GetActorLabel());
	Json->SetBoolField(TEXT("enabled"), Volume->bEnabled);
	Json->SetBoolField(TEXT("unbound"), Volume->bUnbound);
	Json->SetNumberField(TEXT("priority"), Volume->Priority);
	Json->SetNumberField(TEXT("blendRadius"), Volume->BlendRadius);
	Json->SetNumberField(TEXT("blendWeight"), Volume->BlendWeight);

	FVector Location = Volume->GetActorLocation();
	TSharedPtr<FJsonObject> LocJson = MakeShared<FJsonObject>();
	LocJson->SetNumberField(TEXT("x"), Location.X);
	LocJson->SetNumberField(TEXT("y"), Location.Y);
	LocJson->SetNumberField(TEXT("z"), Location.Z);
	Json->SetObjectField(TEXT("location"), LocJson);

	return Json;
}

TSharedPtr<FJsonObject> FUltimateControlRenderHandler::PostProcessSettingsToJson(const FPostProcessSettings& Settings)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	// Bloom
	Json->SetNumberField(TEXT("bloomIntensity"), Settings.BloomIntensity);
	Json->SetNumberField(TEXT("bloomThreshold"), Settings.BloomThreshold);

	// Exposure
	Json->SetNumberField(TEXT("exposureCompensation"), Settings.AutoExposureBias);
	Json->SetNumberField(TEXT("exposureMinBrightness"), Settings.AutoExposureMinBrightness);
	Json->SetNumberField(TEXT("exposureMaxBrightness"), Settings.AutoExposureMaxBrightness);

	// Motion blur
	Json->SetNumberField(TEXT("motionBlurAmount"), Settings.MotionBlurAmount);
	Json->SetNumberField(TEXT("motionBlurMax"), Settings.MotionBlurMax);

	// Vignette
	Json->SetNumberField(TEXT("vignetteIntensity"), Settings.VignetteIntensity);

	// Film grain
	Json->SetNumberField(TEXT("filmGrainIntensity"), Settings.FilmGrainIntensity);

	// Chromatic aberration
	Json->SetNumberField(TEXT("chromaticAberrationIntensity"), Settings.SceneFringeIntensity);

	// Ambient occlusion
	Json->SetNumberField(TEXT("aoIntensity"), Settings.AmbientOcclusionIntensity);

	return Json;
}

APostProcessVolume* FUltimateControlRenderHandler::FindPostProcessVolume(const FString& VolumeName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == VolumeName)
		{
			return *It;
		}
	}

	return nullptr;
}

bool FUltimateControlRenderHandler::HandleGetQualitySettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Game user settings not available"));
		return true;
	}

	TSharedPtr<FJsonObject> QualityJson = MakeShared<FJsonObject>();

	QualityJson->SetNumberField(TEXT("overallQuality"), Settings->GetOverallScalabilityLevel());
	QualityJson->SetNumberField(TEXT("viewDistanceQuality"), Settings->GetViewDistanceQuality());
	QualityJson->SetNumberField(TEXT("antiAliasingQuality"), Settings->GetAntiAliasingQuality());
	QualityJson->SetNumberField(TEXT("shadowQuality"), Settings->GetShadowQuality());
	QualityJson->SetNumberField(TEXT("globalIlluminationQuality"), Settings->GetGlobalIlluminationQuality());
	QualityJson->SetNumberField(TEXT("reflectionQuality"), Settings->GetReflectionQuality());
	QualityJson->SetNumberField(TEXT("postProcessQuality"), Settings->GetPostProcessingQuality());
	QualityJson->SetNumberField(TEXT("textureQuality"), Settings->GetTextureQuality());
	QualityJson->SetNumberField(TEXT("effectsQuality"), Settings->GetVisualEffectQuality());
	QualityJson->SetNumberField(TEXT("foliageQuality"), Settings->GetFoliageQuality());
	QualityJson->SetNumberField(TEXT("shadingQuality"), Settings->GetShadingQuality());

	Result = MakeShared<FJsonValueObject>(QualityJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetQualitySettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Game user settings not available"));
		return true;
	}

	if (Params->HasField(TEXT("overallQuality")))
	{
		Settings->SetOverallScalabilityLevel(static_cast<int32>(Params->GetIntegerField(TEXT("overallQuality"))));
	}

	if (Params->HasField(TEXT("viewDistanceQuality")))
	{
		Settings->SetViewDistanceQuality(static_cast<int32>(Params->GetIntegerField(TEXT("viewDistanceQuality"))));
	}

	if (Params->HasField(TEXT("antiAliasingQuality")))
	{
		Settings->SetAntiAliasingQuality(static_cast<int32>(Params->GetIntegerField(TEXT("antiAliasingQuality"))));
	}

	if (Params->HasField(TEXT("shadowQuality")))
	{
		Settings->SetShadowQuality(static_cast<int32>(Params->GetIntegerField(TEXT("shadowQuality"))));
	}

	if (Params->HasField(TEXT("textureQuality")))
	{
		Settings->SetTextureQuality(static_cast<int32>(Params->GetIntegerField(TEXT("textureQuality"))));
	}

	Settings->ApplySettings(true);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetScalabilityGroups(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	Scalability::FQualityLevels QualityLevels = Scalability::GetQualityLevels();

	TSharedPtr<FJsonObject> GroupsJson = MakeShared<FJsonObject>();
	GroupsJson->SetNumberField(TEXT("resolutionQuality"), QualityLevels.ResolutionQuality);
	GroupsJson->SetNumberField(TEXT("viewDistanceQuality"), QualityLevels.ViewDistanceQuality);
	GroupsJson->SetNumberField(TEXT("antiAliasingQuality"), QualityLevels.AntiAliasingQuality);
	GroupsJson->SetNumberField(TEXT("shadowQuality"), QualityLevels.ShadowQuality);
	GroupsJson->SetNumberField(TEXT("globalIlluminationQuality"), QualityLevels.GlobalIlluminationQuality);
	GroupsJson->SetNumberField(TEXT("reflectionQuality"), QualityLevels.ReflectionQuality);
	GroupsJson->SetNumberField(TEXT("postProcessQuality"), QualityLevels.PostProcessQuality);
	GroupsJson->SetNumberField(TEXT("textureQuality"), QualityLevels.TextureQuality);
	GroupsJson->SetNumberField(TEXT("effectsQuality"), QualityLevels.EffectsQuality);
	GroupsJson->SetNumberField(TEXT("foliageQuality"), QualityLevels.FoliageQuality);
	GroupsJson->SetNumberField(TEXT("shadingQuality"), QualityLevels.ShadingQuality);

	Result = MakeShared<FJsonValueObject>(GroupsJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetScalabilityGroup(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString GroupName = Params->GetStringField(TEXT("group"));
	int32 Level = static_cast<int32>(Params->GetIntegerField(TEXT("level")));

	if (GroupName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("group parameter required"));
		return true;
	}

	Scalability::FQualityLevels QualityLevels = Scalability::GetQualityLevels();

	if (GroupName == TEXT("resolution")) QualityLevels.ResolutionQuality = Level;
	else if (GroupName == TEXT("viewDistance")) QualityLevels.ViewDistanceQuality = Level;
	else if (GroupName == TEXT("antiAliasing")) QualityLevels.AntiAliasingQuality = Level;
	else if (GroupName == TEXT("shadow")) QualityLevels.ShadowQuality = Level;
	else if (GroupName == TEXT("globalIllumination")) QualityLevels.GlobalIlluminationQuality = Level;
	else if (GroupName == TEXT("reflection")) QualityLevels.ReflectionQuality = Level;
	else if (GroupName == TEXT("postProcess")) QualityLevels.PostProcessQuality = Level;
	else if (GroupName == TEXT("texture")) QualityLevels.TextureQuality = Level;
	else if (GroupName == TEXT("effects")) QualityLevels.EffectsQuality = Level;
	else if (GroupName == TEXT("foliage")) QualityLevels.FoliageQuality = Level;
	else if (GroupName == TEXT("shading")) QualityLevels.ShadingQuality = Level;

	Scalability::SetQualityLevels(QualityLevels);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetResolution(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Game user settings not available"));
		return true;
	}

	FIntPoint Resolution = Settings->GetScreenResolution();

	TSharedPtr<FJsonObject> ResJson = MakeShared<FJsonObject>();
	ResJson->SetNumberField(TEXT("width"), Resolution.X);
	ResJson->SetNumberField(TEXT("height"), Resolution.Y);
	ResJson->SetBoolField(TEXT("fullscreen"), Settings->GetFullscreenMode() == EWindowMode::Fullscreen);

	Result = MakeShared<FJsonValueObject>(ResJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetResolution(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 Width = static_cast<int32>(Params->GetIntegerField(TEXT("width")));
	int32 Height = static_cast<int32>(Params->GetIntegerField(TEXT("height")));
	bool bFullscreen = Params->HasField(TEXT("fullscreen")) ? Params->GetBoolField(TEXT("fullscreen")) : false;

	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Game user settings not available"));
		return true;
	}

	Settings->SetScreenResolution(FIntPoint(Width, Height));
	Settings->SetFullscreenMode(bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);
	Settings->ApplySettings(true);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetResolutionScale(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Game user settings not available"));
		return true;
	}

	TSharedPtr<FJsonObject> ScaleJson = MakeShared<FJsonObject>();
	ScaleJson->SetNumberField(TEXT("resolutionScale"), Settings->GetResolutionScaleNormalized() * 100.0f);

	Result = MakeShared<FJsonValueObject>(ScaleJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetResolutionScale(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	float Scale = static_cast<float>(Params->GetNumberField(TEXT("scale")));

	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Game user settings not available"));
		return true;
	}

	Settings->SetResolutionScaleNormalized(Scale / 100.0f);
	Settings->ApplySettings(true);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetFrameRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();

	TSharedPtr<FJsonObject> FPSJson = MakeShared<FJsonObject>();
	FPSJson->SetNumberField(TEXT("targetFrameRate"), Settings ? Settings->GetFrameRateLimit() : 0.0f);
	FPSJson->SetNumberField(TEXT("currentFPS"), 1.0f / FApp::GetDeltaTime());

	Result = MakeShared<FJsonValueObject>(FPSJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetTargetFrameRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	float TargetFPS = static_cast<float>(Params->GetNumberField(TEXT("fps")));

	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Game user settings not available"));
		return true;
	}

	Settings->SetFrameRateLimit(TargetFPS);
	Settings->ApplySettings(true);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetVSyncEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	Result = MakeShared<FJsonValueBoolean>(Settings ? Settings->IsVSyncEnabled() : false);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetVSyncEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	bool bEnabled = Params->GetBoolField(TEXT("enabled"));

	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Game user settings not available"));
		return true;
	}

	Settings->SetVSyncEnabled(bEnabled);
	Settings->ApplySettings(true);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetRaytracingEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> RTJson = MakeShared<FJsonObject>();

	// Check if raytracing is supported and enabled
	static const auto CVarRaytracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
	bool bRaytracingEnabled = CVarRaytracing ? CVarRaytracing->GetInt() != 0 : false;

	RTJson->SetBoolField(TEXT("enabled"), bRaytracingEnabled);
	RTJson->SetBoolField(TEXT("supported"), GRHISupportsRayTracing);

	Result = MakeShared<FJsonValueObject>(RTJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetRaytracingEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	bool bEnabled = Params->GetBoolField(TEXT("enabled"));

	static const auto CVarRaytracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
	if (CVarRaytracing)
	{
		CVarRaytracing->Set(bEnabled ? 1 : 0);
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetNaniteEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> NaniteJson = MakeShared<FJsonObject>();

	static const auto CVarNanite = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
	bool bNaniteEnabled = CVarNanite ? CVarNanite->GetInt() != 0 : false;

	NaniteJson->SetBoolField(TEXT("enabled"), bNaniteEnabled);

	Result = MakeShared<FJsonValueObject>(NaniteJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetLumenEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> LumenJson = MakeShared<FJsonObject>();

	static const auto CVarLumenGI = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow"));
	static const auto CVarLumenReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Reflections.Allow"));

	LumenJson->SetBoolField(TEXT("giEnabled"), CVarLumenGI ? CVarLumenGI->GetInt() != 0 : false);
	LumenJson->SetBoolField(TEXT("reflectionsEnabled"), CVarLumenReflections ? CVarLumenReflections->GetInt() != 0 : false);

	Result = MakeShared<FJsonValueObject>(LumenJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetLumenEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	bool bGIEnabled = Params->HasField(TEXT("giEnabled")) ? Params->GetBoolField(TEXT("giEnabled")) : true;
	bool bReflectionsEnabled = Params->HasField(TEXT("reflectionsEnabled")) ? Params->GetBoolField(TEXT("reflectionsEnabled")) : true;

	static auto CVarLumenGI = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow"));
	static auto CVarLumenReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Reflections.Allow"));

	if (CVarLumenGI) CVarLumenGI->Set(bGIEnabled ? 1 : 0);
	if (CVarLumenReflections) CVarLumenReflections->Set(bReflectionsEnabled ? 1 : 0);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetVirtualShadowMapsEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> VSMJson = MakeShared<FJsonObject>();

	static const auto CVarVSM = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable"));
	VSMJson->SetBoolField(TEXT("enabled"), CVarVSM ? CVarVSM->GetInt() != 0 : false);

	Result = MakeShared<FJsonValueObject>(VSMJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleListPostProcessVolumes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> VolumesArray;

	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		APostProcessVolume* Volume = *It;
		if (Volume)
		{
			VolumesArray.Add(MakeShared<FJsonValueObject>(PostProcessVolumeToJson(Volume)));
		}
	}

	Result = MakeShared<FJsonValueArray>(VolumesArray);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetPostProcessVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("name"));
	if (VolumeName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	TSharedPtr<FJsonObject> VolumeJson = PostProcessVolumeToJson(Volume);
	VolumeJson->SetObjectField(TEXT("settings"), PostProcessSettingsToJson(Volume->Settings));

	Result = MakeShared<FJsonValueObject>(VolumeJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleCreatePostProcessVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	bool bUnbound = Params->HasField(TEXT("unbound")) ? Params->GetBoolField(TEXT("unbound")) : true;
	double Priority = Params->HasField(TEXT("priority")) ? Params->GetNumberField(TEXT("priority")) : 0.0;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>();
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("Failed to spawn post process volume"));
		return true;
	}

	Volume->bUnbound = bUnbound;
	Volume->Priority = static_cast<float>(Priority);

	Result = MakeShared<FJsonValueObject>(PostProcessVolumeToJson(Volume));
	return true;
}

bool FUltimateControlRenderHandler::HandleGetPostProcessSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("name"));
	if (VolumeName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name parameter required"));
		return true;
	}

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(PostProcessSettingsToJson(Volume->Settings));
	return true;
}

bool FUltimateControlRenderHandler::HandleSetPostProcessSetting(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("name"));
	FString SettingName = Params->GetStringField(TEXT("setting"));
	float Value = static_cast<float>(Params->GetNumberField(TEXT("value")));

	if (VolumeName.IsEmpty() || SettingName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("name and setting parameters required"));
		return true;
	}

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	// Set the appropriate setting
	if (SettingName == TEXT("bloomIntensity"))
	{
		Volume->Settings.bOverride_BloomIntensity = true;
		Volume->Settings.BloomIntensity = Value;
	}
	else if (SettingName == TEXT("exposureCompensation"))
	{
		Volume->Settings.bOverride_AutoExposureBias = true;
		Volume->Settings.AutoExposureBias = Value;
	}
	else if (SettingName == TEXT("motionBlurAmount"))
	{
		Volume->Settings.bOverride_MotionBlurAmount = true;
		Volume->Settings.MotionBlurAmount = Value;
	}
	else if (SettingName == TEXT("vignetteIntensity"))
	{
		Volume->Settings.bOverride_VignetteIntensity = true;
		Volume->Settings.VignetteIntensity = Value;
	}

	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetBloomIntensity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));
	float Intensity = static_cast<float>(Params->GetNumberField(TEXT("intensity")));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	Volume->Settings.bOverride_BloomIntensity = true;
	Volume->Settings.BloomIntensity = Intensity;
	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetExposure(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));
	float Bias = static_cast<float>(Params->GetNumberField(TEXT("bias")));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	Volume->Settings.bOverride_AutoExposureBias = true;
	Volume->Settings.AutoExposureBias = Bias;

	if (Params->HasField(TEXT("minBrightness")))
	{
		Volume->Settings.bOverride_AutoExposureMinBrightness = true;
		Volume->Settings.AutoExposureMinBrightness = static_cast<float>(Params->GetNumberField(TEXT("minBrightness")));
	}

	if (Params->HasField(TEXT("maxBrightness")))
	{
		Volume->Settings.bOverride_AutoExposureMaxBrightness = true;
		Volume->Settings.AutoExposureMaxBrightness = static_cast<float>(Params->GetNumberField(TEXT("maxBrightness")));
	}

	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetMotionBlurAmount(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));
	float Amount = static_cast<float>(Params->GetNumberField(TEXT("amount")));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	Volume->Settings.bOverride_MotionBlurAmount = true;
	Volume->Settings.MotionBlurAmount = Amount;
	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetVignetteIntensity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));
	float Intensity = static_cast<float>(Params->GetNumberField(TEXT("intensity")));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	Volume->Settings.bOverride_VignetteIntensity = true;
	Volume->Settings.VignetteIntensity = Intensity;
	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetDepthOfField(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	if (Params->HasField(TEXT("focalDistance")))
	{
		Volume->Settings.bOverride_DepthOfFieldFocalDistance = true;
		Volume->Settings.DepthOfFieldFocalDistance = static_cast<float>(Params->GetNumberField(TEXT("focalDistance")));
	}

	if (Params->HasField(TEXT("fstop")))
	{
		Volume->Settings.bOverride_DepthOfFieldFstop = true;
		Volume->Settings.DepthOfFieldFstop = static_cast<float>(Params->GetNumberField(TEXT("fstop")));
	}

	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetColorGrading(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	if (Params->HasField(TEXT("saturation")))
	{
		Volume->Settings.bOverride_ColorSaturation = true;
		float Sat = static_cast<float>(Params->GetNumberField(TEXT("saturation")));
		Volume->Settings.ColorSaturation = FVector4(Sat, Sat, Sat, Sat);
	}

	if (Params->HasField(TEXT("contrast")))
	{
		Volume->Settings.bOverride_ColorContrast = true;
		float Con = static_cast<float>(Params->GetNumberField(TEXT("contrast")));
		Volume->Settings.ColorContrast = FVector4(Con, Con, Con, Con);
	}

	if (Params->HasField(TEXT("gamma")))
	{
		Volume->Settings.bOverride_ColorGamma = true;
		float Gam = static_cast<float>(Params->GetNumberField(TEXT("gamma")));
		Volume->Settings.ColorGamma = FVector4(Gam, Gam, Gam, Gam);
	}

	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetAmbientOcclusion(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));
	float Intensity = static_cast<float>(Params->GetNumberField(TEXT("intensity")));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	Volume->Settings.bOverride_AmbientOcclusionIntensity = true;
	Volume->Settings.AmbientOcclusionIntensity = Intensity;
	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetFilmGrain(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));
	float Intensity = static_cast<float>(Params->GetNumberField(TEXT("intensity")));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	Volume->Settings.bOverride_FilmGrainIntensity = true;
	Volume->Settings.FilmGrainIntensity = Intensity;
	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetChromaticAberration(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString VolumeName = Params->GetStringField(TEXT("volumeName"));
	float Intensity = static_cast<float>(Params->GetNumberField(TEXT("intensity")));

	APostProcessVolume* Volume = FindPostProcessVolume(VolumeName);
	if (!Volume)
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, FString::Printf(TEXT("Post process volume not found: %s"), *VolumeName));
		return true;
	}

	Volume->Settings.bOverride_SceneFringeIntensity = true;
	Volume->Settings.SceneFringeIntensity = Intensity;
	Volume->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetShowFlags(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Get the active viewport's show flags
	if (!GEditor || !GEditor->GetActiveViewport())
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No active viewport"));
		return true;
	}

	const FEngineShowFlags& ShowFlags = GEditor->GetActiveViewport()->GetClient()->GetEngineShowFlags();

	TSharedPtr<FJsonObject> FlagsJson = MakeShared<FJsonObject>();
	FlagsJson->SetBoolField(TEXT("staticMeshes"), ShowFlags.StaticMeshes != 0);
	FlagsJson->SetBoolField(TEXT("skeletalMeshes"), ShowFlags.SkeletalMeshes != 0);
	FlagsJson->SetBoolField(TEXT("landscape"), ShowFlags.Landscape != 0);
	FlagsJson->SetBoolField(TEXT("fog"), ShowFlags.Fog != 0);
	FlagsJson->SetBoolField(TEXT("particles"), ShowFlags.Particles != 0);
	FlagsJson->SetBoolField(TEXT("lighting"), ShowFlags.Lighting != 0);
	FlagsJson->SetBoolField(TEXT("postProcessing"), ShowFlags.PostProcessing != 0);
	FlagsJson->SetBoolField(TEXT("antiAliasing"), ShowFlags.AntiAliasing != 0);
	FlagsJson->SetBoolField(TEXT("temporalAA"), ShowFlags.TemporalAA != 0);
	FlagsJson->SetBoolField(TEXT("bloom"), ShowFlags.Bloom != 0);
	FlagsJson->SetBoolField(TEXT("motionBlur"), ShowFlags.MotionBlur != 0);
	FlagsJson->SetBoolField(TEXT("ambientOcclusion"), ShowFlags.AmbientOcclusion != 0);

	Result = MakeShared<FJsonValueObject>(FlagsJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetShowFlag(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString FlagName = Params->GetStringField(TEXT("flag"));
	bool bEnabled = Params->GetBoolField(TEXT("enabled"));

	if (FlagName.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("flag parameter required"));
		return true;
	}

	if (!GEditor || !GEditor->GetActiveViewport())
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No active viewport"));
		return true;
	}

	// This sets the show flag via console command
	FString Command = FString::Printf(TEXT("ShowFlag.%s %d"), *FlagName, bEnabled ? 1 : 0);
	GEngine->Exec(nullptr, *Command);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleListShowFlags(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> FlagsArray;

	// Common show flags
	TArray<FString> CommonFlags = {
		TEXT("StaticMeshes"), TEXT("SkeletalMeshes"), TEXT("Landscape"),
		TEXT("Fog"), TEXT("Particles"), TEXT("Lighting"), TEXT("PostProcessing"),
		TEXT("AntiAliasing"), TEXT("TemporalAA"), TEXT("Bloom"), TEXT("MotionBlur"),
		TEXT("AmbientOcclusion"), TEXT("DynamicShadows"), TEXT("Decals"),
		TEXT("BSP"), TEXT("Grid"), TEXT("Collision"), TEXT("Bounds"),
		TEXT("Navigation"), TEXT("Splines"), TEXT("Volumes"), TEXT("Sprites")
	};

	for (const FString& Flag : CommonFlags)
	{
		FlagsArray.Add(MakeShared<FJsonValueString>(Flag));
	}

	Result = MakeShared<FJsonValueArray>(FlagsArray);
	return true;
}

bool FUltimateControlRenderHandler::HandleGetFogSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	TSharedPtr<FJsonObject> FogJson = MakeShared<FJsonObject>();

	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		AExponentialHeightFog* FogActor = *It;
		if (FogActor && FogActor->GetComponent())
		{
			UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();

			FogJson->SetStringField(TEXT("name"), FogActor->GetActorLabel());
			FogJson->SetNumberField(TEXT("density"), FogComp->FogDensity);
			FogJson->SetNumberField(TEXT("heightFalloff"), FogComp->FogHeightFalloff);
			FogJson->SetNumberField(TEXT("startDistance"), FogComp->StartDistance);
			FogJson->SetNumberField(TEXT("maxOpacity"), FogComp->FogMaxOpacity);

			// Note: FogInscatteringColor has been deprecated in UE 5.6
			// Using InscatteringColorCubemap or other fog color properties instead
			FLinearColor FogColor = FLinearColor::White;
			if (FogComp->InscatteringColorCubemap)
			{
				// If cubemap is set, use a default or derive from other properties
				FogColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
			TSharedPtr<FJsonObject> ColorJson = MakeShared<FJsonObject>();
			ColorJson->SetNumberField(TEXT("r"), FogColor.R);
			ColorJson->SetNumberField(TEXT("g"), FogColor.G);
			ColorJson->SetNumberField(TEXT("b"), FogColor.B);
			FogJson->SetObjectField(TEXT("color"), ColorJson);

			break; // Get first fog actor
		}
	}

	Result = MakeShared<FJsonValueObject>(FogJson);
	return true;
}

bool FUltimateControlRenderHandler::HandleSetFogSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No editor world available"));
		return true;
	}

	AExponentialHeightFog* FogActor = nullptr;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		FogActor = *It;
		break;
	}

	if (!FogActor || !FogActor->GetComponent())
	{
		Error = UUltimateControlSubsystem::MakeError(-32603, TEXT("No fog actor found in scene"));
		return true;
	}

	UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();

	if (Params->HasField(TEXT("density")))
	{
		FogComp->FogDensity = static_cast<float>(Params->GetNumberField(TEXT("density")));
	}

	if (Params->HasField(TEXT("heightFalloff")))
	{
		FogComp->FogHeightFalloff = static_cast<float>(Params->GetNumberField(TEXT("heightFalloff")));
	}

	if (Params->HasField(TEXT("startDistance")))
	{
		FogComp->StartDistance = static_cast<float>(Params->GetNumberField(TEXT("startDistance")));
	}

	if (Params->HasField(TEXT("maxOpacity")))
	{
		FogComp->FogMaxOpacity = static_cast<float>(Params->GetNumberField(TEXT("maxOpacity")));
	}

	FogComp->MarkRenderStateDirty();
	FogActor->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}
