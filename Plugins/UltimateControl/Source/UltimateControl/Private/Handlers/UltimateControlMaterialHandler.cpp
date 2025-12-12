// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlMaterialHandler.h"
#include "UltimateControlVersion.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "MaterialEditorUtilities.h"
#include "MaterialGraph/MaterialGraph.h"

FUltimateControlMaterialHandler::FUltimateControlMaterialHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(TEXT("material.list"), TEXT("List materials"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleListMaterials));
	RegisterMethod(TEXT("material.get"), TEXT("Get material"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleGetMaterial));
	RegisterMethod(TEXT("material.create"), TEXT("Create material"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleCreateMaterial));
	RegisterMethod(TEXT("material.getParameters"), TEXT("Get material parameters"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleGetMaterialParameters));
	RegisterMethod(TEXT("material.setParameter"), TEXT("Set material parameter"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleSetMaterialParameter));
	RegisterMethod(TEXT("material.getParameter"), TEXT("Get material parameter"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleGetMaterialParameter));
	RegisterMethod(TEXT("material.getNodes"), TEXT("Get material nodes"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleGetMaterialNodes));
	RegisterMethod(TEXT("material.addNode"), TEXT("Add material node"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleAddMaterialNode));
	RegisterMethod(TEXT("material.deleteNode"), TEXT("Delete material node"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleDeleteMaterialNode));
	RegisterMethod(TEXT("material.connectNodes"), TEXT("Connect material nodes"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleConnectMaterialNodes));
	RegisterMethod(TEXT("material.disconnectNode"), TEXT("Disconnect material node"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleDisconnectMaterialNode));
	RegisterMethod(TEXT("material.compile"), TEXT("Compile material"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleCompileMaterial));
	RegisterMethod(TEXT("material.getCompileErrors"), TEXT("Get compile errors"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleGetCompileErrors));
	RegisterMethod(TEXT("material.listInstances"), TEXT("List material instances"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleListMaterialInstances));
	RegisterMethod(TEXT("material.createInstance"), TEXT("Create material instance"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleCreateMaterialInstance));
	RegisterMethod(TEXT("material.getInstanceParent"), TEXT("Get instance parent"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleGetMaterialInstanceParent));
	RegisterMethod(TEXT("material.setInstanceParent"), TEXT("Set instance parent"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleSetMaterialInstanceParent));
	RegisterMethod(TEXT("materialInstance.setScalar"), TEXT("Set scalar parameter"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleSetInstanceScalarParameter));
	RegisterMethod(TEXT("materialInstance.setVector"), TEXT("Set vector parameter"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleSetInstanceVectorParameter));
	RegisterMethod(TEXT("materialInstance.setTexture"), TEXT("Set texture parameter"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleSetInstanceTextureParameter));
	RegisterMethod(TEXT("materialInstance.getParameters"), TEXT("Get instance parameters"), TEXT("Material"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlMaterialHandler::HandleGetInstanceParameters));
}

TSharedPtr<FJsonObject> FUltimateControlMaterialHandler::MaterialToJson(UMaterial* Material)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), Material->GetName());
	Result->SetStringField(TEXT("path"), Material->GetPathName());
	Result->SetStringField(TEXT("class"), TEXT("Material"));
	Result->SetBoolField(TEXT("twoSided"), Material->TwoSided);
	Result->SetStringField(TEXT("shadingModel"), StaticEnum<EMaterialShadingModel>()->GetNameStringByValue((int64)Material->GetShadingModels().GetFirstShadingModel()));
	Result->SetStringField(TEXT("blendMode"), StaticEnum<EBlendMode>()->GetNameStringByValue((int64)Material->BlendMode));
	Result->SetBoolField(TEXT("isDefaultMaterial"), Material->IsDefaultMaterial());

	// Expression count
	Result->SetNumberField(TEXT("expressionCount"), Material->GetExpressions().Num());

	return Result;
}

