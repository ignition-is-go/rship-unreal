// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialAudioTypes.h"
#include "SpatialSpeaker.h"
#include "SpatialZone.h"
#include "SpatialVenue.generated.h"

/**
 * Represents a complete venue/installation with all speakers, arrays, and zones.
 * This is the top-level container for the spatial audio configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialVenue
{
	GENERATED_BODY()

	// ========================================================================
	// IDENTIFICATION
	// ========================================================================

	/** Unique identifier for this venue */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue")
	FGuid Id;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue")
	FString Name;

	/** Optional description */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue")
	FString Description;

	// ========================================================================
	// COORDINATE SYSTEM
	// ========================================================================

	/** Origin offset for venue coordinates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue")
	FVector VenueOrigin = FVector::ZeroVector;

	/** Scale factor (Unreal units per meter, default 100 = cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
	float UnitsPerMeter = 100.0f;

	/** Forward direction convention */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue")
	FVector ForwardVector = FVector::ForwardVector;

	// ========================================================================
	// ENTITIES
	// ========================================================================

	/** All speakers in the venue, keyed by ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue")
	TMap<FGuid, FSpatialSpeaker> Speakers;

	/** All speaker arrays in the venue, keyed by ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue")
	TMap<FGuid, FSpatialSpeakerArray> Arrays;

	/** All zones in the venue, keyed by ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue")
	TMap<FGuid, FSpatialZone> Zones;

	// ========================================================================
	// GLOBAL SETTINGS
	// ========================================================================

	/** Sample rate for audio processing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue", meta = (ClampMin = "44100", ClampMax = "192000"))
	int32 SampleRate = 48000;

	/** Buffer size in samples */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue", meta = (ClampMin = "32", ClampMax = "4096"))
	int32 BufferSize = 512;

	/** Total number of output channels configured */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|Venue", meta = (ClampMin = "2", ClampMax = "512"))
	int32 OutputChannelCount = 64;

	// ========================================================================
	// METHODS
	// ========================================================================

	FSpatialVenue()
	{
		Id = FGuid::NewGuid();
	}

	// ------------------------------------------------------------------------
	// SPEAKER MANAGEMENT
	// ------------------------------------------------------------------------

	/** Add a speaker to the venue */
	FGuid AddSpeaker(const FSpatialSpeaker& Speaker)
	{
		FGuid NewId = Speaker.Id.IsValid() ? Speaker.Id : FGuid::NewGuid();
		FSpatialSpeaker NewSpeaker = Speaker;
		NewSpeaker.Id = NewId;
		Speakers.Add(NewId, NewSpeaker);
		return NewId;
	}

	/** Get a speaker by ID */
	FSpatialSpeaker* GetSpeaker(const FGuid& SpeakerId)
	{
		return Speakers.Find(SpeakerId);
	}

	const FSpatialSpeaker* GetSpeaker(const FGuid& SpeakerId) const
	{
		return Speakers.Find(SpeakerId);
	}

	/** Remove a speaker by ID */
	bool RemoveSpeaker(const FGuid& SpeakerId)
	{
		// Also remove from any arrays
		for (auto& ArrayPair : Arrays)
		{
			ArrayPair.Value.SpeakerIds.Remove(SpeakerId);
		}
		// Also remove from any zones
		for (auto& ZonePair : Zones)
		{
			ZonePair.Value.SpeakerIds.Remove(SpeakerId);
		}
		return Speakers.Remove(SpeakerId) > 0;
	}

	// ------------------------------------------------------------------------
	// ARRAY MANAGEMENT
	// ------------------------------------------------------------------------

	/** Add an array to the venue */
	FGuid AddArray(const FSpatialSpeakerArray& Array)
	{
		FGuid NewId = Array.Id.IsValid() ? Array.Id : FGuid::NewGuid();
		FSpatialSpeakerArray NewArray = Array;
		NewArray.Id = NewId;
		Arrays.Add(NewId, NewArray);
		return NewId;
	}

	/** Get an array by ID */
	FSpatialSpeakerArray* GetArray(const FGuid& ArrayId)
	{
		return Arrays.Find(ArrayId);
	}

	const FSpatialSpeakerArray* GetArray(const FGuid& ArrayId) const
	{
		return Arrays.Find(ArrayId);
	}

	/** Remove an array by ID */
	bool RemoveArray(const FGuid& ArrayId)
	{
		// Clear parent array reference from speakers
		if (FSpatialSpeakerArray* Array = Arrays.Find(ArrayId))
		{
			for (const FGuid& SpeakerId : Array->SpeakerIds)
			{
				if (FSpatialSpeaker* Speaker = Speakers.Find(SpeakerId))
				{
					Speaker->ParentArrayId.Invalidate();
				}
			}
		}
		// Remove from zones
		for (auto& ZonePair : Zones)
		{
			ZonePair.Value.ArrayIds.Remove(ArrayId);
		}
		return Arrays.Remove(ArrayId) > 0;
	}

	// ------------------------------------------------------------------------
	// ZONE MANAGEMENT
	// ------------------------------------------------------------------------

	/** Add a zone to the venue */
	FGuid AddZone(const FSpatialZone& Zone)
	{
		FGuid NewId = Zone.Id.IsValid() ? Zone.Id : FGuid::NewGuid();
		FSpatialZone NewZone = Zone;
		NewZone.Id = NewId;
		Zones.Add(NewId, NewZone);
		return NewId;
	}

	/** Get a zone by ID */
	FSpatialZone* GetZone(const FGuid& ZoneId)
	{
		return Zones.Find(ZoneId);
	}

	const FSpatialZone* GetZone(const FGuid& ZoneId) const
	{
		return Zones.Find(ZoneId);
	}

	/** Remove a zone by ID */
	bool RemoveZone(const FGuid& ZoneId)
	{
		// Clear zone reference from speakers
		for (auto& SpeakerPair : Speakers)
		{
			if (SpeakerPair.Value.ZoneId == ZoneId)
			{
				SpeakerPair.Value.ZoneId.Invalidate();
			}
		}
		return Zones.Remove(ZoneId) > 0;
	}

	// ------------------------------------------------------------------------
	// QUERIES
	// ------------------------------------------------------------------------

	/** Get all speakers in a zone (including those in arrays) */
	TArray<FGuid> GetAllSpeakersInZone(const FGuid& ZoneId) const
	{
		TArray<FGuid> Result;
		const FSpatialZone* Zone = Zones.Find(ZoneId);
		if (!Zone) return Result;

		// Add direct speakers
		Result.Append(Zone->SpeakerIds);

		// Add speakers from arrays
		for (const FGuid& ArrayId : Zone->ArrayIds)
		{
			if (const FSpatialSpeakerArray* Array = Arrays.Find(ArrayId))
			{
				Result.Append(Array->SpeakerIds);
			}
		}

		return Result;
	}

	/** Get all speakers as a flat array */
	TArray<FSpatialSpeaker> GetAllSpeakers() const
	{
		TArray<FSpatialSpeaker> Result;
		for (const auto& Pair : Speakers)
		{
			Result.Add(Pair.Value);
		}
		return Result;
	}

	/** Get total speaker count */
	int32 GetSpeakerCount() const
	{
		return Speakers.Num();
	}

	/** Get total array count */
	int32 GetArrayCount() const
	{
		return Arrays.Num();
	}

	/** Get total zone count */
	int32 GetZoneCount() const
	{
		return Zones.Num();
	}

	/** Convert world position to venue-local position */
	FVector WorldToVenue(const FVector& WorldPos) const
	{
		return (WorldPos - VenueOrigin);
	}

	/** Convert venue-local position to world position */
	FVector VenueToWorld(const FVector& VenuePos) const
	{
		return VenuePos + VenueOrigin;
	}

	/** Convert Unreal units to meters */
	float UnitsToMeters(float Units) const
	{
		return Units / UnitsPerMeter;
	}

	/** Convert meters to Unreal units */
	float MetersToUnits(float Meters) const
	{
		return Meters * UnitsPerMeter;
	}

	// ------------------------------------------------------------------------
	// VALIDATION
	// ------------------------------------------------------------------------

	/** Validate venue configuration and return any errors */
	TArray<FString> Validate() const
	{
		TArray<FString> Errors;

		// Check for duplicate output channels
		TMap<int32, FGuid> ChannelUsage;
		for (const auto& SpeakerPair : Speakers)
		{
			int32 Channel = SpeakerPair.Value.OutputChannel;
			if (const FGuid* ExistingId = ChannelUsage.Find(Channel))
			{
				Errors.Add(FString::Printf(TEXT("Output channel %d used by multiple speakers"), Channel));
			}
			else
			{
				ChannelUsage.Add(Channel, SpeakerPair.Key);
			}

			// Check channel range
			if (Channel < 0 || Channel >= OutputChannelCount)
			{
				Errors.Add(FString::Printf(TEXT("Speaker '%s' uses invalid output channel %d (max: %d)"),
					*SpeakerPair.Value.Name, Channel, OutputChannelCount - 1));
			}
		}

		// Check for orphaned array references
		for (const auto& ArrayPair : Arrays)
		{
			for (const FGuid& SpeakerId : ArrayPair.Value.SpeakerIds)
			{
				if (!Speakers.Contains(SpeakerId))
				{
					Errors.Add(FString::Printf(TEXT("Array '%s' references non-existent speaker"),
						*ArrayPair.Value.Name));
				}
			}
		}

		// Check for orphaned zone references
		for (const auto& ZonePair : Zones)
		{
			for (const FGuid& ArrayId : ZonePair.Value.ArrayIds)
			{
				if (!Arrays.Contains(ArrayId))
				{
					Errors.Add(FString::Printf(TEXT("Zone '%s' references non-existent array"),
						*ZonePair.Value.Name));
				}
			}
			for (const FGuid& SpeakerId : ZonePair.Value.SpeakerIds)
			{
				if (!Speakers.Contains(SpeakerId))
				{
					Errors.Add(FString::Printf(TEXT("Zone '%s' references non-existent speaker"),
						*ZonePair.Value.Name));
				}
			}
		}

		return Errors;
	}
};
