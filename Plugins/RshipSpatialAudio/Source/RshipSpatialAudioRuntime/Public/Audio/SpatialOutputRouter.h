// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"
#include "SpatialOutputRouter.generated.h"

/**
 * Output device/card configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialOutputDevice
{
	GENERATED_BODY()

	/** Device identifier (e.g., "Dante Card 1", "MADI 1") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	FString DeviceId;

	/** Human-readable name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	FString DisplayName;

	/** Number of channels on this device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	int32 ChannelCount = 64;

	/** First channel index in the global output space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	int32 FirstChannelIndex = 0;

	/** Is device online and available */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpatialAudio|Output")
	bool bIsOnline = true;

	/** Sample rate (Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	int32 SampleRate = 48000;
};

/**
 * Channel routing entry - maps a speaker to a physical output.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialChannelRoute
{
	GENERATED_BODY()

	/** Speaker ID being routed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	FGuid SpeakerId;

	/** Target device ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	FString DeviceId;

	/** Channel index within the device (0-based) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	int32 DeviceChannel = 0;

	/** Global output channel index (computed from device + device channel) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpatialAudio|Output")
	int32 GlobalChannel = 0;

	/** Gain trim for this route (linear) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float GainTrim = 1.0f;

	/** Delay trim for this route (ms) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
	float DelayTrimMs = 0.0f;

	/** Is this route enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	bool bEnabled = true;
};

/**
 * Output routing matrix configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialRoutingMatrix
{
	GENERATED_BODY()

	/** Output devices */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	TArray<FSpatialOutputDevice> Devices;

	/** Channel routes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Output")
	TArray<FSpatialChannelRoute> Routes;

	/** Total output channel count (sum of all devices) */
	int32 GetTotalChannelCount() const
	{
		int32 Total = 0;
		for (const FSpatialOutputDevice& Device : Devices)
		{
			Total += Device.ChannelCount;
		}
		return Total;
	}

	/** Find device by ID */
	const FSpatialOutputDevice* FindDevice(const FString& DeviceId) const
	{
		for (const FSpatialOutputDevice& Device : Devices)
		{
			if (Device.DeviceId == DeviceId)
			{
				return &Device;
			}
		}
		return nullptr;
	}

	/** Find route for speaker */
	const FSpatialChannelRoute* FindRouteForSpeaker(const FGuid& SpeakerId) const
	{
		for (const FSpatialChannelRoute& Route : Routes)
		{
			if (Route.SpeakerId == SpeakerId)
			{
				return &Route;
			}
		}
		return nullptr;
	}

	/** Compute global channel indices for all routes */
	void UpdateGlobalChannels()
	{
		for (FSpatialChannelRoute& Route : Routes)
		{
			if (const FSpatialOutputDevice* Device = FindDevice(Route.DeviceId))
			{
				Route.GlobalChannel = Device->FirstChannelIndex + Route.DeviceChannel;
			}
		}
	}
};

/**
 * Output router - manages speaker to physical channel mapping.
 *
 * Responsibilities:
 * - Map virtual speakers to physical output channels
 * - Support multiple output devices (Dante, MADI, etc.)
 * - Apply per-route gain and delay trims
 * - Handle device hot-plugging
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialOutputRouter
{
public:
	FSpatialOutputRouter();
	~FSpatialOutputRouter() = default;

	// Non-copyable
	FSpatialOutputRouter(const FSpatialOutputRouter&) = delete;
	FSpatialOutputRouter& operator=(const FSpatialOutputRouter&) = delete;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/**
	 * Set the routing matrix configuration.
	 */
	void SetRoutingMatrix(const FSpatialRoutingMatrix& Matrix);

	/**
	 * Get the current routing matrix.
	 */
	const FSpatialRoutingMatrix& GetRoutingMatrix() const { return RoutingMatrix; }

	/**
	 * Auto-configure routing from speakers.
	 * Creates 1:1 mapping from speaker output channel to global channel.
	 */
	void AutoConfigureFromSpeakers(const TArray<FSpatialSpeaker>& Speakers);

	/**
	 * Add an output device.
	 */
	void AddDevice(const FSpatialOutputDevice& Device);

	/**
	 * Remove an output device.
	 */
	bool RemoveDevice(const FString& DeviceId);

	/**
	 * Set device online/offline status.
	 */
	void SetDeviceOnline(const FString& DeviceId, bool bOnline);

	// ========================================================================
	// ROUTING
	// ========================================================================

	/**
	 * Add a route from speaker to output channel.
	 */
	void AddRoute(const FSpatialChannelRoute& Route);

	/**
	 * Remove a route by speaker ID.
	 */
	bool RemoveRoute(const FGuid& SpeakerId);

	/**
	 * Update route settings.
	 */
	bool UpdateRoute(const FGuid& SpeakerId, const FSpatialChannelRoute& Route);

	/**
	 * Get the global output channel for a speaker.
	 * Returns -1 if speaker is not routed.
	 */
	int32 GetOutputChannelForSpeaker(const FGuid& SpeakerId) const;

	/**
	 * Get the global output channel index from speaker's configured output channel.
	 * This is the fast path used during rendering.
	 */
	int32 GetOutputChannelFromIndex(int32 SpeakerOutputChannel) const;

	/**
	 * Get route information for a speaker.
	 */
	bool GetRouteForSpeaker(const FGuid& SpeakerId, FSpatialChannelRoute& OutRoute) const;

	/**
	 * Get gain trim for a speaker route.
	 */
	float GetRouteTrim(const FGuid& SpeakerId) const;

	/**
	 * Get delay trim for a speaker route.
	 */
	float GetDelayTrim(const FGuid& SpeakerId) const;

	// ========================================================================
	// QUERIES
	// ========================================================================

	/**
	 * Get total number of output channels.
	 */
	int32 GetTotalOutputChannels() const;

	/**
	 * Get list of all devices.
	 */
	TArray<FSpatialOutputDevice> GetDevices() const { return RoutingMatrix.Devices; }

	/**
	 * Get list of all routes.
	 */
	TArray<FSpatialChannelRoute> GetRoutes() const { return RoutingMatrix.Routes; }

	/**
	 * Validate routing configuration.
	 */
	TArray<FString> Validate() const;

	// ========================================================================
	// SERIALIZATION
	// ========================================================================

	/**
	 * Export routing configuration to JSON.
	 */
	FString ExportToJson() const;

	/**
	 * Import routing configuration from JSON.
	 */
	bool ImportFromJson(const FString& JsonString);

private:
	/** Routing configuration */
	FSpatialRoutingMatrix RoutingMatrix;

	/** Fast lookup: speaker output channel index -> global channel */
	TArray<int32> ChannelIndexMap;

	/** Fast lookup: speaker ID -> route index */
	TMap<FGuid, int32> SpeakerToRouteIndex;

	/** Rebuild lookup tables */
	void RebuildLookups();
};
