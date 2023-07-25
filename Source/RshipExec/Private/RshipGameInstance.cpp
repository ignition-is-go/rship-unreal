
#include "RshipGameInstance.h"
#include "RshipSettings.h"
#include "WebSocketsModule.h"
#include "EngineUtils.h"
#include "RshipTargetComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Json.h"
#include "JsonObjectWrapper.h"
#include "JsonObjectConverter.h"
#include "Util.h"

#include "Myko.h"

using namespace std;

void URshipGameInstance::Init()
{
    Super::Init();

    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    // Send Exec
    MachineId = GetUniqueMachineId();
    ServiceId = FApp::GetProjectName();
    RunId = FGuid::NewGuid().ToString();

    ClusterId = MachineId + ":" + ServiceId;

    ClientId = ClusterId + ":" + RunId;

    const URshipSettings *Settings = GetDefault<URshipSettings>();
    FString rshipHostAddress = Settings->rshipHostAddress;

    if (rshipHostAddress.IsEmpty() || rshipHostAddress.Len() == 0)
    {
        rshipHostAddress = "localhost";
    }

    WebSocket = FWebSocketsModule::Get().CreateWebSocket("ws://" + rshipHostAddress + ":5155/myko?clientId=" + ClientId);

    WebSocket->OnConnected().AddLambda([&]()
                                       {
                                           UE_LOG(LogTemp, Warning, TEXT("Connected"));
                                           SendAllTargetActions(); });

    WebSocket->OnConnectionError().AddLambda([](const FString &Error)
                                             {
                                                 UE_LOG(LogTemp, Warning, TEXT("Connection Error %s"), *Error);
                                                 //
                                             });

    WebSocket->OnClosed().AddLambda([](int32 StatusCode, const FString &Reason, bool bWasClean)
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("Closed %d %s %d"), StatusCode, *Reason, bWasClean);
                                        //
                                    });

    WebSocket->OnMessage().AddLambda([&](const FString &MessageString)
                                     {
                                         UE_LOG(LogTemp, Warning, TEXT("Message %s"), *MessageString);
                                         ProcessMessage(*MessageString);
                                         //
                                     });

    WebSocket->OnMessageSent().AddLambda([](const FString &MessageString)
                                         {
                                             // UE_LOG(LogTemp, Warning, TEXT("Message Sent %s"), *MessageString);

                                             //
                                         });

    WebSocket->Connect();

    // float schema
    // {
    //   '$schema': 'http://json-schema.org/draft-07/schema#',
    //   type: 'object',
    //   properties: { value: { type: 'number' } }
    // }

    TSharedPtr<FJsonObject> valueFloat = MakeShareable(new FJsonObject);
    valueFloat->SetStringField(TEXT("type"), TEXT("number"));

    TSharedPtr<FJsonObject> propertiesFloat = MakeShareable(new FJsonObject);
    propertiesFloat->SetObjectField(TEXT("value"), valueFloat);

    TSharedPtr<FJsonObject> schemaFloat = MakeShareable(new FJsonObject);
    schemaFloat->SetStringField(TEXT("$schema"), TEXT("http://json-schema.org/draft-07/schema#"));
    schemaFloat->SetStringField(TEXT("type"), TEXT("object"));
    schemaFloat->SetObjectField(TEXT("properties"), propertiesFloat);

    ActionSchemasJson.Add(TEXT("float"), schemaFloat);

    // string schema
    // {
    //   '$schema': 'http://json-schema.org/draft-07/schema#',
    //   type: 'object',
    //   properties: { value: { type: 'string' } }
    // }

    TSharedPtr<FJsonObject> valueString = MakeShareable(new FJsonObject);
    valueString->SetStringField(TEXT("type"), TEXT("string"));

    TSharedPtr<FJsonObject> propertiesString = MakeShareable(new FJsonObject);
    propertiesString->SetObjectField(TEXT("value"), valueString);

    TSharedPtr<FJsonObject> schemaString = MakeShareable(new FJsonObject);
    schemaString->SetStringField(TEXT("$schema"), TEXT("http://json-schema.org/draft-07/schema#"));
    schemaString->SetStringField(TEXT("type"), TEXT("object"));
    schemaString->SetObjectField(TEXT("properties"), propertiesString);

    ActionSchemasJson.Add(TEXT("string"), schemaString);
}