TSharedPtr<FJsonObject> FUltimateControlMaterialHandler::MaterialInstanceToJson(UMaterialInstance* MaterialInstance)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), MaterialInstance->GetName());
	Result->SetStringField(TEXT("path"), MaterialInstance->GetPathName());
	Result->SetStringField(TEXT("class"), MaterialInstance->GetClass()->GetName());

	if (MaterialInstance->Parent)
	{
		Result->SetStringField(TEXT("parent"), MaterialInstance->Parent->GetPathName());
	}

	// Count of overridden parameters
	if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInstance))
	{
		Result->SetNumberField(TEXT("scalarParameterCount"), MIC->ScalarParameterValues.Num());
		Result->SetNumberField(TEXT("vectorParameterCount"), MIC->VectorParameterValues.Num());
		Result->SetNumberField(TEXT("textureParameterCount"), MIC->TextureParameterValues.Num());
	}

	return Result;
}

TSharedPtr<FJsonObject> FUltimateControlMaterialHandler::MaterialExpressionToJson(UMaterialExpression* Expression)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), Expression->GetName());
	Result->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
	Result->SetNumberField(TEXT("positionX"), Expression->MaterialExpressionEditorX);
	Result->SetNumberField(TEXT("positionY"), Expression->MaterialExpressionEditorY);
	Result->SetStringField(TEXT("description"), Expression->Desc);

	// Check if it's a parameter expression
	if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expression))
	{
		Result->SetStringField(TEXT("parameterName"), ParamExpr->ParameterName.ToString());
		Result->SetStringField(TEXT("group"), ParamExpr->Group.ToString());
	}

	return Result;
}

bool FUltimateControlMaterialHandler::HandleListMaterials(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path = TEXT("/Game");
	if (Params->HasField(TEXT("path")))
	{
		Path = Params->GetStringField(TEXT("path"));
	}

	bool bRecursive = true;
	if (Params->HasField(TEXT("recursive")))
	{
		bRecursive = Params->GetBoolField(TEXT("recursive"));
	}

	int32 Limit = 500;
	if (Params->HasField(TEXT("limit")))
	{
		Limit = FMath::Clamp(FMath::RoundToInt(Params->GetNumberField(TEXT("limit"))), 1, 10000);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	FARFilter Filter;
	Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = bRecursive;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> MaterialsArray;
	for (int32 i = 0; i < FMath::Min(AssetDataList.Num(), Limit); i++)
	{
		const FAssetData& AssetData = AssetDataList[i];
		TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
		MatObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		MatObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		MatObj->SetStringField(TEXT("class"), TEXT("Material"));
		MaterialsArray.Add(MakeShared<FJsonValueObject>(MatObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("materials"), MaterialsArray);
	ResultObj->SetNumberField(TEXT("count"), MaterialsArray.Num());
	ResultObj->SetNumberField(TEXT("totalCount"), AssetDataList.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleGetMaterial(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
	if (!Material)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material not found: %s"), *Path));
		return false;
	}

	Result = MakeShared<FJsonValueObject>(MaterialToJson(Material));
	return true;
}

bool FUltimateControlMaterialHandler::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(Path);
	FString AssetName = FPackageName::GetShortName(Path);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), MaterialFactory);

	if (!NewAsset)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, FString::Printf(TEXT("Failed to create material at: %s"), *Path));
		return false;
	}

	UMaterial* NewMaterial = Cast<UMaterial>(NewAsset);

	// Apply initial settings if provided
	if (Params->HasField(TEXT("twoSided")))
	{
		NewMaterial->TwoSided = Params->GetBoolField(TEXT("twoSided"));
	}

	if (Params->HasField(TEXT("blendMode")))
	{
		FString BlendModeStr = Params->GetStringField(TEXT("blendMode"));
		// UE 5.6+: GetValueByNameString returns the value directly, not via output param
		int64 BlendModeValue = StaticEnum<EBlendMode>()->GetValueByNameString(BlendModeStr);
		if (BlendModeValue != INDEX_NONE)
		{
			NewMaterial->BlendMode = (EBlendMode)BlendModeValue;
		}
	}

	NewMaterial->PostEditChange();
	NewMaterial->MarkPackageDirty();

	Result = MakeShared<FJsonValueObject>(MaterialToJson(NewMaterial));
	return true;
}

