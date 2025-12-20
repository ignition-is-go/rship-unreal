// Rship CineCamera Binding Implementation

#include "RshipCameraBinding.h"
#include "RshipSubsystem.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

URshipCameraBinding::URshipCameraBinding()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.016f; // ~60Hz
}

void URshipCameraBinding::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine)
	{
		Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	}

	// Auto-find CineCamera component
	if (!CameraComponent)
	{
		if (ACineCameraActor* CineActor = Cast<ACineCameraActor>(GetOwner()))
		{
			CameraComponent = CineActor->GetCineCameraComponent();
		}
		else
		{
			CameraComponent = GetOwner()->FindComponentByClass<UCineCameraComponent>();
		}
	}

	if (!CameraComponent)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipCameraBinding: No CineCameraComponent found on %s"), *GetOwner()->GetName());
		return;
	}

	PublishInterval = 1.0 / FMath::Max(1, PublishRateHz);

	UE_LOG(LogRshipExec, Log, TEXT("RshipCameraBinding: Initialized on %s"), *GetOwner()->GetName());
}

void URshipCameraBinding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void URshipCameraBinding::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CameraComponent) return;

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastPublishTime >= PublishInterval)
	{
		ReadAndPublishState();
		LastPublishTime = CurrentTime;
	}
}

void URshipCameraBinding::ReadAndPublishState()
{
	if (!CameraComponent) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Read current values
	float CurrentFocalLength = CameraComponent->CurrentFocalLength;
	float CurrentAperture = CameraComponent->CurrentAperture;
	float CurrentSqueezeFactor = CameraComponent->LensSettings.SqueezeFactor;
	float CurrentSensorWidth = CameraComponent->Filmback.SensorWidth;
	float CurrentSensorHeight = CameraComponent->Filmback.SensorHeight;
	float CurrentSensorAspectRatio = CameraComponent->Filmback.SensorAspectRatio;
	float CurrentFocusDistance = CameraComponent->CurrentFocusDistance;
	int32 CurrentFocusMethod = (int32)CameraComponent->FocusSettings.FocusMethod;
	float CurrentHFOV = CameraComponent->GetHorizontalFieldOfView();
	float CurrentVFOV = CameraComponent->GetVerticalFieldOfView();
	FVector CurrentLocation = Owner->GetActorLocation();
	FRotator CurrentRotation = Owner->GetActorRotation();

	// Publish changes
	if (!bOnlyPublishOnChange || HasValueChanged(LastFocalLength, CurrentFocalLength))
	{
		RS_OnFocalLengthChanged.Broadcast(CurrentFocalLength);
		LastFocalLength = CurrentFocalLength;
	}

	if (!bOnlyPublishOnChange || HasValueChanged(LastAperture, CurrentAperture))
	{
		RS_OnApertureChanged.Broadcast(CurrentAperture);
		LastAperture = CurrentAperture;
	}

	if (!bOnlyPublishOnChange || HasValueChanged(LastSqueezeFactor, CurrentSqueezeFactor))
	{
		RS_OnSqueezeFactorChanged.Broadcast(CurrentSqueezeFactor);
		LastSqueezeFactor = CurrentSqueezeFactor;
	}

	if (!bOnlyPublishOnChange || HasValueChanged(LastSensorWidth, CurrentSensorWidth))
	{
		RS_OnSensorWidthChanged.Broadcast(CurrentSensorWidth);
		LastSensorWidth = CurrentSensorWidth;
	}

	if (!bOnlyPublishOnChange || HasValueChanged(LastSensorHeight, CurrentSensorHeight))
	{
		RS_OnSensorHeightChanged.Broadcast(CurrentSensorHeight);
		LastSensorHeight = CurrentSensorHeight;
	}

	if (!bOnlyPublishOnChange || HasValueChanged(LastSensorAspectRatio, CurrentSensorAspectRatio))
	{
		RS_OnSensorAspectRatioChanged.Broadcast(CurrentSensorAspectRatio);
		LastSensorAspectRatio = CurrentSensorAspectRatio;
	}

	if (!bOnlyPublishOnChange || HasValueChanged(LastFocusDistance, CurrentFocusDistance, 1.0f))
	{
		RS_OnFocusDistanceChanged.Broadcast(CurrentFocusDistance);
		LastFocusDistance = CurrentFocusDistance;
	}

	if (!bOnlyPublishOnChange || LastFocusMethod != CurrentFocusMethod)
	{
		RS_OnFocusMethodChanged.Broadcast(CurrentFocusMethod);
		LastFocusMethod = CurrentFocusMethod;
	}

	if (!bOnlyPublishOnChange || HasValueChanged(LastHFOV, CurrentHFOV, 0.1f))
	{
		RS_OnHorizontalFOVChanged.Broadcast(CurrentHFOV);
		LastHFOV = CurrentHFOV;
	}

	if (!bOnlyPublishOnChange || HasValueChanged(LastVFOV, CurrentVFOV, 0.1f))
	{
		RS_OnVerticalFOVChanged.Broadcast(CurrentVFOV);
		LastVFOV = CurrentVFOV;
	}

	if (!bOnlyPublishOnChange || !CurrentLocation.Equals(LastLocation, 0.1f))
	{
		RS_OnLocationChanged.Broadcast(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z);
		LastLocation = CurrentLocation;
	}

	if (!bOnlyPublishOnChange || !CurrentRotation.Equals(LastRotation, 0.1f))
	{
		RS_OnRotationChanged.Broadcast(CurrentRotation.Pitch, CurrentRotation.Yaw, CurrentRotation.Roll);
		LastRotation = CurrentRotation;
	}
}

