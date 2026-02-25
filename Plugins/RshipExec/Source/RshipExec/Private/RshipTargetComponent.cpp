// Fill out your copyright notice in the Description page of Project Settings.

#include "RshipTargetComponent.h"
#include "RshipTargetGroup.h"
#include "RshipActionProvider.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RshipSubsystem.h"
#include "GameFramework/Actor.h"
#include "Util.h"
#include "Logs.h"
#include "Algo/Sort.h"
#include "Misc/Crc.h"

void URshipTargetComponent::OnRegister()
{
	Super::OnRegister();
	PrimaryComponentTick.bCanEverTick = false;
	SetComponentTickEnabled(false);
	Register();
}

void URshipTargetComponent::OnComponentDestroyed(bool bDestoryHierarchy)
{
	for (const auto& Handler : EmitterHandlers)
	{
		if (Handler.Value)
		{
			Handler.Value->Destroy();
		}
	}
	EmitterHandlers.Empty();

	if (GEngine)
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			Subsystem->UnregisterTargetComponent(this);

			if (URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager())
			{
				GroupManager->UnregisterTarget(this);
			}
		}
	}

	if (TargetData)
	{
		TargetData->SetBoundTargetComponent(nullptr);
		delete TargetData;
		TargetData = nullptr;
	}

	Super::OnComponentDestroyed(bDestoryHierarchy);
}

void URshipTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

uint32 URshipTargetComponent::ComputeSiblingComponentSignature() const
{
	TArray<UActorComponent*> Components;
	GatherSiblingComponents(Components);

	TArray<FString> Keys;
	Keys.Reserve(Components.Num());
	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}
		Keys.Add(Component->GetClass()->GetPathName() + TEXT("|") + Component->GetName());
	}
	Keys.Sort();

	uint32 Signature = 0;
	for (const FString& Key : Keys)
	{
		Signature = FCrc::StrCrc32(*Key, Signature);
	}
	return Signature;
}
void URshipTargetComponent::OnDataReceived()
{
	OnRshipData.Broadcast();
}

void URshipTargetComponent::HandleAfterTake(const FString& ActionName, UObject* ActionOwner)
{
	for (int32 Index = CachedActionProviderObjects.Num() - 1; Index >= 0; --Index)
	{
		UObject* ProviderObject = CachedActionProviderObjects[Index].Get();
		if (!ProviderObject)
		{
			CachedActionProviderObjects.RemoveAtSwap(Index);
			continue;
		}

		if (IRshipActionProvider* Provider = Cast<IRshipActionProvider>(ProviderObject))
		{
			Provider->OnRshipAfterTake(this, ActionName, ActionOwner);
		}
	}
}

void URshipTargetComponent::GatherSiblingComponents(TArray<UActorComponent*>& OutSiblingComponents) const
{
	OutSiblingComponents.Reset();
	if (AActor* Owner = GetOwner())
	{
		Owner->GetComponents(OutSiblingComponents);
	}
}

void URshipTargetComponent::RebuildActionProviderCache()
{
	CachedActionProviderObjects.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Owner->GetClass()->ImplementsInterface(URshipActionProvider::StaticClass()))
	{
		CachedActionProviderObjects.Add(Owner);
	}

	TArray<UActorComponent*> SiblingComponents;
	GatherSiblingComponents(SiblingComponents);
	for (UActorComponent* Sibling : SiblingComponents)
	{
		if (Sibling && Sibling->GetClass()->ImplementsInterface(URshipActionProvider::StaticClass()))
		{
			CachedActionProviderObjects.Add(Sibling);
		}
	}
}

