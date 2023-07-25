// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipTargetComponent.generated.h"

DECLARE_DYNAMIC_DELEGATE(FActionCallBack);

DECLARE_DYNAMIC_DELEGATE_OneParam(FActionCallBackFloat, float, Data);

DECLARE_DYNAMIC_DELEGATE_OneParam(FActionCallBackString, FString, Data);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URshipTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URshipTargetComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, meta=(Category="RShip"))
	void BindAction(FActionCallBack callback, FString actionId);

	UFUNCTION(BlueprintCallable, meta=(Category="RShip"))
	void BindActionFloat(FActionCallBackFloat floatCallback, FString actionId);

	UFUNCTION(BlueprintCallable, meta=(Category="RShip"))
	void BindActionString(FActionCallBackString stringCallback, FString actionId);

	UFUNCTION(BlueprintCallable, meta=(Category="RShip"))
	void BindActionStringWithOptions(FActionCallBackString callbackWithString, FString actionId,TArray<FString> options);
};
