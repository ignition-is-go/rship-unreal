// Copyright Lucid. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RshipNDIStreamTypes.h"
#include "RshipNDIStreamComponent.generated.h"

// Forward declarations
class ACineCameraActor;
class UCineCameraComponent;
class UCineCaptureComponent2D;
class FNDIStreamRenderer;

/**
 * Component that streams a CineCamera's exact output via NDI.
 *
 * Attach this component to an ACineCameraActor to stream its rendered output
 * to NDI receivers on the network. Uses UCineCaptureComponent2D for exact
 * visual match with the CineCamera's viewport.
 *
 * Features:
 * - 8K @ 60fps RGBA streaming
 * - Exact CineCamera render match (DOF, filmback, lens effects)
 * - Triple-buffered async GPU readback for minimal latency
 * - Multiple simultaneous streams supported
 * - Works even when camera is not in viewport
 *
 * Example usage:
 * @code
 * // In Blueprint or C++:
 * ACineCameraActor* Camera = GetWorld()->SpawnActor<ACineCameraActor>();
 * URshipNDIStreamComponent* NDI = Camera->AddComponentByClass(
 *     URshipNDIStreamComponent::StaticClass(), false, FTransform::Identity, false);
 * NDI->Config.StreamName = TEXT("My Camera");
 * NDI->StartStreaming();
 * @endcode
 */
UCLASS(ClassGroup = "Rship", meta = (BlueprintSpawnableComponent, DisplayName = "NDI Stream"))
class RSHIPNDI_API URshipNDIStreamComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URshipNDIStreamComponent();

	// ========================================================================
	// UActorComponent Interface
	// ========================================================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================================================
	// Configuration
	// ========================================================================

	/** NDI stream configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|NDI")
	FRshipNDIStreamConfig Config;

	// ========================================================================
	// Streaming Control
	// ========================================================================

	/**
	 * Start NDI streaming.
	 * @return true if streaming started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|NDI")
	bool StartStreaming();

	/**
	 * Stop NDI streaming and release resources.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|NDI")
	void StopStreaming();

	/**
	 * Check if currently streaming.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|NDI")
	bool IsStreaming() const { return StreamState == ERshipNDIStreamState::Streaming; }

	/**
	 * Get current stream state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|NDI")
	ERshipNDIStreamState GetStreamState() const { return StreamState; }

	/**
	 * Get streaming statistics.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|NDI")
	FRshipNDIStreamStats GetStats() const { return Stats; }

	/**
	 * Update stream name at runtime.
	 * Note: Requires restart of streaming to take effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|NDI")
	void SetStreamName(const FString& NewName);

	/**
	 * Update resolution at runtime.
	 * Note: Requires restart of streaming to take effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|NDI")
	void SetResolution(int32 NewWidth, int32 NewHeight);

	/**
	 * Check if the NDI sender library is available.
	 * If false, streaming will not work.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|NDI")
	static bool IsNDISenderAvailable();

	// ========================================================================
	// Events
	// ========================================================================

	/** Fired when stream state changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|NDI")
	FOnNDIStreamStateChanged OnStreamStateChanged;

	/** Fired when NDI receiver count changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|NDI")
	FOnNDIReceiverCountChanged OnReceiverCountChanged;

protected:
	/** Current stream state */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|NDI")
	ERshipNDIStreamState StreamState = ERshipNDIStreamState::Stopped;

	/** Runtime statistics */
	UPROPERTY(BlueprintReadOnly, Category = "Rship|NDI")
	FRshipNDIStreamStats Stats;

private:
	// ========================================================================
	// Internal State
	// ========================================================================

	/** Reference to owning CineCamera actor */
	UPROPERTY()
	TWeakObjectPtr<ACineCameraActor> OwningCameraActor;

	/** Reference to CineCamera component */
	UPROPERTY()
	TWeakObjectPtr<UCineCameraComponent> CineCameraComponent;

	/** Scene capture component for exact CineCamera match */
	UPROPERTY()
	UCineCaptureComponent2D* CineCapture = nullptr;

	/** Triple-buffered render targets */
	UPROPERTY()
	TArray<UTextureRenderTarget2D*> RenderTargets;

	/** GPU renderer (handles async readback and NDI send) */
	TUniquePtr<FNDIStreamRenderer> Renderer;

	/** Current buffer index for round-robin */
	int32 CurrentBufferIndex = 0;

	/** Frame counter for statistics */
	uint64 FrameCounter = 0;

	/** Time of last frame for FPS calculation */
	double LastFrameTime = 0.0;

	/** Previous receiver count for change detection */
	int32 LastReceiverCount = 0;

	/** Last error message */
	FString LastErrorMessage;

	// ========================================================================
	// Internal Methods
	// ========================================================================

	/** Find and validate the owning CineCamera */
	bool FindOwningCineCamera();

	/** Initialize the CineCaptureComponent2D */
	bool InitializeCineCapture();

	/** Initialize render targets */
	bool InitializeRenderTargets();

	/** Initialize the NDI sender */
	bool InitializeNDISender();

	/** Cleanup all resources */
	void CleanupResources();

	/** Capture the current frame */
	void CaptureFrame();

	/** Process completed GPU readbacks */
	void ProcessReadbacks();

	/** Update statistics */
	void UpdateStats(float DeltaTime);

	/** Set stream state and fire delegate */
	void SetStreamState(ERshipNDIStreamState NewState);

	/** Set error state with message */
	void SetError(const FString& ErrorMessage);
};
