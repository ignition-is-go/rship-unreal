// Copyright Rocketship. All Rights Reserved.

#include "Audio/SpatialOutputRouter.h"
#include "RshipSpatialAudioRuntimeModule.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

FSpatialOutputRouter::FSpatialOutputRouter()
{
	// Initialize with a default device
	FSpatialOutputDevice DefaultDevice;
	DefaultDevice.DeviceId = TEXT("Default");
	DefaultDevice.DisplayName = TEXT("Default Output");
	DefaultDevice.ChannelCount = 64;
	DefaultDevice.FirstChannelIndex = 0;
	RoutingMatrix.Devices.Add(DefaultDevice);
}

void FSpatialOutputRouter::SetRoutingMatrix(const FSpatialRoutingMatrix& Matrix)
{
	RoutingMatrix = Matrix;
	RoutingMatrix.UpdateGlobalChannels();
	RebuildLookups();

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("Output router configured: %d devices, %d routes, %d total channels"),
		RoutingMatrix.Devices.Num(), RoutingMatrix.Routes.Num(), GetTotalOutputChannels());
}

void FSpatialOutputRouter::AutoConfigureFromSpeakers(const TArray<FSpatialSpeaker>& Speakers)
{
	RoutingMatrix.Routes.Empty();

	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		FSpatialChannelRoute Route;
		Route.SpeakerId = Speaker.Id;
		Route.DeviceId = RoutingMatrix.Devices.Num() > 0 ? RoutingMatrix.Devices[0].DeviceId : TEXT("Default");
		Route.DeviceChannel = Speaker.OutputChannel;
		Route.GlobalChannel = Speaker.OutputChannel;
		Route.GainTrim = 1.0f;
		Route.DelayTrimMs = 0.0f;
		Route.bEnabled = true;

		RoutingMatrix.Routes.Add(Route);
	}

	RoutingMatrix.UpdateGlobalChannels();
	RebuildLookups();

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("Auto-configured %d speaker routes"), Speakers.Num());
}

void FSpatialOutputRouter::AddDevice(const FSpatialOutputDevice& Device)
{
	// Check for existing device with same ID
	for (int32 i = 0; i < RoutingMatrix.Devices.Num(); ++i)
	{
		if (RoutingMatrix.Devices[i].DeviceId == Device.DeviceId)
		{
			// Update existing
			RoutingMatrix.Devices[i] = Device;
			RoutingMatrix.UpdateGlobalChannels();
			RebuildLookups();
			return;
		}
	}

	// Calculate first channel index
	FSpatialOutputDevice NewDevice = Device;
	int32 NextIndex = 0;
	for (const FSpatialOutputDevice& Existing : RoutingMatrix.Devices)
	{
		NextIndex = FMath::Max(NextIndex, Existing.FirstChannelIndex + Existing.ChannelCount);
	}
	NewDevice.FirstChannelIndex = NextIndex;

	RoutingMatrix.Devices.Add(NewDevice);
	RoutingMatrix.UpdateGlobalChannels();
	RebuildLookups();

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("Added output device '%s': %d channels starting at %d"),
		*Device.DisplayName, Device.ChannelCount, NewDevice.FirstChannelIndex);
}

bool FSpatialOutputRouter::RemoveDevice(const FString& DeviceId)
{
	for (int32 i = 0; i < RoutingMatrix.Devices.Num(); ++i)
	{
		if (RoutingMatrix.Devices[i].DeviceId == DeviceId)
		{
			RoutingMatrix.Devices.RemoveAt(i);

			// Remove routes to this device
			RoutingMatrix.Routes.RemoveAll([&DeviceId](const FSpatialChannelRoute& Route) {
				return Route.DeviceId == DeviceId;
			});

			// Recalculate first channel indices
			int32 CurrentIndex = 0;
			for (FSpatialOutputDevice& Device : RoutingMatrix.Devices)
			{
				Device.FirstChannelIndex = CurrentIndex;
				CurrentIndex += Device.ChannelCount;
			}

			RoutingMatrix.UpdateGlobalChannels();
			RebuildLookups();
			return true;
		}
	}
	return false;
}

