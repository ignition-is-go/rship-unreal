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

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    UE_LOG(LogTemp, Warning, TEXT("RshipSubsystem::Initialize"));
    Reconnect();

    auto world = GetWorld();    

    if (world != nullptr) {
        this->EmitterHandler = GetWorld()->SpawnActor<AEmitterHandler>();
    }

    this->TargetComponents = new TSet<URshipTargetComponent*>;
}

void URshipSubsystem::Reconnect()
{
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    MachineId = Rship::Sdk::GetUniqueMachineId();
    ServiceId = FApp::GetProjectName();
    InstanceId = FGuid::NewGuid().ToString();
    ClusterId = MachineId + ":" + ServiceId;

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const FString HostAddress = Settings->rshipHostAddress;
    const Rship::Sdk::FConnectionDetails Connection = Rship::Sdk::MakeConnectionDetails(HostAddress);

    if (WebSocket)
    {
        WebSocket->Close();
    }

    WebSocket = FWebSocketsModule::Get().CreateWebSocket(Rship::Sdk::BuildWebSocketUrl(Connection));

    WebSocket->OnConnected().AddLambda([this]()
                                       {
                                           UE_LOG(LogTemp, Warning, TEXT("Connected"));
                                           SendAll();
                                       });

    WebSocket->OnConnectionError().AddLambda([](const FString& Error)
                                             {
                                                 UE_LOG(LogTemp, Warning, TEXT("Connection Error %s"), *Error);
                                             });

    WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean)
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("Closed %d %s %d"), StatusCode, *Reason, bWasClean);
                                        ResetPayloadDeduplication();
                                    });

    WebSocket->OnMessage().AddLambda([this](const FString& MessageString)
                                     {
                                         ProcessMessage(MessageString);
                                     });

    WebSocket->OnMessageSent().AddLambda([](const FString& MessageString)
                                         {
                                             UE_LOG(LogTemp, Verbose, TEXT("Message Sent %s"), *MessageString);
                                         });

    if (!WebSocket->IsConnected())
    {
        WebSocket->Connect();
    }
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
     //UE_LOG(LogTemp, Warning, TEXT("Received Event %s"), *type);

    if (type == "ws:m:command")
    {
        TSharedRef<FJsonObject> data = obj->GetObjectField(TEXT("data")).ToSharedRef();

        FString commandId = data->GetStringField(TEXT("commandId"));
        TSharedRef<FJsonObject> command = data->GetObjectField(TEXT("command")).ToSharedRef();

        if (commandId == "SetClientId")
        {

            ClientId = command->GetStringField(TEXT("clientId"));
            UE_LOG(LogTemp, Warning, TEXT("Received ClientId %s"), *ClientId);
            SendAll();
            return;
        }

        if (commandId == "ExecTargetAction")
        {

            TSharedRef<FJsonObject> execAction = command->GetObjectField(TEXT("action")).ToSharedRef();
            TSharedRef<FJsonObject> execData = command->GetObjectField(TEXT("data")).ToSharedRef();

            FString actionId = execAction->GetStringField(TEXT("id"));

            FString targetId = execAction->GetStringField(TEXT("targetId"));



            for (URshipTargetComponent* comp : *this->TargetComponents) {
                
                if (comp->TargetData->GetId() == targetId) {

                    Target* target = comp->TargetData;
                    AActor* owner = comp->GetOwner();

                    if (target != nullptr)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Taking Action: %s"), *actionId);
                        target->TakeAction(owner, actionId, execData);
                    }
                }
            }

            // command.Reset();
        }
        obj.Reset();
    }
}

void URshipSubsystem::Deinitialize()
{

    UE_LOG(LogTemp, Warning, TEXT("RshipSubsystem::Deinitialize"));

    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        WebSocket->Close();
    }
    ResetPayloadDeduplication();
    Super::Deinitialize();
}

