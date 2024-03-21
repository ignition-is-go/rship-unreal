// Fill out your copyright notice in the Description page of Project Settings.

#include "FJsonSchemaObject.h"
#include "FJsonSchemaProperty.h"

FJsonSchemaObject::FJsonSchemaObject()
{
    this->objectProperties = TMap<FString, FJsonSchemaObject*>();
    this->properties = TMap<FString, FJsonSchemaProperty*>();
}

FJsonSchemaObject::~FJsonSchemaObject()
{
}

FJsonSchemaObject* FJsonSchemaObject::Prop(FString name, FJsonSchemaProperty *prop)
{
    if (this->objectProperties.Contains(name) || this->properties.Contains(name))
    {
        UE_LOG(LogTemp, Warning, TEXT("Property %s already exists"), *name);
    }
    this->properties.Add(name, prop);

    return this;
}

FJsonSchemaObject* FJsonSchemaObject::Prop(FString name, FJsonSchemaObject *prop)
{

    if (this->objectProperties.Contains(name) || this->properties.Contains(name))
    {
        UE_LOG(LogTemp, Warning, TEXT("Property %s already exists"), *name);
    }

    

    this->objectProperties.Add(name, prop);

    return this;
}

TSharedPtr<FJsonObject> FJsonSchemaObject::ValueOf()
{
    TSharedPtr<FJsonObject> obj = MakeShareable(new FJsonObject());

    obj->SetStringField("type", "object");

    TSharedPtr<FJsonObject> props = MakeShareable(new FJsonObject());

    for (const TPair<FString, FJsonSchemaObject*>& prop : this->objectProperties)
    {
        props->SetObjectField(prop.Key, prop.Value->ValueOf());
    }


    for (const TPair<FString, FJsonSchemaProperty*>& objProp : this->properties)
    {
        props->SetObjectField(objProp.Key, objProp.Value->ValueOf());
    }

    obj->SetObjectField("properties", props);

    return obj;
}

void FJsonSchemaObject::Clear()
{
	this->objectProperties.Empty();
	this->properties.Empty();
}