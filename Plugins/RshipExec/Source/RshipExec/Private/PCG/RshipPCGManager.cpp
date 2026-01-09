// Rship PCG Manager Implementation

#include "PCG/RshipPCGManager.h"
#include "PCG/RshipPCGAutoBindComponent.h"
#include "RshipSubsystem.h"
#include "Target.h"
#include "Action.h"
#include "EmitterContainer.h"
#include "Logs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void URshipPCGManager::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	LastTickTime = FPlatformTime::Seconds();
	TotalRegistrations = 0;
	TotalUnregistrations = 0;
	TotalActionsRouted = 0;
	TotalPulsesEmitted = 0;

	UE_LOG(LogRshipExec, Log, TEXT("URshipPCGManager: Initialized"));
}

void URshipPCGManager::Shutdown()
{
	// Unregister all instances
	TArray<FGuid> GuidsToRemove;
	for (const auto& Pair : RegisteredInstances)
	{
		GuidsToRemove.Add(Pair.Key);
	}

	for (const FGuid& Guid : GuidsToRemove)
	{
		if (TWeakObjectPtr<URshipPCGAutoBindComponent>* CompPtr = RegisteredInstances.Find(Guid))
		{
			if (CompPtr->IsValid())
			{
				UnregisterInstance(CompPtr->Get());
			}
		}
	}

	RegisteredInstances.Empty();
	PathToGuidMap.Empty();
	ClassBindingsCache.Empty();
	PendingRegistrations.Empty();

	UE_LOG(LogRshipExec, Log, TEXT("URshipPCGManager: Shutdown (total: %d registrations, %d unregistrations)"),
		TotalRegistrations, TotalUnregistrations);
}

void URshipPCGManager::Tick(float DeltaTime)
{
	// Process pending registrations
	ProcessPendingRegistrations();

	// Clean up stale instances periodically
	double Now = FPlatformTime::Seconds();
	if (Now - LastTickTime > 5.0) // Every 5 seconds
	{
		CleanupStaleInstances();
		LastTickTime = Now;
	}
}

// ============================================================================
// INSTANCE REGISTRATION
// ============================================================================

void URshipPCGManager::RegisterInstance(URshipPCGAutoBindComponent* Component)
{
	if (!Component)
	{
		return;
	}

	const FRshipPCGInstanceId& Id = Component->GetInstanceId();
	if (!Id.IsValid())
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGManager: Cannot register component with invalid InstanceId"));
		return;
	}

	// Check for duplicate
	if (RegisteredInstances.Contains(Id.StableGuid))
	{
		TWeakObjectPtr<URshipPCGAutoBindComponent>& Existing = RegisteredInstances[Id.StableGuid];
		if (Existing.IsValid() && Existing.Get() != Component)
		{
			// Newer instance wins - unregister old one
			UE_LOG(LogRshipExec, Log, TEXT("URshipPCGManager: Replacing existing instance for %s"),
				*Id.TargetPath);
			UnregisterInstance(Existing.Get());
		}
		else if (Existing.Get() == Component)
		{
			// Already registered
			return;
		}
	}

	// Register
	RegisteredInstances.Add(Id.StableGuid, Component);
	PathToGuidMap.Add(Id.TargetPath, Id.StableGuid);

	// Send registration to rShip
	SendTargetRegistration(Component);

	TotalRegistrations++;

	// Fire event
	OnInstanceRegistered.Broadcast(Component);

	UE_LOG(LogRshipExec, Log, TEXT("URshipPCGManager: Registered instance %s (%s)"),
		*Id.DisplayName, *Id.TargetPath);
}

void URshipPCGManager::UnregisterInstance(URshipPCGAutoBindComponent* Component)
{
	if (!Component)
	{
		return;
	}

	const FRshipPCGInstanceId& Id = Component->GetInstanceId();
	if (!Id.IsValid())
	{
		return;
	}

	// Remove from maps
	if (!RegisteredInstances.Contains(Id.StableGuid))
	{
		return;
	}

	RegisteredInstances.Remove(Id.StableGuid);
	PathToGuidMap.Remove(Id.TargetPath);

	// Send deregistration to rShip
	SendTargetDeregistration(Component);

	TotalUnregistrations++;

	// Fire event
	OnInstanceUnregistered.Broadcast(Component);

	UE_LOG(LogRshipExec, Log, TEXT("URshipPCGManager: Unregistered instance %s"),
		*Id.TargetPath);
}