bool URshipCameraBinding::HasValueChanged(float OldValue, float NewValue, float Threshold) const
{
	return FMath::Abs(NewValue - OldValue) > Threshold;
}

// ============================================================================
// RS_ ACTIONS - Lens Controls
// ============================================================================

void URshipCameraBinding::RS_SetFocalLength(float FocalLengthMM)
{
	if (!CameraComponent) return;
	CameraComponent->SetCurrentFocalLength(FocalLengthMM);
}

void URshipCameraBinding::RS_SetAperture(float FStop)
{
	if (!CameraComponent) return;
	CameraComponent->SetCurrentAperture(FStop);
}

void URshipCameraBinding::RS_SetFocalLengthRange(float MinMM, float MaxMM)
{
	if (!CameraComponent) return;
	CameraComponent->LensSettings.MinFocalLength = MinMM;
	CameraComponent->LensSettings.MaxFocalLength = MaxMM;
}

void URshipCameraBinding::RS_SetApertureRange(float MinFStop, float MaxFStop)
{
	if (!CameraComponent) return;
	CameraComponent->LensSettings.MinFStop = MinFStop;
	CameraComponent->LensSettings.MaxFStop = MaxFStop;
}

void URshipCameraBinding::RS_SetMinimumFocusDistance(float DistanceCM)
{
	if (!CameraComponent) return;
	// LensSettings stores MinimumFocusDistance in mm
	CameraComponent->LensSettings.MinimumFocusDistance = DistanceCM * 10.0f;
}

void URshipCameraBinding::RS_SetSqueezeFactor(float Squeeze)
{
	if (!CameraComponent) return;
	CameraComponent->LensSettings.SqueezeFactor = FMath::Clamp(Squeeze, 1.0f, 2.0f);
}

void URshipCameraBinding::RS_SetDiaphragmBladeCount(int32 BladeCount)
{
	if (!CameraComponent) return;
	CameraComponent->LensSettings.DiaphragmBladeCount = FMath::Clamp(BladeCount, 4, 16);
}

void URshipCameraBinding::RS_SetLensPreset(const FString& PresetName)
{
	if (!CameraComponent) return;
	CameraComponent->SetLensPresetByName(PresetName);
}

// ============================================================================
// RS_ ACTIONS - Sensor/Filmback Controls
// ============================================================================

void URshipCameraBinding::RS_SetSensorSize(float WidthMM, float HeightMM)
{
	if (!CameraComponent) return;
	FCameraFilmbackSettings NewFilmback = CameraComponent->Filmback;
	NewFilmback.SensorWidth = WidthMM;
	NewFilmback.SensorHeight = HeightMM;
	NewFilmback.RecalcSensorAspectRatio();
	CameraComponent->SetFilmback(NewFilmback);
}

void URshipCameraBinding::RS_SetSensorOffset(float HorizontalMM, float VerticalMM)
{
	if (!CameraComponent) return;
	FCameraFilmbackSettings NewFilmback = CameraComponent->Filmback;
	NewFilmback.SensorHorizontalOffset = HorizontalMM;
	NewFilmback.SensorVerticalOffset = VerticalMM;
	CameraComponent->SetFilmback(NewFilmback);
}

void URshipCameraBinding::RS_SetFilmbackPreset(const FString& PresetName)
{
	if (!CameraComponent) return;
	CameraComponent->SetFilmbackPresetByName(PresetName);
}

