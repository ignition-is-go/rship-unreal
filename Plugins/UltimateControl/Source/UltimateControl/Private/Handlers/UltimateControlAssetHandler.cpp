// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlAssetHandler.h"
#include "UltimateControl.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"
#include "UObject/PropertyPortFlags.h"
#include "HAL/FileManager.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

FUltimateControlAssetHandler::FUltimateControlAssetHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(
		TEXT("asset.list"),
		TEXT("List assets with optional path and class filtering"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleList));

	RegisterMethod(
		TEXT("asset.get"),
		TEXT("Get detailed information about a specific asset"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleGet));

	RegisterMethod(
		TEXT("asset.exists"),
		TEXT("Check if an asset exists at the given path"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleExists));

	RegisterMethod(
		TEXT("asset.search"),
		TEXT("Search for assets by name pattern or tags"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleSearch));

	RegisterMethod(
		TEXT("asset.getClasses"),
		TEXT("Get all available asset classes"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleGetClasses));

	RegisterMethod(
		TEXT("asset.getDependencies"),
		TEXT("Get assets that this asset depends on"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleGetDependencies));

	RegisterMethod(
		TEXT("asset.getReferencers"),
		TEXT("Get assets that reference this asset"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleGetReferencers));

	RegisterMethod(
		TEXT("asset.duplicate"),
		TEXT("Duplicate an asset to a new location"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleDuplicate));

	RegisterMethod(
		TEXT("asset.rename"),
		TEXT("Rename or move an asset"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleRename),
		/* bIsDangerous */ true);

	RegisterMethod(
		TEXT("asset.delete"),
		TEXT("Delete an asset"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleDelete),
		/* bIsDangerous */ true,
		/* bRequiresConfirmation */ true);

	RegisterMethod(
		TEXT("asset.createFolder"),
		TEXT("Create a new content folder"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleCreateFolder));

	RegisterMethod(
		TEXT("asset.import"),
		TEXT("Import an external file as an asset"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleImport));

	RegisterMethod(
		TEXT("asset.export"),
		TEXT("Export an asset to an external file"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleExport));

	RegisterMethod(
		TEXT("asset.getProperty"),
		TEXT("Get a property value from an asset"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleGetProperty));

	RegisterMethod(
		TEXT("asset.setProperty"),
		TEXT("Set a property value on an asset"),
		TEXT("Asset"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAssetHandler::HandleSetProperty));
}

TSharedPtr<FJsonObject> FUltimateControlAssetHandler::AssetDataToJson(const FAssetData& AssetData, bool bIncludeMetadata)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
	Obj->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
	Obj->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
	Obj->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
	Obj->SetStringField(TEXT("assetClass"), AssetData.AssetClassPath.GetAssetName().ToString());
	Obj->SetBoolField(TEXT("isValid"), AssetData.IsValid());
	Obj->SetBoolField(TEXT("isAssetLoaded"), AssetData.IsAssetLoaded());

	// Check if it's a redirector
	Obj->SetBoolField(TEXT("isRedirector"), AssetData.IsRedirector());

	// Get disk size if available
	const FAssetPackageData* PackageData = AssetData.GetTagValueRef<FAssetPackageData>(FPrimaryAssetId::PrimaryAssetTypeName);
	// We can't easily get the package data from FAssetData directly, but we can get the file size
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(AssetData.PackageName.ToString(), PackageFilename))
	{
		int64 FileSize = IFileManager::Get().FileSize(*PackageFilename);
		if (FileSize >= 0)
		{
			Obj->SetNumberField(TEXT("diskSize"), static_cast<double>(FileSize));
		}
	}

	if (bIncludeMetadata)
	{
		TSharedPtr<FJsonObject> MetadataObj = MakeShared<FJsonObject>();

		// Get all tags
		FAssetDataTagMap TagsAndValues;
		AssetData.GetTagsAndValues(TagsAndValues);
		for (const auto& Tag : TagsAndValues)
		{
			MetadataObj->SetStringField(Tag.Key.ToString(), Tag.Value.GetValue());
		}

		Obj->SetObjectField(TEXT("metadata"), MetadataObj);
	}

	return Obj;
}

