// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"
#include "Core/SpatialZone.h"
#include "Core/SpatialAudioObject.h"
#include "Rendering/ISpatialRenderer.h"
#include "Rendering/SpatialRendererRegistry.h"
#include "SpatialZoneManager.generated.h"

/**
 * Runtime state for a zone.
 */
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialZoneState
{
	/** Zone configuration */
	FSpatialZone Zone;

	/** Speakers in this zone (cached for fast access) */
	TArray<FSpatialSpeaker> Speakers;

	/** Speaker IDs in this zone */
	TSet<FGuid> SpeakerIds;

	/** Renderer for this zone (owned by registry) */
	ISpatialRenderer* Renderer = nullptr;

	/** Objects currently in this zone */
	TSet<FGuid> ObjectIds;

	/** Is zone active (has objects) */
	bool bIsActive = false;

	/** Zone bounds for containment testing */
	FBox Bounds;
};

/**
 * Zone Manager - manages spatial rendering zones.
 *
 * A zone is a region of the venue with its own speaker subset and renderer.
 * Objects can be routed to specific zones or auto-assigned based on position.
 *
 * Features:
 * - Multiple zones with different renderer types (VBAP, DBAP, etc.)
 * - Automatic zone assignment based on object position
 * - Manual zone routing override
 * - Zone blending at boundaries
 * - Per-zone speaker subsets
 *
 * Typical configurations:
 * - Main array (VBAP) + surround (DBAP) + subwoofers (Direct)
 * - Stage (VBAP) + audience (DBAP) + effects (HOA)
 * - Multiple overlapping zones with different characteristics
 */
UCLASS(BlueprintType)
class RSHIPSPATIALAUDIORUNTIME_API USpatialZoneManager : public UObject
{
	GENERATED_BODY()

public:
	USpatialZoneManager();

	// ========================================================================
	// INITIALIZATION
	// ========================================================================

	/**
	 * Initialize the zone manager with speaker configuration.
	 */
	void Initialize(const TArray<FSpatialSpeaker>& AllSpeakers);

	/**
	 * Shutdown and cleanup.
	 */
	void Shutdown();

	/**
	 * Check if manager is initialized.
	 */
	bool IsInitialized() const { return bIsInitialized; }

	// ========================================================================
	// ZONE MANAGEMENT
	// ========================================================================

	/**
	 * Add a zone.
	 * @return Zone ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Zones")
	FGuid AddZone(const FSpatialZone& Zone);

	/**
	 * Update zone configuration.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Zones")
	bool UpdateZone(const FGuid& ZoneId, const FSpatialZone& Zone);

	/**
	 * Remove a zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Zones")
	bool RemoveZone(const FGuid& ZoneId);

	/**
	 * Get zone by ID.
	 */
	bool GetZone(const FGuid& ZoneId, FSpatialZone& OutZone) const;

	/**
	 * Get all zones.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Zones")
	TArray<FSpatialZone> GetAllZones() const;

	/**
	 * Get zone count.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpatialAudio|Zones")
	int32 GetZoneCount() const { return ZoneStates.Num(); }

	/**
	 * Set renderer type for a zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Zones")
	void SetZoneRenderer(const FGuid& ZoneId, ESpatialRendererType RendererType);

	/**
	 * Assign speakers to a zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Zones")
	void SetZoneSpeakers(const FGuid& ZoneId, const TArray<FGuid>& SpeakerIds);

	/**
	 * Add speaker to a zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Zones")
	void AddSpeakerToZone(const FGuid& ZoneId, const FGuid& SpeakerId);

	/**
	 * Remove speaker from a zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Zones")
	void RemoveSpeakerFromZone(const FGuid& ZoneId, const FGuid& SpeakerId);

	// ========================================================================
	// OBJECT ROUTING
	// ========================================================================

	/**
	 * Get zones for an object based on its routing configuration.
	 * Returns zones in priority order.
	 */
	TArray<FGuid> GetZonesForObject(const FSpatialAudioObject& Object) const;

	/**
	 * Find zone containing a position (for auto-routing).
	 * Returns invalid GUID if no zone contains the position.
	 */
	FGuid FindZoneContainingPosition(const FVector& Position) const;

	/**
	 * Find all zones overlapping a position.
	 */
	TArray<FGuid> FindZonesOverlappingPosition(const FVector& Position) const;

	/**
	 * Set manual zone routing for an object.
	 */
	void SetObjectZoneRouting(const FGuid& ObjectId, const TArray<FGuid>& ZoneIds);

	/**
	 * Clear manual zone routing (use auto-routing).
	 */
	void ClearObjectZoneRouting(const FGuid& ObjectId);

	// ========================================================================
	// RENDERING
	// ========================================================================

	/**
	 * Compute gains for an object across all its target zones.
	 *
	 * @param Object The audio object.
	 * @param OutGains Output gains (combined from all zones).
	 */
	void ComputeGainsForObject(
		const FSpatialAudioObject& Object,
		TArray<FSpatialSpeakerGain>& OutGains);

	/**
	 * Compute gains for an object in a specific zone.
	 */
	void ComputeGainsInZone(
		const FGuid& ZoneId,
		const FVector& Position,
		float Spread,
		TArray<FSpatialSpeakerGain>& OutGains);

	/**
	 * Get the renderer for a zone.
	 */
	ISpatialRenderer* GetZoneRenderer(const FGuid& ZoneId);

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/**
	 * Set the reference point for all zone renderers.
	 */
	void SetGlobalReferencePoint(const FVector& Point);

	/**
	 * Enable/disable zone boundary blending.
	 */
	void SetBoundaryBlending(bool bEnabled, float BlendDistance = 100.0f);

	/**
	 * Get zone diagnostic info.
	 */
	FString GetDiagnosticInfo() const;

private:
	// ========================================================================
	// STATE
	// ========================================================================

	/** Is manager initialized */
	bool bIsInitialized;

	/** All speakers in the venue */
	TArray<FSpatialSpeaker> AllSpeakers;

	/** Speaker ID to index map */
	TMap<FGuid, int32> SpeakerIdToIndex;

	/** Zone states keyed by zone ID */
	TMap<FGuid, FSpatialZoneState> ZoneStates;

	/** Manual object zone routing */
	TMap<FGuid, TArray<FGuid>> ObjectZoneRouting;

	/** Renderer registry */
	FSpatialRendererRegistry RendererRegistry;

	/** Global reference point */
	FVector GlobalReferencePoint;

	/** Enable boundary blending */
	bool bBoundaryBlending;

	/** Blend distance at zone boundaries */
	float BoundaryBlendDistance;

	// ========================================================================
	// INTERNAL METHODS
	// ========================================================================

	/** Reconfigure renderer for a zone */
	void ReconfigureZoneRenderer(FSpatialZoneState& State);

	/** Rebuild zone speakers from speaker IDs */
	void RebuildZoneSpeakers(FSpatialZoneState& State);

	/** Get speaker by ID */
	const FSpatialSpeaker* GetSpeakerById(const FGuid& SpeakerId) const;

	/** Compute blend weight for a position relative to a zone */
	float ComputeZoneBlendWeight(const FSpatialZoneState& State, const FVector& Position) const;

	/** Merge gains from multiple zones */
	void MergeGains(
		TArray<FSpatialSpeakerGain>& OutGains,
		const TArray<FSpatialSpeakerGain>& NewGains,
		float Weight);
};
