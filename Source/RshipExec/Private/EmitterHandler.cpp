// Fill out your copyright notice in the Description page of Project Settings.


#include "EmitterHandler.h"
#include "RshipSubsystem.h"
#include "Util.h"
#include "Engine/Engine.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Text.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Quat.h"
#include "Math/Color.h"
#include "UObject/UnrealType.h"

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
                FProperty* Property = prop.Property;
                if (!Property) {
                        argIndex++;
                        continue;
                }

                if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property)) {
                        const bool boolValue = args[argIndex] != 0;
                        json->SetBoolField(prop.Name, boolValue);
                }
                else if (const FStrProperty* StrProp = CastField<FStrProperty>(Property)) {
                        const FString* StrValue = reinterpret_cast<FString*>(args[argIndex]);
                        json->SetStringField(prop.Name, StrValue ? *StrValue : FString());
                }
                else if (const FNameProperty* NameProp = CastField<FNameProperty>(Property)) {
                        const FName* NameValue = reinterpret_cast<FName*>(args[argIndex]);
                        json->SetStringField(prop.Name, NameValue ? NameValue->ToString() : FString());
                }
                else if (const FTextProperty* TextProp = CastField<FTextProperty>(Property)) {
                        const FText* TextValue = reinterpret_cast<FText*>(args[argIndex]);
                        json->SetStringField(prop.Name, TextValue ? TextValue->ToString() : FString());
                }
                else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property)) {
                        const float floatValue = *reinterpret_cast<const float*>(&args[argIndex]);
                        json->SetNumberField(prop.Name, floatValue);
                }
                else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property)) {
                        const double doubleValue = *reinterpret_cast<const double*>(&args[argIndex]);
                        json->SetNumberField(prop.Name, doubleValue);
                }
                else if (const FIntProperty* IntProp = CastField<FIntProperty>(Property)) {
                        const int32 intValue = static_cast<int32>(args[argIndex]);
                        json->SetNumberField(prop.Name, intValue);
                }
                else if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Property)) {
                        const int64 intValue = static_cast<int64>(args[argIndex]);
                        json->SetNumberField(prop.Name, static_cast<double>(intValue));
                }
                else if (const FUInt64Property* UInt64Prop = CastField<FUInt64Property>(Property)) {
                        const uint64 value = args[argIndex];
                        json->SetNumberField(prop.Name, static_cast<double>(value));
                }
                else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property)) {
                        const uint8 value = static_cast<uint8>(args[argIndex] & 0xFF);
                        json->SetNumberField(prop.Name, value);
                }
                else if (const FUInt16Property* UInt16Prop = CastField<FUInt16Property>(Property)) {
                        const uint16 value = static_cast<uint16>(args[argIndex] & 0xFFFF);
                        json->SetNumberField(prop.Name, value);
                }
                else if (const FInt16Property* Int16Prop = CastField<FInt16Property>(Property)) {
                        const int16 value = static_cast<int16>(args[argIndex] & 0xFFFF);
                        json->SetNumberField(prop.Name, value);
                }
                else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property)) {
                        const uint64 value = args[argIndex];
                        FString EnumString = EnumProp->GetEnum() ? EnumProp->GetEnum()->GetNameStringByValue(value) : FString::FromInt(static_cast<int32>(value));
                        json->SetStringField(prop.Name, EnumString);
                }
                else if (const FStructProperty* StructProp = CastField<FStructProperty>(Property)) {
                        const void* RawValue = reinterpret_cast<const void*>(args[argIndex]);
                        if (!RawValue)
                        {
                                json->SetField(prop.Name, MakeShareable(new FJsonValueNull()));
                        }
                        else if (StructProp->Struct == TBaseStructure<FVector>::Get())
                        {
                                const FVector* VectorValue = reinterpret_cast<const FVector*>(RawValue);
                                TSharedPtr<FJsonObject> VectorJson = MakeShareable(new FJsonObject);
                                VectorJson->SetNumberField(TEXT("x"), VectorValue->X);
                                VectorJson->SetNumberField(TEXT("y"), VectorValue->Y);
                                VectorJson->SetNumberField(TEXT("z"), VectorValue->Z);
                                json->SetObjectField(prop.Name, VectorJson);
                        }
                        else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
                        {
                                const FRotator* RotatorValue = reinterpret_cast<const FRotator*>(RawValue);
                                TSharedPtr<FJsonObject> RotatorJson = MakeShareable(new FJsonObject);
                                RotatorJson->SetNumberField(TEXT("pitch"), RotatorValue->Pitch);
                                RotatorJson->SetNumberField(TEXT("yaw"), RotatorValue->Yaw);
                                RotatorJson->SetNumberField(TEXT("roll"), RotatorValue->Roll);
                                json->SetObjectField(prop.Name, RotatorJson);
                        }
                        else if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
                        {
                                const FLinearColor* ColorValue = reinterpret_cast<const FLinearColor*>(RawValue);
                                TSharedPtr<FJsonObject> ColorJson = MakeShareable(new FJsonObject);
                                ColorJson->SetNumberField(TEXT("r"), ColorValue->R);
                                ColorJson->SetNumberField(TEXT("g"), ColorValue->G);
                                ColorJson->SetNumberField(TEXT("b"), ColorValue->B);
                                ColorJson->SetNumberField(TEXT("a"), ColorValue->A);
                                json->SetObjectField(prop.Name, ColorJson);
                        }
                        else if (StructProp->Struct == TBaseStructure<FTransform>::Get())
                        {
                                const FTransform* TransformValue = reinterpret_cast<const FTransform*>(RawValue);
                                TSharedPtr<FJsonObject> TransformJson = MakeShareable(new FJsonObject);

                                const FVector Translation = TransformValue->GetTranslation();
                                const FRotator Rotation = TransformValue->Rotator();
                                const FVector Scale = TransformValue->GetScale3D();

                                TSharedPtr<FJsonObject> TranslationJson = MakeShareable(new FJsonObject);
                                TranslationJson->SetNumberField(TEXT("x"), Translation.X);
                                TranslationJson->SetNumberField(TEXT("y"), Translation.Y);
                                TranslationJson->SetNumberField(TEXT("z"), Translation.Z);

                                TSharedPtr<FJsonObject> RotationJson = MakeShareable(new FJsonObject);
                                RotationJson->SetNumberField(TEXT("pitch"), Rotation.Pitch);
                                RotationJson->SetNumberField(TEXT("yaw"), Rotation.Yaw);
                                RotationJson->SetNumberField(TEXT("roll"), Rotation.Roll);

                                TSharedPtr<FJsonObject> ScaleJson = MakeShareable(new FJsonObject);
                                ScaleJson->SetNumberField(TEXT("x"), Scale.X);
                                ScaleJson->SetNumberField(TEXT("y"), Scale.Y);
                                ScaleJson->SetNumberField(TEXT("z"), Scale.Z);

                                TransformJson->SetObjectField(TEXT("translation"), TranslationJson);
                                TransformJson->SetObjectField(TEXT("rotation"), RotationJson);
                                TransformJson->SetObjectField(TEXT("scale"), ScaleJson);

                                json->SetObjectField(prop.Name, TransformJson);
                        }
                        else
                        {
                                FString ExportText;
                                StructProp->ExportTextItem(ExportText, const_cast<uint8*>(reinterpret_cast<const uint8*>(RawValue)), nullptr, nullptr, PPF_None);
                                json->SetStringField(prop.Name, ExportText);
                        }
                }
                else {
                        UE_LOG(LogTemp, Warning, TEXT("EMITTER: Unsupported property type %s"), *Property->GetClass()->GetName());
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



