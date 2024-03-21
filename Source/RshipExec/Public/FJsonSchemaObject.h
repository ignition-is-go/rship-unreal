// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FJsonSchemaProperty.h"

/**
 *
 */
class RSHIPEXEC_API FJsonSchemaObject
{

private:
	TMap<FString, FJsonSchemaProperty*> properties;
	TMap<FString, FJsonSchemaObject*> objectProperties;

public:
	FJsonSchemaObject();
	~FJsonSchemaObject();

	FJsonSchemaObject* Prop(FString name, FJsonSchemaProperty *prop);
	FJsonSchemaObject* Prop(FString name, FJsonSchemaObject *prop);

	TSharedPtr<FJsonObject> ValueOf();
	void Clear();
};
