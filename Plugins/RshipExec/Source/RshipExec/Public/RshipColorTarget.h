// Copyright Lucid. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

// IMPORTANT: Include RshipColorConfig.h BEFORE generated.h
// because it has its own generated.h and UHT requires our .generated.h to be last
#if RSHIP_HAS_COLOR_MANAGEMENT
#include "RshipColorConfig.h"
#endif

#include "RshipColorTarget.generated.h"

class URshipActorRegistrationComponent;
class URshipColorManagementSubsystem;

/**
 * Delegate signatures for color management emitters.
 * These use the RS_ prefix to be auto-registered by RshipTargetComponent.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRshipColorConfigChangedDelegate, const FString&, ConfigJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRshipExposureModeChangedDelegate, const FString&, Mode, float, EV);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRshipColorSpaceChangedDelegate, const FString&, ColorSpace);

/**
 * Actor that exposes the color management system as a rship target.
 * Provides actions for controlling exposure, color space, and HDR settings.
 * Emits state changes to the rship server.
 *
 * Actions (RS_ functions):
 * - RS_SetExposureMode: Set exposure mode (Manual/Auto/Histogram) and optional EV
 * - RS_SetManualEV: Set manual exposure value
 * - RS_SetExposureBias: Set exposure compensation bias
 * - RS_SetColorSpace: Set output color space
 * - RS_SetHDREnabled: Enable/disable HDR pipeline
 * - RS_ApplyToViewport: Apply current settings to viewport
 * - RS_GetConfig: Get current configuration as JSON
 *
 * Emitters (RS_ delegates):
 * - RS_OnColorConfigChanged: Fires when any config changes (full JSON)
 * - RS_OnExposureModeChanged: Fires when exposure mode changes
 * - RS_OnColorSpaceChanged: Fires when color space changes
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Rship Color Target"))
class RSHIPEXEC_API ARshipColorTarget : public AActor
{
	GENERATED_BODY()

public:
	ARshipColorTarget();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========================================================================
	// COMPONENTS
	// ========================================================================

	/** Rship target component for auto-registration */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Components")
	URshipActorRegistrationComponent* TargetComponent;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** Target name for rship registration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Color")
	FString TargetName = TEXT("ColorManagement");

	// ========================================================================
	// RSHIP ACTIONS (RS_ prefix for auto-registration)
	// ========================================================================

	/**
	 * Set the exposure mode.
	 * @param Mode - "Manual", "Auto", or "Histogram"
	 * @param EV - Manual exposure value (only used when Mode is "Manual")
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_SetExposureMode(const FString& Mode, float EV = 0.0f);

	/**
	 * Set the manual exposure value (EV100).
	 * Only effective when exposure mode is Manual.
	 * @param EV - Exposure value in EV100 units (-16 to +16)
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_SetManualEV(float EV);

	/**
	 * Set the exposure bias.
	 * Applies to all exposure modes.
	 * @param Bias - Exposure compensation in EV (-16 to +16)
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_SetExposureBias(float Bias);

	/**
	 * Set the output color space.
	 * @param ColorSpace - "sRGB", "Rec709", "Rec2020", or "DCIP3"
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_SetColorSpace(const FString& ColorSpace);

	/**
	 * Enable or disable HDR output pipeline.
	 * @param bEnabled - True to enable HDR
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_SetHDREnabled(bool bEnabled);

	/**
	 * Set HDR luminance range.
	 * @param MaxNits - Maximum luminance in nits (100-10000)
	 * @param MinNits - Minimum luminance in nits (0.0001-1.0)
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_SetHDRLuminance(float MaxNits, float MinNits);

	/**
	 * Apply current color settings to the viewport.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_ApplyToViewport();

	/**
	 * Get current configuration as JSON string.
	 * @return JSON string representation of current config
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	FString RS_GetConfig();

	/**
	 * Set the capture mode for scene capture.
	 * @param CaptureMode - "FinalColorLDR", "SceneColorHDR", or "RawSceneColor"
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_SetCaptureMode(const FString& CaptureMode);

	/**
	 * Set whether to sync exposure settings to viewport.
	 * @param bSync - True to sync viewport exposure
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RS_SetViewportSync(bool bSync);

	// ========================================================================
	// RSHIP EMITTERS (RS_ prefix for auto-registration)
	// ========================================================================

	/** Fired when color configuration changes (full JSON) */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Color")
	FRshipColorConfigChangedDelegate RS_OnColorConfigChanged;

	/** Fired when exposure mode changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Color")
	FRshipExposureModeChangedDelegate RS_OnExposureModeChanged;

	/** Fired when color space changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Color")
	FRshipColorSpaceChangedDelegate RS_OnColorSpaceChanged;

	// ========================================================================
	// BLUEPRINT EVENTS
	// ========================================================================

	/** Called when connected to color management subsystem */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rship|Color")
	void OnColorSubsystemConnected();

	/** Called when disconnected from color management subsystem */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rship|Color")
	void OnColorSubsystemDisconnected();

protected:
#if RSHIP_HAS_COLOR_MANAGEMENT
	/** Handle color config changes from subsystem (uses native delegate, no UFUNCTION needed) */
	void OnColorConfigChangedInternal(const FRshipColorConfig& NewConfig);
#endif

private:
#if RSHIP_HAS_COLOR_MANAGEMENT
	/** Cached reference to color management subsystem (weak ptr, no UPROPERTY due to preprocessor block) */
	TWeakObjectPtr<URshipColorManagementSubsystem> ColorSubsystem;

	/** Delegate handle for native config change notifications */
	FDelegateHandle ConfigChangedHandle;

	/** Convert config to JSON string */
	FString ConfigToJson(const FRshipColorConfig& Config) const;

	/** Get exposure mode as string */
	static FString GetExposureModeString(ERshipExposureMode Mode);

	/** Get color space as string */
	static FString GetColorSpaceString(ERshipColorSpace ColorSpace);

	/** Parse exposure mode from string */
	static bool ParseExposureMode(const FString& ModeStr, ERshipExposureMode& OutMode);

	/** Parse color space from string */
	static bool ParseColorSpace(const FString& SpaceStr, ERshipColorSpace& OutColorSpace);

	/** Parse capture mode from string */
	static bool ParseCaptureMode(const FString& ModeStr, ERshipCaptureMode& OutMode);
#endif

	/** Bind to color management subsystem */
	void BindToColorSubsystem();

	/** Unbind from color management subsystem */
	void UnbindFromColorSubsystem();
};

