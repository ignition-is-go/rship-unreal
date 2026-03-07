// Copyright Lucid. All Rights Reserved.

#include "RshipColorTarget.h"
#include "RshipActorRegistrationComponent.h"
#include "Engine/World.h"
#include "Logs.h"

#if RSHIP_HAS_COLOR_MANAGEMENT
#include "RshipColorManagementSubsystem.h"
#include "RshipColorConfig.h"
#endif

ARshipColorTarget::ARshipColorTarget()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Create target component for rship registration (UActorComponent, no attachment needed)
	TargetComponent = CreateDefaultSubobject<URshipActorRegistrationComponent>(TEXT("RshipTarget"));
}

void ARshipColorTarget::BeginPlay()
{
	Super::BeginPlay();

	// Set target name on component
	if (TargetComponent)
	{
		TargetComponent->targetName = TargetName;
	}

	// Bind to color management subsystem
	BindToColorSubsystem();
}

void ARshipColorTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromColorSubsystem();
	Super::EndPlay(EndPlayReason);
}

void ARshipColorTarget::BindToColorSubsystem()
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (UWorld* World = GetWorld())
	{
		ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>();
		if (ColorSubsystem.IsValid())
		{
			// Bind to native config change delegate (doesn't require UFUNCTION on callback)
			ConfigChangedHandle = ColorSubsystem->OnColorConfigChangedNative.AddUObject(
				this, &ARshipColorTarget::OnColorConfigChangedInternal);

			UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Bound to ColorManagementSubsystem"));
			OnColorSubsystemConnected();

			// Emit initial state
			OnColorConfigChangedInternal(ColorSubsystem->GetColorConfig());
		}
		else
		{
			UE_LOG(LogRshipExec, Warning, TEXT("RshipColorTarget: ColorManagementSubsystem not available"));
		}
	}
#else
	UE_LOG(LogRshipExec, Warning, TEXT("RshipColorTarget: RshipColorManagement plugin not available"));
#endif
}

void ARshipColorTarget::UnbindFromColorSubsystem()
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (ColorSubsystem.IsValid())
	{
		ColorSubsystem->OnColorConfigChangedNative.Remove(ConfigChangedHandle);
		ConfigChangedHandle.Reset();
		ColorSubsystem.Reset();

		UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Unbound from ColorManagementSubsystem"));
		OnColorSubsystemDisconnected();
	}
#endif
}

#if RSHIP_HAS_COLOR_MANAGEMENT
void ARshipColorTarget::OnColorConfigChangedInternal(const FRshipColorConfig& NewConfig)
{
	// Emit full config change
	FString ConfigJson = ConfigToJson(NewConfig);
	RS_OnColorConfigChanged.Broadcast(ConfigJson);

	// Emit exposure mode change
	FString ModeStr = GetExposureModeString(NewConfig.Exposure.Mode);
	float EV = NewConfig.Exposure.Mode == ERshipExposureMode::Manual
		? NewConfig.Exposure.ManualExposureEV
		: NewConfig.Exposure.ExposureBias;
	RS_OnExposureModeChanged.Broadcast(ModeStr, EV);

	// Emit color space change
	FString ColorSpaceStr = GetColorSpaceString(NewConfig.ColorSpace);
	RS_OnColorSpaceChanged.Broadcast(ColorSpaceStr);
}
#endif

// ============================================================================
// RSHIP ACTIONS
// ============================================================================

void ARshipColorTarget::RS_SetExposureMode(const FString& Mode, float EV)
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipColorTarget: ColorSubsystem not available"));
		return;
	}

	ERshipExposureMode ExposureMode;
	if (!ParseExposureMode(Mode, ExposureMode))
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipColorTarget: Invalid exposure mode: %s"), *Mode);
		return;
	}

	FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
	Config.Exposure.Mode = ExposureMode;

	if (ExposureMode == ERshipExposureMode::Manual)
	{
		Config.Exposure.ManualExposureEV = FMath::Clamp(EV, -16.0f, 16.0f);
	}

	ColorSubsystem->SetColorConfig(Config);

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Set exposure mode to %s (EV: %.2f)"), *Mode, EV);
#endif
}

void ARshipColorTarget::RS_SetManualEV(float EV)
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return;
	}

	FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
	Config.Exposure.ManualExposureEV = FMath::Clamp(EV, -16.0f, 16.0f);
	ColorSubsystem->SetColorConfig(Config);

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Set manual EV to %.2f"), EV);
#endif
}

void ARshipColorTarget::RS_SetExposureBias(float Bias)
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return;
	}

	FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
	Config.Exposure.ExposureBias = FMath::Clamp(Bias, -16.0f, 16.0f);
	ColorSubsystem->SetColorConfig(Config);

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Set exposure bias to %.2f"), Bias);
#endif
}

