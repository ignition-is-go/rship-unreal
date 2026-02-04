// Rship PCG Manager
// Central subsystem for managing PCG-spawned actor bindings

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCG/RshipPCGTypes.h"
#include "RshipPCGManager.generated.h"

class URshipSubsystem;
class URshipPCGAutoBindComponent;
class Target;
class Action;
class EmitterContainer;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPCGInstanceRegistered, URshipPCGAutoBindComponent*, Component);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPCGInstanceUnregistered, URshipPCGAutoBindComponent*, Component);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPCGActionExecuted, URshipPCGAutoBindComponent*, Component, const FString&, ActionId, bool, bSuccess);

// ============================================================================
// PCG MANAGER
// ============================================================================

/**
 * Central manager for PCG-spawned actor bindings.
 *
 * Responsibilities:
 * - Maintain registry of all PCG-bound instances
 * - Cache class bindings for efficient per-instance binding
 * - Route actions from rShip to correct instances
 * - Aggregate and emit pulses from instances
 * - Handle PCG regeneration (deregister old, register new)
 * - Provide debug/validation utilities
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipPCGManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(URshipSubsystem* InSubsystem);
	void Shutdown();
	void Tick(float DeltaTime);

	// ========================================================================
	// INSTANCE REGISTRATION
	// ========================================================================

	/** Register a PCG instance binding */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void RegisterInstance(URshipPCGAutoBindComponent* Component);

	/** Unregister a PCG instance binding */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void UnregisterInstance(URshipPCGAutoBindComponent* Component);

	/** Check if an instance is registered */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	bool IsInstanceRegistered(const FGuid& StableGuid) const;

	/** Find instance by stable GUID */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	URshipPCGAutoBindComponent* FindInstanceByGuid(const FGuid& StableGuid) const;

	/** Find instance by target path */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	URshipPCGAutoBindComponent* FindInstanceByPath(const FString& TargetPath) const;

	/** Get all registered instances */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	TArray<URshipPCGAutoBindComponent*> GetAllInstances() const;

	/** Get instance count */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	int32 GetInstanceCount() const { return RegisteredInstances.Num(); }

	// ========================================================================
	// CLASS BINDINGS
	// ========================================================================

	/** Get or create cached class bindings */
	FRshipPCGClassBindings* GetOrCreateClassBindings(UClass* Class);

	/** Get cached class bindings (returns nullptr if not cached) */
	const FRshipPCGClassBindings* GetClassBindings(UClass* Class) const;

	/** Invalidate cached class bindings (call after class modification) */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void InvalidateClassBindings(UClass* Class);

	/** Clear all cached class bindings */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void ClearAllClassBindings();

	// ========================================================================
	// ACTION ROUTING
	// ========================================================================

	/** Route an action to the appropriate instance */
	bool RouteAction(const FString& TargetPath, const FString& ActionId, const TSharedPtr<FJsonObject>& Data);

	/** Execute action on a specific instance */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	bool ExecuteAction(URshipPCGAutoBindComponent* Component, FName PropertyName, const FString& JsonValue);

	// ========================================================================
	// PULSE EMISSION
	// ========================================================================

	/** Emit a pulse for an instance */
	void EmitPulse(URshipPCGAutoBindComponent* Component, const FString& EmitterId, TSharedPtr<FJsonObject> Data);

	/** Force emit all pulses for all instances */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void EmitAllPulses();

	// ========================================================================
	// BULK OPERATIONS
	// ========================================================================

	/** Set property on all instances of a class */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	int32 SetPropertyOnAllInstances(UClass* Class, FName PropertyName, const FString& JsonValue);

	/** Set property on instances matching a tag */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	int32 SetPropertyOnTaggedInstances(const FString& Tag, FName PropertyName, const FString& JsonValue);

	/** Get all instances of a specific class */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	TArray<URshipPCGAutoBindComponent*> GetInstancesOfClass(UClass* Class) const;

	/** Get all instances with a specific tag */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	TArray<URshipPCGAutoBindComponent*> GetInstancesWithTag(const FString& Tag) const;

	// ========================================================================
	// DEBUG / VALIDATION
	// ========================================================================

	/** Dump all registered targets to log */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rship|PCG|Debug")
	void DumpAllTargets();

	/** Dump specific target to log */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rship|PCG|Debug")
	void DumpTarget(const FString& TargetPath);

	/** Validate all bindings */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rship|PCG|Debug")
	bool ValidateAllBindings();

	/** Get statistics */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG|Debug")
	FString GetStatistics() const;

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when a PCG instance is registered */
	UPROPERTY(BlueprintAssignable, Category = "Rship|PCG|Events")
	FOnPCGInstanceRegistered OnInstanceRegistered;

	/** Fired when a PCG instance is unregistered */
	UPROPERTY(BlueprintAssignable, Category = "Rship|PCG|Events")
	FOnPCGInstanceUnregistered OnInstanceUnregistered;

	/** Fired when an action is executed */
	UPROPERTY(BlueprintAssignable, Category = "Rship|PCG|Events")
	FOnPCGActionExecuted OnActionExecuted;

private:
	// ========================================================================
	// PRIVATE STATE
	// ========================================================================

	UPROPERTY()
	URshipSubsystem* Subsystem;

	/** Registered instances by stable GUID */
	TMap<FGuid, TWeakObjectPtr<URshipPCGAutoBindComponent>> RegisteredInstances;

	/** Target path to GUID mapping for fast lookup */
	TMap<FString, FGuid> PathToGuidMap;

	/** Cached class bindings */
	TMap<TWeakObjectPtr<UClass>, FRshipPCGClassBindings> ClassBindingsCache;

	/** Pending registrations (batched for efficiency) */
	TArray<TWeakObjectPtr<URshipPCGAutoBindComponent>> PendingRegistrations;

	/** Last tick time */
	double LastTickTime;

	/** Statistics */
	int32 TotalRegistrations;
	int32 TotalUnregistrations;
	int32 TotalActionsRouted;
	int32 TotalPulsesEmitted;

	// ========================================================================
	// PRIVATE METHODS
	// ========================================================================

	/** Send target registration to rShip */
	void SendTargetRegistration(URshipPCGAutoBindComponent* Component);

	/** Send target deregistration to rShip */
	void SendTargetDeregistration(URshipPCGAutoBindComponent* Component);

	/** Build Target from component */
	Target* BuildTarget(URshipPCGAutoBindComponent* Component);

	/** Build Actions from class bindings */
	TArray<Action*> BuildActions(URshipPCGAutoBindComponent* Component, const FRshipPCGClassBindings& Bindings);

	/** Build Emitters from class bindings */
	TArray<EmitterContainer*> BuildEmitters(URshipPCGAutoBindComponent* Component, const FRshipPCGClassBindings& Bindings);

	/** Process pending registrations */
	void ProcessPendingRegistrations();

	/** Clean up stale instances */
	void CleanupStaleInstances();
};
