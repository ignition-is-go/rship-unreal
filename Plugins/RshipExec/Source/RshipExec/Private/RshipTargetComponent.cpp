// Fill out your copyright notice in the Description page of Project Settings.

#include "RshipTargetComponent.h"
#include "RshipTargetGroup.h"
#include <iostream>
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RshipSubsystem.h"
#include "GameFramework/Actor.h"
#include "Misc/DefaultValueHelper.h"
#include "Util.h"
#include "Logs.h"

using namespace std;

// Sets default values for this component's properties
void URshipTargetComponent::OnRegister()
{

    Super::OnRegister();
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;

    Register();
}

void URshipTargetComponent::OnComponentDestroyed(bool bDestoryHierarchy)
{
    for (const auto &handler : EmitterHandlers)
    {
        if (handler.Value)
        {
            handler.Value->Destroy();
        }
    }
    EmitterHandlers.Empty();

    // GEngine can be null during editor shutdown - check before accessing
    if (!GEngine)
    {
        return;
    }

    URshipSubsystem *subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

    if(!subsystem)
    {
        return;
    }

    if (!subsystem->TargetComponents)
    {
        return;
    }

    subsystem->TargetComponents->Remove(this);

    // Unregister from GroupManager
    if (URshipTargetGroupManager* GroupManager = subsystem->GetGroupManager())
    {
        GroupManager->UnregisterTarget(this);
    }
}

// Called every frame
void URshipTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URshipTargetComponent::OnDataReceived() {
    this->OnRshipData.Broadcast();
}

void URshipTargetComponent::Reconnect()
{
    URshipSubsystem *subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    subsystem->Reconnect();
}

void URshipTargetComponent::RegisterFunction(UObject* owner, UFunction* func, FString *targetId) {
    FString name = func->GetName();

    if (!name.StartsWith("RS_")) {
        return;
    }

    // Skip auto-generated delegate signature functions (e.g., RS_FocalLengthEmitter__DelegateSignature)
    if (name.Contains("__DelegateSignature")) {
        return;
    }

    FString fullActionId = *targetId + ":" + name;

    auto action = new Action(fullActionId, name, func, owner);
    this->TargetData->AddAction(action);
}

void URshipTargetComponent::RegisterProperty(UObject* owner, FProperty* prop, FString* targetId) {
    FString name = prop->GetName();
    UE_LOG(LogRshipExec, Verbose, TEXT("RshipTargetComponent: Processing Property [%s]"), *name);

    if (!name.StartsWith("RS_")) {
        return;
    }

    // Skip delegate properties - they are registered as emitters, not actions
    if (prop->IsA<FMulticastDelegateProperty>()) {
        return;
    }

    FString fullActionId = *targetId + ":" + name;
    auto action = new Action(fullActionId, name, prop, owner);
    this->TargetData->AddAction(action);
    UE_LOG(LogRshipExec, Verbose, TEXT("RshipTargetComponent: Added Action [%s]"), *fullActionId);
}

