// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialAudioTypes.h"
#include "SpatialDSPTypes.h"
#include "SpatialSpeaker.generated.h"

/**
 * Represents a single loudspeaker in the spatial audio system.
 * Contains physical characteristics, position, routing, and DSP state.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialSpeaker
{
	GENERATED_BODY()

	// ========================================================================
	// IDENTIFICATION
	// ========================================================================

	/** Unique identifier for this speaker */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker")
	FGuid Id;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker")
	FString Name;

	/** Optional short label (e.g., "L1", "R2", "SUB_A") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker")
	FString Label;

	/** Color for visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker")
	FLinearColor Color = FLinearColor::White;

	// ========================================================================
	// PHYSICAL CHARACTERISTICS
	// ========================================================================

	/** Type of speaker */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Physical")
	ESpatialSpeakerType Type = ESpatialSpeakerType::PointSource;

	/** Manufacturer and model (e.g., "L-Acoustics K2", "d&b V8") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Physical")
	FString MakeModel;

	/** Nominal horizontal dispersion in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Physical", meta = (ClampMin = "10.0", ClampMax = "360.0"))
	float NominalDispersionH = 90.0f;

	/** Nominal vertical dispersion in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Physical", meta = (ClampMin = "10.0", ClampMax = "180.0"))
	float NominalDispersionV = 60.0f;

	/** Maximum SPL capability in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Physical", meta = (ClampMin = "80.0", ClampMax = "150.0"))
	float MaxSPL = 130.0f;

	/** Nominal frequency range - low end in Hz */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Physical", meta = (ClampMin = "20.0", ClampMax = "500.0"))
	float FrequencyRangeLow = 60.0f;

	/** Nominal frequency range - high end in Hz */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Physical", meta = (ClampMin = "1000.0", ClampMax = "25000.0"))
	float FrequencyRangeHigh = 18000.0f;

	// ========================================================================
	// SPATIAL POSITION
	// ========================================================================

	/** World position in Unreal coordinates (centimeters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Position")
	FVector WorldPosition = FVector::ZeroVector;

	/** Speaker orientation (where it's pointing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Position")
	FRotator Orientation = FRotator::ZeroRotator;

	// ========================================================================
	// HIERARCHY & ROUTING
	// ========================================================================

	/** Parent array ID (if this speaker is part of an array) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Routing")
	FGuid ParentArrayId;

	/** Zone ID this speaker belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Routing")
	FGuid ZoneId;

	/** Physical output channel index (0-based) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|Routing", meta = (ClampMin = "0", ClampMax = "511"))
	int32 OutputChannel = 0;

	// ========================================================================
	// DSP STATE
	// ========================================================================

	/** Per-speaker DSP processing state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Speaker|DSP")
	FSpatialSpeakerDSPState DSP;

	// ========================================================================
	// RUNTIME STATE (not serialized)
	// ========================================================================

	/** Whether this speaker is currently online/active */
	bool bOnline = true;

	/** Last meter reading */
	FSpatialMeterReading LastMeterReading;

	// ========================================================================
	// METHODS
	// ========================================================================

	FSpatialSpeaker()
	{
		Id = FGuid::NewGuid();
	}

	/** Get forward vector based on orientation */
	FVector GetForwardVector() const
	{
		return Orientation.Vector();
	}

	/** Calculate distance to a point in meters */
	float GetDistanceMeters(const FVector& Point) const
	{
		return FVector::Dist(WorldPosition, Point) / 100.0f;  // UE uses cm
	}

	/** Calculate propagation delay to a point in milliseconds */
	float GetPropagationDelayMs(const FVector& Point) const
	{
		return GetDistanceMeters(Point) * SpatialAudioConstants::MsPerMeter;
	}

	/** Check if a point is within the speaker's dispersion pattern (simplified) */
	bool IsPointInCoverage(const FVector& Point, float ToleranceDegrees = 10.0f) const
	{
		FVector ToPoint = (Point - WorldPosition).GetSafeNormal();
		FVector Forward = GetForwardVector();
		float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Forward, ToPoint)));
		float HalfDispersion = FMath::Max(NominalDispersionH, NominalDispersionV) / 2.0f;
		return AngleDegrees <= (HalfDispersion + ToleranceDegrees);
	}

	/** Get effective output gain including mute state */
	float GetEffectiveOutputGain() const
	{
		if (DSP.bMuted) return 0.0f;
		return DSP.GetOutputGainLinear();
	}
};

/**
 * Represents a group of speakers forming an array (e.g., line array, sub array).
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialSpeakerArray
{
	GENERATED_BODY()

	// ========================================================================
	// IDENTIFICATION
	// ========================================================================

	/** Unique identifier for this array */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	FGuid Id;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	FString Name;

	/** Color for visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	FLinearColor Color = FLinearColor::White;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** Type of array */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	ESpatialArrayType ArrayType = ESpatialArrayType::LineArray;

	/** Ordered list of speaker IDs in this array */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	TArray<FGuid> SpeakerIds;

	/** Array reference point (typically top of array or center) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	FVector ArrayPosition = FVector::ZeroVector;

	/** Array reference orientation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	FRotator ArrayOrientation = FRotator::ZeroRotator;

	// ========================================================================
	// ARRAY-LEVEL CONTROL
	// ========================================================================

	/** Master gain trim for the entire array in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array", meta = (ClampMin = "-40.0", ClampMax = "12.0"))
	float ArrayGainDb = 0.0f;

	/** Array muted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	bool bMuted = false;

	/** Array soloed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Array")
	bool bSoloed = false;

	// ========================================================================
	// METHODS
	// ========================================================================

	FSpatialSpeakerArray()
	{
		Id = FGuid::NewGuid();
	}

	/** Get linear gain multiplier */
	float GetGainLinear() const
	{
		if (bMuted) return 0.0f;
		return FMath::Pow(10.0f, ArrayGainDb / 20.0f);
	}

	/** Get speaker count */
	int32 GetSpeakerCount() const
	{
		return SpeakerIds.Num();
	}
};