void URshipGameInstance::ProcessMessage(FString message)
{
    TSharedPtr<FJsonObject> obj = ParseNestedJsonString(message);

    FString type = obj->GetStringField(TEXT("event"));

    if (type == "ws:m:command")
    {
        TSharedPtr<FJsonObject> data = obj->GetObjectField(TEXT("data"));

        FString commandId = data->GetStringField(TEXT("commandId"));

        if (commandId == "target:action:exec")
        {

            TSharedPtr<FJsonObject> command = data->GetObjectField(TEXT("command"));

            TSharedPtr<FJsonObject> action = command->GetObjectField(TEXT("action"));

            FString id = action->GetStringField(TEXT("id"));

            UE_LOG(LogTemp, Warning, TEXT("Action %s"), *id)

            if (ActionMap.Contains(id))
            {
                UE_LOG(LogTemp, Warning, TEXT("ActionMap Contains %s"), *id)
                FActionCallBack callback = ActionMap[id];
                callback.ExecuteIfBound();
            }
            else if(ActionFloatMap.Contains(id)){
                UE_LOG(LogTemp, Warning, TEXT("ActionFloatMap Contains %s"), *id);
                FActionCallBackFloat callback = ActionFloatMap[id];

                TSharedPtr<FJsonObject> d = command->GetObjectField(TEXT("data"));

                float val = d->GetNumberField(TEXT("value"));

                callback.ExecuteIfBound(val);
            }
            else if(ActionStringMap.Contains(id)){
                UE_LOG(LogTemp, Warning, TEXT("ActionStringMap Contains %s"), *id);
                FActionCallBackString callback = ActionStringMap[id];

                TSharedPtr<FJsonObject> d = command->GetObjectField(TEXT("data"));

                FString val = d->GetStringField(TEXT("value"));

                callback.ExecuteIfBound(val);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ActionMap Does Not Contain %s"), *id)
            }
        }
    }

    //
}

void URshipGameInstance::Shutdown()
{
    if (WebSocket->IsConnected())
    {
        WebSocket->Close();
    }
    Super::Shutdown();
}

void URshipGameInstance::SendAllTargetActions()
{

    // Send Exec
    TSharedPtr<FJsonObject> Exec = MakeShareable(new FJsonObject);

    Exec->SetStringField(TEXT("name"), ServiceId);
    Exec->SetStringField(TEXT("machineId"), MachineId);
    Exec->SetStringField(TEXT("id"), ClientId);

    SetItem("Executor", Exec);

    // Send Machine

    TSharedPtr<FJsonObject> Machine = MakeShareable(new FJsonObject);

    Machine->SetStringField(TEXT("id"), MachineId);
    Machine->SetStringField(TEXT("execName"), MachineId);

    SetItem("Machine", Machine);

    const URshipSettings* Settings = GetDefault<URshipSettings>();

    FColor SRGBColor = Settings->ServiceColor.ToFColor(true); // Convert to FColor (SRGB space)
    FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);

    TSharedPtr<FJsonObject> Instance = MakeShareable(new FJsonObject);

    Instance->SetStringField(TEXT("execId"), ClientId);
    Instance->SetStringField(TEXT("name"), ServiceId);
    Instance->SetStringField(TEXT("id"), ClientId);
    Instance->SetStringField(TEXT("clusterId"), ClusterId);
    Instance->SetStringField(TEXT("serviceTypeCode"), TEXT("unreal"));
    Instance->SetStringField(TEXT("serviceId"), ServiceId);
    Instance->SetStringField(TEXT("machineId"), MachineId);
    Instance->SetStringField(TEXT("status"), "Available");
    Instance->SetStringField(TEXT("color"), ColorHex);

    SetItem("Instance", Instance);

    TMap<FString, TSharedPtr<FJsonObject>> TargetMap;

    for (auto &KeyValuePair : TargetActionMap)
    {
        FString targetId = KeyValuePair.Key;

        TSet actionIds = KeyValuePair.Value;

        TSharedPtr<FJsonObject> Target = MakeShareable(new FJsonObject);

        Target->SetStringField(TEXT("id"), targetId);

        TArray<TSharedPtr<FJsonValue>> ActionIdsJson;

        for (const FString &Element : actionIds)
        {
            UE_LOG(LogTemp, Warning, TEXT("Action %s"), *Element)

            ActionIdsJson.Add(MakeShareable(new FJsonValueString(Element)));

            TSharedPtr<FJsonObject> Action = MakeShareable(new FJsonObject);

            Action->SetStringField(TEXT("id"), Element);
            Action->SetStringField(TEXT("name"), Element);
            Action->SetStringField(TEXT("targetId"), targetId);
            Action->SetStringField(TEXT("systemId"), ServiceId);
            if(ActionSchemas.Contains(Element)){
                Action->SetObjectField(TEXT("schema"), ActionSchemasJson[ActionSchemas[Element]]);
            }
            if(ActionSchemasJson.Contains(Element)){
                Action->SetObjectField(TEXT("schema"), ActionSchemasJson[Element]);
            }

            SetItem("Action", Action);
        }

        Target->SetArrayField(TEXT("actionIds"), ActionIdsJson);
        Target->SetStringField(TEXT("fgColor"), ColorHex);
        Target->SetStringField(TEXT("bgColor"), ColorHex);
        Target->SetStringField(TEXT("name"), targetId);
        Target->SetStringField(TEXT("serviceId"), ServiceId);

        SetItem("Target", Target);



        // string keyString = TCHAR_TO_UTF8(*key);

        // cout << keyString << endl;

        // FActionCallBack value = KeyValuePair.Value;

        // value.ExecuteIfBound();
    }
    //
}

