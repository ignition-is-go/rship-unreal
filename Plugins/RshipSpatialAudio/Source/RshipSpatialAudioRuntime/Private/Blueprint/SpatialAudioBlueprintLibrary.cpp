// Copyright Rocketship. All Rights Reserved.

#include "Blueprint/SpatialAudioBlueprintLibrary.h"
#include "RshipSpatialAudioManager.h"
#include "ExternalProcessor/ExternalProcessorRegistry.h"
#include "Core/SpatialZone.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"  // For TActorIterator

// ============================================================================
// MANAGER ACCESS
// ============================================================================

URshipSpatialAudioManager* USpatialAudioBlueprintLibrary::GetSpatialAudioManager(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Get from game instance subsystem or create singleton
	// This is a simplified accessor - production code might use subsystems
	static TWeakObjectPtr<URshipSpatialAudioManager> CachedManager;

	if (!CachedManager.IsValid())
	{
		CachedManager = NewObject<URshipSpatialAudioManager>(GetTransientPackage(), NAME_None, RF_MarkAsRootSet);
		// Initialize without subsystem (Myko features won't be available)
		CachedManager->Initialize(nullptr);
	}

	return CachedManager.Get();
}

// ============================================================================
// QUICK SETUP
// ============================================================================

TArray<FGuid> USpatialAudioBlueprintLibrary::CreateStereoPair(
	UObject* WorldContextObject,
	float Distance,
	float Height)
{
	TArray<FGuid> SpeakerIds;

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return SpeakerIds;
	}

	// Left speaker
	FSpatialSpeaker LeftSpeaker;
	LeftSpeaker.Id = FGuid::NewGuid();
	LeftSpeaker.Name = TEXT("Left");
	LeftSpeaker.WorldPosition = FVector(-Distance, 0.0f, Height);
	LeftSpeaker.OutputChannel = 1;
	LeftSpeaker.Type = ESpatialSpeakerType::PointSource;

	// Right speaker
	FSpatialSpeaker RightSpeaker;
	RightSpeaker.Id = FGuid::NewGuid();
	RightSpeaker.Name = TEXT("Right");
	RightSpeaker.WorldPosition = FVector(Distance, 0.0f, Height);
	RightSpeaker.OutputChannel = 2;
	RightSpeaker.Type = ESpatialSpeakerType::PointSource;

	Manager->AddSpeaker(LeftSpeaker);
	Manager->AddSpeaker(RightSpeaker);

	SpeakerIds.Add(LeftSpeaker.Id);
	SpeakerIds.Add(RightSpeaker.Id);

	return SpeakerIds;
}

TArray<FGuid> USpatialAudioBlueprintLibrary::Create51SurroundLayout(
	UObject* WorldContextObject,
	float Radius,
	float Height)
{
	TArray<FGuid> SpeakerIds;

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return SpeakerIds;
	}

	// ITU-R BS.775-1 standard 5.1 layout
	struct FSpeakerDef
	{
		const TCHAR* Name;
		float Angle;  // Degrees from front
		int32 Channel;
	};

	const FSpeakerDef Speakers51[] = {
		{ TEXT("Left"),        -30.0f,  1 },
		{ TEXT("Right"),        30.0f,  2 },
		{ TEXT("Center"),        0.0f,  3 },
		{ TEXT("LFE"),           0.0f,  4 },  // LFE typically at front center
		{ TEXT("Left Surr"),  -110.0f,  5 },
		{ TEXT("Right Surr"),  110.0f,  6 }
	};

	for (const FSpeakerDef& Def : Speakers51)
	{
		float AngleRad = FMath::DegreesToRadians(Def.Angle);

		FSpatialSpeaker Speaker;
		Speaker.Id = FGuid::NewGuid();
		Speaker.Name = Def.Name;
		Speaker.WorldPosition = FVector(
			FMath::Cos(AngleRad) * Radius,
			FMath::Sin(AngleRad) * Radius,
			(Def.Channel == 4) ? 0.0f : Height  // LFE on floor
		);
		Speaker.OutputChannel = Def.Channel;
		Speaker.Type = (Def.Channel == 4) ? ESpatialSpeakerType::Subwoofer : ESpatialSpeakerType::PointSource;

		Manager->AddSpeaker(Speaker);
		SpeakerIds.Add(Speaker.Id);
	}

	return SpeakerIds;
}

