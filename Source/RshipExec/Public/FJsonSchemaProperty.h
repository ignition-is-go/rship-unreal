// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 *
 */
class RSHIPEXEC_API FJsonSchemaProperty
{
private:
public:
	FString type;
	FJsonSchemaProperty(FString type);
	~FJsonSchemaProperty();

	TSharedPtr<FJsonObject> ValueOf();
};
