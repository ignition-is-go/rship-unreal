// Fill out your copyright notice in the Description page of Project Settings.

#include "Action.h"
#include "GameFramework/Actor.h"
#include "Misc/OutputDeviceNull.h"
#include "Util.h"
#include "SchemaHelpers.h"
#include "Logs.h"

Action::Action(FString id, FString name, UFunction *function, UObject *owner)
{
    this->id = id;
    this->name = name;
    this->functionName = function->GetName();
    this->owner = owner;
    this->UpdateSchema(function);
	this->property = nullptr;
}

Action::Action(FString id, FString name, FProperty* property, UObject* owner)
{
    this->id = id;
    this->name = name;
    this->functionName = property->GetName();
    this->owner = owner;
    this->property = property;
    this->UpdateSchema(property);
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

    if (this->property != nullptr) {
        // For ImportText_Direct, don't quote strings - it expects raw values
        const FString argList = BuildArgStringFromJson(this->props, data, false);
		UE_LOG(LogRshipExec, Verbose, TEXT("Setting property %s with args: %s"), *this->functionName, *argList);

        void* propAddress = this->property->ContainerPtrToValuePtr<void>(this->owner);

        const TCHAR* result = this->property->ImportText_Direct(*argList, propAddress, this->owner, 0);
        int32 resultLength = result ? FCString::Strlen(result) : 0;


        if (resultLength == 0) {
            return true;
        }

        UE_LOG(LogRshipExec, Warning, TEXT("Import Result: %s"), result);
        return false;

    }
    else {
        // For CallFunctionByNameWithArguments, quote strings for proper parsing
        const FString argList = BuildArgStringFromJson(this->props, data, true);

        FString args;
        args.Append(TEXT("\""));
        args.Append(this->functionName);
        args.Append(TEXT("\""));

        if (!argList.IsEmpty())
        {
            args.Append(TEXT(" "));
            args.Append(argList);
        }

        FOutputDeviceNull out = FOutputDeviceNull();

        TCHAR *outStr = args.GetCharArray().GetData();

        UE_LOG(LogRshipExec, Log, TEXT("Calling function with args: %s"), outStr);

        return this->owner->CallFunctionByNameWithArguments(outStr, *GLog, NULL, true);
    }

}

void Action::UpdateSchema(UFunction *handler)
{
    BuildSchemaPropsFromUFunction(handler, this->props);
}

void Action::UpdateSchema(FProperty *inProp)
{
    BuildSchemaPropsFromFProperty(inProp, this->props);
}

TSharedPtr<FJsonObject> Action::GetSchema()
{
    return PropsToSchema(&this->props);
}
