// Copyright Rocketship. All Rights Reserved.

#include "Myko/SpatialAudioMykoTypes.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::VenueToJson(const FSpatialVenue& Venue)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

	Json->SetStringField(SpatialAudioMykoSchema::PropId, Venue.Id.ToString());
	Json->SetStringField(SpatialAudioMykoSchema::PropName, Venue.Name);

	// Serialize reference point
	Json->SetObjectField(TEXT("referencePoint"), VectorToJson(Venue.ReferencePoint));

	// Speaker/zone/array counts for overview
	Json->SetNumberField(TEXT("speakerCount"), Venue.GetSpeakerCount());
	Json->SetNumberField(TEXT("zoneCount"), Venue.GetZoneCount());
	Json->SetNumberField(TEXT("arrayCount"), Venue.GetArrayCount());

	return Json;
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::SpeakerToJson(const FSpatialSpeaker& Speaker, const FGuid& VenueId)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

	// Core identity
	Json->SetStringField(SpatialAudioMykoSchema::PropId, Speaker.Id.ToString());
	Json->SetStringField(SpatialAudioMykoSchema::PropVenueId, VenueId.ToString());
	Json->SetStringField(SpatialAudioMykoSchema::PropName, Speaker.Name);

	// Position
	Json->SetObjectField(SpatialAudioMykoSchema::PropPosition, VectorToJson(Speaker.Position));

	// Routing
	Json->SetNumberField(SpatialAudioMykoSchema::PropChannel, Speaker.OutputChannel);
	if (Speaker.ArrayId.IsValid())
	{
		Json->SetStringField(SpatialAudioMykoSchema::PropArrayId, Speaker.ArrayId.ToString());
	}

	// Type
	Json->SetStringField(SpatialAudioMykoSchema::PropType, StaticEnum<ESpatialSpeakerType>()->GetNameStringByValue(static_cast<int64>(Speaker.Type)));

	// DSP state
	TSharedPtr<FJsonObject> DSPJson = MakeShareable(new FJsonObject());
	DSPJson->SetNumberField(TEXT("inputGain"), Speaker.DSP.InputGainDb);
	DSPJson->SetNumberField(TEXT("outputGain"), Speaker.DSP.OutputGainDb);
	DSPJson->SetNumberField(SpatialAudioMykoSchema::PropDelay, Speaker.DSP.DelayMs);
	DSPJson->SetBoolField(SpatialAudioMykoSchema::PropMute, Speaker.DSP.bMuted);
	DSPJson->SetBoolField(SpatialAudioMykoSchema::PropPolarity, Speaker.DSP.bPolarityInvert);

	// EQ bands
	DSPJson->SetField(SpatialAudioMykoSchema::PropEQ, EQBandsToJson(Speaker.DSP.EQBands));

	// Limiter
	DSPJson->SetObjectField(SpatialAudioMykoSchema::PropLimiter, LimiterToJson(Speaker.DSP.Limiter));

	Json->SetObjectField(TEXT("dsp"), DSPJson);

	return Json;
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::ZoneToJson(const FSpatialZone& Zone, const FGuid& VenueId)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

	Json->SetStringField(SpatialAudioMykoSchema::PropId, Zone.Id.ToString());
	Json->SetStringField(SpatialAudioMykoSchema::PropVenueId, VenueId.ToString());
	Json->SetStringField(SpatialAudioMykoSchema::PropName, Zone.Name);

	// Renderer type
	Json->SetStringField(SpatialAudioMykoSchema::PropRenderer,
		StaticEnum<ESpatialRendererType>()->GetNameStringByValue(static_cast<int64>(Zone.RendererType)));

	// Bounds
	Json->SetObjectField(SpatialAudioMykoSchema::PropBounds, BoxToJson(Zone.Bounds));

	// Priority
	Json->SetNumberField(SpatialAudioMykoSchema::PropPriority, Zone.Priority);

	// Speaker IDs
	TArray<TSharedPtr<FJsonValue>> SpeakerArray;
	for (const FGuid& SpeakerId : Zone.SpeakerIds)
	{
		SpeakerArray.Add(MakeShareable(new FJsonValueString(SpeakerId.ToString())));
	}
	Json->SetArrayField(SpatialAudioMykoSchema::PropSpeakers, SpeakerArray);

	return Json;
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::ObjectToJson(const FSpatialAudioObject& Object, const FGuid& VenueId)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

	Json->SetStringField(SpatialAudioMykoSchema::PropId, Object.Id.ToString());
	Json->SetStringField(SpatialAudioMykoSchema::PropVenueId, VenueId.ToString());
	Json->SetStringField(SpatialAudioMykoSchema::PropName, Object.Name);

	// Position
	Json->SetObjectField(SpatialAudioMykoSchema::PropPosition, VectorToJson(Object.Position));

	// Parameters
	Json->SetNumberField(SpatialAudioMykoSchema::PropSpread, Object.Spread);
	Json->SetNumberField(SpatialAudioMykoSchema::PropGain, Object.GainDb);
	Json->SetBoolField(SpatialAudioMykoSchema::PropMute, Object.bMuted);

	// Zone routing
	TArray<TSharedPtr<FJsonValue>> RoutingArray;
	for (const FGuid& ZoneId : Object.ZoneRouting)
	{
		RoutingArray.Add(MakeShareable(new FJsonValueString(ZoneId.ToString())));
	}
	Json->SetArrayField(SpatialAudioMykoSchema::PropRouting, RoutingArray);

	// Bound actor path if any
	if (Object.BoundActor.IsValid())
	{
		Json->SetStringField(SpatialAudioMykoSchema::PropBoundActor, Object.BoundActor->GetPathName());
	}

	return Json;
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::MeterToJson(const FGuid& EntityId, const FSpatialMeterReading& Meter)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

	Json->SetStringField(SpatialAudioMykoSchema::PropId, EntityId.ToString());
	Json->SetNumberField(SpatialAudioMykoSchema::PropPeak, Meter.PeakDb);
	Json->SetNumberField(SpatialAudioMykoSchema::PropRMS, Meter.RMSDb);

	return Json;
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::GainReductionToJson(const FGuid& SpeakerId, float GainReductionDb)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

	Json->SetStringField(SpatialAudioMykoSchema::PropId, SpeakerId.ToString());
	Json->SetNumberField(SpatialAudioMykoSchema::PropGainReduction, GainReductionDb);

	return Json;
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::PositionToJson(const FGuid& ObjectId, const FVector& Position)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

	Json->SetStringField(SpatialAudioMykoSchema::PropId, ObjectId.ToString());
	Json->SetObjectField(SpatialAudioMykoSchema::PropPosition, VectorToJson(Position));

	return Json;
}

