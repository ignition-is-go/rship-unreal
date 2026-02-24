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
	FProperty *property;
	FString id;
	FString name;
	UObject* owner;
	TDoubleLinkedList<SchemaNode> props;

public:
	Action(FString id, FString name, UFunction *handler, UObject *owner);
	Action(FString id, FString name, FProperty* property, UObject* owner);
	~Action();
	FString GetId();
	FString GetName();
	UObject* GetOwnerObject() const;
	TSharedPtr<FJsonObject> GetSchema();
	bool Take(AActor *actor, const TSharedRef<FJsonObject> data);
	void UpdateSchema(UFunction *handler);
	void UpdateSchema(FProperty *property);
};
