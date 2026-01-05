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
#include "EmitterHandler.h"
#include "Logs.h"

using namespace std;

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipSubsystem::Initialize"));
    Reconnect();

    auto world = GetWorld();

    if (world != nullptr)
    {
        this->EmitterHandler = GetWorld()->SpawnActor<AEmitterHandler>();
    }

    this->TargetComponents = new TSet<URshipTargetComponent *>;
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

    ClusterId = MachineId + ":" + ServiceId;
    InstanceId = ClusterId;

    const URshipSettings *Settings = GetDefault<URshipSettings>();
    FString rshipHostAddress = *Settings->rshipHostAddress;
    int32 rshipServerPort = Settings->rshipServerPort;

    if (rshipHostAddress.IsEmpty() || rshipHostAddress.Len() == 0)
    {
        rshipHostAddress = FString("localhost");
    }

    if (WebSocket)
    {
        WebSocket->Close();
    }

    WebSocket = FWebSocketsModule::Get().CreateWebSocket("ws://" + rshipHostAddress + ":" + FString::FromInt(rshipServerPort) + "/myko");

    WebSocket->OnConnected().AddLambda([&]()
                                       {
                                           UE_LOG(LogRshipExec, Log, TEXT("Connected"));
                                           SendAll();
                                           //
                                       });

    WebSocket->OnConnectionError().AddLambda([](const FString &Error)
                                             {
                                                 UE_LOG(LogRshipExec, Warning, TEXT("Connection Error %s"), *Error);
                                                 //
                                             });

    WebSocket->OnClosed().AddLambda([](int32 StatusCode, const FString &Reason, bool bWasClean)
                                    {
                                        UE_LOG(LogRshipExec, Warning, TEXT("Closed %d %s %d"), StatusCode, *Reason, bWasClean);
                                        //
                                    });

    WebSocket->OnMessage().AddLambda([&](const FString &MessageString)
                                     {
                                         // UE_LOG(LogRshipExec, Warning, TEXT("Message %s"), *MessageString);
                                         ProcessMessage(*MessageString);
                                         //
                                     });

    WebSocket->OnMessageSent().AddLambda([](const FString &MessageString)
                                         {
                                             // UE_LOG(LogRshipExec, Warning, TEXT("Message Sent %s"), *MessageString);

                                             //
                                         });

    WebSocket->Connect();
}

void URshipSubsystem::ProcessMessage(const FString &message)
{

    TSharedPtr<FJsonObject> obj = MakeShareable(new FJsonObject);
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(message);

    if (!(FJsonSerializer::Deserialize(Reader, obj) && obj.IsValid()))
    {
        return;
    }

    TSharedRef<FJsonObject> objRef = obj.ToSharedRef();

    FString type = objRef->GetStringField(TEXT("event"));
    UE_LOG(LogRshipExec, Verbose, TEXT("Received Event %s"), *type);

    if (type == "ws:m:command")
    {
        TSharedRef<FJsonObject> data = obj->GetObjectField(TEXT("data")).ToSharedRef();

        FString commandId = data->GetStringField(TEXT("commandId"));
        TSharedRef<FJsonObject> command = data->GetObjectField(TEXT("command")).ToSharedRef();

        FString txId = command->GetStringField(TEXT("tx"));

        if (commandId == "SetClientId")
        {

            ClientId = command->GetStringField(TEXT("clientId"));
            UE_LOG(LogRshipExec, Warning, TEXT("Received ClientId %s"), *ClientId);
            SendAll();
            return;
        }

        if (commandId == "ExecTargetAction")
        {

            TSharedRef<FJsonObject> execAction = command->GetObjectField(TEXT("action")).ToSharedRef();
            TSharedRef<FJsonObject> execData = command->GetObjectField(TEXT("data")).ToSharedRef();

            FString actionId = execAction->GetStringField(TEXT("id"));

            FString targetId = execAction->GetStringField(TEXT("targetId"));

            bool result = false;

            for (URshipTargetComponent *comp : *this->TargetComponents)
            {

                if (comp->TargetData->GetId() == targetId)
                {

                    Target *target = comp->TargetData;
                    AActor *owner = comp->GetOwner();

                    if (target != nullptr)
                    {
                        UE_LOG(LogRshipExec, Verbose, TEXT("Taking Action: %s on Target %s"), *actionId, *targetId);
                        bool takeResult = target->TakeAction(owner, actionId, execData);
                        result |= takeResult;
                        comp->OnDataReceived();
                    }
                    else
                    {
                        UE_LOG(LogRshipExec, Warning, TEXT("Target not found: %s"), *targetId);
                    }
                }
            }

            TSharedPtr<FJsonObject> responseData = MakeShareable(new FJsonObject);

            responseData->SetStringField(TEXT("commandId"), commandId);
            responseData->SetStringField(TEXT("tx"), txId);

            if (result)
            {
                // send success response
                TSharedPtr<FJsonObject> response = MakeShareable(new FJsonObject);
                response->SetStringField(TEXT("event"), TEXT("ws:m:command-response"));
                response->SetObjectField(TEXT("data"), responseData);

                SendJson(response);
            }
            else
            {
                // send failure response
                UE_LOG(LogRshipExec, Warning, TEXT("Action not taken: %s on Target %s"), *actionId, *targetId);
                TSharedPtr<FJsonObject> response = MakeShareable(new FJsonObject);
                response->SetStringField(TEXT("event"), TEXT("ws:m:command-error"));
                response->SetObjectField(TEXT("data"), responseData);

                SendJson(response);
            }

            // command.Reset();
        }
        obj.Reset();
    }
}

