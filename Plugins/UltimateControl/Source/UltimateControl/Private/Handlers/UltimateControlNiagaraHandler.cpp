// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlNiagaraHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraActor.h"
// NiagaraEmitter.h removed - not needed for component-level Niagara control
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

void FUltimateControlNiagaraHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	// System listing and info
	Methods.Add(TEXT("niagara.listSystems"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleListNiagaraSystems));
	Methods.Add(TEXT("niagara.getSystem"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleGetNiagaraSystem));
	Methods.Add(TEXT("niagara.listEmitters"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleListEmitters));

	// Spawning and management
	Methods.Add(TEXT("niagara.spawn"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSpawnNiagaraSystem));
	Methods.Add(TEXT("niagara.spawnAttached"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSpawnNiagaraSystemAttached));
	Methods.Add(TEXT("niagara.destroy"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleDestroyNiagaraComponent));

	// Component control
	Methods.Add(TEXT("niagara.getComponents"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleGetNiagaraComponents));
	Methods.Add(TEXT("niagara.activate"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleActivateNiagaraComponent));
	Methods.Add(TEXT("niagara.deactivate"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleDeactivateNiagaraComponent));
	Methods.Add(TEXT("niagara.reset"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleResetNiagaraComponent));
	Methods.Add(TEXT("niagara.reinitialize"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleReinitializeNiagaraComponent));

	// Parameters
	Methods.Add(TEXT("niagara.getParameters"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleGetNiagaraParameters));
	Methods.Add(TEXT("niagara.setFloat"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSetNiagaraFloatParameter));
	Methods.Add(TEXT("niagara.setVector"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSetNiagaraVectorParameter));
	Methods.Add(TEXT("niagara.setColor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSetNiagaraColorParameter));
	Methods.Add(TEXT("niagara.setBool"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSetNiagaraBoolParameter));
	Methods.Add(TEXT("niagara.setInt"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSetNiagaraIntParameter));

	// Emitter control
	Methods.Add(TEXT("niagara.setEmitterEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSetEmitterEnabled));
	Methods.Add(TEXT("niagara.getEmitterEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleGetEmitterEnabled));

	// Debug
	Methods.Add(TEXT("niagara.getStats"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleGetNiagaraStats));
	Methods.Add(TEXT("niagara.setDebugHUD"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlNiagaraHandler::HandleSetNiagaraDebugHUD));
}

TSharedPtr<FJsonObject> FUltimateControlNiagaraHandler::NiagaraSystemToJson(UNiagaraSystem* System)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!System)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), System->GetName());
	Json->SetStringField(TEXT("path"), System->GetPathName());
	Json->SetBoolField(TEXT("isValid"), System->IsValid());

	// Get emitter count
	TArray<TSharedPtr<FJsonValue>> EmittersArray;
	for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
	{
		TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
		EmitterJson->SetStringField(TEXT("name"), EmitterHandle.GetName().ToString());
		EmitterJson->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());
		EmittersArray.Add(MakeShared<FJsonValueObject>(EmitterJson));
	}
	Json->SetArrayField(TEXT("emitters"), EmittersArray);

	return Json;
}

TSharedPtr<FJsonObject> FUltimateControlNiagaraHandler::NiagaraComponentToJson(UNiagaraComponent* Component)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Component)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Component->GetName());
	Json->SetBoolField(TEXT("isActive"), Component->IsActive());
	Json->SetBoolField(TEXT("isPaused"), Component->IsPaused());

	if (UNiagaraSystem* System = Component->GetAsset())
	{
		Json->SetStringField(TEXT("systemName"), System->GetName());
		Json->SetStringField(TEXT("systemPath"), System->GetPathName());
	}

	AActor* Owner = Component->GetOwner();
	if (Owner)
	{
		Json->SetStringField(TEXT("ownerName"), Owner->GetActorLabel());
	}

	// Location
	FVector Location = Component->GetComponentLocation();
	TSharedPtr<FJsonObject> LocationJson = MakeShared<FJsonObject>();
	LocationJson->SetNumberField(TEXT("x"), Location.X);
	LocationJson->SetNumberField(TEXT("y"), Location.Y);
	LocationJson->SetNumberField(TEXT("z"), Location.Z);
	Json->SetObjectField(TEXT("location"), LocationJson);

	return Json;
}

UNiagaraComponent* FUltimateControlNiagaraHandler::FindNiagaraComponent(const FString& ComponentName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		TArray<UNiagaraComponent*> NiagaraComponents;
		Actor->GetComponents<UNiagaraComponent>(NiagaraComponents);

		for (UNiagaraComponent* Component : NiagaraComponents)
		{
			if (Component && Component->GetName() == ComponentName)
			{
				return Component;
			}
		}
	}

	return nullptr;
}

