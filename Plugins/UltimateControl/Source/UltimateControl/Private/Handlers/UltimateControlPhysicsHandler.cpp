// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlPhysicsHandler.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "EngineUtils.h"

void FUltimateControlPhysicsHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	Methods.Add(TEXT("physics.getGravity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetGravity));
	Methods.Add(TEXT("physics.setGravity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSetGravity));
	Methods.Add(TEXT("physics.getSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetPhysicsSettings));
	Methods.Add(TEXT("physics.getSimulationSpeed"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetSimulationSpeed));
	Methods.Add(TEXT("physics.setSimulationSpeed"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSetSimulationSpeed));
	Methods.Add(TEXT("physics.pause"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandlePausePhysics));
	Methods.Add(TEXT("physics.resume"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleResumePhysics));
	Methods.Add(TEXT("physics.step"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleStepPhysics));
	Methods.Add(TEXT("physics.getEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetPhysicsEnabled));
	Methods.Add(TEXT("physics.setEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSetPhysicsEnabled));
	Methods.Add(TEXT("physics.getMass"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetMass));
	Methods.Add(TEXT("physics.setMass"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSetMass));
	Methods.Add(TEXT("physics.getVelocity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetVelocity));
	Methods.Add(TEXT("physics.setVelocity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSetVelocity));
	Methods.Add(TEXT("physics.getAngularVelocity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetAngularVelocity));
	Methods.Add(TEXT("physics.setAngularVelocity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSetAngularVelocity));
	Methods.Add(TEXT("physics.applyForce"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleApplyForce));
	Methods.Add(TEXT("physics.applyImpulse"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleApplyImpulse));
	Methods.Add(TEXT("physics.applyTorque"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleApplyTorque));
	Methods.Add(TEXT("physics.applyRadialForce"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleApplyRadialForce));
	Methods.Add(TEXT("physics.getCollisionEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetCollisionEnabled));
	Methods.Add(TEXT("physics.setCollisionEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSetCollisionEnabled));
	Methods.Add(TEXT("physics.getCollisionProfile"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleGetCollisionProfile));
	Methods.Add(TEXT("physics.setCollisionProfile"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSetCollisionProfile));
	Methods.Add(TEXT("physics.listCollisionProfiles"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleListCollisionProfiles));
	Methods.Add(TEXT("physics.lineTrace"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleLineTrace));
	Methods.Add(TEXT("physics.sphereTrace"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleSphereTrace));
	Methods.Add(TEXT("physics.boxTrace"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleBoxTrace));
	Methods.Add(TEXT("physics.overlapSphere"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleOverlapSphere));
	Methods.Add(TEXT("physics.overlapBox"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleOverlapBox));
	Methods.Add(TEXT("physics.wake"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleWakeRigidBody));
	Methods.Add(TEXT("physics.sleep"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandlePutRigidBodyToSleep));
	Methods.Add(TEXT("physics.isSleeping"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlPhysicsHandler::HandleIsSleeping));
}

UPrimitiveComponent* FUltimateControlPhysicsHandler::GetPrimitiveComponent(const FString& ActorName, const FString& ComponentName)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return nullptr;

	// Find actor by label using TActorIterator
	AActor* Actor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
		{
			Actor = *It;
			break;
		}
	}
	if (!Actor) return nullptr;

	if (ComponentName.IsEmpty())
	{
		return Cast<UPrimitiveComponent>(Actor->GetRootComponent());
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component && Component->GetName() == ComponentName)
		{
			return Cast<UPrimitiveComponent>(Component);
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> FUltimateControlPhysicsHandler::HitResultToJson(const FHitResult& HitResult)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetBoolField(TEXT("blockingHit"), HitResult.bBlockingHit);
	Result->SetBoolField(TEXT("startPenetrating"), HitResult.bStartPenetrating);
	Result->SetNumberField(TEXT("time"), HitResult.Time);
	Result->SetNumberField(TEXT("distance"), HitResult.Distance);
	Result->SetObjectField(TEXT("location"), VectorToJson(HitResult.Location));
	Result->SetObjectField(TEXT("impactPoint"), VectorToJson(HitResult.ImpactPoint));
	Result->SetObjectField(TEXT("normal"), VectorToJson(HitResult.Normal));
	Result->SetObjectField(TEXT("impactNormal"), VectorToJson(HitResult.ImpactNormal));

	if (HitResult.GetActor())
	{
		Result->SetStringField(TEXT("actor"), HitResult.GetActor()->GetName());
	}

	if (HitResult.GetComponent())
	{
		Result->SetStringField(TEXT("component"), HitResult.GetComponent()->GetName());
	}

	Result->SetStringField(TEXT("boneName"), HitResult.BoneName.ToString());
	Result->SetStringField(TEXT("physMaterial"), HitResult.PhysMaterial.IsValid() ? HitResult.PhysMaterial->GetName() : TEXT(""));

	return Result;
}

bool FUltimateControlPhysicsHandler::HandleGetGravity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	FVector Gravity = World->GetGravityZ() * FVector(0, 0, 1);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetObjectField(TEXT("gravity"), VectorToJson(Gravity));
	ResultObj->SetNumberField(TEXT("gravityZ"), World->GetGravityZ());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSetGravity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("World settings not available"));
		return false;
	}

	if (Params->HasField(TEXT("gravityZ")))
	{
		WorldSettings->GlobalGravityZ = Params->GetNumberField(TEXT("gravityZ"));
	}
	else if (Params->HasField(TEXT("gravity")))
	{
		FVector GravityVec = JsonToVector(Params->GetObjectField(TEXT("gravity")));
		WorldSettings->GlobalGravityZ = GravityVec.Z;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleGetPhysicsSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	const UPhysicsSettings* Settings = UPhysicsSettings::Get();
	if (!Settings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Physics settings not available"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("defaultGravityZ"), Settings->DefaultGravityZ);
	// Note: bEnableAsyncScene and bDefaultHasComplexCollision are deprecated in UE 5.6
	// These settings have been removed or reorganized in the physics settings

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleGetSimulationSpeed(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("World settings not available"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("timeDilation"), WorldSettings->TimeDilation);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSetSimulationSpeed(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("speed")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: speed"));
		return false;
	}
	float Speed = Params->GetNumberField(TEXT("speed"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("World settings not available"));
		return false;
	}

	WorldSettings->TimeDilation = FMath::Clamp(Speed, 0.0001f, 20.0f);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandlePausePhysics(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (WorldSettings)
	{
		WorldSettings->SetPauserPlayerState(nullptr);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleResumePhysics(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (WorldSettings)
	{
		WorldSettings->SetPauserPlayerState(nullptr);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleStepPhysics(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Stepping physics is complex and typically done through simulation
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Stepping physics requires PIE. Use pie.simulate for physics simulation."));
	return false;
}

bool FUltimateControlPhysicsHandler::HandleGetPhysicsEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	FString ComponentName;
	if (Params->HasField(TEXT("component")))
	{
		ComponentName = Params->GetStringField(TEXT("component"));
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, ComponentName);
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("simulatesPhysics"), Component->IsSimulatingPhysics());
	ResultObj->SetBoolField(TEXT("gravityEnabled"), Component->IsGravityEnabled());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSetPhysicsEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	FString ComponentName;
	if (Params->HasField(TEXT("component")))
	{
		ComponentName = Params->GetStringField(TEXT("component"));
	}

	bool bEnabled = true;
	if (Params->HasField(TEXT("enabled")))
	{
		bEnabled = Params->GetBoolField(TEXT("enabled"));
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, ComponentName);
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	Component->SetSimulatePhysics(bEnabled);

	if (Params->HasField(TEXT("gravity")))
	{
		Component->SetEnableGravity(Params->GetBoolField(TEXT("gravity")));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleGetMass(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("mass"), Component->GetMass());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSetMass(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("mass")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: mass"));
		return false;
	}
	float Mass = Params->GetNumberField(TEXT("mass"));

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	Component->SetMassOverrideInKg(NAME_None, Mass, true);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleGetVelocity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetObjectField(TEXT("velocity"), VectorToJson(Component->GetPhysicsLinearVelocity()));
	ResultObj->SetNumberField(TEXT("speed"), Component->GetPhysicsLinearVelocity().Size());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSetVelocity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("velocity")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: velocity"));
		return false;
	}
	FVector Velocity = JsonToVector(Params->GetObjectField(TEXT("velocity")));

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	Component->SetPhysicsLinearVelocity(Velocity);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleGetAngularVelocity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetObjectField(TEXT("angularVelocity"), VectorToJson(Component->GetPhysicsAngularVelocityInDegrees()));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSetAngularVelocity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("angularVelocity")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: angularVelocity"));
		return false;
	}
	FVector AngularVelocity = JsonToVector(Params->GetObjectField(TEXT("angularVelocity")));

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	Component->SetPhysicsAngularVelocityInDegrees(AngularVelocity);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleApplyForce(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("force")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: force"));
		return false;
	}
	FVector Force = JsonToVector(Params->GetObjectField(TEXT("force")));

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	FName BoneName = NAME_None;
	if (Params->HasField(TEXT("bone")))
	{
		BoneName = FName(*Params->GetStringField(TEXT("bone")));
	}

	bool bAccelChange = false;
	if (Params->HasField(TEXT("accelChange")))
	{
		bAccelChange = Params->GetBoolField(TEXT("accelChange"));
	}

	Component->AddForce(Force, BoneName, bAccelChange);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleApplyImpulse(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("impulse")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: impulse"));
		return false;
	}
	FVector Impulse = JsonToVector(Params->GetObjectField(TEXT("impulse")));

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	FName BoneName = NAME_None;
	if (Params->HasField(TEXT("bone")))
	{
		BoneName = FName(*Params->GetStringField(TEXT("bone")));
	}

	bool bVelChange = false;
	if (Params->HasField(TEXT("velChange")))
	{
		bVelChange = Params->GetBoolField(TEXT("velChange"));
	}

	Component->AddImpulse(Impulse, BoneName, bVelChange);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleApplyTorque(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("torque")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: torque"));
		return false;
	}
	FVector Torque = JsonToVector(Params->GetObjectField(TEXT("torque")));

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	FName BoneName = NAME_None;
	if (Params->HasField(TEXT("bone")))
	{
		BoneName = FName(*Params->GetStringField(TEXT("bone")));
	}

	bool bAccelChange = false;
	if (Params->HasField(TEXT("accelChange")))
	{
		bAccelChange = Params->GetBoolField(TEXT("accelChange"));
	}

	Component->AddTorqueInDegrees(Torque, BoneName, bAccelChange);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleApplyRadialForce(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("location")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: location"));
		return false;
	}
	FVector Location = JsonToVector(Params->GetObjectField(TEXT("location")));

	if (!Params->HasField(TEXT("radius")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: radius"));
		return false;
	}
	float Radius = Params->GetNumberField(TEXT("radius"));

	if (!Params->HasField(TEXT("strength")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: strength"));
		return false;
	}
	float Strength = Params->GetNumberField(TEXT("strength"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	// Apply radial force to all overlapping objects
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;

	bool bHit = World->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeSphere(Radius), QueryParams);

	int32 AffectedCount = 0;
	if (bHit)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (UPrimitiveComponent* Comp = Overlap.GetComponent())
			{
				if (Comp->IsSimulatingPhysics())
				{
					FVector Direction = (Comp->GetComponentLocation() - Location).GetSafeNormal();
					Comp->AddImpulse(Direction * Strength, NAME_None, true);
					AffectedCount++;
				}
			}
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetNumberField(TEXT("affectedCount"), AffectedCount);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleGetCollisionEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	FString CollisionEnabledStr;
	switch (Component->GetCollisionEnabled())
	{
	case ECollisionEnabled::NoCollision: CollisionEnabledStr = TEXT("NoCollision"); break;
	case ECollisionEnabled::QueryOnly: CollisionEnabledStr = TEXT("QueryOnly"); break;
	case ECollisionEnabled::PhysicsOnly: CollisionEnabledStr = TEXT("PhysicsOnly"); break;
	case ECollisionEnabled::QueryAndPhysics: CollisionEnabledStr = TEXT("QueryAndPhysics"); break;
	default: CollisionEnabledStr = TEXT("Unknown"); break;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("collisionEnabled"), CollisionEnabledStr);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSetCollisionEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	FString CollisionEnabledStr;
	if (!RequireString(Params, TEXT("collision"), CollisionEnabledStr, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	if (CollisionEnabledStr == TEXT("NoCollision")) CollisionEnabled = ECollisionEnabled::NoCollision;
	else if (CollisionEnabledStr == TEXT("QueryOnly")) CollisionEnabled = ECollisionEnabled::QueryOnly;
	else if (CollisionEnabledStr == TEXT("PhysicsOnly")) CollisionEnabled = ECollisionEnabled::PhysicsOnly;
	else if (CollisionEnabledStr == TEXT("QueryAndPhysics")) CollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	Component->SetCollisionEnabled(CollisionEnabled);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleGetCollisionProfile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("profileName"), Component->GetCollisionProfileName().ToString());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSetCollisionProfile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	FString ProfileName;
	if (!RequireString(Params, TEXT("profile"), ProfileName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	Component->SetCollisionProfileName(FName(*ProfileName));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleListCollisionProfiles(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> ProfilesArray;

	// Common collision profiles
	TArray<FString> CommonProfiles = {
		TEXT("NoCollision"),
		TEXT("BlockAll"),
		TEXT("OverlapAll"),
		TEXT("BlockAllDynamic"),
		TEXT("OverlapAllDynamic"),
		TEXT("IgnoreOnlyPawn"),
		TEXT("OverlapOnlyPawn"),
		TEXT("Pawn"),
		TEXT("Spectator"),
		TEXT("CharacterMesh"),
		TEXT("PhysicsActor"),
		TEXT("Destructible"),
		TEXT("InvisibleWall"),
		TEXT("InvisibleWallDynamic"),
		TEXT("Trigger"),
		TEXT("Ragdoll"),
		TEXT("Vehicle"),
		TEXT("UI")
	};

	for (const FString& Profile : CommonProfiles)
	{
		ProfilesArray.Add(MakeShared<FJsonValueString>(Profile));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("profiles"), ProfilesArray);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleLineTrace(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("start")) || !Params->HasField(TEXT("end")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: start, end"));
		return false;
	}
	FVector Start = JsonToVector(Params->GetObjectField(TEXT("start")));
	FVector End = JsonToVector(Params->GetObjectField(TEXT("end")));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = Params->HasField(TEXT("traceComplex")) ? Params->GetBoolField(TEXT("traceComplex")) : false;

	bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("hit"), bHit);
	if (bHit)
	{
		ResultObj->SetObjectField(TEXT("hitResult"), HitResultToJson(HitResult));
	}
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleSphereTrace(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("start")) || !Params->HasField(TEXT("end")) || !Params->HasField(TEXT("radius")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: start, end, radius"));
		return false;
	}
	FVector Start = JsonToVector(Params->GetObjectField(TEXT("start")));
	FVector End = JsonToVector(Params->GetObjectField(TEXT("end")));
	float Radius = Params->GetNumberField(TEXT("radius"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;

	bool bHit = World->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), QueryParams);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("hit"), bHit);
	if (bHit)
	{
		ResultObj->SetObjectField(TEXT("hitResult"), HitResultToJson(HitResult));
	}
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleBoxTrace(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("start")) || !Params->HasField(TEXT("end")) || !Params->HasField(TEXT("halfExtent")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: start, end, halfExtent"));
		return false;
	}
	FVector Start = JsonToVector(Params->GetObjectField(TEXT("start")));
	FVector End = JsonToVector(Params->GetObjectField(TEXT("end")));
	FVector HalfExtent = JsonToVector(Params->GetObjectField(TEXT("halfExtent")));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;

	bool bHit = World->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeBox(HalfExtent), QueryParams);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("hit"), bHit);
	if (bHit)
	{
		ResultObj->SetObjectField(TEXT("hitResult"), HitResultToJson(HitResult));
	}
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleCapsuleTrace(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("start")) || !Params->HasField(TEXT("end")) || !Params->HasField(TEXT("radius")) || !Params->HasField(TEXT("halfHeight")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: start, end, radius, halfHeight"));
		return false;
	}
	FVector Start = JsonToVector(Params->GetObjectField(TEXT("start")));
	FVector End = JsonToVector(Params->GetObjectField(TEXT("end")));
	float Radius = Params->GetNumberField(TEXT("radius"));
	float HalfHeight = Params->GetNumberField(TEXT("halfHeight"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;

	bool bHit = World->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("hit"), bHit);
	if (bHit)
	{
		ResultObj->SetObjectField(TEXT("hitResult"), HitResultToJson(HitResult));
	}
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleOverlapSphere(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("location")) || !Params->HasField(TEXT("radius")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: location, radius"));
		return false;
	}
	FVector Location = JsonToVector(Params->GetObjectField(TEXT("location")));
	float Radius = Params->GetNumberField(TEXT("radius"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;

	bool bHit = World->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), QueryParams);

	TArray<TSharedPtr<FJsonValue>> OverlapsArray;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		TSharedPtr<FJsonObject> OverlapObj = MakeShared<FJsonObject>();
		if (AActor* Actor = Overlap.GetActor())
		{
			OverlapObj->SetStringField(TEXT("actor"), Actor->GetName());
		}
		if (UPrimitiveComponent* Comp = Overlap.GetComponent())
		{
			OverlapObj->SetStringField(TEXT("component"), Comp->GetName());
		}
		OverlapsArray.Add(MakeShared<FJsonValueObject>(OverlapObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("hasOverlaps"), bHit);
	ResultObj->SetArrayField(TEXT("overlaps"), OverlapsArray);
	ResultObj->SetNumberField(TEXT("count"), OverlapsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleOverlapBox(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("location")) || !Params->HasField(TEXT("halfExtent")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: location, halfExtent"));
		return false;
	}
	FVector Location = JsonToVector(Params->GetObjectField(TEXT("location")));
	FVector HalfExtent = JsonToVector(Params->GetObjectField(TEXT("halfExtent")));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;

	bool bHit = World->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeBox(HalfExtent), QueryParams);

	TArray<TSharedPtr<FJsonValue>> OverlapsArray;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		TSharedPtr<FJsonObject> OverlapObj = MakeShared<FJsonObject>();
		if (AActor* Actor = Overlap.GetActor())
		{
			OverlapObj->SetStringField(TEXT("actor"), Actor->GetName());
		}
		if (UPrimitiveComponent* Comp = Overlap.GetComponent())
		{
			OverlapObj->SetStringField(TEXT("component"), Comp->GetName());
		}
		OverlapsArray.Add(MakeShared<FJsonValueObject>(OverlapObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("hasOverlaps"), bHit);
	ResultObj->SetArrayField(TEXT("overlaps"), OverlapsArray);
	ResultObj->SetNumberField(TEXT("count"), OverlapsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleWakeRigidBody(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	Component->WakeRigidBody();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandlePutRigidBodyToSleep(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	Component->PutRigidBodyToSleep();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleIsSleeping(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	UPrimitiveComponent* Component = GetPrimitiveComponent(ActorName, TEXT(""));
	if (!Component)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Primitive component not found on actor: %s"), *ActorName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("isSleeping"), Component->RigidBodyIsAwake() == false);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleListConstraints(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// List physics constraints in the world
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> ConstraintsArray;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		TArray<UPhysicsConstraintComponent*> Constraints;
		Actor->GetComponents<UPhysicsConstraintComponent>(Constraints);

		for (UPhysicsConstraintComponent* Constraint : Constraints)
		{
			TSharedPtr<FJsonObject> ConstraintObj = MakeShared<FJsonObject>();
			ConstraintObj->SetStringField(TEXT("name"), Constraint->GetName());
			ConstraintObj->SetStringField(TEXT("owner"), Actor->GetName());
			ConstraintsArray.Add(MakeShared<FJsonValueObject>(ConstraintObj));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("constraints"), ConstraintsArray);
	ResultObj->SetNumberField(TEXT("count"), ConstraintsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlPhysicsHandler::HandleGetConstraint(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Get constraint details not fully implemented."));
	return false;
}

bool FUltimateControlPhysicsHandler::HandleCreateConstraint(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Creating constraints via API not fully implemented."));
	return false;
}

bool FUltimateControlPhysicsHandler::HandleBreakConstraint(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Breaking constraints via API not fully implemented."));
	return false;
}
