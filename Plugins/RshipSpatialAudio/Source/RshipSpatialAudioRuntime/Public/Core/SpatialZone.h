// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialAudioTypes.h"
#include "SpatialDSPTypes.h"
#include "SpatialZone.generated.h"

/**
 * Represents a rendering zone - a region with a specific renderer and speaker set.
 * Objects routed to a zone are rendered using that zone's renderer to its speakers.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialZone
{
	GENERATED_BODY()

	// ========================================================================
	// IDENTIFICATION
	// ========================================================================

	/** Unique identifier for this zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	FGuid Id;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	FString Name;

	/** Color for visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	FLinearColor Color = FLinearColor::Blue;

	// ========================================================================
	// SPEAKER MEMBERSHIP
	// ========================================================================

	/** Arrays belonging to this zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	TArray<FGuid> ArrayIds;

	/** Individual speakers in this zone (not part of arrays) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	TArray<FGuid> SpeakerIds;

	// ========================================================================
	// RENDERING
	// ========================================================================

	/** Renderer type for this zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	ESpatialRendererType RendererType = ESpatialRendererType::VBAP;

	/** Renderer-specific parameters (JSON for flexibility) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	FString RendererParams;

	// ========================================================================
	// SPATIAL BOUNDS
	// ========================================================================

	/** Bounding box for this zone (for visualization and object containment) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	FBox BoundingBox = FBox(FVector(-1000, -1000, -1000), FVector(1000, 1000, 1000));

	/** Whether objects must be inside bounds to be rendered by this zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	bool bEnforceBounds = false;

	// ========================================================================
	// ZONE-LEVEL CONTROL
	// ========================================================================

	/** Zone master gain in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone", meta = (ClampMin = "-80.0", ClampMax = "12.0"))
	float ZoneGainDb = 0.0f;

	/** Zone muted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	bool bMuted = false;

	/** Zone soloed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Zone")
	bool bSoloed = false;

	// ========================================================================
	// METHODS
	// ========================================================================

	FSpatialZone()
	{
		Id = FGuid::NewGuid();
	}

	/** Get linear gain multiplier */
	float GetGainLinear() const
	{
		if (bMuted) return 0.0f;
		return FMath::Pow(10.0f, ZoneGainDb / 20.0f);
	}

	/** Get total speaker count (arrays + individual) */
	int32 GetTotalSpeakerCount() const
	{
		// Note: This doesn't expand arrays - caller should do that with venue data
		return SpeakerIds.Num();
	}

	/** Check if a point is within the zone bounds */
	bool ContainsPoint(const FVector& Point) const
	{
		if (!bEnforceBounds) return true;
		return BoundingBox.IsInside(Point);
	}
};