TArray<FGuid> USpatialAudioBlueprintLibrary::CreateSpeakerRing(
	UObject* WorldContextObject,
	int32 NumSpeakers,
	float Radius,
	float Height)
{
	TArray<FGuid> SpeakerIds;

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager || NumSpeakers <= 0)
	{
		return SpeakerIds;
	}

	for (int32 i = 0; i < NumSpeakers; ++i)
	{
		// Start from front (0 degrees) and go clockwise
		float Angle = (float(i) / float(NumSpeakers)) * 2.0f * PI;

		FSpatialSpeaker Speaker;
		Speaker.Id = FGuid::NewGuid();
		Speaker.Name = FString::Printf(TEXT("Speaker_%d"), i + 1);
		Speaker.WorldPosition = FVector(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			Height
		);
		Speaker.OutputChannel = i + 1;
		Speaker.Type = ESpatialSpeakerType::PointSource;

		Manager->AddSpeaker(Speaker);
		SpeakerIds.Add(Speaker.Id);
	}

	return SpeakerIds;
}

TArray<FGuid> USpatialAudioBlueprintLibrary::CreateSpeakerDome(
	UObject* WorldContextObject,
	int32 NumRings,
	int32 SpeakersPerRing,
	float Radius)
{
	TArray<FGuid> SpeakerIds;

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager || NumRings <= 0 || SpeakersPerRing <= 0)
	{
		return SpeakerIds;
	}

	int32 ChannelIndex = 1;

	// Create rings from bottom to top
	for (int32 Ring = 0; Ring < NumRings; ++Ring)
	{
		// Elevation from 0 (equator) to 90 (zenith)
		float Elevation = (float(Ring + 1) / float(NumRings + 1)) * (PI / 2.0f);
		float RingRadius = Radius * FMath::Cos(Elevation);
		float RingHeight = Radius * FMath::Sin(Elevation);

		// Stagger every other ring for better coverage
		float AngleOffset = (Ring % 2 == 1) ? (PI / SpeakersPerRing) : 0.0f;

		for (int32 i = 0; i < SpeakersPerRing; ++i)
		{
			float Azimuth = AngleOffset + (float(i) / float(SpeakersPerRing)) * 2.0f * PI;

			FSpatialSpeaker Speaker;
			Speaker.Id = FGuid::NewGuid();
			Speaker.Name = FString::Printf(TEXT("Dome_R%d_S%d"), Ring + 1, i + 1);
			Speaker.WorldPosition = FVector(
				FMath::Cos(Azimuth) * RingRadius,
				FMath::Sin(Azimuth) * RingRadius,
				RingHeight
			);
			Speaker.OutputChannel = ChannelIndex++;
			Speaker.Type = ESpatialSpeakerType::PointSource;

			Manager->AddSpeaker(Speaker);
			SpeakerIds.Add(Speaker.Id);
		}
	}

	// Add zenith speaker
	FSpatialSpeaker ZenithSpeaker;
	ZenithSpeaker.Id = FGuid::NewGuid();
	ZenithSpeaker.Name = TEXT("Dome_Zenith");
	ZenithSpeaker.WorldPosition = FVector(0.0f, 0.0f, Radius);
	ZenithSpeaker.OutputChannel = ChannelIndex;
	ZenithSpeaker.Type = ESpatialSpeakerType::PointSource;

	Manager->AddSpeaker(ZenithSpeaker);
	SpeakerIds.Add(ZenithSpeaker.Id);

	return SpeakerIds;
}

