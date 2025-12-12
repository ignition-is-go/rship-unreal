// Copyright Rocketship. All Rights Reserved.

#include "RshipSpatialAudioManager.h"
#include "Myko/SpatialAudioMykoTypes.h"
#include "RshipSpatialAudioRuntimeModule.h"
#include "RshipSubsystem.h"
#include "Audio/SpatialAudioProcessor.h"
#include "Audio/SpatialRenderingEngine.h"
#include "DSP/SpatialSpeakerDSP.h"
#include "ExternalProcessor/IExternalSpatialProcessor.h"
#include "ExternalProcessor/ExternalProcessorRegistry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY(LogRshipSpatialAudioManager);

URshipSpatialAudioManager::URshipSpatialAudioManager()
	: Subsystem(nullptr)
	, MeterUpdateAccumulator(0.0f)
	, bMykoRegistered(false)
	, AudioProcessor(nullptr)
	, RenderingEngine(nullptr)
	, CurrentRendererType(ESpatialRendererType::VBAP)
	, ExternalProcessor(nullptr)
	, bExternalProcessorForwardingEnabled(false)
	, bSceneInterpolationActive(false)
	, SceneInterpolationDuration(0.0f)
	, SceneInterpolationElapsed(0.0f)
{
}

void URshipSpatialAudioManager::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("SpatialAudioManager initialized"));

	// Create a default venue if none exists
	if (!Venue.Id.IsValid())
	{
		CreateVenue(TEXT("Default Venue"));
	}
}

void URshipSpatialAudioManager::Shutdown()
{
	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("SpatialAudioManager shutting down"));

	// Disconnect external processor
	DisconnectExternalProcessor();
	ExternalProcessor = nullptr;

	UnregisterMykoTargets();
	AudioObjects.Empty();
	StoredScenes.Empty();

	Subsystem = nullptr;
}

void URshipSpatialAudioManager::Tick(float DeltaTime)
{
	// Update scene interpolation if active
	if (bSceneInterpolationActive)
	{
		UpdateSceneInterpolation(DeltaTime);
	}

	// Update audio objects from bound actors
	for (auto& Pair : AudioObjects)
	{
		FSpatialAudioObject& Object = Pair.Value;
		if (Object.bFollowBoundActor && Object.BoundActor.IsValid())
		{
			FVector OldPosition = Object.Position;
			Object.UpdateFromBoundActor();
			if (!OldPosition.Equals(Object.Position, 0.1f))
			{
				NotifyObjectChange(Object.Id);
				OnObjectPositionChanged.Broadcast(Object.Id, Object.Position);
			}
		}
	}

	// Accumulate time for meter updates
	MeterUpdateAccumulator += DeltaTime;
	if (MeterUpdateAccumulator >= MeterUpdateInterval)
	{
		MeterUpdateAccumulator -= MeterUpdateInterval;
		SendMeterPulses();
	}

	// Update audio engine with any pending changes
	UpdateAudioEngine();
}

// ============================================================================
// VENUE MANAGEMENT
// ============================================================================

void URshipSpatialAudioManager::CreateVenue(const FString& VenueName)
{
	// Clear existing venue
	UnregisterMykoTargets();

	// Create new venue
	Venue = FSpatialVenue();
	Venue.Name = VenueName;

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Created venue: %s (ID: %s)"),
		*VenueName, *Venue.Id.ToString());

	// Register with Myko
	RegisterMykoTargets();

	OnVenueChanged.Broadcast();
}

// ============================================================================
// SPEAKER MANAGEMENT
// ============================================================================

FGuid URshipSpatialAudioManager::AddSpeaker(const FSpatialSpeaker& Speaker)
{
	FGuid NewId = Venue.AddSpeaker(Speaker);

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Added speaker: %s (ID: %s, Channel: %d)"),
		*Speaker.Name, *NewId.ToString(), Speaker.OutputChannel);

	// Get the speaker with assigned ID for registration
	if (const FSpatialSpeaker* AddedSpeaker = Venue.GetSpeaker(NewId))
	{
		RegisterSpeakerTarget(*AddedSpeaker);
		CachedSpeakerIds.Add(NewId);

		// Register with audio processor DSP manager
		if (AudioProcessor)
		{
			FSpatialSpeakerDSPManager* DSPManager = AudioProcessor->GetDSPManager();
			if (DSPManager)
			{
				DSPManager->AddSpeaker(NewId);
			}

			// Rebuild index mapping and apply initial DSP config
			RebuildSpeakerIndexMapping();
			FSpatialSpeakerDSPConfig Config = BuildDSPConfig(*AddedSpeaker);
			AudioProcessor->ApplySpeakerDSPConfig(NewId, Config);
		}

		// Sync speakers to rendering engine for VBAP/DBAP triangulation update
		SyncSpeakersToRenderingEngine();
	}

	OnSpeakerAdded.Broadcast(NewId);

	return NewId;
}

bool URshipSpatialAudioManager::UpdateSpeaker(const FGuid& SpeakerId, const FSpatialSpeaker& Speaker)
{
	FSpatialSpeaker* ExistingSpeaker = Venue.GetSpeaker(SpeakerId);
	if (!ExistingSpeaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("UpdateSpeaker: Speaker not found: %s"), *SpeakerId.ToString());
		return false;
	}

	// Check if position changed (requires re-triangulation)
	bool bPositionChanged = !ExistingSpeaker->WorldPosition.Equals(Speaker.WorldPosition, 0.1f);

	// Preserve the ID
	*ExistingSpeaker = Speaker;
	ExistingSpeaker->Id = SpeakerId;

	NotifyDSPChange(SpeakerId);
	SendSpeakerUpdate(SpeakerId);

	// If position changed, need to update rendering engine triangulation
	if (bPositionChanged)
	{
		SyncSpeakersToRenderingEngine();
	}

	OnSpeakerUpdated.Broadcast(SpeakerId);

	return true;
}

bool URshipSpatialAudioManager::RemoveSpeaker(const FGuid& SpeakerId)
{
	if (!Venue.RemoveSpeaker(SpeakerId))
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("RemoveSpeaker: Speaker not found: %s"), *SpeakerId.ToString());
		return false;
	}

	UnregisterSpeakerTarget(SpeakerId);
	CachedSpeakerIds.Remove(SpeakerId);

	// Remove from audio processor DSP manager
	if (AudioProcessor)
	{
		FSpatialSpeakerDSPManager* DSPManager = AudioProcessor->GetDSPManager();
		if (DSPManager)
		{
			DSPManager->RemoveSpeaker(SpeakerId);
		}

		// Rebuild index mapping
		RebuildSpeakerIndexMapping();
	}

	// Sync speakers to rendering engine for VBAP/DBAP triangulation update
	SyncSpeakersToRenderingEngine();

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Removed speaker: %s"), *SpeakerId.ToString());
	OnSpeakerRemoved.Broadcast(SpeakerId);

	return true;
}

bool URshipSpatialAudioManager::GetSpeaker(const FGuid& SpeakerId, FSpatialSpeaker& OutSpeaker) const
{
	const FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		return false;
	}
	OutSpeaker = *Speaker;
	return true;
}

TArray<FSpatialSpeaker> URshipSpatialAudioManager::GetAllSpeakers() const
{
	return Venue.GetAllSpeakers();
}

// ============================================================================
// SPEAKER DSP CONTROL
// ============================================================================

void URshipSpatialAudioManager::SetSpeakerGain(const FGuid& SpeakerId, float GainDb)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerGain: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	Speaker->DSP.OutputGainDb = FMath::Clamp(GainDb, -80.0f, 20.0f);
	NotifyDSPChange(SpeakerId);
}

void URshipSpatialAudioManager::SetSpeakerDelay(const FGuid& SpeakerId, float DelayMs)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerDelay: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	Speaker->DSP.DelayMs = FMath::Clamp(DelayMs, 0.0f, SpatialAudioConstants::MaxDelayMs);
	NotifyDSPChange(SpeakerId);
}

void URshipSpatialAudioManager::SetSpeakerMute(const FGuid& SpeakerId, bool bMuted)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerMute: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	Speaker->DSP.bMuted = bMuted;
	NotifyDSPChange(SpeakerId);
}

void URshipSpatialAudioManager::SetSpeakerPolarity(const FGuid& SpeakerId, bool bInverted)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerPolarity: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	Speaker->DSP.bPolarityInvert = bInverted;
	NotifyDSPChange(SpeakerId);
}

void URshipSpatialAudioManager::SetSpeakerEQ(const FGuid& SpeakerId, const TArray<FSpatialEQBand>& Bands)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerEQ: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	// Limit to max bands
	Speaker->DSP.EQBands = Bands;
	if (Speaker->DSP.EQBands.Num() > SPATIAL_AUDIO_MAX_EQ_BANDS)
	{
		Speaker->DSP.EQBands.SetNum(SPATIAL_AUDIO_MAX_EQ_BANDS);
	}

	NotifyDSPChange(SpeakerId);
}

void URshipSpatialAudioManager::SetSpeakerLimiter(const FGuid& SpeakerId, const FSpatialLimiterSettings& Settings)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerLimiter: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	Speaker->DSP.Limiter = Settings;
	NotifyDSPChange(SpeakerId);
}

void URshipSpatialAudioManager::SetSpeakerDSP(const FGuid& SpeakerId, const FSpatialSpeakerDSPState& DSPState)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerDSP: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	// Apply full DSP state from calibration preset
	Speaker->DSP = DSPState;

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Applied DSP preset to speaker %s: Delay=%.2fms, Gain=%.1fdB, EQ bands=%d"),
		*Speaker->Name,
		DSPState.DelayMs,
		DSPState.InputGainDb,
		DSPState.EQBands.Num());

	NotifyDSPChange(SpeakerId);
	SendSpeakerUpdate(SpeakerId);
}

void URshipSpatialAudioManager::SetSpeakerHighPass(const FGuid& SpeakerId, const FSpatialHighPassFilter& HighPass)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerHighPass: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	Speaker->DSP.HighPass = HighPass;
	NotifyDSPChange(SpeakerId);
}

void URshipSpatialAudioManager::SetSpeakerLowPass(const FGuid& SpeakerId, const FSpatialLowPassFilter& LowPass)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetSpeakerLowPass: Speaker not found: %s"), *SpeakerId.ToString());
		return;
	}

	Speaker->DSP.LowPass = LowPass;
	NotifyDSPChange(SpeakerId);
}

// ============================================================================
// ZONE MANAGEMENT
// ============================================================================

FGuid URshipSpatialAudioManager::AddZone(const FSpatialZone& Zone)
{
	FGuid NewId = Venue.AddZone(Zone);

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Added zone: %s (ID: %s, Renderer: %d)"),
		*Zone.Name, *NewId.ToString(), static_cast<int32>(Zone.RendererType));

	// Get the zone with assigned ID for registration
	if (const FSpatialZone* AddedZone = Venue.GetZone(NewId))
	{
		RegisterZoneTarget(*AddedZone);
	}

	OnZoneAdded.Broadcast(NewId);

	return NewId;
}

bool URshipSpatialAudioManager::UpdateZone(const FGuid& ZoneId, const FSpatialZone& Zone)
{
	FSpatialZone* ExistingZone = Venue.GetZone(ZoneId);
	if (!ExistingZone)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("UpdateZone: Zone not found: %s"), *ZoneId.ToString());
		return false;
	}

	*ExistingZone = Zone;
	ExistingZone->Id = ZoneId;

	SendZoneUpdate(ZoneId);

	return true;
}