bool FUltimateControlNiagaraHandler::HandleListNiagaraSystems(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Filter = Params->HasField(TEXT("filter")) ? Params->GetStringField(TEXT("filter")) : TEXT("");

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(UNiagaraSystem::StaticClass()->GetClassPathName(), AssetDataList);

	TArray<TSharedPtr<FJsonValue>> SystemsArray;

	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetName = AssetData.AssetName.ToString();
		if (!Filter.IsEmpty() && !AssetName.Contains(Filter))
		{
			continue;
		}

		TSharedPtr<FJsonObject> SystemJson = MakeShared<FJsonObject>();
		SystemJson->SetStringField(TEXT("name"), AssetName);
		SystemJson->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		SystemJson->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());

		SystemsArray.Add(MakeShared<FJsonValueObject>(SystemJson));
	}

	Result = MakeShared<FJsonValueArray>(SystemsArray);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleGetNiagaraSystem(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path = Params->GetStringField(TEXT("path"));
	if (Path.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("path parameter required"));
		return true;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Path);
	if (!System)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara system not found: %s"), *Path));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(NiagaraSystemToJson(System));
	return true;
}

bool FUltimateControlNiagaraHandler::HandleListEmitters(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SystemPath = Params->GetStringField(TEXT("systemPath"));
	if (SystemPath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("systemPath parameter required"));
		return true;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara system not found: %s"), *SystemPath));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> EmittersArray;

	for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
	{
		TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
		EmitterJson->SetStringField(TEXT("name"), EmitterHandle.GetName().ToString());
		EmitterJson->SetStringField(TEXT("uniqueName"), EmitterHandle.GetUniqueInstanceName());
		EmitterJson->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());

		EmittersArray.Add(MakeShared<FJsonValueObject>(EmitterJson));
	}

	Result = MakeShared<FJsonValueArray>(EmittersArray);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSpawnNiagaraSystem(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SystemPath = Params->GetStringField(TEXT("systemPath"));
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));
	double Z = Params->GetNumberField(TEXT("z"));
	bool bAutoDestroy = Params->HasField(TEXT("autoDestroy")) ? Params->GetBoolField(TEXT("autoDestroy")) : true;

	if (SystemPath.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("systemPath parameter required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara system not found: %s"), *SystemPath));
		return true;
	}

	FVector Location(X, Y, Z);
	FRotator Rotation = FRotator::ZeroRotator;

	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, System, Location, Rotation, FVector::OneVector, bAutoDestroy);

	if (!Component)
	{
		Error = CreateError(-32603, TEXT("Failed to spawn Niagara system"));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(NiagaraComponentToJson(Component));
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSpawnNiagaraSystemAttached(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SystemPath = Params->GetStringField(TEXT("systemPath"));
	FString ActorName = Params->GetStringField(TEXT("actorName"));
	FString SocketName = Params->HasField(TEXT("socketName")) ? Params->GetStringField(TEXT("socketName")) : TEXT("");

	if (SystemPath.IsEmpty() || ActorName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("systemPath and actorName parameters required"));
		return true;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	// Find target actor
	AActor* TargetActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName)
		{
			TargetActor = *It;
			break;
		}
	}

	if (!TargetActor)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return true;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara system not found: %s"), *SystemPath));
		return true;
	}

	USceneComponent* AttachComponent = TargetActor->GetRootComponent();
	if (!AttachComponent)
	{
		Error = CreateError(-32603, TEXT("Target actor has no root component"));
		return true;
	}

	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System, AttachComponent, FName(*SocketName), FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, true);

	if (!Component)
	{
		Error = CreateError(-32603, TEXT("Failed to spawn attached Niagara system"));
		return true;
	}

	Result = MakeShared<FJsonValueObject>(NiagaraComponentToJson(Component));
	return true;
}