void URshipSubsystem::SendTarget(Target* target)
{
    if (!target)
    {
        return;
    }

    const FString TargetId = target->GetId();
    TArray<TSharedPtr<FJsonValue>> EmitterIdsJson;
    TArray<TSharedPtr<FJsonValue>> ActionIdsJson;

    const TMap<FString, Action*>& Actions = target->GetActions();
    for (const TPair<FString, Action*>& Elem : Actions)
    {
        ActionIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));
        SendAction(Elem.Value, TargetId);
    }

    const TMap<FString, EmitterContainer*>& Emitters = target->GetEmitters();
    for (const TPair<FString, EmitterContainer*>& Elem : Emitters)
    {
        EmitterIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));
        SendEmitter(Elem.Value, TargetId);
    }

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const FColor SRGBColor = Settings->ServiceColor.ToFColor(true);
    const FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);

    TSharedPtr<FJsonObject> TargetPayload = MakeShareable(new FJsonObject);
    TargetPayload->SetStringField(TEXT("id"), TargetId);
    TargetPayload->SetArrayField(TEXT("actionIds"), ActionIdsJson);
    TargetPayload->SetArrayField(TEXT("emitterIds"), EmitterIdsJson);
    TargetPayload->SetStringField(TEXT("fgColor"), ColorHex);
    TargetPayload->SetStringField(TEXT("bgColor"), ColorHex);
    TargetPayload->SetStringField(TEXT("name"), TargetId);
    TargetPayload->SetStringField(TEXT("serviceId"), ServiceId);

    SetItem(TEXT("Target"), TargetPayload);

    TSharedPtr<FJsonObject> TargetStatus = MakeShareable(new FJsonObject);
    TargetStatus->SetStringField(TEXT("targetId"), TargetId);
    TargetStatus->SetStringField(TEXT("instanceId"), InstanceId);
    TargetStatus->SetStringField(TEXT("status"), TEXT("online"));
    TargetStatus->SetStringField(TEXT("id"), InstanceId + TEXT(":") + TargetId);

    SetItem(TEXT("TargetStatus"), TargetStatus);
}

void URshipSubsystem::SendAction(Action* action, const FString& targetId)
{
    if (!action)
    {
        return;
    }

    TSharedPtr<FJsonObject> ActionPayload = MakeShareable(new FJsonObject);
    ActionPayload->SetStringField(TEXT("id"), action->GetId());
    ActionPayload->SetStringField(TEXT("name"), action->GetName());
    ActionPayload->SetStringField(TEXT("targetId"), targetId);
    ActionPayload->SetStringField(TEXT("serviceId"), ServiceId);

    if (const TSharedPtr<FJsonObject> Schema = action->GetSchema())
    {
        ActionPayload->SetObjectField(TEXT("schema"), Schema);
    }

    SetItem(TEXT("Action"), ActionPayload);
}

void URshipSubsystem::SendEmitter(EmitterContainer* emitter, const FString& targetId)
{
    if (!emitter)
    {
        return;
    }

    TSharedPtr<FJsonObject> EmitterPayload = MakeShareable(new FJsonObject);
    EmitterPayload->SetStringField(TEXT("id"), emitter->GetId());
    EmitterPayload->SetStringField(TEXT("name"), emitter->GetName());
    EmitterPayload->SetStringField(TEXT("targetId"), targetId);
    EmitterPayload->SetStringField(TEXT("serviceId"), ServiceId);

    if (const TSharedPtr<FJsonObject> Schema = emitter->GetSchema())
    {
        EmitterPayload->SetObjectField(TEXT("schema"), Schema);
    }

    SetItem(TEXT("Emitter"), EmitterPayload);
}



void URshipSubsystem::SendTargetStatus(Target *target, bool online)
{
}