bool URshipSpatialAudioManager::RemoveZone(const FGuid& ZoneId)
{
	if (!Venue.RemoveZone(ZoneId))
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("RemoveZone: Zone not found: %s"), *ZoneId.ToString());
		return false;
	}

	UnregisterZoneTarget(ZoneId);

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Removed zone: %s"), *ZoneId.ToString());
	OnZoneRemoved.Broadcast(ZoneId);

	return true;
}

void URshipSpatialAudioManager::SetZoneRenderer(const FGuid& ZoneId, ESpatialRendererType RendererType)
{
	FSpatialZone* Zone = Venue.GetZone(ZoneId);
	if (!Zone)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetZoneRenderer: Zone not found: %s"), *ZoneId.ToString());
		return;
	}

	Zone->RendererType = RendererType;

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Zone %s renderer set to %d"),
		*Zone->Name, static_cast<int32>(RendererType));

	// For now, zones share the global renderer type
	// In a full implementation, each zone would have its own renderer instance
	// For simplicity, just update the global renderer if this is the active zone
	SetGlobalRendererType(RendererType);
}

// ============================================================================
// AUDIO OBJECT MANAGEMENT
// ============================================================================

FGuid URshipSpatialAudioManager::CreateAudioObject(const FString& Name)
{
	FSpatialAudioObject NewObject;
	NewObject.Name = Name;

	// Default routing to all zones if any exist
	for (const auto& ZonePair : Venue.Zones)
	{
		NewObject.ZoneRouting.Add(ZonePair.Key);
	}

	FGuid NewId = NewObject.Id;
	AudioObjects.Add(NewId, NewObject);

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Created audio object: %s (ID: %s)"),
		*Name, *NewId.ToString());

	RegisterObjectTarget(NewObject);
	OnObjectAdded.Broadcast(NewId);

	return NewId;
}

bool URshipSpatialAudioManager::RemoveAudioObject(const FGuid& ObjectId)
{
	if (AudioObjects.Remove(ObjectId) == 0)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("RemoveAudioObject: Object not found: %s"), *ObjectId.ToString());
		return false;
	}

	UnregisterObjectTarget(ObjectId);

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Removed audio object: %s"), *ObjectId.ToString());
	OnObjectRemoved.Broadcast(ObjectId);

	return true;
}

void URshipSpatialAudioManager::SetObjectPosition(const FGuid& ObjectId, FVector Position)
{
	FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetObjectPosition: Object not found: %s"), *ObjectId.ToString());
		return;
	}

	Object->Position = Position;
	NotifyObjectChange(ObjectId);
	OnObjectPositionChanged.Broadcast(ObjectId, Position);

	// Forward to external processor if enabled
	UpdateExternalProcessorObjectPosition(ObjectId, Position);
}

void URshipSpatialAudioManager::SetObjectSpread(const FGuid& ObjectId, float Spread)
{
	FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetObjectSpread: Object not found: %s"), *ObjectId.ToString());
		return;
	}

	Object->Spread = FMath::Clamp(Spread, 0.0f, 180.0f);
	NotifyObjectChange(ObjectId);

	// Forward to external processor if enabled
	UpdateExternalProcessorObjectSpread(ObjectId, Object->Spread / 180.0f);  // Normalize to 0-1
}

void URshipSpatialAudioManager::SetObjectGain(const FGuid& ObjectId, float GainDb)
{
	FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetObjectGain: Object not found: %s"), *ObjectId.ToString());
		return;
	}

	Object->GainDb = FMath::Clamp(GainDb, -80.0f, 12.0f);
	NotifyObjectChange(ObjectId);

	// Forward to external processor if enabled
	UpdateExternalProcessorObjectGain(ObjectId, Object->GainDb);
}

void URshipSpatialAudioManager::SetObjectZoneRouting(const FGuid& ObjectId, const TArray<FGuid>& ZoneIds)
{
	FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("SetObjectZoneRouting: Object not found: %s"), *ObjectId.ToString());
		return;
	}

	Object->ZoneRouting = ZoneIds;
	NotifyObjectChange(ObjectId);
}

TArray<FSpatialAudioObject> URshipSpatialAudioManager::GetAllAudioObjects() const
{
	TArray<FSpatialAudioObject> Result;
	for (const auto& Pair : AudioObjects)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

bool URshipSpatialAudioManager::GetAudioObject(const FGuid& ObjectId, FSpatialAudioObject& OutObject) const
{
	const FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (Object)
	{
		OutObject = *Object;
		return true;
	}
	return false;
}

bool URshipSpatialAudioManager::GetAudioObjectByName(const FString& Name, FSpatialAudioObject& OutObject) const
{
	for (const auto& Pair : AudioObjects)
	{
		if (Pair.Value.Name.Equals(Name, ESearchCase::IgnoreCase))
		{
			OutObject = Pair.Value;
			return true;
		}
	}
	return false;
}

bool URshipSpatialAudioManager::GetObjectPosition(const FGuid& ObjectId, FVector& OutPosition) const
{
	const FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (Object)
	{
		OutPosition = Object->Position;
		return true;
	}
	return false;
}

bool URshipSpatialAudioManager::IsObjectActive(const FGuid& ObjectId) const
{
	const FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		return false;
	}

	// Object is active if it has gain, is not muted, and has zone routing
	return !Object->bMuted && Object->GainDb > -80.0f && Object->ZoneRouting.Num() > 0;
}

FGuid URshipSpatialAudioManager::AddObject(const FSpatialAudioObject& Object)
{
	FSpatialAudioObject NewObject = Object;
	if (!NewObject.Id.IsValid())
	{
		NewObject.Id = FGuid::NewGuid();
	}

	AudioObjects.Add(NewObject.Id, NewObject);
	RegisterObjectTarget(NewObject);
	OnObjectAdded.Broadcast(NewObject.Id);

	return NewObject.Id;
}

// ============================================================================
// ZONE QUERY & CONVENIENCE
// ============================================================================

bool URshipSpatialAudioManager::GetZone(const FGuid& ZoneId, FSpatialZone& OutZone) const
{
	const FSpatialZone* Zone = Venue.GetZone(ZoneId);
	if (Zone)
	{
		OutZone = *Zone;
		return true;
	}
	return false;
}

