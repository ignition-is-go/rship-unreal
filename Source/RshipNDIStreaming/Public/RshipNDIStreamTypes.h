// Copyright Lucid. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RshipNDIStreamTypes.generated.h"

/**
 * NDI stream state enumeration.
 */
UENUM(BlueprintType)
enum class ERshipNDIStreamState : uint8
{
	/** Stream is stopped */
	Stopped     UMETA(DisplayName = "Stopped"),
	/** Stream is starting up */
	Starting    UMETA(DisplayName = "Starting"),
	/** Stream is active and sending frames */
	Streaming   UMETA(DisplayName = "Streaming"),
	/** Stream encountered an error */
	Error       UMETA(DisplayName = "Error")
};

/**
 * Configuration for NDI streaming.
 */
USTRUCT(BlueprintType)
struct RSHIPNDISTREAMING_API FRshipNDIStreamConfig
{
	GENERATED_BODY()

	/** Stream name visible on the network */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDI")
	FString StreamName = TEXT("Unreal CineCamera");

	/** Resolution width (default 8K = 7680) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDI", meta = (ClampMin = "640", ClampMax = "15360"))
	int32 Width = 7680;

	/** Resolution height (default 8K = 4320) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDI", meta = (ClampMin = "360", ClampMax = "8640"))
	int32 Height = 4320;

	/** Target framerate (default 60) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDI", meta = (ClampMin = "1", ClampMax = "120"))
	int32 FrameRate = 60;

	/** Enable alpha channel (RGBA vs RGB) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDI")
	bool bEnableAlpha = true;

	/** Number of frame buffers for async pipeline (2-4, default 3 for triple-buffering) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDI|Advanced", meta = (ClampMin = "2", ClampMax = "4"))
	int32 BufferCount = 3;

	/** Use async GPU readback (required for high performance, disable only for debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDI|Advanced")
	bool bUseAsyncReadback = true;

	/** Automatically start streaming when component begins play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDI")
	bool bAutoStartOnBeginPlay = false;

	/** Calculate total VRAM required for this configuration */
	FORCEINLINE int64 GetVRAMUsageBytes() const
	{
		// Each buffer: width * height * 4 bytes (RGBA)
		// Render targets + staging buffers
		const int64 FrameSize = static_cast<int64>(Width) * Height * 4;
		return FrameSize * BufferCount * 2; // RT + staging
	}

	/** Calculate bandwidth in GB/s */
	FORCEINLINE float GetBandwidthGBps() const
	{
		const float FrameSize = static_cast<float>(Width) * Height * 4;
		return (FrameSize * FrameRate) / (1024.0f * 1024.0f * 1024.0f);
	}
};

/**
 * Runtime statistics for NDI streaming.
 */
USTRUCT(BlueprintType)
struct RSHIPNDISTREAMING_API FRshipNDIStreamStats
{
	GENERATED_BODY()

	/** Current effective FPS */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	float CurrentFPS = 0.0f;

	/** Average time per frame in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	float AverageFrameTimeMs = 0.0f;

	/** Average GPU readback time in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	float GPUReadbackTimeMs = 0.0f;

	/** Average NDI send time in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	float NDISendTimeMs = 0.0f;

	/** Total frames successfully sent */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	int64 TotalFramesSent = 0;

	/** Frames dropped due to pipeline stall */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	int64 DroppedFrames = 0;

	/** Current bandwidth in Mbps */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	float BandwidthMbps = 0.0f;

	/** Number of connected NDI receivers */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	int32 ConnectedReceivers = 0;

	/** Current queue depth (frames pending send) */
	UPROPERTY(BlueprintReadOnly, Category = "NDI|Stats")
	int32 QueueDepth = 0;

	/** Reset all statistics */
	void Reset()
	{
		CurrentFPS = 0.0f;
		AverageFrameTimeMs = 0.0f;
		GPUReadbackTimeMs = 0.0f;
		NDISendTimeMs = 0.0f;
		TotalFramesSent = 0;
		DroppedFrames = 0;
		BandwidthMbps = 0.0f;
		ConnectedReceivers = 0;
		QueueDepth = 0;
	}
};

/**
 * Delegate fired when stream state changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNDIStreamStateChanged, ERshipNDIStreamState, NewState);

/**
 * Delegate fired when NDI receivers connect or disconnect.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNDIReceiverCountChanged, int32, ReceiverCount);
