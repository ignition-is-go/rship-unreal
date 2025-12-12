// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handlers/UltimateControlHandlerBase.h"

class APostProcessVolume;
class UPostProcessComponent;

/**
 * Handler for Rendering and Post-Process operations
 */
class ULTIMATECONTROL_API FUltimateControlRenderHandler : public FUltimateControlHandlerBase
{
public:
	using FUltimateControlHandlerBase::FUltimateControlHandlerBase;

private:
	// Quality settings
	bool HandleGetQualitySettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetQualitySettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetScalabilityGroups(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetScalabilityGroup(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Resolution
	bool HandleGetResolution(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetResolution(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetResolutionScale(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetResolutionScale(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Frame rate
	bool HandleGetFrameRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetTargetFrameRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetVSyncEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetVSyncEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Rendering features
	bool HandleGetRaytracingEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetRaytracingEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetNaniteEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetLumenEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetLumenEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetVirtualShadowMapsEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Post-process volumes
	bool HandleListPostProcessVolumes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetPostProcessVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleCreatePostProcessVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Post-process settings
	bool HandleGetPostProcessSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetPostProcessSetting(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Common PP settings shortcuts
	bool HandleSetBloomIntensity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetExposure(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetMotionBlurAmount(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetVignetteIntensity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetDepthOfField(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetColorGrading(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetAmbientOcclusion(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetFilmGrain(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetChromaticAberration(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Show flags
	bool HandleGetShowFlags(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetShowFlag(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleListShowFlags(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Fog
	bool HandleGetFogSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetFogSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Helper methods
	TSharedPtr<FJsonObject> PostProcessVolumeToJson(APostProcessVolume* Volume);
	TSharedPtr<FJsonObject> PostProcessSettingsToJson(const FPostProcessSettings& Settings);
	APostProcessVolume* FindPostProcessVolume(const FString& VolumeName);
};
