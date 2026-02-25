// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EmitterHandler.h"
#include "Target.h"
#include "RshipTargetComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipData);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URshipTargetComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual void OnRegister() override;

	virtual void OnComponentDestroyed(bool bDestoryHierarchy) override;

	void OnDataReceived();

	UPROPERTY(BlueprintAssignable)
	FOnRshipData OnRshipData;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RshipTarget")
	void Reconnect();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RshipTarget")
	void Register();

	/** Unregister this target from rship (cleans up emitters and removes from server) */
	UFUNCTION(BlueprintCallable, Category = "RshipTarget")
	void Unregister();

	/** Re-scan sibling components for RS_ members and update registration.
	 *  Call this when a new component with RS_ members is added at runtime. */
	UFUNCTION(BlueprintCallable, Category = "RshipTarget")
	void RescanSiblingComponents();

	/** Set the Target ID dynamically and re-register with the new ID.
	 *  Useful for PCG-spawned actors or runtime-generated targets.
	 *  @param NewTargetId The new unique identifier for this target */
	UFUNCTION(BlueprintCallable, Category = "RshipTarget")
	void SetTargetId(const FString& NewTargetId);

	/** Get the current Target ID */
	UFUNCTION(BlueprintPure, Category = "RshipTarget")
	FString GetTargetId() const { return targetName; }

	/** Check if this target is currently registered with rship */
	UFUNCTION(BlueprintPure, Category = "RshipTarget")
	bool IsRegistered() const { return TargetData != nullptr; }

	UPROPERTY(EditAnywhere, config, Category = "RshipTarget", meta = (DisplayName = "Target Id"))
	FString targetName;

	/** Category for organizing targets (e.g., "light", "camera", "actor") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RshipTarget", meta = (DisplayName = "Category"))
	FString Category;

	// ========================================================================
	// ORGANIZATION - Tags and Groups
	// ========================================================================

	/** User-defined tags for organizing and filtering targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RshipTarget|Organization")
	TArray<FString> Tags;

	/** Groups this target belongs to (managed by URshipTargetGroupManager) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RshipTarget|Organization")
	TArray<FString> GroupIds;

	/** Check if this target has a specific tag */
	UFUNCTION(BlueprintCallable, Category = "RshipTarget|Organization")
	bool HasTag(const FString& Tag) const;

	/** Get all tags on this target */
	UFUNCTION(BlueprintCallable, Category = "RshipTarget|Organization")
	TArray<FString> GetTags() const { return Tags; }

	TMap<FString, AEmitterHandler*> EmitterHandlers;

	Target* TargetData;


private: 

	void RegisterFunction(UObject* owner, UFunction* func, FString *targetId);
	void RegisterProperty(UObject* owner, FProperty* prop, FString* targetId);
};
