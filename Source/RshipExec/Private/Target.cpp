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

bool Target::HasAction(FString actionId)
{
	return this->actions.Contains(actionId);
}

TMap<FString, Action*> Target::GetActions() {
	return this->actions;
}

FString Target::GetId() {
	return this->id;
}

void Target::TakeAction(FString actionId, TSharedPtr<FJsonObject> data) {
	if (!this->actions.Contains(actionId)) {
		UE_LOG(LogTemp, Warning, TEXT("Action not found: [%s]"), *actionId);
		return;
	}

	this->actions[actionId]->Take(data);
	data.Reset();
}