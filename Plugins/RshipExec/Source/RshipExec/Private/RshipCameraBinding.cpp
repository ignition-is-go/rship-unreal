// Rship CineCamera Binding Implementation

#include "RshipCameraBinding.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Dom/JsonObject.h"

// ============================================================================
// CAMERA BINDING COMPONENT
// ============================================================================

URshipCameraBinding::URshipCameraBinding()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.016f; // ~60Hz for smooth camera updates
}

void URshipCameraBinding::BeginPlay()
{
	Super::BeginPlay();

	// Get subsystem
	if (GEngine)
	{
		Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	}

	// Auto-find CineCamera component if not set
	if (!CameraComponent)
	{
		// First try to get from CineCameraActor
		if (ACineCameraActor* CineActor = Cast<ACineCameraActor>(GetOwner()))
		{
			CameraComponent = CineActor->GetCineCameraComponent();
		}
		else
		{
			// Fallback to finding any CineCameraComponent
			CameraComponent = GetOwner()->FindComponentByClass<UCineCameraComponent>();
		}
	}

	if (!CameraComponent)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipCameraBinding: No CineCameraComponent found on %s"),
			*GetOwner()->GetName());
		return;
	}

	// Setup default bindings if enabled
	if (bUseDefaultBindings && ParameterBindings.Num() == 0)
	{
		SetupDefaultBindings();
	}

	// Calculate publish interval
	PublishInterval = 1.0 / FMath::Max(1, PublishRateHz);

	// Subscribe to pulse events for receiving
	if (Subsystem && (BindingMode == ERshipCameraBindingMode::Receive || BindingMode == ERshipCameraBindingMode::Bidirectional))
	{
		URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
		if (Receiver && !ReceiveEmitterId.IsEmpty())
		{
			PulseReceivedHandle = Receiver->OnEmitterPulseReceived.AddLambda(
				[this](const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data)
				{
					if (InEmitterId == ReceiveEmitterId)
					{
						OnPulseReceivedInternal(InEmitterId, Intensity, Color, Data);
					}
				});
		}
	}

	// Read initial state from camera
	ReadFromCameraComponent();

	UE_LOG(LogRshipExec, Log, TEXT("RshipCameraBinding: Initialized on %s (Mode=%d, Bindings=%d)"),
		*GetOwner()->GetName(), (int32)BindingMode, ParameterBindings.Num());
}

void URshipCameraBinding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unsubscribe from pulse events
	if (Subsystem && PulseReceivedHandle.IsValid())
	{
		URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
		if (Receiver)
		{
			Receiver->OnEmitterPulseReceived.Remove(PulseReceivedHandle);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void URshipCameraBinding::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CameraComponent) return;

	// Update smoothed values for receiving mode
	if (BindingMode == ERshipCameraBindingMode::Receive || BindingMode == ERshipCameraBindingMode::Bidirectional)
	{
		UpdateSmoothedValues(DeltaTime);
		ApplyToCameraComponent();
	}

	// Publish camera state for publishing mode
	if (BindingMode == ERshipCameraBindingMode::Publish || BindingMode == ERshipCameraBindingMode::Bidirectional)
	{
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - LastPublishTime >= PublishInterval)
		{
			ReadFromCameraComponent();
			PublishCameraState();
			LastPublishTime = CurrentTime;
		}
	}
}

void URshipCameraBinding::SetupDefaultBindings()
{
	// Create default bindings for common CineCamera parameters
	ParameterBindings.Empty();

	// Focal Length
	{
		FRshipCameraParameterBinding Binding;
		Binding.ParameterName = FName("FocalLength");
		Binding.PulseField = TEXT("focalLength");
		Binding.bReceive = true;
		Binding.bPublish = true;
		Binding.ReceiveScale = 1.0f;
		Binding.PublishScale = 1.0f;
		Binding.Smoothing = 0.5f;  // Smooth focal length changes
		ParameterBindings.Add(Binding);
	}

	// Aperture (F-Stop)
	{
		FRshipCameraParameterBinding Binding;
		Binding.ParameterName = FName("Aperture");
		Binding.PulseField = TEXT("aperture");
		Binding.bReceive = true;
		Binding.bPublish = true;
		Binding.Smoothing = 0.3f;
		ParameterBindings.Add(Binding);
	}

	// Focus Distance
	{
		FRshipCameraParameterBinding Binding;
		Binding.ParameterName = FName("FocusDistance");
		Binding.PulseField = TEXT("focusDistance");
		Binding.bReceive = true;
		Binding.bPublish = true;
		Binding.Smoothing = 0.7f;  // Smooth focus pulls
		ParameterBindings.Add(Binding);
	}

	// Sensor Width
	{
		FRshipCameraParameterBinding Binding;
		Binding.ParameterName = FName("SensorWidth");
		Binding.PulseField = TEXT("sensorWidth");
		Binding.bReceive = true;
		Binding.bPublish = true;
		Binding.Smoothing = 0.0f;  // Sensor size changes are usually instant
		ParameterBindings.Add(Binding);
	}

	// Sensor Height
	{
		FRshipCameraParameterBinding Binding;
		Binding.ParameterName = FName("SensorHeight");
		Binding.PulseField = TEXT("sensorHeight");
		Binding.bReceive = true;
		Binding.bPublish = true;
		Binding.Smoothing = 0.0f;
		ParameterBindings.Add(Binding);
	}

	UE_LOG(LogRshipExec, Log, TEXT("RshipCameraBinding: Created %d default bindings"), ParameterBindings.Num());
}

