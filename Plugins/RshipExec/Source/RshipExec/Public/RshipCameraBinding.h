// Rship CineCamera Binding
// Comprehensive filmmaker-centric control of virtual cameras

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CineCameraComponent.h"
#include "CineCameraSettings.h"
#include "RshipCameraBinding.generated.h"

class URshipSubsystem;
class ACineCameraActor;

// ============================================================================
// CAMERA BINDING COMPONENT
// ============================================================================

/**
 * Comprehensive binding for CineCamera parameters to rship.
 * Exposes all filmmaker-centric controls needed to define a specific view in a 3D scene:
 * - Lens: Focal length, aperture, zoom range, anamorphic squeeze, bokeh
 * - Sensor: Size, aspect ratio, offsets
 * - Focus: Method, distance, smoothing, tracking
 * - Exposure: Method and settings
 * - Transform: Position, rotation, look-at
 * - Crop: Aspect ratio masking
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Camera Binding"))
class RSHIPEXEC_API URshipCameraBinding : public UActorComponent
{
	GENERATED_BODY()

public:
	URshipCameraBinding();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** The CineCamera component to bind (auto-found if not set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	UCineCameraComponent* CameraComponent;

	/** Publish rate in Hz (how often to publish camera state as emitters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera", meta = (ClampMin = "1", ClampMax = "120"))
	int32 PublishRateHz = 30;

	/** Only publish when values change (reduces network traffic) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bOnlyPublishOnChange = true;

	// ========================================================================
	// RS_ ACTIONS - Lens Controls
	// ========================================================================

	/** Set focal length in mm (affects field of view / zoom) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Lens")
	void RS_SetFocalLength(float FocalLengthMM);

	/** Set aperture as f-stop (e.g., 2.8 for f/2.8) - affects depth of field */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Lens")
	void RS_SetAperture(float FStop);

	/** Set focal length range for zoom lenses (min, max in mm) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Lens")
	void RS_SetFocalLengthRange(float MinMM, float MaxMM);

	/** Set aperture range (min and max f-stop) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Lens")
	void RS_SetApertureRange(float MinFStop, float MaxFStop);

	/** Set minimum focus distance in cm */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Lens")
	void RS_SetMinimumFocusDistance(float DistanceCM);

	/** Set anamorphic squeeze factor (1.0 = spherical, 2.0 = 2x anamorphic) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Lens")
	void RS_SetSqueezeFactor(float Squeeze);

	/** Set diaphragm blade count (affects bokeh shape, 4-16) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Lens")
	void RS_SetDiaphragmBladeCount(int32 BladeCount);

	/** Apply a lens preset by name */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Lens")
	void RS_SetLensPreset(const FString& PresetName);

	// ========================================================================
	// RS_ ACTIONS - Sensor/Filmback Controls
	// ========================================================================

	/** Set sensor dimensions in mm */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Sensor")
	void RS_SetSensorSize(float WidthMM, float HeightMM);

	/** Set sensor offset in mm (for lens shift effects) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Sensor")
	void RS_SetSensorOffset(float HorizontalMM, float VerticalMM);

	/** Apply a filmback preset by name (e.g., "Super 35mm", "Full Frame 35mm") */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Sensor")
	void RS_SetFilmbackPreset(const FString& PresetName);

	// ========================================================================
	// RS_ ACTIONS - Focus Controls
	// ========================================================================

	/** Set focus distance in cm (for manual focus mode) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Focus")
	void RS_SetFocusDistance(float DistanceCM);

	/** Set focus method (0=DoNotOverride, 1=Manual, 2=Tracking, 3=Disable) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Focus")
	void RS_SetFocusMethod(int32 Method);

	/** Set focus offset in cm (additional adjustment to focus distance) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Focus")
	void RS_SetFocusOffset(float OffsetCM);

	/** Enable/disable smooth focus transitions */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Focus")
	void RS_SetSmoothFocus(bool bEnabled, float InterpSpeed);

	// ========================================================================
	// RS_ ACTIONS - Crop/Masking Controls
	// ========================================================================

	/** Set crop aspect ratio (0 = no crop, otherwise target aspect like 2.39 for scope) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Crop")
	void RS_SetCropAspectRatio(float AspectRatio);

	/** Apply a crop preset by name */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Crop")
	void RS_SetCropPreset(const FString& PresetName);

	// ========================================================================
	// RS_ ACTIONS - Transform Controls
	// ========================================================================

	/** Set camera world location */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Transform")
	void RS_SetLocation(float X, float Y, float Z);

	/** Set camera world rotation (degrees) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Transform")
	void RS_SetRotation(float Pitch, float Yaw, float Roll);

	/** Move camera relative to current position */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Transform")
	void RS_AddLocation(float DeltaX, float DeltaY, float DeltaZ);

	/** Rotate camera relative to current rotation (degrees) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Transform")
	void RS_AddRotation(float DeltaPitch, float DeltaYaw, float DeltaRoll);

	/** Look at a specific world location */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Transform")
	void RS_LookAt(float TargetX, float TargetY, float TargetZ);

	// ========================================================================
	// RS_ ACTIONS - Exposure Controls
	// ========================================================================

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6)
	/** Set exposure method (0=DoNotOverride, 1=Enabled) - Only available in UE < 5.6 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Exposure")
	void RS_SetExposureMethod(int32 Method);
#endif

	/** Set custom near clipping plane in cm (0 to use default) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Exposure")
	void RS_SetNearClipPlane(float DistanceCM);

	// ========================================================================
	// RS_ ACTIONS - Utility
	// ========================================================================

	/** Reset camera to default settings */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Utility")
	void RS_ResetToDefaults();

	/** Copy settings from another CineCamera in the scene by name */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Utility")
	void RS_CopyFromCamera(const FString& CameraActorName);

	// ========================================================================
	// RS_ EMITTERS - State Publishing
	// ========================================================================

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_FloatEmitter, float, Value);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRS_VectorEmitter, float, X, float, Y, float, Z);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_IntEmitter, int32, Value);

	// Lens state
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnFocalLengthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnApertureChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnSqueezeFactorChanged;

	// Sensor state
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnSensorWidthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnSensorHeightChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnSensorAspectRatioChanged;

	// Focus state
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnFocusDistanceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_IntEmitter RS_OnFocusMethodChanged;

	// FOV (derived)
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnHorizontalFOVChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FloatEmitter RS_OnVerticalFOVChanged;

	// Transform
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_VectorEmitter RS_OnLocationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_VectorEmitter RS_OnRotationChanged;

	// ========================================================================
	// PUBLIC METHODS
	// ========================================================================

	/** Force publish all current values */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
	void ForcePublish();

	/** Get current camera state as JSON */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
	FString GetCameraStateJson() const;

private:
	UPROPERTY()
	URshipSubsystem* Subsystem;

	double LastPublishTime = 0.0;
	double PublishInterval = 0.033;

	// Cached values for change detection
	float LastFocalLength = 0.0f;
	float LastAperture = 0.0f;
	float LastSqueezeFactor = 0.0f;
	float LastSensorWidth = 0.0f;
	float LastSensorHeight = 0.0f;
	float LastSensorAspectRatio = 0.0f;
	float LastFocusDistance = 0.0f;
	int32 LastFocusMethod = 0;
	float LastHFOV = 0.0f;
	float LastVFOV = 0.0f;
	FVector LastLocation = FVector::ZeroVector;
	FRotator LastRotation = FRotator::ZeroRotator;

	void ReadAndPublishState();
	bool HasValueChanged(float OldValue, float NewValue, float Threshold = 0.001f) const;
};
