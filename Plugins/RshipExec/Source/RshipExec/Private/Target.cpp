#include "Target.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "Async/Async.h"

Target::Target(FString id)
{
	this->id = id;
}

Target::~Target()
{
}

void Target::AddAction(Action* action)
{
	actions.Add(action->GetId(), action);
}

void Target::AddEmitter(EmitterContainer* emitter)
{
	emitters.Add(emitter->GetId(), emitter);
}

FString Target::GetId() const
{
	return id;
}

const TMap<FString, Action*>& Target::GetActions() const
{
	return actions;
}

const TMap<FString, EmitterContainer*>& Target::GetEmitters() const
{
	return emitters;
}

void Target::SetBoundTargetComponent(URshipTargetComponent* InTargetComponent)
{
	BoundTargetComponent = InTargetComponent;
}

URshipTargetComponent* Target::GetBoundTargetComponent() const
{
	return BoundTargetComponent.Get();
}

bool Target::TakeAction(AActor* actor, FString actionId, const TSharedRef<FJsonObject> data)
{
	Action** ActionPtr = actions.Find(actionId);
	if (!ActionPtr || !(*ActionPtr))
	{
		UE_LOG(LogRshipExec, Error, TEXT("Action not found: [%s] on target [%s]"), *actionId, *id);
		return false;
	}

	Action* TakenAction = *ActionPtr;
	const bool bTaken = TakenAction->Take(actor, data);

	if (GEngine)
	{
		TWeakObjectPtr<URshipTargetComponent> WeakTargetComponent(BoundTargetComponent);
		const FString ActionName = TakenAction->GetName();
		TWeakObjectPtr<UObject> WeakActionOwner(TakenAction->GetOwnerObject());

		auto DispatchAfterTake = [WeakTargetComponent, WeakActionOwner, ActionName]()
		{
			if (!GEngine)
			{
				return;
			}

			URshipTargetComponent* TargetComponent = WeakTargetComponent.Get();
			if (!IsValid(TargetComponent))
			{
				return;
			}

			URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
			if (!Subsystem)
			{
				return;
			}

			TargetComponent->HandleAfterTake(ActionName, WeakActionOwner.Get());
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
