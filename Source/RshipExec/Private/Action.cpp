// Fill out your copyright notice in the Description page of Project Settings.


#include "Action.h"
#include "GameFramework/Actor.h"


Action::Action(FString id, AActor *actor, UFunction *function)
{
	this->parent = actor;
	this->handler = function;
	this->id = id;
	this->props = new TDoubleLinkedList<RshipActionProperty>();
    this->schema = MakeShareable(new FJsonObject);

    for (TFieldIterator<FProperty> It(function); It; ++It)
    {
        FProperty* Property = *It;
        FString PropertyName = Property->GetName();
        FName PropertyType = Property->GetClass()->GetFName();

        RshipActionProperty prop = RshipActionProperty({ PropertyName, Property->GetClass()->GetFName().ToString() });
        this->schema->SetStringField(PropertyName, PropertyType.ToString());

        props->AddTail(prop);
    }
}

Action::~Action()
{
}


FString Action::GetId() {
	return this->id;
}

void Action::Take() {
    UE_LOG(LogTemp, Warning, TEXT("Taking Action %s"), *this->id);
    this->parent->ProcessEvent(this->handler, nullptr);
}

TSharedPtr<FJsonObject> Action::GetSchema() {
    return this->schema;
}