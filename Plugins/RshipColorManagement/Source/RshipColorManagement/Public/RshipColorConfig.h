// Copyright Lucid. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RshipColorConfig.generated.h"

/**
 * Exposure control mode for broadcast output.
 */
UENUM(BlueprintType)
enum class ERshipExposureMode : uint8
{
	/** Manual exposure - fixed EV value, most predictable for broadcast */
	Manual          UMETA(DisplayName = "Manual"),

	/** Auto exposure - eye adaptation enabled, matches viewport drift */
	Auto            UMETA(DisplayName = "Auto Exposure"),

	/** Histogram-based - uses luminance histogram with constrained range */
	Histogram       UMETA(DisplayName = "Histogram")
};

/**
 * Output color space (color primaries).
 */
UENUM(BlueprintType)
enum class ERshipColorSpace : uint8
{
	/** sRGB - standard for SDR displays and NDI */
	sRGB            UMETA(DisplayName = "sRGB"),

	/** Rec.709 - broadcast standard for SDR HD/UHD */
	Rec709          UMETA(DisplayName = "Rec.709"),

	/** Rec.2020 - wide color gamut for HDR broadcast */
	Rec2020         UMETA(DisplayName = "Rec.2020"),

	/** DCI-P3 - cinema standard */
	DCIP3           UMETA(DisplayName = "DCI-P3")
};

/**
 * Transfer function (gamma/EOTF).
 */
UENUM(BlueprintType)
enum class ERshipTransferFunction : uint8
{
	/** sRGB gamma curve (approximately 2.2) */
	sRGB            UMETA(DisplayName = "sRGB Gamma"),

	/** BT.1886 - precise broadcast gamma */
	BT1886          UMETA(DisplayName = "BT.1886"),

	/** PQ (ST.2084) - HDR perceptual quantizer */
	PQ              UMETA(DisplayName = "PQ (ST.2084)"),

	/** HLG - Hybrid Log-Gamma for HDR broadcast */
	HLG             UMETA(DisplayName = "HLG"),

	/** Linear - no gamma, for processing */
	Linear          UMETA(DisplayName = "Linear")
};

/**
 * Capture mode - what stage of the rendering pipeline to capture.
 */
UENUM(BlueprintType)
enum class ERshipCaptureMode : uint8
{
	/** Capture final LDR output (post-tonemapped, matches viewport exactly) */
	FinalColorLDR   UMETA(DisplayName = "Final Color LDR"),

	/** Capture HDR scene color (pre-tonemapped, for downstream processing) */
	SceneColorHDR   UMETA(DisplayName = "Scene Color HDR"),

	/** Capture raw scene color without post-process */
	RawSceneColor   UMETA(DisplayName = "Raw Scene Color")
};

/**
 * Tonemapping curve selection.
 */
UENUM(BlueprintType)
enum class ERshipTonemapCurve : uint8
{
	/** ACES filmic curve (Unreal default) */
	ACES            UMETA(DisplayName = "ACES Filmic"),

	/** Neutral/Linear (no tonemapping) */
	Neutral         UMETA(DisplayName = "Neutral"),

	/** Custom curve via parameters */
	Custom          UMETA(DisplayName = "Custom")
};

/**
 * Exposure settings for broadcast output.
 */
USTRUCT(BlueprintType)
struct RSHIPCOLORMANAGEMENT_API FRshipExposureSettings
{
	GENERATED_BODY()

	/** Exposure mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure")
	ERshipExposureMode Mode = ERshipExposureMode::Auto;

	/** Manual exposure value (EV100) - only used when Mode is Manual */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure",
		meta = (ClampMin = "-16.0", ClampMax = "16.0", EditCondition = "Mode == ERshipExposureMode::Manual"))
	float ManualExposureEV = 0.0f;

	/** Exposure compensation bias (EV) - applies to all modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure",
		meta = (ClampMin = "-16.0", ClampMax = "16.0"))
	float ExposureBias = 0.0f;

	/** Min brightness for auto exposure (nits) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure|Auto",
		meta = (ClampMin = "0.001", ClampMax = "100.0"))
	float AutoExposureMinBrightness = 0.03f;

	/** Max brightness for auto exposure (nits) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure|Auto",
		meta = (ClampMin = "0.001", ClampMax = "100.0"))
	float AutoExposureMaxBrightness = 2.0f;

	/** Auto exposure adaptation speed (seconds to adapt) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure|Auto",
		meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float AutoExposureSpeed = 0.5f;
};

/**
 * Tonemapping settings for broadcast output.
 */
