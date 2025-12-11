// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialAudioProcessor.h"
#include "SpatialOutputRouter.h"
#include "Rendering/ISpatialRenderer.h"
#include "Rendering/SpatialRendererRegistry.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"
#include "Core/SpatialAudioObject.h"

/**
 * Spatial Rendering Engine.
 *
 * The central coordinator for the spatial audio pipeline:
 * 1. Maintains speaker configuration
 * 2. Selects and configures renderers (VBAP, DBAP, etc.)
 * 3. Computes gains for audio objects
 * 4. Sends gains to audio thread via processor
 * 5. Handles output routing
 *
 * Usage:
 *   - Configure speakers once at startup
 *   - Call UpdateObject() each frame for moving objects
 *   - Audio thread receives gains automatically via lock-free queue
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialRenderingEngine
{
public:
	FSpatialRenderingEngine();
	~FSpatialRenderingEngine();

	// Non-copyable
	FSpatialRenderingEngine(const FSpatialRenderingEngine&) = delete;
	FSpatialRenderingEngine& operator=(const FSpatialRenderingEngine&) = delete;

	// ========================================================================
	// INITIALIZATION
	// ========================================================================

	/**
	 * Initialize the rendering engine.
	 * @param SampleRate Audio sample rate.
	 * @param BufferSize Audio buffer size.
	 * @param OutputChannelCount Number of output channels.
	 */
	void Initialize(float SampleRate, int32 BufferSize, int32 OutputChannelCount);

	/**
	 * Shutdown the rendering engine.
	 */
	void Shutdown();

	/**
	 * Check if engine is initialized.
	 */
	bool IsInitialized() const { return bIsInitialized; }

	// ========================================================================
	// SPEAKER CONFIGURATION
	// ========================================================================

	/**
	 * Configure speakers for rendering.
	 * This triggers renderer reconfiguration (triangulation, etc.).
	 *
	 * @param Speakers Array of speaker definitions.
	 * @param RendererType Which renderer algorithm to use.
	 */
	void ConfigureSpeakers(
		const TArray<FSpatialSpeaker>& Speakers,
		ESpatialRendererType RendererType = ESpatialRendererType::VBAP);

	/**
	 * Get the current speaker configuration.
	 */
	const TArray<FSpatialSpeaker>& GetSpeakers() const { return CachedSpeakers; }

	/**
	 * Get the current renderer type.
	 */
	ESpatialRendererType GetRendererType() const { return CurrentRendererType; }

	/**
	 * Set the reference point for phase-coherent rendering.
	 * Typically the listener position.
	 */
	void SetReferencePoint(const FVector& Point);

	/**
	 * Get the current reference point.
	 */
	FVector GetReferencePoint() const { return ReferencePoint; }

	/**
	 * Set whether to use 2D (horizontal only) or 3D rendering.
	 */
	void SetUse2DMode(bool bIn2D);

	// ========================================================================
	// OBJECT RENDERING
	// ========================================================================

	/**
	 * Update an audio object's position and compute new gains.
	 * This sends the computed gains to the audio thread.
	 *
	 * @param Object The audio object with current position/spread.
	 */
	void UpdateObject(const FSpatialAudioObject& Object);

	/**
	 * Update multiple objects in batch.
	 * More efficient than calling UpdateObject repeatedly.
	 */
	void UpdateObjectsBatch(const TArray<FSpatialAudioObject>& Objects);

	/**
	 * Remove an object from rendering.
	 */
	void RemoveObject(const FGuid& ObjectId);

	/**
	 * Compute gains for a position without sending to audio thread.
	 * Useful for preview/visualization.
	 *
	 * @param Position World position.
	 * @param Spread Source spread in degrees.
	 * @param OutGains Output gains per speaker.
	 */
	void ComputeGains(const FVector& Position, float Spread, TArray<FSpatialSpeakerGain>& OutGains);

	// ========================================================================
	// SPEAKER DSP
	// ========================================================================

	/**
	 * Update speaker DSP settings (sent to audio thread).
	 */
	void SetSpeakerDSP(int32 SpeakerIndex, float GainDb, float DelayMs, bool bMuted);

	/**
	 * Set master gain (in dB).
	 */
	void SetMasterGain(float GainDb);

	// ========================================================================
	// OUTPUT ROUTING
	// ========================================================================

	/**
	 * Get the output router for configuration.
	 */
	FSpatialOutputRouter& GetOutputRouter() { return OutputRouter; }
	const FSpatialOutputRouter& GetOutputRouter() const { return OutputRouter; }

	// ========================================================================
	// METERING
	// ========================================================================

	/**
	 * Process meter feedback from audio thread.
	 * Call periodically from game thread (e.g., in Tick).
	 *
	 * @param OutMeterReadings Output: meter readings per speaker.
	 */
	void ProcessMeterFeedback(TMap<int32, FSpatialMeterReading>& OutMeterReadings);

	// ========================================================================
	// ACCESSORS
	// ========================================================================

	/**
	 * Get the audio processor (for direct access if needed).
	 */
	FSpatialAudioProcessor* GetProcessor() { return Processor.Get(); }

	/**
	 * Get the current renderer (for diagnostics).
	 */
	ISpatialRenderer* GetRenderer() { return CurrentRenderer; }

	/**
	 * Get diagnostic info as string.
	 */
	FString GetDiagnosticInfo() const;

private:
	// ========================================================================
	// STATE
	// ========================================================================

	/** Is engine initialized */
	bool bIsInitialized;

	/** Cached sample rate */
	float CachedSampleRate;

	/** Cached speaker configuration */
	TArray<FSpatialSpeaker> CachedSpeakers;

	/** Speaker ID to index map */
	TMap<FGuid, int32> SpeakerIdToIndex;

	/** Current renderer type */
	ESpatialRendererType CurrentRendererType;

	/** Current renderer (owned by registry) */
	ISpatialRenderer* CurrentRenderer;

	/** Reference point for phase calculations */
	FVector ReferencePoint;

	/** Use 2D mode */
	bool bUse2DMode;

	/** Renderer registry */
	FSpatialRendererRegistry RendererRegistry;

	/** Audio processor */
	TUniquePtr<FSpatialAudioProcessor> Processor;

	/** Output router */
	FSpatialOutputRouter OutputRouter;

	// ========================================================================
	// INTERNAL METHODS
	// ========================================================================

	/** Reconfigure renderer with current settings */
	void ReconfigureRenderer();

	/** Convert dB to linear gain */
	float DbToLinear(float Db) const
	{
		return FMath::Pow(10.0f, Db / 20.0f);
	}

	/** Convert linear gain to dB */
	float LinearToDb(float Linear) const
	{
		return 20.0f * FMath::LogX(10.0f, FMath::Max(Linear, 0.0001f));
	}
};

/**
 * Global accessor for the rendering engine.
 * Typically owned by SpatialAudioManager but can be accessed directly.
 */
RSHIPSPATIALAUDIORUNTIME_API FSpatialRenderingEngine* GetGlobalSpatialRenderingEngine();
