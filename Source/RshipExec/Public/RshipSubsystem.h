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

/**
 *
 */
UCLASS()
class RSHIPEXEC_API URshipSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

	TMap<FString, Target *> AllTargets;

	TMap<FString, TSharedPtr<FJsonObject>> ActionSchemasJson;
	TSharedPtr<IWebSocket> WebSocket;
	FString RunId;
	FString ServiceId;
	FString MachineId;

	FString ClientId;
	FString ClusterId;

	void SetItem(FString itemType, TSharedPtr<FJsonObject> data);
	void SendJson(TSharedPtr<FJsonObject> Payload);
	void GenerateSchemas();
	void SendTarget(Target* target);
	void SendAction(Action* action, FString targetId);
	void SendTargetStatus(Target* target, bool online);
	void SendAll();
	void ScanActors();
	void ProcessMessage(FString message);

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	void Reconnect();
	void Reset();

	void RegisterTarget(AActor *target);

	void RegisterEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> schema);
	void PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data);
};
