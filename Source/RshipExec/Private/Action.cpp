// Fill out your copyright notice in the Description page of Project Settings.

#include "Action.h"
#include "GameFramework/Actor.h"
#include "Misc/OutputDeviceNull.h"
#include "Util.h"

// Helper to recursively collect child properties for UScriptStructs
static void BuildChildrenForStruct(const UScriptStruct *InStruct, RshipSchemaProperty &OutProp)
{
    for (TFieldIterator<FProperty> It(InStruct); It; ++It)
    {
        const FProperty *Field = *It;
        RshipSchemaProperty Child;
        Child.Name = Field->GetName();
        Child.Type = Field->GetClass()->GetFName().ToString();
        if (const FStructProperty *NestedStruct = CastField<FStructProperty>(Field))
        {
            if (const UScriptStruct *NestedScript = NestedStruct->Struct)
            {
                BuildChildrenForStruct(NestedScript, Child);
            }
        }
        OutProp.Children.Add(Child);
    }
}

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

FString Action::GetName()
{
    return this->name;
}

void Action::Take(AActor *actor, const TSharedRef<FJsonObject> data)
{
    // UE_LOG(LogTemp, Warning, TEXT("Taking Action %s"), *this->id);

    // use our props list to build a string of our arguments

    FString *args = new FString();

    args->Append(this->functionName);

    auto SerializeValueForArg = [&](const RshipSchemaProperty &schemaProp, const TSharedPtr<FJsonValue> &jsonVal, auto &&SerializeRef) -> FString
    {
        // Helper lambdas
        auto IsStringLike = [](const FString &UnrealType)
        {
            return UnrealType == TEXT("StrProperty") || UnrealType == TEXT("TextProperty") || UnrealType == TEXT("NameProperty");
        };

        // For struct/object, build Unreal ImportText style value: (FieldA=...,FieldB=...)
        if (schemaProp.Type == TEXT("StructProperty"))
        {
            const TSharedPtr<FJsonObject> asObj = (jsonVal.IsValid() && jsonVal->Type == EJson::Object) ? jsonVal->AsObject() : MakeShareable(new FJsonObject());
            TArray<FString> pairs;
            pairs.Reserve(schemaProp.Children.Num());
            for (const RshipSchemaProperty &child : schemaProp.Children)
            {
                const TSharedPtr<FJsonValue> childVal = asObj->TryGetField(child.Name);
                const FString childStr = SerializeRef(child, childVal, SerializeRef);
                if (!childStr.IsEmpty())
                {
                    pairs.Add(FString::Printf(TEXT("%s=%s"), *child.Name, *childStr));
                }
            }
            return FString::Printf(TEXT("(%s)"), *FString::Join(pairs, TEXT(",")));
        }

        // Primitive: coerce to string; quote only if string-like
        if (!jsonVal.IsValid())
        {
            return FString("");
        }
        FString out;
        switch (jsonVal->Type)
        {
        case EJson::String:
            out = jsonVal->AsString();
            break;
        case EJson::Number:
            out = FString::SanitizeFloat(jsonVal->AsNumber());
            break;
        case EJson::Boolean:
            out = jsonVal->AsBool() ? TEXT("true") : TEXT("false");
            break;
        case EJson::Object:
        case EJson::Array:
            // Fallback to JSON text for non-struct complex types
            out = GetJsonString(jsonVal->AsObject());
            break;
        default:
            out = FString("");
            break;
        }

        if (IsStringLike(schemaProp.Type))
        {
            // Escape embedded quotes
            out.ReplaceInline(TEXT("\""), TEXT("\\\""));
            return FString::Printf(TEXT("\"%s\""), *out);
        }
        return out;
    };

    for (RshipSchemaProperty const &prop : *this->props)
    {
        const TSharedPtr<FJsonValue> piece = data->TryGetField(prop.Name);
        if (!piece.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("Missing field: %s"), *prop.Name);
            continue;
        }
        const FString valueStr = SerializeValueForArg(prop, piece, SerializeValueForArg);
        args->Append(" ");
        args->Append(valueStr);
    }

    // get current output device

    FOutputDeviceNull out = FOutputDeviceNull();

    TCHAR *outStr = args->GetCharArray().GetData();

    // UE_LOG(LogTemp, Warning, TEXT("Calling function with args: %s"), outStr);

    actor->CallFunctionByNameWithArguments(outStr, out, NULL, true);

    delete args;
}

void Action::UpdateSchema(UFunction *handler)
{
    this->props->Empty();

    for (TFieldIterator<FProperty> It(handler); It && (It->PropertyFlags & (CPF_Parm)) == CPF_Parm; ++It)
    {
        FProperty *Property = *It;
        FString PropertyName = Property->GetName();
        FName PropertyType = Property->GetClass()->GetFName();

        FFieldClass *propertyClass = Property->GetClass();

        FString displayName = propertyClass->GetDisplayNameText().ToString();

        UE_LOG(LogTemp, Verbose, TEXT("Property: %s, Class: %s, DisplayName: %s"), *PropertyName, *PropertyType.ToString(), *displayName);

        RshipSchemaProperty prop;
        prop.Name = PropertyName;
        prop.Type = PropertyType.ToString();

        // If this is a struct, collect its fields recursively
        if (const FStructProperty *StructProp = CastField<FStructProperty>(Property))
        {
            if (const UScriptStruct *ScriptStruct = StructProp->Struct)
            {
                BuildChildrenForStruct(ScriptStruct, prop);
            }
        }

        this->props->AddTail(prop);
    }
}

TSharedPtr<FJsonObject> Action::GetSchema()
{
    return PropsToSchema(this->props);
}