TSharedPtr<FJsonValue> FSpatialAudioMykoSerializer::EQBandsToJson(const TArray<FSpatialEQBand>& Bands)
{
	TArray<TSharedPtr<FJsonValue>> Array;

	for (const FSpatialEQBand& Band : Bands)
	{
		TSharedPtr<FJsonObject> BandJson = MakeShareable(new FJsonObject());
		BandJson->SetBoolField(SpatialAudioMykoSchema::PropEnabled, Band.bEnabled);
		BandJson->SetStringField(SpatialAudioMykoSchema::PropBandType,
			StaticEnum<ESpatialEQBandType>()->GetNameStringByValue(static_cast<int64>(Band.Type)));
		BandJson->SetNumberField(SpatialAudioMykoSchema::PropFrequency, Band.FrequencyHz);
		BandJson->SetNumberField(SpatialAudioMykoSchema::PropGain, Band.GainDb);
		BandJson->SetNumberField(SpatialAudioMykoSchema::PropQ, Band.Q);

		Array.Add(MakeShareable(new FJsonValueObject(BandJson)));
	}

	return MakeShareable(new FJsonValueArray(Array));
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::LimiterToJson(const FSpatialLimiterSettings& Limiter)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

	Json->SetBoolField(SpatialAudioMykoSchema::PropEnabled, Limiter.bEnabled);
	Json->SetNumberField(SpatialAudioMykoSchema::PropThreshold, Limiter.ThresholdDb);
	Json->SetNumberField(SpatialAudioMykoSchema::PropAttack, Limiter.AttackMs);
	Json->SetNumberField(SpatialAudioMykoSchema::PropRelease, Limiter.ReleaseMs);
	Json->SetNumberField(SpatialAudioMykoSchema::PropKnee, Limiter.KneeDb);
	Json->SetNumberField(SpatialAudioMykoSchema::PropCeiling, Limiter.CeilingDb);

	return Json;
}

