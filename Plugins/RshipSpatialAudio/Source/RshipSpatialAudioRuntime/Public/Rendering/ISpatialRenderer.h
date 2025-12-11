// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"

/**
 * Interface for spatial audio renderers.
 *
 * A renderer computes speaker gains and delays for audio objects based on
 * their position relative to a speaker configuration. Different algorithms
 * (VBAP, DBAP, HOA, etc.) implement this interface.
 *
 * Renderers are expected to be:
 * - Thread-safe for reading (ComputeGains can be called from audio thread)
 * - Not thread-safe for configuration changes (Configure must be called from game thread)
 * - Stateless per-computation (no object tracking between ComputeGains calls)
 */
class RSHIPSPATIALAUDIORUNTIME_API ISpatialRenderer
{
public:
	virtual ~ISpatialRenderer() = default;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/**
	 * Configure the renderer with a set of speakers.
	 * This may trigger preprocessing (e.g., triangulation for VBAP).
	 * Must be called from game thread before using the renderer.
	 *
	 * @param Speakers Array of speakers to render to.
	 */
	virtual void Configure(const TArray<FSpatialSpeaker>& Speakers) = 0;

	/**
	 * Check if the renderer is properly configured and ready to use.
	 */
	virtual bool IsConfigured() const = 0;

	/**
	 * Get the number of speakers this renderer is configured for.
	 */
	virtual int32 GetSpeakerCount() const = 0;

	// ========================================================================
	// RENDERING
	// ========================================================================

	/**
	 * Compute speaker gains and delays for an object at the given position.
	 * This is the core rendering function called per object per frame.
	 *
	 * Must be thread-safe for reading (can be called from audio thread).
	 *
	 * @param ObjectPosition World position of the audio object.
	 * @param Spread Source spread/width in degrees (0 = point source).
	 * @param OutGains Output array of speaker gains (cleared and populated by this function).
	 */
	virtual void ComputeGains(
		const FVector& ObjectPosition,
		float Spread,
		TArray<FSpatialSpeakerGain>& OutGains) const = 0;

	/**
	 * Compute gains for multiple objects at once (batch processing).
	 * Default implementation calls ComputeGains for each object.
	 * Renderers may override for better cache efficiency.
	 *
	 * @param ObjectPositions Array of object positions.
	 * @param Spreads Array of spread values (same length as positions).
	 * @param OutGainsPerObject Output: array of gain arrays, one per object.
	 */
	virtual void ComputeGainsBatch(
		const TArray<FVector>& ObjectPositions,
		const TArray<float>& Spreads,
		TArray<TArray<FSpatialSpeakerGain>>& OutGainsPerObject) const
	{
		OutGainsPerObject.SetNum(ObjectPositions.Num());
		for (int32 i = 0; i < ObjectPositions.Num(); ++i)
		{
			ComputeGains(ObjectPositions[i], Spreads[i], OutGainsPerObject[i]);
		}
	}

	// ========================================================================
	// METADATA
	// ========================================================================

	/**
	 * Get the renderer type.
	 */
	virtual ESpatialRendererType GetType() const = 0;

	/**
	 * Get a human-readable name for this renderer.
	 */
	virtual FString GetName() const = 0;

	/**
	 * Get a description of this renderer.
	 */
	virtual FString GetDescription() const = 0;

	// ========================================================================
	// DIAGNOSTICS
	// ========================================================================

	/**
	 * Get debug/diagnostic information about the current configuration.
	 */
	virtual FString GetDiagnosticInfo() const = 0;

	/**
	 * Validate the current configuration.
	 * @return Array of error/warning messages (empty if valid).
	 */
	virtual TArray<FString> Validate() const = 0;
};

/**
 * Parameters for configuring a renderer.
 * Renderer-specific parameters can be encoded in the JSON string.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialRendererConfig
{
	GENERATED_BODY()

	/** Renderer type to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Rendering")
	ESpatialRendererType RendererType = ESpatialRendererType::VBAP;

	/** Whether to enable phase-coherent panning (adds delay computation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Rendering")
	bool bPhaseCoherent = true;

	/** Reference distance for phase calculations in centimeters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Rendering", meta = (ClampMin = "0.0"))
	float ReferenceDistanceCm = 0.0f;

	/** Renderer-specific parameters as JSON */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Rendering")
	FString CustomParams;
};
