// Fill out your copyright notice in the Description page of Project Settings.


#include "EmitterHandler.h"
#include "RshipSubsystem.h"
#include "Util.h"
#include "Engine/Engine.h"

// Sets default values
AEmitterHandler::AEmitterHandler()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void AEmitterHandler::ProcessEmitter(
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
)
{
	uint64 args[32] = {
		arg0,
		arg1,
		arg2,
		arg3,
		arg4,
		arg5,
		arg6,
		arg7,
		arg8,
		arg9,
		arg10,
		arg11,
		arg12,
		arg13,
		arg14,
		arg15,
		arg16,
		arg17,
		arg18,
		arg19,
		arg20,
		arg21,
		arg22,
		arg23,
		arg24,
		arg25,
		arg26,
		arg27,
		arg28,
		arg29,
		arg30,
		arg31
	};

	URshipSubsystem* subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

	EmitterContainer* emitter = subsystem->GetEmitterInfo(this->targetId, this->emitterId);

	if (this->targetId.IsEmpty() || this->emitterId.IsEmpty()) {
		return;
	}

	if (emitter == nullptr) {
		UE_LOG(LogTemp, Error, TEXT("EMITTER CANNOT PROCEED - Emitter not found: %s:%s"), *this->targetId, *this->emitterId);
		return;
	}

	auto props = emitter->GetProps();

	TSharedPtr<FJsonObject> json = MakeShareable(new FJsonObject);

	int argIndex = 0;

	for (const auto& prop : *props) {
		//for each arg, get the next mem address
		//cast to the right type of pointer
		//check for NULL
		//dereference the pointer to get the value

		if (prop.Type == "StrProperty") {
			FString strValue = "test string";
			json->SetStringField(prop.Name, strValue);
			// bump an extra one cuz strings are 2....
			argIndex++;
		}
		else if (prop.Type == "IntProperty") {
			int intValue = args[argIndex];
			json->SetNumberField(prop.Name, intValue);
		}
		else if (prop.Type == "BoolProperty") {
			bool boolValue = (bool)args[argIndex];
			json->SetBoolField(prop.Name, boolValue);
		}
		else if (prop.Type == "DoubleProperty") {

			int64 doubleBinary = args[argIndex];
			double doubleValue = *(double*)&doubleBinary;
			json->SetNumberField(prop.Name, doubleValue);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("EMITTER CANNOT PROCEED - Unknown Type: %s"), *prop.Type);
			return;
		}

		argIndex++;
	}

	subsystem->PulseEmitter(this->targetId, this->emitterId, json);

}

// Called when the game starts or when spawned
void AEmitterHandler::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AEmitterHandler::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEmitterHandler::SetServiceId(FString sid)
{
	this->serviceId = sid;
}

void AEmitterHandler::SetTargetId(FString tid)
{
	this->targetId = tid;
}

void AEmitterHandler::SetEmitterId(FString eid)
{
	this->emitterId = eid;
}

void AEmitterHandler::SetDelegate(FScriptDelegate* d)
{
	this->delegate = d;
}



