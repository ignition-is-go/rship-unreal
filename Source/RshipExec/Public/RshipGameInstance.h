// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "IWebSocket.h"
#include "RshipTargetComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RshipGameInstance.generated.h"

using namespace std;

/**
 *
 */
UCLASS()
class URshipGameInstance : public UGameInstance
{
	GENERATED_BODY()
	TMap<FString, FActionCallBack> ActionMap;
	TMap<FString, FActionCallBackFloat> ActionFloatMap;
	TMap<FString, FActionCallBackString> ActionStringMap;
	TMap<FString, TSet<FString>> TargetActionMap;
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
	void SendAllTargetActions();
	void ProcessMessage(FString message);

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	void RegisterAction(FString targetId, FString actionId, FActionCallBack callback);
	void RegisterActionFloat(FString targetId, FString actionId, FActionCallBackFloat callback);
	void RegisterActionString(FString targetId, FString actionId, FActionCallBackString callback);
	void RegisterActionStringWithOptions(FString targetId, FString actionId, FActionCallBackString stringCallback, TArray<FString> options);

};
