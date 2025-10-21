// Fill out your copyright notice in the Description page of Project Settings.

#include "RshipTargetComponent.h"
#include <iostream>
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RshipSubsystem.h"
#include "GameFramework/Actor.h"
#include "Misc/DefaultValueHelper.h"
#include "Util.h"
#include "Action.h"
#include "EmitterContainer.h"

namespace
{
static TArray<FRshipSchemaField> BuildSchemaFields(TDoubleLinkedList<RshipSchemaProperty>* Props)
{
        TArray<FRshipSchemaField> Fields;
        if (!Props)
        {
                return Fields;
        }

        for (const RshipSchemaProperty& Prop : *Props)
        {
                FRshipSchemaField Field;
                Field.Name = Prop.Name;
                Field.Type = Prop.Type;
                Fields.Add(Field);
        }

        return Fields;
}
}

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

	for (const auto& handler : EmitterHandlers)
	{
		handler.Value->Destroy();
	}

	URshipSubsystem* subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

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



void URshipTargetComponent::Register()
{

	URshipSubsystem* subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

	AActor *parent = GetOwner();

	if (!parent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Parent not found"));
	}

	subsystem->TargetComponents->Add(this);

    FString fullTargetId = subsystem->GetServiceId() + ":" + this->targetName;

    this->TargetData = new Target(fullTargetId);

    UClass* ownerClass = parent->GetClass();

    for (TFieldIterator<UFunction> field(ownerClass, EFieldIteratorFlags::ExcludeSuper); field; ++field)
    {
        UFunction* handler = *field;

        FString name = field->GetName();

        if (!name.StartsWith("RS_"))
        {
            continue;
        }
        FString fullActionId = fullTargetId + ":" + name;

        auto actions = this->TargetData->GetActions();

        auto action = new Action(fullActionId, name, handler);
        this->TargetData->AddAction(action);
    }

    for (TFieldIterator<FMulticastInlineDelegateProperty> It(ownerClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        FMulticastInlineDelegateProperty* EmitterProp = *It;
        FString EmitterName = EmitterProp->GetName();
        FName EmitterType = EmitterProp->GetClass()->GetFName();

        UE_LOG(LogTemp, Warning, TEXT("Emitter: %s, Type: %s"), *EmitterName, *EmitterType.ToString());

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

        if (!world) {
            UE_LOG(LogTemp, Warning, TEXT("World Not Found"));
            return;
        }

        FActorSpawnParameters spawnInfo;
        spawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        spawnInfo.Owner = parent;
        spawnInfo.bNoFail = true;
        spawnInfo.bDeferConstruction = false;
        spawnInfo.bAllowDuringConstructionScript = true;


        if (this->EmitterHandlers.Contains(EmitterName)) {
            return;
        }

        AEmitterHandler* handler = world->SpawnActor<AEmitterHandler>(spawnInfo);

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

    FRshipTargetDescription Snapshot = GetTargetDescription();

    UE_LOG(LogTemp, Display, TEXT("Rship Target '%s' exposes %d actions and %d emitters"),
        *Snapshot.TargetId, Snapshot.Actions.Num(), Snapshot.Emitters.Num());

    for (const FRshipActionDescription& ActionDesc : Snapshot.Actions)
    {
        FString ParamText;
        for (const FRshipSchemaField& Field : ActionDesc.Parameters)
        {
                ParamText += FString::Printf(TEXT("%s(%s) "), *Field.Name, *Field.Type);
        }
        UE_LOG(LogTemp, Display, TEXT("  Action: %s [%s] %s"), *ActionDesc.ActionId, *ActionDesc.FunctionName, *ParamText);
    }

    for (const FRshipEmitterDescription& EmitterDesc : Snapshot.Emitters)
    {
        FString PayloadText;
        for (const FRshipSchemaField& Field : EmitterDesc.Payload)
        {
                PayloadText += FString::Printf(TEXT("%s(%s) "), *Field.Name, *Field.Type);
        }
        UE_LOG(LogTemp, Display, TEXT("  Emitter: %s %s"), *EmitterDesc.EmitterId, *PayloadText);
    }

        UE_LOG(LogTemp, Warning, TEXT("Component Registered: %s"), *parent->GetName());
}

FRshipTargetDescription URshipTargetComponent::GetTargetDescription() const
{
        FRshipTargetDescription Description;
        Description.TargetName = targetName;

        if (!TargetData)
        {
                return Description;
        }

        Description.TargetId = TargetData->GetId();
        if (Description.TargetName.IsEmpty())
        {
                Description.TargetName = Description.TargetId;
        }

        auto Actions = TargetData->GetActions();
        for (const TPair<FString, Action*>& Pair : Actions)
        {
                if (!Pair.Value)
                {
                        continue;
                }

                FRshipActionDescription ActionDesc;
                ActionDesc.ActionId = Pair.Value->GetId();
                ActionDesc.DisplayName = Pair.Value->GetName();
                ActionDesc.FunctionName = Pair.Value->GetFunctionName();
                ActionDesc.Parameters = BuildSchemaFields(Pair.Value->GetProps());

                Description.Actions.Add(ActionDesc);
        }

        auto Emitters = TargetData->GetEmitters();
        for (const TPair<FString, EmitterContainer*>& Pair : Emitters)
        {
                if (!Pair.Value)
                {
                        continue;
                }

                FRshipEmitterDescription EmitterDesc;
                EmitterDesc.EmitterId = Pair.Value->GetId();
                EmitterDesc.DisplayName = Pair.Value->GetName();
                EmitterDesc.Payload = BuildSchemaFields(Pair.Value->GetProps());

                Description.Emitters.Add(EmitterDesc);
        }

        return Description;
}

TArray<FRshipActionDescription> URshipTargetComponent::GetActionDescriptions() const
{
        return GetTargetDescription().Actions;
}

TArray<FRshipEmitterDescription> URshipTargetComponent::GetEmitterDescriptions() const
{
        return GetTargetDescription().Emitters;
}