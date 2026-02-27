// Fill out your copyright notice in the Description page of Project Settings.

#include "RshipActorRegistrationComponent.h"
#include "RshipTargetGroup.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RshipSubsystem.h"
#include "GameFramework/Actor.h"
#include "Logs.h"
#include "Core/RshipBindingContributor.h"
#include "Controllers/RshipControllerComponent.h"

void URshipActorRegistrationComponent::OnRegister()
{
	Super::OnRegister();
	PrimaryComponentTick.bCanEverTick = false;
	SetComponentTickEnabled(false);
	Register();
}

void URshipActorRegistrationComponent::OnComponentDestroyed(bool bDestoryHierarchy)
{
	if (GEngine)
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			Subsystem->UnregisterTargetComponent(this);

			if (URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager())
			{
				GroupManager->UnregisterTarget(this);
			}
		}
	}

	if (TargetData)
	{
		TargetData->SetBoundTargetComponent(nullptr);
		delete TargetData;
		TargetData = nullptr;
	}

	Super::OnComponentDestroyed(bDestoryHierarchy);
}

void URshipActorRegistrationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URshipActorRegistrationComponent::OnDataReceived()
{
	OnRshipData.Broadcast();
}

TArray<FString> URshipActorRegistrationComponent::BuildFullParentTargetIds(const FString& ServiceId) const
{
	TArray<FString> Result;
	Result.Reserve(ParentTargetIds.Num());

	for (const FString& ParentId : ParentTargetIds)
	{
		const FString Trimmed = ParentId.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			continue;
		}

		if (Trimmed.Contains(TEXT(":")))
		{
			Result.Add(Trimmed);
		}
		else
		{
			Result.Add(ServiceId + TEXT(":") + Trimmed);
		}
	}

	return Result;
}

void URshipActorRegistrationComponent::Reconnect()
{
	if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
	{
		Subsystem->Reconnect();
	}
}

FString URshipActorRegistrationComponent::GetFullTargetId() const
{
	if (TargetData)
	{
		return TargetData->GetId();
	}

	if (!GEngine)
	{
		return targetName;
	}

	if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
	{
		return targetName.Contains(TEXT(":")) ? targetName : (Subsystem->GetServiceId() + TEXT(":") + targetName);
	}

	return targetName;
}

FRshipTargetRegistrar URshipActorRegistrationComponent::GetTargetRegistrar() const
{
	if (!GEngine)
	{
		return FRshipTargetRegistrar();
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return FRshipTargetRegistrar();
	}

	return Subsystem->GetTargetRegistrarForActor(GetOwner());
}

void URshipActorRegistrationComponent::Register()
{
	UWorld* World = GetWorld();
	if (World && World->WorldType == EWorldType::EditorPreview)
	{
		UE_LOG(LogRshipExec, Verbose, TEXT("Skipping registration for blueprint preview actor: %s"), *targetName);
		return;
	}

	if (TargetData != nullptr)
	{
		UE_LOG(LogRshipExec, Log, TEXT("Register called on already-registered target '%s', re-registering..."), *targetName);
		Unregister();
	}

	URshipSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
	AActor* Parent = GetOwner();
	if (!Subsystem || !Parent)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Register failed: missing subsystem or owner"));
		return;
	}

	FString OutlinerName = Parent->GetName();
#if WITH_EDITOR
	OutlinerName = Parent->GetActorLabel();
#endif

	if (targetName.IsEmpty())
	{
		targetName = OutlinerName;
		UE_LOG(LogRshipExec, Log, TEXT("Target Id not set, defaulting to actor name: %s"), *targetName);
	}

	const FString FullTargetId = Subsystem->GetServiceId() + TEXT(":") + targetName;

	TargetData = new Target(FullTargetId, Subsystem);
	TargetData->SetName(targetName);
	TargetData->SetParentTargetIds(BuildFullParentTargetIds(Subsystem->GetServiceId()));
	TargetData->SetBoundTargetComponent(this);
	Subsystem->RegisterTargetComponent(this);

	if (URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager())
	{
		GroupManager->RegisterTarget(this);
	}

	RebindSiblingContributors();

	UE_LOG(LogRshipExec, Log, TEXT("Component Registered: %s (actions=%d emitters=%d)"), *Parent->GetName(), TargetData->GetActions().Num(), TargetData->GetEmitters().Num());
}

bool URshipActorRegistrationComponent::HasTag(const FString& Tag) const
{
	const FString NormalizedTag = Tag.TrimStartAndEnd().ToLower();
	for (const FString& ExistingTag : Tags)
	{
		if (ExistingTag.TrimStartAndEnd().ToLower() == NormalizedTag)
		{
			return true;
		}
	}
	return false;
}

void URshipActorRegistrationComponent::Unregister()
{
	if (!GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	Subsystem->UnregisterTargetComponent(this);

	if (TargetData)
	{
		TargetData->SetBoundTargetComponent(nullptr);
		delete TargetData;
		TargetData = nullptr;
	}

	if (URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager())
	{
		GroupManager->UnregisterTarget(this);
	}

	UE_LOG(LogRshipExec, Log, TEXT("Target unregistered: %s"), *targetName);
}

void URshipActorRegistrationComponent::SetTargetId(const FString& NewTargetId)
{
	if (NewTargetId.IsEmpty())
	{
		UE_LOG(LogRshipExec, Warning, TEXT("SetTargetId called with empty ID - ignoring"));
		return;
	}

	if (targetName == NewTargetId)
	{
		return;
	}

	const FString OldTargetId = targetName;
	if (TargetData != nullptr)
	{
		Unregister();
	}

	targetName = NewTargetId;
	Register();

	UE_LOG(LogRshipExec, Log, TEXT("Target ID changed: %s -> %s"), *OldTargetId, *NewTargetId);
}

void URshipActorRegistrationComponent::RebindSiblingContributors()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (!Component || Component == this)
		{
			continue;
		}

		if (URshipControllerComponent* Controller = Cast<URshipControllerComponent>(Component))
		{
			Controller->RegisterRshipBindings();
			continue;
		}

		IRshipBindingContributor* Contributor = Cast<IRshipBindingContributor>(Component);
		if (Contributor)
		{
			Contributor->RegisterRshipBindings();
		}
	}
}
