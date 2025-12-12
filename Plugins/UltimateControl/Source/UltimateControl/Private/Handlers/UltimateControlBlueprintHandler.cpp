// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlBlueprintHandler.h"
#include "UltimateControl.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"
#include "Factories/BlueprintFactory.h"
#include "AssetToolsModule.h"

FUltimateControlBlueprintHandler::FUltimateControlBlueprintHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(
		TEXT("blueprint.list"),
		TEXT("List all blueprints in the project or specified path"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleList));

	RegisterMethod(
		TEXT("blueprint.get"),
		TEXT("Get detailed information about a blueprint"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleGet));

	RegisterMethod(
		TEXT("blueprint.getGraphs"),
		TEXT("Get all graphs (event graph, functions, macros) in a blueprint"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleGetGraphs));

	RegisterMethod(
		TEXT("blueprint.getNodes"),
		TEXT("Get all nodes in a specific graph"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleGetNodes));

	RegisterMethod(
		TEXT("blueprint.getVariables"),
		TEXT("Get all variables defined in a blueprint"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleGetVariables));

	RegisterMethod(
		TEXT("blueprint.getFunctions"),
		TEXT("Get all functions defined in a blueprint"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleGetFunctions));

	RegisterMethod(
		TEXT("blueprint.getEventDispatchers"),
		TEXT("Get all event dispatchers in a blueprint"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleGetEventDispatchers));

	RegisterMethod(
		TEXT("blueprint.compile"),
		TEXT("Compile a blueprint"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleCompile));

	RegisterMethod(
		TEXT("blueprint.create"),
		TEXT("Create a new blueprint class"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleCreate));

	RegisterMethod(
		TEXT("blueprint.addVariable"),
		TEXT("Add a new variable to a blueprint"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleAddVariable));

	RegisterMethod(
		TEXT("blueprint.addFunction"),
		TEXT("Add a new function to a blueprint"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleAddFunction));

	RegisterMethod(
		TEXT("blueprint.addNode"),
		TEXT("Add a node to a blueprint graph"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleAddNode));

	RegisterMethod(
		TEXT("blueprint.connectPins"),
		TEXT("Connect two pins in a blueprint graph"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleConnectPins));

	RegisterMethod(
		TEXT("blueprint.deleteNode"),
		TEXT("Delete a node from a blueprint graph"),
		TEXT("Blueprint"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlBlueprintHandler::HandleDeleteNode),
		/* bIsDangerous */ true);
}

UBlueprint* FUltimateControlBlueprintHandler::LoadBlueprint(const FString& Path, TSharedPtr<FJsonObject>& OutError)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(Path);
	if (!Asset)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Blueprint not found: %s"), *Path));
		return nullptr;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			FString::Printf(TEXT("Asset is not a blueprint: %s"), *Path));
		return nullptr;
	}

	return Blueprint;
}

TSharedPtr<FJsonObject> FUltimateControlBlueprintHandler::BlueprintToJson(UBlueprint* Blueprint)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("path"), Blueprint->GetPathName());
	Obj->SetStringField(TEXT("name"), Blueprint->GetName());
	Obj->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));
	Obj->SetStringField(TEXT("blueprintType"), StaticEnum<EBlueprintType>()->GetNameStringByValue(static_cast<int64>(Blueprint->BlueprintType)));

	// Compile status
	FString StatusStr;
	switch (Blueprint->Status)
	{
	case BS_Unknown: StatusStr = TEXT("Unknown"); break;
	case BS_Dirty: StatusStr = TEXT("Dirty"); break;
	case BS_Error: StatusStr = TEXT("Error"); break;
	case BS_UpToDate: StatusStr = TEXT("UpToDate"); break;
	case BS_BeingCreated: StatusStr = TEXT("BeingCreated"); break;
	case BS_UpToDateWithWarnings: StatusStr = TEXT("UpToDateWithWarnings"); break;
	default: StatusStr = TEXT("Unknown"); break;
	}
	Obj->SetStringField(TEXT("status"), StatusStr);

	// Count elements
	Obj->SetNumberField(TEXT("graphCount"), Blueprint->UbergraphPages.Num() + Blueprint->FunctionGraphs.Num());
	Obj->SetNumberField(TEXT("variableCount"), Blueprint->NewVariables.Num());

	return Obj;
}

TSharedPtr<FJsonObject> FUltimateControlBlueprintHandler::GraphToJson(UEdGraph* Graph)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("name"), Graph->GetName());
	Obj->SetStringField(TEXT("guid"), Graph->GraphGuid.ToString());
	Obj->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());

	// Determine graph type
	FString GraphType = TEXT("Unknown");
	if (Graph->GetSchema())
	{
		GraphType = Graph->GetSchema()->GetClass()->GetName();
	}
	Obj->SetStringField(TEXT("schemaClass"), GraphType);

	return Obj;
}

