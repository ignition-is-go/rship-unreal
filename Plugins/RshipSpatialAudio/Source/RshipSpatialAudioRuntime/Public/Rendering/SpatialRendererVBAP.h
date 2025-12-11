// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialRenderer.h"
#include "SpatialTriangulation.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"

/**
 * Vector Base Amplitude Panning (VBAP) renderer with phase coherence.
 *
 * VBAP pans audio to 2 or 3 speakers that form a triangle/tetrahedron
 * containing the source direction. This implementation extends standard
 * VBAP with:
 *
 * 1. Phase-coherent panning: Computes delay per speaker to maintain
 *    wavefront coherence at a reference point
 * 2. Spread control: Distributes energy across multiple speaker sets
 *    for sources with non-zero width
 * 3. 2D and 3D modes: 2D for horizontal-only arrays, 3D for full spatial
 *
 * Thread Safety:
 * - Configure() must be called from game thread
 * - ComputeGains() is thread-safe for concurrent calls (audio thread safe)
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialRendererVBAP : public ISpatialRenderer
{
public:
	FSpatialRendererVBAP();
	virtual ~FSpatialRendererVBAP() = default;

	// ========================================================================
	// ISpatialRenderer Interface
	// ========================================================================

	virtual void Configure(const TArray<FSpatialSpeaker>& Speakers) override;
	virtual bool IsConfigured() const override;
	virtual int32 GetSpeakerCount() const override;

	virtual void ComputeGains(
		const FVector& ObjectPosition,
		float Spread,
		TArray<FSpatialSpeakerGain>& OutGains) const override;

	virtual void ComputeGainsBatch(
		const TArray<FVector>& ObjectPositions,
		const TArray<float>& Spreads,
		TArray<TArray<FSpatialSpeakerGain>>& OutGainsPerObject) const override;

	virtual ESpatialRendererType GetType() const override { return ESpatialRendererType::VBAP; }
	virtual FString GetName() const override { return TEXT("VBAP"); }
	virtual FString GetDescription() const override;
	virtual FString GetDiagnosticInfo() const override;
	virtual TArray<FString> Validate() const override;

	// ========================================================================
	// VBAP-Specific Configuration
	// ========================================================================

	/**
	 * Set whether to use 2D (horizontal only) or 3D triangulation.
	 * Must call Configure() after changing this.
	 */
	void SetUse2DMode(bool bIn2D) { bUse2DMode = bIn2D; }
	bool GetUse2DMode() const { return bUse2DMode; }

	/**
	 * Set the reference point for phase-coherent panning.
	 * Delays are computed relative to this point.
	 * Default is origin (0,0,0).
	 */
	void SetReferencePoint(const FVector& Point) { ReferencePoint = Point; }
	FVector GetReferencePoint() const { return ReferencePoint; }

	/**
	 * Enable/disable phase-coherent delay computation.
	 * When disabled, only gains are computed (faster but less accurate).
	 */
	void SetPhaseCoherent(bool bEnabled) { bPhaseCoherent = bEnabled; }
	bool GetPhaseCoherent() const { return bPhaseCoherent; }

	/**
	 * Set minimum gain threshold.
	 * Speakers with gains below this are excluded from output.
	 */
	void SetMinGainThreshold(float Threshold) { MinGainThreshold = FMath::Clamp(Threshold, 0.0f, 1.0f); }
	float GetMinGainThreshold() const { return MinGainThreshold; }

	/**
	 * Set spread factor scaling.
	 * Higher values spread energy to more speakers for given spread angle.
	 */
	void SetSpreadFactor(float Factor) { SpreadFactor = FMath::Max(0.1f, Factor); }
	float GetSpreadFactor() const { return SpreadFactor; }

	// ========================================================================
	// Diagnostics
	// ========================================================================

	/** Get the number of triangles (2D) or tetrahedra (3D) in the mesh */
	int32 GetMeshElementCount() const;

	/** Get the centroid of the speaker configuration */
	FVector GetSpeakerCentroid() const { return SpeakerCentroid; }

private:
	// ========================================================================
	// Internal State
	// ========================================================================

	/** Cached speaker data */
	TArray<FSpatialSpeaker> CachedSpeakers;

	/** Speaker positions in normalized direction form (unit vectors from origin) */
	TArray<FVector> SpeakerDirections;

	/** Speaker distances from origin (for delay calculation) */
	TArray<float> SpeakerDistances;

	/** 2D triangulation (used when bUse2DMode is true) */
	FSpatialDelaunay2D Triangulation2D;

	/** 3D triangulation (used when bUse2DMode is false) */
	FSpatialDelaunay3D Triangulation3D;

	/** Is renderer configured and ready? */
	bool bIsConfigured;

	/** Use 2D (horizontal) or 3D triangulation */
	bool bUse2DMode;

	/** Enable phase-coherent delay computation */
	bool bPhaseCoherent;

	/** Reference point for delay calculations */
	FVector ReferencePoint;

	/** Centroid of speaker positions */
	FVector SpeakerCentroid;

	/** Minimum gain to include in output */
	float MinGainThreshold;

	/** Spread energy distribution factor */
	float SpreadFactor;

	// ========================================================================
	// Internal Methods
	// ========================================================================

	/**
	 * Compute gains for a point source (spread = 0) in 2D mode.
	 */
	void ComputePointGains2D(
		const FVector& Direction,
		float Distance,
		TArray<FSpatialSpeakerGain>& OutGains) const;

	/**
	 * Compute gains for a point source (spread = 0) in 3D mode.
	 */
	void ComputePointGains3D(
		const FVector& Direction,
		float Distance,
		TArray<FSpatialSpeakerGain>& OutGains) const;

	/**
	 * Compute gains with spread (energy distributed across multiple speakers).
	 */
	void ComputeSpreadGains(
		const FVector& Direction,
		float Distance,
		float Spread,
		TArray<FSpatialSpeakerGain>& OutGains) const;

	/**
	 * Compute delay in milliseconds for a speaker given source position.
	 * Uses the reference point and speed of sound.
	 */
	float ComputeSpeakerDelay(
		int32 SpeakerIndex,
		const FVector& SourcePosition) const;

	/**
	 * Normalize gains to maintain constant power.
	 */
	void NormalizeGains(TArray<FSpatialSpeakerGain>& Gains) const;

	/**
	 * Apply minimum threshold and remove zero-gain entries.
	 */
	void ApplyThreshold(TArray<FSpatialSpeakerGain>& Gains) const;
};
