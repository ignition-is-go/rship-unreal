#include "Controllers/RshipCameraController.h"

#include "RshipSubsystem.h"
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"
#include "CineCameraSettings.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"

void URshipCameraController::OnBeforeRegisterRshipBindings()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(bPublishStateEmitters);
}

void URshipCameraController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPublishStateEmitters)
	{
		return;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	const double PublishInterval = 1.0 / static_cast<double>(FMath::Max(1, PublishRateHz));
	if (NowSeconds - LastPublishTimeSeconds < PublishInterval)
	{
		return;
	}

	LastPublishTimeSeconds = NowSeconds;
	PublishState();
}

FString URshipCameraController::GetTargetId() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !GEngine)
	{
		return FString();
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return FString();
	}

	FRshipTargetProxy RootTarget = Subsystem->EnsureActorIdentity(const_cast<AActor*>(Owner));
	return RootTarget.IsValid() ? RootTarget.GetId() : FString();
}

void URshipCameraController::RegisterOrRefreshTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner || !GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FRshipTargetProxy Target = Subsystem->EnsureActorIdentity(Owner);
	if (!Target.IsValid())
	{
		return;
	}

	FRshipTargetProxy SensorTarget = Target.AddTarget(TEXT("sensor"), TEXT("Sensor"));
	FRshipTargetProxy LensTarget = Target.AddTarget(TEXT("lens"), TEXT("Lens"));
	if (!SensorTarget.IsValid() || !LensTarget.IsValid())
	{
		return;
	}

	UCameraComponent* Camera = ResolveCameraComponent();
	if (!Camera)
	{
		return;
	}

	if (bIncludeCommonCameraProperties)
	{
		SensorTarget
			.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipCameraController, SetFieldOfViewAction), TEXT("FieldOfView"))
			.AddPropertyAction(Camera, TEXT("AspectRatio"))
			.AddPropertyAction(Camera, TEXT("bConstrainAspectRatio"))
			.AddPropertyAction(Camera, TEXT("ProjectionMode"))
			.AddPropertyAction(Camera, TEXT("OrthoWidth"))
			.AddPropertyAction(Camera, TEXT("OrthoNearClipPlane"))
			.AddPropertyAction(Camera, TEXT("OrthoFarClipPlane"));

		Target.AddPropertyAction(Camera, TEXT("PostProcessBlendWeight"));
	}

	if (bIncludeCineCameraProperties)
	{
		if (UCineCameraComponent* Cine = ResolveCineCameraComponent())
		{
			LensTarget
				.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipCameraController, SetFocalLengthAction), TEXT("CurrentFocalLength"))
				.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipCameraController, SetFocalLengthAction), TEXT("FocalLength"))
				.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipCameraController, SetApertureAction), TEXT("CurrentAperture"))
				.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipCameraController, SetApertureAction), TEXT("Aperture"))
				.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipCameraController, SetFocusDistanceAction), TEXT("CurrentFocusDistance"))
				.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipCameraController, SetFocusDistanceAction), TEXT("FocusDistance"))
				.AddPropertyAction(Cine, TEXT("LensSettings"))
				.AddPropertyAction(Cine, TEXT("FocusSettings"));

			SensorTarget
				.AddPropertyAction(Cine, TEXT("Filmback"))
				.AddPropertyAction(Cine, TEXT("CropSettings"));
		}
	}

	if (bPublishStateEmitters)
	{
		Target
			.AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipCameraController, OnFocalLengthChanged), TEXT("focalLength"))
			.AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipCameraController, OnApertureChanged), TEXT("aperture"))
			.AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipCameraController, OnFocusDistanceChanged), TEXT("focusDistance"))
			.AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipCameraController, OnHorizontalFovChanged), TEXT("horizontalFov"))
			.AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipCameraController, OnVerticalFovChanged), TEXT("verticalFov"))
			.AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipCameraController, OnLocationChanged), TEXT("location"))
			.AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipCameraController, OnRotationChanged), TEXT("rotation"));
	}
}