void FSpatialOutputRouter::SetDeviceOnline(const FString& DeviceId, bool bOnline)
{
	for (FSpatialOutputDevice& Device : RoutingMatrix.Devices)
	{
		if (Device.DeviceId == DeviceId)
		{
			Device.bIsOnline = bOnline;
			UE_LOG(LogRshipSpatialAudio, Log, TEXT("Device '%s' is now %s"),
				*Device.DisplayName, bOnline ? TEXT("online") : TEXT("offline"));
			return;
		}
	}
}

void FSpatialOutputRouter::AddRoute(const FSpatialChannelRoute& Route)
{
	// Check for existing route for this speaker
	for (int32 i = 0; i < RoutingMatrix.Routes.Num(); ++i)
	{
		if (RoutingMatrix.Routes[i].SpeakerId == Route.SpeakerId)
		{
			// Update existing
			RoutingMatrix.Routes[i] = Route;
			RoutingMatrix.UpdateGlobalChannels();
			RebuildLookups();
			return;
		}
	}

	FSpatialChannelRoute NewRoute = Route;

	// Calculate global channel
	if (const FSpatialOutputDevice* Device = RoutingMatrix.FindDevice(Route.DeviceId))
	{
		NewRoute.GlobalChannel = Device->FirstChannelIndex + Route.DeviceChannel;
	}

	RoutingMatrix.Routes.Add(NewRoute);
	RebuildLookups();
}

bool FSpatialOutputRouter::RemoveRoute(const FGuid& SpeakerId)
{
	int32 Removed = RoutingMatrix.Routes.RemoveAll([&SpeakerId](const FSpatialChannelRoute& Route) {
		return Route.SpeakerId == SpeakerId;
	});

	if (Removed > 0)
	{
		RebuildLookups();
		return true;
	}
	return false;
}

bool FSpatialOutputRouter::UpdateRoute(const FGuid& SpeakerId, const FSpatialChannelRoute& Route)
{
	for (FSpatialChannelRoute& Existing : RoutingMatrix.Routes)
	{
		if (Existing.SpeakerId == SpeakerId)
		{
			Existing = Route;
			Existing.SpeakerId = SpeakerId;  // Preserve ID

			// Update global channel
			if (const FSpatialOutputDevice* Device = RoutingMatrix.FindDevice(Route.DeviceId))
			{
				Existing.GlobalChannel = Device->FirstChannelIndex + Route.DeviceChannel;
			}

			RebuildLookups();
			return true;
		}
	}
	return false;
}

int32 FSpatialOutputRouter::GetOutputChannelForSpeaker(const FGuid& SpeakerId) const
{
	const int32* RouteIndex = SpeakerToRouteIndex.Find(SpeakerId);
	if (RouteIndex && *RouteIndex >= 0 && *RouteIndex < RoutingMatrix.Routes.Num())
	{
		const FSpatialChannelRoute& Route = RoutingMatrix.Routes[*RouteIndex];
		if (Route.bEnabled)
		{
			return Route.GlobalChannel;
		}
	}
	return -1;
}

int32 FSpatialOutputRouter::GetOutputChannelFromIndex(int32 SpeakerOutputChannel) const
{
	if (SpeakerOutputChannel >= 0 && SpeakerOutputChannel < ChannelIndexMap.Num())
	{
		return ChannelIndexMap[SpeakerOutputChannel];
	}
	// Fall back to direct mapping
	return SpeakerOutputChannel;
}

bool FSpatialOutputRouter::GetRouteForSpeaker(const FGuid& SpeakerId, FSpatialChannelRoute& OutRoute) const
{
	const int32* RouteIndex = SpeakerToRouteIndex.Find(SpeakerId);
	if (RouteIndex && *RouteIndex >= 0 && *RouteIndex < RoutingMatrix.Routes.Num())
	{
		OutRoute = RoutingMatrix.Routes[*RouteIndex];
		return true;
	}
	return false;
}

float FSpatialOutputRouter::GetRouteTrim(const FGuid& SpeakerId) const
{
	const int32* RouteIndex = SpeakerToRouteIndex.Find(SpeakerId);
	if (RouteIndex && *RouteIndex >= 0 && *RouteIndex < RoutingMatrix.Routes.Num())
	{
		return RoutingMatrix.Routes[*RouteIndex].GainTrim;
	}
	return 1.0f;
}

