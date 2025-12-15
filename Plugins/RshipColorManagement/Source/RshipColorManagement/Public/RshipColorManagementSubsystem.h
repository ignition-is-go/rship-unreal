// Copyright Lucid. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RshipColorConfig.h"
#include "RshipColorManagementSubsystem.generated.h"

class APostProcessVolume;
class USceneCaptureComponent2D;
class ACineCameraActor;

/**
 * World subsystem that manages broadcast-grade color settings.
 * Provides a single source of truth for color configuration across
 * viewport, NDI, SMPTE 2110, and any other outputs.
 */
UCLASS()
class RSHIPCOLORMANAGEMENT_API URshipColorManagementSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ========================================================================
	// Configuration
	// ========================================================================

	/**
	 * Set the active color configuration.
	 * This will update the viewport and all registered outputs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void SetColorConfig(const FRshipColorConfig& NewConfig);

	/** Get the active color configuration */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	FRshipColorConfig GetColorConfig() const { return ActiveConfig; }

	// ========================================================================
	// Viewport Control
	// ========================================================================

	/**
	 * Apply the current color config to the viewport.
	 * Creates/updates a high-priority post-process volume.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void ApplyToViewport();

	/**
	 * Apply color config to a specific CineCamera actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void ApplyToCamera(ACineCameraActor* Camera);

	/**
	 * Remove color overrides from viewport (restore default behavior).
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	void RemoveViewportOverrides();

	// ========================================================================
	// Scene Capture Configuration
	// ========================================================================

	/**
	 * Configure a scene capture component to match current color settings.
	 * Call this when initializing NDI/2110 capture components.
	 */
	void ConfigureSceneCapture(USceneCaptureComponent2D* Capture);

	/**
	 * Get post-process settings struct configured for current color config.
	 */
	FPostProcessSettings GetPostProcessSettings() const;

	/**
	 * Check if eye adaptation should be enabled based on current config.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	bool ShouldEnableEyeAdaptation() const;

	// ========================================================================
	// HDR Utilities
	// ========================================================================

	/** Check if HDR pipeline is active */
	UFUNCTION(BlueprintCallable, Category = "Rship|Color")
	bool IsHDRActive() const { return ActiveConfig.bEnableHDR; }

	/** Get the capture source enum for current config */
	ESceneCaptureSource GetCaptureSource() const;

	// ========================================================================
	// Events
	// ========================================================================

	/** Fired when color configuration changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Color")
	FOnColorConfigChanged OnColorConfigChanged;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	/** Active color configuration */
	FRshipColorConfig ActiveConfig;

	/** Post-process volume used to override viewport settings */
	UPROPERTY()
	TWeakObjectPtr<APostProcessVolume> ColorControlVolume;

	/** Apply config to a post-process settings struct */
	void ApplyConfigToPostProcessSettings(FPostProcessSettings& Settings) const;

	/** Update viewport console variables for exposure */
	void UpdateViewportCVars();

	/** Create the control post-process volume */
	APostProcessVolume* CreateColorControlVolume(float Priority);
};
