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


using namespace std;

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    UE_LOG(LogTemp, Warning, TEXT("RshipSubsystem::Initialize"));
    Reconnect();

    auto world = GetWorld();    

    if (world != nullptr) {
        this->EmitterHandler = GetWorld()->SpawnActor<AEmitterHandler>();
    }
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
    InstanceId = FGuid::NewGuid().ToString();

    ClusterId = MachineId + ":" + ServiceId;

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

        if (commandId == "client:setId")
        {

            ClientId = command->GetStringField(TEXT("clientId"));
            UE_LOG(LogTemp, Warning, TEXT("Received ClientId %s"), *ClientId);
            SendAll();
            return;
        }

        if (commandId == "target:action:exec")
        {

            TSharedRef<FJsonObject> execAction = command->GetObjectField(TEXT("action")).ToSharedRef();
            TSharedRef<FJsonObject> execData = command->GetObjectField(TEXT("data")).ToSharedRef();

            FString actionId = execAction->GetStringField(TEXT("id"));

            FString targetId = execAction->GetStringField(TEXT("targetId"));

            if (!AllTargets.Contains(targetId))
            {
                UE_LOG(LogTemp, Warning, TEXT("Target Not Found: [%s]"), *targetId);
                return;
            }

            Target *target = AllTargets[targetId];

            if (target != nullptr)
            {
                target->TakeAction(actionId, execData);
            }
            // command.Reset();
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
    Action->SetStringField(TEXT("name"), action->GetId());
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
	Emitter->SetStringField(TEXT("name"), emitter->GetId());
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

void URshipSubsystem::RegisterTarget(AActor *actor, UWorld *world)
{

    URshipTargetComponent *targetComponent = actor->GetComponentByClass<URshipTargetComponent>();

    FString targetId = ServiceId + ":" + targetComponent->targetId;

    if (!AllTargets.Contains(targetId))
    {
        AllTargets.Add(targetId, new Target(targetId));
    }

    Target *target = AllTargets[targetId];

    UClass *ownerClass = actor->GetClass();

    for (TFieldIterator<UFunction> field(ownerClass, EFieldIteratorFlags::ExcludeSuper); field; ++field)
    {
        UFunction *handler = *field;

        FString name = field->GetName();

        if (!name.StartsWith("RS_"))
        {
            continue;
        }
        FString fullActionId = targetId + ":" + name;

        auto actions = target->GetActions();
 
        if (actions.Contains(fullActionId))
        {
            auto action = target->GetActions()[fullActionId];
            action->AddParent(actor);
            action->UpdateSchema(handler);
        }
        else
        {
            auto action = new Action(fullActionId, handler);
            action->AddParent(actor);
            target->AddAction(action);
        }
    }

    for (TFieldIterator<FMulticastInlineDelegateProperty> It(ownerClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
		FMulticastInlineDelegateProperty* EmitterProp = *It;
		FString EmitterName = EmitterProp->GetName();
		FName EmitterType = EmitterProp->GetClass()->GetFName();

		UE_LOG(LogTemp, Warning, TEXT("Emitter: %s, Type: %s"), *EmitterName, *EmitterType.ToString());

        if (!EmitterName.StartsWith("RS_"))
        {
			continue;
		}

        auto emitters = target->GetEmitters();

        FString fullEmitterId = targetId + ":" + EmitterName;

        if (emitters.Contains(fullEmitterId))
        {
            auto emitter = emitters[fullEmitterId];
            emitter->UpdateSchema(EmitterProp);
        }
        else {
			auto emitter = new EmitterContainer(fullEmitterId, EmitterProp);
            target->AddEmitter(emitter);
        }

        FMulticastScriptDelegate EmitterDelegate = EmitterProp->GetPropertyValue_InContainer(actor);

        TScriptDelegate localDelegate;
     
         if (!world) {
			 UE_LOG(LogTemp, Warning, TEXT("World Not Found"));
			 return;
		 }




         FActorSpawnParameters spawnInfo;
         spawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
         spawnInfo.Owner = actor;
         spawnInfo.bNoFail = true;
         spawnInfo.bDeferConstruction = false;
         spawnInfo.bAllowDuringConstructionScript = true;

         
         if (targetComponent->EmitterHandlers.Contains(EmitterName)) {
             return;
         }

         AEmitterHandler* handler =  world->SpawnActor<AEmitterHandler>(spawnInfo);

         handler->SetActorLabel(actor->GetActorLabel() + " " + EmitterName + " Handler");

         handler->SetServiceId(ServiceId);
         handler->SetTargetId(targetId);
         handler->SetEmitterId(EmitterName);
         handler->SetDelegate(&localDelegate);

         localDelegate.BindUFunction(handler, TEXT("ProcessEmitter"));

         EmitterDelegate.Add(localDelegate);

         EmitterProp->SetPropertyValue_InContainer(actor, EmitterDelegate);

         targetComponent->EmitterHandlers.Add(EmitterName, handler);

	}

    AllTargets.Add(targetId, target);
    SendAll();
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
    if (!AllTargets.Contains(fullTargetId))
    {
		return nullptr;
	}

    auto target = AllTargets[fullTargetId];

    FString fullEmitterId = fullTargetId + ":" + emitterId;

    auto emitters = target->GetEmitters();

    if (!emitters.Contains(fullEmitterId))
    {
        return nullptr;
	}

    return emitters[fullEmitterId];

}

void URshipSubsystem::Reset()
{
    AllTargets.Reset();
}
