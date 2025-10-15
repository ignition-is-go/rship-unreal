// Fill out your copyright notice in the Description page of Project Settings.

#include "Action.h"
#include "GameFramework/Actor.h"
#include "Misc/OutputDeviceNull.h"
#include "Util.h"

Action::Action(FString id, FString name, UFunction *function)
{
    this->id = id;
    this->name = name;
    this->props = new TDoubleLinkedList<RshipSchemaProperty>();
    this->functionName = function->GetName();
    this->UpdateSchema(function);
}

Action::~Action()
{
}

FString Action::GetId()
{
    return this->id;
}

FString Action::GetName() {
    return this->name;
}

void Action::Take(AActor* actor, const TSharedRef<FJsonObject> data)
{
    // UE_LOG(LogTemp, Warning, TEXT("Taking Action %s"), *this->id);

    // use our props list to build a string of our arguments

    FString Args = this->functionName;
    Args.Reserve(Args.Len() + this->props->Num() * 8);

    for (const RshipSchemaProperty& Prop : *this->props)
    {
        const TSharedPtr<FJsonValue> FieldValue = data->TryGetField(Prop.Name);
        const FString Piece = FieldValue.IsValid() ? FieldValue->AsString() : FString();
        Args.Append(TEXT(" \""));
        Args.Append(Piece);
        Args.Append(TEXT("\""));
    }

    FOutputDeviceNull Out;
    UE_LOG(LogTemp, Warning, TEXT("Calling function with args: %s"), *Args);

    actor->CallFunctionByNameWithArguments(*Args, Out, nullptr, true);
}

void Action::UpdateSchema(UFunction *handler)
{
    this->props->Empty();

    for (TFieldIterator<FProperty> It(handler); It && (It->PropertyFlags & (CPF_Parm)) == CPF_Parm; ++It)
    {
        FProperty *Property = *It;
        FString PropertyName = Property->GetName();
        FName PropertyType = Property->GetClass()->GetFName();

        RshipSchemaProperty prop = RshipSchemaProperty({PropertyName, PropertyType.ToString()});

        UE_LOG(LogTemp, Warning, TEXT("Property: %s, Type: %s"), *PropertyName, *PropertyType.ToString());

        this->props->AddTail(prop);
    }
}

TSharedPtr<FJsonObject> Action::GetSchema()
{
    return PropsToSchema(this->props);
}
