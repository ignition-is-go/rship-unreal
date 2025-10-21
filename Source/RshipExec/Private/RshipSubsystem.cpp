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
#include "Engine/Engine.h"
#include "UObject/FieldPath.h"
#include "Misc/StructBuilder.h"
#include "UObject/UnrealTypePrivate.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Action.h"
#include "Target.h"

#include "Myko.h"
#include "EmitterHandler.h"


using namespace std;

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    UE_LOG(LogTemp, Warning, TEXT("RshipSubsystem::Initialize"));
    Reconnect();

    auto world = GetWorld();    

    if (world != nullptr) {
        this->EmitterHandler = GetWorld()->SpawnActor<AEmitterHandler>();
    }

    this->TargetComponents = new TSet<URshipTargetComponent*>;

    ConfigureFrameSyncFromSettings();
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

    if (rshipHostAddress.IsEmpty() || rshipHostAddress.Len() == 0)
    {
        rshipHostAddress = FString("localhost");
    }

    if (WebSocket)
    {
        WebSocket->Close();
    }

    WebSocket = FWebSocketsModule::Get().CreateWebSocket("ws://" + rshipHostAddress + ":5155/myko");

    WebSocket->OnConnected().AddLambda([&]()
                                       {
                                           UE_LOG(LogTemp, Warning, TEXT("Connected"));
                                           SendAll();
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
                                          //UE_LOG(LogTemp, Warning, TEXT("Message %s"), *MessageString);
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
        else if (commandId == "SubmitPTPTimestamp")
        {
            if (!GEngine)
            {
                return;
            }

            URshipFrameSyncSubsystem* FrameSyncSubsystem = GEngine->GetEngineSubsystem<URshipFrameSyncSubsystem>();
            if (!FrameSyncSubsystem)
            {
                return;
            }

            TSharedPtr<FJsonObject> TimestampObj;
            if (command->TryGetObjectField(TEXT("timestamp"), TimestampObj))
            {
                FRshipPTPTimestamp Timestamp;
                double SecondsValue;
                double NanosecondsValue;
                double FrameValue;

                if (TimestampObj->TryGetNumberField(TEXT("seconds"), SecondsValue))
                {
                    Timestamp.Seconds = static_cast<int64>(SecondsValue);
                }
                if (TimestampObj->TryGetNumberField(TEXT("nanoseconds"), NanosecondsValue))
                {
                    Timestamp.Nanoseconds = static_cast<int32>(NanosecondsValue);
                }
                if (TimestampObj->TryGetNumberField(TEXT("frameNumber"), FrameValue))
                {
                    Timestamp.FrameNumber = static_cast<int64>(FrameValue);
                }

                FrameSyncSubsystem->PushPTPTimestamp(Timestamp);
                SendFrameSyncStatus();
            }
        }
        else if (commandId == "RequestFrameSyncStatus")
        {
            SendFrameSyncStatus();
        }
        else if (commandId == "RequestTargetSnapshot")
        {
            SendTargetSnapshotEvent();
        }
        else if (commandId == "ResetFrameSyncHistory")
        {
            if (GEngine)
            {
                if (URshipFrameSyncSubsystem* FrameSyncSubsystem = GEngine->GetEngineSubsystem<URshipFrameSyncSubsystem>())
                {
                    FrameSyncSubsystem->ResetFrameHistory();
                }
            }
            SendFrameSyncStatus();
        }
        obj.Reset();
    }
}