void URshipSubsystem::SendAll()
{
    ResetPayloadDeduplication();

    TSharedPtr<FJsonObject> Machine = MakeShareable(new FJsonObject);
    Machine->SetStringField(TEXT("id"), MachineId);
    Machine->SetStringField(TEXT("execName"), MachineId);
    SetItem(TEXT("Machine"), Machine);

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const FColor SRGBColor = Settings->ServiceColor.ToFColor(true);
    const FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);

    TSharedPtr<FJsonObject> Instance = MakeShareable(new FJsonObject);
    Instance->SetStringField(TEXT("clientId"), ClientId);
    Instance->SetStringField(TEXT("name"), ServiceId);
    Instance->SetStringField(TEXT("id"), InstanceId);
    Instance->SetStringField(TEXT("clusterId"), ClusterId);
    Instance->SetStringField(TEXT("serviceTypeCode"), TEXT("unreal"));
    Instance->SetStringField(TEXT("serviceId"), ServiceId);
    Instance->SetStringField(TEXT("machineId"), MachineId);
    Instance->SetStringField(TEXT("status"), TEXT("Available"));
    Instance->SetStringField(TEXT("color"), ColorHex);

    SetItem(TEXT("Instance"), Instance);

    for (URshipTargetComponent* Comp : *this->TargetComponents)
    {
        if (Comp && Comp->TargetData)
        {
            SendTarget(Comp->TargetData);
        }
    }
}

void URshipSubsystem::SendJson(const TSharedPtr<FJsonObject>& Payload)
{
    if (!Payload.IsValid())
    {
        return;
    }

    SendSerialized(Rship::Sdk::Serialize(Payload));
}

void URshipSubsystem::SetItem(const FString& itemType, const TSharedPtr<FJsonObject>& data)
{
    if (!data.IsValid())
    {
        return;
    }

    const TSharedPtr<FJsonObject> Wrapped = Rship::Sdk::WrapWSEvent(Rship::Sdk::MakeSet(itemType, data));
    FString Serialized = Rship::Sdk::Serialize(Wrapped);
    const uint32 PayloadHash = Rship::Sdk::CalculatePayloadHash(Serialized);

    FString DedupKey = itemType;
    FString Identifier;
    if (data->TryGetStringField(TEXT("id"), Identifier) || data->TryGetStringField(TEXT("targetId"), Identifier))
    {
        DedupKey += TEXT(":") + Identifier;
    }

    if (const uint32* ExistingHash = SentPayloadHashes.Find(DedupKey))
    {
        if (*ExistingHash == PayloadHash)
        {
            return;
        }
    }

    SentPayloadHashes.Add(DedupKey, PayloadHash);
    SendSerialized(MoveTemp(Serialized));
}

void URshipSubsystem::SendSerialized(FString&& Payload)
{
    if (!WebSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to send JSON object. Socket not initialized."));
        return;
    }

    if (!WebSocket->IsConnected())
    {
        WebSocket->Connect();
    }

    WebSocket->Send(Payload);
}

void URshipSubsystem::ResetPayloadDeduplication()
{
    SentPayloadHashes.Reset();
}

void URshipSubsystem::PulseEmitter(const FString& targetId, const FString& emitterId, const TSharedPtr<FJsonObject>& data)
{
    const FString FullEmitterId = targetId + TEXT(":") + emitterId;
    const TSharedPtr<FJsonObject> Wrapped = Rship::Sdk::WrapWSEvent(Rship::Sdk::MakePulse(FullEmitterId, data));
    SendSerialized(Rship::Sdk::Serialize(Wrapped));
}

EmitterContainer* URshipSubsystem::GetEmitterInfo(const FString& fullTargetId, const FString& emitterId)
{
    Target* target = nullptr;

    for (URshipTargetComponent* Comp : *this->TargetComponents)
    {
        if (Comp && Comp->TargetData && Comp->TargetData->GetId() == fullTargetId)
        {
            target = Comp->TargetData;
            break;
        }
    }

    if (target == nullptr)
    {
        return nullptr;
    }

    const FString FullEmitterId = fullTargetId + TEXT(":") + emitterId;
    const TMap<FString, EmitterContainer*>& Emitters = target->GetEmitters();

    if (!Emitters.Contains(FullEmitterId))
    {
        return nullptr;
    }

    return Emitters[FullEmitterId];
}



FString URshipSubsystem::GetServiceId() 
{
    return ServiceId;
}