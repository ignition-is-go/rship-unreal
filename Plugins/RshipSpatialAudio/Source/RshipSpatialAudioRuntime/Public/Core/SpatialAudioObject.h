// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialAudioTypes.h"
#include "SpatialDSPTypes.h"
#include "Components/AudioComponent.h"
#include "SpatialAudioObject.generated.h"

/**
 * Represents a spatialized audio object (virtual sound source).
 * Objects are rendered through zones to speakers using the assigned renderer.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialAudioObject
{
	GENERATED_BODY()

	// ========================================================================
	// IDENTIFICATION
	// ========================================================================

	/** Unique identifier for this object */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object")
	FGuid Id;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object")
	FString Name;

	/** Color for visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object")
	FLinearColor Color = FLinearColor::Green;

	/** Group for organizational purposes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object")
	FString Group;

	// ========================================================================
	// SPATIAL PROPERTIES
	// ========================================================================

	/** World position of the object */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Spatial")
	FVector Position = FVector::ZeroVector;

	/** Object spread/width in degrees (0 = point source, 180 = omnidirectional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Spatial", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float Spread = 0.0f;

	/** Object size for DBAP calculations (larger = more speakers engaged) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Spatial", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
	float Size = 0.0f;

	/** Directivity pattern angle (0 = omnidirectional, 180 = highly directional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Spatial", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float DirectivityAngle = 0.0f;

	/** Direction the object is "facing" (for directivity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Spatial")
	FRotator DirectivityOrientation = FRotator::ZeroRotator;

	// ========================================================================
	// LEVEL & ROUTING
	// ========================================================================

	/** Object gain in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Level", meta = (ClampMin = "-80.0", ClampMax = "12.0"))
	float GainDb = 0.0f;

	/** Object muted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Level")
	bool bMuted = false;

	/** Object soloed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Level")
	bool bSoloed = false;

	/** Zones this object is routed to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Routing")
	TArray<FGuid> ZoneRouting;

	/** Per-zone gain modifiers in dB (keyed by zone ID as string) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Routing")
	TMap<FString, float> ZoneGainModifiers;

	// ========================================================================
	// SOURCE BINDING
	// ========================================================================

	/** Type of audio source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Source")
	ESpatialObjectSourceType SourceType = ESpatialObjectSourceType::UEAudioComponent;

	/** Bound Unreal audio component (if SourceType is UEAudioComponent) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Source")
	TWeakObjectPtr<UAudioComponent> BoundAudioComponent;

	/** External input channel index (if SourceType is ExternalInput) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Source", meta = (ClampMin = "0", ClampMax = "255"))
	int32 ExternalInputChannel = 0;

	/** Test oscillator frequency (if SourceType is Oscillator) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Source", meta = (ClampMin = "20.0", ClampMax = "20000.0"))
	float OscillatorFrequency = 1000.0f;

	// ========================================================================
	// AUTOMATION
	// ========================================================================

	/** Whether position is controlled by a bound Actor's transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Automation")
	bool bFollowBoundActor = true;

	/** Bound actor for transform following */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Automation")
	TWeakObjectPtr<AActor> BoundActor;

	/** Offset from bound actor position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Object|Automation")
	FVector BoundActorOffset = FVector::ZeroVector;

	// ========================================================================
	// RUNTIME STATE (not serialized)
	// ========================================================================

	/** Whether this object is currently active */
	bool bActive = true;

	/** Last computed gains per speaker (cached for efficiency) */
	TMap<FGuid, FSpatialSpeakerGain> CachedGains;

	/** Frame counter when gains were last computed */
	uint64 GainsComputedFrame = 0;

	/** Current meter reading */
	FSpatialMeterReading LastMeterReading;

	// ========================================================================
	// METHODS
	// ========================================================================

	FSpatialAudioObject()
	{
		Id = FGuid::NewGuid();
	}

	/** Get linear gain multiplier */
	float GetGainLinear() const
	{
		if (bMuted) return 0.0f;
		return FMath::Pow(10.0f, GainDb / 20.0f);
	}

	/** Get effective gain for a specific zone */
	float GetZoneGainLinear(const FGuid& ZoneId) const
	{
		float BaseGain = GetGainLinear();
		if (const float* Modifier = ZoneGainModifiers.Find(ZoneId.ToString()))
		{
			return BaseGain * FMath::Pow(10.0f, *Modifier / 20.0f);
		}
		return BaseGain;
	}

	/** Update position from bound actor if applicable */
	void UpdateFromBoundActor()
	{
		if (bFollowBoundActor && BoundActor.IsValid())
		{
			Position = BoundActor->GetActorLocation() + BoundActorOffset;
		}
	}

	/** Check if this object is routed to a specific zone */
	bool IsRoutedToZone(const FGuid& ZoneId) const
	{
		return ZoneRouting.Contains(ZoneId);
	}

	/** Get the forward vector for directivity */
	FVector GetDirectivityForward() const
	{
		return DirectivityOrientation.Vector();
	}
};

/**
 * Represents a routing bus in the audio system.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialBus
{
	GENERATED_BODY()

	// ========================================================================
	// IDENTIFICATION
	// ========================================================================

	/** Unique identifier for this bus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Bus")
	FGuid Id;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Bus")
	FString Name;

	/** Bus type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Bus")
	ESpatialBusType Type = ESpatialBusType::Object;

	// ========================================================================
	// ROUTING
	// ========================================================================

	/** Input sources (object IDs or other bus IDs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Bus")
	TArray<FGuid> InputSourceIds;

	/** Output destinations (bus IDs or output channel indices) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Bus")
	TArray<FGuid> OutputDestinationIds;

	// ========================================================================
	// PROCESSING
	// ========================================================================

	/** Bus DSP state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Bus")
	FSpatialBusDSPState DSP;

	// ========================================================================
	// METHODS
	// ========================================================================

	FSpatialBus()
	{
		Id = FGuid::NewGuid();
	}
};

/**
 * Routing matrix entry for source-to-destination connections.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialRoutingEntry
{
	GENERATED_BODY()

	/** Source ID (object, bus, or input) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Routing")
	FGuid SourceId;

	/** Destination ID (bus or output) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Routing")
	FGuid DestinationId;

	/** Send level in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Routing", meta = (ClampMin = "-80.0", ClampMax = "12.0"))
	float GainDb = 0.0f;

	/** Whether this routing is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Routing")
	bool bEnabled = true;

	/** Pre or post fader send */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Routing")
	bool bPreFader = false;

	/** Get linear gain */
	float GetGainLinear() const
	{
		if (!bEnabled) return 0.0f;
		return FMath::Pow(10.0f, GainDb / 20.0f);
	}
};