void URshipTargetComponent::RegisterScannableMembers(UObject* OwnerObject, const FString& FullTargetId, const FString& MutableTargetId, bool bRequireRSPrefix)
{
	if (!OwnerObject)
	{
		return;
	}

	UClass* OwnerClass = OwnerObject->GetClass();
	for (TFieldIterator<UFunction> FuncIt(OwnerClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
	{
		TryRegisterFunctionAction(OwnerObject, *FuncIt, MutableTargetId, TEXT(""), bRequireRSPrefix);
	}
	for (TFieldIterator<FProperty> PropIt(OwnerClass, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		TryRegisterPropertyAction(OwnerObject, *PropIt, MutableTargetId, TEXT(""), bRequireRSPrefix);
	}
	for (TFieldIterator<FMulticastInlineDelegateProperty> EmitIt(OwnerClass, EFieldIteratorFlags::ExcludeSuper); EmitIt; ++EmitIt)
	{
		TryRegisterEmitter(OwnerObject, *EmitIt, FullTargetId, TEXT(""), bRequireRSPrefix);
	}
}

void URshipTargetComponent::RegisterProviderWhitelistActions(AActor* OwnerActor, const TArray<UActorComponent*>& SiblingComponents)
{
	if (!OwnerActor)
	{
		return;
	}

	RebuildActionProviderCache();

	if (IRshipActionProvider* OwnerProvider = Cast<IRshipActionProvider>(OwnerActor))
	{
		OwnerProvider->RegisterRshipWhitelistedActions(this);
	}
	for (UActorComponent* Sibling : SiblingComponents)
	{
		if (IRshipActionProvider* Provider = Cast<IRshipActionProvider>(Sibling))
		{
			Provider->RegisterRshipWhitelistedActions(this);
		}
	}
}

void URshipTargetComponent::Reconnect()
{
	if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
	{
		Subsystem->Reconnect();
	}
}

bool URshipTargetComponent::TryRegisterFunctionAction(UObject* owner, UFunction* func, const FString& fullTargetId, const FString& actionName, bool bRequireRSPrefix)
{
	if (!owner || !func || !TargetData)
	{
		return false;
	}

	const FString NameToCheck = func->GetName();
	if (bRequireRSPrefix && !NameToCheck.StartsWith(TEXT("RS_")))
	{
		return false;
	}

	if (NameToCheck.Contains(TEXT("__DelegateSignature")))
	{
		return false;
	}

	const FString FinalName = actionName.IsEmpty() ? NameToCheck : actionName;
	const FString FullActionId = fullTargetId + TEXT(":") + FinalName;

	if (TargetData->GetActions().Contains(FullActionId))
	{
		return false;
	}

	TargetData->AddAction(new Action(FullActionId, FinalName, func, owner));
	return true;
}

bool URshipTargetComponent::TryRegisterPropertyAction(UObject* owner, FProperty* prop, const FString& fullTargetId, const FString& actionName, bool bRequireRSPrefix)
{
	if (!owner || !prop || !TargetData)
	{
		return false;
	}

	const FString NameToCheck = prop->GetName();
	if (bRequireRSPrefix && !NameToCheck.StartsWith(TEXT("RS_")))
	{
		return false;
	}

	if (prop->IsA<FMulticastDelegateProperty>())
	{
		return false;
	}

	const FString FinalName = actionName.IsEmpty() ? NameToCheck : actionName;
	const FString FullActionId = fullTargetId + TEXT(":") + FinalName;

	if (TargetData->GetActions().Contains(FullActionId))
	{
		return false;
	}

	TargetData->AddAction(new Action(FullActionId, FinalName, prop, owner));
	UE_LOG(LogRshipExec, Verbose, TEXT("RshipTargetComponent: Added Action [%s]"), *FullActionId);
	return true;
}

bool URshipTargetComponent::TryRegisterEmitter(UObject* owner, FMulticastInlineDelegateProperty* emitterProp, const FString& fullTargetId, const FString& emitterName, bool bRequireRSPrefix)
{
	if (!owner || !emitterProp || !TargetData)
	{
		return false;
	}

	if (!GEngine)
	{
		return false;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return false;
	}

	const FString NameToCheck = emitterProp->GetName();
	if (bRequireRSPrefix && !NameToCheck.StartsWith(TEXT("RS_")))
	{
		return false;
	}

	const FString FinalName = emitterName.IsEmpty() ? NameToCheck : emitterName;
	const FString FullEmitterId = fullTargetId + TEXT(":") + FinalName;

	if (TargetData->GetEmitters().Contains(FullEmitterId) || EmitterHandlers.Contains(FinalName))
	{
		return false;
	}

	AActor* Parent = GetOwner();
	UWorld* World = GetWorld();
	if (!Parent || !World)
	{
		return false;
	}

	TargetData->AddEmitter(new EmitterContainer(FullEmitterId, FinalName, emitterProp));

	FMulticastScriptDelegate EmitterDelegate = emitterProp->GetPropertyValue_InContainer(owner);
	TScriptDelegate LocalDelegate;

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.Owner = Parent;
	SpawnInfo.bNoFail = true;
	SpawnInfo.bDeferConstruction = false;
	SpawnInfo.bAllowDuringConstructionScript = true;

	AEmitterHandler* Handler = World->SpawnActor<AEmitterHandler>(SpawnInfo);
#if WITH_EDITOR
	Handler->SetActorLabel(Parent->GetActorLabel() + TEXT(" ") + FinalName + TEXT(" Handler"));
#endif

	Handler->SetServiceId(Subsystem->GetServiceId());
	Handler->SetTargetId(fullTargetId);
	Handler->SetEmitterId(FinalName);
	Handler->SetDelegate(&LocalDelegate);

	LocalDelegate.BindUFunction(Handler, TEXT("ProcessEmitter"));
	EmitterDelegate.Add(LocalDelegate);
	emitterProp->SetPropertyValue_InContainer(owner, EmitterDelegate);

	EmitterHandlers.Add(FinalName, Handler);
	return true;
}

bool URshipTargetComponent::RegisterWhitelistedFunction(UObject* Owner, const FName& FunctionName, const FString& ExposedActionName)
{
	if (!TargetData || !Owner)
	{
		return false;
	}

	UFunction* Func = Owner->FindFunction(FunctionName);
	if (!Func)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RegisterWhitelistedFunction failed: function '%s' not found on %s"), *FunctionName.ToString(), *Owner->GetName());
		return false;
	}

	return TryRegisterFunctionAction(Owner, Func, TargetData->GetId(), ExposedActionName, false);
}

bool URshipTargetComponent::RegisterWhitelistedProperty(UObject* Owner, const FName& PropertyName, const FString& ExposedActionName)
{
	if (!TargetData || !Owner)
	{
		return false;
	}

	FProperty* Prop = Owner->GetClass()->FindPropertyByName(PropertyName);
	if (!Prop)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RegisterWhitelistedProperty failed: property '%s' not found on %s"), *PropertyName.ToString(), *Owner->GetName());
		return false;
	}

	return TryRegisterPropertyAction(Owner, Prop, TargetData->GetId(), ExposedActionName, false);
}

bool URshipTargetComponent::RegisterWhitelistedEmitter(UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName)
{
	if (!TargetData || !Owner)
	{
		return false;
	}

	FProperty* Prop = Owner->GetClass()->FindPropertyByName(DelegateName);
	FMulticastInlineDelegateProperty* EmitterProp = CastField<FMulticastInlineDelegateProperty>(Prop);
	if (!EmitterProp)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RegisterWhitelistedEmitter failed: delegate '%s' not found on %s"), *DelegateName.ToString(), *Owner->GetName());
		return false;
	}

	return TryRegisterEmitter(Owner, EmitterProp, TargetData->GetId(), ExposedEmitterName, false);
}

