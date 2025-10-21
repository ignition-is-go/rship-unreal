#pragma once
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

struct RshipSchemaProperty
{
        FString Name;
        FString Type;
        FProperty* Property;
};

TSharedPtr<FJsonObject> ParseJSON(const FString& JsonString);

TSharedPtr<FJsonObject> ParseJSONObject(const TWeakPtr<FJsonValue>& Value);
TArray<TSharedPtr<FJsonValue>> ParseJSONArray(const TWeakPtr<FJsonValue>& Value);

FString GetJsonString(TSharedPtr<FJsonObject> JsonObject);

FString UnrealToJsonSchemaTypeLookup(FString unrealType);

TSharedPtr<FJsonObject> PropsToSchema(TDoubleLinkedList<RshipSchemaProperty> *props);