bool FSpatialAudioMykoSerializer::ParseSpeakerUpdate(const TSharedPtr<FJsonObject>& Json, FSpatialSpeaker& OutSpeaker)
{
	if (!Json.IsValid())
	{
		return false;
	}

	// Parse name if present
	if (Json->HasField(SpatialAudioMykoSchema::PropName))
	{
		OutSpeaker.Name = Json->GetStringField(SpatialAudioMykoSchema::PropName);
	}

	// Parse position if present
	if (Json->HasField(SpatialAudioMykoSchema::PropPosition))
	{
		ParseVector(Json->GetObjectField(SpatialAudioMykoSchema::PropPosition), OutSpeaker.Position);
	}

	// Parse channel if present
	if (Json->HasField(SpatialAudioMykoSchema::PropChannel))
	{
		OutSpeaker.OutputChannel = static_cast<int32>(Json->GetNumberField(SpatialAudioMykoSchema::PropChannel));
	}

	// Parse DSP if present
	if (Json->HasField(TEXT("dsp")))
	{
		TSharedPtr<FJsonObject> DSPJson = Json->GetObjectField(TEXT("dsp"));

		if (DSPJson->HasField(TEXT("inputGain")))
		{
			OutSpeaker.DSP.InputGainDb = DSPJson->GetNumberField(TEXT("inputGain"));
		}
		if (DSPJson->HasField(TEXT("outputGain")))
		{
			OutSpeaker.DSP.OutputGainDb = DSPJson->GetNumberField(TEXT("outputGain"));
		}
		if (DSPJson->HasField(SpatialAudioMykoSchema::PropDelay))
		{
			OutSpeaker.DSP.DelayMs = DSPJson->GetNumberField(SpatialAudioMykoSchema::PropDelay);
		}
		if (DSPJson->HasField(SpatialAudioMykoSchema::PropMute))
		{
			OutSpeaker.DSP.bMuted = DSPJson->GetBoolField(SpatialAudioMykoSchema::PropMute);
		}
		if (DSPJson->HasField(SpatialAudioMykoSchema::PropPolarity))
		{
			OutSpeaker.DSP.bPolarityInvert = DSPJson->GetBoolField(SpatialAudioMykoSchema::PropPolarity);
		}
		if (DSPJson->HasField(SpatialAudioMykoSchema::PropEQ))
		{
			ParseEQBands(DSPJson->TryGetField(SpatialAudioMykoSchema::PropEQ), OutSpeaker.DSP.EQBands);
		}
		if (DSPJson->HasField(SpatialAudioMykoSchema::PropLimiter))
		{
			ParseLimiter(DSPJson->GetObjectField(SpatialAudioMykoSchema::PropLimiter), OutSpeaker.DSP.Limiter);
		}
	}

	return true;
}