TArray<FSpatialZone> URshipSpatialAudioManager::GetAllZones() const
{
	TArray<FSpatialZone> Result;
	for (const auto& Pair : Venue.Zones)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

TArray<FSpatialSpeaker> URshipSpatialAudioManager::GetSpeakersByZone(const FGuid& ZoneId) const
{
	TArray<FSpatialSpeaker> Result;
	const FSpatialZone* Zone = Venue.GetZone(ZoneId);
	if (!Zone)
	{
		return Result;
	}

	for (const FGuid& SpeakerId : Zone->SpeakerIds)
	{
		const FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
		if (Speaker)
		{
			Result.Add(*Speaker);
		}
	}
	return Result;
}

TArray<FSpatialAudioObject> URshipSpatialAudioManager::GetObjectsByZone(const FGuid& ZoneId) const
{
	TArray<FSpatialAudioObject> Result;
	for (const auto& Pair : AudioObjects)
	{
		if (Pair.Value.ZoneRouting.Contains(ZoneId))
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

ESpatialRendererType URshipSpatialAudioManager::GetZoneRenderer(const FGuid& ZoneId) const
{
	const FSpatialZone* Zone = Venue.GetZone(ZoneId);
	if (Zone)
	{
		return Zone->RendererType;
	}
	return ESpatialRendererType::VBAP;
}

// ============================================================================
// ARRAY QUERY
// ============================================================================

bool URshipSpatialAudioManager::GetArray(const FGuid& ArrayId, FSpatialSpeakerArray& OutArray) const
{
	const FSpatialSpeakerArray* Array = Venue.GetArray(ArrayId);
	if (Array)
	{
		OutArray = *Array;
		return true;
	}
	return false;
}

TArray<FSpatialSpeakerArray> URshipSpatialAudioManager::GetAllArrays() const
{
	TArray<FSpatialSpeakerArray> Result;
	for (const auto& Pair : Venue.Arrays)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

// ============================================================================
// SPATIAL QUERIES
// ============================================================================

TArray<FSpatialSpeaker> URshipSpatialAudioManager::FindSpeakersNearPosition(FVector Position, float Radius) const
{
	TArray<TPair<float, FSpatialSpeaker>> SpeakersWithDistance;

	for (const auto& Pair : Venue.Speakers)
	{
		float Distance = FVector::Dist(Position, Pair.Value.WorldPosition);
		if (Distance <= Radius)
		{
			SpeakersWithDistance.Add(TPair<float, FSpatialSpeaker>(Distance, Pair.Value));
		}
	}

	// Sort by distance
	SpeakersWithDistance.Sort([](const TPair<float, FSpatialSpeaker>& A, const TPair<float, FSpatialSpeaker>& B)
	{
		return A.Key < B.Key;
	});

	TArray<FSpatialSpeaker> Result;
	for (const auto& Pair : SpeakersWithDistance)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

bool URshipSpatialAudioManager::FindClosestSpeaker(FVector Position, FSpatialSpeaker& OutSpeaker) const
{
	if (Venue.Speakers.Num() == 0)
	{
		return false;
	}

	float MinDistance = TNumericLimits<float>::Max();
	const FSpatialSpeaker* ClosestSpeaker = nullptr;

	for (const auto& Pair : Venue.Speakers)
	{
		float Distance = FVector::Dist(Position, Pair.Value.WorldPosition);
		if (Distance < MinDistance)
		{
			MinDistance = Distance;
			ClosestSpeaker = &Pair.Value;
		}
	}

	if (ClosestSpeaker)
	{
		OutSpeaker = *ClosestSpeaker;
		return true;
	}
	return false;
}

// ============================================================================
// CONVENIENCE HELPERS
// ============================================================================

bool URshipSpatialAudioManager::AddSpeakerToZone(const FGuid& SpeakerId, const FGuid& ZoneId)
{
	FSpatialZone* Zone = Venue.GetZoneMutable(ZoneId);
	FSpatialSpeaker* Speaker = Venue.GetSpeakerMutable(SpeakerId);

	if (!Zone || !Speaker)
	{
		return false;
	}

	// Add speaker to zone if not already present
	if (!Zone->SpeakerIds.Contains(SpeakerId))
	{
		Zone->SpeakerIds.Add(SpeakerId);
	}

	// Update speaker's zone reference
	Speaker->ZoneId = ZoneId;

	OnSpeakerUpdated.Broadcast(SpeakerId);
	return true;
}

bool URshipSpatialAudioManager::RemoveSpeakerFromZone(const FGuid& SpeakerId, const FGuid& ZoneId)
{
	FSpatialZone* Zone = Venue.GetZoneMutable(ZoneId);
	FSpatialSpeaker* Speaker = Venue.GetSpeakerMutable(SpeakerId);

	if (!Zone)
	{
		return false;
	}

	Zone->SpeakerIds.Remove(SpeakerId);

	if (Speaker && Speaker->ZoneId == ZoneId)
	{
		Speaker->ZoneId = FGuid();
	}

	OnSpeakerUpdated.Broadcast(SpeakerId);
	return true;
}

bool URshipSpatialAudioManager::AddObjectToZone(const FGuid& ObjectId, const FGuid& ZoneId)
{
	FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object || !Venue.GetZone(ZoneId))
	{
		return false;
	}

	if (!Object->ZoneRouting.Contains(ZoneId))
	{
		Object->ZoneRouting.Add(ZoneId);
		NotifyObjectChange(ObjectId);
	}
	return true;
}

bool URshipSpatialAudioManager::RemoveObjectFromZone(const FGuid& ObjectId, const FGuid& ZoneId)
{
	FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		return false;
	}

	Object->ZoneRouting.Remove(ZoneId);
	NotifyObjectChange(ObjectId);
	return true;
}

void URshipSpatialAudioManager::ClearAllObjects()
{
	TArray<FGuid> ObjectIds;
	AudioObjects.GetKeys(ObjectIds);

	for (const FGuid& Id : ObjectIds)
	{
		UnregisterObjectTarget(Id);
		OnObjectRemoved.Broadcast(Id);
	}
	AudioObjects.Empty();
}

void URshipSpatialAudioManager::ClearAllSpeakers()
{
	// Clear objects first (they may reference zones)
	ClearAllObjects();

	// Unregister all targets
	for (const auto& Pair : Venue.Zones)
	{
		UnregisterZoneTarget(Pair.Key);
		OnZoneRemoved.Broadcast(Pair.Key);
	}
	for (const auto& Pair : Venue.Speakers)
	{
		UnregisterSpeakerTarget(Pair.Key);
		OnSpeakerRemoved.Broadcast(Pair.Key);
	}

	// Clear venue
	Venue.Zones.Empty();
	Venue.Arrays.Empty();
	Venue.Speakers.Empty();

	OnVenueChanged.Broadcast();
}

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

void URshipSpatialAudioManager::SetMultipleSpeakerGains(const TArray<FGuid>& SpeakerIds, float GainDb)
{
	for (const FGuid& Id : SpeakerIds)
	{
		SetSpeakerGain(Id, GainDb);
	}
}

void URshipSpatialAudioManager::SetMultipleSpeakerDelays(const TArray<FGuid>& SpeakerIds, float DelayMs)
{
	for (const FGuid& Id : SpeakerIds)
	{
		SetSpeakerDelay(Id, DelayMs);
	}
}

void URshipSpatialAudioManager::SetMultipleSpeakerMute(const TArray<FGuid>& SpeakerIds, bool bMuted)
{
	for (const FGuid& Id : SpeakerIds)
	{
		SetSpeakerMute(Id, bMuted);
	}
}

void URshipSpatialAudioManager::SoloSpeakers(const TArray<FGuid>& SpeakerIds)
{
	// Mute all speakers except those in the list
	for (auto& Pair : Venue.Speakers)
	{
		bool bShouldMute = !SpeakerIds.Contains(Pair.Key);
		Pair.Value.DSP.bMuted = bShouldMute;
		Pair.Value.DSP.bSoloed = !bShouldMute;
		NotifyDSPChange(Pair.Key);
		OnSpeakerUpdated.Broadcast(Pair.Key);
	}
}

void URshipSpatialAudioManager::ClearSolo()
{
	// Unmute all speakers and clear solo state
	for (auto& Pair : Venue.Speakers)
	{
		if (Pair.Value.DSP.bSoloed)
		{
			Pair.Value.DSP.bSoloed = false;
		}
		// Only unmute if it was muted due to solo (not manually muted before)
		// For now, just unmute all - a more sophisticated implementation would track manual mutes
		Pair.Value.DSP.bMuted = false;
		NotifyDSPChange(Pair.Key);
		OnSpeakerUpdated.Broadcast(Pair.Key);
	}
}

// ============================================================================
// SYSTEM STATUS
// ============================================================================

float URshipSpatialAudioManager::GetSceneInterpolationProgress() const
{
	if (!bSceneInterpolationActive || SceneInterpolationDuration <= 0.0f)
	{
		return bSceneInterpolationActive ? 0.0f : 1.0f;
	}
	return FMath::Clamp(SceneInterpolationElapsed / SceneInterpolationDuration, 0.0f, 1.0f);
}

FSpatialAudioSystemStatus URshipSpatialAudioManager::GetSystemStatus() const
{
	FSpatialAudioSystemStatus Status;

	Status.bHasVenue = HasVenue();
	Status.bHasAudioProcessor = HasAudioProcessor();
	Status.bHasRenderingEngine = HasRenderingEngine();
	Status.bHasExternalProcessor = ExternalProcessor != nullptr;
	Status.bExternalProcessorConnected = IsExternalProcessorConnected();
	Status.bMykoRegistered = bMykoRegistered;
	Status.bSceneInterpolating = bSceneInterpolationActive;
	Status.bIsReady = Status.bHasVenue && (Status.bHasAudioProcessor || Status.bHasRenderingEngine);

	Status.SpeakerCount = Venue.GetSpeakerCount();
	Status.ZoneCount = Venue.GetZoneCount();
	Status.ArrayCount = Venue.GetArrayCount();
	Status.ObjectCount = AudioObjects.Num();
	Status.SceneCount = StoredScenes.Num();

	Status.ActiveSceneId = ActiveSceneId;
	Status.CurrentRendererType = CurrentRendererType;
	Status.VenueName = Venue.Name;

	// Add any validation warnings
	Status.Warnings = ValidateConfiguration();

	return Status;
}

// ============================================================================
// METERING
// ============================================================================

FSpatialMeterReading URshipSpatialAudioManager::GetSpeakerMeter(const FGuid& SpeakerId) const
{
	const FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (Speaker)
	{
		return Speaker->LastMeterReading;
	}
	return FSpatialMeterReading();
}

FSpatialMeterReading URshipSpatialAudioManager::GetObjectMeter(const FGuid& ObjectId) const
{
	const FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (Object)
	{
		return Object->LastMeterReading;
	}
	return FSpatialMeterReading();
}

// ============================================================================
// SCENE/PRESET MANAGEMENT
// ============================================================================

FString URshipSpatialAudioManager::StoreScene(const FString& SceneName)
{
	// Generate scene ID
	FString SceneId = FGuid::NewGuid().ToString();

	// Build scene JSON containing speaker DSP states and object positions
	TSharedPtr<FJsonObject> SceneJson = MakeShareable(new FJsonObject());
	SceneJson->SetStringField(TEXT("name"), SceneName);
	SceneJson->SetStringField(TEXT("venueId"), Venue.Id.ToString());
	SceneJson->SetStringField(TEXT("timestamp"), FDateTime::Now().ToString());

	// Store speaker DSP states
	TArray<TSharedPtr<FJsonValue>> SpeakersArray;
	for (const auto& Pair : Venue.Speakers)
	{
		const FSpatialSpeaker& Speaker = Pair.Value;
		TSharedPtr<FJsonObject> SpeakerState = MakeShareable(new FJsonObject());
		SpeakerState->SetStringField(TEXT("id"), Speaker.Id.ToString());
		SpeakerState->SetNumberField(TEXT("inputGain"), Speaker.DSP.InputGainDb);
		SpeakerState->SetNumberField(TEXT("outputGain"), Speaker.DSP.OutputGainDb);
		SpeakerState->SetNumberField(TEXT("delay"), Speaker.DSP.DelayMs);
		SpeakerState->SetBoolField(TEXT("muted"), Speaker.DSP.bMuted);
		SpeakerState->SetBoolField(TEXT("polarity"), Speaker.DSP.bPolarityInvert);
		SpeakerState->SetBoolField(TEXT("soloed"), Speaker.DSP.bSoloed);
		SpeakersArray.Add(MakeShareable(new FJsonValueObject(SpeakerState)));
	}
	SceneJson->SetArrayField(TEXT("speakers"), SpeakersArray);

	// Store audio object positions and parameters
	TArray<TSharedPtr<FJsonValue>> ObjectsArray;
	for (const auto& Pair : AudioObjects)
	{
		const FSpatialAudioObject& Object = Pair.Value;
		TSharedPtr<FJsonObject> ObjectState = MakeShareable(new FJsonObject());
		ObjectState->SetStringField(TEXT("id"), Object.Id.ToString());
		ObjectState->SetNumberField(TEXT("x"), Object.Position.X);
		ObjectState->SetNumberField(TEXT("y"), Object.Position.Y);
		ObjectState->SetNumberField(TEXT("z"), Object.Position.Z);
		ObjectState->SetNumberField(TEXT("spread"), Object.Spread);
		ObjectState->SetNumberField(TEXT("gain"), Object.GainDb);
		ObjectState->SetBoolField(TEXT("muted"), Object.bMuted);
		ObjectsArray.Add(MakeShareable(new FJsonValueObject(ObjectState)));
	}
	SceneJson->SetArrayField(TEXT("objects"), ObjectsArray);

	// Serialize to string
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(SceneJson.ToSharedRef(), Writer);

	StoredScenes.Add(SceneId, JsonString);
	SceneNames.Add(SceneId, SceneName);

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Stored scene: %s (ID: %s) with %d speakers, %d objects"),
		*SceneName, *SceneId, Venue.GetSpeakerCount(), AudioObjects.Num());

	return SceneId;
}

bool URshipSpatialAudioManager::RecallScene(const FString& SceneId, bool bInterpolate, float InterpolateTimeMs)
{
	const FString* SceneJson = StoredScenes.Find(SceneId);
	if (!SceneJson)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("RecallScene: Scene not found: %s"), *SceneId);
		return false;
	}

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*SceneJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("RecallScene: Failed to parse scene JSON: %s"), *SceneId);
		return false;
	}

	// If interpolating, set up interpolation state
	if (bInterpolate && InterpolateTimeMs > 0.0f)
	{
		// Clear any existing interpolation
		SpeakerInterpolationTargets.Empty();
		ObjectInterpolationTargets.Empty();

		// Set up speaker interpolation targets
		const TArray<TSharedPtr<FJsonValue>>* SpeakersArray;
		if (JsonObject->TryGetArrayField(TEXT("speakers"), SpeakersArray))
		{
			for (const TSharedPtr<FJsonValue>& Value : *SpeakersArray)
			{
				TSharedPtr<FJsonObject> SpeakerState = Value->AsObject();
				if (!SpeakerState.IsValid())
				{
					continue;
				}

				FGuid SpeakerId;
				if (!FGuid::Parse(SpeakerState->GetStringField(TEXT("id")), SpeakerId))
				{
					continue;
				}

				FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
				if (!Speaker)
				{
					continue;
				}

				// Create interpolation target
				FSpeakerInterpolationTarget Target;
				Target.StartInputGain = Speaker->DSP.InputGainDb;
				Target.StartOutputGain = Speaker->DSP.OutputGainDb;
				Target.StartDelay = Speaker->DSP.DelayMs;
				Target.bStartMuted = Speaker->DSP.bMuted;

				// Parse target values
				Target.TargetInputGain = SpeakerState->HasField(TEXT("inputGain"))
					? SpeakerState->GetNumberField(TEXT("inputGain"))
					: Speaker->DSP.InputGainDb;
				Target.TargetOutputGain = SpeakerState->HasField(TEXT("outputGain"))
					? SpeakerState->GetNumberField(TEXT("outputGain"))
					: Speaker->DSP.OutputGainDb;
				Target.TargetDelay = SpeakerState->HasField(TEXT("delay"))
					? SpeakerState->GetNumberField(TEXT("delay"))
					: Speaker->DSP.DelayMs;
				Target.bTargetMuted = SpeakerState->HasField(TEXT("muted"))
					? SpeakerState->GetBoolField(TEXT("muted"))
					: Speaker->DSP.bMuted;

				SpeakerInterpolationTargets.Add(SpeakerId, Target);
			}
		}

		// Set up object interpolation targets
		const TArray<TSharedPtr<FJsonValue>>* ObjectsArray;
		if (JsonObject->TryGetArrayField(TEXT("objects"), ObjectsArray))
		{
			for (const TSharedPtr<FJsonValue>& Value : *ObjectsArray)
			{
				TSharedPtr<FJsonObject> ObjectState = Value->AsObject();
				if (!ObjectState.IsValid())
				{
					continue;
				}

				FGuid ObjectId;
				if (!FGuid::Parse(ObjectState->GetStringField(TEXT("id")), ObjectId))
				{
					continue;
				}

				FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
				if (!Object)
				{
					continue;
				}

				// Create interpolation target
				FObjectInterpolationTarget Target;
				Target.StartPosition = Object->Position;
				Target.StartSpread = Object->Spread;
				Target.StartGain = Object->GainDb;
				Target.bStartMuted = Object->bMuted;

				// Parse target values
				Target.TargetPosition.X = ObjectState->HasField(TEXT("x"))
					? ObjectState->GetNumberField(TEXT("x"))
					: Object->Position.X;
				Target.TargetPosition.Y = ObjectState->HasField(TEXT("y"))
					? ObjectState->GetNumberField(TEXT("y"))
					: Object->Position.Y;
				Target.TargetPosition.Z = ObjectState->HasField(TEXT("z"))
					? ObjectState->GetNumberField(TEXT("z"))
					: Object->Position.Z;
				Target.TargetSpread = ObjectState->HasField(TEXT("spread"))
					? ObjectState->GetNumberField(TEXT("spread"))
					: Object->Spread;
				Target.TargetGain = ObjectState->HasField(TEXT("gain"))
					? ObjectState->GetNumberField(TEXT("gain"))
					: Object->GainDb;
				Target.bTargetMuted = ObjectState->HasField(TEXT("muted"))
					? ObjectState->GetBoolField(TEXT("muted"))
					: Object->bMuted;

				ObjectInterpolationTargets.Add(ObjectId, Target);
			}
		}

		// Start interpolation
		SceneInterpolationDuration = InterpolateTimeMs / 1000.0f;  // Convert to seconds
		SceneInterpolationElapsed = 0.0f;
		bSceneInterpolationActive = true;

		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Started scene interpolation: %s (%.0fms, %d speakers, %d objects)"),
			*SceneId, InterpolateTimeMs, SpeakerInterpolationTargets.Num(), ObjectInterpolationTargets.Num());
	}
	else
	{
		// Apply immediately without interpolation

		// Apply speaker states
		const TArray<TSharedPtr<FJsonValue>>* SpeakersArray;
		if (JsonObject->TryGetArrayField(TEXT("speakers"), SpeakersArray))
		{
			for (const TSharedPtr<FJsonValue>& Value : *SpeakersArray)
			{
				TSharedPtr<FJsonObject> SpeakerState = Value->AsObject();
				if (!SpeakerState.IsValid())
				{
					continue;
				}

				FGuid SpeakerId;
				if (!FGuid::Parse(SpeakerState->GetStringField(TEXT("id")), SpeakerId))
				{
					continue;
				}

				FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
				if (!Speaker)
				{
					continue;
				}

				// Apply DSP state
				if (SpeakerState->HasField(TEXT("inputGain")))
				{
					Speaker->DSP.InputGainDb = SpeakerState->GetNumberField(TEXT("inputGain"));
				}
				if (SpeakerState->HasField(TEXT("outputGain")))
				{
					Speaker->DSP.OutputGainDb = SpeakerState->GetNumberField(TEXT("outputGain"));
				}
				if (SpeakerState->HasField(TEXT("delay")))
				{
					Speaker->DSP.DelayMs = SpeakerState->GetNumberField(TEXT("delay"));
				}
				if (SpeakerState->HasField(TEXT("muted")))
				{
					Speaker->DSP.bMuted = SpeakerState->GetBoolField(TEXT("muted"));
				}
				if (SpeakerState->HasField(TEXT("polarity")))
				{
					Speaker->DSP.bPolarityInvert = SpeakerState->GetBoolField(TEXT("polarity"));
				}

				NotifyDSPChange(SpeakerId);
				OnSpeakerUpdated.Broadcast(SpeakerId);
			}
		}

		// Apply object states
		const TArray<TSharedPtr<FJsonValue>>* ObjectsArray;
		if (JsonObject->TryGetArrayField(TEXT("objects"), ObjectsArray))
		{
			for (const TSharedPtr<FJsonValue>& Value : *ObjectsArray)
			{
				TSharedPtr<FJsonObject> ObjectState = Value->AsObject();
				if (!ObjectState.IsValid())
				{
					continue;
				}

				FGuid ObjectId;
				if (!FGuid::Parse(ObjectState->GetStringField(TEXT("id")), ObjectId))
				{
					continue;
				}

				FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
				if (!Object)
				{
					continue;
				}

				// Apply position
				if (ObjectState->HasField(TEXT("x")))
				{
					Object->Position.X = ObjectState->GetNumberField(TEXT("x"));
				}
				if (ObjectState->HasField(TEXT("y")))
				{
					Object->Position.Y = ObjectState->GetNumberField(TEXT("y"));
				}
				if (ObjectState->HasField(TEXT("z")))
				{
					Object->Position.Z = ObjectState->GetNumberField(TEXT("z"));
				}
				if (ObjectState->HasField(TEXT("spread")))
				{
					Object->Spread = ObjectState->GetNumberField(TEXT("spread"));
				}
				if (ObjectState->HasField(TEXT("gain")))
				{
					Object->GainDb = ObjectState->GetNumberField(TEXT("gain"));
				}
				if (ObjectState->HasField(TEXT("muted")))
				{
					Object->bMuted = ObjectState->GetBoolField(TEXT("muted"));
				}

				NotifyObjectChange(ObjectId);
				OnObjectPositionChanged.Broadcast(ObjectId, Object->Position);
			}
		}
	}

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Recalled scene: %s"), *SceneId);

	return true;
}

