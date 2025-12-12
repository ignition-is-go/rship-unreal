// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlHandlerBase.h"

void FUltimateControlHandlerBase::RegisterMethod(
	const FString& MethodName,
	const FString& Description,
	const FString& Category,
	FJsonRpcMethodHandler Handler,
	bool bIsDangerous,
	bool bRequiresConfirmation)
{
	if (Subsystem)
	{
		FJsonRpcMethodInfo Info;
		Info.Name = MethodName;
		Info.Description = Description;
		Info.Category = Category;
		Info.Handler = Handler;
		Info.bIsDangerous = bIsDangerous;
		Info.bRequiresConfirmation = bRequiresConfirmation;

		Subsystem->RegisterMethod(MethodName, Info);
	}
}

TSharedPtr<FJsonObject> FUltimateControlHandlerBase::MakeParamsSchema(
	std::initializer_list<TPair<FString, FString>> Params)
{
	TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

	for (const auto& Param : Params)
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("type"), Param.Value);
		Properties->SetObjectField(Param.Key, ParamObj);
	}

	Schema->SetObjectField(TEXT("properties"), Properties);
	return Schema;
}

bool FUltimateControlHandlerBase::RequireString(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ParamName,
	FString& OutValue,
	TSharedPtr<FJsonObject>& OutError)
{
	if (!Params->TryGetStringField(ParamName, OutValue))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			FString::Printf(TEXT("Missing required parameter: %s"), *ParamName));
		return false;
	}
	return true;
}

bool FUltimateControlHandlerBase::RequireInt(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ParamName,
	int32& OutValue,
	TSharedPtr<FJsonObject>& OutError)
{
	if (!Params->TryGetNumberField(ParamName, OutValue))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			FString::Printf(TEXT("Missing required parameter: %s"), *ParamName));
		return false;
	}
	return true;
}

bool FUltimateControlHandlerBase::RequireBool(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ParamName,
	bool& OutValue,
	TSharedPtr<FJsonObject>& OutError)
{
	if (!Params->TryGetBoolField(ParamName, OutValue))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			FString::Printf(TEXT("Missing required parameter: %s"), *ParamName));
		return false;
	}
	return true;
}

FString FUltimateControlHandlerBase::GetOptionalString(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ParamName,
	const FString& Default)
{
	FString Value;
	if (Params->TryGetStringField(ParamName, Value))
	{
		return Value;
	}
	return Default;
}

int32 FUltimateControlHandlerBase::GetOptionalInt(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ParamName,
	int32 Default)
{
	int32 Value;
	if (Params->TryGetNumberField(ParamName, Value))
	{
		return Value;
	}
	return Default;
}

bool FUltimateControlHandlerBase::GetOptionalBool(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ParamName,
	bool Default)
{
	bool Value;
	if (Params->TryGetBoolField(ParamName, Value))
	{
		return Value;
	}
	return Default;
}

TArray<TSharedPtr<FJsonValue>> FUltimateControlHandlerBase::GetOptionalArray(
	const TSharedPtr<FJsonObject>& Params,
	const FString& ParamName)
{
	const TArray<TSharedPtr<FJsonValue>>* Value;
	if (Params->TryGetArrayField(ParamName, Value))
	{
		return *Value;
	}
	return TArray<TSharedPtr<FJsonValue>>();
}

TSharedPtr<FJsonObject> FUltimateControlHandlerBase::VectorToJson(const FVector& Vector)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("x"), Vector.X);
	Obj->SetNumberField(TEXT("y"), Vector.Y);
	Obj->SetNumberField(TEXT("z"), Vector.Z);
	return Obj;
}

TSharedPtr<FJsonObject> FUltimateControlHandlerBase::RotatorToJson(const FRotator& Rotator)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
	Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
	Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
	return Obj;
}

TSharedPtr<FJsonObject> FUltimateControlHandlerBase::TransformToJson(const FTransform& Transform)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("location"), VectorToJson(Transform.GetLocation()));
	Obj->SetObjectField(TEXT("rotation"), RotatorToJson(Transform.Rotator()));
	Obj->SetObjectField(TEXT("scale"), VectorToJson(Transform.GetScale3D()));
	return Obj;
}

FVector FUltimateControlHandlerBase::JsonToVector(const TSharedPtr<FJsonObject>& JsonObj)
{
	if (!JsonObj.IsValid())
	{
		return FVector::ZeroVector;
	}
	return FVector(
		JsonObj->GetNumberField(TEXT("x")),
		JsonObj->GetNumberField(TEXT("y")),
		JsonObj->GetNumberField(TEXT("z"))
	);
}

FRotator FUltimateControlHandlerBase::JsonToRotator(const TSharedPtr<FJsonObject>& JsonObj)
{
	if (!JsonObj.IsValid())
	{
		return FRotator::ZeroRotator;
	}
	return FRotator(
		JsonObj->GetNumberField(TEXT("pitch")),
		JsonObj->GetNumberField(TEXT("yaw")),
		JsonObj->GetNumberField(TEXT("roll"))
	);
}

FTransform FUltimateControlHandlerBase::JsonToTransform(const TSharedPtr<FJsonObject>& JsonObj)
{
	if (!JsonObj.IsValid())
	{
		return FTransform::Identity;
	}

	FVector Location = JsonToVector(JsonObj->GetObjectField(TEXT("location")));
	FRotator Rotation = JsonToRotator(JsonObj->GetObjectField(TEXT("rotation")));
	FVector Scale = FVector::OneVector;

	const TSharedPtr<FJsonObject>* ScaleObj;
	if (JsonObj->TryGetObjectField(TEXT("scale"), ScaleObj))
	{
		Scale = JsonToVector(*ScaleObj);
	}

	return FTransform(Rotation, Location, Scale);
}
