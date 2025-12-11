// Copyright Rocketship. All Rights Reserved.

#include "Zones/SpatialZoneManager.h"
#include "RshipSpatialAudioRuntimeModule.h"

USpatialZoneManager::USpatialZoneManager()
	: bIsInitialized(false)
	, GlobalReferencePoint(FVector::ZeroVector)
	, bBoundaryBlending(true)
	, BoundaryBlendDistance(100.0f)  // 1 meter
{
}

void USpatialZoneManager::Initialize(const TArray<FSpatialSpeaker>& InAllSpeakers)
{
	if (bIsInitialized)
	{
		Shutdown();
	}

	AllSpeakers = InAllSpeakers;

	// Build speaker ID to index map
	SpeakerIdToIndex.Empty();
	for (int32 i = 0; i < AllSpeakers.Num(); ++i)
	{
		SpeakerIdToIndex.Add(AllSpeakers[i].Id, i);
	}

	bIsInitialized = true;

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("ZoneManager initialized with %d speakers"), AllSpeakers.Num());
}

void USpatialZoneManager::Shutdown()
{
	ZoneStates.Empty();
	ObjectZoneRouting.Empty();
	RendererRegistry.InvalidateCache();
	bIsInitialized = false;
}

FGuid USpatialZoneManager::AddZone(const FSpatialZone& Zone)
{
	FGuid ZoneId = Zone.Id.IsValid() ? Zone.Id : FGuid::NewGuid();

	FSpatialZoneState State;
	State.Zone = Zone;
	State.Zone.Id = ZoneId;

	// Build speaker list from IDs
	RebuildZoneSpeakers(State);

	// Compute bounds
	if (State.Speakers.Num() > 0)
	{
		State.Bounds = FBox(ForceInit);
		for (const FSpatialSpeaker& Speaker : State.Speakers)
		{
			State.Bounds += Speaker.Position;
		}
		// Expand bounds slightly
		State.Bounds = State.Bounds.ExpandBy(100.0f);
	}

	// Configure renderer
	ReconfigureZoneRenderer(State);

	ZoneStates.Add(ZoneId, MoveTemp(State));

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("Added zone '%s' with %d speakers, renderer %s"),
		*Zone.Name,
		State.Speakers.Num(),
		*FSpatialRendererRegistry::GetRendererTypeName(Zone.RendererType));

	return ZoneId;
}

bool USpatialZoneManager::UpdateZone(const FGuid& ZoneId, const FSpatialZone& Zone)
{
	FSpatialZoneState* State = ZoneStates.Find(ZoneId);
	if (!State)
	{
		return false;
	}

	bool bRendererChanged = (State->Zone.RendererType != Zone.RendererType);
	State->Zone = Zone;
	State->Zone.Id = ZoneId;  // Preserve ID

	RebuildZoneSpeakers(*State);

	// Recompute bounds
	if (State->Speakers.Num() > 0)
	{
		State->Bounds = FBox(ForceInit);
		for (const FSpatialSpeaker& Speaker : State->Speakers)
		{
			State->Bounds += Speaker.Position;
		}
		State->Bounds = State->Bounds.ExpandBy(100.0f);
	}

	if (bRendererChanged)
	{
		ReconfigureZoneRenderer(*State);
	}

	return true;
}

bool USpatialZoneManager::RemoveZone(const FGuid& ZoneId)
{
	return ZoneStates.Remove(ZoneId) > 0;
}

bool USpatialZoneManager::GetZone(const FGuid& ZoneId, FSpatialZone& OutZone) const
{
	const FSpatialZoneState* State = ZoneStates.Find(ZoneId);
	if (State)
	{
		OutZone = State->Zone;
		return true;
	}
	return false;
}

TArray<FSpatialZone> USpatialZoneManager::GetAllZones() const
{
	TArray<FSpatialZone> Result;
	Result.Reserve(ZoneStates.Num());
	for (const auto& Pair : ZoneStates)
	{
		Result.Add(Pair.Value.Zone);
	}
	return Result;
}

