// Copyright Rocketship. All Rights Reserved.

#include "ExternalProcessor/IExternalSpatialProcessor.h"

// ============================================================================
// FExternalSpatialProcessorBase
// ============================================================================

FExternalSpatialProcessorBase::FExternalSpatialProcessorBase()
	: bInitialized(false)
	, ConnectionState(EProcessorConnectionState::Disconnected)
	, bInBatch(false)
	, MessagesSent(0)
	, MessagesReceived(0)
{
}

FExternalSpatialProcessorBase::~FExternalSpatialProcessorBase()
{
	if (bInitialized)
	{
		Shutdown();
	}
}

bool FExternalSpatialProcessorBase::Initialize(const FExternalProcessorConfig& InConfig)
{
	FScopeLock Lock(&StateLock);

	if (bInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("ExternalProcessor: Already initialized"));
		return false;
	}

	Config = InConfig;
	bInitialized = true;
	ConnectionState = EProcessorConnectionState::Disconnected;
	MessagesSent = 0;
	MessagesReceived = 0;

	// Initialize object mappings from config
	{
		FScopeLock MappingsGuard(&MappingsLock);
		ObjectMappings.Empty();
		for (const FExternalObjectMapping& Mapping : Config.ObjectMappings)
		{
			ObjectMappings.Add(Mapping.InternalObjectId, Mapping);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ExternalProcessor: Initialized %s at %s:%d"),
		*Config.DisplayName, *Config.Network.Host, Config.Network.SendPort);

	return true;
}

void FExternalSpatialProcessorBase::Shutdown()
{
	FScopeLock Lock(&StateLock);

	if (!bInitialized)
	{
		return;
	}

	// Disconnect if connected
	if (ConnectionState != EProcessorConnectionState::Disconnected)
	{
		Disconnect();
	}

	bInitialized = false;
	ConnectionState = EProcessorConnectionState::Disconnected;

	{
		FScopeLock MappingsGuard(&MappingsLock);
		ObjectMappings.Empty();
	}

	{
		FScopeLock PositionsGuard(&PositionsLock);
		LastPositions.Empty();
	}

	{
		FScopeLock BatchGuard(&BatchLock);
		BatchedMessages.Empty();
		bInBatch = false;
	}

	UE_LOG(LogTemp, Log, TEXT("ExternalProcessor: Shutdown complete"));
}

bool FExternalSpatialProcessorBase::IsConnected() const
{
	FScopeLock Lock(&StateLock);
	return ConnectionState == EProcessorConnectionState::Connected;
}

FExternalProcessorStatus FExternalSpatialProcessorBase::GetStatus() const
{
	FScopeLock Lock(&StateLock);

	FExternalProcessorStatus Status;
	Status.ConnectionState = ConnectionState;
	Status.MessagesSent = MessagesSent;
	Status.MessagesReceived = MessagesReceived;
	Status.LastCommunicationTime = LastCommunicationTime;

	{
		FScopeLock MappingsGuard(&MappingsLock);
		Status.ActiveMappings = ObjectMappings.Num();
	}

	return Status;
}

bool FExternalSpatialProcessorBase::RegisterObjectMapping(const FExternalObjectMapping& Mapping)
{
	FScopeLock Lock(&MappingsLock);

	if (!Mapping.InternalObjectId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ExternalProcessor: Cannot register mapping with invalid internal ID"));
		return false;
	}

	ObjectMappings.Add(Mapping.InternalObjectId, Mapping);

	UE_LOG(LogTemp, Log, TEXT("ExternalProcessor: Registered mapping %s -> External %d (Mapping %d)"),
		*Mapping.InternalObjectId.ToString(), Mapping.ExternalObjectNumber, Mapping.MappingNumber);

	return true;
}

bool FExternalSpatialProcessorBase::UnregisterObjectMapping(const FGuid& InternalObjectId)
{
	FScopeLock Lock(&MappingsLock);

	if (ObjectMappings.Remove(InternalObjectId) > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("ExternalProcessor: Unregistered mapping for %s"),
			*InternalObjectId.ToString());
		return true;
	}

	return false;
}

int32 FExternalSpatialProcessorBase::GetExternalObjectNumber(const FGuid& InternalObjectId) const
{
	FScopeLock Lock(&MappingsLock);

	if (const FExternalObjectMapping* Mapping = ObjectMappings.Find(InternalObjectId))
	{
		return Mapping->ExternalObjectNumber;
	}

	return -1;
}

bool FExternalSpatialProcessorBase::IsObjectMapped(const FGuid& ObjectId) const
{
	FScopeLock Lock(&MappingsLock);
	return ObjectMappings.Contains(ObjectId);
}

TArray<FExternalObjectMapping> FExternalSpatialProcessorBase::GetAllMappings() const
{
	FScopeLock Lock(&MappingsLock);

	TArray<FExternalObjectMapping> Result;
	ObjectMappings.GenerateValueArray(Result);
	return Result;
}

void FExternalSpatialProcessorBase::BeginBatch()
{
	FScopeLock Lock(&BatchLock);
	bInBatch = true;
	BatchedMessages.Empty();
}

