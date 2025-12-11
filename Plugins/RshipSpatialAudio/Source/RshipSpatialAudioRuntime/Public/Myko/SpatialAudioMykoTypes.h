// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"
#include "Core/SpatialZone.h"
#include "Core/SpatialAudioObject.h"

/**
 * Myko entity type names for spatial audio.
 * These map to the rShip entity schema.
 */
namespace SpatialAudioMykoTypes
{
	// Instance (venue) level
	static const FString Venue = TEXT("SpatialAudioVenue");

	// Target types
	static const FString Speaker = TEXT("SpatialAudioSpeaker");
	static const FString Zone = TEXT("SpatialAudioZone");
	static const FString Object = TEXT("SpatialAudioObject");
	static const FString Array = TEXT("SpatialAudioArray");

	// Meter/pulse types
	static const FString SpeakerMeter = TEXT("SpatialAudioSpeakerMeter");
	static const FString ObjectMeter = TEXT("SpatialAudioObjectMeter");
	static const FString GainReduction = TEXT("SpatialAudioGainReduction");
}

/**
 * Myko action IDs for spatial audio targets.
 */
namespace SpatialAudioMykoActions
{
	// Speaker actions
	static const FString SetSpeakerGain = TEXT("setSpeakerGain");
	static const FString SetSpeakerDelay = TEXT("setSpeakerDelay");
	static const FString SetSpeakerMute = TEXT("setSpeakerMute");
	static const FString SetSpeakerPolarity = TEXT("setSpeakerPolarity");
	static const FString SetSpeakerEQ = TEXT("setSpeakerEQ");
	static const FString SetSpeakerLimiter = TEXT("setSpeakerLimiter");
	static const FString SetSpeakerPosition = TEXT("setSpeakerPosition");

	// Zone actions
	static const FString SetZoneRenderer = TEXT("setZoneRenderer");
	static const FString SetZoneSpeakers = TEXT("setZoneSpeakers");
	static const FString SetZoneActive = TEXT("setZoneActive");

	// Object actions
	static const FString SetObjectPosition = TEXT("setObjectPosition");
	static const FString SetObjectSpread = TEXT("setObjectSpread");
	static const FString SetObjectGain = TEXT("setObjectGain");
	static const FString SetObjectRouting = TEXT("setObjectRouting");
	static const FString SetObjectMute = TEXT("setObjectMute");

	// Venue actions
	static const FString RecallScene = TEXT("recallScene");
	static const FString StoreScene = TEXT("storeScene");
	static const FString SetMasterGain = TEXT("setMasterGain");
}

/**
 * Myko emitter IDs for spatial audio targets.
 */
namespace SpatialAudioMykoEmitters
{
	// Speaker emitters
	static const FString SpeakerLevel = TEXT("speakerLevel");
	static const FString SpeakerGainReduction = TEXT("speakerGainReduction");
	static const FString SpeakerConfig = TEXT("speakerConfig");

	// Zone emitters
	static const FString ZoneConfig = TEXT("zoneConfig");
	static const FString ZoneActive = TEXT("zoneActive");

	// Object emitters
	static const FString ObjectPosition = TEXT("objectPosition");
	static const FString ObjectLevel = TEXT("objectLevel");
	static const FString ObjectConfig = TEXT("objectConfig");

	// Venue emitters
	static const FString VenueConfig = TEXT("venueConfig");
	static const FString VenueStatus = TEXT("venueStatus");
}

