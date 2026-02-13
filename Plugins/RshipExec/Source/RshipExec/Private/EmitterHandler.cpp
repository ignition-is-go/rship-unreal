// Fill out your copyright notice in the Description page of Project Settings.

#include "EmitterHandler.h"
#include "RshipSubsystem.h"
#include "Util.h"
#include "Engine/Engine.h"

// Helpers to decode emitter argument slots into JSON based on SchemaNode
static TSharedPtr<FJsonValue> ExtractValueFromArgs(const SchemaNode &Node, const uint64 *Args, int &Index)
{
	if (Index >= 32)
	{
		return TSharedPtr<FJsonValue>();
	}
	// Recursively build an FJsonValue matching the SchemaNode type using the raw slots
	if (Node.Type == TEXT("StructProperty"))
	{
		TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
		for (const SchemaNode &Child : Node.Children)
		{
			TSharedPtr<FJsonValue> ChildVal = ExtractValueFromArgs(Child, Args, Index);
			if (ChildVal.IsValid())
			{
				// Assign by type
				switch (ChildVal->Type)
				{
				case EJson::String:
					Obj->SetStringField(Child.Name, ChildVal->AsString());
					break;
				case EJson::Number:
					Obj->SetNumberField(Child.Name, ChildVal->AsNumber());
					break;
				case EJson::Boolean:
					Obj->SetBoolField(Child.Name, ChildVal->AsBool());
					break;
				case EJson::Object:
					Obj->SetObjectField(Child.Name, ChildVal->AsObject());
					break;
				default:
					break;
				}
			}
		}
		return MakeShareable(new FJsonValueObject(Obj));
	}

	// Primitive decoding from one slot, best-effort
	if (Node.Type == TEXT("IntProperty"))
	{
		int64 v = static_cast<int64>(Args[Index++]);
		return MakeShareable(new FJsonValueNumber(static_cast<double>(v)));
	}
	if (Node.Type == TEXT("UIntProperty") || Node.Type == TEXT("UInt32Property"))
	{
		uint64 v = Args[Index++];
		return MakeShareable(new FJsonValueNumber(static_cast<double>(static_cast<uint32>(v))));
	}
	if (Node.Type == TEXT("Int64Property"))
	{
		int64 v = static_cast<int64>(Args[Index++]);
		return MakeShareable(new FJsonValueNumber(static_cast<double>(v)));
	}
	if (Node.Type == TEXT("UInt64Property"))
	{
		uint64 v = Args[Index++];
		return MakeShareable(new FJsonValueNumber(static_cast<double>(v)));
	}
	if (Node.Type == TEXT("ByteProperty"))
	{
		uint8 v = static_cast<uint8>(Args[Index++] & 0xFF);
		return MakeShareable(new FJsonValueNumber(static_cast<double>(v)));
	}
	if (Node.Type == TEXT("BoolProperty"))
	{
		bool b = static_cast<bool>(Args[Index++]);
		return MakeShareable(new FJsonValueBoolean(b));
	}
	if (Node.Type == TEXT("FloatProperty"))
	{
		uint64 raw = Args[Index++];
		uint32 low = static_cast<uint32>(raw & 0xFFFFFFFFu);
		float f;
		FMemory::Memcpy(&f, &low, sizeof(float));
		return MakeShareable(new FJsonValueNumber(static_cast<double>(f)));
	}
	if (Node.Type == TEXT("DoubleProperty"))
	{
		int64 raw = static_cast<int64>(Args[Index++]);
		double d = *(reinterpret_cast<double *>(&raw));
		return MakeShareable(new FJsonValueNumber(d));
	}
	if (Node.Type == TEXT("StrProperty"))
	{
		uint64 ptrVal = Args[Index++];
		FString out;
		if (ptrVal != 0)
		{
			const FString* pStr = reinterpret_cast<const FString*>(ptrVal);
			// Best-effort: pointer assumed valid during this call frame
			out = *pStr;
		}
		return MakeShareable(new FJsonValueString(out));
	}
	if (Node.Type == TEXT("NameProperty"))
	{
		uint64 ptrVal = Args[Index++];
		FString out;
		if (ptrVal != 0)
		{
			const FName* pName = reinterpret_cast<const FName*>(ptrVal);
			out = pName->ToString();
		}
		return MakeShareable(new FJsonValueString(out));
	}
	if (Node.Type == TEXT("TextProperty"))
	{
		uint64 ptrVal = Args[Index++];
		FString out;
		if (ptrVal != 0)
		{
			const FText* pText = reinterpret_cast<const FText*>(ptrVal);
			out = pText->ToString();
		}
		return MakeShareable(new FJsonValueString(out));
	}

	// Unknown types: consume one slot to keep moving and emit null
	++Index;
	return TSharedPtr<FJsonValue>();
}

// Sets default values
AEmitterHandler::AEmitterHandler()
{
	// Emitters are callback-driven; disable actor tick for lower runtime overhead.
	PrimaryActorTick.bCanEverTick = false;
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
	uint64 arg31)
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
		arg31};

	if (this->targetId.IsEmpty() || this->emitterId.IsEmpty())
	{
		return;
	}

	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("Emitter callback received while GEngine is unavailable"));
		return;
	}

	URshipSubsystem *subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Emitter callback received while subsystem is unavailable"));
		return;
	}

	EmitterContainer *emitter = subsystem->GetEmitterInfo(this->targetId, this->emitterId);

	if (emitter == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("EMITTER CANNOT PROCEED - Emitter not found: %s:%s"), *this->targetId, *this->emitterId);
		return;
	}

	auto props = emitter->GetProps();

	TSharedPtr<FJsonObject> json = MakeShareable(new FJsonObject);

	int argIndex = 0;

	for (const auto &prop : *props)
	{
		TSharedPtr<FJsonValue> val = ExtractValueFromArgs(prop, args, argIndex);
		if (!val.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Emitter skipping unsupported or null value for %s (Type: %s)"), *prop.Name, *prop.Type);
			continue;
		}
		switch (val->Type)
		{
		case EJson::String:
			json->SetStringField(prop.Name, val->AsString());
			break;
		case EJson::Number:
			json->SetNumberField(prop.Name, val->AsNumber());
			break;
		case EJson::Boolean:
			json->SetBoolField(prop.Name, val->AsBool());
			break;
		case EJson::Object:
			json->SetObjectField(prop.Name, val->AsObject());
			break;
		default:
			break;
		}
	}

	if (json->Values.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Emitter produced empty JSON for %s:%s"), *this->targetId, *this->emitterId);
	}

	UE_LOG(LogTemp, Verbose, TEXT("Emitter JSON: %s"), *GetJsonString(json));
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

void AEmitterHandler::SetDelegate(FScriptDelegate *d)
{
	this->delegate = d;
}
