// Copyright Rocketship. All Rights Reserved.

#include "ExternalProcessor/DS100Processor.h"
#include "TimerManager.h"
#include "Engine/World.h"

// DS100 OSC address constants
namespace DS100Addresses
{
	// Coordinate mapping
	const FString SourcePositionXY = TEXT("/coordinatemapping/source_position_xy");
	const FString SourcePosition = TEXT("/coordinatemapping/source_position");
	const FString SourceSpread = TEXT("/coordinatemapping/source_spread");
	const FString SourceDelayMode = TEXT("/coordinatemapping/source_delaymode");

	// Matrix input
	const FString MatrixInputGain = TEXT("/matrixinput/gain");
	const FString MatrixInputMute = TEXT("/matrixinput/mute");
	const FString MatrixInputReverbSendGain = TEXT("/matrixinput/reverbsendgain");
	const FString MatrixInputDelayMode = TEXT("/matrixinput/delaymode");

	// Matrix output
	const FString MatrixOutputGain = TEXT("/matrixoutput/gain");
	const FString MatrixOutputMute = TEXT("/matrixoutput/mute");

	// En-Space
	const FString EnSpaceRoom = TEXT("/enspace/room");
	const FString EnSpacePreset = TEXT("/enspace/preset");

	// Device
	const FString DeviceName = TEXT("/device/name");
	const FString DeviceStatus = TEXT("/device/status");
}

// ============================================================================
// FDS100Processor
// ============================================================================

FDS100Processor::FDS100Processor()
{
}

FDS100Processor::~FDS100Processor()
{
	if (IsInitialized())
	{
		Shutdown();
	}
}

