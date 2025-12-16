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
        handler.Value->Destroy();
    }

    URshipSubsystem *subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

    if(!subsystem)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipTargetComponent: Subsystem not found during destruction"));
        return;
	}

    if (!subsystem->TargetComponents) {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipTargetComponent: Subsystem TargetComponents not found during destruction"));
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

    for (UActorComponent* sibling : siblingComponents) {
        UClass* siblingClass = sibling->GetClass();
        for (TFieldIterator<UFunction> siblingFunc(siblingClass, EFieldIteratorFlags::ExcludeSuper); siblingFunc; ++siblingFunc) {
            this->RegisterFunction(sibling, *siblingFunc, &fullTargetId);
        }

        for (TFieldIterator<FProperty> siblingProp(siblingClass, EFieldIteratorFlags::ExcludeSuper); siblingProp; ++siblingProp) {
            this->RegisterProperty(sibling, *siblingProp, &fullTargetId);
        }
    }

    for (TFieldIterator<FMulticastInlineDelegateProperty> It(ownerClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
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

        FMulticastScriptDelegate EmitterDelegate = EmitterProp->GetPropertyValue_InContainer(parent);

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

        EmitterProp->SetPropertyValue_InContainer(parent, EmitterDelegate);

        this->EmitterHandlers.Add(EmitterName, handler);
    }

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