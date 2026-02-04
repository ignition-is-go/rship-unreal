// Rship PCG Auto-Bind Component
// Automatically binds PCG-spawned actor properties to rShip

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PCG/RshipPCGTypes.h"
#include "RshipPCGAutoBindComponent.generated.h"

class URshipSubsystem;
class URshipPCGManager;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipPCGBound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRshipPCGParamChanged, FName, ParamName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipPCGActionReceived, FName, ActionName, const FString&, JsonData);

// ============================================================================
// AUTO-BIND COMPONENT
// ============================================================================

/**
 * Component that automatically binds PCG-spawned actor properties to rShip.
 *
 * Attach this component to any actor spawned by PCG to automatically:
 * - Register the actor as an rShip Target with deterministic ID
 * - Expose marked properties as rShip parameters (Actions for writes, Emitters for reads)
 * - Handle property change detection for pulse emission
 * - Manage lifecycle (register on spawn, deregister on destroy)
 *
 * Properties are bound using metadata:
 *   UPROPERTY(EditAnywhere, meta=(RShipParam))
 *   UPROPERTY(EditAnywhere, meta=(RShipParam="CustomName", RShipCategory="Lighting"))
 *   UPROPERTY(EditAnywhere, meta=(RShipParam, RShipMin="0", RShipMax="1", RShipPulseMode="onchange"))
 *
 * For Blueprint compatibility, properties prefixed with "RS_" are also automatically bound.
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship PCG Auto-Bind"))
class RSHIPEXEC_API URshipPCGAutoBindComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URshipPCGAutoBindComponent();

	// ========================================================================
	// COMPONENT LIFECYCLE
	// ========================================================================

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	// ========================================================================
	// PCG IDENTITY
	// ========================================================================

	/** The PCG instance identity (set by PCG spawner or computed automatically) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
	FRshipPCGInstanceId InstanceId;

	/** If true, auto-generate ID from actor position when InstanceId is not set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
	bool bAutoGenerateId = true;

	/** Optional custom target name (overrides generated name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
	FString CustomTargetName;

	/** Category for organizing in rShip UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
	FString TargetCategory = TEXT("PCG");

	/** Tags for filtering and grouping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
	TArray<FString> Tags;

	// ========================================================================
	// BINDING CONFIGURATION
	// ========================================================================

	/** Enable automatic property binding on registration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Binding")
	bool bAutoBindProperties = true;

	/** Include properties from sibling components */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Binding")
	bool bIncludeSiblingComponents = true;

	/** Include inherited properties (not just class-specific) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Binding")
	bool bIncludeInheritedProperties = false;

	/** Default pulse mode for readable properties */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Binding")
	ERshipPCGPulseMode DefaultPulseMode = ERshipPCGPulseMode::Off;

	/** Default pulse rate in Hz */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Binding", meta = (ClampMin = "0.1", ClampMax = "60.0"))
	float DefaultPulseRateHz = 10.0f;

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Called when the component has been bound to rShip */
	UPROPERTY(BlueprintAssignable, Category = "Rship|PCG|Events")
	FOnRshipPCGBound OnRshipBound;

	/** Called when a parameter value is changed from rShip */
	UPROPERTY(BlueprintAssignable, Category = "Rship|PCG|Events")
	FOnRshipPCGParamChanged OnRshipParamChanged;

	/** Called when any action is received from rShip */
	UPROPERTY(BlueprintAssignable, Category = "Rship|PCG|Events")
	FOnRshipPCGActionReceived OnRshipActionReceived;

	// ========================================================================
	// PUBLIC API
	// ========================================================================

	/** Set the PCG instance identity */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void SetInstanceId(const FRshipPCGInstanceId& InInstanceId);

	/** Get the current instance identity */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	FRshipPCGInstanceId GetInstanceId() const { return InstanceId; }

	/** Check if this component is registered with rShip */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	bool IsRegistered() const { return bIsRegistered; }

	/** Force re-registration with rShip */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void Reregister();

	/** Force re-scan of properties */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void RescanProperties();

	/** Get all bound property names */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	TArray<FName> GetBoundPropertyNames() const;

	/** Get property value as string */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	FString GetPropertyValueAsString(FName PropertyName) const;

	/** Set property value from string */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	bool SetPropertyValueFromString(FName PropertyName, const FString& Value);

	/** Get property value as JSON */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	FString GetPropertyValueAsJson(FName PropertyName) const;

	/** Set property value from JSON */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	bool SetPropertyValueFromJson(FName PropertyName, const FString& JsonValue);

	/** Force emit pulse for a specific property */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void EmitPulse(FName PropertyName);

	/** Force emit pulse for all readable properties */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
	void EmitAllPulses();

	/** Get the full target path for rShip */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
	FString GetTargetPath() const;

	// ========================================================================
	// PROPERTY ACCESS (Type-safe)
	// ========================================================================

	/** Get float property value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG|Properties")
	float GetFloatProperty(FName PropertyName, bool& bSuccess) const;

	/** Set float property value */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG|Properties")
	bool SetFloatProperty(FName PropertyName, float Value);

	/** Get int property value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG|Properties")
	int32 GetIntProperty(FName PropertyName, bool& bSuccess) const;

	/** Set int property value */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG|Properties")
	bool SetIntProperty(FName PropertyName, int32 Value);

	/** Get bool property value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG|Properties")
	bool GetBoolProperty(FName PropertyName, bool& bSuccess) const;

	/** Set bool property value */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG|Properties")
	bool SetBoolProperty(FName PropertyName, bool Value);

	/** Get vector property value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG|Properties")
	FVector GetVectorProperty(FName PropertyName, bool& bSuccess) const;

	/** Set vector property value */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG|Properties")
	bool SetVectorProperty(FName PropertyName, FVector Value);

	/** Get rotator property value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG|Properties")
	FRotator GetRotatorProperty(FName PropertyName, bool& bSuccess) const;

	/** Set rotator property value */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG|Properties")
	bool SetRotatorProperty(FName PropertyName, FRotator Value);

	/** Get color property value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG|Properties")
	FLinearColor GetColorProperty(FName PropertyName, bool& bSuccess) const;

	/** Set color property value */
	UFUNCTION(BlueprintCallable, Category = "Rship|PCG|Properties")
	bool SetColorProperty(FName PropertyName, FLinearColor Value);

	// ========================================================================
	// INTERNAL - Called by PCG Manager
	// ========================================================================

	/** Called when an action is received from rShip */
	void HandleAction(const FString& ActionId, const TSharedPtr<FJsonObject>& Data);

	/** Get the class bindings for this actor */
	const FRshipPCGClassBindings* GetClassBindings() const { return ClassBindings; }

	/** Get property states for pulse emission */
	TArray<FRshipPCGPropertyState>& GetPropertyStates() { return PropertyStates; }