void ARshipColorTarget::RS_SetColorSpace(const FString& ColorSpaceStr)
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return;
	}

	ERshipColorSpace ColorSpace;
	if (!ParseColorSpace(ColorSpaceStr, ColorSpace))
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipColorTarget: Invalid color space: %s"), *ColorSpaceStr);
		return;
	}

	FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
	Config.ColorSpace = ColorSpace;
	ColorSubsystem->SetColorConfig(Config);

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Set color space to %s"), *ColorSpaceStr);
#endif
}

void ARshipColorTarget::RS_SetHDREnabled(bool bEnabled)
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return;
	}

	FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
	Config.bEnableHDR = bEnabled;
	ColorSubsystem->SetColorConfig(Config);

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: HDR %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
#endif
}

void ARshipColorTarget::RS_SetHDRLuminance(float MaxNits, float MinNits)
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return;
	}

	FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
	Config.HDRMaxLuminance = FMath::Clamp(MaxNits, 100.0f, 10000.0f);
	Config.HDRMinLuminance = FMath::Clamp(MinNits, 0.0001f, 1.0f);
	ColorSubsystem->SetColorConfig(Config);

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Set HDR luminance range %.2f - %.2f nits"), MinNits, MaxNits);
#endif
}

void ARshipColorTarget::RS_ApplyToViewport()
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return;
	}

	ColorSubsystem->ApplyToViewport();

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Applied color config to viewport"));
#endif
}

FString ARshipColorTarget::RS_GetConfig()
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return TEXT("{}");
	}

	return ConfigToJson(ColorSubsystem->GetColorConfig());
#else
	return TEXT("{\"error\": \"ColorManagement not available\"}");
#endif
}

void ARshipColorTarget::RS_SetCaptureMode(const FString& CaptureMode)
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return;
	}

	ERshipCaptureMode Mode;
	if (!ParseCaptureMode(CaptureMode, Mode))
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipColorTarget: Invalid capture mode: %s"), *CaptureMode);
		return;
	}

	FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
	Config.CaptureMode = Mode;
	ColorSubsystem->SetColorConfig(Config);

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Set capture mode to %s"), *CaptureMode);
#endif
}