bool URshipSpatialAudioManager::DeleteScene(const FString& SceneId)
{
	if (StoredScenes.Remove(SceneId) > 0)
	{
		SceneNames.Remove(SceneId);
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Deleted scene: %s"), *SceneId);
		return true;
	}
	return false;
}

TArray<FString> URshipSpatialAudioManager::GetSceneList() const
{
	TArray<FString> Result;
	StoredScenes.GetKeys(Result);
	return Result;
}

FString URshipSpatialAudioManager::GetSceneName(const FString& SceneId) const
{
	const FString* Name = SceneNames.Find(SceneId);
	return Name ? *Name : TEXT("");
}

// ============================================================================
// VENUE IMPORT/EXPORT
// ============================================================================

FString URshipSpatialAudioManager::ExportVenueToJson() const
{
	TSharedPtr<FJsonObject> VenueJson = MakeShareable(new FJsonObject());

	// Venue metadata
	VenueJson->SetStringField(TEXT("id"), Venue.Id.ToString());
	VenueJson->SetStringField(TEXT("name"), Venue.Name);
	VenueJson->SetNumberField(TEXT("version"), 1);
	VenueJson->SetStringField(TEXT("exportTime"), FDateTime::Now().ToString());

	// Reference point (venue origin)
	TSharedPtr<FJsonObject> RefPoint = MakeShareable(new FJsonObject());
	RefPoint->SetNumberField(TEXT("x"), Venue.VenueOrigin.X);
	RefPoint->SetNumberField(TEXT("y"), Venue.VenueOrigin.Y);
	RefPoint->SetNumberField(TEXT("z"), Venue.VenueOrigin.Z);
	VenueJson->SetObjectField(TEXT("referencePoint"), RefPoint);

	// Export speakers
	TArray<TSharedPtr<FJsonValue>> SpeakersArray;
	for (const auto& Pair : Venue.Speakers)
	{
		TSharedPtr<FJsonObject> SpeakerJson = FSpatialAudioMykoSerializer::SpeakerToJson(Pair.Value, Venue.Id);
		SpeakersArray.Add(MakeShareable(new FJsonValueObject(SpeakerJson)));
	}
	VenueJson->SetArrayField(TEXT("speakers"), SpeakersArray);

	// Export zones
	TArray<TSharedPtr<FJsonValue>> ZonesArray;
	for (const auto& Pair : Venue.Zones)
	{
		TSharedPtr<FJsonObject> ZoneJson = FSpatialAudioMykoSerializer::ZoneToJson(Pair.Value, Venue.Id);
		ZonesArray.Add(MakeShareable(new FJsonValueObject(ZoneJson)));
	}
	VenueJson->SetArrayField(TEXT("zones"), ZonesArray);

	// Export arrays
	TArray<TSharedPtr<FJsonValue>> ArraysArray;
	for (const auto& Pair : Venue.Arrays)
	{
		const FSpatialArray& Arr = Pair.Value;
		TSharedPtr<FJsonObject> ArrayJson = MakeShareable(new FJsonObject());
		ArrayJson->SetStringField(TEXT("id"), Arr.Id.ToString());
		ArrayJson->SetStringField(TEXT("name"), Arr.Name);

		TArray<TSharedPtr<FJsonValue>> SpeakerIds;
		for (const FGuid& SpkId : Arr.SpeakerIds)
		{
			SpeakerIds.Add(MakeShareable(new FJsonValueString(SpkId.ToString())));
		}
		ArrayJson->SetArrayField(TEXT("speakerIds"), SpeakerIds);
		ArraysArray.Add(MakeShareable(new FJsonValueObject(ArrayJson)));
	}
	VenueJson->SetArrayField(TEXT("arrays"), ArraysArray);

	// Export audio objects
	TArray<TSharedPtr<FJsonValue>> ObjectsArray;
	for (const auto& Pair : AudioObjects)
	{
		TSharedPtr<FJsonObject> ObjectJson = FSpatialAudioMykoSerializer::ObjectToJson(Pair.Value, Venue.Id);
		ObjectsArray.Add(MakeShareable(new FJsonValueObject(ObjectJson)));
	}
	VenueJson->SetArrayField(TEXT("audioObjects"), ObjectsArray);

	// Serialize to string with pretty print
	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(VenueJson.ToSharedRef(), Writer);

	return JsonString;
}