bool FUltimateControlMaterialHandler::HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	UMaterialInterface* MaterialInterface = LoadObject<UMaterialInterface>(nullptr, *Path);
	if (!MaterialInterface)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material not found: %s"), *Path));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> ScalarParams;
	TArray<TSharedPtr<FJsonValue>> VectorParams;
	TArray<TSharedPtr<FJsonValue>> TextureParams;

	TArray<FMaterialParameterInfo> ParameterInfo;
	TArray<FGuid> ParameterIds;

	// Get scalar parameters
	MaterialInterface->GetAllScalarParameterInfo(ParameterInfo, ParameterIds);
	for (const FMaterialParameterInfo& Info : ParameterInfo)
	{
		float Value;
		if (MaterialInterface->GetScalarParameterValue(Info, Value))
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), Info.Name.ToString());
			ParamObj->SetNumberField(TEXT("value"), Value);
			ParamObj->SetStringField(TEXT("type"), TEXT("scalar"));
			ScalarParams.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
	}

	// Get vector parameters
	ParameterInfo.Empty();
	ParameterIds.Empty();
	MaterialInterface->GetAllVectorParameterInfo(ParameterInfo, ParameterIds);
	for (const FMaterialParameterInfo& Info : ParameterInfo)
	{
		FLinearColor Value;
		if (MaterialInterface->GetVectorParameterValue(Info, Value))
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), Info.Name.ToString());
			ParamObj->SetObjectField(TEXT("value"), ColorToJson(Value));
			ParamObj->SetStringField(TEXT("type"), TEXT("vector"));
			VectorParams.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
	}

	// Get texture parameters
	ParameterInfo.Empty();
	ParameterIds.Empty();
	MaterialInterface->GetAllTextureParameterInfo(ParameterInfo, ParameterIds);
	for (const FMaterialParameterInfo& Info : ParameterInfo)
	{
		UTexture* Value;
		if (MaterialInterface->GetTextureParameterValue(Info, Value))
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), Info.Name.ToString());
			ParamObj->SetStringField(TEXT("value"), Value ? Value->GetPathName() : TEXT(""));
			ParamObj->SetStringField(TEXT("type"), TEXT("texture"));
			TextureParams.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("scalarParameters"), ScalarParams);
	ResultObj->SetArrayField(TEXT("vectorParameters"), VectorParams);
	ResultObj->SetArrayField(TEXT("textureParameters"), TextureParams);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleSetMaterialParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// This requires editing the material graph for base materials
	// For now, return an error directing to use material instances
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Cannot set parameters on base materials directly. Use materialInstance.setScalar/setVector/setTexture for material instances."));
	return false;
}

bool FUltimateControlMaterialHandler::HandleGetMaterialParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString ParameterName;
	if (!RequireString(Params, TEXT("name"), ParameterName, Error))
	{
		return false;
	}

	UMaterialInterface* MaterialInterface = LoadObject<UMaterialInterface>(nullptr, *Path);
	if (!MaterialInterface)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material not found: %s"), *Path));
		return false;
	}

	// UE 5.6+: Use FHashedMaterialParameterInfo; earlier versions use FMaterialParameterInfo
	// Use brace initialization to avoid "most vexing parse" ambiguity
#if ULTIMATE_CONTROL_UE_5_6_OR_LATER
	FHashedMaterialParameterInfo ParamInfo{FName{*ParameterName}};
#else
	FMaterialParameterInfo ParamInfo{FName{*ParameterName}};