// ============================================================================
// RS_ ACTIONS - Focus Controls
// ============================================================================

void URshipCameraBinding::RS_SetFocusDistance(float DistanceCM)
{
	if (!CameraComponent) return;
	FCameraFocusSettings NewFocus = CameraComponent->FocusSettings;
	NewFocus.ManualFocusDistance = DistanceCM;
	CameraComponent->SetFocusSettings(NewFocus);
}

void URshipCameraBinding::RS_SetFocusMethod(int32 Method)
{
	if (!CameraComponent) return;
	FCameraFocusSettings NewFocus = CameraComponent->FocusSettings;
	switch (Method)
	{
		case 0: NewFocus.FocusMethod = ECameraFocusMethod::DoNotOverride; break;
		case 1: NewFocus.FocusMethod = ECameraFocusMethod::Manual; break;
		case 2: NewFocus.FocusMethod = ECameraFocusMethod::Tracking; break;
		case 3: NewFocus.FocusMethod = ECameraFocusMethod::Disable; break;
		default: NewFocus.FocusMethod = ECameraFocusMethod::Manual; break;
	}
	CameraComponent->SetFocusSettings(NewFocus);
}

void URshipCameraBinding::RS_SetFocusOffset(float OffsetCM)
{
	if (!CameraComponent) return;
	FCameraFocusSettings NewFocus = CameraComponent->FocusSettings;
	NewFocus.FocusOffset = OffsetCM;
	CameraComponent->SetFocusSettings(NewFocus);
}

void URshipCameraBinding::RS_SetSmoothFocus(bool bEnabled, float InterpSpeed)
{
	if (!CameraComponent) return;
	FCameraFocusSettings NewFocus = CameraComponent->FocusSettings;
	NewFocus.bSmoothFocusChanges = bEnabled;
	NewFocus.FocusSmoothingInterpSpeed = InterpSpeed;
	CameraComponent->SetFocusSettings(NewFocus);
}

// ============================================================================
// RS_ ACTIONS - Crop/Masking Controls
// ============================================================================

void URshipCameraBinding::RS_SetCropAspectRatio(float AspectRatio)
{
	if (!CameraComponent) return;
	FPlateCropSettings NewCrop;
	NewCrop.AspectRatio = AspectRatio;
	CameraComponent->SetCropSettings(NewCrop);
}

void URshipCameraBinding::RS_SetCropPreset(const FString& PresetName)
{
	if (!CameraComponent) return;
	CameraComponent->SetCropPresetByName(PresetName);
}

// ============================================================================
// RS_ ACTIONS - Transform Controls
// ============================================================================

void URshipCameraBinding::RS_SetLocation(float X, float Y, float Z)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	Owner->SetActorLocation(FVector(X, Y, Z));
}

void URshipCameraBinding::RS_SetRotation(float Pitch, float Yaw, float Roll)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	Owner->SetActorRotation(FRotator(Pitch, Yaw, Roll));
}

void URshipCameraBinding::RS_AddLocation(float DeltaX, float DeltaY, float DeltaZ)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	FVector CurrentLocation = Owner->GetActorLocation();
	Owner->SetActorLocation(CurrentLocation + FVector(DeltaX, DeltaY, DeltaZ));
}

void URshipCameraBinding::RS_AddRotation(float DeltaPitch, float DeltaYaw, float DeltaRoll)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	FRotator CurrentRotation = Owner->GetActorRotation();
	Owner->SetActorRotation(CurrentRotation + FRotator(DeltaPitch, DeltaYaw, DeltaRoll));
}

void URshipCameraBinding::RS_LookAt(float TargetX, float TargetY, float TargetZ)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector TargetLocation(TargetX, TargetY, TargetZ);
	FVector CameraLocation = Owner->GetActorLocation();
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(CameraLocation, TargetLocation);
	Owner->SetActorRotation(LookAtRotation);
}

// ============================================================================
// RS_ ACTIONS - Exposure Controls
// ============================================================================

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6)
void URshipCameraBinding::RS_SetExposureMethod(int32 Method)
{
	if (!CameraComponent) return;
	switch (Method)
	{
		case 0: CameraComponent->ExposureMethod = ECameraExposureMethod::DoNotOverride; break;
		case 1: CameraComponent->ExposureMethod = ECameraExposureMethod::Enabled; break;
		default: CameraComponent->ExposureMethod = ECameraExposureMethod::DoNotOverride; break;
	}
}
#endif

