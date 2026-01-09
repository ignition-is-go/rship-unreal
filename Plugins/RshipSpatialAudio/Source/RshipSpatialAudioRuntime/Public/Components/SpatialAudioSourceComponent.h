// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/SpatialAudioObject.h"
#include "SpatialAudioSourceComponent.generated.h"

class URshipSpatialAudioManager;

/**
 * Component that binds an actor to a spatial audio object.
 * When attached to an actor, it will automatically update the audio object's
 * position to match the actor's world position.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent, DisplayName = "Spatial Audio Source"))
class RSHIPSPATIALAUDIORUNTIME_API USpatialAudioSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialAudioSourceComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** Name for the audio object (defaults to owning actor name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Config")
	FString AudioObjectName;

	/** Initial spread in degrees (0-180) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Config", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float InitialSpread = 0.0f;

	/** Initial gain in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Config", meta = (ClampMin = "-80.0", ClampMax = "12.0"))
	float InitialGain = 0.0f;

	/** Whether to start muted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Config")
	bool bStartMuted = false;

	/** Auto-register on BeginPlay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Config")
	bool bAutoRegister = true;

	/** Update rate in Hz (0 = every frame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Config", meta = (ClampMin = "0", ClampMax = "120"))
	int32 UpdateRateHz = 60;

	/** Position offset from actor origin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Config")
	FVector PositionOffset = FVector::ZeroVector;

	// ========================================================================
	// ZONE ROUTING
	// ========================================================================

	/** Specific zone IDs to route to (empty = all zones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Routing")
	TArray<FString> ZoneRouting;

	// ========================================================================
	// RUNTIME API
	// ========================================================================

	/** Register this component with the spatial audio system */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void RegisterAudioObject();

	/** Unregister from the spatial audio system */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void UnregisterAudioObject();

	/** Check if currently registered */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Audio")
	bool IsRegistered() const { return AudioObjectId.IsValid(); }

	/** Get the audio object ID */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Audio")
	FGuid GetAudioObjectId() const { return AudioObjectId; }

	// ========================================================================
	// PARAMETER CONTROL
	// ========================================================================

	/** Set the spread (0-180 degrees) */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Parameters")
	void SetSpread(float Spread);

	/** Set the gain in dB */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Parameters")
	void SetGain(float GainDb);

	/** Set mute state */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Parameters")
	void SetMuted(bool bMuted);

	/** Update zone routing */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Parameters")
	void SetZoneRouting(const TArray<FGuid>& ZoneIds);

	/** Force position update (call when actor moves but isn't ticking) */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Parameters")
	void UpdatePosition();

	// ========================================================================
	// METERING
	// ========================================================================

	/** Get the last meter reading for this object */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Audio|Metering")
	FSpatialMeterReading GetMeterReading() const;

private:
	/** The audio object ID assigned by the manager */
	UPROPERTY()
	FGuid AudioObjectId;

	/** Cached reference to the audio manager */
	UPROPERTY()
	URshipSpatialAudioManager* AudioManager;

	/** Last update time for rate limiting */
	float LastUpdateTime;

	/** Cached last position to avoid redundant updates */
	FVector LastPosition;

	/** Get or create the audio manager reference */
	URshipSpatialAudioManager* GetAudioManager();
};
