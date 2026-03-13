// Fill out your copyright notice in the Description page of Project Settings.


#include "EmitterContainer.h"
#include "Util.h"
#include "SchemaHelpers.h"


EmitterContainer::EmitterContainer(FString id, FString name,  FMulticastDelegateProperty* Emitter)
{

	this->id = id;
    this->name = name;
    this->UpdateSchema(Emitter);

}

EmitterContainer::~EmitterContainer()
{
}

void EmitterContainer::UpdateSchema(FMulticastDelegateProperty* Emitter)
{
    this->props.Empty();
    // Build schema props from the delegate signature function
    BuildSchemaPropsFromUFunction(Emitter->SignatureFunction, this->props);

}

TSharedPtr<FJsonObject> EmitterContainer::GetSchema()
{
    return PropsToSchema(&this->props);
}

TDoubleLinkedList<SchemaNode>* EmitterContainer::GetProps()
{
    return &this->props;
}

FString EmitterContainer::GetId()
{
    return this->id;
}

FString EmitterContainer::GetName()
{
    return this->name;
}