void URshipCameraBinding::OnPulseReceivedInternal(const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data)
{
	if (!CameraComponent) return;

	LastPulseTime = FPlatformTime::Seconds();
	bIsReceivingPulses = true;

	// Apply received bindings
	ApplyReceivedBindings(Data);

	// Fire event
	OnPulseReceived.Broadcast(InEmitterId, TEXT(""));
}

void URshipCameraBinding::ApplyReceivedBindings(TSharedPtr<FJsonObject> Data)
{
	if (!Data.IsValid()) return;

	for (FRshipCameraParameterBinding& Binding : ParameterBindings)
	{
		if (!Binding.bEnabled || !Binding.bReceive) continue;

		float RawValue = GetFloatFromJson(Data, Binding.PulseField);
		float ScaledValue = RawValue * Binding.ReceiveScale;
		Binding.TargetValue = ScaledValue;

		// If no smoothing, apply immediately
		if (Binding.Smoothing <= 0.0f)
		{
			Binding.SmoothedValue = ScaledValue;
		}
	}
}

void URshipCameraBinding::UpdateSmoothedValues(float DeltaTime)
{
	for (FRshipCameraParameterBinding& Binding : ParameterBindings)
	{
		if (!Binding.bEnabled || !Binding.bReceive) continue;

		if (Binding.Smoothing > 0.0f)
		{
			// Apply exponential smoothing
			float Alpha = 1.0f - FMath::Pow(Binding.Smoothing, DeltaTime * 60.0f);
			Binding.SmoothedValue = FMath::Lerp(Binding.SmoothedValue, Binding.TargetValue, Alpha);
		}
	}
}

void URshipCameraBinding::ApplyToCameraComponent()
{
	if (!CameraComponent) return;

	for (FRshipCameraParameterBinding& Binding : ParameterBindings)
	{
		if (!Binding.bEnabled || !Binding.bReceive) continue;

		float Value = Binding.SmoothedValue;

		if (Binding.ParameterName == FName("FocalLength"))
		{
			CameraComponent->CurrentFocalLength = Value;
		}
		else if (Binding.ParameterName == FName("Aperture"))
		{
			CameraComponent->CurrentAperture = Value;
		}
		else if (Binding.ParameterName == FName("FocusDistance"))
		{
			CameraComponent->FocusSettings.ManualFocusDistance = Value;
		}
		else if (Binding.ParameterName == FName("SensorWidth"))
		{
			CameraComponent->Filmback.SensorWidth = Value;
		}
		else if (Binding.ParameterName == FName("SensorHeight"))
		{
			CameraComponent->Filmback.SensorHeight = Value;
		}
	}
}

