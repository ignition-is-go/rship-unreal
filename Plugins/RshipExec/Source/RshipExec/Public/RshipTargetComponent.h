// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EmitterHandler.h"
#include "Target.h"
#include "RshipTargetComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipData);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RSHIPEXEC_API URshipTargetComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnRegister() override;
	virtual void OnComponentDestroyed(bool bDestoryHierarchy) override;

	void OnDataReceived();
	void HandleAfterTake(const FString& ActionName, UObject* ActionOwner);

	UPROPERTY(BlueprintAssignable)
	FOnRshipData OnRshipData;

	UFUNCTION(BlueprintCallable, Category = "RshipTarget")
	void Reconnect();

	UFUNCTION(BlueprintCallable, Category = "RshipTarget")
	void Register();

	UFUNCTION(BlueprintCallable, Category = "RshipTarget")
	void Unregister();

	UFUNCTION(BlueprintCallable, Category = "RshipTarget")
	void RescanSiblingComponents();

	UFUNCTION(BlueprintCallable, Category = "RshipTarget")
	void SetTargetId(const FString& NewTargetId);

	bool RegisterWhitelistedFunction(UObject* Owner, const FName& FunctionName, const FString& ExposedActionName = TEXT(""));
	bool RegisterWhitelistedProperty(UObject* Owner, const FName& PropertyName, const FString& ExposedActionName = TEXT(""));
	bool RegisterWhitelistedEmitter(UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "RshipTarget")
	FString GetTargetId() const { return targetName; }

	UFUNCTION(BlueprintPure, Category = "RshipTarget")
	bool IsRegistered() const { return TargetData != nullptr; }

	UPROPERTY(EditAnywhere, Category = "RshipTarget", meta = (DisplayName = "Target Id"))
	FString targetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RshipTarget", meta = (DisplayName = "Category"))
	FString Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RshipTarget|Organization")
	TArray<FString> Tags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RshipTarget|Organization")
	TArray<FString> GroupIds;

	UFUNCTION(BlueprintCallable, Category = "RshipTarget|Organization")
	bool HasTag(const FString& Tag) const;

	UFUNCTION(BlueprintCallable, Category = "RshipTarget|Organization")
	TArray<FString> GetTags() const { return Tags; }

	TMap<FString, AEmitterHandler*> EmitterHandlers;
	Target* TargetData = nullptr;

private:
	bool TryRegisterFunctionAction(UObject* Owner, UFunction* Function, const FString& FullTargetId, const FString& ExposedActionName = TEXT(""), bool bRequireRSPrefix = false);
	bool TryRegisterPropertyAction(UObject* Owner, FProperty* Property, const FString& FullTargetId, const FString& ExposedActionName = TEXT(""), bool bRequireRSPrefix = false);
	bool TryRegisterEmitter(UObject* Owner, FMulticastInlineDelegateProperty* EmitterProperty, const FString& FullTargetId, const FString& ExposedEmitterName = TEXT(""), bool bRequireRSPrefix = false);
	uint32 ComputeSiblingComponentSignature() const;
	void RebuildActionProviderCache();
	void GatherSiblingComponents(TArray<UActorComponent*>& OutSiblingComponents) const;
	void RegisterScannableMembers(UObject* OwnerObject, const FString& FullTargetId, const FString& MutableTargetId, bool bRequireRSPrefix);
	void RegisterProviderWhitelistActions(AActor* OwnerActor, const TArray<UActorComponent*>& SiblingComponents);

	uint32 CachedSiblingComponentSignature = 0;
	bool bHasCachedSiblingComponentSignature = false;
	TArray<TWeakObjectPtr<UObject>> CachedActionProviderObjects;
};
