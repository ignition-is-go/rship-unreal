// Fill out your copyright notice in the Description page of Project Settings.

#include "Action.h"
#include "GameFramework/Actor.h"
#include "Misc/OutputDeviceNull.h"
#include "Util.h"

Action::Action(FString id, UFunction *function)
{
    this->id = id;
    this->props = new TDoubleLinkedList<RshipSchemaProperty>();
    this->functionName = function->GetName();
    this->parents = TSet<AActor *>();
    this->UpdateSchema(function);
}

Action::~Action()
{
}

FString Action::GetId()
{
    return this->id;
}

void Action::Take(const TSharedRef<FJsonObject> data)
{
    // UE_LOG(LogTemp, Warning, TEXT("Taking Action %s"), *this->id);

    // use our props list to build a string of our arguments

    FString *args = new FString();

    args->Append(this->functionName);

    for (const auto prop : *this->props)
    {
        auto piece = data->TryGetField(prop.Name)->AsString();
        args->Append(" \"");
        args->Append(piece);
        args->Append("\"");
    }

    // get current output device

    FOutputDeviceNull out = FOutputDeviceNull();

    TCHAR *outStr = args->GetCharArray().GetData();

    // UE_LOG(LogTemp, Warning, TEXT("Calling function with args: %s"), outStr);

    TSet<AActor *> *safeParents = new TSet<AActor *>;

    for (auto parent : this->parents)
    {
        if (parent->IsValidLowLevelFast())
        {
            safeParents->Add(parent);
        }
    }

    this->parents = *safeParents;

    for (auto parent : this->parents)
    {
        if (parent)
        {
            try
            {
                parent->CallFunctionByNameWithArguments(outStr, out, NULL, true);
            }
            catch (const std::exception &e)
            {
                UE_LOG(LogTemp, Warning, TEXT("Caught exception: %s"), e.what());
            }
        }
    }

    delete args;
}

void Action::AddParent(AActor *parent)
{
    this->parents.Add(parent);
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