/**
 * Helper to convert spatial audio entities to JSON for Myko.
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialAudioMykoSerializer
{
public:
	// Serialize venue to JSON
	static TSharedPtr<FJsonObject> VenueToJson(const FSpatialVenue& Venue);

	// Serialize speaker to JSON
	static TSharedPtr<FJsonObject> SpeakerToJson(const FSpatialSpeaker& Speaker, const FGuid& VenueId);

	// Serialize zone to JSON
	static TSharedPtr<FJsonObject> ZoneToJson(const FSpatialZone& Zone, const FGuid& VenueId);

	// Serialize audio object to JSON
	static TSharedPtr<FJsonObject> ObjectToJson(const FSpatialAudioObject& Object, const FGuid& VenueId);

	// Serialize meter reading to JSON (for pulse)
	static TSharedPtr<FJsonObject> MeterToJson(const FGuid& EntityId, const FSpatialMeterReading& Meter);

	// Serialize gain reduction to JSON (for pulse)
	static TSharedPtr<FJsonObject> GainReductionToJson(const FGuid& SpeakerId, float GainReductionDb);

	// Serialize position to JSON (for pulse)
	static TSharedPtr<FJsonObject> PositionToJson(const FGuid& ObjectId, const FVector& Position);

	// Serialize EQ bands to JSON array
	static TSharedPtr<FJsonValue> EQBandsToJson(const TArray<FSpatialEQBand>& Bands);

	// Serialize limiter settings to JSON
	static TSharedPtr<FJsonObject> LimiterToJson(const FSpatialLimiterSettings& Limiter);

	// Parse speaker update from JSON
	static bool ParseSpeakerUpdate(const TSharedPtr<FJsonObject>& Json, FSpatialSpeaker& OutSpeaker);

	// Parse zone update from JSON
	static bool ParseZoneUpdate(const TSharedPtr<FJsonObject>& Json, FSpatialZone& OutZone);

	// Parse object update from JSON
	static bool ParseObjectUpdate(const TSharedPtr<FJsonObject>& Json, FSpatialAudioObject& OutObject);

	// Parse EQ bands from JSON array
	static bool ParseEQBands(const TSharedPtr<FJsonValue>& Json, TArray<FSpatialEQBand>& OutBands);

	// Parse limiter settings from JSON
	static bool ParseLimiter(const TSharedPtr<FJsonObject>& Json, FSpatialLimiterSettings& OutLimiter);

private:
	// Helper to serialize FVector to JSON
	static TSharedPtr<FJsonObject> VectorToJson(const FVector& Vec);

	// Helper to parse FVector from JSON
	static bool ParseVector(const TSharedPtr<FJsonObject>& Json, FVector& OutVec);

	// Helper to serialize FBox to JSON
	static TSharedPtr<FJsonObject> BoxToJson(const FBox& Box);

	// Helper to parse FBox from JSON
	static bool ParseBox(const TSharedPtr<FJsonObject>& Json, FBox& OutBox);
};

/**
 * Schema definitions for spatial audio Myko entities.
 * These define the property types for actions and emitters.
 */
namespace SpatialAudioMykoSchema
{
	// Common property names
	static const FString PropId = TEXT("id");
	static const FString PropName = TEXT("name");
	static const FString PropVenueId = TEXT("venueId");
	static const FString PropPosition = TEXT("position");
	static const FString PropGain = TEXT("gain");
	static const FString PropDelay = TEXT("delay");
	static const FString PropMute = TEXT("mute");
	static const FString PropPolarity = TEXT("polarity");
	static const FString PropSpread = TEXT("spread");
	static const FString PropLevel = TEXT("level");
	static const FString PropPeak = TEXT("peak");
	static const FString PropRMS = TEXT("rms");
	static const FString PropGainReduction = TEXT("gainReduction");

	// Position sub-properties
	static const FString PropX = TEXT("x");
	static const FString PropY = TEXT("y");
	static const FString PropZ = TEXT("z");

	// Speaker-specific
	static const FString PropChannel = TEXT("channel");
	static const FString PropArrayId = TEXT("arrayId");
	static const FString PropType = TEXT("type");
	static const FString PropEQ = TEXT("eq");
	static const FString PropLimiter = TEXT("limiter");
	static const FString PropHighPass = TEXT("highPass");
	static const FString PropLowPass = TEXT("lowPass");

	// EQ band properties
	static const FString PropFrequency = TEXT("frequency");
	static const FString PropQ = TEXT("q");
	static const FString PropEnabled = TEXT("enabled");
	static const FString PropBandType = TEXT("bandType");

	// Limiter properties
	static const FString PropThreshold = TEXT("threshold");
	static const FString PropAttack = TEXT("attack");
	static const FString PropRelease = TEXT("release");
	static const FString PropKnee = TEXT("knee");
	static const FString PropCeiling = TEXT("ceiling");

	// Zone-specific
	static const FString PropRenderer = TEXT("renderer");
	static const FString PropSpeakers = TEXT("speakers");
	static const FString PropBounds = TEXT("bounds");
	static const FString PropPriority = TEXT("priority");

	// Object-specific
	static const FString PropRouting = TEXT("routing");
	static const FString PropBoundActor = TEXT("boundActor");
}
