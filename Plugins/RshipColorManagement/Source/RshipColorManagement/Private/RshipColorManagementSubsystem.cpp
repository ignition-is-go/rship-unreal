// Copyright Lucid. All Rights Reserved.

#include "RshipColorManagementSubsystem.h"
#include "RshipColorManagement.h"

#include "Engine/PostProcessVolume.h"
#include "Components/SceneCaptureComponent2D.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void URshipColorManagementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogRshipColor, Log, TEXT("RshipColorManagementSubsystem initialized"));

	// Set default config
	ActiveConfig = FRshipColorConfig();
}

void URshipColorManagementSubsystem::Deinitialize()
{
	RemoveViewportOverrides();

	UE_LOG(LogRshipColor, Log, TEXT("RshipColorManagementSubsystem deinitialized"));

	Super::Deinitialize();
}

bool URshipColorManagementSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Create in all worlds (game, editor, PIE)
	return true;
}

void URshipColorManagementSubsystem::SetColorConfig(const FRshipColorConfig& NewConfig)
{
	ActiveConfig = NewConfig;

	UE_LOG(LogRshipColor, Log, TEXT("Color config updated: %s"), *ActiveConfig.GetDescription());

	// Update viewport if sync is enabled
	if (ActiveConfig.bSyncExposureToViewport)
	{
		ApplyToViewport();
	}

	// Broadcast change
	OnColorConfigChanged.Broadcast(ActiveConfig);
}

void URshipColorManagementSubsystem::ApplyToViewport()
{
	// Update console variables for global exposure control
	UpdateViewportCVars();

	// Create or update control volume
	if (!ColorControlVolume.IsValid())
	{
		ColorControlVolume = CreateColorControlVolume(1000.0f);  // High priority
	}

	if (ColorControlVolume.IsValid())
	{
		ApplyConfigToPostProcessSettings(ColorControlVolume->Settings);
		ColorControlVolume->bUnbound = true;
		ColorControlVolume->bEnabled = true;

		UE_LOG(LogRshipColor, Log, TEXT("Applied color config to viewport via PostProcessVolume"));
	}
}

void URshipColorManagementSubsystem::ApplyToCamera(ACineCameraActor* Camera)
{
	if (!Camera)
	{
		return;
	}

	UCineCameraComponent* CineCamera = Camera->GetCineCameraComponent();
	if (!CineCamera)
	{
		return;
	}

	ApplyConfigToPostProcessSettings(CineCamera->PostProcessSettings);

	UE_LOG(LogRshipColor, Log, TEXT("Applied color config to camera: %s"), *Camera->GetName());
}

void URshipColorManagementSubsystem::RemoveViewportOverrides()
{
	// Restore default CVars
	if (GEngine)
	{
		GEngine->Exec(nullptr, TEXT("r.EyeAdaptationQuality 2"));
		GEngine->Exec(nullptr, TEXT("r.DefaultFeature.AutoExposure 1"));
	}

	// Disable control volume
	if (ColorControlVolume.IsValid())
	{
		ColorControlVolume->bEnabled = false;

		UE_LOG(LogRshipColor, Log, TEXT("Removed viewport color overrides"));
	}
}

void URshipColorManagementSubsystem::ConfigureSceneCapture(USceneCaptureComponent2D* Capture)
{
	if (!Capture)
	{
		return;
	}

	// Set capture source based on config
	Capture->CaptureSource = GetCaptureSource();

	// Apply post-process settings
	Capture->PostProcessSettings = GetPostProcessSettings();
	Capture->PostProcessBlendWeight = 1.0f;

	// Set eye adaptation based on exposure mode
	Capture->ShowFlags.SetEyeAdaptation(ShouldEnableEyeAdaptation());

	UE_LOG(LogRshipColor, Verbose, TEXT("Configured scene capture with color settings"));
}

FPostProcessSettings URshipColorManagementSubsystem::GetPostProcessSettings() const
{
	FPostProcessSettings Settings;
	ApplyConfigToPostProcessSettings(Settings);
	return Settings;
}

bool URshipColorManagementSubsystem::ShouldEnableEyeAdaptation() const
{
	// Enable eye adaptation only for Auto exposure mode
	return ActiveConfig.Exposure.Mode == ERshipExposureMode::Auto;
}

