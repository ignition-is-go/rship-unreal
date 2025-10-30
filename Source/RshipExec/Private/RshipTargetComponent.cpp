// Fill out your copyright notice in the Description page of Project Settings.

#include "RshipTargetComponent.h"
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

    subsystem->TargetComponents->Remove(this);
}

// Called every frame
void URshipTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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

    auto actions = this->TargetData->GetActions();

    auto action = new Action(fullActionId, name, func, owner);
    this->TargetData->AddAction(action);
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

    UE_LOG(LogRshipExec, Log, TEXT("Registering OUTLINER: %s as %s"), *outlinerName, *this->targetName);

    FString fullTargetId = subsystem->GetServiceId() + ":" + this->targetName;

    this->TargetData = new Target(fullTargetId);

    UClass *ownerClass = parent->GetClass();

    for (TFieldIterator<UFunction> field(ownerClass, EFieldIteratorFlags::ExcludeSuper); field; ++field)
    {
        this->RegisterFunction(parent, *field, &fullTargetId);
    }

    TArray<UActorComponent*> siblingComponents;

    parent->GetComponents(siblingComponents);

    for (UActorComponent* sibling : siblingComponents) {
        UClass* siblingClass = sibling->GetClass();
        for (TFieldIterator<UFunction> siblingFunc(siblingClass, EFieldIteratorFlags::ExcludeSuper); siblingFunc; ++siblingFunc) {
            this->RegisterFunction(sibling, *siblingFunc, &fullTargetId);
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

        handler->SetActorLabel(parent->GetActorLabel() + " " + EmitterName + " Handler");

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

    UE_LOG(LogRshipExec, Log, TEXT("Component Registered: %s"), *parent->GetName());
}