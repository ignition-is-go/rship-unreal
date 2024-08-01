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
#include "RshipSubsystem.generated.h"


DECLARE_DYNAMIC_DELEGATE(FRshipMessageDelegate);

/**
 *
 */
UCLASS()
class RSHIPEXEC_API URshipSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

	TMap<FString, Target*> AllTargets;
	TMap<FString, int> TargetCounts;

	AEmitterHandler *EmitterHandler;

	TSharedPtr<IWebSocket> WebSocket;
	FString InstanceId;
	FString ServiceId;
	FString MachineId;

	FString ClientId = "UNSET";
	FString ClusterId;

	void SetItem(FString itemType, TSharedPtr<FJsonObject> data);
	void SendJson(TSharedPtr<FJsonObject> Payload);
	void SendTarget(Target* target);
	void SendAction(Action* action, FString targetId);
	void SendEmitter(EmitterContainer* emitter, FString targetId);
	void SendTargetStatus(Target* target, bool online);
	void SendAll();
	void ProcessMessage(const FString& message);

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	void Reconnect();
	void Reset();

	void RegisterTarget(AActor *target, UWorld *world);
	void PulseEmitter(FString TargetId, FString EmitterId, TSharedPtr<FJsonObject> data);

	EmitterContainer* GetEmitterInfo(FString targetId, FString emitterId);

};
