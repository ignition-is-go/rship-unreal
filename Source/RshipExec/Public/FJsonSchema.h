// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FJsonSchemaObject.h"
#include "FJsonSchemaProperty.h"

/**
 *
 */
class RSHIPEXEC_API FJsonSchema
{

private:
	TSharedPtr<FJsonSchemaObject> root;

public:
	FJsonSchema();
	~FJsonSchema();

	static FJsonSchemaProperty* String();
	static FJsonSchemaProperty* Number();
	static FJsonSchemaProperty* Boolean();
	static FJsonSchemaObject* Object();

	TSharedPtr<FJsonObject> ValueOf();
	FJsonSchemaObject* Prop(FString name, FJsonSchemaProperty *prop);
	FJsonSchemaObject* Prop(FString name, FJsonSchemaObject *prop);

	void Empty();
};