void ARshipColorTarget::RS_SetViewportSync(bool bSync)
{
#if RSHIP_HAS_COLOR_MANAGEMENT
	if (!ColorSubsystem.IsValid())
	{
		return;
	}

	FRshipColorConfig Config = ColorSubsystem->GetColorConfig();
	Config.bSyncExposureToViewport = bSync;
	ColorSubsystem->SetColorConfig(Config);

	UE_LOG(LogRshipExec, Log, TEXT("RshipColorTarget: Viewport sync %s"), bSync ? TEXT("enabled") : TEXT("disabled"));
#endif
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

#if RSHIP_HAS_COLOR_MANAGEMENT

FString ARshipColorTarget::ConfigToJson(const FRshipColorConfig& Config) const
{
	// Build JSON manually for precise control
	FString Json = TEXT("{");

	// Capture mode
	Json += FString::Printf(TEXT("\"captureMode\":\"%s\","),
		Config.CaptureMode == ERshipCaptureMode::FinalColorLDR ? TEXT("FinalColorLDR") :
		Config.CaptureMode == ERshipCaptureMode::SceneColorHDR ? TEXT("SceneColorHDR") :
		TEXT("RawSceneColor"));

	// Color space
	Json += FString::Printf(TEXT("\"colorSpace\":\"%s\","), *GetColorSpaceString(Config.ColorSpace));

	// Exposure
	Json += TEXT("\"exposure\":{");
	Json += FString::Printf(TEXT("\"mode\":\"%s\","), *GetExposureModeString(Config.Exposure.Mode));
	Json += FString::Printf(TEXT("\"manualEV\":%.3f,"), Config.Exposure.ManualExposureEV);
	Json += FString::Printf(TEXT("\"bias\":%.3f,"), Config.Exposure.ExposureBias);
	Json += FString::Printf(TEXT("\"autoMinBrightness\":%.3f,"), Config.Exposure.AutoExposureMinBrightness);
	Json += FString::Printf(TEXT("\"autoMaxBrightness\":%.3f,"), Config.Exposure.AutoExposureMaxBrightness);
	Json += FString::Printf(TEXT("\"autoSpeed\":%.3f"), Config.Exposure.AutoExposureSpeed);
	Json += TEXT("},");

	// Tonemap
	Json += TEXT("\"tonemap\":{");
	Json += FString::Printf(TEXT("\"enabled\":%s,"), Config.Tonemap.bEnabled ? TEXT("true") : TEXT("false"));
	Json += FString::Printf(TEXT("\"slope\":%.3f,"), Config.Tonemap.Slope);
	Json += FString::Printf(TEXT("\"toe\":%.3f,"), Config.Tonemap.Toe);
	Json += FString::Printf(TEXT("\"shoulder\":%.3f,"), Config.Tonemap.Shoulder);
	Json += FString::Printf(TEXT("\"blackClip\":%.3f,"), Config.Tonemap.BlackClip);
	Json += FString::Printf(TEXT("\"whiteClip\":%.3f"), Config.Tonemap.WhiteClip);
	Json += TEXT("},");

	// HDR
	Json += FString::Printf(TEXT("\"hdrEnabled\":%s,"), Config.bEnableHDR ? TEXT("true") : TEXT("false"));
	Json += FString::Printf(TEXT("\"hdrMaxLuminance\":%.1f,"), Config.HDRMaxLuminance);
	Json += FString::Printf(TEXT("\"hdrMinLuminance\":%.4f,"), Config.HDRMinLuminance);

	// Viewport sync
	Json += FString::Printf(TEXT("\"syncToViewport\":%s"), Config.bSyncExposureToViewport ? TEXT("true") : TEXT("false"));

	Json += TEXT("}");

	return Json;
}

FString ARshipColorTarget::GetExposureModeString(ERshipExposureMode Mode)
{
	switch (Mode)
	{
	case ERshipExposureMode::Manual: return TEXT("Manual");
	case ERshipExposureMode::Auto: return TEXT("Auto");
	case ERshipExposureMode::Histogram: return TEXT("Histogram");
	default: return TEXT("Manual");
	}
}

FString ARshipColorTarget::GetColorSpaceString(ERshipColorSpace ColorSpace)
{
	switch (ColorSpace)
	{
	case ERshipColorSpace::sRGB: return TEXT("sRGB");
	case ERshipColorSpace::Rec709: return TEXT("Rec709");
	case ERshipColorSpace::Rec2020: return TEXT("Rec2020");
	case ERshipColorSpace::DCIP3: return TEXT("DCIP3");
	default: return TEXT("Rec709");
	}
}

bool ARshipColorTarget::ParseExposureMode(const FString& ModeStr, ERshipExposureMode& OutMode)
{
	if (ModeStr.Equals(TEXT("Manual"), ESearchCase::IgnoreCase))
	{
		OutMode = ERshipExposureMode::Manual;
		return true;
	}
	else if (ModeStr.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
	{
		OutMode = ERshipExposureMode::Auto;
		return true;
	}
	else if (ModeStr.Equals(TEXT("Histogram"), ESearchCase::IgnoreCase))
	{
		OutMode = ERshipExposureMode::Histogram;
		return true;
	}
	return false;
}

bool ARshipColorTarget::ParseColorSpace(const FString& SpaceStr, ERshipColorSpace& OutColorSpace)
{
	if (SpaceStr.Equals(TEXT("sRGB"), ESearchCase::IgnoreCase))
	{
		OutColorSpace = ERshipColorSpace::sRGB;
		return true;
	}
	else if (SpaceStr.Equals(TEXT("Rec709"), ESearchCase::IgnoreCase))
	{
		OutColorSpace = ERshipColorSpace::Rec709;
		return true;
	}
	else if (SpaceStr.Equals(TEXT("Rec2020"), ESearchCase::IgnoreCase))
	{
		OutColorSpace = ERshipColorSpace::Rec2020;
		return true;
	}
	else if (SpaceStr.Equals(TEXT("DCIP3"), ESearchCase::IgnoreCase))
	{
		OutColorSpace = ERshipColorSpace::DCIP3;
		return true;
	}
	return false;
}

bool ARshipColorTarget::ParseCaptureMode(const FString& ModeStr, ERshipCaptureMode& OutMode)
{
	if (ModeStr.Equals(TEXT("FinalColorLDR"), ESearchCase::IgnoreCase))
	{
		OutMode = ERshipCaptureMode::FinalColorLDR;
		return true;
	}
	else if (ModeStr.Equals(TEXT("SceneColorHDR"), ESearchCase::IgnoreCase))
	{
		OutMode = ERshipCaptureMode::SceneColorHDR;
		return true;
	}
	else if (ModeStr.Equals(TEXT("RawSceneColor"), ESearchCase::IgnoreCase))
	{
		OutMode = ERshipCaptureMode::RawSceneColor;
		return true;
	}
	return false;
}

#endif // RSHIP_HAS_COLOR_MANAGEMENT

