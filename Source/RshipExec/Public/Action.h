// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Util.h"

/**
 *
 */
class RSHIPEXEC_API Action
{

private:
	FString functionName;
	FString id;
	FString name;
	TDoubleLinkedList<SchemaNode> props;

public:
	Action(FString id, FString name, UFunction *handler);
	~Action();
	FString GetId();
	FString GetName();
	TSharedPtr<FJsonObject> GetSchema();
	bool Take(AActor *actor, const TSharedRef<FJsonObject> data);
	void UpdateSchema(UFunction *handler);
};