void USpatialZoneManager::SetZoneRenderer(const FGuid& ZoneId, ESpatialRendererType RendererType)
{
	FSpatialZoneState* State = ZoneStates.Find(ZoneId);
	if (State && State->Zone.RendererType != RendererType)
	{
		State->Zone.RendererType = RendererType;
		ReconfigureZoneRenderer(*State);
	}
}

void USpatialZoneManager::SetZoneSpeakers(const FGuid& ZoneId, const TArray<FGuid>& SpeakerIds)
{
	FSpatialZoneState* State = ZoneStates.Find(ZoneId);
	if (State)
	{
		State->Zone.SpeakerIds = SpeakerIds;
		RebuildZoneSpeakers(*State);

		// Reconfigure renderer with new speakers
		ReconfigureZoneRenderer(*State);
	}
}

void USpatialZoneManager::AddSpeakerToZone(const FGuid& ZoneId, const FGuid& SpeakerId)
{
	FSpatialZoneState* State = ZoneStates.Find(ZoneId);
	if (State && !State->SpeakerIds.Contains(SpeakerId))
	{
		State->Zone.SpeakerIds.Add(SpeakerId);
		RebuildZoneSpeakers(*State);
		ReconfigureZoneRenderer(*State);
	}
}

void USpatialZoneManager::RemoveSpeakerFromZone(const FGuid& ZoneId, const FGuid& SpeakerId)
{
	FSpatialZoneState* State = ZoneStates.Find(ZoneId);
	if (State)
	{
		State->Zone.SpeakerIds.Remove(SpeakerId);
		RebuildZoneSpeakers(*State);
		ReconfigureZoneRenderer(*State);
	}
}

TArray<FGuid> USpatialZoneManager::GetZonesForObject(const FSpatialAudioObject& Object) const
{
	// Check for manual routing first
	const TArray<FGuid>* ManualRouting = ObjectZoneRouting.Find(Object.Id);
	if (ManualRouting && ManualRouting->Num() > 0)
	{
		return *ManualRouting;
	}

	// Check object's own zone routing
	if (Object.ZoneRouting.Num() > 0)
	{
		return Object.ZoneRouting;
	}

	// Auto-route based on position
	TArray<FGuid> Zones = FindZonesOverlappingPosition(Object.Position);
	if (Zones.Num() > 0)
	{
		return Zones;
	}

	// Fallback: use all zones
	TArray<FGuid> AllZones;
	AllZones.Reserve(ZoneStates.Num());
	for (const auto& Pair : ZoneStates)
	{
		AllZones.Add(Pair.Key);
	}
	return AllZones;
}

FGuid USpatialZoneManager::FindZoneContainingPosition(const FVector& Position) const
{
	for (const auto& Pair : ZoneStates)
	{
		if (Pair.Value.Bounds.IsInside(Position))
		{
			return Pair.Key;
		}
	}
	return FGuid();
}