bool FUltimateControlNiagaraHandler::HandleDestroyNiagaraComponent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	if (ComponentName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName parameter required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->DestroyComponent();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleGetNiagaraComponents(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName = Params->HasField(TEXT("actorName")) ? Params->GetStringField(TEXT("actorName")) : TEXT("");

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ComponentsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		if (!ActorName.IsEmpty() && Actor->GetActorLabel() != ActorName)
		{
			continue;
		}

		TArray<UNiagaraComponent*> NiagaraComponents;
		Actor->GetComponents<UNiagaraComponent>(NiagaraComponents);

		for (UNiagaraComponent* Component : NiagaraComponents)
		{
			if (Component)
			{
				ComponentsArray.Add(MakeShared<FJsonValueObject>(NiagaraComponentToJson(Component)));
			}
		}
	}

	Result = MakeShared<FJsonValueArray>(ComponentsArray);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleActivateNiagaraComponent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	if (ComponentName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName parameter required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->Activate(true);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetBoolField(TEXT("isActive"), Component->IsActive());
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleDeactivateNiagaraComponent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	if (ComponentName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName parameter required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->Deactivate();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetBoolField(TEXT("isActive"), Component->IsActive());
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleResetNiagaraComponent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	if (ComponentName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName parameter required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->ResetSystem();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleReinitializeNiagaraComponent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	if (ComponentName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName parameter required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->ReinitializeSystem();

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleGetNiagaraParameters(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	if (ComponentName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName parameter required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ParametersArray;

	// Get override parameters from the component
	const TArray<FNiagaraVariableBase>& OverrideParameters = Component->GetOverrideParameters().ReadParameterVariables();
	for (const FNiagaraVariableBase& Var : OverrideParameters)
	{
		TSharedPtr<FJsonObject> ParamJson = MakeShared<FJsonObject>();
		ParamJson->SetStringField(TEXT("name"), Var.GetName().ToString());
		ParamJson->SetStringField(TEXT("type"), Var.GetType().GetName());
		ParametersArray.Add(MakeShared<FJsonValueObject>(ParamJson));
	}

	Result = MakeShared<FJsonValueArray>(ParametersArray);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSetNiagaraFloatParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	FString ParameterName = Params->GetStringField(TEXT("parameterName"));
	float Value = static_cast<float>(Params->GetNumberField(TEXT("value")));

	if (ComponentName.IsEmpty() || ParameterName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName and parameterName parameters required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->SetVariableFloat(FName(*ParameterName), Value);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSetNiagaraVectorParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	FString ParameterName = Params->GetStringField(TEXT("parameterName"));
	double X = Params->GetNumberField(TEXT("x"));
	double Y = Params->GetNumberField(TEXT("y"));
	double Z = Params->GetNumberField(TEXT("z"));

	if (ComponentName.IsEmpty() || ParameterName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName and parameterName parameters required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->SetVariableVec3(FName(*ParameterName), FVector(X, Y, Z));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSetNiagaraColorParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	FString ParameterName = Params->GetStringField(TEXT("parameterName"));
	float R = static_cast<float>(Params->GetNumberField(TEXT("r")));
	float G = static_cast<float>(Params->GetNumberField(TEXT("g")));
	float B = static_cast<float>(Params->GetNumberField(TEXT("b")));
	float A = Params->HasField(TEXT("a")) ? static_cast<float>(Params->GetNumberField(TEXT("a"))) : 1.0f;

	if (ComponentName.IsEmpty() || ParameterName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName and parameterName parameters required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->SetVariableLinearColor(FName(*ParameterName), FLinearColor(R, G, B, A));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSetNiagaraBoolParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	FString ParameterName = Params->GetStringField(TEXT("parameterName"));
	bool Value = Params->GetBoolField(TEXT("value"));

	if (ComponentName.IsEmpty() || ParameterName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName and parameterName parameters required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->SetVariableBool(FName(*ParameterName), Value);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSetNiagaraIntParameter(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	FString ParameterName = Params->GetStringField(TEXT("parameterName"));
	int32 Value = static_cast<int32>(Params->GetIntegerField(TEXT("value")));

	if (ComponentName.IsEmpty() || ParameterName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName and parameterName parameters required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->SetVariableInt(FName(*ParameterName), Value);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSetEmitterEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	FString EmitterName = Params->GetStringField(TEXT("emitterName"));
	bool bEnabled = Params->GetBoolField(TEXT("enabled"));

	if (ComponentName.IsEmpty() || EmitterName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName and emitterName parameters required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	Component->SetEmitterEnable(FName(*EmitterName), bEnabled);

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleGetEmitterEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->GetStringField(TEXT("componentName"));
	FString EmitterName = Params->GetStringField(TEXT("emitterName"));

	if (ComponentName.IsEmpty() || EmitterName.IsEmpty())
	{
		Error = CreateError(-32602, TEXT("componentName and emitterName parameters required"));
		return true;
	}

	UNiagaraComponent* Component = FindNiagaraComponent(ComponentName);
	if (!Component)
	{
		Error = CreateError(-32602, FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
		return true;
	}

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetStringField(TEXT("emitterName"), EmitterName);
	// Note: There's no direct GetEmitterEnable, we'd need to check system state

	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleGetNiagaraStats(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ComponentName = Params->HasField(TEXT("componentName")) ? Params->GetStringField(TEXT("componentName")) : TEXT("");

	TSharedPtr<FJsonObject> StatsJson = MakeShared<FJsonObject>();

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = CreateError(-32603, TEXT("No editor world available"));
		return true;
	}

	int32 TotalComponents = 0;
	int32 ActiveComponents = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		TArray<UNiagaraComponent*> NiagaraComponents;
		Actor->GetComponents<UNiagaraComponent>(NiagaraComponents);

		for (UNiagaraComponent* Component : NiagaraComponents)
		{
			if (Component)
			{
				TotalComponents++;
				if (Component->IsActive())
				{
					ActiveComponents++;
				}
			}
		}
	}

	StatsJson->SetNumberField(TEXT("totalComponents"), TotalComponents);
	StatsJson->SetNumberField(TEXT("activeComponents"), ActiveComponents);

	Result = MakeShared<FJsonValueObject>(StatsJson);
	return true;
}

bool FUltimateControlNiagaraHandler::HandleSetNiagaraDebugHUD(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	bool bEnabled = Params->GetBoolField(TEXT("enabled"));

	// Niagara debug HUD is typically controlled through console commands
	GEngine->Exec(nullptr, bEnabled ? TEXT("fx.Niagara.Debug.Enabled 1") : TEXT("fx.Niagara.Debug.Enabled 0"));

	TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
	ResultJson->SetBoolField(TEXT("success"), true);
	ResultJson->SetBoolField(TEXT("enabled"), bEnabled);
	Result = MakeShared<FJsonValueObject>(ResultJson);
	return true;
}
