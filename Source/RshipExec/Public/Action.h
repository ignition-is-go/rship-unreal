// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

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
	UFunction *handler;
	AActor *parent;
	FString id;
	TDoubleLinkedList<RshipActionProperty>* props;
	TSharedPtr<FJsonObject> schema;

public:
	Action(FString id, AActor *parent, UFunction *handler);
	~Action();
	FString GetId();
	TSharedPtr<FJsonObject> GetSchema();
	void Take();
};
