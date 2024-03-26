// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Action.h"
#include "EmitterContainer.h"
/**
 * 
 */
class RSHIPEXEC_API Target
{

private: 
	FString id;
	TMap<FString, Action*> actions;
	TMap<FString, EmitterContainer*> emitters;

public:
	Target(FString id);
	~Target();


	void AddAction(Action* action);
	void AddEmitter(EmitterContainer* emitter);

	FString GetId();

	TMap<FString, Action*> GetActions();
	TMap<FString, EmitterContainer*> GetEmitters();

	void TakeAction(FString actionId, const TSharedRef<FJsonObject> data);
};
