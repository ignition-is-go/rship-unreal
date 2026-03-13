// Fill out your copyright notice in the Description page of Project Settings.

#include "LevelReporter.h"
// #include "RshipGameInstance.h"
#include "EngineUtils.h"

// Sets default values
ALevelReporter::ALevelReporter()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ALevelReporter::BeginPlay()
{
	Super::BeginPlay();

	// while (!GameInstance)
	// {
	// 	GameInstance = Cast<URshipGameInstance>(GetWorld()->GetGameInstance());
	// }

	// TSharedPtr<FJsonObject> VectorSchema = MakeShareable(new FJsonObject());
	// VectorSchema->SetStringField("$schema", "http://json-schema.org/draft-07/schema#");
	// VectorSchema->SetStringField("type", "object");
	// TSharedPtr<FJsonObject> VectorProperties = MakeShareable(new FJsonObject());
	// VectorProperties->SetStringField("x", "number");
	// VectorProperties->SetStringField("y", "number");
	// VectorProperties->SetStringField("z", "number");
	// VectorSchema->SetObjectField("properties", VectorProperties);

	// for(TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	// {
	//     AActor *Actor = *ActorItr;
	//     FString ActorName = Actor->GetName();

	// 	// position
	// 	GameInstance->RegisterEmitter(ActorName, "Location", VectorSchema);

	// 	// rotation
	// 	GameInstance->RegisterEmitter(ActorName, "Rotation", VectorSchema);

	// 	// scale
	// 	GameInstance->RegisterEmitter(ActorName, "Scale", VectorSchema);
	// }

	// actor name

	// vector schema
	// {
	// '$schema': 'http://json-schema.org/draft-07/schema#',
	// type: 'object',
	// properties: {
	// 	x: { type: 'number' },
	// 	y: { type: 'number' },
	// 	z: { type: 'number' }
	// }
	// }
}

// Called every frame
void ALevelReporter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// auto timestamp = FDateTime::Now().ToUnixTimestamp();

	// TSharedPtr<FJsonObject> ActorLocationJson = MakeShareable(new FJsonObject());
	// TSharedPtr<FJsonObject> ActorRotationJson = MakeShareable(new FJsonObject());
	// TSharedPtr<FJsonObject> ActorScaleJson = MakeShareable(new FJsonObject());

	// for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	// {
	//     // Same as with the Object Iterator, access the subclass instance with the * or -> operators.
	//     AActor *Actor = *ActorItr;
	//     FString ActorName = Actor->GetName();

	//     // position
	//     FVector ActorLocation = Actor->GetActorLocation();
	//     FString ActorLocationString = ActorLocation.ToString();

	// 	ActorLocationJson->SetNumberField("x", ActorLocation.X);
	// 	ActorLocationJson->SetNumberField("y", ActorLocation.Y);
	// 	ActorLocationJson->SetNumberField("z", ActorLocation.Z);

	//     // rotation
	//     FRotator ActorRotation = Actor->GetActorRotation();
	//     FString ActorRotationString = ActorRotation.ToString();

	// 	ActorRotationJson->SetNumberField("x", ActorRotation.Pitch);
	// 	ActorRotationJson->SetNumberField("y", ActorRotation.Yaw);
	// 	ActorRotationJson->SetNumberField("z", ActorRotation.Roll);

	//     // scale
	//     FVector ActorScale = Actor->GetActorScale();
	//     FString ActorScaleString = ActorScale.ToString();

	// 	ActorScaleJson->SetNumberField("x", ActorScale.X);
	// 	ActorScaleJson->SetNumberField("y", ActorScale.Y);
	// 	ActorScaleJson->SetNumberField("z", ActorScale.Z);

	// 	GameInstance->PulseEmitter(ActorName, "Location", ActorLocationJson);
	// 	GameInstance->PulseEmitter(ActorName, "Rotation", ActorRotationJson);
	// 	GameInstance->PulseEmitter(ActorName, "Scale", ActorScaleJson);
	// }
}