void URshipCameraBinding::ReadFromCameraComponent()
{
	if (!CameraComponent) return;

	CurrentFocalLength = CameraComponent->CurrentFocalLength;
	CurrentAperture = CameraComponent->CurrentAperture;
	CurrentFocusDistance = CameraComponent->FocusSettings.ManualFocusDistance;
	CurrentSensorWidth = CameraComponent->Filmback.SensorWidth;
	CurrentSensorHeight = CameraComponent->Filmback.SensorHeight;

	// Update binding values for publishing
	for (FRshipCameraParameterBinding& Binding : ParameterBindings)
	{
		if (!Binding.bEnabled) continue;

		float Value = 0.0f;
		if (Binding.ParameterName == FName("FocalLength"))
		{
			Value = CurrentFocalLength;
		}
		else if (Binding.ParameterName == FName("Aperture"))
		{
			Value = CurrentAperture;
		}
		else if (Binding.ParameterName == FName("FocusDistance"))
		{
			Value = CurrentFocusDistance;
		}
		else if (Binding.ParameterName == FName("SensorWidth"))
		{
			Value = CurrentSensorWidth;
		}
		else if (Binding.ParameterName == FName("SensorHeight"))
		{
			Value = CurrentSensorHeight;
		}

		// Set smoothed value if not receiving
		if (!Binding.bReceive || BindingMode == ERshipCameraBindingMode::Publish)
		{
			Binding.SmoothedValue = Value;
		}
	}
}

void URshipCameraBinding::PublishCameraState()
{
	if (!Subsystem) return;

	bool bAnyChanged = false;

	// Check for changes and broadcast RS_ emitters
	for (FRshipCameraParameterBinding& Binding : ParameterBindings)
	{
		if (!Binding.bEnabled || !Binding.bPublish) continue;

		float CurrentValue = Binding.SmoothedValue * Binding.PublishScale;

		if (!bOnlyPublishOnChange || HasValueChanged(Binding.LastPublishedValue, CurrentValue))
		{
			bAnyChanged = true;
			Binding.LastPublishedValue = CurrentValue;

			// Broadcast the RS_ emitter delegate
			if (Binding.ParameterName == FName("FocalLength"))
			{
				RS_OnFocalLengthChanged.Broadcast(CurrentValue);
				OnParameterChanged.Broadcast(TEXT("FocalLength"), CurrentValue);
			}
			else if (Binding.ParameterName == FName("Aperture"))
			{
				RS_OnApertureChanged.Broadcast(CurrentValue);
				OnParameterChanged.Broadcast(TEXT("Aperture"), CurrentValue);
			}
			else if (Binding.ParameterName == FName("FocusDistance"))
			{
				RS_OnFocusDistanceChanged.Broadcast(CurrentValue);
				OnParameterChanged.Broadcast(TEXT("FocusDistance"), CurrentValue);
			}
			else if (Binding.ParameterName == FName("SensorWidth"))
			{
				RS_OnSensorWidthChanged.Broadcast(CurrentValue);
				OnParameterChanged.Broadcast(TEXT("SensorWidth"), CurrentValue);
			}
			else if (Binding.ParameterName == FName("SensorHeight"))
			{
				RS_OnSensorHeightChanged.Broadcast(CurrentValue);
				OnParameterChanged.Broadcast(TEXT("SensorHeight"), CurrentValue);
			}
		}
	}
}

float URshipCameraBinding::GetFloatFromJson(TSharedPtr<FJsonObject> Data, const FString& FieldPath)
{
	if (!Data.IsValid()) return 0.0f;

	// Handle nested paths like "camera.focalLength"
	TArray<FString> Parts;
	FieldPath.ParseIntoArray(Parts, TEXT("."));

	TSharedPtr<FJsonObject> Current = Data;
	for (int32 i = 0; i < Parts.Num() - 1; i++)
	{
		const TSharedPtr<FJsonObject>* NextObj;
		if (!Current->TryGetObjectField(Parts[i], NextObj))
		{
			return 0.0f;
		}
		Current = *NextObj;
	}

	double Value = 0.0;
	Current->TryGetNumberField(Parts.Last(), Value);
	return (float)Value;
}

bool URshipCameraBinding::HasValueChanged(float OldValue, float NewValue) const
{
	return FMath::Abs(NewValue - OldValue) > ChangeThreshold;
}

// ============================================================================
// RS_ ACTIONS
// ============================================================================

void URshipCameraBinding::RS_SetFocalLength(float FocalLength)
{
	if (!CameraComponent) return;

	CurrentFocalLength = FocalLength;
	CameraComponent->CurrentFocalLength = FocalLength;

	UE_LOG(LogRshipExec, Verbose, TEXT("RshipCameraBinding: Set FocalLength to %.1fmm"), FocalLength);
}

void URshipCameraBinding::RS_SetAperture(float Aperture)
{
	if (!CameraComponent) return;

	CurrentAperture = Aperture;
	CameraComponent->CurrentAperture = Aperture;

	UE_LOG(LogRshipExec, Verbose, TEXT("RshipCameraBinding: Set Aperture to f/%.1f"), Aperture);
}