bool URshipSpatialAudioManager::ImportVenueFromJson(const FString& JsonString)
{
	// Parse JSON
	TSharedPtr<FJsonObject> VenueJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, VenueJson) || !VenueJson.IsValid())
	{
		UE_LOG(LogRshipSpatialAudioManager, Error, TEXT("ImportVenueFromJson: Failed to parse JSON"));
		return false;
	}

	// Unregister current venue
	UnregisterMykoTargets();

	// Clear current state
	Venue = FSpatialVenue();
	AudioObjects.Empty();
	CachedSpeakerIds.Empty();

	// Import venue metadata
	if (VenueJson->HasField(TEXT("name")))
	{
		Venue.Name = VenueJson->GetStringField(TEXT("name"));
	}

	// Import reference point (venue origin)
	if (VenueJson->HasField(TEXT("referencePoint")))
	{
		TSharedPtr<FJsonObject> RefPoint = VenueJson->GetObjectField(TEXT("referencePoint"));
		Venue.VenueOrigin.X = RefPoint->GetNumberField(TEXT("x"));
		Venue.VenueOrigin.Y = RefPoint->GetNumberField(TEXT("y"));
		Venue.VenueOrigin.Z = RefPoint->GetNumberField(TEXT("z"));
	}

	// Import speakers
	const TArray<TSharedPtr<FJsonValue>>* SpeakersArray;
	if (VenueJson->TryGetArrayField(TEXT("speakers"), SpeakersArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *SpeakersArray)
		{
			TSharedPtr<FJsonObject> SpeakerJson = Value->AsObject();
			if (!SpeakerJson.IsValid())
			{
				continue;
			}

			FSpatialSpeaker Speaker;
			FSpatialAudioMykoSerializer::ParseSpeakerUpdate(SpeakerJson, Speaker);

			// Try to preserve original ID
			if (SpeakerJson->HasField(TEXT("id")))
			{
				FGuid::Parse(SpeakerJson->GetStringField(TEXT("id")), Speaker.Id);
			}

			Venue.Speakers.Add(Speaker.Id, Speaker);
			CachedSpeakerIds.Add(Speaker.Id);
		}
	}

	// Import zones
	const TArray<TSharedPtr<FJsonValue>>* ZonesArray;
	if (VenueJson->TryGetArrayField(TEXT("zones"), ZonesArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ZonesArray)
		{
			TSharedPtr<FJsonObject> ZoneJson = Value->AsObject();
			if (!ZoneJson.IsValid())
			{
				continue;
			}

			FSpatialZone Zone;
			FSpatialAudioMykoSerializer::ParseZoneUpdate(ZoneJson, Zone);

			// Try to preserve original ID
			if (ZoneJson->HasField(TEXT("id")))
			{
				FGuid::Parse(ZoneJson->GetStringField(TEXT("id")), Zone.Id);
			}

			Venue.Zones.Add(Zone.Id, Zone);
		}
	}

	// Import audio objects
	const TArray<TSharedPtr<FJsonValue>>* ObjectsArray;
	if (VenueJson->TryGetArrayField(TEXT("audioObjects"), ObjectsArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ObjectsArray)
		{
			TSharedPtr<FJsonObject> ObjectJson = Value->AsObject();
			if (!ObjectJson.IsValid())
			{
				continue;
			}

			FSpatialAudioObject Object;
			FSpatialAudioMykoSerializer::ParseObjectUpdate(ObjectJson, Object);

			// Try to preserve original ID
			if (ObjectJson->HasField(TEXT("id")))
			{
				FGuid::Parse(ObjectJson->GetStringField(TEXT("id")), Object.Id);
			}

			AudioObjects.Add(Object.Id, Object);
		}
	}

	// Re-register with Myko
	RegisterMykoTargets();

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Imported venue: %s with %d speakers, %d zones, %d objects"),
		*Venue.Name, Venue.GetSpeakerCount(), Venue.GetZoneCount(), AudioObjects.Num());

	OnVenueChanged.Broadcast();

	return true;
}

bool URshipSpatialAudioManager::ExportVenueToFile(const FString& FilePath) const
{
	FString JsonString = ExportVenueToJson();
	if (FFileHelper::SaveStringToFile(JsonString, *FilePath))
	{
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Exported venue to: %s"), *FilePath);
		return true;
	}
	UE_LOG(LogRshipSpatialAudioManager, Error, TEXT("Failed to export venue to: %s"), *FilePath);
	return false;
}

bool URshipSpatialAudioManager::ImportVenueFromFile(const FString& FilePath)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogRshipSpatialAudioManager, Error, TEXT("Failed to load venue file: %s"), *FilePath);
		return false;
	}
	return ImportVenueFromJson(JsonString);
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

FString URshipSpatialAudioManager::GetDiagnosticInfo() const
{
	return FString::Printf(
		TEXT("Venue: %s\n")
		TEXT("Speakers: %d\n")
		TEXT("Arrays: %d\n")
		TEXT("Zones: %d\n")
		TEXT("Audio Objects: %d\n")
		TEXT("Stored Scenes: %d"),
		*Venue.Name,
		Venue.GetSpeakerCount(),
		Venue.GetArrayCount(),
		Venue.GetZoneCount(),
		AudioObjects.Num(),
		StoredScenes.Num()
	);
}

TArray<FString> URshipSpatialAudioManager::ValidateConfiguration() const
{
	return Venue.Validate();
}

// ============================================================================
// INTERNAL METHODS - MYKO INTEGRATION
// ============================================================================

void URshipSpatialAudioManager::RegisterMykoTargets()
{
	if (!Subsystem || bMykoRegistered)
	{
		return;
	}

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Registering spatial audio entities with Myko"));

	// Register venue as instance-level entity
	TSharedPtr<FJsonObject> VenueJson = FSpatialAudioMykoSerializer::VenueToJson(Venue);
	Subsystem->SetItem(SpatialAudioMykoTypes::Venue, VenueJson, ERshipMessagePriority::High, Venue.Id.ToString());

	// Register all existing speakers
	CachedSpeakerIds.Empty();
	for (const auto& Pair : Venue.Speakers)
	{
		RegisterSpeakerTarget(Pair.Value);
		CachedSpeakerIds.Add(Pair.Key);
	}

	// Register all existing zones
	for (const auto& Pair : Venue.Zones)
	{
		RegisterZoneTarget(Pair.Value);
	}

	// Register all existing audio objects
	for (const auto& Pair : AudioObjects)
	{
		RegisterObjectTarget(Pair.Value);
	}

	bMykoRegistered = true;
	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Registered %d speakers, %d zones, %d objects with Myko"),
		Venue.GetSpeakerCount(), Venue.GetZoneCount(), AudioObjects.Num());
}

void URshipSpatialAudioManager::UnregisterMykoTargets()
{
	if (!Subsystem || !bMykoRegistered)
	{
		return;
	}

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Unregistering spatial audio entities from Myko"));

	// Unregister all audio objects
	for (const auto& Pair : AudioObjects)
	{
		UnregisterObjectTarget(Pair.Key);
	}

	// Unregister all zones
	for (const auto& Pair : Venue.Zones)
	{
		UnregisterZoneTarget(Pair.Key);
	}

	// Unregister all speakers
	for (const FGuid& SpeakerId : CachedSpeakerIds)
	{
		UnregisterSpeakerTarget(SpeakerId);
	}
	CachedSpeakerIds.Empty();

	// Unregister venue
	TSharedPtr<FJsonObject> DeleteJson = MakeShareable(new FJsonObject());
	DeleteJson->SetStringField(TEXT("id"), Venue.Id.ToString());

	TSharedPtr<FJsonObject> EventData = MakeShareable(new FJsonObject());
	EventData->SetStringField(TEXT("itemType"), SpatialAudioMykoTypes::Venue);
	EventData->SetObjectField(TEXT("item"), DeleteJson);

	TSharedPtr<FJsonObject> Event = MakeShareable(new FJsonObject());
	Event->SetStringField(TEXT("event"), TEXT("ws:m:del"));
	Event->SetObjectField(TEXT("data"), EventData);

	Subsystem->SendJson(Event);

	bMykoRegistered = false;
}

void URshipSpatialAudioManager::RegisterSpeakerTarget(const FSpatialSpeaker& Speaker)
{
	if (!Subsystem)
	{
		return;
	}

	TSharedPtr<FJsonObject> SpeakerJson = FSpatialAudioMykoSerializer::SpeakerToJson(Speaker, Venue.Id);
	Subsystem->SetItem(SpatialAudioMykoTypes::Speaker, SpeakerJson, ERshipMessagePriority::High, Speaker.Id.ToString());

	UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Registered speaker target: %s"), *Speaker.Name);
}

void URshipSpatialAudioManager::RegisterZoneTarget(const FSpatialZone& Zone)
{
	if (!Subsystem)
	{
		return;
	}

	TSharedPtr<FJsonObject> ZoneJson = FSpatialAudioMykoSerializer::ZoneToJson(Zone, Venue.Id);
	Subsystem->SetItem(SpatialAudioMykoTypes::Zone, ZoneJson, ERshipMessagePriority::High, Zone.Id.ToString());

	UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Registered zone target: %s"), *Zone.Name);
}

void URshipSpatialAudioManager::RegisterObjectTarget(const FSpatialAudioObject& Object)
{
	if (!Subsystem)
	{
		return;
	}

	TSharedPtr<FJsonObject> ObjectJson = FSpatialAudioMykoSerializer::ObjectToJson(Object, Venue.Id);
	Subsystem->SetItem(SpatialAudioMykoTypes::Object, ObjectJson, ERshipMessagePriority::High, Object.Id.ToString());

	UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Registered audio object target: %s"), *Object.Name);
}

void URshipSpatialAudioManager::UnregisterSpeakerTarget(const FGuid& SpeakerId)
{
	if (!Subsystem)
	{
		return;
	}

	TSharedPtr<FJsonObject> DeleteJson = MakeShareable(new FJsonObject());
	DeleteJson->SetStringField(TEXT("id"), SpeakerId.ToString());

	TSharedPtr<FJsonObject> EventData = MakeShareable(new FJsonObject());
	EventData->SetStringField(TEXT("itemType"), SpatialAudioMykoTypes::Speaker);
	EventData->SetObjectField(TEXT("item"), DeleteJson);

	TSharedPtr<FJsonObject> Event = MakeShareable(new FJsonObject());
	Event->SetStringField(TEXT("event"), TEXT("ws:m:del"));
	Event->SetObjectField(TEXT("data"), EventData);

	Subsystem->SendJson(Event);

	UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Unregistered speaker target: %s"), *SpeakerId.ToString());
}

void URshipSpatialAudioManager::UnregisterZoneTarget(const FGuid& ZoneId)
{
	if (!Subsystem)
	{
		return;
	}

	TSharedPtr<FJsonObject> DeleteJson = MakeShareable(new FJsonObject());
	DeleteJson->SetStringField(TEXT("id"), ZoneId.ToString());

	TSharedPtr<FJsonObject> EventData = MakeShareable(new FJsonObject());
	EventData->SetStringField(TEXT("itemType"), SpatialAudioMykoTypes::Zone);
	EventData->SetObjectField(TEXT("item"), DeleteJson);

	TSharedPtr<FJsonObject> Event = MakeShareable(new FJsonObject());
	Event->SetStringField(TEXT("event"), TEXT("ws:m:del"));
	Event->SetObjectField(TEXT("data"), EventData);

	Subsystem->SendJson(Event);

	UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Unregistered zone target: %s"), *ZoneId.ToString());
}

void URshipSpatialAudioManager::UnregisterObjectTarget(const FGuid& ObjectId)
{
	if (!Subsystem)
	{
		return;
	}

	TSharedPtr<FJsonObject> DeleteJson = MakeShareable(new FJsonObject());
	DeleteJson->SetStringField(TEXT("id"), ObjectId.ToString());

	TSharedPtr<FJsonObject> EventData = MakeShareable(new FJsonObject());
	EventData->SetStringField(TEXT("itemType"), SpatialAudioMykoTypes::Object);
	EventData->SetObjectField(TEXT("item"), DeleteJson);

	TSharedPtr<FJsonObject> Event = MakeShareable(new FJsonObject());
	Event->SetStringField(TEXT("event"), TEXT("ws:m:del"));
	Event->SetObjectField(TEXT("data"), EventData);

	Subsystem->SendJson(Event);

	UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Unregistered audio object target: %s"), *ObjectId.ToString());
}

