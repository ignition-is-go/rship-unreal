// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Action.h"
/**
 * 
 */
class RSHIPEXEC_API Target
{

private: 
	FString id;
	TMap<FString, Action*> actions;

public:
	Target(FString id);
	~Target();


	void AddAction(Action* action);

	FString GetId();

	TMap<FString, Action*> GetActions();

	void TakeAction(FString actionId);
	
};