float FSpatialOutputRouter::GetDelayTrim(const FGuid& SpeakerId) const
{
	const int32* RouteIndex = SpeakerToRouteIndex.Find(SpeakerId);
	if (RouteIndex && *RouteIndex >= 0 && *RouteIndex < RoutingMatrix.Routes.Num())
	{
		return RoutingMatrix.Routes[*RouteIndex].DelayTrimMs;
	}
	return 0.0f;
}

int32 FSpatialOutputRouter::GetTotalOutputChannels() const
{
	return RoutingMatrix.GetTotalChannelCount();
}

TArray<FString> FSpatialOutputRouter::Validate() const
{
	TArray<FString> Errors;

	// Check for duplicate global channel assignments
	TMap<int32, FGuid> ChannelUsage;
	for (const FSpatialChannelRoute& Route : RoutingMatrix.Routes)
	{
		if (!Route.bEnabled)
		{
			continue;
		}

		if (const FGuid* ExistingId = ChannelUsage.Find(Route.GlobalChannel))
		{
			Errors.Add(FString::Printf(TEXT("Global channel %d assigned to multiple speakers"),
				Route.GlobalChannel));
		}
		else
		{
			ChannelUsage.Add(Route.GlobalChannel, Route.SpeakerId);
		}
	}

	// Check for routes to offline devices
	for (const FSpatialChannelRoute& Route : RoutingMatrix.Routes)
	{
		if (const FSpatialOutputDevice* Device = RoutingMatrix.FindDevice(Route.DeviceId))
		{
			if (!Device->bIsOnline)
			{
				Errors.Add(FString::Printf(TEXT("Speaker routed to offline device '%s'"),
					*Device->DisplayName));
			}

			if (Route.DeviceChannel >= Device->ChannelCount)
			{
				Errors.Add(FString::Printf(TEXT("Route to device channel %d exceeds device '%s' channel count (%d)"),
					Route.DeviceChannel, *Device->DisplayName, Device->ChannelCount));
			}
		}
		else
		{
			Errors.Add(FString::Printf(TEXT("Route references unknown device '%s'"), *Route.DeviceId));
		}
	}

	return Errors;
}

FString FSpatialOutputRouter::ExportToJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	// Export devices
	TArray<TSharedPtr<FJsonValue>> DevicesArray;
	for (const FSpatialOutputDevice& Device : RoutingMatrix.Devices)
	{
		TSharedRef<FJsonObject> DeviceObj = MakeShared<FJsonObject>();
		DeviceObj->SetStringField(TEXT("deviceId"), Device.DeviceId);
		DeviceObj->SetStringField(TEXT("displayName"), Device.DisplayName);
		DeviceObj->SetNumberField(TEXT("channelCount"), Device.ChannelCount);
		DeviceObj->SetNumberField(TEXT("firstChannelIndex"), Device.FirstChannelIndex);
		DeviceObj->SetNumberField(TEXT("sampleRate"), Device.SampleRate);
		DevicesArray.Add(MakeShared<FJsonValueObject>(DeviceObj));
	}
	Root->SetArrayField(TEXT("devices"), DevicesArray);

	// Export routes
	TArray<TSharedPtr<FJsonValue>> RoutesArray;
	for (const FSpatialChannelRoute& Route : RoutingMatrix.Routes)
	{
		TSharedRef<FJsonObject> RouteObj = MakeShared<FJsonObject>();
		RouteObj->SetStringField(TEXT("speakerId"), Route.SpeakerId.ToString());
		RouteObj->SetStringField(TEXT("deviceId"), Route.DeviceId);
		RouteObj->SetNumberField(TEXT("deviceChannel"), Route.DeviceChannel);
		RouteObj->SetNumberField(TEXT("globalChannel"), Route.GlobalChannel);
		RouteObj->SetNumberField(TEXT("gainTrim"), Route.GainTrim);
		RouteObj->SetNumberField(TEXT("delayTrimMs"), Route.DelayTrimMs);
		RouteObj->SetBoolField(TEXT("enabled"), Route.bEnabled);
		RoutesArray.Add(MakeShared<FJsonValueObject>(RouteObj));
	}
	Root->SetArrayField(TEXT("routes"), RoutesArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root, Writer);
	return OutputString;
}