TSharedPtr<FJsonObject> FUltimateControlBlueprintHandler::NodeToJson(UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
	Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	Obj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Obj->SetNumberField(TEXT("posX"), Node->NodePosX);
	Obj->SetNumberField(TEXT("posY"), Node->NodePosY);
	Obj->SetStringField(TEXT("comment"), Node->NodeComment);
	Obj->SetBoolField(TEXT("hasCompilerMessage"), Node->bHasCompilerMessage);

	// Get pins
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("id"), Pin->PinId.ToString());
			PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinObj->SetBoolField(TEXT("hidden"), Pin->bHidden);
			PinObj->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);

			// Connected pins
			TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->GetOwningNode())
				{
					TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
					ConnObj->SetStringField(TEXT("nodeGuid"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
					ConnObj->SetStringField(TEXT("pinId"), LinkedPin->PinId.ToString());
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
				}
			}
			PinObj->SetArrayField(TEXT("connections"), ConnectionsArray);

			PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}
	Obj->SetArrayField(TEXT("pins"), PinsArray);

	return Obj;
}

bool FUltimateControlBlueprintHandler::HandleList(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path = GetOptionalString(Params, TEXT("path"), TEXT("/Game"));
	bool bRecursive = GetOptionalBool(Params, TEXT("recursive"), true);
	int32 Limit = GetOptionalInt(Params, TEXT("limit"), 500);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = bRecursive;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	TArray<TSharedPtr<FJsonValue>> BlueprintsArray;
	for (int32 i = 0; i < FMath::Min(AssetList.Num(), Limit); ++i)
	{
		TSharedPtr<FJsonObject> BpObj = MakeShared<FJsonObject>();
		BpObj->SetStringField(TEXT("path"), AssetList[i].GetObjectPathString());
		BpObj->SetStringField(TEXT("name"), AssetList[i].AssetName.ToString());
		BlueprintsArray.Add(MakeShared<FJsonValueObject>(BpObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("blueprints"), BlueprintsArray);
	ResultObj->SetNumberField(TEXT("totalCount"), AssetList.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleGet(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	OutResult = MakeShared<FJsonValueObject>(BlueprintToJson(Blueprint));
	return true;
}

bool FUltimateControlBlueprintHandler::HandleGetGraphs(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	// Add ubergraph pages (event graphs)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph)
		{
			TSharedPtr<FJsonObject> GraphObj = GraphToJson(Graph);
			GraphObj->SetStringField(TEXT("type"), TEXT("EventGraph"));
			GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
		}
	}

	// Add function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			TSharedPtr<FJsonObject> GraphObj = GraphToJson(Graph);
			GraphObj->SetStringField(TEXT("type"), TEXT("Function"));
			GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
		}
	}

	// Add macro graphs
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph)
		{
			TSharedPtr<FJsonObject> GraphObj = GraphToJson(Graph);
			GraphObj->SetStringField(TEXT("type"), TEXT("Macro"));
			GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(GraphsArray);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleGetNodes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	FString GraphName;

	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("graph"), GraphName, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	// Find the graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}
	if (!TargetGraph)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	if (!TargetGraph)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Graph not found: %s"), *GraphName));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node)
		{
			NodesArray.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(NodesArray);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleGetVariables(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> VariablesArray;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		VarObj->SetStringField(TEXT("category"), Var.Category.ToString());
		VarObj->SetStringField(TEXT("defaultValue"), Var.DefaultValue);
		VarObj->SetBoolField(TEXT("isInstanceEditable"), Var.PropertyFlags & CPF_Edit);
		VarObj->SetBoolField(TEXT("isBlueprintReadOnly"), Var.PropertyFlags & CPF_BlueprintReadOnly);
		VarObj->SetBoolField(TEXT("isExposeOnSpawn"), Var.PropertyFlags & CPF_ExposeOnSpawn);
		VariablesArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}

	OutResult = MakeShared<FJsonValueArray>(VariablesArray);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleGetFunctions(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
			FuncObj->SetStringField(TEXT("name"), Graph->GetName());
			FuncObj->SetStringField(TEXT("guid"), Graph->GraphGuid.ToString());

			// Find the function entry node to get parameter info
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
				{
					TArray<TSharedPtr<FJsonValue>> ParamsArray;
					for (UEdGraphPin* Pin : EntryNode->Pins)
					{
						if (Pin && Pin->Direction == EGPD_Output && !Pin->PinType.PinCategory.IsNone())
						{
							TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
							ParamObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
							ParamObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
							ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
						}
					}
					FuncObj->SetArrayField(TEXT("parameters"), ParamsArray);
					break;
				}
			}

			FunctionsArray.Add(MakeShared<FJsonValueObject>(FuncObj));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(FunctionsArray);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleGetEventDispatchers(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> DispatchersArray;
	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (Graph)
		{
			TSharedPtr<FJsonObject> DispObj = MakeShared<FJsonObject>();
			DispObj->SetStringField(TEXT("name"), Graph->GetName());
			DispObj->SetStringField(TEXT("guid"), Graph->GraphGuid.ToString());
			DispatchersArray.Add(MakeShared<FJsonValueObject>(DispObj));
		}
	}

	OutResult = MakeShared<FJsonValueArray>(DispatchersArray);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleCompile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), Blueprint->Status != BS_Error);
	ResultObj->SetStringField(TEXT("status"), Blueprint->Status == BS_Error ? TEXT("Error") : TEXT("Success"));

	// Get compile messages
	TArray<TSharedPtr<FJsonValue>> MessagesArray;
	// Messages would require accessing the message log - simplified here
	ResultObj->SetArrayField(TEXT("messages"), MessagesArray);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleCreate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	FString ParentClass;

	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	ParentClass = GetOptionalString(Params, TEXT("parentClass"), TEXT("Actor"));

	// Find the parent class
	UClass* Parent = FindObject<UClass>(nullptr, *ParentClass);
	if (!Parent)
	{
		Parent = LoadObject<UClass>(nullptr, *ParentClass);
	}
	if (!Parent)
	{
		// Try common classes
		if (ParentClass == TEXT("Actor"))
		{
			Parent = AActor::StaticClass();
		}
		else if (ParentClass == TEXT("Pawn"))
		{
			Parent = APawn::StaticClass();
		}
		else if (ParentClass == TEXT("Character"))
		{
			Parent = ACharacter::StaticClass();
		}
		else
		{
			OutError = UUltimateControlSubsystem::MakeError(
				EJsonRpcError::NotFound,
				FString::Printf(TEXT("Parent class not found: %s"), *ParentClass));
			return false;
		}
	}

	// Extract package path and asset name from path
	FString PackagePath;
	FString AssetName;
	Path.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	// Create the blueprint
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = Parent;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UBlueprint::StaticClass(), Factory);

	if (!NewAsset)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			TEXT("Failed to create blueprint"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("path"), NewAsset->GetPathName());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleAddVariable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	FString VariableName;
	FString VariableType;

	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("name"), VariableName, OutError))
	{
		return false;
	}

	VariableType = GetOptionalString(Params, TEXT("type"), TEXT("bool"));

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	// Create the variable
	FEdGraphPinType PinType;
	PinType.PinCategory = FName(*VariableType);

	bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType);

	if (!bSuccess)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			TEXT("Failed to add variable"));
		return false;
	}

	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), VariableName);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleAddFunction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	FString FunctionName;

	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("name"), FunctionName, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	// Create a new function graph
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			TEXT("Failed to create function graph"));
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, /* bIsUserCreated */ true, nullptr);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), FunctionName);
	ResultObj->SetStringField(TEXT("guid"), NewGraph->GraphGuid.ToString());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleAddNode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	FString GraphName;
	FString NodeClass;

	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("graph"), GraphName, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("nodeClass"), NodeClass, OutError))
	{
		return false;
	}

	int32 PosX = GetOptionalInt(Params, TEXT("posX"), 0);
	int32 PosY = GetOptionalInt(Params, TEXT("posY"), 0);

	UBlueprint* Blueprint = LoadBlueprint(Path, OutError);
	if (!Blueprint)
	{
		return false;
	}

	// Find the graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}
	if (!TargetGraph)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	if (!TargetGraph)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Graph not found: %s"), *GraphName));
		return false;
	}

	// Find the node class
	UClass* NodeUClass = FindObject<UClass>(nullptr, *NodeClass);
	if (!NodeUClass)
	{
		NodeUClass = LoadObject<UClass>(nullptr, *NodeClass);
	}

	if (!NodeUClass || !NodeUClass->IsChildOf(UEdGraphNode::StaticClass()))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			FString::Printf(TEXT("Invalid node class: %s"), *NodeClass));
		return false;
	}

	// Create the node
	UEdGraphNode* NewNode = NewObject<UEdGraphNode>(TargetGraph, NodeUClass);
	NewNode->CreateNewGuid();
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NewNode->AllocateDefaultPins();
	TargetGraph->AddNode(NewNode);

	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("guid"), NewNode->NodeGuid.ToString());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlBlueprintHandler::HandleConnectPins(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	// This would need more complex implementation to find nodes and pins by GUID
	// Simplified implementation
	OutError = UUltimateControlSubsystem::MakeError(
		EJsonRpcError::InternalError,
		TEXT("ConnectPins not yet fully implemented"));
	return false;
}

bool FUltimateControlBlueprintHandler::HandleDeleteNode(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	// This would need more complex implementation to find nodes by GUID
	// Simplified implementation
	OutError = UUltimateControlSubsystem::MakeError(
		EJsonRpcError::InternalError,
		TEXT("DeleteNode not yet fully implemented"));
	return false;
}