// ============================================================================
// AUDIO OBJECT HELPERS
// ============================================================================

FGuid USpatialAudioBlueprintLibrary::CreateAudioObjectForActor(
	UObject* WorldContextObject,
	AActor* ActorToFollow,
	const FString& Name)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager || !ActorToFollow)
	{
		return FGuid();
	}

	FSpatialAudioObject Object;
	Object.Id = FGuid::NewGuid();
	Object.Name = Name.IsEmpty() ? ActorToFollow->GetName() : Name;
	Object.Position = ActorToFollow->GetActorLocation();

	Manager->AddObject(Object);

	return Object.Id;
}

void USpatialAudioBlueprintLibrary::SetAudioObjectPath(
	UObject* WorldContextObject,
	const FGuid& ObjectId,
	const TArray<FVector>& PathPoints,
	float Duration,
	bool bLoop)
{
	// This would typically store path data for later interpolation
	// The actual implementation depends on how your path system works

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager || !ObjectId.IsValid() || PathPoints.Num() < 2 || Duration <= 0.0f)
	{
		return;
	}

	// Store path metadata on the object
	// In a full implementation, you'd have a path animation system
	UE_LOG(LogTemp, Log, TEXT("SpatialAudio: Set path for object %s with %d points over %.2fs (loop: %s)"),
		*ObjectId.ToString(), PathPoints.Num(), Duration, bLoop ? TEXT("yes") : TEXT("no"));
}

TArray<FVector> USpatialAudioBlueprintLibrary::GetAllSpeakerPositions(UObject* WorldContextObject)
{
	TArray<FVector> Positions;

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return Positions;
	}

	TArray<FSpatialSpeaker> Speakers = Manager->GetAllSpeakers();
	Positions.Reserve(Speakers.Num());

	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		Positions.Add(Speaker.WorldPosition);
	}

	return Positions;
}

TArray<FVector> USpatialAudioBlueprintLibrary::GetAllAudioObjectPositions(UObject* WorldContextObject)
{
	TArray<FVector> Positions;

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return Positions;
	}

	TArray<FSpatialAudioObject> Objects = Manager->GetAllAudioObjects();
	Positions.Reserve(Objects.Num());

	for (const FSpatialAudioObject& Object : Objects)
	{
		Positions.Add(Object.Position);
	}

	return Positions;
}

// ============================================================================
// EXTERNAL PROCESSOR HELPERS
// ============================================================================

bool USpatialAudioBlueprintLibrary::QuickConnectDS100(
	UObject* WorldContextObject,
	const FString& IPAddress,
	int32 SendPort,
	int32 ReceivePort)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return false;
	}

	FExternalProcessorConfig Config;
	Config.ProcessorType = EExternalProcessorType::DS100;
	Config.DisplayName = TEXT("DS100");
	Config.Network.Host = IPAddress;
	Config.Network.SendPort = SendPort;
	Config.Network.ReceivePort = ReceivePort;
	Config.Network.bAutoReconnect = true;

	// DS100-specific defaults
	Config.DS100.DefaultMappingArea = EDS100MappingArea::MappingArea1;
	Config.DS100.bUse3DPositioning = true;
	Config.DS100.bSendEnSpace = true;

	if (!Manager->ConfigureExternalProcessor(Config))
	{
		return false;
	}

	return Manager->ConnectExternalProcessor();
}

int32 USpatialAudioBlueprintLibrary::AutoMapObjectsToDS100(UObject* WorldContextObject)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return 0;
	}

	TArray<FSpatialAudioObject> Objects = Manager->GetAllAudioObjects();
	int32 MappedCount = 0;

	// Map objects sequentially to DS100 sources
	for (int32 i = 0; i < Objects.Num() && i < 64; ++i)  // DS100 supports 64 sources
	{
		if (Manager->MapObjectToExternalProcessor(Objects[i].Id, i + 1, 1))
		{
			++MappedCount;
		}
	}

	if (MappedCount > 0)
	{
		Manager->SetExternalProcessorForwarding(true);
	}

	return MappedCount;
}