bool FSpatialOutputRouter::ImportFromJson(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogRshipSpatialAudio, Error, TEXT("Failed to parse routing JSON"));
		return false;
	}

	FSpatialRoutingMatrix NewMatrix;

	// Import devices
	const TArray<TSharedPtr<FJsonValue>>* DevicesArray;
	if (Root->TryGetArrayField(TEXT("devices"), DevicesArray))
	{
		for (const TSharedPtr<FJsonValue>& DeviceVal : *DevicesArray)
		{
			const TSharedPtr<FJsonObject>* DeviceObj;
			if (DeviceVal->TryGetObject(DeviceObj))
			{
				FSpatialOutputDevice Device;
				(*DeviceObj)->TryGetStringField(TEXT("deviceId"), Device.DeviceId);
				(*DeviceObj)->TryGetStringField(TEXT("displayName"), Device.DisplayName);
				(*DeviceObj)->TryGetNumberField(TEXT("channelCount"), Device.ChannelCount);
				(*DeviceObj)->TryGetNumberField(TEXT("firstChannelIndex"), Device.FirstChannelIndex);
				(*DeviceObj)->TryGetNumberField(TEXT("sampleRate"), Device.SampleRate);
				NewMatrix.Devices.Add(Device);
			}
		}
	}

	// Import routes
	const TArray<TSharedPtr<FJsonValue>>* RoutesArray;
	if (Root->TryGetArrayField(TEXT("routes"), RoutesArray))
	{
		for (const TSharedPtr<FJsonValue>& RouteVal : *RoutesArray)
		{
			const TSharedPtr<FJsonObject>* RouteObj;
			if (RouteVal->TryGetObject(RouteObj))
			{
				FSpatialChannelRoute Route;
				FString SpeakerIdStr;
				if ((*RouteObj)->TryGetStringField(TEXT("speakerId"), SpeakerIdStr))
				{
					FGuid::Parse(SpeakerIdStr, Route.SpeakerId);
				}
				(*RouteObj)->TryGetStringField(TEXT("deviceId"), Route.DeviceId);
				(*RouteObj)->TryGetNumberField(TEXT("deviceChannel"), Route.DeviceChannel);
				(*RouteObj)->TryGetNumberField(TEXT("globalChannel"), Route.GlobalChannel);
				(*RouteObj)->TryGetNumberField(TEXT("gainTrim"), Route.GainTrim);
				(*RouteObj)->TryGetNumberField(TEXT("delayTrimMs"), Route.DelayTrimMs);
				(*RouteObj)->TryGetBoolField(TEXT("enabled"), Route.bEnabled);
				NewMatrix.Routes.Add(Route);
			}
		}
	}

	SetRoutingMatrix(NewMatrix);
	return true;
}

void FSpatialOutputRouter::RebuildLookups()
{
	SpeakerToRouteIndex.Empty();

	// Find max channel index for channel map
	int32 MaxChannel = 0;
	for (const FSpatialChannelRoute& Route : RoutingMatrix.Routes)
	{
		MaxChannel = FMath::Max(MaxChannel, Route.DeviceChannel + 1);
		MaxChannel = FMath::Max(MaxChannel, Route.GlobalChannel + 1);
	}

	// Initialize channel index map with identity mapping
	ChannelIndexMap.SetNum(FMath::Max(MaxChannel, 256));
	for (int32 i = 0; i < ChannelIndexMap.Num(); ++i)
	{
		ChannelIndexMap[i] = i;
	}

	// Build speaker to route index map
	for (int32 i = 0; i < RoutingMatrix.Routes.Num(); ++i)
	{
		const FSpatialChannelRoute& Route = RoutingMatrix.Routes[i];
		SpeakerToRouteIndex.Add(Route.SpeakerId, i);

		// Update channel index map (device channel -> global channel)
		if (Route.DeviceChannel >= 0 && Route.DeviceChannel < ChannelIndexMap.Num())
		{
			ChannelIndexMap[Route.DeviceChannel] = Route.GlobalChannel;
		}
	}
}
