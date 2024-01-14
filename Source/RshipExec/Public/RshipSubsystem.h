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
#include "RshipSubsystem.generated.h"

struct RshipActionProperty
{
	FString Name;
	FString Type;
};

/**
 *
 */
UCLASS()
class RSHIPEXEC_API URshipSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
	TMap<FString, UFunction *> ActionMap;
	TMap<FString, AActor *> TargetMap;
	TMap<FString, TDoubleLinkedList<RshipActionProperty> *> PropMap;

	TSet<FString> RegisteredTargets;
	TMap<FString, TSet<FString> *> TargetActionMap;
	TMap<FString, TSet<FString> *> TargetEmitterMap;
	TMap<FString, TSharedPtr<FJsonObject>> TargetSchemas;
	TMap<FString, TSharedPtr<FJsonObject>> EmitterSchemas;
	TMap<FString, UObject *> UObjectsByActionId;

	TMap<FString, FString> ActionSchemas;
	TMap<FString, TSharedPtr<FJsonObject>> ActionSchemasJson;
	TSharedPtr<IWebSocket> WebSocket;
	FString RunId;
	FString ServiceId;
	FString MachineId;

	FString ClientId;
	FString ClusterId;

	void SetItem(FString itemType, TSharedPtr<FJsonObject> data);
	void SendJson(TSharedPtr<FJsonObject> Payload);
	void SendAll();
	void ProcessMessage(FString message);
	void RegisterAction(FString targetId, AActor *target, UFunction *action);

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	void RegisterTarget(AActor *target);
	//

	void RegisterEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> schema);
	void PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data);
};