void URshipTargetComponent::Register()
{
	UWorld* World = GetWorld();
	if (World && World->WorldType == EWorldType::EditorPreview)
	{
		UE_LOG(LogRshipExec, Verbose, TEXT("Skipping registration for blueprint preview actor: %s"), *targetName);
		return;
	}

	if (TargetData != nullptr)
	{
		UE_LOG(LogRshipExec, Log, TEXT("Register called on already-registered target '%s', re-registering..."), *targetName);
		Unregister();
	}

	URshipSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
	AActor* Parent = GetOwner();
	if (!Subsystem || !Parent)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Register failed: missing subsystem or owner"));
		return;
	}

	FString OutlinerName = Parent->GetName();
#if WITH_EDITOR
	OutlinerName = Parent->GetActorLabel();
#endif

	if (targetName.IsEmpty())
	{
		targetName = OutlinerName;
		UE_LOG(LogRshipExec, Log, TEXT("Target Id not set, defaulting to actor name: %s"), *targetName);
	}

	const FString FullTargetId = Subsystem->GetServiceId() + TEXT(":") + targetName;
	FString MutableTargetId = FullTargetId;

	TargetData = new Target(FullTargetId);
	TargetData->SetBoundTargetComponent(this);
	Subsystem->RegisterTargetComponent(this);

	RegisterScannableMembers(Parent, FullTargetId, MutableTargetId, true);

	TArray<UActorComponent*> SiblingComponents;
	GatherSiblingComponents(SiblingComponents);
	for (UActorComponent* Sibling : SiblingComponents)
	{
		RegisterScannableMembers(Sibling, FullTargetId, MutableTargetId, true);
	}

	RegisterProviderWhitelistActions(Parent, SiblingComponents);

	Subsystem->SendTarget(TargetData);
	Subsystem->ProcessMessageQueue();

	if (URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager())
	{
		GroupManager->RegisterTarget(this);
	}

	CachedSiblingComponentSignature = ComputeSiblingComponentSignature();
	bHasCachedSiblingComponentSignature = true;

	UE_LOG(LogRshipExec, Log, TEXT("Component Registered: %s (actions=%d emitters=%d)"), *Parent->GetName(), TargetData->GetActions().Num(), TargetData->GetEmitters().Num());
}

