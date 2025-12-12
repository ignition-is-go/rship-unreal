// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for file system operations
 */
class ULTIMATECONTROL_API FUltimateControlFileHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlFileHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// file.read - Read a file
	bool HandleRead(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// file.write - Write a file
	bool HandleWrite(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// file.exists - Check if file exists
	bool HandleExists(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// file.delete - Delete a file
	bool HandleDelete(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// file.list - List files in directory
	bool HandleList(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// file.getInfo - Get file info
	bool HandleGetInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// file.copy - Copy a file
	bool HandleCopy(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// file.move - Move/rename a file
	bool HandleMove(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// Helper to validate and resolve path (security)
	bool ValidatePath(const FString& Path, FString& OutResolvedPath, TSharedPtr<FJsonObject>& OutError);
};
