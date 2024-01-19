// Fill out your copyright notice in the Description page of Project Settings.

#include "RshipTargetComponent.h"
#include <iostream>
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RshipSubsystem.h"
#include "GameFramework/Actor.h"
#include "Misc/DefaultValueHelper.h"

using namespace std;

// Sets default values for this component's properties
void URshipTargetComponent::OnRegister()
{

	Super::OnRegister();
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	Register();
}

// Called every frame
void URshipTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URshipTargetComponent::Reconnect()
{
	URshipSubsystem *subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	subsystem->Reconnect();
}

void URshipTargetComponent::Reset()
{
	URshipSubsystem* subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	subsystem->Reset();
}


void URshipTargetComponent::Register()
{

	URshipSubsystem *subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

	AActor *parent = GetOwner();

	if (!parent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Parent not found"));
	}

	subsystem->RegisterTarget(parent);
}