// ============================================================================
// RENDERER HELPERS
// ============================================================================

void USpatialAudioBlueprintLibrary::SetRendererByName(UObject* WorldContextObject, const FString& RendererName)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return;
	}

	ESpatialRendererType Type = ESpatialRendererType::VBAP;  // Default

	if (RendererName.Equals(TEXT("VBAP"), ESearchCase::IgnoreCase))
	{
		Type = ESpatialRendererType::VBAP;
	}
	else if (RendererName.Equals(TEXT("DBAP"), ESearchCase::IgnoreCase))
	{
		Type = ESpatialRendererType::DBAP;
	}
	else if (RendererName.Equals(TEXT("HOA"), ESearchCase::IgnoreCase) ||
	         RendererName.Equals(TEXT("Ambisonics"), ESearchCase::IgnoreCase))
	{
		Type = ESpatialRendererType::HOA;
	}
	else if (RendererName.Equals(TEXT("Direct"), ESearchCase::IgnoreCase) ||
	         RendererName.Equals(TEXT("DirectRouting"), ESearchCase::IgnoreCase))
	{
		Type = ESpatialRendererType::Direct;
	}

	Manager->SetGlobalRendererType(Type);
}

FString USpatialAudioBlueprintLibrary::GetCurrentRendererName(UObject* WorldContextObject)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return TEXT("None");
	}

	switch (Manager->GetGlobalRendererType())
	{
	case ESpatialRendererType::VBAP:
		return TEXT("VBAP");
	case ESpatialRendererType::DBAP:
		return TEXT("DBAP");
	case ESpatialRendererType::HOA:
		return TEXT("HOA");
	case ESpatialRendererType::Direct:
		return TEXT("Direct");
	default:
		return TEXT("Unknown");
	}
}

// ============================================================================
// DSP HELPERS
// ============================================================================

void USpatialAudioBlueprintLibrary::SetAllSpeakersGain(UObject* WorldContextObject, float GainDb)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return;
	}

	TArray<FSpatialSpeaker> Speakers = Manager->GetAllSpeakers();
	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		Manager->SetSpeakerGain(Speaker.Id, GainDb);
	}
}

void USpatialAudioBlueprintLibrary::MuteAllSpeakers(UObject* WorldContextObject, bool bMute)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return;
	}

	TArray<FSpatialSpeaker> Speakers = Manager->GetAllSpeakers();
	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		Manager->SetSpeakerMute(Speaker.Id, bMute);
	}
}

void USpatialAudioBlueprintLibrary::AutoAlignSpeakerDelays(
	UObject* WorldContextObject,
	FVector ReferencePoint,
	float SpeedOfSound)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager || SpeedOfSound <= 0.0f)
	{
		return;
	}

	TArray<FSpatialSpeaker> Speakers = Manager->GetAllSpeakers();
	if (Speakers.Num() == 0)
	{
		return;
	}

	// Find the furthest speaker distance
	float MaxDistance = 0.0f;
	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		float Distance = FVector::Dist(Speaker.WorldPosition, ReferencePoint);
		MaxDistance = FMath::Max(MaxDistance, Distance);
	}

	// Apply delays so all speakers are time-aligned to the furthest one
	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		float Distance = FVector::Dist(Speaker.WorldPosition, ReferencePoint);
		float DeltaDistance = MaxDistance - Distance;
		float DelayMs = DistanceToDelayMs(DeltaDistance, SpeedOfSound);

		Manager->SetSpeakerDelay(Speaker.Id, DelayMs);
	}

	UE_LOG(LogTemp, Log, TEXT("SpatialAudio: Auto-aligned %d speakers (max distance: %.1f cm, max delay: %.2f ms)"),
		Speakers.Num(), MaxDistance, DistanceToDelayMs(MaxDistance, SpeedOfSound));
}