ESceneCaptureSource URshipColorManagementSubsystem::GetCaptureSource() const
{
	switch (ActiveConfig.CaptureMode)
	{
	case ERshipCaptureMode::FinalColorLDR:
		return ESceneCaptureSource::SCS_FinalColorLDR;

	case ERshipCaptureMode::SceneColorHDR:
		return ESceneCaptureSource::SCS_SceneColorHDR;

	case ERshipCaptureMode::RawSceneColor:
		return ESceneCaptureSource::SCS_SceneColorHDRNoAlpha;

	default:
		return ESceneCaptureSource::SCS_FinalColorLDR;
	}
}

void URshipColorManagementSubsystem::ApplyConfigToPostProcessSettings(FPostProcessSettings& Settings) const
{
	// ==== EXPOSURE ====
	switch (ActiveConfig.Exposure.Mode)
	{
	case ERshipExposureMode::Manual:
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;

		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = ActiveConfig.Exposure.ManualExposureEV + ActiveConfig.Exposure.ExposureBias;
		break;

	case ERshipExposureMode::Auto:
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;

		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = ActiveConfig.Exposure.ExposureBias;

		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.AutoExposureMinBrightness = ActiveConfig.Exposure.AutoExposureMinBrightness;

		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.AutoExposureMaxBrightness = ActiveConfig.Exposure.AutoExposureMaxBrightness;

		Settings.bOverride_AutoExposureSpeedUp = true;
		Settings.AutoExposureSpeedUp = 1.0f / ActiveConfig.Exposure.AutoExposureSpeed;

		Settings.bOverride_AutoExposureSpeedDown = true;
		Settings.AutoExposureSpeedDown = 1.0f / ActiveConfig.Exposure.AutoExposureSpeed;
		break;

	case ERshipExposureMode::Histogram:
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;

		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = ActiveConfig.Exposure.ExposureBias;

		// Lock min/max to same value for fixed histogram exposure
		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.AutoExposureMinBrightness = ActiveConfig.Exposure.AutoExposureMinBrightness;

		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.AutoExposureMaxBrightness = ActiveConfig.Exposure.AutoExposureMinBrightness;  // Same as min = locked
		break;
	}

	// ==== TONEMAPPING ====
	if (ActiveConfig.Tonemap.bEnabled)
	{
		Settings.bOverride_FilmSlope = true;
		Settings.FilmSlope = ActiveConfig.Tonemap.Slope;

		Settings.bOverride_FilmToe = true;
		Settings.FilmToe = ActiveConfig.Tonemap.Toe;

		Settings.bOverride_FilmShoulder = true;
		Settings.FilmShoulder = ActiveConfig.Tonemap.Shoulder;

		Settings.bOverride_FilmBlackClip = true;
		Settings.FilmBlackClip = ActiveConfig.Tonemap.BlackClip;

		Settings.bOverride_FilmWhiteClip = true;
		Settings.FilmWhiteClip = ActiveConfig.Tonemap.WhiteClip;
	}
}

void URshipColorManagementSubsystem::UpdateViewportCVars()
{
	if (!GEngine)
	{
		return;
	}

	switch (ActiveConfig.Exposure.Mode)
	{
	case ERshipExposureMode::Manual:
		// Disable auto-exposure globally for manual mode
		GEngine->Exec(nullptr, TEXT("r.EyeAdaptationQuality 0"));
		GEngine->Exec(nullptr, TEXT("r.DefaultFeature.AutoExposure 0"));
		break;

	case ERshipExposureMode::Auto:
	case ERshipExposureMode::Histogram:
		// Enable auto-exposure
		GEngine->Exec(nullptr, TEXT("r.EyeAdaptationQuality 2"));
		GEngine->Exec(nullptr, TEXT("r.DefaultFeature.AutoExposure 1"));
		break;
	}
}

APostProcessVolume* URshipColorManagementSubsystem::CreateColorControlVolume(float Priority)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = TEXT("RshipColorControlVolume");
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>(SpawnParams);
	if (Volume)
	{
		Volume->bUnbound = true;  // Affects entire world
		Volume->Priority = Priority;  // High priority to override others
		Volume->bEnabled = true;

#if WITH_EDITOR
		Volume->SetActorLabel(TEXT("Rship Color Control"));
#endif

		UE_LOG(LogRshipColor, Log, TEXT("Created color control PostProcessVolume with priority %.1f"), Priority);
	}

	return Volume;
}
