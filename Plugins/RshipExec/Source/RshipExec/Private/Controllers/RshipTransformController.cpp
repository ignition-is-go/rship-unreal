#include "Controllers/RshipTransformController.h"

#include "RshipSubsystem.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

void URshipTransformController::OnBeforeRegisterRshipBindings()
{
	if (AActor* Owner = GetOwner())
	{
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			if (Root->Mobility != EComponentMobility::Movable)
			{
				Root->SetMobility(EComponentMobility::Movable);
			}
		}
	}
}

void URshipTransformController::RegisterOrRefreshTarget()
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

	FRshipTargetProxy ParentIdentity = Subsystem->EnsureActorIdentity(Owner);
	if (!ParentIdentity.IsValid())
	{
		return;
	}

	USceneComponent* Root = Owner->GetRootComponent();
	if (!Root)
	{
		return;
	}

	if (bExposeLocation)
	{
		ParentIdentity
			.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipTransformController, SetRelativeLocationAction), TEXT("RelativeLocation"))
			.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipTransformController, SetRelativeLocationAction), TEXT("Location"));
	}
	if (bExposeRotation)
	{
		ParentIdentity
			.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipTransformController, SetRelativeRotationAction), TEXT("RelativeRotation"))
			.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipTransformController, SetRelativeRotationAction), TEXT("Rotation"));
	}
	if (bExposeScale)
	{
		ParentIdentity
			.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipTransformController, SetRelativeScaleAction), TEXT("RelativeScale3D"))
			.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipTransformController, SetRelativeScaleAction), TEXT("Scale"));
	}
}

void URshipTransformController::SetRelativeLocationAction(float X, float Y, float Z)
{
	if (AActor* Owner = GetOwner())
	{
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			Root->SetRelativeLocation(FVector(X, Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void URshipTransformController::SetRelativeRotationAction(float X, float Y, float Z)
{
	if (AActor* Owner = GetOwner())
	{
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			Root->SetRelativeRotation(FRotator(X, Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void URshipTransformController::SetRelativeScaleAction(float X, float Y, float Z)
{
	if (AActor* Owner = GetOwner())
	{
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			Root->SetRelativeScale3D(FVector(X, Y, Z));
		}
	}
}

