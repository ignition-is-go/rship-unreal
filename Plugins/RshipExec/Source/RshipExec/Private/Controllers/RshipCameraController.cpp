#include "Controllers/RshipCameraController.h"

#include "RshipTargetComponent.h"
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"

namespace
{
	void RequestControllerRescan(AActor* Owner, const bool bOnlyIfRegistered)
	{
		if (!Owner)
		{
			return;
		}

		if (URshipTargetComponent* TargetComponent = Owner->FindComponentByClass<URshipTargetComponent>())
		{
			if (!bOnlyIfRegistered || TargetComponent->IsRegistered())
			{
				TargetComponent->RescanSiblingComponents();
			}
		}
	}
}

void URshipCameraController::OnRegister()
{
	Super::OnRegister();
	RequestControllerRescan(GetOwner(), false);
}

void URshipCameraController::BeginPlay()
{
	Super::BeginPlay();
	RequestControllerRescan(GetOwner(), false);
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

void URshipCameraController::NotifyCameraEdited(UCameraComponent* InCamera) const
{
	if (!InCamera)
	{
		return;
	}

	InCamera->MarkRenderStateDirty();

#if WITH_EDITOR
	InCamera->Modify();
	InCamera->PostEditChange();
	if (AActor* Owner = GetOwner())
	{
		Owner->Modify();
		Owner->MarkPackageDirty();
	}
#endif
}

void URshipCameraController::RegisterRshipWhitelistedActions(URshipTargetComponent* TargetComponent)
{
	if (!TargetComponent)
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
		TargetComponent->RegisterWhitelistedProperty(Camera, TEXT("FieldOfView"));
		TargetComponent->RegisterWhitelistedProperty(Camera, TEXT("AspectRatio"));
		TargetComponent->RegisterWhitelistedProperty(Camera, TEXT("bConstrainAspectRatio"));
		TargetComponent->RegisterWhitelistedProperty(Camera, TEXT("ProjectionMode"));
		TargetComponent->RegisterWhitelistedProperty(Camera, TEXT("OrthoWidth"));
		TargetComponent->RegisterWhitelistedProperty(Camera, TEXT("OrthoNearClipPlane"));
		TargetComponent->RegisterWhitelistedProperty(Camera, TEXT("OrthoFarClipPlane"));
		TargetComponent->RegisterWhitelistedProperty(Camera, TEXT("PostProcessBlendWeight"));
	}

	if (!bIncludeCineCameraProperties)
	{
		return;
	}

	if (UCineCameraComponent* Cine = ResolveCineCameraComponent())
	{
		TargetComponent->RegisterWhitelistedProperty(Cine, TEXT("CurrentFocalLength"));
		TargetComponent->RegisterWhitelistedProperty(Cine, TEXT("CurrentAperture"));
		TargetComponent->RegisterWhitelistedProperty(Cine, TEXT("CurrentFocusDistance"));
		TargetComponent->RegisterWhitelistedProperty(Cine, TEXT("Filmback"));
		TargetComponent->RegisterWhitelistedProperty(Cine, TEXT("LensSettings"));
		TargetComponent->RegisterWhitelistedProperty(Cine, TEXT("FocusSettings"));
		TargetComponent->RegisterWhitelistedProperty(Cine, TEXT("CropSettings"));
	}
}

void URshipCameraController::OnRshipAfterTake(URshipTargetComponent* TargetComponent, const FString& ActionName, UObject* ActionOwner)
{
	(void)TargetComponent;
	(void)ActionName;

	if (ActionOwner == ResolveCameraComponent())
	{
		NotifyCameraEdited(Cast<UCameraComponent>(ActionOwner));
		return;
	}

	if (ActionOwner == ResolveCineCameraComponent())
	{
		NotifyCameraEdited(Cast<UCameraComponent>(ActionOwner));
	}
}

