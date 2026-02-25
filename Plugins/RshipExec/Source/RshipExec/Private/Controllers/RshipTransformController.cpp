#include "Controllers/RshipTransformController.h"

#include "RshipTargetComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

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

void URshipTransformController::OnRegister()
{
	Super::OnRegister();

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

	RequestControllerRescan(GetOwner(), false);
}

void URshipTransformController::BeginPlay()
{
	Super::BeginPlay();
	RequestControllerRescan(GetOwner(), false);
}

void URshipTransformController::RegisterRshipWhitelistedActions(URshipTargetComponent* TargetComponent)
{
	if (!TargetComponent)
	{
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}

	if (bExposeLocation)
	{
		TargetComponent->RegisterWhitelistedProperty(Root, TEXT("RelativeLocation"));
		TargetComponent->RegisterWhitelistedProperty(Root, TEXT("RelativeLocation"), TEXT("Location"));
	}
	if (bExposeRotation)
	{
		TargetComponent->RegisterWhitelistedProperty(Root, TEXT("RelativeRotation"));
		TargetComponent->RegisterWhitelistedProperty(Root, TEXT("RelativeRotation"), TEXT("Rotation"));
	}
	if (bExposeScale)
	{
		TargetComponent->RegisterWhitelistedProperty(Root, TEXT("RelativeScale3D"));
		TargetComponent->RegisterWhitelistedProperty(Root, TEXT("RelativeScale3D"), TEXT("Scale"));
	}
}

bool URshipTransformController::IsTransformAction(const FString& ActionName) const
{
	return ActionName == TEXT("RelativeLocation") || ActionName == TEXT("RelativeRotation") || ActionName == TEXT("RelativeScale3D") ||
		ActionName == TEXT("Location") || ActionName == TEXT("Rotation") || ActionName == TEXT("Scale");
}

void URshipTransformController::OnRshipAfterTake(URshipTargetComponent* TargetComponent, const FString& ActionName, UObject* ActionOwner)
{
	(void)TargetComponent;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		return;
	}

	USceneComponent* Root = Owner->GetRootComponent();
	if (!IsValid(Root) || ActionOwner != Root || !IsTransformAction(ActionName))
	{
		return;
	}

	ApplyTransformRuntimeRefresh(Root, ActionName);
	NotifyEditorTransformChanged();
}

void URshipTransformController::ApplyTransformRuntimeRefresh(USceneComponent* Root, const FString& ActionName) const
{
	if (!IsValid(Root))
	{
		return;
	}

	if (ActionName == TEXT("RelativeLocation") || ActionName == TEXT("Location"))
	{
		Root->SetRelativeLocation_Direct(Root->GetRelativeLocation());
	}
	else if (ActionName == TEXT("RelativeRotation") || ActionName == TEXT("Rotation"))
	{
		Root->SetRelativeRotation_Direct(Root->GetRelativeRotation());
	}
	else if (ActionName == TEXT("RelativeScale3D") || ActionName == TEXT("Scale"))
	{
		Root->SetRelativeScale3D_Direct(Root->GetRelativeScale3D());
	}

	Root->UpdateComponentToWorld(EUpdateTransformFlags::PropagateFromParent, ETeleportType::TeleportPhysics);
	Root->MarkRenderTransformDirty();

	if (AActor* Owner = GetOwner())
	{
		if (IsValid(Owner) && !Owner->IsActorBeingDestroyed())
		{
			Owner->SetActorTransform(Root->GetComponentTransform(), false, nullptr, ETeleportType::TeleportPhysics);
			Owner->MarkComponentsRenderStateDirty();
		}
	}
}

void URshipTransformController::NotifyEditorTransformChanged() const
{
#if WITH_EDITOR
	if (!IsInGameThread())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	Owner->Modify();
	Owner->PostEditMove(true);
	Owner->MarkPackageDirty();
#endif
}


