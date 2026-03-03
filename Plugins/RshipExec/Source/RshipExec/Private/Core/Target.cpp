#include "Core/Target.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "RshipSubsystem.h"
#include "RshipActorRegistrationComponent.h"
#include "Async/Async.h"

Target::Target(FString id, URshipSubsystem* InSubsystem)
{
	this->id = id;
	this->name = id;
	BoundSubsystem = InSubsystem;

	if (URshipSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->RegisterManagedTarget(this);
	}
}

Target::~Target()
{
	if (URshipSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->UnregisterManagedTarget(this);
	}
}

void Target::AddAction(const FRshipActionProxy& action)
{
	if (action.IsValid())
	{
		actions.Add(action.Id, action);
		if (URshipSubsystem* Subsystem = BoundSubsystem.Get())
		{
			Subsystem->OnManagedTargetChanged(this);
		}
	}
}

void Target::AddEmitter(const FRshipEmitterProxy& emitter)
{
	if (emitter.IsValid())
	{
		emitters.Add(emitter.Id, emitter);
		if (URshipSubsystem* Subsystem = BoundSubsystem.Get())
		{
			Subsystem->OnManagedTargetChanged(this);
		}
	}
}

FString Target::GetId() const
{
	return id;
}

void Target::SetId(const FString& InId)
{
	if (id == InId || InId.IsEmpty())
	{
		return;
	}

	id = InId;
	if (URshipSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnManagedTargetChanged(this);
	}
}

const FString& Target::GetName() const
{
	return name;
}

void Target::SetName(const FString& InName)
{
	name = InName;
	if (URshipSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnManagedTargetChanged(this);
	}
}

const TArray<FString>& Target::GetParentTargetIds() const
{
	return parentTargetIds;
}

void Target::SetParentTargetIds(const TArray<FString>& InParentTargetIds)
{
	parentTargetIds = InParentTargetIds;
	if (URshipSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnManagedTargetChanged(this);
	}
}

const TMap<FString, FRshipActionProxy>& Target::GetActions() const
{
	return actions;
}

const TMap<FString, FRshipEmitterProxy>& Target::GetEmitters() const
{
	return emitters;
}

void Target::SetBoundTargetComponent(URshipActorRegistrationComponent* InTargetComponent)
{
	BoundTargetComponent = InTargetComponent;
}

URshipActorRegistrationComponent* Target::GetBoundTargetComponent() const
{
	return BoundTargetComponent.Get();
}

URshipSubsystem* Target::GetBoundSubsystem() const
{
	return BoundSubsystem.Get();
}

bool Target::TakeAction(AActor* actor, FString actionId, const TSharedRef<FJsonObject> data)
{
	FRshipActionProxy* ActionPtr = actions.Find(actionId);
	if (!ActionPtr)
	{
		UE_LOG(LogRshipExec, Error, TEXT("Action not found: [%s] on target [%s]"), *actionId, *id);
		return false;
	}

	const FRshipActionProxy& TakenAction = *ActionPtr;
	UObject* OwnerObject = TakenAction.GetOwnerObject();
	if (!IsValid(OwnerObject))
	{
		UE_LOG(LogRshipExec, Error, TEXT("Action '%s' failed: owner is invalid or destroyed."), *actionId);

		// Targeted refresh: re-register only this component to rebuild actions.
		if (URshipActorRegistrationComponent* TargetComponent = BoundTargetComponent.Get())
		{
			if (IsValid(TargetComponent))
			{
				TargetComponent->Register();
				ActionPtr = actions.Find(actionId);
			}
		}

		if (!ActionPtr || !IsValid(ActionPtr->GetOwnerObject()))
		{
			return false;
		}
	}

	const bool bTaken = ActionPtr->Take(actor, data);
	if (!bTaken)
	{
		UE_LOG(LogRshipExec, Error, TEXT("Action '%s' failed on target '%s'."), *actionId, *id);
	}

	if (GEngine)
	{
		TWeakObjectPtr<URshipActorRegistrationComponent> WeakTargetComponent(BoundTargetComponent);

		auto DispatchAfterTake = [WeakTargetComponent]()
		{
			if (!GEngine)
			{
				return;
			}

			URshipActorRegistrationComponent* TargetComponent = WeakTargetComponent.Get();
			if (!IsValid(TargetComponent))
			{
				return;
			}

			URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
			if (!Subsystem)
			{
				return;
			}

			Subsystem->QueueOnDataReceived(TargetComponent);
		};

		if (IsInGameThread())
		{
			DispatchAfterTake();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, MoveTemp(DispatchAfterTake));
		}
	}

	return bTaken;
}