void URshipSpatialAudioManager::SendMeterPulses()
{
	if (!Subsystem || !bMykoRegistered)
	{
		return;
	}

	// Send speaker meter pulses
	for (const FGuid& SpeakerId : CachedSpeakerIds)
	{
		const FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
		if (!Speaker)
		{
			continue;
		}

		// Only send if there's meaningful activity (Peak > -80dB threshold)
		if (Speaker->LastMeterReading.Peak > SpatialAudioConstants::MinGainThreshold)
		{
			TSharedPtr<FJsonObject> MeterJson = FSpatialAudioMykoSerializer::MeterToJson(SpeakerId, Speaker->LastMeterReading);
			Subsystem->PulseEmitter(SpeakerId.ToString(), SpatialAudioMykoEmitters::SpeakerLevel, MeterJson);
		}
	}

	// Send audio object meter pulses
	for (const auto& Pair : AudioObjects)
	{
		const FSpatialAudioObject& Object = Pair.Value;
		if (Object.LastMeterReading.Peak > SpatialAudioConstants::MinGainThreshold)
		{
			TSharedPtr<FJsonObject> MeterJson = FSpatialAudioMykoSerializer::MeterToJson(Object.Id, Object.LastMeterReading);
			Subsystem->PulseEmitter(Object.Id.ToString(), SpatialAudioMykoEmitters::ObjectLevel, MeterJson);
		}
	}
}

void URshipSpatialAudioManager::SendSpeakerUpdate(const FGuid& SpeakerId)
{
	if (!Subsystem || !bMykoRegistered)
	{
		return;
	}

	const FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		return;
	}

	TSharedPtr<FJsonObject> SpeakerJson = FSpatialAudioMykoSerializer::SpeakerToJson(*Speaker, Venue.Id);
	Subsystem->SetItem(SpatialAudioMykoTypes::Speaker, SpeakerJson, ERshipMessagePriority::Normal, SpeakerId.ToString());
}

void URshipSpatialAudioManager::SendZoneUpdate(const FGuid& ZoneId)
{
	if (!Subsystem || !bMykoRegistered)
	{
		return;
	}

	const FSpatialZone* Zone = Venue.GetZone(ZoneId);
	if (!Zone)
	{
		return;
	}

	TSharedPtr<FJsonObject> ZoneJson = FSpatialAudioMykoSerializer::ZoneToJson(*Zone, Venue.Id);
	Subsystem->SetItem(SpatialAudioMykoTypes::Zone, ZoneJson, ERshipMessagePriority::Normal, ZoneId.ToString());
}

void URshipSpatialAudioManager::SendObjectUpdate(const FGuid& ObjectId)
{
	if (!Subsystem || !bMykoRegistered)
	{
		return;
	}

	const FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		return;
	}

	TSharedPtr<FJsonObject> ObjectJson = FSpatialAudioMykoSerializer::ObjectToJson(*Object, Venue.Id);
	Subsystem->SetItem(SpatialAudioMykoTypes::Object, ObjectJson, ERshipMessagePriority::Normal, ObjectId.ToString());
}

void URshipSpatialAudioManager::ProcessRshipAction(const FString& TargetId, const FString& ActionId, const TSharedPtr<FJsonObject>& Data)
{
	if (!Data.IsValid())
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("ProcessRshipAction: Invalid data for %s.%s"), *TargetId, *ActionId);
		return;
	}

	// Parse the target GUID
	FGuid EntityId;
	if (!FGuid::Parse(TargetId, EntityId))
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("ProcessRshipAction: Invalid target ID: %s"), *TargetId);
		return;
	}

	// Determine entity type and route to appropriate handler
	if (Venue.GetSpeaker(EntityId))
	{
		ProcessSpeakerAction(EntityId, ActionId, Data);
	}
	else if (Venue.GetZone(EntityId))
	{
		ProcessZoneAction(EntityId, ActionId, Data);
	}
	else if (AudioObjects.Contains(EntityId))
	{
		ProcessObjectAction(EntityId, ActionId, Data);
	}
	else
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("ProcessRshipAction: Unknown target: %s"), *TargetId);
	}
}

void URshipSpatialAudioManager::ProcessSpeakerAction(const FGuid& SpeakerId, const FString& ActionId, const TSharedPtr<FJsonObject>& Data)
{
	FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
	if (!Speaker)
	{
		return;
	}

	if (ActionId == SpatialAudioMykoActions::SetSpeakerGain)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropGain))
		{
			SetSpeakerGain(SpeakerId, Data->GetNumberField(SpatialAudioMykoSchema::PropGain));
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetSpeakerDelay)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropDelay))
		{
			SetSpeakerDelay(SpeakerId, Data->GetNumberField(SpatialAudioMykoSchema::PropDelay));
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetSpeakerMute)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropMute))
		{
			SetSpeakerMute(SpeakerId, Data->GetBoolField(SpatialAudioMykoSchema::PropMute));
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetSpeakerPolarity)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropPolarity))
		{
			SetSpeakerPolarity(SpeakerId, Data->GetBoolField(SpatialAudioMykoSchema::PropPolarity));
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetSpeakerEQ)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropEQ))
		{
			TArray<FSpatialEQBand> Bands;
			if (FSpatialAudioMykoSerializer::ParseEQBands(Data->TryGetField(SpatialAudioMykoSchema::PropEQ), Bands))
			{
				SetSpeakerEQ(SpeakerId, Bands);
			}
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetSpeakerLimiter)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropLimiter))
		{
			FSpatialLimiterSettings Limiter;
			if (FSpatialAudioMykoSerializer::ParseLimiter(Data->GetObjectField(SpatialAudioMykoSchema::PropLimiter), Limiter))
			{
				SetSpeakerLimiter(SpeakerId, Limiter);
			}
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetSpeakerPosition)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropPosition))
		{
			FVector Position;
			TSharedPtr<FJsonObject> PosJson = Data->GetObjectField(SpatialAudioMykoSchema::PropPosition);
			Position.X = PosJson->GetNumberField(SpatialAudioMykoSchema::PropX);
			Position.Y = PosJson->GetNumberField(SpatialAudioMykoSchema::PropY);
			Position.Z = PosJson->GetNumberField(SpatialAudioMykoSchema::PropZ);
			Speaker->Position = Position;
			SendSpeakerUpdate(SpeakerId);
		}
	}
	else
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("ProcessSpeakerAction: Unknown action: %s"), *ActionId);
	}
}

void URshipSpatialAudioManager::ProcessZoneAction(const FGuid& ZoneId, const FString& ActionId, const TSharedPtr<FJsonObject>& Data)
{
	FSpatialZone* Zone = Venue.GetZone(ZoneId);
	if (!Zone)
	{
		return;
	}

	if (ActionId == SpatialAudioMykoActions::SetZoneRenderer)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropRenderer))
		{
			FString RendererStr = Data->GetStringField(SpatialAudioMykoSchema::PropRenderer);
			int64 EnumValue = StaticEnum<ESpatialRendererType>()->GetValueByNameString(RendererStr);
			if (EnumValue != INDEX_NONE)
			{
				SetZoneRenderer(ZoneId, static_cast<ESpatialRendererType>(EnumValue));
			}
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetZoneSpeakers)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropSpeakers))
		{
			Zone->SpeakerIds.Empty();
			const TArray<TSharedPtr<FJsonValue>>& SpeakerArray = Data->GetArrayField(SpatialAudioMykoSchema::PropSpeakers);
			for (const TSharedPtr<FJsonValue>& Value : SpeakerArray)
			{
				FGuid SpeakerId;
				if (FGuid::Parse(Value->AsString(), SpeakerId))
				{
					Zone->SpeakerIds.Add(SpeakerId);
				}
			}
			SendZoneUpdate(ZoneId);
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetZoneActive)
	{
		// Zone activation is typically handled by scene management
		// For now, just log
		UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Zone active state change: %s"), *ZoneId.ToString());
	}
	else
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("ProcessZoneAction: Unknown action: %s"), *ActionId);
	}
}

void URshipSpatialAudioManager::ProcessObjectAction(const FGuid& ObjectId, const FString& ActionId, const TSharedPtr<FJsonObject>& Data)
{
	FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		return;
	}

	if (ActionId == SpatialAudioMykoActions::SetObjectPosition)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropPosition))
		{
			FVector Position;
			TSharedPtr<FJsonObject> PosJson = Data->GetObjectField(SpatialAudioMykoSchema::PropPosition);
			Position.X = PosJson->GetNumberField(SpatialAudioMykoSchema::PropX);
			Position.Y = PosJson->GetNumberField(SpatialAudioMykoSchema::PropY);
			Position.Z = PosJson->GetNumberField(SpatialAudioMykoSchema::PropZ);
			SetObjectPosition(ObjectId, Position);
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetObjectSpread)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropSpread))
		{
			SetObjectSpread(ObjectId, Data->GetNumberField(SpatialAudioMykoSchema::PropSpread));
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetObjectGain)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropGain))
		{
			SetObjectGain(ObjectId, Data->GetNumberField(SpatialAudioMykoSchema::PropGain));
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetObjectRouting)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropRouting))
		{
			TArray<FGuid> ZoneIds;
			const TArray<TSharedPtr<FJsonValue>>& RoutingArray = Data->GetArrayField(SpatialAudioMykoSchema::PropRouting);
			for (const TSharedPtr<FJsonValue>& Value : RoutingArray)
			{
				FGuid ZoneId;
				if (FGuid::Parse(Value->AsString(), ZoneId))
				{
					ZoneIds.Add(ZoneId);
				}
			}
			SetObjectZoneRouting(ObjectId, ZoneIds);
		}
	}
	else if (ActionId == SpatialAudioMykoActions::SetObjectMute)
	{
		if (Data->HasField(SpatialAudioMykoSchema::PropMute))
		{
			Object->bMuted = Data->GetBoolField(SpatialAudioMykoSchema::PropMute);
			NotifyObjectChange(ObjectId);
			SendObjectUpdate(ObjectId);
		}
	}
	else
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("ProcessObjectAction: Unknown action: %s"), *ActionId);
	}
}

// ============================================================================
// INTERNAL METHODS - AUDIO ENGINE
// ============================================================================

void URshipSpatialAudioManager::SetAudioProcessor(FSpatialAudioProcessor* Processor)
{
	AudioProcessor = Processor;

	if (AudioProcessor)
	{
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Audio processor connected"));

		// Rebuild speaker index mapping
		RebuildSpeakerIndexMapping();

		// Enable DSP chain on processor
		AudioProcessor->QueueEnableDSPChain(true);

		// Push current DSP state for all speakers
		TArray<FSpatialSpeaker> AllSpeakers = GetAllSpeakers();
		for (const FSpatialSpeaker& Speaker : AllSpeakers)
		{
			// Register speaker with DSP manager
			FSpatialSpeakerDSPManager* DSPManager = AudioProcessor->GetDSPManager();
			if (DSPManager)
			{
				DSPManager->AddSpeaker(Speaker.Id);
			}

			// Apply current configuration
			FSpatialSpeakerDSPConfig Config = BuildDSPConfig(Speaker);
			AudioProcessor->ApplySpeakerDSPConfig(Speaker.Id, Config);
		}
	}
	else
	{
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Audio processor disconnected"));
		SpeakerIdToIndex.Empty();
	}
}

