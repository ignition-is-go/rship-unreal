// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Util.h"

/**
 * 
 */
class RSHIPEXEC_API EmitterContainer
{

private:
	FString id;
	TDoubleLinkedList<RshipSchemaProperty> *props;

public:
	EmitterContainer(FString id, FMulticastInlineDelegateProperty* Emitter);
	~EmitterContainer();

	void UpdateSchema(FMulticastInlineDelegateProperty* Emitter);

	TSharedPtr<FJsonObject> GetSchema();

	TDoubleLinkedList<RshipSchemaProperty> *GetProps();

	FString GetId();

};