bool URshipPCGManager::IsInstanceRegistered(const FGuid& StableGuid) const
{
	return RegisteredInstances.Contains(StableGuid);
}

URshipPCGAutoBindComponent* URshipPCGManager::FindInstanceByGuid(const FGuid& StableGuid) const
{
	const TWeakObjectPtr<URshipPCGAutoBindComponent>* CompPtr = RegisteredInstances.Find(StableGuid);
	if (CompPtr && CompPtr->IsValid())
	{
		return CompPtr->Get();
	}
	return nullptr;
}

URshipPCGAutoBindComponent* URshipPCGManager::FindInstanceByPath(const FString& TargetPath) const
{
	const FGuid* GuidPtr = PathToGuidMap.Find(TargetPath);
	if (GuidPtr)
	{
		return FindInstanceByGuid(*GuidPtr);
	}
	return nullptr;
}

TArray<URshipPCGAutoBindComponent*> URshipPCGManager::GetAllInstances() const
{
	TArray<URshipPCGAutoBindComponent*> Result;
	for (const auto& Pair : RegisteredInstances)
	{
		if (Pair.Value.IsValid())
		{
			Result.Add(Pair.Value.Get());
		}
	}
	return Result;
}

// ============================================================================
// CLASS BINDINGS
// ============================================================================

FRshipPCGClassBindings* URshipPCGManager::GetOrCreateClassBindings(UClass* Class)
{
	if (!Class)
	{
		return nullptr;
	}

	TWeakObjectPtr<UClass> WeakClass(Class);

	// Check cache
	FRshipPCGClassBindings* Existing = ClassBindingsCache.Find(WeakClass);
	if (Existing && Existing->bIsValid)
	{
		// Rebuild property pointers if needed (after garbage collection)
		if (!Existing->BoundClass)
		{
			Existing->BoundClass = Class;
			Existing->RebuildPropertyPointers();
		}
		return Existing;
	}

	// Create new bindings
	FRshipPCGClassBindings Bindings;
	Bindings.BuildFromClass(Class);

	ClassBindingsCache.Add(WeakClass, MoveTemp(Bindings));
	return ClassBindingsCache.Find(WeakClass);
}

const FRshipPCGClassBindings* URshipPCGManager::GetClassBindings(UClass* Class) const
{
	if (!Class)
	{
		return nullptr;
	}

	TWeakObjectPtr<UClass> WeakClass(Class);
	return ClassBindingsCache.Find(WeakClass);
}

void URshipPCGManager::InvalidateClassBindings(UClass* Class)
{
	if (!Class)
	{
		return;
	}

	TWeakObjectPtr<UClass> WeakClass(Class);
	ClassBindingsCache.Remove(WeakClass);

	UE_LOG(LogRshipExec, Log, TEXT("URshipPCGManager: Invalidated class bindings for %s"),
		*Class->GetName());
}

void URshipPCGManager::ClearAllClassBindings()
{
	ClassBindingsCache.Empty();
	UE_LOG(LogRshipExec, Log, TEXT("URshipPCGManager: Cleared all class bindings"));
}

// ============================================================================
// ACTION ROUTING
// ============================================================================

bool URshipPCGManager::RouteAction(const FString& TargetPath, const FString& ActionId, const TSharedRef<FJsonObject>& Data)
{
	URshipPCGAutoBindComponent* Component = FindInstanceByPath(TargetPath);
	if (!Component)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGManager: Cannot route action - target not found: %s"),
			*TargetPath);
		return false;
	}

	Component->HandleAction(ActionId, Data);
	TotalActionsRouted++;

	OnActionExecuted.Broadcast(Component, ActionId, true);
	return true;
}

bool URshipPCGManager::ExecuteAction(URshipPCGAutoBindComponent* Component, FName PropertyName, const FString& JsonValue)
{
	if (!Component)
	{
		return false;
	}

	return Component->SetPropertyValueFromJson(PropertyName, JsonValue);
}

// ============================================================================
// PULSE EMISSION
// ============================================================================