void URshipTargetComponent::Register()
{

    URshipSubsystem *subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

    AActor *parent = GetOwner();

    if (!parent)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Parent not found"));
    }

    subsystem->TargetComponents->Add(this);

    FString outlinerName = parent->GetName();

    // Default target name to actor name if not set
    if (this->targetName.IsEmpty())
    {
        this->targetName = outlinerName;
        UE_LOG(LogRshipExec, Log, TEXT("Target Id not set, defaulting to actor name: %s"), *this->targetName);
    }

    UE_LOG(LogRshipExec, Log, TEXT("Registering OUTLINER: %s as %s"), *outlinerName, *this->targetName);

    FString fullTargetId = subsystem->GetServiceId() + ":" + this->targetName;

    this->TargetData = new Target(fullTargetId);

    UClass *ownerClass = parent->GetClass();

    for (TFieldIterator<UFunction> field(ownerClass, EFieldIteratorFlags::ExcludeSuper); field; ++field)
    {
        this->RegisterFunction(parent, *field, &fullTargetId);
    }

    for(TFieldIterator<FProperty> propIt(ownerClass, EFieldIteratorFlags::ExcludeSuper); propIt; ++propIt) {
        FProperty* property = *propIt;
        this->RegisterProperty(parent, property, &fullTargetId);
	}

    TArray<UActorComponent*> siblingComponents;

    parent->GetComponents(siblingComponents);

    UE_LOG(LogRshipExec, Log, TEXT("Scanning %d sibling components for RS_ members"), siblingComponents.Num());

    for (UActorComponent* sibling : siblingComponents) {
        UClass* siblingClass = sibling->GetClass();
        UE_LOG(LogRshipExec, Log, TEXT("  Sibling: %s (Class: %s)"), *sibling->GetName(), *siblingClass->GetName());

        for (TFieldIterator<UFunction> siblingFunc(siblingClass, EFieldIteratorFlags::ExcludeSuper); siblingFunc; ++siblingFunc) {
            FString funcName = (*siblingFunc)->GetName();
            if (funcName.StartsWith("RS_")) {
                UE_LOG(LogRshipExec, Log, TEXT("    Found RS_ function: %s"), *funcName);
            }
            this->RegisterFunction(sibling, *siblingFunc, &fullTargetId);
        }

        for (TFieldIterator<FProperty> siblingProp(siblingClass, EFieldIteratorFlags::ExcludeSuper); siblingProp; ++siblingProp) {
            FString propName = (*siblingProp)->GetName();
            if (propName.StartsWith("RS_")) {
                UE_LOG(LogRshipExec, Log, TEXT("    Found RS_ property: %s"), *propName);
            }
            this->RegisterProperty(sibling, *siblingProp, &fullTargetId);
        }
    }

    // Helper lambda to register emitters from a class/object pair
    auto RegisterEmittersFromObject = [&](UClass* targetClass, UObject* targetObject) {
        for (TFieldIterator<FMulticastInlineDelegateProperty> It(targetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            FMulticastInlineDelegateProperty *EmitterProp = *It;
            FString EmitterName = EmitterProp->GetName();
            FName EmitterType = EmitterProp->GetClass()->GetFName();

            UE_LOG(LogRshipExec, Log, TEXT("Emitter: %s, Type: %s"), *EmitterName, *EmitterType.ToString());

            if (!EmitterName.StartsWith("RS_"))
            {
                continue;
            }

            auto emitters = this->TargetData->GetEmitters();

            FString fullEmitterId = fullTargetId + ":" + EmitterName;

            auto emitter = new EmitterContainer(fullEmitterId, EmitterName, EmitterProp);
            this->TargetData->AddEmitter(emitter);

            FMulticastScriptDelegate EmitterDelegate = EmitterProp->GetPropertyValue_InContainer(targetObject);

            TScriptDelegate localDelegate;

            auto world = GetWorld();

            if (!world)
            {
                UE_LOG(LogRshipExec, Warning, TEXT("World Not Found"));
                return;
            }

            FActorSpawnParameters spawnInfo;
            spawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            spawnInfo.Owner = parent;
            spawnInfo.bNoFail = true;
            spawnInfo.bDeferConstruction = false;
            spawnInfo.bAllowDuringConstructionScript = true;

            if (this->EmitterHandlers.Contains(EmitterName))
            {
                return;
            }

            AEmitterHandler *handler = world->SpawnActor<AEmitterHandler>(spawnInfo);
#if WITH_EDITOR
            handler->SetActorLabel(parent->GetActorLabel() + " " + EmitterName + " Handler");
#endif

            handler->SetServiceId(subsystem->GetServiceId());
            handler->SetTargetId(fullTargetId);
            handler->SetEmitterId(EmitterName);
            handler->SetDelegate(&localDelegate);

            localDelegate.BindUFunction(handler, TEXT("ProcessEmitter"));

            EmitterDelegate.Add(localDelegate);

            EmitterProp->SetPropertyValue_InContainer(targetObject, EmitterDelegate);

            this->EmitterHandlers.Add(EmitterName, handler);
        }
    };

    // Register emitters from owner actor
    RegisterEmittersFromObject(ownerClass, parent);

    // Register emitters from sibling components
    for (UActorComponent* sibling : siblingComponents) {
        UClass* siblingClass = sibling->GetClass();
        UE_LOG(LogRshipExec, Log, TEXT("  Scanning sibling %s for emitters"), *siblingClass->GetName());
        RegisterEmittersFromObject(siblingClass, sibling);
    }

    UE_LOG(LogRshipExec, Log, TEXT("Target %s: %d emitters, %d actions registered"),
        *fullTargetId,
        this->TargetData->GetEmitters().Num(),
        this->TargetData->GetActions().Num());

    subsystem->SendAll();

    // Register with GroupManager for tagging/grouping support
    if (URshipTargetGroupManager* GroupManager = subsystem->GetGroupManager())
    {
        GroupManager->RegisterTarget(this);
    }

    UE_LOG(LogRshipExec, Log, TEXT("Component Registered: %s"), *parent->GetName());
}

