// Fill out your copyright notice in the Description page of Project Settings
#include "RshipSubsystem.h"
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
#include "Logging/LogMacros.h"
#include "Subsystems/SubsystemCollection.h"
#include "GameFramework/Actor.h"
#include "UObject/FieldPath.h"
#include "Misc/StructBuilder.h"
#include "UObject/UnrealTypePrivate.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Action.h"
#include "Target.h"

#include "Myko.h"

using namespace std;

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    GenerateSchemas();
    Reconnect();
}

void URshipSubsystem::GenerateSchemas()
{
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

void URshipSubsystem::Reconnect()
{

    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    // Send Exec
    MachineId = GetUniqueMachineId();
    ServiceId = FApp::GetProjectName();
    RunId = FGuid::NewGuid().ToString();

    ClusterId = MachineId + ":" + ServiceId;

    const URshipSettings *Settings = GetDefault<URshipSettings>();
    FString rshipHostAddress = *Settings->rshipHostAddress;

    if (rshipHostAddress.IsEmpty() || rshipHostAddress.Len() == 0)
    {
        rshipHostAddress = FString("localhost");
    }

    if (WebSocket) {
        WebSocket->Close();
    }

    WebSocket = FWebSocketsModule::Get().CreateWebSocket("ws://" + rshipHostAddress + ":5155/myko");

    WebSocket->OnConnected().AddLambda([&]()
                                       {
                                           UE_LOG(LogTemp, Warning, TEXT("Connected"));
                                           //
                                       });

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
}

void URshipSubsystem::ProcessMessage(FString message)
{
    TSharedPtr<FJsonObject> obj = ParseNestedJsonString(*message);

    FString type = obj->GetStringField(TEXT("event"));

    if (type == "ws:m:command")
    {
        TSharedPtr<FJsonObject> data = obj->GetObjectField(TEXT("data"));

        FString commandId = data->GetStringField(TEXT("commandId"));

        if (commandId == "client:setId")
        {
            TSharedPtr<FJsonObject> command = data->GetObjectField(TEXT("command"));

            ClientId = command->GetStringField(TEXT("clientId"));
            UE_LOG(LogTemp, Warning, TEXT("Received ClientId %s"), *ClientId);

            SendAll();
        }

        if (commandId == "target:action:exec")
        {
            TSharedPtr<FJsonObject> command = data->GetObjectField(TEXT("command"));

            TSharedPtr<FJsonObject> execAction = command->GetObjectField(TEXT("action"));

            FString actionId = execAction->GetStringField(TEXT("id"));

            FString targetId = execAction->GetStringField(TEXT("targetId"));

            if (!AllTargets.Contains(targetId)) {
                UE_LOG(LogTemp, Warning, TEXT("Target Not Found: [%s]"), *targetId);
                return;
            }

            Target *target = AllTargets[targetId];

            target->TakeAction(actionId);

        }
    }

    //
}

void URshipSubsystem::Deinitialize()
{
    if (WebSocket->IsConnected())
    {
        WebSocket->Close();
    }
    Super::Deinitialize();
}

void URshipSubsystem::SendTarget(Target *target)
{
    TArray<TSharedPtr<FJsonValue>> EmitterIdsJson;
    TArray<TSharedPtr<FJsonValue>> ActionIdsJson;

    UE_LOG(LogTemp, Warning, TEXT("Building Target JSON: %s"), *target->GetId());

    for (auto& Elem : target->GetActions())
    {
        ActionIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));

        SendAction(Elem.Value, target->GetId());
    }

    const URshipSettings *Settings = GetDefault<URshipSettings>();

    FColor SRGBColor = Settings->ServiceColor.ToFColor(true); // Convert to FColor (SRGB space)
    FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);
    TSharedPtr<FJsonObject> Target = MakeShareable(new FJsonObject);
    Target->SetStringField(TEXT("id"), target->GetId());

    Target->SetArrayField(TEXT("actionIds"), ActionIdsJson);
    Target->SetArrayField(TEXT("emitterIds"), EmitterIdsJson);
    Target->SetStringField(TEXT("fgColor"), ColorHex);
    Target->SetStringField(TEXT("bgColor"), ColorHex);
    Target->SetStringField(TEXT("name"), target->GetId());
    Target->SetStringField(TEXT("serviceId"), ServiceId);

    SetItem("Target", Target);

    TSharedPtr<FJsonObject> TargetStatus = MakeShareable(new FJsonObject);

    TargetStatus->SetStringField(TEXT("targetId"), target->GetId());
    TargetStatus->SetStringField(TEXT("instanceId"), RunId);
    TargetStatus->SetStringField(TEXT("status"), TEXT("online"));
    TargetStatus->SetStringField(TEXT("id"), RunId + ":" + target->GetId());

    SetItem("TargetStatus", TargetStatus);
}

