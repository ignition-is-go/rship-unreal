// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declaration
class UUltimateControlSubsystem;

/**
 * JSON-RPC 2.0 Error Codes
 */
namespace EJsonRpcError
{
	constexpr int32 ParseError = -32700;
	constexpr int32 InvalidRequest = -32600;
	constexpr int32 MethodNotFound = -32601;
	constexpr int32 InvalidParams = -32602;
	constexpr int32 InternalError = -32603;
	// Application-specific errors
	constexpr int32 Unauthorized = -32000;
	constexpr int32 FeatureDisabled = -32001;
	constexpr int32 OperationFailed = -32002;
	constexpr int32 NotFound = -32003;
	constexpr int32 ConfirmationRequired = -32004;
	constexpr int32 Timeout = -32005;
}

/**
 * Delegate for JSON-RPC method handlers
 * @param Params - The params object from the JSON-RPC request
 * @param OutResult - The result to return (set on success)
 * @param OutError - The error object to return (set on failure)
 * @return true if the method succeeded, false if it failed
 */
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FJsonRpcMethodHandler,
	const TSharedPtr<FJsonObject>& /* Params */,
	TSharedPtr<FJsonValue>& /* OutResult */,
	TSharedPtr<FJsonObject>& /* OutError */);

/**
 * Information about a registered JSON-RPC method
 */
struct FJsonRpcMethodInfo
{
	FString Name;
	FString Description;
	FString Category;
	TSharedPtr<FJsonObject> ParamsSchema;
	TSharedPtr<FJsonObject> ResultSchema;
	FJsonRpcMethodHandler Handler;
	bool bRequiresConfirmation = false;
	bool bIsDangerous = false;
};
