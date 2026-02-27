// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Target.h"
#include "Core/RshipTargetRegistrar.h"
#include "RshipActorRegistrationComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipData);

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Actor Registration"))
class RSHIPEXEC_API URshipActorRegistrationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnRegister() override;
	virtual void OnComponentDestroyed(bool bDestoryHierarchy) override;

	void OnDataReceived();

	UPROPERTY(BlueprintAssignable)
	FOnRshipData OnRshipData;

	UFUNCTION(BlueprintCallable, Category = "RshipActorRegistration")
	void Reconnect();

	UFUNCTION(BlueprintCallable, Category = "RshipActorRegistration")
	void Register();

	UFUNCTION(BlueprintCallable, Category = "RshipActorRegistration")
	void Unregister();

	UFUNCTION(BlueprintCallable, Category = "RshipActorRegistration")
	void SetTargetId(const FString& NewTargetId);

	UFUNCTION(BlueprintPure, Category = "RshipActorRegistration")
	FString GetTargetId() const { return targetName; }

	UFUNCTION(BlueprintPure, Category = "RshipActorRegistration")
	FString GetFullTargetId() const;

	FRshipTargetRegistrar GetTargetRegistrar() const;

	UFUNCTION(BlueprintPure, Category = "RshipActorRegistration")
	bool IsRegistered() const { return TargetData != nullptr; }

	UPROPERTY(EditAnywhere, Category = "RshipActorRegistration", meta = (DisplayName = "Target Id"))
	FString targetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RshipActorRegistration", meta = (DisplayName = "Category"))
	FString Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RshipActorRegistration|Organization")
	TArray<FString> Tags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RshipActorRegistration|Organization")
	TArray<FString> GroupIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RshipActorRegistration|Organization", meta = (DisplayName = "Parent Target Ids"))
	TArray<FString> ParentTargetIds;

	UFUNCTION(BlueprintCallable, Category = "RshipActorRegistration|Organization")
	bool HasTag(const FString& Tag) const;

	UFUNCTION(BlueprintCallable, Category = "RshipActorRegistration|Organization")
	TArray<FString> GetTags() const { return Tags; }

	Target* TargetData = nullptr;

private:
	TArray<FString> BuildFullParentTargetIds(const FString& ServiceId) const;
	void RebindSiblingContributors();
};