void URshipPCGManager::EmitPulse(URshipPCGAutoBindComponent* Component, const FString& EmitterId, TSharedPtr<FJsonObject> Data)
{
	if (!Subsystem || !Component || !Data.IsValid())
	{
		return;
	}

	const FRshipPCGInstanceId& Id = Component->GetInstanceId();
	if (!Id.IsValid())
	{
		return;
	}

	// Use the subsystem's PulseEmitter method
	Subsystem->PulseEmitter(Id.TargetPath, EmitterId, Data);
	TotalPulsesEmitted++;
}

void URshipPCGManager::EmitAllPulses()
{
	for (const auto& Pair : RegisteredInstances)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->EmitAllPulses();
		}
	}
}

// ============================================================================
// BULK OPERATIONS
// ============================================================================

int32 URshipPCGManager::SetPropertyOnAllInstances(UClass* Class, FName PropertyName, const FString& JsonValue)
{
	int32 Count = 0;
	for (const auto& Pair : RegisteredInstances)
	{
		if (Pair.Value.IsValid())
		{
			AActor* Owner = Pair.Value->GetOwner();
			if (Owner && Owner->IsA(Class))
			{
				if (Pair.Value->SetPropertyValueFromJson(PropertyName, JsonValue))
				{
					Count++;
				}
			}
		}
	}
	return Count;
}

int32 URshipPCGManager::SetPropertyOnTaggedInstances(const FString& Tag, FName PropertyName, const FString& JsonValue)
{
	int32 Count = 0;
	for (const auto& Pair : RegisteredInstances)
	{
		if (Pair.Value.IsValid())
		{
			if (Pair.Value->Tags.Contains(Tag))
			{
				if (Pair.Value->SetPropertyValueFromJson(PropertyName, JsonValue))
				{
					Count++;
				}
			}
		}
	}
	return Count;
}

TArray<URshipPCGAutoBindComponent*> URshipPCGManager::GetInstancesOfClass(UClass* Class) const
{
	TArray<URshipPCGAutoBindComponent*> Result;
	for (const auto& Pair : RegisteredInstances)
	{
		if (Pair.Value.IsValid())
		{
			AActor* Owner = Pair.Value->GetOwner();
			if (Owner && Owner->IsA(Class))
			{
				Result.Add(Pair.Value.Get());
			}
		}
	}
	return Result;
}

TArray<URshipPCGAutoBindComponent*> URshipPCGManager::GetInstancesWithTag(const FString& Tag) const
{
	TArray<URshipPCGAutoBindComponent*> Result;
	for (const auto& Pair : RegisteredInstances)
	{
		if (Pair.Value.IsValid())
		{
			if (Pair.Value->Tags.Contains(Tag))
			{
				Result.Add(Pair.Value.Get());
			}
		}
	}
	return Result;
}

// ============================================================================
// DEBUG / VALIDATION
// ============================================================================

void URshipPCGManager::DumpAllTargets()
{
	UE_LOG(LogRshipExec, Log, TEXT("=== PCG Targets (%d registered) ==="), RegisteredInstances.Num());

	for (const auto& Pair : RegisteredInstances)
	{
		if (Pair.Value.IsValid())
		{
			const FRshipPCGInstanceId& Id = Pair.Value->GetInstanceId();
			UE_LOG(LogRshipExec, Log, TEXT("  [%s] %s -> %s"),
				*Id.StableGuid.ToString(EGuidFormats::DigitsWithHyphens),
				*Id.DisplayName,
				*Id.TargetPath);

			if (const FRshipPCGClassBindings* Bindings = Pair.Value->GetClassBindings())
			{
				for (const FRshipPCGPropertyDescriptor& Desc : Bindings->Properties)
				{
					FString AccessStr;
					switch (Desc.Access)
					{
					case ERshipPCGPropertyAccess::ReadOnly: AccessStr = TEXT("R"); break;
					case ERshipPCGPropertyAccess::WriteOnly: AccessStr = TEXT("W"); break;
					case ERshipPCGPropertyAccess::ReadWrite: AccessStr = TEXT("RW"); break;
					}
					UE_LOG(LogRshipExec, Log, TEXT("    - %s [%s] (%s)"),
						*Desc.DisplayName, *AccessStr, *Desc.UnrealTypeName);
				}
			}
		}
	}
}

