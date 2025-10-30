// Fill out your copyright notice in the Description page of Project Settings.

#include "Action.h"
#include "GameFramework/Actor.h"
#include "Misc/OutputDeviceNull.h"
#include "Util.h"
#include "SchemaHelpers.h"
#include "Logs.h"

Action::Action(FString id, FString name, UFunction *function)
{
    this->id = id;
    this->name = name;
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

FString Action::GetName()
{
    return this->name;
}

bool Action::Take(AActor *actor, const TSharedRef<FJsonObject> data)
{
    UE_LOG(LogRshipExec, Verbose, TEXT("Taking Action %s"), *this->id);

    // use our props list to build a string of our arguments

    FString args;
    args.Append(TEXT("\""));
    args.Append(this->functionName);
    args.Append(TEXT("\""));

    const FString argList = BuildArgStringFromJson(this->props, data);
    if (!argList.IsEmpty())
    {
        args.Append(TEXT(" "));
        args.Append(argList);
    }

    // get current output device

    FOutputDeviceNull out = FOutputDeviceNull();

    TCHAR *outStr = args.GetCharArray().GetData();

    UE_LOG(LogRshipExec, Log, TEXT("Calling function with args: %s"), outStr);

    return actor->CallFunctionByNameWithArguments(outStr, out, NULL, true);
}

void Action::UpdateSchema(UFunction *handler)
{
    BuildSchemaPropsFromUFunction(handler, this->props);
}

TSharedPtr<FJsonObject> Action::GetSchema()
{
    return PropsToSchema(&this->props);
}