private:
	// ========================================================================
	// PRIVATE STATE
	// ========================================================================

	UPROPERTY()
	URshipSubsystem* Subsystem;

	/** Cached class bindings (shared across instances of same class) */
	FRshipPCGClassBindings* ClassBindings;

	/** Per-property runtime state */
	TArray<FRshipPCGPropertyState> PropertyStates;

	/** Map from property name to owner object (actor or component) */
	TMap<FName, TWeakObjectPtr<UObject>> PropertyOwners;

	/** Is this component registered with rShip? */
	bool bIsRegistered;

	/** Has this component been initialized? */
	bool bIsInitialized;

	/** Time of last full pulse check */
	double LastPulseCheckTime;

	// ========================================================================
	// PRIVATE METHODS
	// ========================================================================

	/** Initialize bindings and register with rShip */
	void InitializeBinding();

	/** Build property bindings for the owner actor */
	void BuildPropertyBindings();

	/** Register this target with the PCG manager */
	void RegisterWithManager();

	/** Unregister from the PCG manager */
	void UnregisterFromManager();

	/** Generate an automatic instance ID from actor properties */
	void GenerateAutoInstanceId();

	/** Initialize property states from class bindings */
	void InitializePropertyStates();

	/** Check for property changes and emit pulses */
	void CheckPropertyChanges(float DeltaTime);

	/** Emit pulse for a single property */
	void EmitPropertyPulse(int32 PropertyIndex);

	/** Find property descriptor and owner */
	bool FindPropertyAndOwner(FName PropertyName, FRshipPCGPropertyDescriptor*& OutDesc, UObject*& OutOwner);
	bool FindPropertyAndOwner(FName PropertyName, const FRshipPCGPropertyDescriptor*& OutDesc, const UObject*& OutOwner) const;

	/** Apply action value to property */
	bool ApplyActionToProperty(const FRshipPCGPropertyDescriptor& Desc, UObject* Owner, const TSharedPtr<FJsonObject>& Data);

	/** Validate and clamp property value if range is specified */
	void ClampPropertyValue(const FRshipPCGPropertyDescriptor& Desc, void* ValuePtr);
};
