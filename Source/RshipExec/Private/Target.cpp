// Fill out your copyright notice in the Description page of Project Settings.


#include "Target.h"

Target::Target(FString id)
{
	this->id = id;

}

Target::~Target()
{
}

void Target::AddAction(Action* action) {

	this->actions.Add(action->GetId(), action);
}


void Target::AddEmitter(EmitterContainer* emitter)
{
	this->emitters.Add(emitter->GetId(), emitter);
}


const TMap<FString, Action*>& Target::GetActions() const {
        return this->actions;
}

const TMap<FString, EmitterContainer*>& Target::GetEmitters() const
{
        return this->emitters;
}

FString Target::GetId() {
	return this->id;
}

void Target::TakeAction(AActor* actor, FString actionId, const TSharedRef<FJsonObject> data) {
	if (!this->actions.Contains(actionId)) {
		UE_LOG(LogTemp, Warning, TEXT("Action not found: [%s]"), *actionId);
		return;
	}

	this->actions[actionId]->Take(actor, data);
}