// ============================================================================
// SCENE HELPERS
// ============================================================================

FString USpatialAudioBlueprintLibrary::StoreCurrentScene(UObject* WorldContextObject, const FString& SceneName)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return FString();
	}

	return Manager->StoreScene(SceneName);
}

bool USpatialAudioBlueprintLibrary::RecallSceneWithFade(
	UObject* WorldContextObject,
	const FString& SceneId,
	float FadeTimeSeconds)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return false;
	}

	// Convert seconds to milliseconds and enable interpolation if fade time > 0
	bool bInterpolate = FadeTimeSeconds > 0.0f;
	float FadeTimeMs = FadeTimeSeconds * 1000.0f;

	return Manager->RecallScene(SceneId, bInterpolate, FadeTimeMs);
}

// ============================================================================
// CONVERSION UTILITIES
// ============================================================================

float USpatialAudioBlueprintLibrary::DbToLinear(float Db)
{
	// Handle negative infinity (silence)
	if (Db <= -96.0f)
	{
		return 0.0f;
	}

	return FMath::Pow(10.0f, Db / 20.0f);
}

float USpatialAudioBlueprintLibrary::LinearToDb(float Linear)
{
	// Handle zero/negative values
	if (Linear <= 0.0f)
	{
		return -96.0f;  // Practical silence
	}

	return 20.0f * FMath::LogX(10.0f, Linear);
}

int32 USpatialAudioBlueprintLibrary::MsToSamples(float Ms, float SampleRate)
{
	if (SampleRate <= 0.0f)
	{
		return 0;
	}

	return FMath::RoundToInt((Ms / 1000.0f) * SampleRate);
}

float USpatialAudioBlueprintLibrary::SamplesToMs(int32 Samples, float SampleRate)
{
	if (SampleRate <= 0.0f)
	{
		return 0.0f;
	}

	return (float(Samples) / SampleRate) * 1000.0f;
}

float USpatialAudioBlueprintLibrary::DistanceToDelayMs(float DistanceCm, float SpeedOfSoundCmPerSec)
{
	if (SpeedOfSoundCmPerSec <= 0.0f)
	{
		return 0.0f;
	}

	// Time = Distance / Speed, convert to milliseconds
	return (DistanceCm / SpeedOfSoundCmPerSec) * 1000.0f;
}

// ============================================================================
// ADVANCED SETUP HELPERS
// ============================================================================

TArray<FGuid> USpatialAudioBlueprintLibrary::CreateLineArray(
	UObject* WorldContextObject,
	FVector StartPosition,
	FVector EndPosition,
	int32 NumSpeakers,
	const FString& ArrayName)
{
	TArray<FGuid> SpeakerIds;

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager || NumSpeakers <= 0)
	{
		return SpeakerIds;
	}

	// Calculate step between speakers
	FVector Step = (NumSpeakers > 1) ? (EndPosition - StartPosition) / float(NumSpeakers - 1) : FVector::ZeroVector;

	for (int32 i = 0; i < NumSpeakers; ++i)
	{
		FSpatialSpeaker Speaker;
		Speaker.Id = FGuid::NewGuid();
		Speaker.Name = FString::Printf(TEXT("%s_%d"), *ArrayName, i + 1);
		Speaker.WorldPosition = StartPosition + Step * float(i);
		Speaker.OutputChannel = i + 1;
		Speaker.Type = ESpatialSpeakerType::LineArrayElement;

		Manager->AddSpeaker(Speaker);
		SpeakerIds.Add(Speaker.Id);
	}

	return SpeakerIds;
}

FGuid USpatialAudioBlueprintLibrary::CreateZoneWithSpeakers(
	UObject* WorldContextObject,
	const FString& ZoneName,
	const TArray<FGuid>& SpeakerIds,
	ESpatialRendererType RendererType)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return FGuid();
	}

	FSpatialZone Zone;
	Zone.Id = FGuid::NewGuid();
	Zone.Name = ZoneName;
	Zone.RendererType = RendererType;
	Zone.SpeakerIds = SpeakerIds;

	FGuid ZoneId = Manager->AddZone(Zone);

	// Update each speaker's zone reference
	for (const FGuid& SpeakerId : SpeakerIds)
	{
		Manager->AddSpeakerToZone(SpeakerId, ZoneId);
	}

	return ZoneId;
}