void URshipCameraBinding::RS_SetNearClipPlane(float DistanceCM)
{
	if (!CameraComponent) return;
	if (DistanceCM > 0.0f)
	{
		CameraComponent->bOverride_CustomNearClippingPlane = true;
		CameraComponent->SetCustomNearClippingPlane(DistanceCM);
	}
	else
	{
		CameraComponent->bOverride_CustomNearClippingPlane = false;
	}
}

// ============================================================================
// RS_ ACTIONS - Utility
// ============================================================================

void URshipCameraBinding::RS_ResetToDefaults()
{
	if (!CameraComponent) return;

	// Reset lens
	CameraComponent->SetCurrentFocalLength(35.0f);
	CameraComponent->SetCurrentAperture(2.8f);
	CameraComponent->LensSettings.SqueezeFactor = 1.0f;
	CameraComponent->LensSettings.DiaphragmBladeCount = 8;

	// Reset filmback to Super 35mm
	FCameraFilmbackSettings DefaultFilmback;
	DefaultFilmback.SensorWidth = 24.89f;
	DefaultFilmback.SensorHeight = 18.67f;
	DefaultFilmback.SensorHorizontalOffset = 0.0f;
	DefaultFilmback.SensorVerticalOffset = 0.0f;
	DefaultFilmback.RecalcSensorAspectRatio();
	CameraComponent->SetFilmback(DefaultFilmback);

	// Reset focus
	FCameraFocusSettings DefaultFocus;
	DefaultFocus.FocusMethod = ECameraFocusMethod::Manual;
	DefaultFocus.ManualFocusDistance = 100000.0f;
	DefaultFocus.FocusOffset = 0.0f;
	DefaultFocus.bSmoothFocusChanges = false;
	CameraComponent->SetFocusSettings(DefaultFocus);

	// Reset crop
	FPlateCropSettings DefaultCrop;
	DefaultCrop.AspectRatio = 0.0f;
	CameraComponent->SetCropSettings(DefaultCrop);

	// Reset exposure
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6)
	CameraComponent->ExposureMethod = ECameraExposureMethod::DoNotOverride;
#endif
	CameraComponent->bOverride_CustomNearClippingPlane = false;
}

void URshipCameraBinding::RS_CopyFromCamera(const FString& CameraActorName)
{
	if (!CameraComponent) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Find the source camera
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, ACineCameraActor::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		if (Actor->GetName() == CameraActorName || Actor->GetActorLabel() == CameraActorName)
		{
			ACineCameraActor* SourceCamera = Cast<ACineCameraActor>(Actor);
			if (SourceCamera)
			{
				UCineCameraComponent* SourceComp = SourceCamera->GetCineCameraComponent();
				if (SourceComp)
				{
					// Copy all settings
					CameraComponent->SetCurrentFocalLength(SourceComp->CurrentFocalLength);
					CameraComponent->SetCurrentAperture(SourceComp->CurrentAperture);
					CameraComponent->SetFilmback(SourceComp->Filmback);
					CameraComponent->SetLensSettings(SourceComp->LensSettings);
					CameraComponent->SetFocusSettings(SourceComp->FocusSettings);
					CameraComponent->SetCropSettings(SourceComp->CropSettings);
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6)
					CameraComponent->ExposureMethod = SourceComp->ExposureMethod;
#endif

					UE_LOG(LogRshipExec, Log, TEXT("RshipCameraBinding: Copied settings from %s"), *CameraActorName);
					return;
				}
			}
		}
	}

	UE_LOG(LogRshipExec, Warning, TEXT("RshipCameraBinding: Could not find camera named %s"), *CameraActorName);
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

void URshipCameraBinding::ForcePublish()
{
	// Reset all cached values to force publishing
	LastFocalLength = -1.0f;
	LastAperture = -1.0f;
	LastSqueezeFactor = -1.0f;
	LastSensorWidth = -1.0f;
	LastSensorHeight = -1.0f;
	LastSensorAspectRatio = -1.0f;
	LastFocusDistance = -1.0f;
	LastFocusMethod = -1;
	LastHFOV = -1.0f;
	LastVFOV = -1.0f;
	LastLocation = FVector(TNumericLimits<float>::Max());
	LastRotation = FRotator(TNumericLimits<float>::Max(), 0, 0);

	ReadAndPublishState();
}

