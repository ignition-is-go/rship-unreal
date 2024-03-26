// Fill out your copyright notice in the Description page of Project Settings.


#include "EmitterContainer.h"
#include "Util.h"


EmitterContainer::EmitterContainer(FString id, FMulticastInlineDelegateProperty* Emitter)
{

	this->id = id;
    this->props = new TDoubleLinkedList<RshipSchemaProperty>();
	this->UpdateSchema(Emitter);

}

EmitterContainer::~EmitterContainer()
{
}

void EmitterContainer::UpdateSchema(FMulticastInlineDelegateProperty* Emitter)
{
    this->props->Empty();

    for (TFieldIterator<FProperty> PropIt(Emitter->SignatureFunction); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        FString PropertyName = Property->GetName();
        FName PropertyType = Property->GetClass()->GetFName();
        UE_LOG(LogTemp, Warning, TEXT("Emitter Property: %s, Type: %s"), *PropertyName, *PropertyType.ToString());

        RshipSchemaProperty prop = RshipSchemaProperty({ PropertyName, PropertyType.ToString() });

        this->props->AddTail(prop);

        UE_LOG(LogTemp, Warning, TEXT("Property: %s, Type: %s"), *PropertyName, *PropertyType.ToString());
    }

}

TSharedPtr<FJsonObject> EmitterContainer::GetSchema()
{
    return PropsToSchema(this->props);
}

TDoubleLinkedList<RshipSchemaProperty>* EmitterContainer::GetProps()
{
    return this->props;
}

FString EmitterContainer::GetId()
{
    return this->id;
}
