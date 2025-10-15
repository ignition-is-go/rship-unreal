// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "IWebSocket.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RshipTargetComponent.h"
#include "GameFramework/Actor.h"
#include "Containers/List.h"
#include "Target.h"
#include "Containers/Map.h"
#include "RshipSubsystem.generated.h"


DECLARE_DYNAMIC_DELEGATE(FRshipMessageDelegate);

/**
 *
 */
UCLASS()
class RSHIPEXEC_API URshipSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

	AEmitterHandler *EmitterHandler;

	TSharedPtr<IWebSocket> WebSocket;
	FString InstanceId;
	FString ServiceId;
	FString MachineId;

        FString ClientId = "UNSET";
        FString ClusterId;
        TMap<FString, uint32> SentPayloadHashes;

        void SetItem(const FString& itemType, const TSharedPtr<FJsonObject>& data);
        void SendJson(const TSharedPtr<FJsonObject>& Payload);
        void SendSerialized(FString&& Payload);
        void ResetPayloadDeduplication();
        void SendTarget(Target* target);
        void SendAction(Action* action, const FString& targetId);
        void SendEmitter(EmitterContainer* emitter, const FString& targetId);
        void SendTargetStatus(Target* target, bool online);
        void ProcessMessage(const FString& message);

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

        void Reconnect();
        void PulseEmitter(const FString& TargetId, const FString& EmitterId, const TSharedPtr<FJsonObject>& data);
        void SendAll();

        EmitterContainer* GetEmitterInfo(const FString& targetId, const FString& emitterId);

	TSet<URshipTargetComponent*>* TargetComponents;

	FString GetServiceId();
};
