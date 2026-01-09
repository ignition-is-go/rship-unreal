// Copyright Lucid. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHIGPUReadback.h"
#include "Engine/TextureRenderTarget2D.h"

// Forward declare opaque Rust handle type
// The full struct definitions are in rship_ndi_sender.h, included only in the .cpp
#if RSHIP_HAS_NDI_SENDER
struct RshipNDISender;
#endif

/**
 * Manages the GPU rendering pipeline for NDI streaming.
 *
 * This class handles:
 * - Async GPU texture readback using FRHIGPUTextureReadback
 * - Triple-buffered staging for pipeline efficiency
 * - Frame submission to the Rust NDI sender via FFI
 *
 * Thread safety: This class is NOT thread-safe. All methods must be called
 * from the game thread.
 */
class FNDIStreamRenderer
{
public:
	/**
	 * Configuration for the renderer.
	 */
	struct FConfig
	{
		/** Frame width in pixels */
		int32 Width = 1920;
		/** Frame height in pixels */
		int32 Height = 1080;
		/** Number of staging buffers (2-4) */
		int32 BufferCount = 3;
		/** Enable alpha channel */
		bool bEnableAlpha = true;
		/** NDI stream name */
		FString StreamName;
		/** Target framerate */
		int32 FrameRate = 60;
	};

	/**
	 * Statistics from the GPU pipeline.
	 */
	struct FStats
	{
		/** Average GPU readback time in milliseconds */
		float AvgReadbackTimeMs = 0.0f;
		/** Average NDI send time in milliseconds */
		float AvgSendTimeMs = 0.0f;
		/** Total frames sent */
		int64 FramesSent = 0;
		/** Frames dropped */
		int64 FramesDropped = 0;
		/** Connected receiver count */
		int32 ConnectedReceivers = 0;
		/** Current queue depth */
		int32 QueueDepth = 0;
	};

	FNDIStreamRenderer();
	~FNDIStreamRenderer();

	// Non-copyable, non-movable
	FNDIStreamRenderer(const FNDIStreamRenderer&) = delete;
	FNDIStreamRenderer& operator=(const FNDIStreamRenderer&) = delete;

	/**
	 * Initialize GPU resources and Rust NDI sender.
	 * @param InConfig Configuration for the renderer
	 * @return true if initialization succeeded
	 */
	bool Initialize(const FConfig& InConfig);

	/**
	 * Shutdown and cleanup all resources.
	 */
	void Shutdown();

	/**
	 * Check if the renderer is initialized and ready.
	 */
	bool IsInitialized() const { return bIsInitialized; }

	/**
	 * Submit a render target for NDI streaming.
	 *
	 * This initiates an async GPU readback. The frame will be sent
	 * to NDI when the readback completes.
	 *
	 * @param RenderTarget The render target to read from
	 * @param FrameNumber Frame number for ordering
	 * @return true if submission was accepted
	 */
	bool SubmitFrame(UTextureRenderTarget2D* RenderTarget, int64 FrameNumber);

	/**
	 * Poll for completed readbacks and send to NDI.
	 * Call this every frame.
	 */
	void ProcessPendingFrames();

	/**
	 * Get current statistics.
	 */
	FStats GetStats() const;

	/**
	 * Check if the NDI sender is healthy.
	 */
	bool IsHealthy() const;

private:
	/**
	 * Staging buffer for async readback.
	 */
	struct FStagingBuffer
	{
		/** GPU readback object */
		FRHIGPUTextureReadback* Readback = nullptr;
		/** Frame number being read back */
		int64 FrameNumber = -1;
		/** Whether this buffer has a readback in flight */
		bool bInFlight = false;
		/** Time when readback was submitted */
		double SubmitTime = 0.0;
	};

	/** Configuration */
	FConfig Config;

	/** Whether renderer is initialized */
	bool bIsInitialized = false;

	/** Staging buffers for async readback */
	TArray<FStagingBuffer> StagingBuffers;

	/** Current staging buffer index (round-robin) */
	int32 CurrentStagingIndex = 0;

#if RSHIP_HAS_NDI_SENDER
	/** Rust NDI sender handle */
	RshipNDISender* NDISender = nullptr;
#endif

	/** Rolling average for readback times */
	TArray<float> ReadbackTimes;

	/** Total frames sent */
	int64 TotalFramesSent = 0;

	/** Total frames dropped */
	int64 TotalFramesDropped = 0;

	/** Diagnostic frame counter (resets on Initialize) */
	int32 DiagFrameCount = 0;

	// Internal methods

	/** Allocate staging buffers */
	bool AllocateStagingBuffers();

	/** Free staging buffers */
	void FreeStagingBuffers();

	/** Enqueue a GPU readback */
	void EnqueueReadback(UTextureRenderTarget2D* RenderTarget, int32 StagingIndex, int64 FrameNumber);

	/** Process a completed readback and send to NDI */
	void ProcessCompletedReadback(FStagingBuffer& Staging);
};
