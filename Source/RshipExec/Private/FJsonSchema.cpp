// Fill out your copyright notice in the Description page of Project Settings.

#include "FJsonSchema.h"

FJsonSchema::FJsonSchema()
{
    this->root = MakeShareable(new FJsonSchemaObject());
}

FJsonSchema::~FJsonSchema()
{
}

FJsonSchemaProperty* FJsonSchema::String()
{
    return new  FJsonSchemaProperty( FString("string"));
}

FJsonSchemaProperty* FJsonSchema::Number()
{
    return new FJsonSchemaProperty( FString("number"));
}

FJsonSchemaProperty* FJsonSchema::Boolean()
{
    return new FJsonSchemaProperty( FString("boolean"));
}

FJsonSchemaObject* FJsonSchema::Object()
{
    return new FJsonSchemaObject();
}

TSharedPtr<FJsonObject> FJsonSchema::ValueOf()
{
    TSharedPtr<FJsonObject> obj = this->root->ValueOf();
    obj->SetStringField("$schema", "http://json-schema.org/draft-07/schema#");
    return obj;
}

FJsonSchemaObject* FJsonSchema::Prop(FString name, FJsonSchemaProperty *prop)
{
    return this->root->Prop(name, prop);
}

FJsonSchemaObject* FJsonSchema::Prop(FString name, FJsonSchemaObject *prop)
{
    return this->root->Prop(name, prop);
}

void FJsonSchema::Empty()
{
	this->root->Clear();
}