TArray<FGuid> USpatialAudioBlueprintLibrary::CreateSpeakersAtPositions(
	UObject* WorldContextObject,
	const TArray<FVector>& Positions,
	const FString& NamePrefix)
{
	TArray<FGuid> SpeakerIds;

	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return SpeakerIds;
	}

	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		FSpatialSpeaker Speaker;
		Speaker.Id = FGuid::NewGuid();
		Speaker.Name = FString::Printf(TEXT("%s_%d"), *NamePrefix, i + 1);
		Speaker.WorldPosition = Positions[i];
		Speaker.OutputChannel = i + 1;
		Speaker.Type = ESpatialSpeakerType::PointSource;

		Manager->AddSpeaker(Speaker);
		SpeakerIds.Add(Speaker.Id);
	}

	return SpeakerIds;
}

int32 USpatialAudioBlueprintLibrary::AutoAssignOutputChannels(UObject* WorldContextObject, int32 StartChannel)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return 0;
	}

	TArray<FSpatialSpeaker> Speakers = Manager->GetAllSpeakers();
	int32 Channel = StartChannel;

	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		FSpatialSpeaker UpdatedSpeaker = Speaker;
		UpdatedSpeaker.OutputChannel = Channel++;
		Manager->UpdateSpeaker(Speaker.Id, UpdatedSpeaker);
	}

	return Speakers.Num();
}

// ============================================================================
// STATUS & DIAGNOSTICS
// ============================================================================

bool USpatialAudioBlueprintLibrary::IsSystemReady(UObject* WorldContextObject)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	return Manager && Manager->IsSystemReady();
}

FSpatialAudioSystemStatus USpatialAudioBlueprintLibrary::GetSystemStatus(UObject* WorldContextObject)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return FSpatialAudioSystemStatus();
	}
	return Manager->GetSystemStatus();
}

bool USpatialAudioBlueprintLibrary::GetClosestSpeaker(
	UObject* WorldContextObject,
	FVector Position,
	FGuid& OutSpeakerId,
	float& OutDistance)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return false;
	}

	FSpatialSpeaker ClosestSpeaker;
	if (Manager->FindClosestSpeaker(Position, ClosestSpeaker))
	{
		OutSpeakerId = ClosestSpeaker.Id;
		OutDistance = FVector::Dist(Position, ClosestSpeaker.WorldPosition);
		return true;
	}
	return false;
}

void USpatialAudioBlueprintLibrary::GetEntityCounts(
	UObject* WorldContextObject,
	int32& OutSpeakerCount,
	int32& OutZoneCount,
	int32& OutObjectCount)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		OutSpeakerCount = 0;
		OutZoneCount = 0;
		OutObjectCount = 0;
		return;
	}

	OutSpeakerCount = Manager->GetSpeakerCount();
	OutZoneCount = Manager->GetZoneCount();
	OutObjectCount = Manager->GetAudioObjectCount();
}

// ============================================================================
// OBJECT MANAGEMENT HELPERS
// ============================================================================

int32 USpatialAudioBlueprintLibrary::CreateObjectsForTaggedActors(
	UObject* WorldContextObject,
	FName ActorTag)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager || !WorldContextObject)
	{
		return 0;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 CreatedCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->Tags.Contains(ActorTag))
		{
			FSpatialAudioObject Object;
			Object.Id = FGuid::NewGuid();
			Object.Name = Actor->GetName();
			Object.Position = Actor->GetActorLocation();

			Manager->AddObject(Object);
			++CreatedCount;
		}
	}

	return CreatedCount;
}