bool URshipTargetComponent::HasTag(const FString& Tag) const
{
    // Case-insensitive tag search
    FString NormalizedTag = Tag.TrimStartAndEnd().ToLower();
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
    // Remove from subsystem first (before cleanup)
    if (!GEngine)
    {
        return;
    }

    URshipSubsystem* subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    if (!subsystem)
    {
        return;
    }

    // Send deletion events to server BEFORE cleaning up local state
    if (TargetData)
    {
        subsystem->DeleteTarget(TargetData);
    }

    // Clean up emitter handlers
    for (const auto& handler : EmitterHandlers)
    {
        if (handler.Value)
        {
            handler.Value->Destroy();
        }
    }
    EmitterHandlers.Empty();

    // Clean up target data
    if (TargetData)
    {
        delete TargetData;
        TargetData = nullptr;
    }

    if (subsystem->TargetComponents)
    {
        subsystem->TargetComponents->Remove(this);
    }

    // Unregister from GroupManager
    if (URshipTargetGroupManager* GroupManager = subsystem->GetGroupManager())
    {
        GroupManager->UnregisterTarget(this);
    }

    UE_LOG(LogRshipExec, Log, TEXT("Target unregistered: %s"), *targetName);
}

void URshipTargetComponent::SetTargetId(const FString& NewTargetId)
{
    if (NewTargetId.IsEmpty())
    {
        UE_LOG(LogRshipExec, Warning, TEXT("SetTargetId called with empty ID - ignoring"));
        return;
    }

    // If same ID, no need to re-register
    if (targetName == NewTargetId)
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("SetTargetId called with same ID (%s) - no change needed"), *NewTargetId);
        return;
    }

    FString OldTargetId = targetName;

    // Unregister with old ID if we were registered
    if (TargetData != nullptr)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Changing Target ID from '%s' to '%s'"), *OldTargetId, *NewTargetId);
        Unregister();
    }

    // Set new ID
    targetName = NewTargetId;

    // Re-register with new ID
    Register();

    UE_LOG(LogRshipExec, Log, TEXT("Target ID changed: %s -> %s"), *OldTargetId, *NewTargetId);
}

