#include "Json.h"
#include "JsonUtilities.h"

void ParseNestedJson(TSharedPtr<FJsonValue> JsonValue, FString KeyPath);

TSharedPtr<FJsonObject> ParseNestedJsonString(FString JsonString);