bool FSpatialAudioMykoSerializer::ParseZoneUpdate(const TSharedPtr<FJsonObject>& Json, FSpatialZone& OutZone)
{
	if (!Json.IsValid())
	{
		return false;
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropName))
	{
		OutZone.Name = Json->GetStringField(SpatialAudioMykoSchema::PropName);
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropRenderer))
	{
		FString RendererStr = Json->GetStringField(SpatialAudioMykoSchema::PropRenderer);
		int64 EnumValue;
		if (StaticEnum<ESpatialRendererType>()->FindEnumValueFromString(RendererStr, EnumValue))
		{
			OutZone.RendererType = static_cast<ESpatialRendererType>(EnumValue);
		}
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropBounds))
	{
		ParseBox(Json->GetObjectField(SpatialAudioMykoSchema::PropBounds), OutZone.Bounds);
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropPriority))
	{
		OutZone.Priority = static_cast<int32>(Json->GetNumberField(SpatialAudioMykoSchema::PropPriority));
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropSpeakers))
	{
		OutZone.SpeakerIds.Empty();
		const TArray<TSharedPtr<FJsonValue>>& SpeakerArray = Json->GetArrayField(SpatialAudioMykoSchema::PropSpeakers);
		for (const TSharedPtr<FJsonValue>& Value : SpeakerArray)
		{
			FGuid SpeakerId;
			if (FGuid::Parse(Value->AsString(), SpeakerId))
			{
				OutZone.SpeakerIds.Add(SpeakerId);
			}
		}
	}

	return true;
}

bool FSpatialAudioMykoSerializer::ParseObjectUpdate(const TSharedPtr<FJsonObject>& Json, FSpatialAudioObject& OutObject)
{
	if (!Json.IsValid())
	{
		return false;
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropName))
	{
		OutObject.Name = Json->GetStringField(SpatialAudioMykoSchema::PropName);
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropPosition))
	{
		ParseVector(Json->GetObjectField(SpatialAudioMykoSchema::PropPosition), OutObject.Position);
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropSpread))
	{
		OutObject.Spread = Json->GetNumberField(SpatialAudioMykoSchema::PropSpread);
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropGain))
	{
		OutObject.GainDb = Json->GetNumberField(SpatialAudioMykoSchema::PropGain);
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropMute))
	{
		OutObject.bMuted = Json->GetBoolField(SpatialAudioMykoSchema::PropMute);
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropRouting))
	{
		OutObject.ZoneRouting.Empty();
		const TArray<TSharedPtr<FJsonValue>>& RoutingArray = Json->GetArrayField(SpatialAudioMykoSchema::PropRouting);
		for (const TSharedPtr<FJsonValue>& Value : RoutingArray)
		{
			FGuid ZoneId;
			if (FGuid::Parse(Value->AsString(), ZoneId))
			{
				OutObject.ZoneRouting.Add(ZoneId);
			}
		}
	}

	return true;
}

bool FSpatialAudioMykoSerializer::ParseEQBands(const TSharedPtr<FJsonValue>& Json, TArray<FSpatialEQBand>& OutBands)
{
	if (!Json.IsValid() || Json->Type != EJson::Array)
	{
		return false;
	}

	OutBands.Empty();
	const TArray<TSharedPtr<FJsonValue>>& Array = Json->AsArray();

	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		if (Value->Type != EJson::Object)
		{
			continue;
		}

		TSharedPtr<FJsonObject> BandJson = Value->AsObject();
		FSpatialEQBand Band;

		if (BandJson->HasField(SpatialAudioMykoSchema::PropEnabled))
		{
			Band.bEnabled = BandJson->GetBoolField(SpatialAudioMykoSchema::PropEnabled);
		}
		if (BandJson->HasField(SpatialAudioMykoSchema::PropBandType))
		{
			FString TypeStr = BandJson->GetStringField(SpatialAudioMykoSchema::PropBandType);
			int64 EnumValue;
			if (StaticEnum<ESpatialEQBandType>()->FindEnumValueFromString(TypeStr, EnumValue))
			{
				Band.Type = static_cast<ESpatialEQBandType>(EnumValue);
			}
		}
		if (BandJson->HasField(SpatialAudioMykoSchema::PropFrequency))
		{
			Band.FrequencyHz = BandJson->GetNumberField(SpatialAudioMykoSchema::PropFrequency);
		}
		if (BandJson->HasField(SpatialAudioMykoSchema::PropGain))
		{
			Band.GainDb = BandJson->GetNumberField(SpatialAudioMykoSchema::PropGain);
		}
		if (BandJson->HasField(SpatialAudioMykoSchema::PropQ))
		{
			Band.Q = BandJson->GetNumberField(SpatialAudioMykoSchema::PropQ);
		}

		OutBands.Add(Band);
	}

	return true;
}

