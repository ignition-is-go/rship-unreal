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
	TDoubleLinkedList<RshipSchemaProperty> *props;

public:
	Action(FString id, FString name, UFunction *handler);
	~Action();
	FString GetId();
        FString GetName();
        FString GetFunctionName() const { return functionName; }
        TSharedPtr<FJsonObject> GetSchema();
        void Take(AActor* actor, const TSharedRef<FJsonObject> data);
        void UpdateSchema(UFunction* handler);
        TDoubleLinkedList<RshipSchemaProperty>* GetProps() const { return props; }
};
