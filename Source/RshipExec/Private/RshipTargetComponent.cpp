// Fill out your copyright notice in the Description page of Project Settings.

#include "RshipTargetComponent.h"
#include <iostream>
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RshipGameInstance.h"
#include "Misc/DefaultValueHelper.h"

using namespace std;

// Sets default values for this component's properties
URshipTargetComponent::URshipTargetComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

// Called when the game starts
void URshipTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	URshipGameInstance *GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	UE_LOG(LogTemp, Warning, TEXT("Begin"))
	while (!GameInstance)
	{
		GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	}

	// ...
}

// Called every frame
void URshipTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void URshipTargetComponent::BindAction(FActionCallBack callback, FString actionId)
{

	URshipGameInstance *GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	while (!GameInstance)
	{
		GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	}
	if (GameInstance)
	{
		FString name = GetName();

		GameInstance->RegisterAction(name, actionId, callback);
	}
}

void URshipTargetComponent::BindActionFloat(FActionCallBackFloat callbackWithFloat, FString actionId)
{
	URshipGameInstance *GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	while (!GameInstance)
	{
		GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	}
	if (GameInstance)
	{
		FString name = GetName();

		GameInstance->RegisterActionFloat(name, actionId, callbackWithFloat);
	}
}

void URshipTargetComponent::BindActionString(FActionCallBackString callbackWithString, FString actionId)
{
	URshipGameInstance *GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	while (!GameInstance)
	{
		GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	}
	if (GameInstance)
	{
		FString name = GetName();

		GameInstance->RegisterActionString(name, actionId, callbackWithString);
	}
}

void URshipTargetComponent::BindActionStringWithOptions(FActionCallBackString callbackWithString, FString actionId,TArray<FString> options)
{
	URshipGameInstance *GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	while (!GameInstance)
	{
		GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	}
	if (GameInstance)
	{
		FString name = GetName();

		GameInstance->RegisterActionStringWithOptions(name, actionId, callbackWithString, options);
	}
}