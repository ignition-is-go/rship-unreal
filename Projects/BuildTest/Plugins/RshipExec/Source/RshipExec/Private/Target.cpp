// Fill out your copyright notice in the Description page of Project Settings.

#include "Target.h"
#include "Logs.h"

Target::Target(FString id)
{
	this->id = id;
}

Target::~Target()
{
	for (auto& Entry : this->actions)
	{
		delete Entry.Value;
	}

	for (auto& Entry : this->emitters)
	{
		delete Entry.Value;
	}
}

void Target::AddAction(Action *action)
{
	if (!action)
	{
		return;
	}

	const FString ActionId = action->GetId();
	if (Action* Existing = this->actions.FindRef(ActionId))
	{
		delete Existing;
		this->actions.Remove(ActionId);
	}

	this->actions.Add(action->GetId(), action);
}

void Target::AddEmitter(EmitterContainer *emitter)
{
	if (!emitter)
	{
		return;
	}

	const FString EmitterId = emitter->GetId();
	if (EmitterContainer* Existing = this->emitters.FindRef(EmitterId))
	{
		delete Existing;
		this->emitters.Remove(EmitterId);
	}

	this->emitters.Add(emitter->GetId(), emitter);
}

const TMap<FString, Action*>& Target::GetActions() const
{
	return this->actions;
}

const TMap<FString, EmitterContainer*>& Target::GetEmitters() const
{
	return this->emitters;
}

FString Target::GetId()
{
	return this->id;
}

bool Target::TakeAction(AActor *actor, FString actionId, const TSharedRef<FJsonObject> data)
{
	if (!this->actions.Contains(actionId))
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Action not found: [%s]"), *actionId);
		return 1;
	}

	return this->actions[actionId]->Take(actor, data);
}
