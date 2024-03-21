#include "Util.h"
#include "Json.h"
#include "JsonUtilities.h"

void ParseNestedJson(TSharedPtr<FJsonValue> JsonValue, FString KeyPath = "")
{
    if (!JsonValue.IsValid())
    {
        return;
    }

    switch (JsonValue->Type)
    {
    case EJson::Object:
    {
        TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();

        for (auto &Entry : JsonObject->Values)
        {
            FString NewKeyPath = KeyPath.IsEmpty() ? Entry.Key : KeyPath + "." + Entry.Key;
            ParseNestedJson(Entry.Value, NewKeyPath);
        }
        break;
    }
    case EJson::Array:
    {
        TArray<TSharedPtr<FJsonValue>> JsonArray = JsonValue->AsArray();

        for (int32 i = 0; i < JsonArray.Num(); ++i)
        {
            FString NewKeyPath = KeyPath + "[" + FString::FromInt(i) + "]";
            ParseNestedJson(JsonArray[i], NewKeyPath);
        }
        break;
    }
    case EJson::String:
    case EJson::Number:
    case EJson::Boolean:
    case EJson::Null:
    {
        // Process the JSON value here
        UE_LOG(LogTemp, Log, TEXT("%s: %s"), *KeyPath, *JsonValue->AsString());
        break;
    }
    default:
    {
        break;
    }
    }
}

TSharedPtr<FJsonObject> ParseNestedJsonString(FString JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject))
    {
        ParseNestedJson(MakeShareable(new FJsonValueObject(JsonObject)));
    }

    return JsonObject;
}

FString GetJsonString(TSharedPtr<FJsonObject> JsonObject)
{
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    return OutputString;
}