void URshipGameInstance::SendJson(TSharedPtr<FJsonObject> Payload)
{
    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    if (FJsonSerializer::Serialize(Payload.ToSharedRef(), JsonWriter))
    {
        // UE_LOG(LogTemp, Warning, TEXT("JSON: %s"), *JsonString);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to serialize JSON object."));
    }

    if (WebSocket->IsConnected())
    {

        WebSocket->Send(JsonString);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to send JSON object. Socket Not Connected"));
    }
}

void URshipGameInstance::SetItem(FString itemType, TSharedPtr<FJsonObject> data)
{
    SendJson(WrapWSEvent(MakeSet(itemType, data)));
}

void URshipGameInstance::RegisterAction(FString targetId, FString actionId, FActionCallBack callback)
{
    FString fullActionId = ServiceId + ":" + targetId + ":" + actionId;

    ActionMap.Add(fullActionId, callback);

    ActionSchemas.Add(fullActionId, "void");

    if (TargetActionMap.Contains(targetId))
    {
        TSet<FString> actionSet = TargetActionMap[targetId];
        actionSet.Add(fullActionId);
        TargetActionMap.Add(targetId, actionSet);
    }
    else
    {
        TSet<FString> actionSet;
        actionSet.Add(fullActionId);
        TargetActionMap.Add(targetId, actionSet);
    }
}

void URshipGameInstance::RegisterActionFloat(FString targetId, FString actionId, FActionCallBackFloat callback)
{
    FString fullActionId = ServiceId + ":" + targetId + ":" + actionId;

    ActionFloatMap.Add(fullActionId, callback);
    
    ActionSchemas.Add(fullActionId, "float");

    if (TargetActionMap.Contains(targetId))
    {
        TSet<FString> actionSet = TargetActionMap[targetId];
        actionSet.Add(fullActionId);
        TargetActionMap.Add(targetId, actionSet);
    }
    else
    {
        TSet<FString> actionSet;
        actionSet.Add(fullActionId);
        TargetActionMap.Add(targetId, actionSet);

    }
}

void URshipGameInstance::RegisterActionString(FString targetId, FString actionId, FActionCallBackString callback)
{
    FString fullActionId = ServiceId + ":" + targetId + ":" + actionId;

    ActionStringMap.Add(fullActionId, callback);

    ActionSchemas.Add(fullActionId, "string");

    if (TargetActionMap.Contains(targetId))
    {
        TSet<FString> actionSet = TargetActionMap[targetId];
        actionSet.Add(fullActionId);
        TargetActionMap.Add(targetId, actionSet);
    }
    else
    {
        TSet<FString> actionSet;
        actionSet.Add(fullActionId);
        TargetActionMap.Add(targetId, actionSet);
    }
}

void URshipGameInstance::RegisterActionStringWithOptions(FString targetId, FString actionId, FActionCallBackString stringCallback,  TArray<FString> options)
{
    FString fullActionId = ServiceId + ":" + targetId + ":" + actionId;

    ActionStringMap.Add(fullActionId, stringCallback);

    // enum schema
//    '{' 
//   '  "$schema": "http://json-schema.org/draft-07/schema#",' 
//   '  "type": "object",' 
//   '  "properties": {' 
//   '    "value": {' 
//   '      "enum": [' 
//   '        "hello",' 
//   '        "world"' 
//   '      ]' 
//   '    }' 
//   '  }' 
//   '}'

// 

    TArray<TSharedPtr<FJsonValue>> enumValues;
    for (const FString& option : options)
    {
        TSharedPtr<FJsonValue> enumValue = MakeShareable(new FJsonValueString(option));
        enumValues.Add(enumValue);
    }

    TSharedPtr<FJsonObject> valueEnum = MakeShareable(new FJsonObject);
    valueEnum->SetArrayField(TEXT("enum"), enumValues);

    TSharedPtr<FJsonObject> propertiesEnum = MakeShareable(new FJsonObject);
    propertiesEnum->SetObjectField(TEXT("value"), valueEnum);

    TSharedPtr<FJsonObject> schemaEnum = MakeShareable(new FJsonObject);
    schemaEnum->SetStringField(TEXT("$schema"), TEXT("http://json-schema.org/draft-07/schema#"));
    schemaEnum->SetStringField(TEXT("type"), TEXT("object"));
    schemaEnum->SetObjectField(TEXT("properties"), propertiesEnum);

    ActionSchemasJson.Add(fullActionId, schemaEnum);

    if (TargetActionMap.Contains(targetId))
    {
        TSet<FString> actionSet = TargetActionMap[targetId];
        actionSet.Add(fullActionId);
        TargetActionMap.Add(targetId, actionSet);
    }
    else
    {
        TSet<FString> actionSet;
        actionSet.Add(fullActionId);
        TargetActionMap.Add(targetId, actionSet);
    }
}