FString URshipCameraBinding::GetCameraStateJson() const
{
	if (!CameraComponent) return TEXT("{}");

	AActor* Owner = GetOwner();

	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);

	// Lens
	TSharedPtr<FJsonObject> Lens = MakeShareable(new FJsonObject);
	Lens->SetNumberField(TEXT("focalLength"), CameraComponent->CurrentFocalLength);
	Lens->SetNumberField(TEXT("aperture"), CameraComponent->CurrentAperture);
	Lens->SetNumberField(TEXT("minFocalLength"), CameraComponent->LensSettings.MinFocalLength);
	Lens->SetNumberField(TEXT("maxFocalLength"), CameraComponent->LensSettings.MaxFocalLength);
	Lens->SetNumberField(TEXT("minFStop"), CameraComponent->LensSettings.MinFStop);
	Lens->SetNumberField(TEXT("maxFStop"), CameraComponent->LensSettings.MaxFStop);
	Lens->SetNumberField(TEXT("squeezeFactor"), CameraComponent->LensSettings.SqueezeFactor);
	Lens->SetNumberField(TEXT("diaphragmBladeCount"), CameraComponent->LensSettings.DiaphragmBladeCount);
	Json->SetObjectField(TEXT("lens"), Lens);

	// Sensor
	TSharedPtr<FJsonObject> Sensor = MakeShareable(new FJsonObject);
	Sensor->SetNumberField(TEXT("width"), CameraComponent->Filmback.SensorWidth);
	Sensor->SetNumberField(TEXT("height"), CameraComponent->Filmback.SensorHeight);
	Sensor->SetNumberField(TEXT("horizontalOffset"), CameraComponent->Filmback.SensorHorizontalOffset);
	Sensor->SetNumberField(TEXT("verticalOffset"), CameraComponent->Filmback.SensorVerticalOffset);
	Sensor->SetNumberField(TEXT("aspectRatio"), CameraComponent->Filmback.SensorAspectRatio);
	Json->SetObjectField(TEXT("sensor"), Sensor);

	// Focus
	TSharedPtr<FJsonObject> Focus = MakeShareable(new FJsonObject);
	Focus->SetNumberField(TEXT("method"), (int32)CameraComponent->FocusSettings.FocusMethod);
	Focus->SetNumberField(TEXT("distance"), CameraComponent->CurrentFocusDistance);
	Focus->SetNumberField(TEXT("manualDistance"), CameraComponent->FocusSettings.ManualFocusDistance);
	Focus->SetNumberField(TEXT("offset"), CameraComponent->FocusSettings.FocusOffset);
	Focus->SetBoolField(TEXT("smoothChanges"), CameraComponent->FocusSettings.bSmoothFocusChanges);
	Focus->SetNumberField(TEXT("smoothSpeed"), CameraComponent->FocusSettings.FocusSmoothingInterpSpeed);
	Json->SetObjectField(TEXT("focus"), Focus);

	// FOV
	TSharedPtr<FJsonObject> Fov = MakeShareable(new FJsonObject);
	Fov->SetNumberField(TEXT("horizontal"), CameraComponent->GetHorizontalFieldOfView());
	Fov->SetNumberField(TEXT("vertical"), CameraComponent->GetVerticalFieldOfView());
	Json->SetObjectField(TEXT("fov"), Fov);

	// Crop
	TSharedPtr<FJsonObject> Crop = MakeShareable(new FJsonObject);
	Crop->SetNumberField(TEXT("aspectRatio"), CameraComponent->CropSettings.AspectRatio);
	Json->SetObjectField(TEXT("crop"), Crop);

	// Transform
	if (Owner)
	{
		FVector Location = Owner->GetActorLocation();
		FRotator Rotation = Owner->GetActorRotation();

		TSharedPtr<FJsonObject> Transform = MakeShareable(new FJsonObject);
		TSharedPtr<FJsonObject> Loc = MakeShareable(new FJsonObject);
		Loc->SetNumberField(TEXT("x"), Location.X);
		Loc->SetNumberField(TEXT("y"), Location.Y);
		Loc->SetNumberField(TEXT("z"), Location.Z);
		Transform->SetObjectField(TEXT("location"), Loc);

		TSharedPtr<FJsonObject> Rot = MakeShareable(new FJsonObject);
		Rot->SetNumberField(TEXT("pitch"), Rotation.Pitch);
		Rot->SetNumberField(TEXT("yaw"), Rotation.Yaw);
		Rot->SetNumberField(TEXT("roll"), Rotation.Roll);
		Transform->SetObjectField(TEXT("rotation"), Rot);

		Json->SetObjectField(TEXT("transform"), Transform);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);
	return OutputString;
}
