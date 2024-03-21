// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FJsonSchema.h"

struct RshipActionProperty
{
	FString Name;
	FString Type;
};

/**
 *
 */
class RSHIPEXEC_API Action
{

private:
	FString functionName;
	TSet<AActor*> parents;
	FString id;
	TDoubleLinkedList<RshipActionProperty> *props;
	TSharedPtr<FJsonSchema> schema;

public:
	Action(FString id, UFunction *handler);
	~Action();
	FString GetId();
	TSharedPtr<FJsonObject> GetSchema();
	void Take(TSharedPtr<FJsonObject> data);
	void AddParent(AActor *parent);
	void UpdateSchema(UFunction* handler);
};