void URshipSubsystem::Deinitialize()
{

    UE_LOG(LogRshipExec, Log, TEXT("RshipSubsystem::Deinitialize"));

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

    for (auto &Elem : target->GetActions())
    {
        ActionIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));

        SendAction(Elem.Value, target->GetId());
    }

    for (auto &Elem : target->GetEmitters())
    {
        EmitterIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));

        SendEmitter(Elem.Value, target->GetId());
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
    TargetStatus->SetStringField(TEXT("instanceId"), InstanceId);
    TargetStatus->SetStringField(TEXT("status"), TEXT("online"));
    TargetStatus->SetStringField(TEXT("id"), InstanceId + ":" + target->GetId());

    SetItem("TargetStatus", TargetStatus);
}

void URshipSubsystem::SendAction(Action *action, FString targetId)
{

    TSharedPtr<FJsonObject> Action = MakeShareable(new FJsonObject);

    Action->SetStringField(TEXT("id"), action->GetId());
    Action->SetStringField(TEXT("name"), action->GetName());
    Action->SetStringField(TEXT("targetId"), targetId);
    Action->SetStringField(TEXT("serviceId"), ServiceId);
    TSharedPtr<FJsonObject> schema = action->GetSchema();
    if (schema)
    {
        Action->SetObjectField(TEXT("schema"), schema);
    }
    SetItem("Action", Action);
}

void URshipSubsystem::SendEmitter(EmitterContainer *emitter, FString targetId)
{
    TSharedPtr<FJsonObject> Emitter = MakeShareable(new FJsonObject);

    Emitter->SetStringField(TEXT("id"), emitter->GetId());
    Emitter->SetStringField(TEXT("name"), emitter->GetName());
    Emitter->SetStringField(TEXT("targetId"), targetId);
    Emitter->SetStringField(TEXT("serviceId"), ServiceId);
    TSharedPtr<FJsonObject> schema = emitter->GetSchema();
    if (schema)
    {
        Emitter->SetObjectField(TEXT("schema"), schema);
    }
    SetItem("Emitter", Emitter);
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
    Instance->SetStringField(TEXT("id"), InstanceId);
    Instance->SetStringField(TEXT("clusterId"), ClusterId);
    Instance->SetStringField(TEXT("serviceTypeCode"), TEXT("unreal"));
    Instance->SetStringField(TEXT("serviceId"), ServiceId);
    Instance->SetStringField(TEXT("machineId"), MachineId);
    Instance->SetStringField(TEXT("status"), "Available");
    Instance->SetStringField(TEXT("color"), ColorHex);

    SetItem("Instance", Instance);

    for (auto &comp : *this->TargetComponents)
    {
        SendTarget(comp->TargetData);
    }
}

void URshipSubsystem::SendJson(TSharedPtr<FJsonObject> Payload)
{
    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    if (FJsonSerializer::Serialize(Payload.ToSharedRef(), JsonWriter))
    {
         UE_LOG(LogRshipExec, Verbose, TEXT("JSON: %s"), *JsonString);
    }
    else
    {
        UE_LOG(LogRshipExec, Error, TEXT("Failed to serialize JSON object."));
    }
    if (WebSocket == nullptr)
    {
        UE_LOG(LogRshipExec, Error, TEXT("Failed to send JSON object. Socket Not Initialized"));
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

void URshipSubsystem::PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data)
{

    TSharedPtr<FJsonObject> pulse = MakeShareable(new FJsonObject);

    FString fullEmitterId = targetId + ":" + emitterId;

    pulse->SetStringField("emitterId", fullEmitterId);
    pulse->SetStringField("id", fullEmitterId);

    pulse->SetObjectField("data", data);

    this->SetItem("Pulse", pulse);
}

EmitterContainer *URshipSubsystem::GetEmitterInfo(FString fullTargetId, FString emitterId)
{
    Target *target = nullptr;

    for (auto &comp : *this->TargetComponents)
    {
        if (comp->TargetData->GetId() == fullTargetId)
        {
            target = comp->TargetData;
            break;
        }
    }

    if (target == nullptr)
    {
        return nullptr;
    }

    FString fullEmitterId = fullTargetId + ":" + emitterId;

    auto emitters = target->GetEmitters();

    if (!emitters.Contains(fullEmitterId))
    {
        return nullptr;
    }

    return emitters[fullEmitterId];
}

FString URshipSubsystem::GetServiceId()
{
    return ServiceId;
}