void URshipSpatialAudioManager::RebuildSpeakerIndexMapping()
{
	SpeakerIdToIndex.Empty();

	TArray<FSpatialSpeaker> AllSpeakers = GetAllSpeakers();
	for (int32 i = 0; i < AllSpeakers.Num(); ++i)
	{
		SpeakerIdToIndex.Add(AllSpeakers[i].Id, i);
	}

	UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Rebuilt speaker index mapping: %d speakers"), AllSpeakers.Num());
}

FSpatialSpeakerDSPConfig URshipSpatialAudioManager::BuildDSPConfig(const FSpatialSpeaker& Speaker) const
{
	FSpatialSpeakerDSPConfig Config;

	Config.SpeakerId = Speaker.Id;
	Config.InputGainDb = Speaker.DSP.InputGainDb;
	Config.OutputGainDb = Speaker.DSP.OutputGainDb;
	Config.DelayMs = Speaker.DSP.DelayMs;
	Config.bInvertPolarity = Speaker.DSP.bPolarityInvert;
	Config.bMuted = Speaker.DSP.bMuted;
	Config.bSoloed = Speaker.DSP.bSoloed;

	// Convert crossover settings
	if (Speaker.DSP.HighPass.bEnabled)
	{
		Config.Crossover.HighPassFrequency = Speaker.DSP.HighPass.FrequencyHz;
		Config.Crossover.HighPassOrder = (Speaker.DSP.HighPass.Slope == ESpatialFilterSlope::Slope24dB) ? 4 : 2;
		Config.Crossover.bLinkwitzRiley = (Speaker.DSP.HighPass.FilterType == ESpatialFilterType::LinkwitzRiley);
	}

	if (Speaker.DSP.LowPass.bEnabled)
	{
		Config.Crossover.LowPassFrequency = Speaker.DSP.LowPass.FrequencyHz;
		Config.Crossover.LowPassOrder = (Speaker.DSP.LowPass.Slope == ESpatialFilterSlope::Slope24dB) ? 4 : 2;
		Config.Crossover.bLinkwitzRiley = (Speaker.DSP.LowPass.FilterType == ESpatialFilterType::LinkwitzRiley);
	}

	// Convert EQ bands
	for (const FSpatialEQBand& Band : Speaker.DSP.EQBands)
	{
		if (!Band.bEnabled)
		{
			continue;
		}

		FSpatialDSPEQBand DSPBand;
		DSPBand.bEnabled = Band.bEnabled;
		DSPBand.Frequency = Band.FrequencyHz;
		DSPBand.GainDb = Band.GainDb;
		DSPBand.Q = Band.Q;

		// Convert filter type
		switch (Band.Type)
		{
		case ESpatialEQBandType::LowShelf:
			DSPBand.Type = ESpatialBiquadType::LowShelf;
			break;
		case ESpatialEQBandType::HighShelf:
			DSPBand.Type = ESpatialBiquadType::HighShelf;
			break;
		case ESpatialEQBandType::Notch:
			DSPBand.Type = ESpatialBiquadType::Notch;
			break;
		case ESpatialEQBandType::AllPass:
			DSPBand.Type = ESpatialBiquadType::AllPass;
			break;
		case ESpatialEQBandType::BandPass:
			DSPBand.Type = ESpatialBiquadType::BandPass;
			break;
		case ESpatialEQBandType::Peak:
		default:
			DSPBand.Type = ESpatialBiquadType::PeakingEQ;
			break;
		}

		Config.EQBands.Add(DSPBand);
	}

	// Convert limiter settings
	Config.Limiter.bEnabled = Speaker.DSP.Limiter.bEnabled;
	Config.Limiter.ThresholdDb = Speaker.DSP.Limiter.ThresholdDb;
	Config.Limiter.AttackMs = Speaker.DSP.Limiter.AttackMs;
	Config.Limiter.ReleaseMs = Speaker.DSP.Limiter.ReleaseMs;
	Config.Limiter.KneeDb = Speaker.DSP.Limiter.KneeDb;

	return Config;
}

void URshipSpatialAudioManager::UpdateAudioEngine()
{
	// Audio engine updates are handled through the audio processor
	// This method processes any pending game-thread operations
	if (!AudioProcessor)
	{
		return;
	}

	// Process feedback from audio thread (meter data, etc.)
	FSpatialFeedbackQueue& FeedbackQueue = AudioProcessor->GetFeedbackQueue();
	FSpatialAudioFeedbackData Feedback;
	while (FeedbackQueue.Pop(Feedback))
	{
		switch (Feedback.Type)
		{
		case ESpatialAudioFeedback::MeterUpdate:
			{
				// Update speaker meter reading by index
				const int32 SpeakerIndex = Feedback.Meter.SpeakerIndex;

				// Find speaker by index
				TArray<FSpatialSpeaker> AllSpeakers = GetAllSpeakers();
				if (SpeakerIndex >= 0 && SpeakerIndex < AllSpeakers.Num())
				{
					FSpatialSpeaker* Speaker = Venue.GetSpeaker(AllSpeakers[SpeakerIndex].Id);
					if (Speaker)
					{
						// Store linear meter values
						Speaker->LastMeterReading.Peak = Feedback.Meter.PeakLevel;
						Speaker->LastMeterReading.RMS = Feedback.Meter.RMSLevel;

						// Update peak hold (decay handled elsewhere)
						if (Feedback.Meter.PeakLevel > Speaker->LastMeterReading.PeakHold)
						{
							Speaker->LastMeterReading.PeakHold = Feedback.Meter.PeakLevel;
						}

						// Detect clipping/limiting
						Speaker->LastMeterReading.bLimiting = Feedback.Meter.PeakLevel > 0.99f;
						Speaker->LastMeterReading.Timestamp = FPlatformTime::Seconds();
					}
				}
			}
			break;

		case ESpatialAudioFeedback::LimiterGRUpdate:
			{
				// Update limiter gain reduction for speaker
				const int32 SpeakerIndex = Feedback.LimiterGR.SpeakerIndex;

				TArray<FSpatialSpeaker> AllSpeakers = GetAllSpeakers();
				if (SpeakerIndex >= 0 && SpeakerIndex < AllSpeakers.Num())
				{
					FSpatialSpeaker* Speaker = Venue.GetSpeaker(AllSpeakers[SpeakerIndex].Id);
					if (Speaker)
					{
						Speaker->LastMeterReading.GainReductionDb = Feedback.LimiterGR.GainReductionDb;
					}
				}
			}
			break;

		case ESpatialAudioFeedback::BufferUnderrun:
			UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("Audio buffer underrun detected! Count: %u"), Feedback.UnderrunCount);
			break;

		case ESpatialAudioFeedback::LatencyReport:
			UE_LOG(LogRshipSpatialAudioManager, Verbose, TEXT("Audio latency: %.2fms"), Feedback.LatencyMs);
			break;

		default:
			break;
		}
	}
}

void URshipSpatialAudioManager::NotifyDSPChange(const FGuid& SpeakerId)
{
	// Send rShip update
	SendSpeakerUpdate(SpeakerId);

	// Queue DSP parameter update to audio thread
	if (!AudioProcessor)
	{
		return;
	}

	FSpatialSpeaker OutSpeaker;
	if (!GetSpeaker(SpeakerId, OutSpeaker))
	{
		return;
	}

	// Build and apply DSP config
	FSpatialSpeakerDSPConfig Config = BuildDSPConfig(OutSpeaker);
	AudioProcessor->ApplySpeakerDSPConfig(SpeakerId, Config);

	// Also queue the basic speaker DSP for quick updates
	int32* IndexPtr = SpeakerIdToIndex.Find(SpeakerId);
	if (IndexPtr)
	{
		AudioProcessor->QueueSpeakerDSP(
			*IndexPtr,
			FMath::Pow(10.0f, OutSpeaker.DSP.OutputGainDb / 20.0f),
			OutSpeaker.DSP.DelayMs,
			OutSpeaker.DSP.bMuted
		);
	}
}

void URshipSpatialAudioManager::NotifyObjectChange(const FGuid& ObjectId)
{
	// Send rShip update
	SendObjectUpdate(ObjectId);

	FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
	if (!Object)
	{
		return;
	}

	// Use rendering engine for VBAP/DBAP gain computation if available
	if (RenderingEngine)
	{
		// UpdateObject computes gains via current renderer (VBAP/DBAP)
		// and sends them to the rendering engine's internal processor
		RenderingEngine->UpdateObject(*Object);
	}
	else if (AudioProcessor)
	{
		// Fallback: just queue position update without gain computation
		AudioProcessor->QueuePositionUpdate(ObjectId, Object->Position, Object->Spread);
	}
}

// ============================================================================
// RENDERING ENGINE INTEGRATION
// ============================================================================

void URshipSpatialAudioManager::SetRenderingEngine(FSpatialRenderingEngine* Engine)
{
	// Check if we need to clear the processor reference
	FSpatialRenderingEngine* OldEngine = RenderingEngine;
	FSpatialAudioProcessor* OldProcessor = AudioProcessor;

	RenderingEngine = Engine;

	if (RenderingEngine)
	{
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Rendering engine connected"));

		// Use the rendering engine's internal processor
		AudioProcessor = RenderingEngine->GetProcessor();

		// Sync speaker configuration to rendering engine
		SyncSpeakersToRenderingEngine();

		// Set the reference point (venue origin)
		RenderingEngine->SetReferencePoint(Venue.VenueOrigin);

		// Rebuild speaker index mapping
		RebuildSpeakerIndexMapping();

		// Push current DSP state for all speakers
		TArray<FSpatialSpeaker> AllSpeakers = GetAllSpeakers();
		for (const FSpatialSpeaker& Speaker : AllSpeakers)
		{
			// Register speaker with DSP manager if using direct processor access
			if (AudioProcessor)
			{
				FSpatialSpeakerDSPManager* DSPManager = AudioProcessor->GetDSPManager();
				if (DSPManager)
				{
					DSPManager->AddSpeaker(Speaker.Id);
				}

				// Apply current configuration
				FSpatialSpeakerDSPConfig Config = BuildDSPConfig(Speaker);
				AudioProcessor->ApplySpeakerDSPConfig(Speaker.Id, Config);
			}
		}

		// Update all existing audio objects through the rendering engine
		for (const auto& Pair : AudioObjects)
		{
			RenderingEngine->UpdateObject(Pair.Value);
		}
	}
	else
	{
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Rendering engine disconnected"));
		// If we were using the old rendering engine's processor, clear it
		if (OldEngine && OldProcessor && OldProcessor == OldEngine->GetProcessor())
		{
			AudioProcessor = nullptr;
		}
		SpeakerIdToIndex.Empty();
	}
}

void URshipSpatialAudioManager::SyncSpeakersToRenderingEngine()
{
	if (!RenderingEngine)
	{
		return;
	}

	// Get all speakers and configure the rendering engine
	TArray<FSpatialSpeaker> AllSpeakers = GetAllSpeakers();

	if (AllSpeakers.Num() > 0)
	{
		// Configure speakers with current renderer type
		// This triggers Delaunay triangulation for VBAP
		RenderingEngine->ConfigureSpeakers(AllSpeakers, CurrentRendererType);

		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Synced %d speakers to rendering engine with renderer type %d"),
			AllSpeakers.Num(), static_cast<int32>(CurrentRendererType));
	}
}