void URshipCameraController::SetFieldOfViewAction(float Value)
{
	if (UCameraComponent* Camera = ResolveCameraComponent())
	{
		Camera->SetFieldOfView(Value);
	}
}

void URshipCameraController::SetFocalLengthAction(float Value)
{
	if (UCineCameraComponent* Cine = ResolveCineCameraComponent())
	{
		Cine->SetCurrentFocalLength(Value);
	}
}

void URshipCameraController::SetApertureAction(float Value)
{
	if (UCineCameraComponent* Cine = ResolveCineCameraComponent())
	{
		Cine->SetCurrentAperture(Value);
	}
}

void URshipCameraController::SetFocusDistanceAction(float Value)
{
	if (UCineCameraComponent* Cine = ResolveCineCameraComponent())
	{
		FCameraFocusSettings FocusSettings = Cine->FocusSettings;
		FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		FocusSettings.ManualFocusDistance = Value;
		Cine->SetFocusSettings(FocusSettings);
	}
}

UCameraComponent* URshipCameraController::ResolveCameraComponent() const
{
	if (AActor* Owner = GetOwner())
	{
		if (UCineCameraComponent* Cine = Owner->FindComponentByClass<UCineCameraComponent>())
		{
			return Cine;
		}
		return Owner->FindComponentByClass<UCameraComponent>();
	}

	return nullptr;
}

UCineCameraComponent* URshipCameraController::ResolveCineCameraComponent() const
{
	if (AActor* Owner = GetOwner())
	{
		return Owner->FindComponentByClass<UCineCameraComponent>();
	}

	return nullptr;
}

void URshipCameraController::PublishState()
{
	AActor* Owner = GetOwner();
	UCameraComponent* Camera = ResolveCameraComponent();
	if (!Owner || !Camera)
	{
		return;
	}
	if (!GEngine)
	{
		return;
	}
	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	const FString TargetId = GetTargetId();
	if (!Subsystem || TargetId.IsEmpty())
	{
		return;
	}

	const FVector Location = Owner->GetActorLocation();
	{
		TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("x"), Location.X);
		Data->SetNumberField(TEXT("y"), Location.Y);
		Data->SetNumberField(TEXT("z"), Location.Z);
		Subsystem->PulseEmitter(TargetId, TEXT("location"), Data);
	}

	const FRotator Rotation = Owner->GetActorRotation();
	{
		TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("x"), Rotation.Pitch);
		Data->SetNumberField(TEXT("y"), Rotation.Yaw);
		Data->SetNumberField(TEXT("z"), Rotation.Roll);
		Subsystem->PulseEmitter(TargetId, TEXT("rotation"), Data);
	}

	if (UCineCameraComponent* Cine = ResolveCineCameraComponent())
	{
		TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("value"), Cine->CurrentFocalLength);
		Subsystem->PulseEmitter(TargetId, TEXT("focalLength"), Data);
		Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("value"), Cine->CurrentAperture);
		Subsystem->PulseEmitter(TargetId, TEXT("aperture"), Data);
		Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("value"), Cine->CurrentFocusDistance);
		Subsystem->PulseEmitter(TargetId, TEXT("focusDistance"), Data);
		Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("value"), Cine->GetHorizontalFieldOfView());
		Subsystem->PulseEmitter(TargetId, TEXT("horizontalFov"), Data);
		Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("value"), Cine->GetVerticalFieldOfView());
		Subsystem->PulseEmitter(TargetId, TEXT("verticalFov"), Data);
	}
	else
	{
		TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("value"), Camera->FieldOfView);
		Subsystem->PulseEmitter(TargetId, TEXT("horizontalFov"), Data);
		Data = MakeShareable(new FJsonObject);
		Data->SetNumberField(TEXT("value"), Camera->FieldOfView);
		Subsystem->PulseEmitter(TargetId, TEXT("verticalFov"), Data);
	}
}
