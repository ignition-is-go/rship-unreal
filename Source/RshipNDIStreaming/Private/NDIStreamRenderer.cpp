// Copyright Lucid. All Rights Reserved.

#include "NDIStreamRenderer.h"
#include "RshipNDIStreaming.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "TextureResource.h"

#if RSHIP_HAS_NDI_SENDER
#include "rship_ndi_sender.h"
#endif

FNDIStreamRenderer::FNDIStreamRenderer()
{
}

FNDIStreamRenderer::~FNDIStreamRenderer()
{
	Shutdown();
}

bool FNDIStreamRenderer::Initialize(const FConfig& InConfig)
{
	if (bIsInitialized)
	{
		UE_LOG(LogRshipNDI, Warning, TEXT("FNDIStreamRenderer::Initialize - Already initialized"));
		return true;
	}

	Config = InConfig;

	// Validate config
	if (Config.Width <= 0 || Config.Height <= 0)
	{
		UE_LOG(LogRshipNDI, Error, TEXT("FNDIStreamRenderer::Initialize - Invalid dimensions %dx%d"),
			Config.Width, Config.Height);
		return false;
	}

	if (Config.BufferCount < 2 || Config.BufferCount > 4)
	{
		UE_LOG(LogRshipNDI, Warning, TEXT("FNDIStreamRenderer::Initialize - BufferCount %d out of range, clamping to 3"),
			Config.BufferCount);
		Config.BufferCount = 3;
	}

	// Allocate staging buffers
	if (!AllocateStagingBuffers())
	{
		UE_LOG(LogRshipNDI, Error, TEXT("FNDIStreamRenderer::Initialize - Failed to allocate staging buffers"));
		return false;
	}

#if RSHIP_HAS_NDI_SENDER
	// Initialize NDI sender
	RshipNDIConfig NdiConfig;
	FTCHARToUTF8 StreamNameUtf8(*Config.StreamName);
	NdiConfig.stream_name = StreamNameUtf8.Get();
	NdiConfig.width = Config.Width;
	NdiConfig.height = Config.Height;
	NdiConfig.framerate_num = Config.FrameRate;
	NdiConfig.framerate_den = 1;
	NdiConfig.enable_alpha = Config.bEnableAlpha;
	NdiConfig.buffer_count = Config.BufferCount;

	NDISender = rship_ndi_create(&NdiConfig);
	if (!NDISender)
	{
		UE_LOG(LogRshipNDI, Error, TEXT("FNDIStreamRenderer::Initialize - Failed to create NDI sender"));
		FreeStagingBuffers();
		return false;
	}

	UE_LOG(LogRshipNDI, Log, TEXT("FNDIStreamRenderer::Initialize - NDI sender created: %s @ %dx%d @ %d fps"),
		*Config.StreamName, Config.Width, Config.Height, Config.FrameRate);
#else
	UE_LOG(LogRshipNDI, Error, TEXT("FNDIStreamRenderer::Initialize - NDI sender library not available"));
	FreeStagingBuffers();
	return false;
#endif

	bIsInitialized = true;
	return true;
}

void FNDIStreamRenderer::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	UE_LOG(LogRshipNDI, Log, TEXT("FNDIStreamRenderer::Shutdown - Shutting down"));

#if RSHIP_HAS_NDI_SENDER
	if (NDISender)
	{
		rship_ndi_destroy(NDISender);
		NDISender = nullptr;
	}
#endif

	FreeStagingBuffers();

	bIsInitialized = false;
}

bool FNDIStreamRenderer::AllocateStagingBuffers()
{
	StagingBuffers.SetNum(Config.BufferCount);

	for (int32 i = 0; i < Config.BufferCount; ++i)
	{
		FStagingBuffer& Buffer = StagingBuffers[i];

		// Create GPU readback object
		FString ReadbackName = FString::Printf(TEXT("NDIReadback_%d"), i);
		Buffer.Readback = new FRHIGPUTextureReadback(FName(*ReadbackName));
		Buffer.FrameNumber = -1;
		Buffer.bInFlight = false;
		Buffer.SubmitTime = 0.0;

		if (!Buffer.Readback)
		{
			UE_LOG(LogRshipNDI, Error, TEXT("FNDIStreamRenderer::AllocateStagingBuffers - Failed to create readback %d"), i);
			FreeStagingBuffers();
			return false;
		}
	}

	UE_LOG(LogRshipNDI, Log, TEXT("FNDIStreamRenderer::AllocateStagingBuffers - Allocated %d staging buffers"),
		Config.BufferCount);

	return true;
}

void FNDIStreamRenderer::FreeStagingBuffers()
{
	// Wait for render thread to finish any pending operations
	FlushRenderingCommands();

	for (FStagingBuffer& Buffer : StagingBuffers)
	{
		if (Buffer.Readback)
		{
			delete Buffer.Readback;
			Buffer.Readback = nullptr;
		}
	}

	StagingBuffers.Empty();
}