#endif

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), ParameterName);

	// Try scalar
	float ScalarValue;
	if (MaterialInterface->GetScalarParameterValue(ParamInfo, ScalarValue))
	{
		ResultObj->SetStringField(TEXT("type"), TEXT("scalar"));
		ResultObj->SetNumberField(TEXT("value"), ScalarValue);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	// Try vector
	FLinearColor VectorValue;
	if (MaterialInterface->GetVectorParameterValue(ParamInfo, VectorValue))
	{
		ResultObj->SetStringField(TEXT("type"), TEXT("vector"));
		ResultObj->SetObjectField(TEXT("value"), ColorToJson(VectorValue));
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	// Try texture
	UTexture* TextureValue;
	if (MaterialInterface->GetTextureParameterValue(ParamInfo, TextureValue))
	{
		ResultObj->SetStringField(TEXT("type"), TEXT("texture"));
		ResultObj->SetStringField(TEXT("value"), TextureValue ? TextureValue->GetPathName() : TEXT(""));
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Parameter not found: %s"), *ParameterName));
	return false;
}

bool FUltimateControlMaterialHandler::HandleGetMaterialNodes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
	if (!Material)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material not found: %s"), *Path));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (Expression)
		{
			NodesArray.Add(MakeShared<FJsonValueObject>(MaterialExpressionToJson(Expression)));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("nodes"), NodesArray);
	ResultObj->SetNumberField(TEXT("count"), NodesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleAddMaterialNode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString NodeClass;
	if (!RequireString(Params, TEXT("class"), NodeClass, Error))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
	if (!Material)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material not found: %s"), *Path));
		return false;
	}

	// Find the expression class
	UClass* ExpressionClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.MaterialExpression%s"), *NodeClass));
	if (!ExpressionClass)
	{
		// Try with full class name
		ExpressionClass = FindObject<UClass>(nullptr, *NodeClass);
	}

	if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material expression class not found: %s"), *NodeClass));
		return false;
	}

	// Create the expression
	UMaterialExpression* NewExpression = NewObject<UMaterialExpression>(Material, ExpressionClass, NAME_None, RF_Transactional);
	if (!NewExpression)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Failed to create material expression"));
		return false;
	}

	// Set position if provided
	if (Params->HasField(TEXT("positionX")))
	{
		NewExpression->MaterialExpressionEditorX = FMath::RoundToInt(Params->GetNumberField(TEXT("positionX")));
	}
	if (Params->HasField(TEXT("positionY")))
	{
		NewExpression->MaterialExpressionEditorY = FMath::RoundToInt(Params->GetNumberField(TEXT("positionY")));
	}

	// Add to material
	Material->GetExpressionCollection().AddExpression(NewExpression);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	Result = MakeShared<FJsonValueObject>(MaterialExpressionToJson(NewExpression));
	return true;
}

bool FUltimateControlMaterialHandler::HandleDeleteMaterialNode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString NodeName;
	if (!RequireString(Params, TEXT("node"), NodeName, Error))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
	if (!Material)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material not found: %s"), *Path));
		return false;
	}

	// Find the expression
	UMaterialExpression* FoundExpression = nullptr;
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (Expression && Expression->GetName() == NodeName)
		{
			FoundExpression = Expression;
			break;
		}
	}

	if (!FoundExpression)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Node not found: %s"), *NodeName));
		return false;
	}

	// Remove from material
	Material->GetExpressionCollection().RemoveExpression(FoundExpression);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Material node connections are complex and require graph manipulation
	// This is a simplified stub
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Material node connections via API not fully implemented. Use the material editor."));
	return false;
}

bool FUltimateControlMaterialHandler::HandleDisconnectMaterialNode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Material node disconnections via API not fully implemented. Use the material editor."));
	return false;
}

bool FUltimateControlMaterialHandler::HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
	if (!Material)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material not found: %s"), *Path));
		return false;
	}

	// Force recompile
	Material->ForceRecompileForRendering();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleGetCompileErrors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
	if (!Material)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material not found: %s"), *Path));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	// Note: Getting compile errors requires accessing internal material data
	// This is a simplified implementation

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("errors"), ErrorsArray);
	ResultObj->SetNumberField(TEXT("errorCount"), ErrorsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleListMaterialInstances(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path = TEXT("/Game");
	if (Params->HasField(TEXT("path")))
	{
		Path = Params->GetStringField(TEXT("path"));
	}

	int32 Limit = 500;
	if (Params->HasField(TEXT("limit")))
	{
		Limit = FMath::Clamp(FMath::RoundToInt(Params->GetNumberField(TEXT("limit"))), 1, 10000);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	FARFilter Filter;
	Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> InstancesArray;
	for (int32 i = 0; i < FMath::Min(AssetDataList.Num(), Limit); i++)
	{
		const FAssetData& AssetData = AssetDataList[i];
		TSharedPtr<FJsonObject> InstObj = MakeShared<FJsonObject>();
		InstObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		InstObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("instances"), InstancesArray);
	ResultObj->SetNumberField(TEXT("count"), InstancesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString ParentPath;
	if (!RequireString(Params, TEXT("parent"), ParentPath, Error))
	{
		return false;
	}

	UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
	if (!ParentMaterial)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Parent material not found: %s"), *ParentPath));
		return false;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(Path);
	FString AssetName = FPackageName::GetShortName(Path);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMaterial;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory);

	if (!NewAsset)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, FString::Printf(TEXT("Failed to create material instance at: %s"), *Path));
		return false;
	}

	UMaterialInstanceConstant* NewMIC = Cast<UMaterialInstanceConstant>(NewAsset);
	Result = MakeShared<FJsonValueObject>(MaterialInstanceToJson(NewMIC));
	return true;
}

