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

#include "Myko.h"

using namespace std;

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
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
            UE_LOG(LogTemp, Warning, TEXT("ClientId %s"), *ClientId);

            SendAll();
        }

        if (commandId == "target:action:exec")
        {
            TSharedPtr<FJsonObject> command = data->GetObjectField(TEXT("command"));

            TSharedPtr<FJsonObject> action = command->GetObjectField(TEXT("action"));

            FString id = action->GetStringField(TEXT("id"));

            UE_LOG(LogTemp, Warning, TEXT("Action %s"), *id)

            if (ActionMap.Contains(id) && TargetMap.Contains(id))
            {
                UE_LOG(LogTemp, Warning, TEXT("ActionMap Contains %s"), *id)
                AActor *target = TargetMap[id];
                UFunction *callback = ActionMap[id];

                FString targetName = target->GetName();
                FString callbackName = callback->GetName();

                if (target && callback)
                {

                    UE_LOG(LogTemp, Warning, TEXT("Target %s"), *targetName);
                    UE_LOG(LogTemp, Warning, TEXT("Action %s"), *callbackName);

                    try
                    {
                        target->ProcessEvent(callback, nullptr);
                    }
                    catch (...)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Error Processing Event"));
                    }
                }
            }

            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ActionMap Does Not Contain %s"), *id)
            }
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

    for (FString targetId : RegisteredTargets)
    {
        TArray<TSharedPtr<FJsonValue>> EmitterIdsJson;
        TArray<TSharedPtr<FJsonValue>> ActionIdsJson;

        if (TargetActionMap.Contains(targetId))
        {

            auto actionIds = TargetActionMap[targetId];

            for (const FString &Element : *actionIds)
            {
                UE_LOG(LogTemp, Warning, TEXT("Action %s"), *Element);

                ActionIdsJson.Push(MakeShareable(new FJsonValueString(Element)));

                TSharedPtr<FJsonObject> Action = MakeShareable(new FJsonObject);

                Action->SetStringField(TEXT("id"), Element);
                Action->SetStringField(TEXT("name"), Element);
                Action->SetStringField(TEXT("targetId"), targetId);
                Action->SetStringField(TEXT("serviceId"), ServiceId);
                if (ActionSchemas.Contains(Element))
                {
                    Action->SetObjectField(TEXT("schema"), ActionSchemasJson[ActionSchemas[Element]]);
                }
                if (ActionSchemasJson.Contains(Element))
                {
                    Action->SetObjectField(TEXT("schema"), ActionSchemasJson[Element]);
                }

                SetItem("Action", Action);
            }
        }

        if (TargetEmitterMap.Contains(targetId))
        {

            auto emitterIds = TargetEmitterMap[targetId];

            for (const FString &emitterId : *emitterIds)
            {

                UE_LOG(LogTemp, Warning, TEXT("Emitter %s"), *emitterId);

                EmitterIdsJson.Push(MakeShareable(new FJsonValueString(emitterId)));

                TSharedPtr<FJsonObject> Emitter = MakeShareable(new FJsonObject);

                Emitter->SetStringField(TEXT("id"), emitterId);
                Emitter->SetStringField(TEXT("name"), emitterId);
                Emitter->SetStringField(TEXT("targetId"), targetId);
                Emitter->SetStringField(TEXT("serviceId"), ServiceId);
                if (EmitterSchemas.Contains(emitterId))
                {
                    Emitter->SetObjectField(TEXT("schema"), EmitterSchemas[emitterId]);
                }

                SetItem("Emitter", Emitter);
            }
        }

        TSharedPtr<FJsonObject> Target = MakeShareable(new FJsonObject);
        Target->SetStringField(TEXT("id"), targetId);

        Target->SetArrayField(TEXT("actionIds"), ActionIdsJson);
        Target->SetArrayField(TEXT("emitterIds"), EmitterIdsJson);
        Target->SetStringField(TEXT("fgColor"), ColorHex);
        Target->SetStringField(TEXT("bgColor"), ColorHex);
        Target->SetStringField(TEXT("name"), targetId);
        Target->SetStringField(TEXT("serviceId"), ServiceId);

        SetItem("Target", Target);

        TSharedPtr<FJsonObject> TargetStatus = MakeShareable(new FJsonObject);
        TargetStatus->SetStringField(TEXT("targetId"), targetId);
        TargetStatus->SetStringField(TEXT("instanceId"), RunId);
        TargetStatus->SetStringField(TEXT("status"), TEXT("online"));

        SetItem("TargetStatus", TargetStatus);
    }
    //
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