void URshipSubsystem::SendAction(Action *action, FString targetId)
{
    UE_LOG(LogTemp, Warning, TEXT("Building Action JSON: %s"), *action->GetId());

    TSharedPtr<FJsonObject> Action = MakeShareable(new FJsonObject);

    Action->SetStringField(TEXT("id"), action->GetId());
    Action->SetStringField(TEXT("name"), action->GetId());
    Action->SetStringField(TEXT("targetId"), targetId);
    Action->SetStringField(TEXT("serviceId"), ServiceId);
    auto schema = action->GetSchema();
    if (schema){
        Action->SetObjectField(TEXT("schema"), schema);
    }
    SetItem("Action", Action);
}

void URshipSubsystem::SendTargetStatus(Target *target, bool online)
{
}

void URshipSubsystem::SendAll()
{

    // Send Machine

    TSharedPtr<FJsonObject> Machine = MakeShareable(new FJsonObject);

    Machine->SetStringField(TEXT("id"), MachineId);
    Machine->SetStringField(TEXT("execName"), MachineId);

    SetItem("Machine", Machine);

    const URshipSettings *Settings = GetDefault<URshipSettings>();

    FColor SRGBColor = Settings->ServiceColor.ToFColor(true); // Convert to FColor (SRGB space)
    FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);

    TSharedPtr<FJsonObject> Instance = MakeShareable(new FJsonObject);

    Instance->SetStringField(TEXT("clientId"), ClientId);
    Instance->SetStringField(TEXT("name"), ServiceId);
    Instance->SetStringField(TEXT("id"), RunId);
    Instance->SetStringField(TEXT("clusterId"), ClusterId);
    Instance->SetStringField(TEXT("serviceTypeCode"), TEXT("unreal"));
    Instance->SetStringField(TEXT("serviceId"), ServiceId);
    Instance->SetStringField(TEXT("machineId"), MachineId);
    Instance->SetStringField(TEXT("status"), "Available");
    Instance->SetStringField(TEXT("color"), ColorHex);

    SetItem("Instance", Instance);

    for (auto &Elem : AllTargets)
    {
        SendTarget(Elem.Value);
    }
}

void URshipSubsystem::SendJson(TSharedPtr<FJsonObject> Payload)
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
    if (WebSocket == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to send JSON object. Socket Not Initialized"));
        return;
    }

    if (!WebSocket->IsConnected())
    {
        WebSocket->Connect();
    }
    WebSocket->Send(JsonString);
}

void URshipSubsystem::SetItem(FString itemType, TSharedPtr<FJsonObject> data)
{
    SendJson(WrapWSEvent(MakeSet(itemType, data)));
}

void URshipSubsystem::RegisterTarget(AActor *actor)
{
    FString targetId = ServiceId + ":" + actor->GetComponentByClass<URshipTargetComponent>()->targetId;

    Target *target = new Target(targetId);

    UClass *ownerClass = actor->GetClass();
    // ...
    for (TFieldIterator<UFunction> field(ownerClass, EFieldIteratorFlags::ExcludeSuper); field; ++field)
    {
        UFunction *handler = *field;

        FString name = field->GetName();

        if (name.StartsWith("RS_"))
        {
            FString fullActionId = targetId + ":" + name;

            Action *action = new Action(fullActionId, actor, handler);
            target->AddAction(action);
        }
    }

    AllTargets.Add(targetId, target);
    SendAll();
}

void URshipSubsystem::RegisterEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> schema)
{

    //FString fullEmitterId = ServiceId + ":" + targetId + ":" + emitterId;

    //RegisteredTargets.Add(targetId);
    //EmitterSchemas.Add(fullEmitterId, schema);

    //if (!TargetEmitterMap.Contains(targetId))
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("Emitter Set Not Found for targetId %s"), *targetId);
    //    return;
    //}

    //TargetEmitterMap[targetId].Add(fullEmitterId);
}

void URshipSubsystem::PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data)
{

    FString fullEmitterId = ServiceId + ":" + targetId + ":" + emitterId;

    auto timestamp = FDateTime::Now().ToUnixTimestamp();
    UE_LOG(LogTemp, Warning, TEXT("Pulse Emitter %s %d"), *fullEmitterId, timestamp);

  /*  if (EmitterSchemas.Contains(fullEmitterId))
    {
        TSharedPtr<FJsonObject> Pulse = MakeShareable(new FJsonObject);

        Pulse->SetStringField(TEXT("emitterId"), fullEmitterId);
        Pulse->SetStringField(TEXT("id"), fullEmitterId);
        Pulse->SetObjectField("data", data);
        Pulse->SetNumberField("timestamp", FDateTime::Now().ToUnixTimestamp());

        SetItem("Pulse", Pulse);
    }*/
}

void URshipSubsystem::Reset() {
    AllTargets.Reset();
}