void URshipTargetComponent::RescanSiblingComponents()
{
    if (!GEngine)
    {
        return;
    }

    URshipSubsystem* subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    if (!subsystem)
    {
        return;
    }

    AActor* parent = GetOwner();
    if (!parent)
    {
        return;
    }

    if (!TargetData)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RescanSiblingComponents called before Register() - calling Register() first"));
        Register();
        return;
    }

    FString fullTargetId = subsystem->GetServiceId() + ":" + this->targetName;

    TArray<UActorComponent*> siblingComponents;
    parent->GetComponents(siblingComponents);

    UE_LOG(LogRshipExec, Log, TEXT("Rescanning %d sibling components for new RS_ members"), siblingComponents.Num());

    int32 newActionsCount = 0;
    int32 newEmittersCount = 0;

    for (UActorComponent* sibling : siblingComponents)
    {
        UClass* siblingClass = sibling->GetClass();

        // Scan for new RS_ functions (Actions)
        for (TFieldIterator<UFunction> siblingFunc(siblingClass, EFieldIteratorFlags::ExcludeSuper); siblingFunc; ++siblingFunc)
        {
            FString funcName = (*siblingFunc)->GetName();
            if (funcName.StartsWith("RS_"))
            {
                FString fullActionId = fullTargetId + ":" + funcName;
                // Check if this action is already registered
                if (!TargetData->GetActions().Contains(fullActionId))
                {
                    this->RegisterFunction(sibling, *siblingFunc, &fullTargetId);
                    UE_LOG(LogRshipExec, Log, TEXT("  New action found: %s"), *funcName);
                    newActionsCount++;
                }
            }
        }

        // Scan for new RS_ properties (Actions)
        for (TFieldIterator<FProperty> siblingProp(siblingClass, EFieldIteratorFlags::ExcludeSuper); siblingProp; ++siblingProp)
        {
            FString propName = (*siblingProp)->GetName();
            if (propName.StartsWith("RS_") && !(*siblingProp)->IsA<FMulticastDelegateProperty>())
            {
                FString fullActionId = fullTargetId + ":" + propName;
                // Check if this action is already registered
                if (!TargetData->GetActions().Contains(fullActionId))
                {
                    this->RegisterProperty(sibling, *siblingProp, &fullTargetId);
                    UE_LOG(LogRshipExec, Log, TEXT("  New property action found: %s"), *propName);
                    newActionsCount++;
                }
            }
        }

        // Scan for new RS_ emitters
        for (TFieldIterator<FMulticastInlineDelegateProperty> It(siblingClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            FMulticastInlineDelegateProperty* EmitterProp = *It;
            FString EmitterName = EmitterProp->GetName();

            if (!EmitterName.StartsWith("RS_"))
            {
                continue;
            }

            // Check if this emitter is already registered
            if (EmitterHandlers.Contains(EmitterName))
            {
                continue;
            }

            FString fullEmitterId = fullTargetId + ":" + EmitterName;

            auto emitter = new EmitterContainer(fullEmitterId, EmitterName, EmitterProp);
            this->TargetData->AddEmitter(emitter);

            FMulticastScriptDelegate EmitterDelegate = EmitterProp->GetPropertyValue_InContainer(sibling);

            TScriptDelegate localDelegate;

            auto world = GetWorld();
            if (!world)
            {
                UE_LOG(LogRshipExec, Warning, TEXT("World Not Found during rescan"));
                continue;
            }

            FActorSpawnParameters spawnInfo;
            spawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            spawnInfo.Owner = parent;
            spawnInfo.bNoFail = true;
            spawnInfo.bDeferConstruction = false;
            spawnInfo.bAllowDuringConstructionScript = true;

            AEmitterHandler* handler = world->SpawnActor<AEmitterHandler>(spawnInfo);
#if WITH_EDITOR
            handler->SetActorLabel(parent->GetActorLabel() + " " + EmitterName + " Handler");
#endif

            handler->SetServiceId(subsystem->GetServiceId());
            handler->SetTargetId(fullTargetId);
            handler->SetEmitterId(EmitterName);
            handler->SetDelegate(&localDelegate);

            localDelegate.BindUFunction(handler, TEXT("ProcessEmitter"));

            EmitterDelegate.Add(localDelegate);

            EmitterProp->SetPropertyValue_InContainer(sibling, EmitterDelegate);

            this->EmitterHandlers.Add(EmitterName, handler);

            UE_LOG(LogRshipExec, Log, TEXT("  New emitter found: %s"), *EmitterName);
            newEmittersCount++;
        }
    }

    if (newActionsCount > 0 || newEmittersCount > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Rescan complete: %d new actions, %d new emitters. Sending update to server."), newActionsCount, newEmittersCount);
        subsystem->SendAll();
    }
    else
    {
        UE_LOG(LogRshipExec, Log, TEXT("Rescan complete: no new RS_ members found"));
    }
}