void URshipSubsystem::RegisterTarget(AActor *target)
{

    FString targetId = ServiceId + ":" + target->GetName();
    RegisteredTargets.Add(targetId);

    if (!TargetActionMap.Contains(targetId))
    {
        TSet<FString> actionSet;
        TargetActionMap.Add(targetId, &actionSet);
    }

    if (!TargetEmitterMap.Contains(targetId))
    {
        TSet<FString> emitterSet;
        TargetEmitterMap.Add(targetId, &emitterSet);
    }

    UClass *ownerClass = target->GetClass();
    FString className = ownerClass->GetName();
    UE_LOG(LogTemp, Warning, TEXT("Class name HAHA: %s"), *className);
    // ...
    for (TFieldIterator<UFunction> func(ownerClass, EFieldIteratorFlags::ExcludeSuper); func; ++func)
    {
        UFunction *f = *func;

        FString name = f->GetName();

        if (name.StartsWith("RS_"))
        {
            UE_LOG(LogTemp, Warning, TEXT("Action found: %s"), *name);

            RegisterAction(targetId, target, f);
        }
    }
    URshipSubsystem::SendAll();
}

void URshipSubsystem::RegisterAction(FString targetId, AActor *target, UFunction *action)
{

    FString fullActionId = targetId + ":" + action->GetName();

    ActionMap.Add(fullActionId, action);
    TargetMap.Add(fullActionId, target);

    TDoubleLinkedList<RshipActionProperty> *props = new TDoubleLinkedList<RshipActionProperty>();

    TSharedPtr<FJsonObject> schema = MakeShareable(new FJsonObject);

    for (TFieldIterator<FProperty> It(action); It; ++It)
    {
        FProperty *Property = *It;
        FString PropertyName = Property->GetName();
        FName PropertyType = Property->GetClass()->GetFName();

        auto prop = RshipActionProperty({PropertyName, Property->GetClass()->GetFName().ToString()});
        schema->SetStringField(PropertyName, PropertyType.ToString());

        props->AddTail(prop);

        UE_LOG(LogTemp, Warning, TEXT("Property found: %s : %s"), *PropertyName, *PropertyType.ToString());
    }

    TSet<FString> *actionSet = TargetActionMap[targetId];
    if (!actionSet)
    {
        UE_LOG(LogTemp, Warning, TEXT("Action Set Not Found for targetId %s"), *targetId);
        return;
    }
    actionSet->Add(fullActionId);
    PropMap.Add(fullActionId, props);
}

void URshipSubsystem::RegisterEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> schema)
{

    FString fullEmitterId = ServiceId + ":" + targetId + ":" + emitterId;

    RegisteredTargets.Add(targetId);
    EmitterSchemas.Add(fullEmitterId, schema);

    auto actionSet = TargetEmitterMap[targetId];
    actionSet->Add(fullEmitterId);
}

void URshipSubsystem::PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data)
{

    FString fullEmitterId = ServiceId + ":" + targetId + ":" + emitterId;

    auto timestamp = FDateTime::Now().ToUnixTimestamp();
    UE_LOG(LogTemp, Warning, TEXT("Pulse Emitter %s %d"), *fullEmitterId, timestamp);

    if (EmitterSchemas.Contains(fullEmitterId))
    {
        TSharedPtr<FJsonObject> Pulse = MakeShareable(new FJsonObject);

        Pulse->SetStringField(TEXT("emitterId"), fullEmitterId);
        Pulse->SetStringField(TEXT("id"), fullEmitterId);
        Pulse->SetObjectField("data", data);
        Pulse->SetNumberField("timestamp", FDateTime::Now().ToUnixTimestamp());

        SetItem("Pulse", Pulse);
    }
}
