#pragma once
#include "CoreMinimal.h"
#include "Util.h"

// Build schema properties from a UFunction with recursive struct support
void BuildSchemaPropsFromUFunction(UFunction* Handler, TDoubleLinkedList<SchemaNode>& OutProps);

void BuildSchemaPropsFromFProperty(FProperty* Property, TDoubleLinkedList<SchemaNode>& OutProps);

void ConstructSchemaProp(FProperty* Property, SchemaNode& OutProp);

// Build a UE ImportText-style argument list string (without function name) from JSON data
// Example output for a single struct arg: (X=5.0,Y=5.0,Z=5.0)
FString BuildArgStringFromJson(const TDoubleLinkedList<SchemaNode>& Props, const TSharedRef<FJsonObject>& Data);