bool FUltimateControlAssetHandler::HandleList(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path = GetOptionalString(Params, TEXT("path"), TEXT("/Game"));
	FString ClassName = GetOptionalString(Params, TEXT("class"), TEXT(""));
	bool bRecursive = GetOptionalBool(Params, TEXT("recursive"), true);
	bool bIncludeMetadata = GetOptionalBool(Params, TEXT("includeMetadata"), false);
	int32 Limit = GetOptionalInt(Params, TEXT("limit"), 1000);
	int32 Offset = GetOptionalInt(Params, TEXT("offset"), 0);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = bRecursive;

	if (!ClassName.IsEmpty())
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(FName(*ClassName)));
	}

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	int32 TotalCount = AssetList.Num();

	for (int32 i = Offset; i < FMath::Min(Offset + Limit, TotalCount); ++i)
	{
		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetList[i], bIncludeMetadata)));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("assets"), AssetsArray);
	ResultObj->SetNumberField(TEXT("totalCount"), TotalCount);
	ResultObj->SetNumberField(TEXT("offset"), Offset);
	ResultObj->SetNumberField(TEXT("limit"), Limit);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleGet(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Path));

	if (!AssetData.IsValid())
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Asset not found: %s"), *Path));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = AssetDataToJson(AssetData, /* bIncludeMetadata */ true);

	// Load the asset to get more detailed information
	UObject* Asset = AssetData.GetAsset();
	if (Asset)
	{
		ResultObj->SetStringField(TEXT("outerName"), Asset->GetOuter() ? Asset->GetOuter()->GetName() : TEXT(""));
		ResultObj->SetStringField(TEXT("flags"), FString::Printf(TEXT("0x%08X"), Asset->GetFlags()));

		// Get property information
		UClass* AssetClass = Asset->GetClass();
		if (AssetClass)
		{
			TArray<TSharedPtr<FJsonValue>> PropertiesArray;

			for (TFieldIterator<FProperty> PropIt(AssetClass); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				if (Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
				{
					TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
					PropObj->SetStringField(TEXT("name"), Property->GetName());
					PropObj->SetStringField(TEXT("type"), Property->GetCPPType());
					PropObj->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
					PropObj->SetBoolField(TEXT("editable"), Property->HasAnyPropertyFlags(CPF_Edit));
					PropObj->SetBoolField(TEXT("blueprintVisible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
					PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
				}
			}

			ResultObj->SetArrayField(TEXT("editableProperties"), PropertiesArray);
		}
	}

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleExists(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	bool bExists = UEditorAssetLibrary::DoesAssetExist(Path);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("exists"), bExists);
	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleSearch(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Query = GetOptionalString(Params, TEXT("query"), TEXT(""));
	FString ClassName = GetOptionalString(Params, TEXT("class"), TEXT(""));
	int32 Limit = GetOptionalInt(Params, TEXT("limit"), 100);

	if (Query.IsEmpty())
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			TEXT("Query parameter is required"));
		return false;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	Filter.PackagePaths.Add(FName(TEXT("/Game")));

	if (!ClassName.IsEmpty())
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(FName(*ClassName)));
	}

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssets(Filter, AllAssets);

	// Filter by query
	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	for (const FAssetData& AssetData : AllAssets)
	{
		if (ResultsArray.Num() >= Limit)
		{
			break;
		}

		FString AssetName = AssetData.AssetName.ToString();
		if (AssetName.Contains(Query, ESearchCase::IgnoreCase))
		{
			ResultsArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetData)));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("results"), ResultsArray);
	ResultObj->SetNumberField(TEXT("count"), ResultsArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleGetClasses(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	TArray<TSharedPtr<FJsonValue>> ClassesArray;

	// Get all UClasses that are asset types
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class && Class->IsChildOf(UObject::StaticClass()))
		{
			// Check if this is an asset class
			if (Class->HasAnyClassFlags(CLASS_Abstract))
			{
				continue;
			}

			TSharedPtr<FJsonObject> ClassObj = MakeShared<FJsonObject>();
			ClassObj->SetStringField(TEXT("name"), Class->GetName());
			ClassObj->SetStringField(TEXT("path"), Class->GetPathName());

			if (Class->GetSuperClass())
			{
				ClassObj->SetStringField(TEXT("parent"), Class->GetSuperClass()->GetName());
			}

			ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(ClassesArray);
	return true;
}

bool FUltimateControlAssetHandler::HandleGetDependencies(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetIdentifier> Dependencies;
	AssetRegistry.GetDependencies(FAssetIdentifier(FName(*Path)), Dependencies);

	TArray<TSharedPtr<FJsonValue>> DepsArray;
	for (const FAssetIdentifier& Dep : Dependencies)
	{
		TSharedPtr<FJsonObject> DepObj = MakeShared<FJsonObject>();
		DepObj->SetStringField(TEXT("path"), Dep.PackageName.ToString());
		DepsArray.Add(MakeShared<FJsonValueObject>(DepObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("dependencies"), DepsArray);
	ResultObj->SetNumberField(TEXT("count"), DepsArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleGetReferencers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(FAssetIdentifier(FName(*Path)), Referencers);

	TArray<TSharedPtr<FJsonValue>> RefsArray;
	for (const FAssetIdentifier& Ref : Referencers)
	{
		TSharedPtr<FJsonObject> RefObj = MakeShared<FJsonObject>();
		RefObj->SetStringField(TEXT("path"), Ref.PackageName.ToString());
		RefsArray.Add(MakeShared<FJsonValueObject>(RefObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("referencers"), RefsArray);
	ResultObj->SetNumberField(TEXT("count"), RefsArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleDuplicate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString SourcePath;
	FString DestPath;

	if (!RequireString(Params, TEXT("source"), SourcePath, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("destination"), DestPath, OutError))
	{
		return false;
	}

	UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);

	if (!DuplicatedAsset)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			FString::Printf(TEXT("Failed to duplicate asset from %s to %s"), *SourcePath, *DestPath));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("path"), DuplicatedAsset->GetPathName());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleRename(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString SourcePath;
	FString DestPath;

	if (!RequireString(Params, TEXT("source"), SourcePath, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("destination"), DestPath, OutError))
	{
		return false;
	}

	bool bSuccess = UEditorAssetLibrary::RenameAsset(SourcePath, DestPath);

	if (!bSuccess)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			FString::Printf(TEXT("Failed to rename asset from %s to %s"), *SourcePath, *DestPath));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("newPath"), DestPath);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleDelete(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	bool bSuccess = UEditorAssetLibrary::DeleteAsset(Path);

	if (!bSuccess)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			FString::Printf(TEXT("Failed to delete asset: %s"), *Path));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleCreateFolder(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	bool bSuccess = UEditorAssetLibrary::MakeDirectory(Path);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bSuccess);
	ResultObj->SetStringField(TEXT("path"), Path);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleImport(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString FilePath;
	FString DestPath;

	if (!RequireString(Params, TEXT("file"), FilePath, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("destination"), DestPath, OutError))
	{
		return false;
	}

	if (!FPaths::FileExists(FilePath))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("File not found: %s"), *FilePath));
		return false;
	}

	// Use the asset tools to import
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	TArray<FString> Files;
	Files.Add(FilePath);

	TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssets(Files, DestPath);

	if (ImportedAssets.Num() == 0)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			FString::Printf(TEXT("Failed to import file: %s"), *FilePath));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> ImportedArray;
	for (UObject* Asset : ImportedAssets)
	{
		if (Asset)
		{
			TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
			AssetObj->SetStringField(TEXT("path"), Asset->GetPathName());
			AssetObj->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
			ImportedArray.Add(MakeShared<FJsonValueObject>(AssetObj));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetArrayField(TEXT("importedAssets"), ImportedArray);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleExport(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString AssetPath;
	FString OutputPath;

	if (!RequireString(Params, TEXT("asset"), AssetPath, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("output"), OutputPath, OutError))
	{
		return false;
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		return false;
	}

	// Use the asset tools to export
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	TArray<UObject*> ObjectsToExport;
	ObjectsToExport.Add(Asset);

	AssetToolsModule.Get().ExportAssets(ObjectsToExport, OutputPath);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("outputPath"), OutputPath);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleGetProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString AssetPath;
	FString PropertyName;

	if (!RequireString(Params, TEXT("asset"), AssetPath, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("property"), PropertyName, OutError))
	{
		return false;
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		return false;
	}

	FProperty* Property = Asset->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Property not found: %s"), *PropertyName));
		return false;
	}

	// Export the property value to a string
	FString ValueStr;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Asset);
	Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, Asset, PPF_None);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("property"), PropertyName);
	ResultObj->SetStringField(TEXT("value"), ValueStr);
	ResultObj->SetStringField(TEXT("type"), Property->GetCPPType());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAssetHandler::HandleSetProperty(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString AssetPath;
	FString PropertyName;
	FString ValueStr;

	if (!RequireString(Params, TEXT("asset"), AssetPath, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("property"), PropertyName, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("value"), ValueStr, OutError))
	{
		return false;
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		return false;
	}

	FProperty* Property = Asset->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Property not found: %s"), *PropertyName));
		return false;
	}

	// Import the property value from string
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Asset);

	const TCHAR* ImportBuffer = *ValueStr;
	Property->ImportText_Direct(ImportBuffer, ValuePtr, Asset, PPF_None);

	// Mark the package as dirty
	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
