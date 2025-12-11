// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/SpatialAudioTypes.h"
#include "SpatialAudioEngineComponent.generated.h"

class FSpatialRenderingEngine;
class URshipSpatialAudioManager;
class FSpatialAudioSubmixEffect;

/**
 * Spatial Audio Engine Component.
 *
 * This component owns the spatial rendering engine and connects it to:
 * - The SpatialAudioManager (for configuration and control)
 * - The SpatialAudioSubmixEffect (for audio processing)
 *
 * Place this component on an actor in your level (typically a manager actor)
 * to enable spatial audio processing.
 *
 * Usage:
 * 1. Add this component to an actor
 * 2. Configure OutputChannelCount to match your hardware
 * 3. Apply a SpatialAudioSubmixEffect to the appropriate submix
 * 4. Configure speakers via SpatialAudioManager
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent, DisplayName = "Spatial Audio Engine"))
class RSHIPSPATIALAUDIORUNTIME_API USpatialAudioEngineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialAudioEngineComponent();
	virtual ~USpatialAudioEngineComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** Number of output channels (should match your audio hardware) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Configuration", meta = (ClampMin = "2", ClampMax = "256"))
	int32 OutputChannelCount = 64;

	/** Sample rate (typically 48000 or 44100) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Configuration", meta = (ClampMin = "8000", ClampMax = "192000"))
	float SampleRate = 48000.0f;

	/** Buffer size in samples */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Configuration", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 BufferSize = 512;

	/** Default renderer type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Configuration")
	ESpatialRendererType DefaultRendererType = ESpatialRendererType::VBAP;

	/** Automatically connect to SpatialAudioManager */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Configuration")
	bool bAutoConnectToManager = true;

	/** Use 2D (horizontal only) mode for rendering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Configuration")
	bool bUse2DMode = false;

	// ========================================================================
	// RUNTIME API
	// ========================================================================

	/**
	 * Initialize the spatial audio engine.
	 * Called automatically in BeginPlay if not already initialized.
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void InitializeEngine();

	/**
	 * Shutdown the spatial audio engine.
	 * Called automatically in EndPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void ShutdownEngine();

	/**
	 * Check if engine is initialized and ready.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Audio")
	bool IsEngineReady() const { return bIsInitialized; }

	/**
	 * Set the renderer type (VBAP, DBAP, etc.).
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void SetRendererType(ESpatialRendererType RendererType);

	/**
	 * Set the listener/reference position.
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void SetListenerPosition(FVector Position);

	/**
	 * Set the master gain in dB.
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void SetMasterGain(float GainDb);

	/**
	 * Get diagnostic info as string.
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Diagnostics")
	FString GetDiagnosticInfo() const;

	/**
	 * Get the rendering engine (for advanced usage).
	 */
	FSpatialRenderingEngine* GetRenderingEngine() const { return RenderingEngine.Get(); }

protected:
	/** The rendering engine instance */
	TUniquePtr<FSpatialRenderingEngine> RenderingEngine;

	/** Is engine initialized */
	bool bIsInitialized;

	/** Connected manager reference */
	UPROPERTY()
	TWeakObjectPtr<URshipSpatialAudioManager> ConnectedManager;

	/** Connect to the active submix effect */
	void ConnectToSubmixEffect();

	/** Disconnect from submix effect */
	void DisconnectFromSubmixEffect();

	/** Connect to the spatial audio manager */
	void ConnectToManager();

	/** Disconnect from manager */
	void DisconnectFromManager();
};
