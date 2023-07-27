// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RshipGameInstance.h"
#include "GameFramework/Actor.h"
#include "LevelReporter.generated.h"

UCLASS()
class RSHIPEXEC_API ALevelReporter : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALevelReporter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	URshipGameInstance *GameInstance;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
