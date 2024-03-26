// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EmitterHandler.h"
#include "RshipTargetComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URshipTargetComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual void OnRegister() override;

	virtual void OnComponentDestroyed(bool bDestoryHierarchy) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RshipTarget")
	void Reconnect();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RshipTarget")
	void Register();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RshipTarget")
	void Reset();

	UPROPERTY(EditAnywhere, config, Category = "RshipTarget", meta = (DisplayName = "Target Name"))
	FString targetId;

	TMap<FString, AEmitterHandler*> EmitterHandlers;

private: 
};
