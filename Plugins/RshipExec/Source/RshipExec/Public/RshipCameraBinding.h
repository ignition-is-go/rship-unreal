// Rship CineCamera Binding
// Bidirectional binding between CineCamera parameters and rship emitters/actions

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CineCameraComponent.h"
#include "RshipCameraBinding.generated.h"

class URshipSubsystem;
class ACineCameraActor;

// ============================================================================
// BINDING CONFIGURATION
// ============================================================================

/** Mode for camera binding - receive from rship, publish to rship, or both */
UENUM(BlueprintType)
enum class ERshipCameraBindingMode : uint8
{
	Receive     UMETA(DisplayName = "Receive"),     // rship pulses drive camera
	Publish     UMETA(DisplayName = "Publish"),     // Camera state published as emitters
	Bidirectional UMETA(DisplayName = "Bidirectional") // Both directions
};

/** Configuration for a single camera parameter binding */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipCameraParameterBinding
{
	GENERATED_BODY()

	/** Name of the camera parameter (e.g., "FocalLength", "Aperture") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	FName ParameterName;

	/** Pulse field name to read from / publish to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	FString PulseField;

	/** Whether to receive this parameter from rship */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bReceive = true;

	/** Whether to publish this parameter to rship */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bPublish = true;

	/** Scale factor when receiving (pulse value * scale = camera value) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	float ReceiveScale = 1.0f;

	/** Scale factor when publishing (camera value * scale = pulse value) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	float PublishScale = 1.0f;

	/** Smoothing factor for receiving (0 = instant, 0.99 = very smooth) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float Smoothing = 0.0f;

	/** Whether this binding is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bEnabled = true;

	// Runtime state
	float TargetValue = 0.0f;
	float SmoothedValue = 0.0f;
	float LastPublishedValue = 0.0f;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCameraPulseReceived, const FString&, EmitterId, const FString&, ParameterName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCameraParameterChanged, const FString&, ParameterName, float, Value);

// ============================================================================
// CAMERA BINDING COMPONENT
// ============================================================================

/**
 * Component that binds CineCamera parameters to rship.
 * Supports bidirectional data flow:
 * - Receive: rship pulses drive camera parameters (focal length, aperture, focus, etc.)
 * - Publish: Camera state is published as emitters for other systems to react to
 *
 * Attach to a CineCameraActor or any actor with a CineCameraComponent.
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

	/** Binding mode - receive from rship, publish to rship, or both */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	ERshipCameraBindingMode BindingMode = ERshipCameraBindingMode::Bidirectional;

	/** The emitter ID to listen for when receiving (e.g., "targetId:emitterId") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera|Receive")
	FString ReceiveEmitterId;

	/** Target ID for publishing camera state as emitters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera|Publish")
	FString PublishTargetId;

	/** The CineCamera component to bind (auto-found if not set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	UCineCameraComponent* CameraComponent;

	/** Parameter bindings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	TArray<FRshipCameraParameterBinding> ParameterBindings;

	/** Use default bindings for standard CineCamera parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bUseDefaultBindings = true;

	/** Publish rate in Hz (how often to publish camera state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera|Publish", meta = (ClampMin = "1", ClampMax = "120"))
	int32 PublishRateHz = 30;

	/** Only publish when values change (reduces network traffic) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera|Publish")
	bool bOnlyPublishOnChange = true;

	/** Threshold for considering a value "changed" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera|Publish", meta = (EditCondition = "bOnlyPublishOnChange"))
	float ChangeThreshold = 0.001f;

	// ========================================================================
	// RUNTIME STATE
	// ========================================================================

	/** Current focal length (mm) */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera|State")
	float CurrentFocalLength = 35.0f;

	/** Current aperture (f-stop) */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera|State")
	float CurrentAperture = 2.8f;

	/** Current focus distance (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera|State")
	float CurrentFocusDistance = 100000.0f;

	/** Current sensor width (mm) */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera|State")
	float CurrentSensorWidth = 36.0f;

	/** Current sensor height (mm) */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera|State")
	float CurrentSensorHeight = 24.0f;

	/** Whether the component is actively receiving pulses */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera|State")
	bool bIsReceivingPulses = false;

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when a pulse is received for this camera */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera")
	FOnCameraPulseReceived OnPulseReceived;

	/** Fired when a camera parameter changes (from any source) */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera")
	FOnCameraParameterChanged OnParameterChanged;

	// ========================================================================
	// RS_ EMITTERS (auto-registered by RshipTargetComponent)
	// ========================================================================

	/** Emitter: Current focal length in mm */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_FocalLengthEmitter, float, FocalLength);
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FocalLengthEmitter RS_OnFocalLengthChanged;

	/** Emitter: Current aperture (f-stop) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_ApertureEmitter, float, Aperture);
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_ApertureEmitter RS_OnApertureChanged;

	/** Emitter: Current focus distance in cm */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_FocusDistanceEmitter, float, FocusDistance);
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_FocusDistanceEmitter RS_OnFocusDistanceChanged;

	/** Emitter: Current sensor width in mm */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_SensorWidthEmitter, float, SensorWidth);
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_SensorWidthEmitter RS_OnSensorWidthChanged;

	/** Emitter: Current sensor height in mm */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_SensorHeightEmitter, float, SensorHeight);
	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRS_SensorHeightEmitter RS_OnSensorHeightChanged;

	// ========================================================================
	// RS_ ACTIONS (auto-registered by RshipTargetComponent)
	// ========================================================================

	/** Action: Set focal length in mm */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Actions")
	void RS_SetFocalLength(float FocalLength);

	/** Action: Set aperture (f-stop) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Actions")
	void RS_SetAperture(float Aperture);

	/** Action: Set focus distance in cm */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Actions")
	void RS_SetFocusDistance(float FocusDistance);

	/** Action: Set sensor size in mm */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Actions")
	void RS_SetSensorSize(float Width, float Height);

	/** Action: Enable/disable manual focus */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Actions")
	void RS_SetManualFocusEnabled(bool bEnabled);

	/** Action: Set focus method (0=DoNotOverride, 1=Manual, 2=Tracking) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera|Actions")
	void RS_SetFocusMethod(int32 Method);

	// ========================================================================
	// PUBLIC METHODS
	// ========================================================================

	/** Manually set a camera parameter (bypasses pulse system) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
	void SetCameraParameter(FName ParameterName, float Value);

	/** Get a camera parameter value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Camera")
	float GetCameraParameter(FName ParameterName) const;

	/** Force publish current camera state */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
	void ForcePublish();

	/** Enable/disable all bindings */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
	void SetBindingsEnabled(bool bEnabled);

	/** Reset camera to default values */
	UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
	void ResetToDefaults();

private:
	UPROPERTY()
	URshipSubsystem* Subsystem;

	FDelegateHandle PulseReceivedHandle;
	double LastPulseTime = 0.0;
	double LastPublishTime = 0.0;
	double PublishInterval = 0.033; // 1/30 sec

	void SetupDefaultBindings();
	void OnPulseReceivedInternal(const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data);
	void ApplyReceivedBindings(TSharedPtr<FJsonObject> Data);
	void UpdateSmoothedValues(float DeltaTime);
	void ApplyToCameraComponent();
	void ReadFromCameraComponent();
	void PublishCameraState();
	float GetFloatFromJson(TSharedPtr<FJsonObject> Data, const FString& FieldPath);
	bool HasValueChanged(float OldValue, float NewValue) const;
};