USTRUCT(BlueprintType)
struct RSHIPCOLORMANAGEMENT_API FRshipTonemapSettings
{
	GENERATED_BODY()

	/** Enable tonemapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tonemap")
	bool bEnabled = true;

	/** Tonemap curve selection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tonemap")
	ERshipTonemapCurve Curve = ERshipTonemapCurve::ACES;

	/** Film slope (ACES) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tonemap|ACES",
		meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Slope = 0.88f;

	/** Film toe (ACES) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tonemap|ACES",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Toe = 0.55f;

	/** Film shoulder (ACES) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tonemap|ACES",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Shoulder = 0.26f;

	/** Film black clip (ACES) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tonemap|ACES",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlackClip = 0.0f;

	/** Film white clip (ACES) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tonemap|ACES",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WhiteClip = 0.04f;
};

/**
 * Complete color configuration for broadcast output.
 * This is the single source of truth for color settings across all outputs.
 */
USTRUCT(BlueprintType)
struct RSHIPCOLORMANAGEMENT_API FRshipColorConfig
{
	GENERATED_BODY()

	// ==== CAPTURE SETTINGS ====

	/** Capture mode - determines what stage of the pipeline to capture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	ERshipCaptureMode CaptureMode = ERshipCaptureMode::FinalColorLDR;

	// ==== COLOR SPACE ====

	/** Output color space (affects color primaries) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Space")
	ERshipColorSpace ColorSpace = ERshipColorSpace::Rec709;

	/** Transfer function (gamma/EOTF) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Space")
	ERshipTransferFunction TransferFunction = ERshipTransferFunction::sRGB;

	// ==== EXPOSURE ====

	/** Exposure settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure")
	FRshipExposureSettings Exposure;

	/** Apply exposure settings to viewport as well (ensures exact match) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure")
	bool bSyncExposureToViewport = true;

	// ==== TONEMAPPING ====

	/** Tonemap settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tonemap")
	FRshipTonemapSettings Tonemap;

	// ==== HDR OUTPUT ====

	/** Enable HDR output pipeline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDR")
	bool bEnableHDR = false;

	/** HDR max luminance (nits) - for PQ mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDR",
		meta = (ClampMin = "100.0", ClampMax = "10000.0", EditCondition = "bEnableHDR"))
	float HDRMaxLuminance = 1000.0f;

	/** HDR min luminance (nits) - for PQ mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDR",
		meta = (ClampMin = "0.0001", ClampMax = "1.0", EditCondition = "bEnableHDR"))
	float HDRMinLuminance = 0.005f;

	// ==== VALIDATION ====

	/** Check if this configuration is valid for broadcast */
	bool IsValidForBroadcast() const
	{
		// For broadcast: prefer manual exposure and standard color spaces
		return Exposure.Mode == ERshipExposureMode::Manual ||
			   (Exposure.Mode == ERshipExposureMode::Histogram &&
				Exposure.AutoExposureMinBrightness == Exposure.AutoExposureMaxBrightness);
	}

	/** Generate a human-readable description */
	FString GetDescription() const
	{
		FString ModeStr;
		switch (Exposure.Mode)
		{
		case ERshipExposureMode::Manual: ModeStr = TEXT("Manual"); break;
		case ERshipExposureMode::Auto: ModeStr = TEXT("Auto"); break;
		case ERshipExposureMode::Histogram: ModeStr = TEXT("Histogram"); break;
		}

		FString ColorStr;
		switch (ColorSpace)
		{
		case ERshipColorSpace::sRGB: ColorStr = TEXT("sRGB"); break;
		case ERshipColorSpace::Rec709: ColorStr = TEXT("Rec.709"); break;
		case ERshipColorSpace::Rec2020: ColorStr = TEXT("Rec.2020"); break;
		case ERshipColorSpace::DCIP3: ColorStr = TEXT("DCI-P3"); break;
		}

		return FString::Printf(TEXT("Exposure: %s (EV %.1f), Color: %s, HDR: %s"),
			*ModeStr, Exposure.ManualExposureEV + Exposure.ExposureBias,
			*ColorStr, bEnableHDR ? TEXT("ON") : TEXT("OFF"));
	}
};

/**
 * Delegate fired when color configuration changes (Blueprint-assignable).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColorConfigChanged, const FRshipColorConfig&, NewConfig);

/**
 * Native C++ delegate for color config changes.
 * Use this for C++ binding - doesn't require UFUNCTION on callback.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnColorConfigChangedNative, const FRshipColorConfig& /*NewConfig*/);