TArray<FGuid> USpatialZoneManager::FindZonesOverlappingPosition(const FVector& Position) const
{
	TArray<FGuid> Result;
	for (const auto& Pair : ZoneStates)
	{
		// Expand bounds by blend distance for overlap testing
		FBox ExpandedBounds = Pair.Value.Bounds.ExpandBy(BoundaryBlendDistance);
		if (ExpandedBounds.IsInside(Position))
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

void USpatialZoneManager::SetObjectZoneRouting(const FGuid& ObjectId, const TArray<FGuid>& ZoneIds)
{
	ObjectZoneRouting.Add(ObjectId, ZoneIds);
}

void USpatialZoneManager::ClearObjectZoneRouting(const FGuid& ObjectId)
{
	ObjectZoneRouting.Remove(ObjectId);
}

void USpatialZoneManager::ComputeGainsForObject(
	const FSpatialAudioObject& Object,
	TArray<FSpatialSpeakerGain>& OutGains)
{
	OutGains.Reset();

	TArray<FGuid> TargetZones = GetZonesForObject(Object);
	if (TargetZones.Num() == 0)
	{
		return;
	}

	// Single zone - simple case
	if (TargetZones.Num() == 1 || !bBoundaryBlending)
	{
		ComputeGainsInZone(TargetZones[0], Object.Position, Object.Spread, OutGains);
		return;
	}

	// Multiple zones - blend based on position
	TMap<int32, float> MergedGains;  // Speaker index -> accumulated gain

	float TotalWeight = 0.0f;
	for (const FGuid& ZoneId : TargetZones)
	{
		const FSpatialZoneState* State = ZoneStates.Find(ZoneId);
		if (!State)
		{
			continue;
		}

		float Weight = ComputeZoneBlendWeight(*State, Object.Position);
		if (Weight < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		TArray<FSpatialSpeakerGain> ZoneGains;
		ComputeGainsInZone(ZoneId, Object.Position, Object.Spread, ZoneGains);

		// Merge gains
		for (const FSpatialSpeakerGain& Gain : ZoneGains)
		{
			float& AccumGain = MergedGains.FindOrAdd(Gain.SpeakerIndex);
			AccumGain += Gain.Gain * Weight;

			// TODO: Merge delays (weighted average or min?)
		}

		TotalWeight += Weight;
	}

	// Normalize and build output
	if (TotalWeight > KINDA_SMALL_NUMBER)
	{
		for (const auto& Pair : MergedGains)
		{
			FSpatialSpeakerGain Gain;
			Gain.SpeakerIndex = Pair.Key;
			Gain.Gain = Pair.Value / TotalWeight;
			Gain.DelayMs = 0.0f;  // TODO: Proper delay merging

			// Look up speaker ID
			if (Pair.Key >= 0 && Pair.Key < AllSpeakers.Num())
			{
				Gain.SpeakerId = AllSpeakers[Pair.Key].Id;
			}

			OutGains.Add(Gain);
		}
	}
}

void USpatialZoneManager::ComputeGainsInZone(
	const FGuid& ZoneId,
	const FVector& Position,
	float Spread,
	TArray<FSpatialSpeakerGain>& OutGains)
{
	OutGains.Reset();

	FSpatialZoneState* State = ZoneStates.Find(ZoneId);
	if (!State || !State->Renderer)
	{
		return;
	}

	State->Renderer->ComputeGains(Position, Spread, OutGains);

	// Map local speaker indices to global indices
	for (FSpatialSpeakerGain& Gain : OutGains)
	{
		if (Gain.SpeakerIndex >= 0 && Gain.SpeakerIndex < State->Speakers.Num())
		{
			// Look up global index for this speaker
			const FSpatialSpeaker& LocalSpeaker = State->Speakers[Gain.SpeakerIndex];
			const int32* GlobalIndex = SpeakerIdToIndex.Find(LocalSpeaker.Id);
			if (GlobalIndex)
			{
				Gain.SpeakerIndex = *GlobalIndex;
			}
		}
	}
}

ISpatialRenderer* USpatialZoneManager::GetZoneRenderer(const FGuid& ZoneId)
{
	FSpatialZoneState* State = ZoneStates.Find(ZoneId);
	return State ? State->Renderer : nullptr;
}

void USpatialZoneManager::SetGlobalReferencePoint(const FVector& Point)
{
	GlobalReferencePoint = Point;

	// Update all zone renderers
	RendererRegistry.SetVBAPConfig(false, GlobalReferencePoint, true);

	for (auto& Pair : ZoneStates)
	{
		ReconfigureZoneRenderer(Pair.Value);
	}
}

void USpatialZoneManager::SetBoundaryBlending(bool bEnabled, float BlendDistance)
{
	bBoundaryBlending = bEnabled;
	BoundaryBlendDistance = FMath::Max(0.0f, BlendDistance);
}

FString USpatialZoneManager::GetDiagnosticInfo() const
{
	FString Info;
	Info += FString::Printf(TEXT("Zone Manager\n"));
	Info += FString::Printf(TEXT("  Initialized: %s\n"), bIsInitialized ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Total Speakers: %d\n"), AllSpeakers.Num());
	Info += FString::Printf(TEXT("  Zones: %d\n"), ZoneStates.Num());
	Info += FString::Printf(TEXT("  Boundary Blending: %s (%.1f cm)\n"),
		bBoundaryBlending ? TEXT("On") : TEXT("Off"), BoundaryBlendDistance);

	for (const auto& Pair : ZoneStates)
	{
		const FSpatialZoneState& State = Pair.Value;
		Info += FString::Printf(TEXT("\n  Zone '%s':\n"), *State.Zone.Name);
		Info += FString::Printf(TEXT("    Speakers: %d\n"), State.Speakers.Num());
		Info += FString::Printf(TEXT("    Renderer: %s\n"),
			*FSpatialRendererRegistry::GetRendererTypeName(State.Zone.RendererType));
		Info += FString::Printf(TEXT("    Active: %s\n"), State.bIsActive ? TEXT("Yes") : TEXT("No"));
		Info += FString::Printf(TEXT("    Bounds: (%.0f, %.0f, %.0f) - (%.0f, %.0f, %.0f)\n"),
			State.Bounds.Min.X, State.Bounds.Min.Y, State.Bounds.Min.Z,
			State.Bounds.Max.X, State.Bounds.Max.Y, State.Bounds.Max.Z);
	}

	return Info;
}

void USpatialZoneManager::ReconfigureZoneRenderer(FSpatialZoneState& State)
{
	if (State.Speakers.Num() < 2)
	{
		State.Renderer = nullptr;
		return;
	}

	// Get or create renderer for this zone's type
	FSpatialRendererConfig Config;
	Config.RendererType = State.Zone.RendererType;
	Config.bPhaseCoherent = true;

	State.Renderer = RendererRegistry.GetOrCreateRenderer(
		State.Zone.RendererType,
		State.Speakers,
		Config);
}

void USpatialZoneManager::RebuildZoneSpeakers(FSpatialZoneState& State)
{
	State.Speakers.Empty();
	State.SpeakerIds.Empty();

	for (const FGuid& SpeakerId : State.Zone.SpeakerIds)
	{
		if (const FSpatialSpeaker* Speaker = GetSpeakerById(SpeakerId))
		{
			State.Speakers.Add(*Speaker);
			State.SpeakerIds.Add(SpeakerId);
		}
	}
}

const FSpatialSpeaker* USpatialZoneManager::GetSpeakerById(const FGuid& SpeakerId) const
{
	const int32* Index = SpeakerIdToIndex.Find(SpeakerId);
	if (Index && *Index >= 0 && *Index < AllSpeakers.Num())
	{
		return &AllSpeakers[*Index];
	}
	return nullptr;
}

float USpatialZoneManager::ComputeZoneBlendWeight(const FSpatialZoneState& State, const FVector& Position) const
{
	if (!bBoundaryBlending || BoundaryBlendDistance < KINDA_SMALL_NUMBER)
	{
		// Binary containment
		return State.Bounds.IsInside(Position) ? 1.0f : 0.0f;
	}

	// Distance-based blend
	FVector ClosestPoint = State.Bounds.GetClosestPointTo(Position);
	float Distance = FVector::Dist(Position, ClosestPoint);

	if (State.Bounds.IsInside(Position))
	{
		return 1.0f;
	}

	if (Distance > BoundaryBlendDistance)
	{
		return 0.0f;
	}

	// Linear falloff
	return 1.0f - (Distance / BoundaryBlendDistance);
}

void USpatialZoneManager::MergeGains(
	TArray<FSpatialSpeakerGain>& OutGains,
	const TArray<FSpatialSpeakerGain>& NewGains,
	float Weight)
{
	for (const FSpatialSpeakerGain& NewGain : NewGains)
	{
		bool bFound = false;
		for (FSpatialSpeakerGain& Existing : OutGains)
		{
			if (Existing.SpeakerIndex == NewGain.SpeakerIndex)
			{
				Existing.Gain += NewGain.Gain * Weight;
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			FSpatialSpeakerGain Weighted = NewGain;
			Weighted.Gain *= Weight;
			OutGains.Add(Weighted);
		}
	}
}
