#include "Controllers/RshipTransformController.h"

#include "RshipTargetComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
	void RequestTransformControllerRescan(AActor* Owner, const bool bOnlyIfRegistered)
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

	RequestTransformControllerRescan(GetOwner(), false);
}

void URshipTransformController::BeginPlay()
{
	Super::BeginPlay();
	RequestTransformControllerRescan(GetOwner(), false);
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

	const UWorld* World = Root->GetWorld();
	const bool bIsEditorWorld = (World && !World->IsGameWorld());

	if (ActionName == TEXT("RelativeLocation") || ActionName == TEXT("Location"))
	{
		if (bIsEditorWorld)
		{
			Root->SetRelativeLocation(Root->GetRelativeLocation(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			Root->SetRelativeLocation_Direct(Root->GetRelativeLocation());
		}
	}
	else if (ActionName == TEXT("RelativeRotation") || ActionName == TEXT("Rotation"))
	{
		if (bIsEditorWorld)
		{
			Root->SetRelativeRotation(Root->GetRelativeRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			Root->SetRelativeRotation_Direct(Root->GetRelativeRotation());
		}
	}
	else if (ActionName == TEXT("RelativeScale3D") || ActionName == TEXT("Scale"))
	{
		if (bIsEditorWorld)
		{
			Root->SetRelativeScale3D(Root->GetRelativeScale3D());
		}
		else
		{
			Root->SetRelativeScale3D_Direct(Root->GetRelativeScale3D());
		}
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


	// Keep editor viewport updated without PostEdit transaction/reconstruction paths.
	Owner->MarkComponentsRenderStateDirty();
#endif
}


