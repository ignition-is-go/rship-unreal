// Fill out your copyright notice in the Description page of Project Settings.

#include "FJsonSchemaProperty.h"

FJsonSchemaProperty::FJsonSchemaProperty(FString type)
{
    this->type = type;
}

FJsonSchemaProperty::~FJsonSchemaProperty()
{
}

TSharedPtr<FJsonObject> FJsonSchemaProperty::ValueOf()
{
    TSharedPtr<FJsonObject> obj = MakeShareable(new FJsonObject());
    obj->SetStringField("type", this->type);
    return obj;
}