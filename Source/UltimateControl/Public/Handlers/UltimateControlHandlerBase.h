// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UltimateControlSubsystem.h"

/**
 * Base class for JSON-RPC method handlers
 */
class ULTIMATECONTROL_API FUltimateControlHandlerBase
{
public:
	FUltimateControlHandlerBase(UUltimateControlSubsystem* InSubsystem)
		: Subsystem(InSubsystem)
	{
	}

	virtual ~FUltimateControlHandlerBase() = default;

protected:
	/** Register a method with the subsystem */
	void RegisterMethod(
		const FString& MethodName,
		const FString& Description,
		const FString& Category,
		FJsonRpcMethodHandler Handler,
		bool bIsDangerous = false,
		bool bRequiresConfirmation = false);

	/** Create a params schema object */
	static TSharedPtr<FJsonObject> MakeParamsSchema(
		std::initializer_list<TPair<FString, FString>> Params);

	/** Require a string parameter */
	static bool RequireString(
		const TSharedPtr<FJsonObject>& Params,
		const FString& ParamName,
		FString& OutValue,
		TSharedPtr<FJsonObject>& OutError);

	/** Require an integer parameter */
	static bool RequireInt(
		const TSharedPtr<FJsonObject>& Params,
		const FString& ParamName,
		int32& OutValue,
		TSharedPtr<FJsonObject>& OutError);

	/** Require a boolean parameter */
	static bool RequireBool(
		const TSharedPtr<FJsonObject>& Params,
		const FString& ParamName,
		bool& OutValue,
		TSharedPtr<FJsonObject>& OutError);

	/** Get optional string parameter with default */
	static FString GetOptionalString(
		const TSharedPtr<FJsonObject>& Params,
		const FString& ParamName,
		const FString& Default = TEXT(""));

	/** Get optional int parameter with default */
	static int32 GetOptionalInt(
		const TSharedPtr<FJsonObject>& Params,
		const FString& ParamName,
		int32 Default = 0);

	/** Get optional bool parameter with default */
	static bool GetOptionalBool(
		const TSharedPtr<FJsonObject>& Params,
		const FString& ParamName,
		bool Default = false);

	/** Get optional array parameter */
	static TArray<TSharedPtr<FJsonValue>> GetOptionalArray(
		const TSharedPtr<FJsonObject>& Params,
		const FString& ParamName);

	/** Convert FVector to JSON */
	static TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector);

	/** Convert FRotator to JSON */
	static TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator);

	/** Convert FTransform to JSON */
	static TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform);

	/** Parse vector from JSON */
	static FVector JsonToVector(const TSharedPtr<FJsonObject>& JsonObj);

	/** Parse rotator from JSON */
	static FRotator JsonToRotator(const TSharedPtr<FJsonObject>& JsonObj);

	/** Parse transform from JSON */
	static FTransform JsonToTransform(const TSharedPtr<FJsonObject>& JsonObj);

	UUltimateControlSubsystem* Subsystem;
};
