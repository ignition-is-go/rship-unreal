#include "Util.h"
#include "Logs.h"

TSharedPtr<FJsonObject> ParseJSON(const FString &JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        return JsonObject;
    }

    return nullptr;
}

TSharedPtr<FJsonObject> ParseJSONObject(const TWeakPtr<FJsonValue> &WeakValue)
{
    if (auto Value = WeakValue.Pin())
    {
        if (Value->Type == EJson::Object)
        {
            return Value->AsObject();
        }
    }
    return nullptr;
}

TArray<TSharedPtr<FJsonValue>> ParseJSONArray(const TWeakPtr<FJsonValue> &WeakValue)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    if (auto Value = WeakValue.Pin())
    {
        if (Value->Type == EJson::Array)
        {
            Array = Value->AsArray();
            for (const auto &Item : Array)
            {
                if (Item->Type == EJson::Object)
                {
                    // Recursive parsing for nested objects within arrays
                    TSharedPtr<FJsonObject> NestedObject = ParseJSONObject(Item);
                    // Process NestedObject as needed
                }
                else if (Item->Type == EJson::Array)
                {
                    // Recursive parsing for nested arrays
                    TArray<TSharedPtr<FJsonValue>> NestedArray = ParseJSONArray(Item);
                    // Process NestedArray as needed
                }
                // Handle other types (String, Number, Bool, etc.) as needed
            }
        }
    }
    return Array;
}

FString GetJsonString(TSharedPtr<FJsonObject> JsonObject)
{
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    return OutputString;
}

FString UnrealToJsonSchemaTypeLookup(FString unrealType)
{

    /*

   [2024-03-20T18:20:37.718Z]LogTemp: Warning: Property: Boolean, Type: BoolProperty
   [2024-03-20T18:20:37.718Z]LogTemp: Warning: Property: Byte, Type: ByteProperty
   [2024-03-20T18:20:37.718Z]LogTemp: Warning: Property: Integer, Type: IntProperty
   [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Integer64, Type: Int64Property
   [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Float, Type: DoubleProperty
   [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Name, Type: NameProperty
   [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: String, Type: StrProperty
   [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Text, Type: TextProperty
   [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Vector, Type: StructProperty
   [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Rotator, Type: StructProperty
   [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Transform, Type: StructProperty

   */
    UE_LOG(LogRshipExec, Verbose, TEXT("Schema Type Type: %s"), *unrealType);
    if (unrealType == "BoolProperty")
    {
        return "boolean";
    }
    else if (unrealType == "ByteProperty")
    {
        return "number";
    }
    else if (unrealType == "IntProperty")
    {
        return "number";
    }
    else if (unrealType == "Int64Property")
    {
        return "number";
    }
    else if (unrealType == "DoubleProperty")
    {
        return "number";
    }
    else if (unrealType == "NameProperty")
    {
        return "string";
    }
    else if (unrealType == "StrProperty")
    {
        return "string";
    }
    else if (unrealType == "TextProperty")
    {
        return "string";
    }
    else if (unrealType == "StructProperty")
    {
        // Structs will be expanded as nested objects; type decided by caller
        return "object";
    }
    else
    {
        return "unknown";
    }
}

static TSharedPtr<FJsonObject> RshipPropToSchemaObject(const SchemaNode &prop)
{
    TSharedPtr<FJsonObject> propObj = MakeShareable(new FJsonObject());

    // Primitive Unreal property classes map directly.
    const FString jsonType = UnrealToJsonSchemaTypeLookup(prop.Type);
    if (prop.Type == "StructProperty")
    {
        // Build nested schema from children
        propObj->SetStringField("type", "object");
        TSharedPtr<FJsonObject> childProps = MakeShareable(new FJsonObject());
        for (const SchemaNode &child : prop.Children)
        {
            childProps->SetObjectField(child.Name, RshipPropToSchemaObject(child));
        }
        propObj->SetObjectField("properties", childProps);
    }
    else if (jsonType != "unknown")
    {
        propObj->SetStringField("type", jsonType);
    }
    else
    {
        // Unknowns are represented loosely as strings
        UE_LOG(LogTemp, Warning, TEXT("Unknown Type in schema: %s for %s. Defaulting to string."), *prop.Type, *prop.Name);
        propObj->SetStringField("type", "string");
    }
    UE_LOG(LogRshipExec, Verbose, TEXT("Added %s: %s"), *prop.Name, *prop.Type);
    return propObj;
}

TSharedPtr<FJsonObject> PropsToSchema(TDoubleLinkedList<SchemaNode> *props)
{
    TSharedPtr<FJsonObject> properties = MakeShareable(new FJsonObject());

    for (SchemaNode const &prop : *props)
    {
        properties->SetObjectField(prop.Name, RshipPropToSchemaObject(prop));
    }

    TSharedPtr<FJsonObject> schema = MakeShareable(new FJsonObject());
    schema->SetObjectField("properties", properties);
    schema->SetStringField("$schema", "http://json-schema.org/draft-07/schema#");
    schema->SetStringField("type", "object");

    // UE_LOG(LogTemp, Warning, TEXT("Schema: %s"), *GetJsonString(schema));

    return schema;
}