bool URshipTargetComponent::HasTag(const FString& Tag) const
{
	const FString NormalizedTag = Tag.TrimStartAndEnd().ToLower();
	for (const FString& ExistingTag : Tags)
	{
		if (ExistingTag.TrimStartAndEnd().ToLower() == NormalizedTag)
		{
			return true;
		}
	}
	return false;
}

void URshipTargetComponent::Unregister()
{
	if (!GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (TargetData)
	{
		Subsystem->DeleteTarget(TargetData);
	}

	for (const auto& Handler : EmitterHandlers)
	{
		if (Handler.Value)
		{
			Handler.Value->Destroy();
		}
	}
	EmitterHandlers.Empty();

	Subsystem->UnregisterTargetComponent(this);

	if (TargetData)
	{
		TargetData->SetBoundTargetComponent(nullptr);
		delete TargetData;
		TargetData = nullptr;
	}

	if (URshipTargetGroupManager* GroupManager = Subsystem->GetGroupManager())
	{
		GroupManager->UnregisterTarget(this);
	}

	bHasCachedSiblingComponentSignature = false;
	CachedSiblingComponentSignature = 0;
	CachedActionProviderObjects.Reset();

	UE_LOG(LogRshipExec, Log, TEXT("Target unregistered: %s"), *targetName);
}

void URshipTargetComponent::SetTargetId(const FString& NewTargetId)
{
	if (NewTargetId.IsEmpty())
	{
		UE_LOG(LogRshipExec, Warning, TEXT("SetTargetId called with empty ID - ignoring"));
		return;
	}

	if (targetName == NewTargetId)
	{
		return;
	}

	const FString OldTargetId = targetName;
	if (TargetData != nullptr)
	{
		Unregister();
	}

	targetName = NewTargetId;
	Register();

	UE_LOG(LogRshipExec, Log, TEXT("Target ID changed: %s -> %s"), *OldTargetId, *NewTargetId);
}

void URshipTargetComponent::RescanSiblingComponents()
{
	if (!GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	AActor* Parent = GetOwner();
	if (!Subsystem || !Parent)
	{
		return;
	}

	if (!TargetData)
	{
		Register();
		return;
	}

	const FString FullTargetId = Subsystem->GetServiceId() + TEXT(":") + targetName;
	FString MutableTargetId = FullTargetId;

	TArray<UActorComponent*> SiblingComponents;
	GatherSiblingComponents(SiblingComponents);
	for (UActorComponent* Sibling : SiblingComponents)
	{
		RegisterScannableMembers(Sibling, FullTargetId, MutableTargetId, true);
	}

	RegisterProviderWhitelistActions(Parent, SiblingComponents);

	CachedSiblingComponentSignature = ComputeSiblingComponentSignature();
	bHasCachedSiblingComponentSignature = true;

	// Re-send target on every rescan. Registration is idempotent and this avoids stale routing/state across world transitions.
	Subsystem->SendTarget(TargetData);
	Subsystem->ProcessMessageQueue();
}




