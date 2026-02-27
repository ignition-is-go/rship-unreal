// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/RshipBindings.h"

class URshipActorRegistrationComponent;
class URshipSubsystem;

class RSHIPEXEC_API Target
{
private:
	FString id;
	FString name;
	TArray<FString> parentTargetIds;
	TMap<FString, FRshipActionBinding> actions;
	TMap<FString, FRshipEmitterBinding> emitters;
	TWeakObjectPtr<URshipActorRegistrationComponent> BoundTargetComponent;
	TWeakObjectPtr<URshipSubsystem> BoundSubsystem;

public:
	Target(FString id, URshipSubsystem* InSubsystem = nullptr);
	~Target();

	Target(const Target&) = delete;
	Target& operator=(const Target&) = delete;
	Target(Target&&) = delete;
	Target& operator=(Target&&) = delete;

	void AddAction(const FRshipActionBinding& action);
	void AddEmitter(const FRshipEmitterBinding& emitter);

	FString GetId() const;
	void SetId(const FString& InId);
	const FString& GetName() const;
	void SetName(const FString& InName);
	const TArray<FString>& GetParentTargetIds() const;
	void SetParentTargetIds(const TArray<FString>& InParentTargetIds);
	const TMap<FString, FRshipActionBinding>& GetActions() const;
	const TMap<FString, FRshipEmitterBinding>& GetEmitters() const;

	void SetBoundTargetComponent(URshipActorRegistrationComponent* InTargetComponent);
	URshipActorRegistrationComponent* GetBoundTargetComponent() const;
	URshipSubsystem* GetBoundSubsystem() const;

	bool TakeAction(AActor* actor, FString actionId, const TSharedRef<FJsonObject> data);
};