void URshipSubsystem::Deinitialize()
{

    UE_LOG(LogTemp, Warning, TEXT("RshipSubsystem::Deinitialize"));

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

    for (auto& Elem : target->GetEmitters())
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

void URshipSubsystem::SendEmitter(EmitterContainer* emitter, FString targetId)
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

void URshipSubsystem::SendFrameSyncStatus()
{
    if (!GEngine)
    {
        return;
    }

    URshipFrameSyncSubsystem* FrameSyncSubsystem = GEngine->GetEngineSubsystem<URshipFrameSyncSubsystem>();
    if (!FrameSyncSubsystem)
    {
        return;
    }

    FRshipFrameSyncStatus Status = FrameSyncSubsystem->GetFrameSyncStatus();

    TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
    Payload->SetStringField(TEXT("id"), InstanceId + TEXT(":FrameSync"));
    Payload->SetStringField(TEXT("serviceId"), ServiceId);
    Payload->SetStringField(TEXT("instanceId"), InstanceId);
    Payload->SetBoolField(TEXT("isLocked"), Status.bIsLocked);
    Payload->SetNumberField(TEXT("driftMicros"), Status.DriftMicroseconds);
    Payload->SetNumberField(TEXT("referenceFrameNumber"), static_cast<double>(Status.ReferenceFrameNumber));
    Payload->SetNumberField(TEXT("referencePTPSeconds"), Status.ReferencePTPTimeSeconds);
    Payload->SetNumberField(TEXT("lastTimestampSeconds"), static_cast<double>(Status.LastTimestamp.Seconds));
    Payload->SetNumberField(TEXT("lastTimestampNanoseconds"), Status.LastTimestamp.Nanoseconds);
    Payload->SetNumberField(TEXT("lastTimestampFrame"), static_cast<double>(Status.LastTimestamp.FrameNumber));

    if (Status.RecentHistory.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> HistoryArray;
        for (const FRshipFrameTimingRecord& Record : Status.RecentHistory)
        {
            TSharedPtr<FJsonObject> RecordObj = MakeShareable(new FJsonObject);
            RecordObj->SetNumberField(TEXT("frameNumber"), static_cast<double>(Record.FrameNumber));
            RecordObj->SetNumberField(TEXT("localFrameStartSeconds"), Record.LocalFrameStartSeconds);
            RecordObj->SetNumberField(TEXT("expectedFrameStartSeconds"), Record.ExpectedFrameStartSeconds);
            RecordObj->SetNumberField(TEXT("errorMicros"), Record.ErrorMicroseconds);
            HistoryArray.Add(MakeShareable(new FJsonValueObject(RecordObj)));
        }
        Payload->SetArrayField(TEXT("history"), HistoryArray);
    }

    SetItem(TEXT("FrameSyncStatus"), Payload);
}

void URshipSubsystem::SendTargetSnapshotEvent()
{
    const TArray<FRshipTargetDescription> Snapshot = GetTargetSnapshot();

    TArray<TSharedPtr<FJsonValue>> TargetsJson;
    for (const FRshipTargetDescription& TargetDesc : Snapshot)
    {
        TSharedPtr<FJsonObject> TargetObj = MakeShareable(new FJsonObject);
        TargetObj->SetStringField(TEXT("id"), TargetDesc.TargetId);
        TargetObj->SetStringField(TEXT("name"), TargetDesc.TargetName);

        TArray<TSharedPtr<FJsonValue>> ActionsJson;
        for (const FRshipActionDescription& ActionDesc : TargetDesc.Actions)
        {
            TSharedPtr<FJsonObject> ActionObj = MakeShareable(new FJsonObject);
            ActionObj->SetStringField(TEXT("id"), ActionDesc.ActionId);
            ActionObj->SetStringField(TEXT("displayName"), ActionDesc.DisplayName);
            ActionObj->SetStringField(TEXT("function"), ActionDesc.FunctionName);

            TArray<TSharedPtr<FJsonValue>> ParamsJson;
            for (const FRshipSchemaField& Field : ActionDesc.Parameters)
            {
                TSharedPtr<FJsonObject> FieldObj = MakeShareable(new FJsonObject);
                FieldObj->SetStringField(TEXT("name"), Field.Name);
                FieldObj->SetStringField(TEXT("type"), Field.Type);
                ParamsJson.Add(MakeShareable(new FJsonValueObject(FieldObj)));
            }
            ActionObj->SetArrayField(TEXT("parameters"), ParamsJson);
            ActionsJson.Add(MakeShareable(new FJsonValueObject(ActionObj)));
        }
        TargetObj->SetArrayField(TEXT("actions"), ActionsJson);

        TArray<TSharedPtr<FJsonValue>> EmittersJson;
        for (const FRshipEmitterDescription& EmitterDesc : TargetDesc.Emitters)
        {
            TSharedPtr<FJsonObject> EmitterObj = MakeShareable(new FJsonObject);
            EmitterObj->SetStringField(TEXT("id"), EmitterDesc.EmitterId);
            EmitterObj->SetStringField(TEXT("displayName"), EmitterDesc.DisplayName);

            TArray<TSharedPtr<FJsonValue>> PayloadJson;
            for (const FRshipSchemaField& Field : EmitterDesc.Payload)
            {
                TSharedPtr<FJsonObject> FieldObj = MakeShareable(new FJsonObject);
                FieldObj->SetStringField(TEXT("name"), Field.Name);
                FieldObj->SetStringField(TEXT("type"), Field.Type);
                PayloadJson.Add(MakeShareable(new FJsonValueObject(FieldObj)));
            }
            EmitterObj->SetArrayField(TEXT("payload"), PayloadJson);
            EmittersJson.Add(MakeShareable(new FJsonValueObject(EmitterObj)));
        }
        TargetObj->SetArrayField(TEXT("emitters"), EmittersJson);

        TargetsJson.Add(MakeShareable(new FJsonValueObject(TargetObj)));
    }

    TSharedPtr<FJsonObject> Envelope = MakeShareable(new FJsonObject);
    Envelope->SetStringField(TEXT("id"), InstanceId + TEXT(":TargetSnapshot"));
    Envelope->SetStringField(TEXT("serviceId"), ServiceId);
    Envelope->SetArrayField(TEXT("targets"), TargetsJson);

    SetItem(TEXT("TargetSnapshot"), Envelope);
}

void URshipSubsystem::ConfigureFrameSyncFromSettings()
{
    if (!GEngine)
    {
        return;
    }

    URshipFrameSyncSubsystem* FrameSyncSubsystem = GEngine->GetEngineSubsystem<URshipFrameSyncSubsystem>();
    if (!FrameSyncSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("Frame sync subsystem not available"));
        return;
    }

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (!Settings)
    {
        return;
    }

    FRshipFrameSyncConfig FrameConfig;
    FrameConfig.bUseFixedFrameRate = Settings->bEnableFrameSync;
    FrameConfig.ExpectedFrameRate = Settings->TargetFrameRate;
    FrameConfig.AllowableDriftMicroseconds = Settings->AllowableDriftMicroseconds;
    FrameConfig.HistorySize = Settings->FrameHistoryLength;
    FrameConfig.bRecordHistory = Settings->FrameHistoryLength > 0;

    FrameSyncSubsystem->Configure(FrameConfig);
    SendFrameSyncStatus();
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

    for (auto& comp : *this->TargetComponents) {
        SendTarget(comp->TargetData);
    }

    SendFrameSyncStatus();
    SendTargetSnapshotEvent();
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


void URshipSubsystem::PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data)
{

    TSharedPtr<FJsonObject> pulse = MakeShareable(new FJsonObject); 

    FString fullEmitterId =  targetId + ":" + emitterId;   

    pulse->SetStringField("emitterId", fullEmitterId);
    pulse->SetStringField("id", fullEmitterId);

    pulse->SetObjectField("data", data);

    this->SetItem("Pulse", pulse);
}

