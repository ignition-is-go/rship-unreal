// Fill out your copyright notice in the Description page of Project Settings.

#include "Target.h"
#include "Logs.h"

Target::Target(FString id)
{
	this->id = id;
}

Target::~Target()
{
}

void Target::AddAction(Action *action)
{

	this->actions.Add(action->GetId(), action);
}

void Target::AddEmitter(EmitterContainer *emitter)
{
	this->emitters.Add(emitter->GetId(), emitter);
}

TMap<FString, Action *> Target::GetActions()
{
	return this->actions;
}

TMap<FString, EmitterContainer *> Target::GetEmitters()
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
		UE_LOG(LogRshipExec, Error, TEXT("Action not found: [%s] on target [%s]"), *actionId, *this->id);
		return false;
	}

	return this->actions[actionId]->Take(actor, data);
}