// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "Util.h"

/**
 * 
 */
class RSHIPEXEC_API EmitterContainer
{

private:
	FString id;
	FString name;
	TDoubleLinkedList<SchemaNode> props;

public:
	EmitterContainer(FString id, FString name, FMulticastDelegateProperty* Emitter);
	~EmitterContainer();

	void UpdateSchema(FMulticastDelegateProperty* Emitter);

	TSharedPtr<FJsonObject> GetSchema();

	TDoubleLinkedList<SchemaNode> *GetProps();

	FString GetId();
	FString GetName();

};