int32 USpatialAudioBlueprintLibrary::RouteAllObjectsToZone(UObject* WorldContextObject, const FGuid& ZoneId)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager || !ZoneId.IsValid())
	{
		return 0;
	}

	TArray<FSpatialAudioObject> Objects = Manager->GetAllAudioObjects();
	int32 RoutedCount = 0;

	for (const FSpatialAudioObject& Object : Objects)
	{
		if (Manager->AddObjectToZone(Object.Id, ZoneId))
		{
			++RoutedCount;
		}
	}

	return RoutedCount;
}

void USpatialAudioBlueprintLibrary::ClearAllObjectRouting(UObject* WorldContextObject)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		return;
	}

	TArray<FSpatialAudioObject> Objects = Manager->GetAllAudioObjects();
	for (const FSpatialAudioObject& Object : Objects)
	{
		TArray<FGuid> EmptyRouting;
		Manager->SetObjectZoneRouting(Object.Id, EmptyRouting);
	}
}

// ============================================================================
// DEBUGGING HELPERS
// ============================================================================

void USpatialAudioBlueprintLibrary::PrintSystemStatus(UObject* WorldContextObject)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpatialAudio: Manager not available"));
		return;
	}

	FSpatialAudioSystemStatus Status = Manager->GetSystemStatus();

	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("SPATIAL AUDIO SYSTEM STATUS"));
	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("System Ready: %s"), Status.bIsReady ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Log, TEXT("Venue: %s"), *Status.VenueName);
	UE_LOG(LogTemp, Log, TEXT("Speakers: %d"), Status.SpeakerCount);
	UE_LOG(LogTemp, Log, TEXT("Zones: %d"), Status.ZoneCount);
	UE_LOG(LogTemp, Log, TEXT("Arrays: %d"), Status.ArrayCount);
	UE_LOG(LogTemp, Log, TEXT("Objects: %d"), Status.ObjectCount);
	UE_LOG(LogTemp, Log, TEXT("Scenes: %d"), Status.SceneCount);
	UE_LOG(LogTemp, Log, TEXT("Audio Processor: %s"), Status.bHasAudioProcessor ? TEXT("Connected") : TEXT("Not connected"));
	UE_LOG(LogTemp, Log, TEXT("Rendering Engine: %s"), Status.bHasRenderingEngine ? TEXT("Connected") : TEXT("Not connected"));
	UE_LOG(LogTemp, Log, TEXT("External Processor: %s"), Status.bExternalProcessorConnected ? TEXT("Connected") : TEXT("Not connected"));
	UE_LOG(LogTemp, Log, TEXT("Myko Registered: %s"), Status.bMykoRegistered ? TEXT("YES") : TEXT("NO"));

	if (Status.Warnings.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Warnings:"));
		for (const FString& Warning : Status.Warnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Warning);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

void USpatialAudioBlueprintLibrary::TestAllSpeakers(UObject* WorldContextObject, float DurationPerSpeaker)
{
	URshipSpatialAudioManager* Manager = GetSpatialAudioManager(WorldContextObject);
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpatialAudio: Cannot test speakers - manager not available"));
		return;
	}

	// Note: This is a placeholder - actual implementation would need to interact
	// with the audio processor to generate test tones
	TArray<FSpatialSpeaker> Speakers = Manager->GetAllSpeakers();
	UE_LOG(LogTemp, Log, TEXT("SpatialAudio: Would test %d speakers (%.2fs each)"), Speakers.Num(), DurationPerSpeaker);

	for (int32 i = 0; i < Speakers.Num(); ++i)
	{
		UE_LOG(LogTemp, Log, TEXT("  [%d] %s at (%0.f, %.0f, %.0f) -> Channel %d"),
			i + 1,
			*Speakers[i].Name,
			Speakers[i].WorldPosition.X,
			Speakers[i].WorldPosition.Y,
			Speakers[i].WorldPosition.Z,
			Speakers[i].OutputChannel);
	}
}