bool FSpatialAudioMykoSerializer::ParseLimiter(const TSharedPtr<FJsonObject>& Json, FSpatialLimiterSettings& OutLimiter)
{
	if (!Json.IsValid())
	{
		return false;
	}

	if (Json->HasField(SpatialAudioMykoSchema::PropEnabled))
	{
		OutLimiter.bEnabled = Json->GetBoolField(SpatialAudioMykoSchema::PropEnabled);
	}
	if (Json->HasField(SpatialAudioMykoSchema::PropThreshold))
	{
		OutLimiter.ThresholdDb = Json->GetNumberField(SpatialAudioMykoSchema::PropThreshold);
	}
	if (Json->HasField(SpatialAudioMykoSchema::PropAttack))
	{
		OutLimiter.AttackMs = Json->GetNumberField(SpatialAudioMykoSchema::PropAttack);
	}
	if (Json->HasField(SpatialAudioMykoSchema::PropRelease))
	{
		OutLimiter.ReleaseMs = Json->GetNumberField(SpatialAudioMykoSchema::PropRelease);
	}
	if (Json->HasField(SpatialAudioMykoSchema::PropKnee))
	{
		OutLimiter.KneeDb = Json->GetNumberField(SpatialAudioMykoSchema::PropKnee);
	}
	if (Json->HasField(SpatialAudioMykoSchema::PropCeiling))
	{
		OutLimiter.CeilingDb = Json->GetNumberField(SpatialAudioMykoSchema::PropCeiling);
	}

	return true;
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::VectorToJson(const FVector& Vec)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());
	Json->SetNumberField(SpatialAudioMykoSchema::PropX, Vec.X);
	Json->SetNumberField(SpatialAudioMykoSchema::PropY, Vec.Y);
	Json->SetNumberField(SpatialAudioMykoSchema::PropZ, Vec.Z);
	return Json;
}

bool FSpatialAudioMykoSerializer::ParseVector(const TSharedPtr<FJsonObject>& Json, FVector& OutVec)
{
	if (!Json.IsValid())
	{
		return false;
	}

	OutVec.X = Json->GetNumberField(SpatialAudioMykoSchema::PropX);
	OutVec.Y = Json->GetNumberField(SpatialAudioMykoSchema::PropY);
	OutVec.Z = Json->GetNumberField(SpatialAudioMykoSchema::PropZ);

	return true;
}

TSharedPtr<FJsonObject> FSpatialAudioMykoSerializer::BoxToJson(const FBox& Box)
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());
	Json->SetObjectField(TEXT("min"), VectorToJson(Box.Min));
	Json->SetObjectField(TEXT("max"), VectorToJson(Box.Max));
	return Json;
}

bool FSpatialAudioMykoSerializer::ParseBox(const TSharedPtr<FJsonObject>& Json, FBox& OutBox)
{
	if (!Json.IsValid())
	{
		return false;
	}

	if (Json->HasField(TEXT("min")))
	{
		ParseVector(Json->GetObjectField(TEXT("min")), OutBox.Min);
	}
	if (Json->HasField(TEXT("max")))
	{
		ParseVector(Json->GetObjectField(TEXT("max")), OutBox.Max);
	}

	return true;
}