bool FNDIStreamRenderer::SubmitFrame(UTextureRenderTarget2D* RenderTarget, int64 FrameNumber)
{
	if (!bIsInitialized || !RenderTarget)
	{
		return false;
	}

	// Find a free staging buffer
	int32 FreeIndex = -1;
	for (int32 i = 0; i < StagingBuffers.Num(); ++i)
	{
		int32 Index = (CurrentStagingIndex + i) % StagingBuffers.Num();
		if (!StagingBuffers[Index].bInFlight)
		{
			FreeIndex = Index;
			break;
		}
	}

	if (FreeIndex < 0)
	{
		// All buffers in flight, pipeline stall - drop frame
		++TotalFramesDropped;
		UE_LOG(LogRshipNDI, Verbose, TEXT("FNDIStreamRenderer::SubmitFrame - All buffers in flight, dropping frame %lld"),
			FrameNumber);
		return false;
	}

	// Enqueue the readback
	EnqueueReadback(RenderTarget, FreeIndex, FrameNumber);
	CurrentStagingIndex = (FreeIndex + 1) % StagingBuffers.Num();

	return true;
}

void FNDIStreamRenderer::EnqueueReadback(UTextureRenderTarget2D* RenderTarget, int32 StagingIndex, int64 FrameNumber)
{
	FStagingBuffer& Buffer = StagingBuffers[StagingIndex];

	// Get the render target resource
	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogRshipNDI, Warning, TEXT("FNDIStreamRenderer::EnqueueReadback - No render target resource"));
		return;
	}

	Buffer.FrameNumber = FrameNumber;
	Buffer.bInFlight = true;
	Buffer.SubmitTime = FPlatformTime::Seconds();

	// Enqueue the GPU readback on the render thread
	FRHIGPUTextureReadback* Readback = Buffer.Readback;

	ENQUEUE_RENDER_COMMAND(NDIEnqueueReadback)(
		[Readback, RTResource](FRHICommandListImmediate& RHICmdList)
		{
			FRHITexture* Texture = RTResource->GetRenderTargetTexture();
			if (Texture)
			{
				// Enqueue copy from render target to staging buffer
				Readback->EnqueueCopy(RHICmdList, Texture);
			}
		});
}

void FNDIStreamRenderer::ProcessPendingFrames()
{
	if (!bIsInitialized)
	{
		return;
	}

	// Check all staging buffers for completed readbacks
	for (FStagingBuffer& Buffer : StagingBuffers)
	{
		if (Buffer.bInFlight && Buffer.Readback->IsReady())
		{
			ProcessCompletedReadback(Buffer);
		}
	}
}

void FNDIStreamRenderer::ProcessCompletedReadback(FStagingBuffer& Staging)
{
	double ReadbackTime = FPlatformTime::Seconds() - Staging.SubmitTime;
	ReadbackTimes.Add(static_cast<float>(ReadbackTime * 1000.0)); // Convert to ms
	if (ReadbackTimes.Num() > 60)
	{
		ReadbackTimes.RemoveAt(0);
	}

#if RSHIP_HAS_NDI_SENDER
	if (NDISender)
	{
		// Lock the readback buffer to get CPU access
		int32 RowPitchInPixels = 0;
		void* Data = Staging.Readback->Lock(RowPitchInPixels);

		if (Data)
		{
			// Calculate data size
			int32 DataSize = Config.Width * Config.Height * 4; // RGBA

			// Create frame struct for Rust
			RshipNDIFrame Frame;
			Frame.data = static_cast<const uint8*>(Data);
			Frame.data_size = DataSize;
			Frame.width = Config.Width;
			Frame.height = Config.Height;
			Frame.frame_number = Staging.FrameNumber;
			Frame.timestamp_100ns = FDateTime::Now().GetTicks(); // 100ns units

			// Submit to NDI sender
			bool bSuccess = rship_ndi_submit_frame(NDISender, &Frame);

			if (bSuccess)
			{
				++TotalFramesSent;
			}
			else
			{
				++TotalFramesDropped;
			}

			Staging.Readback->Unlock();
		}
		else
		{
			UE_LOG(LogRshipNDI, Warning, TEXT("FNDIStreamRenderer::ProcessCompletedReadback - Failed to lock readback buffer"));
		}
	}
#endif

	// Mark buffer as available
	Staging.bInFlight = false;
	Staging.FrameNumber = -1;
}

FNDIStreamRenderer::FStats FNDIStreamRenderer::GetStats() const
{
	FStats OutStats;

	// Calculate average readback time
	if (ReadbackTimes.Num() > 0)
	{
		float Sum = 0.0f;
		for (float Time : ReadbackTimes)
		{
			Sum += Time;
		}
		OutStats.AvgReadbackTimeMs = Sum / ReadbackTimes.Num();
	}

	OutStats.FramesSent = TotalFramesSent;
	OutStats.FramesDropped = TotalFramesDropped;

#if RSHIP_HAS_NDI_SENDER
	if (NDISender)
	{
		RshipNDIStats NdiStats;
		rship_ndi_get_stats(NDISender, &NdiStats);
		OutStats.AvgSendTimeMs = static_cast<float>(NdiStats.avg_send_time_us / 1000.0);
		OutStats.ConnectedReceivers = NdiStats.connected_receivers;
		OutStats.QueueDepth = NdiStats.queue_depth;
	}
#endif

	return OutStats;
}

bool FNDIStreamRenderer::IsHealthy() const
{
#if RSHIP_HAS_NDI_SENDER
	if (NDISender)
	{
		RshipNDIStats NdiStats;
		rship_ndi_get_stats(NDISender, &NdiStats);
		return NdiStats.is_healthy;
	}
#endif
	return false;
}