void URshipPCGManager::DumpTarget(const FString& TargetPath)
{
	URshipPCGAutoBindComponent* Component = FindInstanceByPath(TargetPath);
	if (!Component)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Target not found: %s"), *TargetPath);
		return;
	}

	const FRshipPCGInstanceId& Id = Component->GetInstanceId();
	UE_LOG(LogRshipExec, Log, TEXT("=== Target: %s ==="), *TargetPath);
	UE_LOG(LogRshipExec, Log, TEXT("  DisplayName: %s"), *Id.DisplayName);
	UE_LOG(LogRshipExec, Log, TEXT("  StableGuid: %s"), *Id.StableGuid.ToString(EGuidFormats::DigitsWithHyphens));
	UE_LOG(LogRshipExec, Log, TEXT("  PCGComponentGuid: %s"), *Id.PCGComponentGuid.ToString(EGuidFormats::DigitsWithHyphens));
	UE_LOG(LogRshipExec, Log, TEXT("  SourceKey: %s"), *Id.SourceKey);
	UE_LOG(LogRshipExec, Log, TEXT("  PointIndex: %d"), Id.PointIndex);
	UE_LOG(LogRshipExec, Log, TEXT("  QuantizedDistance: %lld"), Id.QuantizedDistance);

	AActor* Owner = Component->GetOwner();
	if (Owner)
	{
		UE_LOG(LogRshipExec, Log, TEXT("  Actor: %s (%s)"), *Owner->GetName(), *Owner->GetClass()->GetName());
		UE_LOG(LogRshipExec, Log, TEXT("  Location: %s"), *Owner->GetActorLocation().ToString());
	}

	if (const FRshipPCGClassBindings* Bindings = Component->GetClassBindings())
	{
		UE_LOG(LogRshipExec, Log, TEXT("  Properties: %d"), Bindings->Properties.Num());
		for (const FRshipPCGPropertyDescriptor& Desc : Bindings->Properties)
		{
			FString Value = Component->GetPropertyValueAsString(Desc.PropertyName);
			UE_LOG(LogRshipExec, Log, TEXT("    - %s = %s"), *Desc.DisplayName, *Value);
		}
	}
}

bool URshipPCGManager::ValidateAllBindings()
{
	bool bAllValid = true;
	int32 InvalidCount = 0;

	for (const auto& Pair : RegisteredInstances)
	{
		if (!Pair.Value.IsValid())
		{
			UE_LOG(LogRshipExec, Warning, TEXT("Stale instance reference: %s"),
				*Pair.Key.ToString(EGuidFormats::DigitsWithHyphens));
			InvalidCount++;
			bAllValid = false;
			continue;
		}

		URshipPCGAutoBindComponent* Component = Pair.Value.Get();
		if (!Component->GetInstanceId().IsValid())
		{
			UE_LOG(LogRshipExec, Warning, TEXT("Invalid InstanceId on component: %s"),
				*Component->GetOwner()->GetName());
			InvalidCount++;
			bAllValid = false;
		}

		const FRshipPCGClassBindings* Bindings = Component->GetClassBindings();
		if (!Bindings || !Bindings->bIsValid)
		{
			UE_LOG(LogRshipExec, Warning, TEXT("Invalid class bindings on: %s"),
				*Component->GetInstanceId().TargetPath);
			InvalidCount++;
			bAllValid = false;
		}
	}

	if (bAllValid)
	{
		UE_LOG(LogRshipExec, Log, TEXT("All %d PCG bindings validated successfully"),
			RegisteredInstances.Num());
	}
	else
	{
		UE_LOG(LogRshipExec, Warning, TEXT("PCG binding validation found %d issues in %d instances"),
			InvalidCount, RegisteredInstances.Num());
	}

	return bAllValid;
}