bool FUltimateControlMaterialHandler::HandleGetMaterialInstanceParent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	UMaterialInstance* MI = LoadObject<UMaterialInstance>(nullptr, *Path);
	if (!MI)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material instance not found: %s"), *Path));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	if (MI->Parent)
	{
		ResultObj->SetStringField(TEXT("parent"), MI->Parent->GetPathName());
		ResultObj->SetStringField(TEXT("parentClass"), MI->Parent->GetClass()->GetName());
	}
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleSetMaterialInstanceParent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString ParentPath;
	if (!RequireString(Params, TEXT("parent"), ParentPath, Error))
	{
		return false;
	}

	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *Path);
	if (!MIC)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material instance not found: %s"), *Path));
		return false;
	}

	UMaterialInterface* NewParent = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
	if (!NewParent)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Parent material not found: %s"), *ParentPath));
		return false;
	}

	MIC->SetParentEditorOnly(NewParent);
	MIC->PostEditChange();
	MIC->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleSetInstanceScalarParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString ParameterName;
	if (!RequireString(Params, TEXT("name"), ParameterName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("value")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: value"));
		return false;
	}
	float Value = Params->GetNumberField(TEXT("value"));

	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *Path);
	if (!MIC)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material instance not found: %s"), *Path));
		return false;
	}

	MIC->SetScalarParameterValueEditorOnly(FName(*ParameterName), Value);
	MIC->PostEditChange();
	MIC->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleSetInstanceVectorParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString ParameterName;
	if (!RequireString(Params, TEXT("name"), ParameterName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("value")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: value"));
		return false;
	}
	FLinearColor Value = JsonToColor(Params->GetObjectField(TEXT("value")));

	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *Path);
	if (!MIC)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material instance not found: %s"), *Path));
		return false;
	}

	MIC->SetVectorParameterValueEditorOnly(FName(*ParameterName), Value);
	MIC->PostEditChange();
	MIC->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleSetInstanceTextureParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString ParameterName;
	if (!RequireString(Params, TEXT("name"), ParameterName, Error))
	{
		return false;
	}

	FString TexturePath;
	if (!RequireString(Params, TEXT("value"), TexturePath, Error))
	{
		return false;
	}

	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *Path);
	if (!MIC)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material instance not found: %s"), *Path));
		return false;
	}

	UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
	if (!Texture && !TexturePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Texture not found: %s"), *TexturePath));
		return false;
	}

	MIC->SetTextureParameterValueEditorOnly(FName(*ParameterName), Texture);
	MIC->PostEditChange();
	MIC->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlMaterialHandler::HandleGetInstanceParameters(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *Path);
	if (!MIC)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Material instance not found: %s"), *Path));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> ScalarParams;
	TArray<TSharedPtr<FJsonValue>> VectorParams;
	TArray<TSharedPtr<FJsonValue>> TextureParams;

	// Scalar parameters
	for (const FScalarParameterValue& Param : MIC->ScalarParameterValues)
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
		ParamObj->SetNumberField(TEXT("value"), Param.ParameterValue);
		ScalarParams.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	// Vector parameters
	for (const FVectorParameterValue& Param : MIC->VectorParameterValues)
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
		ParamObj->SetObjectField(TEXT("value"), ColorToJson(Param.ParameterValue));
		VectorParams.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	// Texture parameters
	for (const FTextureParameterValue& Param : MIC->TextureParameterValues)
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), Param.ParameterInfo.Name.ToString());
		ParamObj->SetStringField(TEXT("value"), Param.ParameterValue ? Param.ParameterValue->GetPathName() : TEXT(""));
		TextureParams.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("scalarParameters"), ScalarParams);
	ResultObj->SetArrayField(TEXT("vectorParameters"), VectorParams);
	ResultObj->SetArrayField(TEXT("textureParameters"), TextureParams);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
