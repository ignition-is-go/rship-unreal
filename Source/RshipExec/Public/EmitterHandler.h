// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmitterHandler.generated.h"

UCLASS()
class RSHIPEXEC_API AEmitterHandler : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEmitterHandler();

	// Called every frame
	virtual void Tick(float DeltaTime) override;


	UFUNCTION()
	void ProcessEmitter(
		uint64 arg0,
		uint64 arg1,
		uint64 arg2,
		uint64 arg3,
		uint64 arg4,
		uint64 arg5,
		uint64 arg6,
		uint64 arg7,
		uint64 arg8,
		uint64 arg9,
		uint64 arg10,
		uint64 arg11,
		uint64 arg12,
		uint64 arg13,
		uint64 arg14,
		uint64 arg15,
		uint64 arg16,
		uint64 arg17,
		uint64 arg18,
		uint64 arg19,
		uint64 arg20,
		uint64 arg21,
		uint64 arg22,
		uint64 arg23,
		uint64 arg24,
		uint64 arg25,
		uint64 arg26,
		uint64 arg27,
		uint64 arg28,
		uint64 arg29,
		uint64 arg30,
		uint64 arg31
	);

	void SetServiceId(FString serviceId);
	void SetTargetId(FString targetId);
	void SetEmitterId(FString emitterId);
	void SetDelegate(FScriptDelegate* delegate);


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:

	FString serviceId;
	FString targetId;
	FString emitterId;
	FScriptDelegate* delegate;
};
