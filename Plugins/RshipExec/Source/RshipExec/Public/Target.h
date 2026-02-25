// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Action.h"
#include "EmitterContainer.h"

class URshipTargetComponent;

class RSHIPEXEC_API Target
{
private:
	FString id;
	TMap<FString, Action*> actions;
	TMap<FString, EmitterContainer*> emitters;
	TWeakObjectPtr<URshipTargetComponent> BoundTargetComponent;

public:
	Target(FString id);
	~Target();

	void AddAction(Action* action);
	void AddEmitter(EmitterContainer* emitter);

	FString GetId() const;
	const TMap<FString, Action*>& GetActions() const;
	const TMap<FString, EmitterContainer*>& GetEmitters() const;

	void SetBoundTargetComponent(URshipTargetComponent* InTargetComponent);
	URshipTargetComponent* GetBoundTargetComponent() const;

	bool TakeAction(AActor* actor, FString actionId, const TSharedRef<FJsonObject> data);
};