void URshipSpatialAudioManager::SetGlobalRendererType(ESpatialRendererType RendererType)
{
	if (CurrentRendererType == RendererType)
	{
		return;
	}

	CurrentRendererType = RendererType;

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Global renderer type set to %d"), static_cast<int32>(RendererType));

	// Reconfigure rendering engine with new renderer type
	SyncSpeakersToRenderingEngine();

	// Re-update all audio objects with new gains
	if (RenderingEngine)
	{
		for (const auto& Pair : AudioObjects)
		{
			RenderingEngine->UpdateObject(Pair.Value);
		}
	}
}

void URshipSpatialAudioManager::SetListenerPosition(const FVector& Position)
{
	Venue.VenueOrigin = Position;

	if (RenderingEngine)
	{
		RenderingEngine->SetReferencePoint(Position);
	}
}

// ============================================================================
// SCENE INTERPOLATION
// ============================================================================

void URshipSpatialAudioManager::UpdateSceneInterpolation(float DeltaTime)
{
	if (!bSceneInterpolationActive)
	{
		return;
	}

	// Advance interpolation time
	SceneInterpolationElapsed += DeltaTime;

	// Calculate normalized interpolation factor (0.0 to 1.0)
	float Alpha = FMath::Clamp(SceneInterpolationElapsed / SceneInterpolationDuration, 0.0f, 1.0f);

	// Apply smooth easing (cubic ease in-out for professional feel)
	float EasedAlpha = Alpha < 0.5f
		? 4.0f * Alpha * Alpha * Alpha
		: 1.0f - FMath::Pow(-2.0f * Alpha + 2.0f, 3.0f) / 2.0f;

	// Interpolate speaker values
	for (auto& Pair : SpeakerInterpolationTargets)
	{
		const FGuid& SpeakerId = Pair.Key;
		const FSpeakerInterpolationTarget& Target = Pair.Value;

		FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
		if (!Speaker)
		{
			continue;
		}

		// Interpolate gains (dB space for perceptual linearity)
		Speaker->DSP.InputGainDb = FMath::Lerp(Target.StartInputGain, Target.TargetInputGain, EasedAlpha);
		Speaker->DSP.OutputGainDb = FMath::Lerp(Target.StartOutputGain, Target.TargetOutputGain, EasedAlpha);
		Speaker->DSP.DelayMs = FMath::Lerp(Target.StartDelay, Target.TargetDelay, EasedAlpha);

		// Mute state snaps at the end (if going to muted) or start (if going to unmuted)
		if (Target.bStartMuted != Target.bTargetMuted)
		{
			Speaker->DSP.bMuted = Target.bTargetMuted ? (Alpha >= 0.95f) : (Alpha <= 0.05f);
		}

		// Notify audio engine of changes
		NotifyDSPChange(SpeakerId);
	}

	// Interpolate audio object values
	for (auto& Pair : ObjectInterpolationTargets)
	{
		const FGuid& ObjectId = Pair.Key;
		const FObjectInterpolationTarget& Target = Pair.Value;

		FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
		if (!Object)
		{
			continue;
		}

		// Interpolate position
		Object->Position = FMath::Lerp(Target.StartPosition, Target.TargetPosition, EasedAlpha);

		// Interpolate other parameters
		Object->Spread = FMath::Lerp(Target.StartSpread, Target.TargetSpread, EasedAlpha);
		Object->GainDb = FMath::Lerp(Target.StartGain, Target.TargetGain, EasedAlpha);

		// Mute state snaps at the end (if going to muted) or start (if going to unmuted)
		if (Target.bStartMuted != Target.bTargetMuted)
		{
			Object->bMuted = Target.bTargetMuted ? (Alpha >= 0.95f) : (Alpha <= 0.05f);
		}

		// Notify rendering engine of position change
		NotifyObjectChange(ObjectId);
		OnObjectPositionChanged.Broadcast(ObjectId, Object->Position);
	}

	// Check if interpolation is complete
	if (SceneInterpolationElapsed >= SceneInterpolationDuration)
	{
		// Finalize all values to exact targets
		for (auto& Pair : SpeakerInterpolationTargets)
		{
			const FGuid& SpeakerId = Pair.Key;
			const FSpeakerInterpolationTarget& Target = Pair.Value;

			FSpatialSpeaker* Speaker = Venue.GetSpeaker(SpeakerId);
			if (Speaker)
			{
				Speaker->DSP.InputGainDb = Target.TargetInputGain;
				Speaker->DSP.OutputGainDb = Target.TargetOutputGain;
				Speaker->DSP.DelayMs = Target.TargetDelay;
				Speaker->DSP.bMuted = Target.bTargetMuted;
				NotifyDSPChange(SpeakerId);
				OnSpeakerUpdated.Broadcast(SpeakerId);
			}
		}

		for (auto& Pair : ObjectInterpolationTargets)
		{
			const FGuid& ObjectId = Pair.Key;
			const FObjectInterpolationTarget& Target = Pair.Value;

			FSpatialAudioObject* Object = AudioObjects.Find(ObjectId);
			if (Object)
			{
				Object->Position = Target.TargetPosition;
				Object->Spread = Target.TargetSpread;
				Object->GainDb = Target.TargetGain;
				Object->bMuted = Target.bTargetMuted;
				NotifyObjectChange(ObjectId);
			}
		}

		// Clean up
		SpeakerInterpolationTargets.Empty();
		ObjectInterpolationTargets.Empty();
		bSceneInterpolationActive = false;

		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Scene interpolation complete"));
	}
}

// ============================================================================
// EXTERNAL PROCESSOR INTEGRATION
// ============================================================================

bool URshipSpatialAudioManager::ConfigureExternalProcessor(const FExternalProcessorConfig& Config)
{
	// Disconnect existing processor if any
	DisconnectExternalProcessor();

	// Store configuration
	ExternalProcessorConfig = Config;

	// Get the global processor registry
	UExternalProcessorRegistry* Registry = GetGlobalProcessorRegistry();
	if (!Registry)
	{
		UE_LOG(LogRshipSpatialAudioManager, Error, TEXT("ConfigureExternalProcessor: Failed to get processor registry"));
		return false;
	}

	// Get or create the processor from registry
	ExternalProcessor = Registry->GetOrCreateProcessor(Config);
	if (!ExternalProcessor)
	{
		UE_LOG(LogRshipSpatialAudioManager, Error, TEXT("ConfigureExternalProcessor: Failed to create processor"));
		return false;
	}

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Configured external processor: %s at %s:%d"),
		*Config.DisplayName, *Config.Network.Host, Config.Network.SendPort);

	return true;
}

bool URshipSpatialAudioManager::ConnectExternalProcessor()
{
	if (!ExternalProcessor)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("ConnectExternalProcessor: No processor configured"));
		return false;
	}

	bool bResult = ExternalProcessor->Connect();

	if (bResult)
	{
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("External processor connection initiated"));
	}
	else
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("External processor connection failed"));
	}

	return bResult;
}

void URshipSpatialAudioManager::DisconnectExternalProcessor()
{
	if (ExternalProcessor && ExternalProcessor->IsConnected())
	{
		ExternalProcessor->Disconnect();
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("External processor disconnected"));
	}
}

bool URshipSpatialAudioManager::IsExternalProcessorConnected() const
{
	return ExternalProcessor && ExternalProcessor->IsConnected();
}

EProcessorConnectionState URshipSpatialAudioManager::GetExternalProcessorState() const
{
	if (!ExternalProcessor)
	{
		return EProcessorConnectionState::Disconnected;
	}
	return ExternalProcessor->GetStatus().ConnectionState;
}

FExternalProcessorStatus URshipSpatialAudioManager::GetExternalProcessorStatus() const
{
	if (!ExternalProcessor)
	{
		return FExternalProcessorStatus();
	}
	return ExternalProcessor->GetStatus();
}

bool URshipSpatialAudioManager::MapObjectToExternalProcessor(const FGuid& ObjectId, int32 ExternalObjectNumber, int32 MappingArea)
{
	if (!ExternalProcessor)
	{
		UE_LOG(LogRshipSpatialAudioManager, Warning, TEXT("MapObjectToExternalProcessor: No processor configured"));
		return false;
	}

	FExternalObjectMapping Mapping;
	Mapping.InternalObjectId = ObjectId;
	Mapping.ExternalObjectNumber = ExternalObjectNumber;
	Mapping.MappingNumber = MappingArea;
	Mapping.bEnabled = true;

	// Try to get display name from audio object
	if (const FSpatialAudioObject* Object = AudioObjects.Find(ObjectId))
	{
		Mapping.DisplayName = Object->Name;
	}

	bool bResult = ExternalProcessor->RegisterObjectMapping(Mapping);

	if (bResult)
	{
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Mapped object %s -> External %d (Area %d)"),
			*ObjectId.ToString(), ExternalObjectNumber, MappingArea);
	}

	return bResult;
}

bool URshipSpatialAudioManager::UnmapObjectFromExternalProcessor(const FGuid& ObjectId)
{
	if (!ExternalProcessor)
	{
		return false;
	}

	bool bResult = ExternalProcessor->UnregisterObjectMapping(ObjectId);

	if (bResult)
	{
		UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("Unmapped object %s from external processor"),
			*ObjectId.ToString());
	}

	return bResult;
}

void URshipSpatialAudioManager::SetExternalProcessorForwarding(bool bEnable)
{
	bExternalProcessorForwardingEnabled = bEnable;

	UE_LOG(LogRshipSpatialAudioManager, Log, TEXT("External processor forwarding %s"),
		bEnable ? TEXT("enabled") : TEXT("disabled"));
}

bool URshipSpatialAudioManager::SendPositionToExternalProcessor(int32 ExternalObjectNumber, const FVector& Position)
{
	if (!ExternalProcessor || !ExternalProcessor->IsConnected())
	{
		return false;
	}

	// Create a temporary mapping for direct send
	FGuid TempId = FGuid::NewGuid();

	FExternalObjectMapping TempMapping;
	TempMapping.InternalObjectId = TempId;
	TempMapping.ExternalObjectNumber = ExternalObjectNumber;
	TempMapping.bEnabled = true;

	ExternalProcessor->RegisterObjectMapping(TempMapping);
	bool bResult = ExternalProcessor->SetObjectPosition(TempId, Position);
	ExternalProcessor->UnregisterObjectMapping(TempId);

	return bResult;
}

void URshipSpatialAudioManager::UpdateExternalProcessorObjectPosition(const FGuid& ObjectId, const FVector& Position)
{
	if (!bExternalProcessorForwardingEnabled || !ExternalProcessor || !ExternalProcessor->IsConnected())
	{
		return;
	}

	if (!ExternalProcessor->IsObjectMapped(ObjectId))
	{
		return;
	}

	ExternalProcessor->SetObjectPosition(ObjectId, Position);
}

void URshipSpatialAudioManager::UpdateExternalProcessorObjectSpread(const FGuid& ObjectId, float Spread)
{
	if (!bExternalProcessorForwardingEnabled || !ExternalProcessor || !ExternalProcessor->IsConnected())
	{
		return;
	}

	if (!ExternalProcessor->IsObjectMapped(ObjectId))
	{
		return;
	}

	ExternalProcessor->SetObjectSpread(ObjectId, Spread);
}

void URshipSpatialAudioManager::UpdateExternalProcessorObjectGain(const FGuid& ObjectId, float GainDb)
{
	if (!bExternalProcessorForwardingEnabled || !ExternalProcessor || !ExternalProcessor->IsConnected())
	{
		return;
	}

	if (!ExternalProcessor->IsObjectMapped(ObjectId))
	{
		return;
	}

	ExternalProcessor->SetObjectGain(ObjectId, GainDb);
}