bool FDS100Processor::Initialize(const FExternalProcessorConfig& InConfig)
{
	if (!FExternalSpatialProcessorBase::Initialize(InConfig))
	{
		return false;
	}

	// Create OSC client
	OSCClient = MakeUnique<FOSCClient>();

	// Set up message handling
	OSCClient->OnMessageReceived.BindRaw(this, &FDS100Processor::HandleReceivedOSCMessage);

	OSCClient->OnConnectionStateChanged.BindLambda([this](bool bConnected)
	{
		SetConnectionState(bConnected
			? EProcessorConnectionState::Connected
			: EProcessorConnectionState::Disconnected);
	});

	OSCClient->OnError.BindLambda([this](const FString& Error)
	{
		ReportError(Error);
	});

	// Apply rate limits
	OSCClient->SetRateLimits(
		Config.RateLimit.MaxMessagesPerSecond,
		Config.RateLimit.MaxBundleSizeBytes);

	OSCClient->SetBundlingEnabled(Config.RateLimit.bUseBundling);

	// Initialize source params cache from config mappings
	{
		FScopeLock Lock(&SourceParamsLock);
		for (const FExternalObjectMapping& Mapping : Config.ObjectMappings)
		{
			FDS100ObjectParams Params;
			Params.SourceId = Mapping.ExternalObjectNumber;
			Params.MappingArea = DS100Config.DefaultMappingArea;
			Params.EnSpaceSend = DS100Config.GlobalEnSpaceSend;
			Params.Spread = 0.5f;
			Params.DelayMode = 1;

			SourceParamsCache.Add(Mapping.InternalObjectId, Params);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("DS100: Initialized - %s at %s:%d"),
		*DS100Config.DeviceName, *Config.Network.Host, Config.Network.SendPort);

	return true;
}

void FDS100Processor::Shutdown()
{
	// Stop heartbeat
	if (HeartbeatTimerHandle.IsValid())
	{
		// Can't easily cancel timer without world reference - will be cleaned up
		HeartbeatTimerHandle.Invalidate();
	}

	// Disconnect
	if (OSCClient && OSCClient->IsInitialized())
	{
		OSCClient->Shutdown();
	}
	OSCClient.Reset();

	{
		FScopeLock Lock(&SourceParamsLock);
		SourceParamsCache.Empty();
	}

	FExternalSpatialProcessorBase::Shutdown();
}

bool FDS100Processor::Connect()
{
	if (!bInitialized)
	{
		ReportError(TEXT("Cannot connect - not initialized"));
		return false;
	}

	if (!OSCClient)
	{
		ReportError(TEXT("OSC client not created"));
		return false;
	}

	SetConnectionState(EProcessorConnectionState::Connecting);

	bool bSuccess = OSCClient->Initialize(
		Config.Network.Host,
		Config.Network.SendPort,
		Config.Network.ReceivePort);

	if (!bSuccess)
	{
		SetConnectionState(EProcessorConnectionState::Error);
		ReportError(TEXT("Failed to initialize OSC connection"));
		return false;
	}

	// Send initial heartbeat/status request
	SendHeartbeat();

	SetConnectionState(EProcessorConnectionState::Connected);

	UE_LOG(LogTemp, Log, TEXT("DS100: Connected to %s:%d"),
		*Config.Network.Host, Config.Network.SendPort);

	return true;
}

void FDS100Processor::Disconnect()
{
	if (OSCClient && OSCClient->IsInitialized())
	{
		OSCClient->Shutdown();
	}

	SetConnectionState(EProcessorConnectionState::Disconnected);

	UE_LOG(LogTemp, Log, TEXT("DS100: Disconnected"));
}

bool FDS100Processor::IsConnected() const
{
	if (!OSCClient)
	{
		return false;
	}
	return OSCClient->IsConnected();
}

bool FDS100Processor::SetObjectPosition(const FGuid& ObjectId, const FVector& Position)
{
	int32 SourceId = GetDS100SourceId(ObjectId);
	if (SourceId < 0)
	{
		return false;
	}

	EDS100MappingArea MappingArea = GetObjectMappingArea(ObjectId);
	int32 MappingAreaInt = static_cast<int32>(MappingArea);

	// Check if we should send update (threshold check)
	if (!ShouldSendPositionUpdate(ObjectId, Position))
	{
		return true;  // Skip update but not an error
	}

	// Convert to DS100 coordinates
	FVector DS100Pos = ConvertToDS100Coordinates(Position);

	// Build OSC message
	FSpatialOSCMessage Msg;

	if (DS100Config.bUseXYOnly)
	{
		// XY only (2D)
		Msg.Address = BuildPositionXYAddress(MappingAreaInt, SourceId);
		Msg.AddInt(MappingAreaInt);
		Msg.AddInt(SourceId);
		Msg.AddFloat(DS100Pos.X);
		Msg.AddFloat(DS100Pos.Y);
	}
	else
	{
		// XYZ (3D)
		Msg.Address = BuildPositionXYZAddress(MappingAreaInt, SourceId);
		Msg.AddInt(MappingAreaInt);
		Msg.AddInt(SourceId);
		Msg.AddFloat(DS100Pos.X);
		Msg.AddFloat(DS100Pos.Y);
		Msg.AddFloat(DS100Pos.Z);
	}

	bool bSuccess = QueueMessage(Msg);

	if (bSuccess)
	{
		// Update last position
		FScopeLock Lock(&PositionsLock);
		LastPositions.Add(ObjectId, Position);
	}

	return bSuccess;
}

bool FDS100Processor::SetObjectPositionAndSpread(const FGuid& ObjectId, const FVector& Position, float Spread)
{
	// For DS100, we send these as separate messages
	bool bPosResult = SetObjectPosition(ObjectId, Position);
	bool bSpreadResult = SetObjectSpread(ObjectId, Spread);
	return bPosResult && bSpreadResult;
}

bool FDS100Processor::SetObjectSpread(const FGuid& ObjectId, float Spread)
{
	int32 SourceId = GetDS100SourceId(ObjectId);
	if (SourceId < 0)
	{
		return false;
	}

	EDS100MappingArea MappingArea = GetObjectMappingArea(ObjectId);
	int32 MappingAreaInt = static_cast<int32>(MappingArea);

	FSpatialOSCMessage Msg;
	Msg.Address = BuildSpreadAddress(MappingAreaInt, SourceId);
	Msg.AddInt(MappingAreaInt);
	Msg.AddInt(SourceId);
	Msg.AddFloat(FMath::Clamp(Spread, 0.0f, 1.0f));

	// Update cache
	{
		FScopeLock Lock(&SourceParamsLock);
		if (FDS100ObjectParams* Params = SourceParamsCache.Find(ObjectId))
		{
			Params->Spread = Spread;
		}
	}

	return QueueMessage(Msg);
}

bool FDS100Processor::SetObjectGain(const FGuid& ObjectId, float GainDb)
{
	int32 SourceId = GetDS100SourceId(ObjectId);
	if (SourceId < 0)
	{
		return false;
	}

	return SetMatrixInputGain(SourceId, GainDb);
}

bool FDS100Processor::SetObjectReverbSend(const FGuid& ObjectId, float SendLevel)
{
	int32 SourceId = GetDS100SourceId(ObjectId);
	if (SourceId < 0)
	{
		return false;
	}

	return SetSourceEnSpaceSend(SourceId, SendLevel);
}

bool FDS100Processor::SetObjectMute(const FGuid& ObjectId, bool bMute)
{
	int32 SourceId = GetDS100SourceId(ObjectId);
	if (SourceId < 0)
	{
		return false;
	}

	return SetMatrixInputMute(SourceId, bMute);
}

bool FDS100Processor::SendOSCMessage(const FSpatialOSCMessage& Message)
{
	if (!OSCClient || !OSCClient->IsInitialized())
	{
		return false;
	}
	return OSCClient->Send(Message);
}

bool FDS100Processor::SendOSCBundle(const FSpatialOSCBundle& Bundle)
{
	if (!OSCClient || !OSCClient->IsInitialized())
	{
		return false;
	}
	return OSCClient->Send(Bundle);
}

FString FDS100Processor::GetDiagnosticInfo() const
{
	FString Info = FString::Printf(
		TEXT("DS100 Processor: %s\n")
		TEXT("  Host: %s:%d\n")
		TEXT("  Status: %s\n")
		TEXT("  Coordinate Mode: %s\n")
		TEXT("  Default Mapping Area: %d\n")
		TEXT("  Messages Sent: %lld\n")
		TEXT("  Messages Received: %lld\n"),
		*DS100Config.DeviceName,
		*Config.Network.Host,
		Config.Network.SendPort,
		IsConnected() ? TEXT("Connected") : TEXT("Disconnected"),
		DS100Config.bUseXYOnly ? TEXT("XY (2D)") : TEXT("XYZ (3D)"),
		static_cast<int32>(DS100Config.DefaultMappingArea),
		OSCClient ? OSCClient->GetMessagesSent() : 0,
		OSCClient ? OSCClient->GetMessagesReceived() : 0);

	{
		FScopeLock Lock(&MappingsLock);
		Info += FString::Printf(TEXT("  Object Mappings: %d\n"), ObjectMappings.Num());
	}

	return Info;
}

TArray<FString> FDS100Processor::GetCapabilities() const
{
	TArray<FString> Caps = FExternalSpatialProcessorBase::GetCapabilities();
	Caps.Append({
		TEXT("PositionXY"),
		TEXT("PositionXYZ"),
		TEXT("Spread"),
		TEXT("DelayMode"),
		TEXT("EnSpaceReverb"),
		TEXT("MatrixGain"),
		TEXT("MatrixMute"),
		TEXT("MappingAreas"),
		TEXT("64Sources")
	});
	return Caps;
}

void FDS100Processor::SetDS100Config(const FDS100Config& InConfig)
{
	DS100Config = InConfig;
}

bool FDS100Processor::SetSourceDelayMode(int32 SourceId, int32 DelayMode)
{
	if (!ValidateSourceId(SourceId))
	{
		return false;
	}

	FSpatialOSCMessage Msg;
	Msg.Address = BuildDelayModeAddress(SourceId);
	Msg.AddInt(SourceId);
	Msg.AddInt(FMath::Clamp(DelayMode, 0, 2));

	return QueueMessage(Msg);
}

bool FDS100Processor::SetSourceEnSpaceSend(int32 SourceId, float SendLevel)
{
	if (!ValidateSourceId(SourceId))
	{
		return false;
	}

	// Convert 0-1 to dB (-120 to 0)
	float GainDb = (SendLevel <= 0.0f)
		? -120.0f
		: FMath::Clamp(20.0f * FMath::Loge(SendLevel) / FMath::Loge(10.0f), -120.0f, 24.0f);

	FSpatialOSCMessage Msg;
	Msg.Address = BuildReverbSendAddress(SourceId);
	Msg.AddInt(SourceId);
	Msg.AddFloat(GainDb);

	return QueueMessage(Msg);
}

bool FDS100Processor::SetMatrixInputGain(int32 InputChannel, float GainDb)
{
	if (InputChannel < 1 || InputChannel > 64)
	{
		return false;
	}

	FSpatialOSCMessage Msg;
	Msg.Address = BuildMatrixInputGainAddress(InputChannel);
	Msg.AddInt(InputChannel);
	Msg.AddFloat(FMath::Clamp(GainDb, -120.0f, 24.0f));

	return QueueMessage(Msg);
}

bool FDS100Processor::SetMatrixInputMute(int32 InputChannel, bool bMute)
{
	if (InputChannel < 1 || InputChannel > 64)
	{
		return false;
	}

	FSpatialOSCMessage Msg;
	Msg.Address = BuildMatrixInputMuteAddress(InputChannel);
	Msg.AddInt(InputChannel);
	Msg.AddInt(bMute ? 1 : 0);

	return QueueMessage(Msg);
}

bool FDS100Processor::SetMatrixOutputGain(int32 OutputChannel, float GainDb)
{
	FSpatialOSCMessage Msg;
	Msg.Address = BuildMatrixOutputGainAddress(OutputChannel);
	Msg.AddInt(OutputChannel);
	Msg.AddFloat(FMath::Clamp(GainDb, -120.0f, 24.0f));

	return QueueMessage(Msg);
}

bool FDS100Processor::RequestSourcePosition(int32 SourceId, int32 MappingArea)
{
	if (!ValidateSourceId(SourceId) || !ValidateMappingArea(MappingArea))
	{
		return false;
	}

	// DS100 uses empty argument list to request current value
	FSpatialOSCMessage Msg;
	if (DS100Config.bUseXYOnly)
	{
		Msg.Address = BuildPositionXYAddress(MappingArea, SourceId);
	}
	else
	{
		Msg.Address = BuildPositionXYZAddress(MappingArea, SourceId);
	}
	Msg.AddInt(MappingArea);
	Msg.AddInt(SourceId);

	return QueueMessage(Msg);
}

bool FDS100Processor::SetEnSpaceRoom(int32 RoomId)
{
	if (RoomId < 1 || RoomId > 9)
	{
		return false;
	}

	FSpatialOSCMessage Msg;
	Msg.Address = DS100Config.OSCPrefix + DS100Addresses::EnSpaceRoom;
	Msg.AddInt(RoomId);

	return QueueMessage(Msg);
}

bool FDS100Processor::SetObjectMappingArea(const FGuid& ObjectId, EDS100MappingArea MappingArea)
{
	FScopeLock Lock(&SourceParamsLock);

	if (FDS100ObjectParams* Params = SourceParamsCache.Find(ObjectId))
	{
		Params->MappingArea = MappingArea;
		return true;
	}

	return false;
}

int32 FDS100Processor::GetDS100SourceId(const FGuid& ObjectId) const
{
	// First check explicit mapping
	int32 ExternalNum = GetExternalObjectNumber(ObjectId);
	if (ExternalNum > 0)
	{
		return ExternalNum;
	}

	// Then check source params cache
	FScopeLock Lock(&SourceParamsLock);
	if (const FDS100ObjectParams* Params = SourceParamsCache.Find(ObjectId))
	{
		return Params->SourceId;
	}

	return -1;
}

EDS100MappingArea FDS100Processor::GetObjectMappingArea(const FGuid& ObjectId) const
{
	FScopeLock Lock(&SourceParamsLock);

	if (const FDS100ObjectParams* Params = SourceParamsCache.Find(ObjectId))
	{
		return Params->MappingArea;
	}

	return DS100Config.DefaultMappingArea;
}

bool FDS100Processor::SendQueuedMessages(const TArray<FSpatialOSCMessage>& Messages)
{
	if (!OSCClient || !OSCClient->IsInitialized())
	{
		return false;
	}

	if (Messages.Num() == 0)
	{
		return true;
	}

	if (Messages.Num() == 1)
	{
		return OSCClient->Send(Messages[0]);
	}

	// Multiple messages - bundle them
	return OSCClient->SendBundle(Messages);
}

void FDS100Processor::SendHeartbeat()
{
	// Request device status as heartbeat
	FSpatialOSCMessage Msg;
	Msg.Address = DS100Config.OSCPrefix + DS100Addresses::DeviceStatus;
	QueueMessage(Msg);
}

void FDS100Processor::HandleReceivedOSCMessage(const FSpatialOSCMessage& Message)
{
	// Update stats
	{
		FScopeLock Lock(&StateLock);
		MessagesReceived++;
		LastCommunicationTime = FDateTime::UtcNow();
	}

	// Check for position responses
	if (Message.Address.Contains(TEXT("source_position")))
	{
		HandlePositionResponse(Message);
	}

	// Forward to delegates
	if (OnOSCMessageReceived.IsBound())
	{
		AsyncTask(ENamedThreads::GameThread, [this, Message]()
		{
			OnOSCMessageReceived.Broadcast(GetType(), Message);
		});
	}
}

void FDS100Processor::HandlePositionResponse(const FSpatialOSCMessage& Message)
{
	// Parse position response
	// Format: /dbaudio1/coordinatemapping/source_position_xy <mapping> <source> <x> <y>

	if (Message.Arguments.Num() < 4)
	{
		return;
	}

	int32 MappingArea = Message.Arguments[0].IntValue;
	int32 SourceId = Message.Arguments[1].IntValue;
	float X = Message.Arguments[2].FloatValue;
	float Y = Message.Arguments[3].FloatValue;
	float Z = (Message.Arguments.Num() > 4) ? Message.Arguments[4].FloatValue : 0.0f;

	FVector DS100Pos(X, Y, Z);
	FVector UnrealPos = ConvertFromDS100Coordinates(DS100Pos);

	UE_LOG(LogTemp, Verbose, TEXT("DS100: Received position for source %d: (%.2f, %.2f, %.2f)"),
		SourceId, UnrealPos.X, UnrealPos.Y, UnrealPos.Z);
}

FString FDS100Processor::BuildPositionXYAddress(int32 MappingArea, int32 SourceId) const
{
	return DS100Config.OSCPrefix + DS100Addresses::SourcePositionXY;
}

FString FDS100Processor::BuildPositionXYZAddress(int32 MappingArea, int32 SourceId) const
{
	return DS100Config.OSCPrefix + DS100Addresses::SourcePosition;
}

FString FDS100Processor::BuildSpreadAddress(int32 MappingArea, int32 SourceId) const
{
	return DS100Config.OSCPrefix + DS100Addresses::SourceSpread;
}

FString FDS100Processor::BuildDelayModeAddress(int32 SourceId) const
{
	return DS100Config.OSCPrefix + DS100Addresses::MatrixInputDelayMode;
}

FString FDS100Processor::BuildReverbSendAddress(int32 SourceId) const
{
	return DS100Config.OSCPrefix + DS100Addresses::MatrixInputReverbSendGain;
}

FString FDS100Processor::BuildMatrixInputGainAddress(int32 Channel) const
{
	return DS100Config.OSCPrefix + DS100Addresses::MatrixInputGain;
}

FString FDS100Processor::BuildMatrixInputMuteAddress(int32 Channel) const
{
	return DS100Config.OSCPrefix + DS100Addresses::MatrixInputMute;
}

FString FDS100Processor::BuildMatrixOutputGainAddress(int32 Channel) const
{
	return DS100Config.OSCPrefix + DS100Addresses::MatrixOutputGain;
}

FVector FDS100Processor::ConvertToDS100Coordinates(const FVector& UnrealPosition) const
{
	// Use the coordinate mapping from config
	return Config.CoordinateMapping.ConvertPosition(UnrealPosition);
}

FVector FDS100Processor::ConvertFromDS100Coordinates(const FVector& DS100Position) const
{
	return Config.CoordinateMapping.ConvertPositionToUnreal(DS100Position);
}

bool FDS100Processor::ValidateSourceId(int32 SourceId) const
{
	if (SourceId < 1 || SourceId > 64)
	{
		UE_LOG(LogTemp, Warning, TEXT("DS100: Invalid source ID %d (must be 1-64)"), SourceId);
		return false;
	}
	return true;
}

bool FDS100Processor::ValidateMappingArea(int32 MappingArea) const
{
	if (MappingArea < 1 || MappingArea > 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("DS100: Invalid mapping area %d (must be 1-4)"), MappingArea);
		return false;
	}
	return true;
}
