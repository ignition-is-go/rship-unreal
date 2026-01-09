// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Handlers/UltimateControlHandlerBase.h"

/**
 * Handler for asset-related JSON-RPC methods
 */
class ULTIMATECONTROL_API FUltimateControlAssetHandler : public FUltimateControlHandlerBase
{
public:
	FUltimateControlAssetHandler(UUltimateControlSubsystem* InSubsystem);

private:
	// asset.list - List assets with optional filtering
	bool HandleList(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.get - Get detailed information about an asset
	bool HandleGet(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.exists - Check if an asset exists
	bool HandleExists(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.search - Search assets by name or tags
	bool HandleSearch(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.getClasses - Get all asset classes
	bool HandleGetClasses(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.getDependencies - Get asset dependencies
	bool HandleGetDependencies(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.getReferencers - Get assets that reference this asset
	bool HandleGetReferencers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.duplicate - Duplicate an asset
	bool HandleDuplicate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.rename - Rename an asset
	bool HandleRename(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.delete - Delete an asset
	bool HandleDelete(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.createFolder - Create a content folder
	bool HandleCreateFolder(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.import - Import an external file
	bool HandleImport(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.export - Export an asset to file
	bool HandleExport(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.getProperty - Get a property value from an asset
	bool HandleGetProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// asset.setProperty - Set a property value on an asset
	bool HandleSetProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError);

	// Helper to convert FAssetData to JSON
	TSharedPtr<FJsonObject> AssetDataToJson(const struct FAssetData& AssetData, bool bIncludeMetadata = false);
};
