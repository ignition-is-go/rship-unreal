#pragma once
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

struct SchemaNode
{
	FString Name;
	FString Type;
	// For StructProperty types, Children describes nested fields.
	TArray<SchemaNode> Children;
};

TSharedPtr<FJsonObject> ParseJSON(const FString &JsonString);

TSharedPtr<FJsonObject> ParseJSONObject(const TWeakPtr<FJsonValue> &Value);
TArray<TSharedPtr<FJsonValue>> ParseJSONArray(const TWeakPtr<FJsonValue> &Value);

FString GetJsonString(TSharedPtr<FJsonObject> JsonObject);

FString UnrealToJsonSchemaTypeLookup(FString unrealType);

TSharedPtr<FJsonObject> PropsToSchema(TDoubleLinkedList<SchemaNode> *props);