FString URshipPCGManager::GetStatistics() const
{
	return FString::Printf(
		TEXT("PCG Manager: %d instances, %d class bindings cached, %d registrations, %d unregistrations, %d actions routed, %d pulses emitted"),
		RegisteredInstances.Num(),
		ClassBindingsCache.Num(),
		TotalRegistrations,
		TotalUnregistrations,
		TotalActionsRouted,
		TotalPulsesEmitted);
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void URshipPCGManager::SendTargetRegistration(URshipPCGAutoBindComponent* Component)
{
	if (!Subsystem || !Component)
	{
		return;
	}

	const FRshipPCGInstanceId& Id = Component->GetInstanceId();
	const FRshipPCGClassBindings* Bindings = Component->GetClassBindings();

	// Build target
	Target* TargetObj = BuildTarget(Component);
	if (!TargetObj)
	{
		return;
	}

	// Build and add actions for writable properties
	if (Bindings)
	{
		TArray<Action*> Actions = BuildActions(Component, *Bindings);
		for (Action* ActionObj : Actions)
		{
			TargetObj->AddAction(ActionObj);
		}

		// Note: For PCG bindings, we use direct pulse emission rather than EmitterContainer
		// because EmitterContainer requires FMulticastInlineDelegateProperty
		// Our pulse emission goes through EmitPulse() -> Subsystem->PulseEmitter()
	}

	// Send to subsystem (this is a friend class operation)
	// We need to use the subsystem's internal methods
	// For now, we'll manually build and send the JSON

	FString ServiceId = Subsystem->GetServiceId();

	// Build Target JSON
	TSharedPtr<FJsonObject> TargetJson = MakeShared<FJsonObject>();
	TargetJson->SetStringField(TEXT("id"), Id.TargetPath);
	TargetJson->SetStringField(TEXT("name"), Id.DisplayName);
	TargetJson->SetStringField(TEXT("serviceId"), ServiceId);
	TargetJson->SetStringField(TEXT("category"), Component->TargetCategory);

	// Add tags
	TArray<TSharedPtr<FJsonValue>> TagsJson;
	for (const FString& Tag : Component->Tags)
	{
		TagsJson.Add(MakeShared<FJsonValueString>(Tag));
	}
	TagsJson.Add(MakeShared<FJsonValueString>(TEXT("pcg")));
	TargetJson->SetArrayField(TEXT("tags"), TagsJson);

	// Add action IDs
	TArray<TSharedPtr<FJsonValue>> ActionIdsJson;
	for (const auto& ActionPair : TargetObj->GetActions())
	{
		ActionIdsJson.Add(MakeShared<FJsonValueString>(ActionPair.Key));
	}
	TargetJson->SetArrayField(TEXT("actionIds"), ActionIdsJson);

	// Add emitter IDs (property-based emitters)
	TArray<TSharedPtr<FJsonValue>> EmitterIdsJson;
	if (Bindings)
	{
		for (const FRshipPCGPropertyDescriptor& Desc : Bindings->Properties)
		{
			if (Desc.Access == ERshipPCGPropertyAccess::ReadOnly ||
				Desc.Access == ERshipPCGPropertyAccess::ReadWrite)
			{
				FString EmitterId = Id.TargetPath + TEXT(":") + Desc.PropertyName.ToString();
				EmitterIdsJson.Add(MakeShared<FJsonValueString>(EmitterId));
			}
		}
	}
	TargetJson->SetArrayField(TEXT("emitterIds"), EmitterIdsJson);

	TargetJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

	// Build and send the event
	TSharedPtr<FJsonObject> EventData = MakeShared<FJsonObject>();
	EventData->SetStringField(TEXT("itemType"), TEXT("Target"));
	EventData->SetStringField(TEXT("changeType"), TEXT("SET"));
	EventData->SetObjectField(TEXT("item"), TargetJson);
	EventData->SetStringField(TEXT("tx"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	EventData->SetStringField(TEXT("createdAt"), FDateTime::UtcNow().ToIso8601());

	TSharedPtr<FJsonObject> WsEvent = MakeShared<FJsonObject>();
	WsEvent->SetStringField(TEXT("event"), TEXT("ws:m:event"));
	WsEvent->SetObjectField(TEXT("data"), EventData);

	Subsystem->SendJson(WsEvent);

	// Send actions
	for (const auto& ActionPair : TargetObj->GetActions())
	{
		Action* ActionObj = ActionPair.Value;

		TSharedPtr<FJsonObject> ActionJson = MakeShared<FJsonObject>();
		ActionJson->SetStringField(TEXT("id"), ActionObj->GetId());
		ActionJson->SetStringField(TEXT("name"), ActionObj->GetName());
		ActionJson->SetStringField(TEXT("targetId"), Id.TargetPath);
		ActionJson->SetStringField(TEXT("serviceId"), ServiceId);
		if (TSharedPtr<FJsonObject> Schema = ActionObj->GetSchema())
		{
			ActionJson->SetObjectField(TEXT("schema"), Schema);
		}
		ActionJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

		TSharedPtr<FJsonObject> ActionEventData = MakeShared<FJsonObject>();
		ActionEventData->SetStringField(TEXT("itemType"), TEXT("Action"));
		ActionEventData->SetStringField(TEXT("changeType"), TEXT("SET"));
		ActionEventData->SetObjectField(TEXT("item"), ActionJson);
		ActionEventData->SetStringField(TEXT("tx"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
		ActionEventData->SetStringField(TEXT("createdAt"), FDateTime::UtcNow().ToIso8601());

		TSharedPtr<FJsonObject> ActionWsEvent = MakeShared<FJsonObject>();
		ActionWsEvent->SetStringField(TEXT("event"), TEXT("ws:m:event"));
		ActionWsEvent->SetObjectField(TEXT("data"), ActionEventData);

		Subsystem->SendJson(ActionWsEvent);
	}

	// Send emitters for readable properties
	if (Bindings)
	{
		for (const FRshipPCGPropertyDescriptor& Desc : Bindings->Properties)
		{
			if (Desc.Access == ERshipPCGPropertyAccess::ReadOnly ||
				Desc.Access == ERshipPCGPropertyAccess::ReadWrite)
			{
				FString EmitterId = Id.TargetPath + TEXT(":") + Desc.PropertyName.ToString();

				TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
				EmitterJson->SetStringField(TEXT("id"), EmitterId);
				EmitterJson->SetStringField(TEXT("name"), Desc.DisplayName);
				EmitterJson->SetStringField(TEXT("targetId"), Id.TargetPath);
				EmitterJson->SetStringField(TEXT("serviceId"), ServiceId);

				// Build schema
				TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
				Schema->SetStringField(TEXT("type"), TEXT("object"));
				TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
				TSharedPtr<FJsonObject> PropSchema = MakeShared<FJsonObject>();
				PropSchema->SetStringField(TEXT("type"), Desc.GetJsonSchemaType());
				if (!Desc.Description.IsEmpty())
				{
					PropSchema->SetStringField(TEXT("description"), Desc.Description);
				}
				Properties->SetObjectField(Desc.DisplayName, PropSchema);
				Schema->SetObjectField(TEXT("properties"), Properties);
				EmitterJson->SetObjectField(TEXT("schema"), Schema);

				EmitterJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

				TSharedPtr<FJsonObject> EmitterEventData = MakeShared<FJsonObject>();
				EmitterEventData->SetStringField(TEXT("itemType"), TEXT("Emitter"));
				EmitterEventData->SetStringField(TEXT("changeType"), TEXT("SET"));
				EmitterEventData->SetObjectField(TEXT("item"), EmitterJson);
				EmitterEventData->SetStringField(TEXT("tx"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
				EmitterEventData->SetStringField(TEXT("createdAt"), FDateTime::UtcNow().ToIso8601());

				TSharedPtr<FJsonObject> EmitterWsEvent = MakeShared<FJsonObject>();
				EmitterWsEvent->SetStringField(TEXT("event"), TEXT("ws:m:event"));
				EmitterWsEvent->SetObjectField(TEXT("data"), EmitterEventData);

				Subsystem->SendJson(EmitterWsEvent);
			}
		}
	}

	// Clean up
	delete TargetObj;
}

void URshipPCGManager::SendTargetDeregistration(URshipPCGAutoBindComponent* Component)
{
	if (!Subsystem || !Component)
	{
		return;
	}

	const FRshipPCGInstanceId& Id = Component->GetInstanceId();

	// Send TargetStatus offline - server manages target lifecycle (no DEL commands)
	TSharedPtr<FJsonObject> TargetStatus = MakeShared<FJsonObject>();
	TargetStatus->SetStringField(TEXT("targetId"), Id.TargetPath);
	TargetStatus->SetStringField(TEXT("instanceId"), Subsystem->GetInstanceId());
	TargetStatus->SetStringField(TEXT("status"), TEXT("offline"));
	TargetStatus->SetStringField(TEXT("id"), Id.TargetPath);
	TargetStatus->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

	TSharedPtr<FJsonObject> EventData = MakeShared<FJsonObject>();
	EventData->SetStringField(TEXT("itemType"), TEXT("TargetStatus"));
	EventData->SetStringField(TEXT("changeType"), TEXT("SET"));
	EventData->SetObjectField(TEXT("item"), TargetStatus);
	EventData->SetStringField(TEXT("tx"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	EventData->SetStringField(TEXT("createdAt"), FDateTime::UtcNow().ToIso8601());

	TSharedPtr<FJsonObject> WsEvent = MakeShared<FJsonObject>();
	WsEvent->SetStringField(TEXT("event"), TEXT("ws:m:event"));
	WsEvent->SetObjectField(TEXT("data"), EventData);

	Subsystem->SendJson(WsEvent);
}

Target* URshipPCGManager::BuildTarget(URshipPCGAutoBindComponent* Component)
{
	if (!Component)
	{
		return nullptr;
	}

	const FRshipPCGInstanceId& Id = Component->GetInstanceId();
	return new Target(Id.TargetPath);
}

TArray<Action*> URshipPCGManager::BuildActions(URshipPCGAutoBindComponent* Component, const FRshipPCGClassBindings& Bindings)
{
	TArray<Action*> Actions;

	if (!Component)
	{
		return Actions;
	}

	const FRshipPCGInstanceId& Id = Component->GetInstanceId();
	AActor* Owner = Component->GetOwner();

	for (const FRshipPCGPropertyDescriptor& Desc : Bindings.Properties)
	{
		// Only create actions for writable properties
		if (Desc.Access == ERshipPCGPropertyAccess::ReadOnly)
		{
			continue;
		}

		if (!Desc.CachedProperty)
		{
			continue;
		}

		FString ActionId = Id.TargetPath + TEXT(":") + Desc.PropertyName.ToString();
		Action* ActionObj = new Action(ActionId, Desc.DisplayName, Desc.CachedProperty, Owner);
		Actions.Add(ActionObj);
	}

	return Actions;
}

TArray<EmitterContainer*> URshipPCGManager::BuildEmitters(URshipPCGAutoBindComponent* Component, const FRshipPCGClassBindings& Bindings)
{
	// Note: EmitterContainer requires FMulticastInlineDelegateProperty
	// For PCG property-based emitters, we use direct pulse emission instead
	// This method is kept for potential future delegate-based emitter support
	TArray<EmitterContainer*> Emitters;
	return Emitters;
}

void URshipPCGManager::ProcessPendingRegistrations()
{
	if (PendingRegistrations.Num() == 0)
	{
		return;
	}

	// Process pending registrations in batches
	const int32 BatchSize = 10;
	int32 Processed = 0;

	for (int32 i = PendingRegistrations.Num() - 1; i >= 0 && Processed < BatchSize; --i)
	{
		TWeakObjectPtr<URshipPCGAutoBindComponent>& WeakComp = PendingRegistrations[i];
		if (WeakComp.IsValid())
		{
			RegisterInstance(WeakComp.Get());
		}
		PendingRegistrations.RemoveAt(i);
		Processed++;
	}
}

void URshipPCGManager::CleanupStaleInstances()
{
	TArray<FGuid> StaleGuids;

	for (const auto& Pair : RegisteredInstances)
	{
		if (!Pair.Value.IsValid())
		{
			StaleGuids.Add(Pair.Key);
		}
	}

	for (const FGuid& Guid : StaleGuids)
	{
		RegisteredInstances.Remove(Guid);

		// Find and remove from path map
		for (auto It = PathToGuidMap.CreateIterator(); It; ++It)
		{
			if (It.Value() == Guid)
			{
				It.RemoveCurrent();
				break;
			}
		}
	}

	if (StaleGuids.Num() > 0)
	{
		UE_LOG(LogRshipExec, Log, TEXT("URshipPCGManager: Cleaned up %d stale instances"),
			StaleGuids.Num());
	}
}