EmitterContainer* URshipSubsystem::GetEmitterInfo(FString fullTargetId, FString emitterId)
{
    Target* target = nullptr;

    for (auto& comp : *this->TargetComponents) {
        if (comp->TargetData->GetId() ==  fullTargetId) {
            target = comp->TargetData;
            break;
        }
    }

    if (target == nullptr) {
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

TArray<FRshipTargetDescription> URshipSubsystem::GetTargetSnapshot() const
{
    TArray<FRshipTargetDescription> Snapshot;

    if (!TargetComponents)
    {
        return Snapshot;
    }

    for (URshipTargetComponent* Component : *TargetComponents)
    {
        if (!Component)
        {
            continue;
        }

        Snapshot.Add(Component->GetTargetDescription());
    }

    return Snapshot;
}

void URshipSubsystem::DumpTargetSnapshotToLog() const
{
    const TArray<FRshipTargetDescription> Snapshot = GetTargetSnapshot();

    if (Snapshot.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No Rship targets registered."));
        return;
    }

    for (const FRshipTargetDescription& TargetDesc : Snapshot)
    {
        if (TargetDesc.TargetId == TargetDesc.TargetName)
        {
            UE_LOG(LogTemp, Display, TEXT("Target %s"), *TargetDesc.TargetId);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Target %s (%s)"), *TargetDesc.TargetId, *TargetDesc.TargetName);
        }

        for (const FRshipActionDescription& ActionDesc : TargetDesc.Actions)
        {
            FString Params;
            for (const FRshipSchemaField& Field : ActionDesc.Parameters)
            {
                Params += FString::Printf(TEXT("%s(%s) "), *Field.Name, *Field.Type);
            }
            UE_LOG(LogTemp, Display, TEXT("  Action %s -> %s"), *ActionDesc.ActionId, *Params);
        }

        for (const FRshipEmitterDescription& EmitterDesc : TargetDesc.Emitters)
        {
            FString Payload;
            for (const FRshipSchemaField& Field : EmitterDesc.Payload)
            {
                Payload += FString::Printf(TEXT("%s(%s) "), *Field.Name, *Field.Type);
            }
            UE_LOG(LogTemp, Display, TEXT("  Emitter %s -> %s"), *EmitterDesc.EmitterId, *Payload);
        }
    }
}

FString URshipSubsystem::GetFrameSyncReportJson() const
{
    if (!GEngine)
    {
        return TEXT("{}");
    }

    const URshipFrameSyncSubsystem* FrameSyncSubsystem = GEngine->GetEngineSubsystem<URshipFrameSyncSubsystem>();
    if (!FrameSyncSubsystem)
    {
        return TEXT("{}");
    }

    const FRshipFrameSyncStatus Status = FrameSyncSubsystem->GetFrameSyncStatus();

    TSharedPtr<FJsonObject> Report = MakeShareable(new FJsonObject);
    Report->SetBoolField(TEXT("isLocked"), Status.bIsLocked);
    Report->SetNumberField(TEXT("driftMicros"), Status.DriftMicroseconds);
    Report->SetNumberField(TEXT("referenceFrameNumber"), static_cast<double>(Status.ReferenceFrameNumber));
    Report->SetNumberField(TEXT("referencePTPSeconds"), Status.ReferencePTPTimeSeconds);
    Report->SetNumberField(TEXT("lastTimestampSeconds"), static_cast<double>(Status.LastTimestamp.Seconds));
    Report->SetNumberField(TEXT("lastTimestampNanoseconds"), Status.LastTimestamp.Nanoseconds);
    Report->SetNumberField(TEXT("lastTimestampFrame"), static_cast<double>(Status.LastTimestamp.FrameNumber));

    if (Status.RecentHistory.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> HistoryArray;
        for (const FRshipFrameTimingRecord& Record : Status.RecentHistory)
        {
            TSharedPtr<FJsonObject> RecordObj = MakeShareable(new FJsonObject);
            RecordObj->SetNumberField(TEXT("frameNumber"), static_cast<double>(Record.FrameNumber));
            RecordObj->SetNumberField(TEXT("localFrameStartSeconds"), Record.LocalFrameStartSeconds);
            RecordObj->SetNumberField(TEXT("expectedFrameStartSeconds"), Record.ExpectedFrameStartSeconds);
            RecordObj->SetNumberField(TEXT("errorMicros"), Record.ErrorMicroseconds);
            HistoryArray.Add(MakeShareable(new FJsonValueObject(RecordObj)));
        }
        Report->SetArrayField(TEXT("history"), HistoryArray);
    }

    return GetJsonString(Report);
}

void URshipSubsystem::BroadcastFrameSyncStatus()
{
    SendFrameSyncStatus();
}