void FExternalSpatialProcessorBase::EndBatch()
{
	TArray<FSpatialOSCMessage> MessagesToSend;

	{
		FScopeLock Lock(&BatchLock);
		if (!bInBatch)
		{
			return;
		}

		MessagesToSend = MoveTemp(BatchedMessages);
		BatchedMessages.Empty();
		bInBatch = false;
	}

	if (MessagesToSend.Num() > 0)
	{
		SendQueuedMessages(MessagesToSend);
	}
}

int32 FExternalSpatialProcessorBase::SetObjectPositionsBatch(const TMap<FGuid, FVector>& Updates)
{
	int32 Count = 0;

	BeginBatch();

	for (const auto& Pair : Updates)
	{
		if (SetObjectPosition(Pair.Key, Pair.Value))
		{
			++Count;
		}
	}

	EndBatch();

	return Count;
}

TArray<FString> FExternalSpatialProcessorBase::GetCapabilities() const
{
	// Base capabilities - derived classes add more
	return TArray<FString>{
		TEXT("PositionXYZ"),
		TEXT("Spread"),
		TEXT("Gain"),
		TEXT("Mute"),
		TEXT("OSCRaw")
	};
}

TArray<FString> FExternalSpatialProcessorBase::Validate() const
{
	TArray<FString> Errors;

	if (!bInitialized)
	{
		Errors.Add(TEXT("Processor not initialized"));
		return Errors;
	}

	// Validate network config
	if (Config.Network.Host.IsEmpty())
	{
		Errors.Add(TEXT("Host address is empty"));
	}

	if (Config.Network.SendPort <= 0 || Config.Network.SendPort > 65535)
	{
		Errors.Add(FString::Printf(TEXT("Invalid send port: %d"), Config.Network.SendPort));
	}

	if (Config.Network.ReceivePort <= 0 || Config.Network.ReceivePort > 65535)
	{
		Errors.Add(FString::Printf(TEXT("Invalid receive port: %d"), Config.Network.ReceivePort));
	}

	// Validate scale factor
	if (Config.CoordinateMapping.ScaleFactor <= 0.0f)
	{
		Errors.Add(TEXT("Scale factor must be positive"));
	}

	// Check mappings
	{
		FScopeLock Lock(&MappingsLock);
		if (ObjectMappings.Num() == 0)
		{
			Errors.Add(TEXT("No object mappings configured"));
		}

		// Check for duplicate external object numbers
		TSet<int32> UsedNumbers;
		for (const auto& Pair : ObjectMappings)
		{
			if (UsedNumbers.Contains(Pair.Value.ExternalObjectNumber))
			{
				Errors.Add(FString::Printf(TEXT("Duplicate external object number: %d"),
					Pair.Value.ExternalObjectNumber));
			}
			UsedNumbers.Add(Pair.Value.ExternalObjectNumber);
		}
	}

	return Errors;
}

void FExternalSpatialProcessorBase::SetConnectionState(EProcessorConnectionState NewState)
{
	EProcessorConnectionState OldState;

	{
		FScopeLock Lock(&StateLock);
		if (ConnectionState == NewState)
		{
			return;
		}
		OldState = ConnectionState;
		ConnectionState = NewState;
	}

	UE_LOG(LogTemp, Log, TEXT("ExternalProcessor: Connection state changed from %d to %d"),
		static_cast<int32>(OldState), static_cast<int32>(NewState));

	// Broadcast on game thread
	if (OnConnectionStateChanged.IsBound())
	{
		AsyncTask(ENamedThreads::GameThread, [this, NewState]()
		{
			OnConnectionStateChanged.Broadcast(GetType(), NewState);
		});
	}
}

void FExternalSpatialProcessorBase::ReportError(const FString& Error)
{
	{
		FScopeLock Lock(&StateLock);
		// Could store LastError here
	}

	UE_LOG(LogTemp, Error, TEXT("ExternalProcessor: %s"), *Error);

	// Broadcast on game thread
	if (OnError.IsBound())
	{
		AsyncTask(ENamedThreads::GameThread, [this, Error]()
		{
			OnError.Broadcast(GetType(), Error);
		});
	}
}

bool FExternalSpatialProcessorBase::ShouldSendPositionUpdate(const FGuid& ObjectId, const FVector& NewPosition) const
{
	FScopeLock Lock(&PositionsLock);

	if (const FVector* LastPosition = LastPositions.Find(ObjectId))
	{
		// Convert to processor coordinates for threshold comparison
		FVector LastProcessorPos = Config.CoordinateMapping.ConvertPosition(*LastPosition);
		FVector NewProcessorPos = Config.CoordinateMapping.ConvertPosition(NewPosition);

		float DistanceSquared = FVector::DistSquared(LastProcessorPos, NewProcessorPos);
		float ThresholdSquared = Config.RateLimit.PositionChangeThreshold * Config.RateLimit.PositionChangeThreshold;

		return DistanceSquared >= ThresholdSquared;
	}

	// No previous position - always send
	return true;
}

bool FExternalSpatialProcessorBase::QueueMessage(const FSpatialOSCMessage& Message)
{
	FScopeLock Lock(&BatchLock);

	if (bInBatch)
	{
		BatchedMessages.Add(Message);
		return true;
	}

	// Not batching - send immediately
	TArray<FSpatialOSCMessage> SingleMessage;
	SingleMessage.Add(Message);
	return SendQueuedMessages(SingleMessage);
}
