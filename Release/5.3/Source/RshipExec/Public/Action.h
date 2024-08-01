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
	TSet<AActor*> parents;
	FString id;
	FString name;
	TDoubleLinkedList<RshipSchemaProperty> *props;

public:
	Action(FString id, FString name, UFunction *handler);
	~Action();
	FString GetId();
	FString GetName();
	TSharedPtr<FJsonObject> GetSchema();
	void Take(const TSharedRef<FJsonObject> data);
	void AddParent(AActor *parent);
	void UpdateSchema(UFunction* handler);
};