void URshipCameraBinding::RS_SetFocusDistance(float FocusDistance)
{
	if (!CameraComponent) return;

	CurrentFocusDistance = FocusDistance;
	CameraComponent->FocusSettings.ManualFocusDistance = FocusDistance;

	UE_LOG(LogRshipExec, Verbose, TEXT("RshipCameraBinding: Set FocusDistance to %.1fcm"), FocusDistance);
}

void URshipCameraBinding::RS_SetSensorSize(float Width, float Height)
{
	if (!CameraComponent) return;

	CurrentSensorWidth = Width;
	CurrentSensorHeight = Height;
	CameraComponent->Filmback.SensorWidth = Width;
	CameraComponent->Filmback.SensorHeight = Height;

	UE_LOG(LogRshipExec, Verbose, TEXT("RshipCameraBinding: Set SensorSize to %.1fx%.1fmm"), Width, Height);
}

void URshipCameraBinding::RS_SetManualFocusEnabled(bool bEnabled)
{
	if (!CameraComponent) return;

	if (bEnabled)
	{
		CameraComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
	}
	else
	{
		CameraComponent->FocusSettings.FocusMethod = ECameraFocusMethod::DoNotOverride;
	}

	UE_LOG(LogRshipExec, Verbose, TEXT("RshipCameraBinding: Set ManualFocus to %s"), bEnabled ? TEXT("Enabled") : TEXT("Disabled"));
}

void URshipCameraBinding::RS_SetFocusMethod(int32 Method)
{
	if (!CameraComponent) return;

	ECameraFocusMethod FocusMethod = ECameraFocusMethod::DoNotOverride;
	switch (Method)
	{
		case 0: FocusMethod = ECameraFocusMethod::DoNotOverride; break;
		case 1: FocusMethod = ECameraFocusMethod::Manual; break;
		case 2: FocusMethod = ECameraFocusMethod::Tracking; break;
	}

	CameraComponent->FocusSettings.FocusMethod = FocusMethod;

	UE_LOG(LogRshipExec, Verbose, TEXT("RshipCameraBinding: Set FocusMethod to %d"), Method);
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

void URshipCameraBinding::SetCameraParameter(FName ParameterName, float Value)
{
	if (ParameterName == FName("FocalLength"))
	{
		RS_SetFocalLength(Value);
	}
	else if (ParameterName == FName("Aperture"))
	{
		RS_SetAperture(Value);
	}
	else if (ParameterName == FName("FocusDistance"))
	{
		RS_SetFocusDistance(Value);
	}
	else if (ParameterName == FName("SensorWidth"))
	{
		if (CameraComponent)
		{
			CurrentSensorWidth = Value;
			CameraComponent->Filmback.SensorWidth = Value;
		}
	}
	else if (ParameterName == FName("SensorHeight"))
	{
		if (CameraComponent)
		{
			CurrentSensorHeight = Value;
			CameraComponent->Filmback.SensorHeight = Value;
		}
	}
}

float URshipCameraBinding::GetCameraParameter(FName ParameterName) const
{
	if (ParameterName == FName("FocalLength"))
	{
		return CurrentFocalLength;
	}
	else if (ParameterName == FName("Aperture"))
	{
		return CurrentAperture;
	}
	else if (ParameterName == FName("FocusDistance"))
	{
		return CurrentFocusDistance;
	}
	else if (ParameterName == FName("SensorWidth"))
	{
		return CurrentSensorWidth;
	}
	else if (ParameterName == FName("SensorHeight"))
	{
		return CurrentSensorHeight;
	}

	return 0.0f;
}

void URshipCameraBinding::ForcePublish()
{
	ReadFromCameraComponent();

	// Force publish all values by resetting LastPublishedValue
	for (FRshipCameraParameterBinding& Binding : ParameterBindings)
	{
		Binding.LastPublishedValue = TNumericLimits<float>::Max();
	}

	PublishCameraState();
}

void URshipCameraBinding::SetBindingsEnabled(bool bEnabled)
{
	for (FRshipCameraParameterBinding& Binding : ParameterBindings)
	{
		Binding.bEnabled = bEnabled;
	}
}

void URshipCameraBinding::ResetToDefaults()
{
	RS_SetFocalLength(35.0f);
	RS_SetAperture(2.8f);
	RS_SetFocusDistance(100000.0f);
	RS_SetSensorSize(36.0f, 24.0f);
	RS_SetFocusMethod(0);
}
