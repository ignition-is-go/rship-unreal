// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialRenderer.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"

/**
 * Factory and registry for spatial audio renderers.
 *
 * Manages renderer instances and provides factory methods for creating
 * renderers by type. Supports caching configured renderers for reuse.
 *
 * Usage:
 *   FSpatialRendererRegistry Registry;
 *   ISpatialRenderer* Renderer = Registry.GetOrCreateRenderer(ESpatialRendererType::VBAP, SpeakerArray);
 *   Renderer->ComputeGains(Position, Spread, OutGains);
 *
 * Thread Safety:
 * - GetOrCreateRenderer is NOT thread-safe (call from game thread)
 * - Returned renderers can be used from audio thread (read-only)
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialRendererRegistry
{
public:
	FSpatialRendererRegistry();
	~FSpatialRendererRegistry();

	// Non-copyable
	FSpatialRendererRegistry(const FSpatialRendererRegistry&) = delete;
	FSpatialRendererRegistry& operator=(const FSpatialRendererRegistry&) = delete;

	// ========================================================================
	// FACTORY METHODS
	// ========================================================================

	/**
	 * Create a new renderer of the specified type.
	 * The caller owns the returned pointer and is responsible for deletion.
	 *
	 * @param Type The renderer algorithm type.
	 * @return New renderer instance, or nullptr if type is unsupported.
	 */
	static TUniquePtr<ISpatialRenderer> CreateRenderer(ESpatialRendererType Type);

	/**
	 * Create and configure a renderer with the given speakers.
	 * Convenience method combining Create + Configure.
	 *
	 * @param Type The renderer algorithm type.
	 * @param Speakers Speaker configuration.
	 * @return Configured renderer, or nullptr if creation/configuration failed.
	 */
	static TUniquePtr<ISpatialRenderer> CreateConfiguredRenderer(
		ESpatialRendererType Type,
		const TArray<FSpatialSpeaker>& Speakers);

	// ========================================================================
	// CACHED RENDERERS
	// ========================================================================

	/**
	 * Get or create a cached renderer of the specified type.
	 * If a renderer of this type already exists and is configured with
	 * compatible speakers, returns the cached instance. Otherwise creates
	 * and configures a new one.
	 *
	 * The registry owns the returned pointer - do not delete.
	 *
	 * @param Type The renderer algorithm type.
	 * @param Speakers Speaker configuration.
	 * @param Config Optional renderer-specific configuration.
	 * @return Cached renderer pointer (owned by registry).
	 */
	ISpatialRenderer* GetOrCreateRenderer(
		ESpatialRendererType Type,
		const TArray<FSpatialSpeaker>& Speakers,
		const FSpatialRendererConfig& Config = FSpatialRendererConfig());

	/**
	 * Get a cached renderer without creating.
	 * Returns nullptr if no renderer of this type is cached.
	 */
	ISpatialRenderer* GetCachedRenderer(ESpatialRendererType Type) const;

	/**
	 * Invalidate all cached renderers, forcing reconfiguration on next use.
	 */
	void InvalidateCache();

	/**
	 * Invalidate a specific renderer type.
	 */
	void InvalidateRenderer(ESpatialRendererType Type);

	/**
	 * Check if a renderer type is cached and configured.
	 */
	bool IsRendererCached(ESpatialRendererType Type) const;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/**
	 * Configure a specific renderer type with VBAP-specific settings.
	 * Call before GetOrCreateRenderer to set 2D/3D mode, reference point, etc.
	 *
	 * @param bUse2D True for horizontal-only (2D) panning.
	 * @param ReferencePoint Reference point for phase calculations.
	 * @param bPhaseCoherent Enable/disable phase coherent delays.
	 */
	void SetVBAPConfig(bool bUse2D, const FVector& ReferencePoint, bool bPhaseCoherent);

	/**
	 * Configure DBAP-specific settings (for future use).
	 *
	 * @param RolloffExponent Distance rolloff exponent.
	 * @param ReferenceDistance Reference distance for gain calculation.
	 */
	void SetDBAPConfig(float RolloffExponent, float ReferenceDistance);

	/**
	 * Configure HOA-specific settings.
	 *
	 * @param Order Ambisonics order (1-5).
	 * @param DecoderType Decoding algorithm (0=Basic, 1=MaxRE, 2=InPhase, 3=AllRAD, 4=EPAD).
	 * @param ListenerPosition Reference/listener position.
	 */
	void SetHOAConfig(int32 Order, int32 DecoderType, const FVector& ListenerPosition);

	// ========================================================================
	// TYPE INFORMATION
	// ========================================================================

	/**
	 * Get human-readable name for a renderer type.
	 */
	static FString GetRendererTypeName(ESpatialRendererType Type);

	/**
	 * Get description for a renderer type.
	 */
	static FString GetRendererTypeDescription(ESpatialRendererType Type);

	/**
	 * Check if a renderer type is supported/implemented.
	 */
	static bool IsRendererTypeSupported(ESpatialRendererType Type);

	/**
	 * Get list of all supported renderer types.
	 */
	static TArray<ESpatialRendererType> GetSupportedRendererTypes();

private:
	// ========================================================================
	// INTERNAL STATE
	// ========================================================================

	/** Cached renderer instances by type */
	TMap<ESpatialRendererType, TUniquePtr<ISpatialRenderer>> CachedRenderers;

	/** Last speaker configuration hash for cache validation */
	TMap<ESpatialRendererType, uint32> ConfigurationHashes;

	// VBAP configuration
	bool VBAPUse2D;
	FVector VBAPReferencePoint;
	bool VBAPPhaseCoherent;

	// DBAP configuration
	float DBAPRolloffExponent;
	float DBAPReferenceDistance;

	// HOA configuration
	int32 HOAOrder;
	int32 HOADecoderType;
	FVector HOAListenerPosition;

	/**
	 * Compute a hash of the speaker configuration for cache validation.
	 */
	static uint32 ComputeSpeakerHash(const TArray<FSpatialSpeaker>& Speakers);

	/**
	 * Apply type-specific configuration to a renderer.
	 */
	void ApplyConfiguration(ISpatialRenderer* Renderer, ESpatialRendererType Type);
};

/**
 * Global accessor for the default renderer registry.
 * Thread-safe for reading after initial setup.
 */
RSHIPSPATIALAUDIORUNTIME_API FSpatialRendererRegistry& GetGlobalRendererRegistry();
