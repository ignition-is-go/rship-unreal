// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialRenderer.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"

/**
 * Distance-Based Amplitude Panning (DBAP) renderer.
 *
 * DBAP computes speaker gains using inverse distance weighting.
 * All speakers receive signal, with gain falling off with distance.
 * This creates a more diffuse, enveloping sound compared to VBAP.
 *
 * Key characteristics:
 * - All speakers contribute (no triangulation)
 * - Natural distance rolloff
 * - Good for ambient/immersive content
 * - Less precise localization than VBAP
 *
 * The gain for speaker i is computed as:
 *   g_i = (1 / d_i^a) / sum(1 / d_j^a for all j)
 *
 * Where:
 *   d_i = distance from source to speaker i
 *   a = rolloff exponent (typically 2.0 for inverse square)
 *
 * Phase Coherence:
 * - Like VBAP, we compute delays for phase alignment
 * - Delays are relative to the reference point
 *
 * Thread Safety:
 * - Configure() must be called from game thread
 * - ComputeGains() is thread-safe (audio thread safe)
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialRendererDBAP : public ISpatialRenderer
{
public:
	FSpatialRendererDBAP();
	virtual ~FSpatialRendererDBAP() = default;

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

	virtual ESpatialRendererType GetType() const override { return ESpatialRendererType::DBAP; }
	virtual FString GetName() const override { return TEXT("DBAP"); }
	virtual FString GetDescription() const override;
	virtual FString GetDiagnosticInfo() const override;
	virtual TArray<FString> Validate() const override;

	// ========================================================================
	// DBAP-Specific Configuration
	// ========================================================================

	/**
	 * Set the rolloff exponent (default 2.0 = inverse square).
	 * Higher values create more focused sound, lower values more diffuse.
	 */
	void SetRolloffExponent(float Exponent) { RolloffExponent = FMath::Max(0.1f, Exponent); }
	float GetRolloffExponent() const { return RolloffExponent; }

	/**
	 * Set the reference distance for gain calculation.
	 * Distances below this are clamped to avoid infinite gain.
	 */
	void SetReferenceDistance(float Distance) { ReferenceDistance = FMath::Max(1.0f, Distance); }
	float GetReferenceDistance() const { return ReferenceDistance; }

	/**
	 * Set the reference point for phase-coherent panning.
	 */
	void SetReferencePoint(const FVector& Point) { ReferencePoint = Point; }
	FVector GetReferencePoint() const { return ReferencePoint; }

	/**
	 * Enable/disable phase-coherent delay computation.
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
	 * Set maximum number of active speakers.
	 * Limits computation by only using N nearest speakers.
	 * 0 = no limit (use all speakers).
	 */
	void SetMaxActiveSpeakers(int32 Max) { MaxActiveSpeakers = FMath::Max(0, Max); }
	int32 GetMaxActiveSpeakers() const { return MaxActiveSpeakers; }

	/**
	 * Set spatial blur amount (0-1).
	 * Higher values spread energy more evenly across speakers.
	 */
	void SetSpatialBlur(float Blur) { SpatialBlur = FMath::Clamp(Blur, 0.0f, 1.0f); }
	float GetSpatialBlur() const { return SpatialBlur; }

private:
	// ========================================================================
	// Internal State
	// ========================================================================

	/** Cached speaker data */
	TArray<FSpatialSpeaker> CachedSpeakers;

	/** Speaker positions (for fast access) */
	TArray<FVector> SpeakerPositions;

	/** Is renderer configured? */
	bool bIsConfigured;

	/** Rolloff exponent (typically 2.0) */
	float RolloffExponent;

	/** Reference distance for gain calculation (cm) */
	float ReferenceDistance;

	/** Reference point for delay calculations */
	FVector ReferencePoint;

	/** Enable phase-coherent delays */
	bool bPhaseCoherent;

	/** Minimum gain threshold */
	float MinGainThreshold;

	/** Maximum number of active speakers (0 = all) */
	int32 MaxActiveSpeakers;

	/** Spatial blur amount */
	float SpatialBlur;

	// ========================================================================
	// Internal Methods
	// ========================================================================

	/**
	 * Compute raw (unnormalized) gains based on distance.
	 */
	void ComputeRawGains(
		const FVector& SourcePosition,
		TArray<float>& OutGains,
		TArray<float>& OutDistances) const;

	/**
	 * Apply spread to gains (increases contribution of distant speakers).
	 */
	void ApplySpread(TArray<float>& Gains, float Spread) const;

	/**
	 * Apply spatial blur to gains.
	 */
	void ApplyBlur(TArray<float>& Gains) const;

	/**
	 * Compute delay for a speaker given source position.
	 */
	float ComputeSpeakerDelay(int32 SpeakerIndex, const FVector& SourcePosition) const;

	/**
	 * Normalize gains to maintain constant power.
	 */
	void NormalizeGains(TArray<float>& Gains) const;
};
