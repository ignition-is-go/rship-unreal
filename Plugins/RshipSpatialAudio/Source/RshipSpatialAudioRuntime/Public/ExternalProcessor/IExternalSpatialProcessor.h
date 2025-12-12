// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExternalProcessorTypes.h"

/**
 * Interface for external spatial audio processors.
 *
 * External processors are hardware or software systems that handle the actual
 * speaker routing and DSP (e.g., d&b DS100, L-Acoustics L-ISA, Meyer Spacemap).
 * This interface provides a common API for controlling object positions, gains,
 * and other parameters across different processor types.
 *
 * Usage:
 *   // Create processor
 *   TUniquePtr<IExternalSpatialProcessor> Processor = CreateDS100Processor();
 *
 *   // Configure
 *   FExternalProcessorConfig Config;
 *   Config.Network.Host = TEXT("192.168.1.100");
 *   Processor->Initialize(Config);
 *
 *   // Connect
 *   Processor->Connect();
 *
 *   // Update positions
 *   Processor->SetObjectPosition(ObjectGuid, FVector(100, 50, 0));
 *
 * Thread Safety:
 * - Initialize/Shutdown must be called from game thread
 * - Connect/Disconnect should be called from game thread
 * - SetObject* methods can be called from any thread (internally synchronized)
 * - Status queries are thread-safe
 */
class RSHIPSPATIALAUDIORUNTIME_API IExternalSpatialProcessor
{
public:
	virtual ~IExternalSpatialProcessor() = default;

	// ========================================================================
	// LIFECYCLE
	// ========================================================================

	/**
	 * Initialize the processor with configuration.
	 * Must be called before Connect().
	 *
	 * @param Config Processor configuration.
	 * @return True if initialization succeeded.
	 */
	virtual bool Initialize(const FExternalProcessorConfig& Config) = 0;

	/**
	 * Shutdown the processor and release resources.
	 * Automatically disconnects if connected.
	 */
	virtual void Shutdown() = 0;

	/**
	 * Check if processor is initialized.
	 */
	virtual bool IsInitialized() const = 0;

	/**
	 * Get the current configuration.
	 */
	virtual const FExternalProcessorConfig& GetConfig() const = 0;

	// ========================================================================
	// CONNECTION
	// ========================================================================

	/**
	 * Connect to the external processor.
	 * Non-blocking - connection happens asynchronously.
	 * Monitor GetStatus() or bind to OnConnectionStateChanged for results.
	 *
	 * @return True if connection attempt started successfully.
	 */
	virtual bool Connect() = 0;

	/**
	 * Disconnect from the external processor.
	 */
	virtual void Disconnect() = 0;

	/**
	 * Check if currently connected.
	 */
	virtual bool IsConnected() const = 0;

	/**
	 * Get current processor status.
	 */
	virtual FExternalProcessorStatus GetStatus() const = 0;

	// ========================================================================
	// OBJECT CONTROL
	// ========================================================================

	/**
	 * Set the position of an audio object.
	 * Position is in Unreal world coordinates - will be converted per config.
	 *
	 * @param ObjectId Internal object identifier.
	 * @param Position World position in Unreal coordinates (cm).
	 * @return True if position was queued for sending.
	 */
	virtual bool SetObjectPosition(const FGuid& ObjectId, const FVector& Position) = 0;

	/**
	 * Set the position and spread of an audio object.
	 *
	 * @param ObjectId Internal object identifier.
	 * @param Position World position in Unreal coordinates (cm).
	 * @param Spread Source spread/width (0-1, processor-specific interpretation).
	 * @return True if update was queued.
	 */
	virtual bool SetObjectPositionAndSpread(const FGuid& ObjectId, const FVector& Position, float Spread) = 0;

	/**
	 * Set only the spread of an audio object.
	 *
	 * @param ObjectId Internal object identifier.
	 * @param Spread Source spread/width (0-1).
	 * @return True if update was queued.
	 */
	virtual bool SetObjectSpread(const FGuid& ObjectId, float Spread) = 0;

	/**
	 * Set the gain/level of an audio object.
	 *
	 * @param ObjectId Internal object identifier.
	 * @param GainDb Gain in decibels.
	 * @return True if update was queued.
	 */
	virtual bool SetObjectGain(const FGuid& ObjectId, float GainDb) = 0;

	/**
	 * Set reverb send level for an audio object.
	 *
	 * @param ObjectId Internal object identifier.
	 * @param SendLevel Reverb send level (0-1).
	 * @return True if update was queued.
	 */
	virtual bool SetObjectReverbSend(const FGuid& ObjectId, float SendLevel) = 0;

	/**
	 * Mute/unmute an audio object.
	 *
	 * @param ObjectId Internal object identifier.
	 * @param bMute True to mute.
	 * @return True if update was queued.
	 */
	virtual bool SetObjectMute(const FGuid& ObjectId, bool bMute) = 0;

	// ========================================================================
	// BATCH OPERATIONS
	// ========================================================================

	/**
	 * Begin a batch update for multiple objects.
	 * Updates are collected and sent as a single bundle.
	 * Call EndBatch() to send.
	 */
	virtual void BeginBatch() = 0;

	/**
	 * End batch update and send collected messages.
	 */
	virtual void EndBatch() = 0;

	/**
	 * Update multiple object positions at once.
	 * More efficient than individual SetObjectPosition calls.
	 *
	 * @param Updates Map of ObjectId to Position.
	 * @return Number of updates queued.
	 */
	virtual int32 SetObjectPositionsBatch(const TMap<FGuid, FVector>& Updates) = 0;

	// ========================================================================
	// OBJECT MAPPING
	// ========================================================================

	/**
	 * Register an object mapping between internal ID and external processor ID.
	 *
	 * @param Mapping The mapping configuration.
	 * @return True if mapping was registered.
	 */
	virtual bool RegisterObjectMapping(const FExternalObjectMapping& Mapping) = 0;

	/**
	 * Remove an object mapping.
	 *
	 * @param InternalObjectId Internal object identifier.
	 * @return True if mapping was removed.
	 */
	virtual bool UnregisterObjectMapping(const FGuid& InternalObjectId) = 0;

	/**
	 * Get the external object number for an internal ID.
	 *
	 * @param InternalObjectId Internal object identifier.
	 * @return External object number, or -1 if not mapped.
	 */
	virtual int32 GetExternalObjectNumber(const FGuid& InternalObjectId) const = 0;

	/**
	 * Check if an object is mapped.
	 */
	virtual bool IsObjectMapped(const FGuid& ObjectId) const = 0;

	/**
	 * Get all registered mappings.
	 */
	virtual TArray<FExternalObjectMapping> GetAllMappings() const = 0;

	// ========================================================================
	// RAW OSC ACCESS
	// ========================================================================

	/**
	 * Send a raw OSC message to the processor.
	 * Use for processor-specific commands not covered by the standard API.
	 *
	 * @param Message The OSC message to send.
	 * @return True if message was queued.
	 */
	virtual bool SendOSCMessage(const FSpatialOSCMessage& Message) = 0;

	/**
	 * Send a raw OSC bundle to the processor.
	 *
	 * @param Bundle The OSC bundle to send.
	 * @return True if bundle was queued.
	 */
	virtual bool SendOSCBundle(const FSpatialOSCBundle& Bundle) = 0;

	// ========================================================================
	// METADATA
	// ========================================================================

	/**
	 * Get the processor type.
	 */
	virtual EExternalProcessorType GetType() const = 0;

	/**
	 * Get the processor name.
	 */
	virtual FString GetName() const = 0;

	/**
	 * Get processor capabilities/features.
	 */
	virtual TArray<FString> GetCapabilities() const = 0;

	/**
	 * Get the maximum number of audio objects supported.
	 */
	virtual int32 GetMaxObjects() const = 0;

	// ========================================================================
	// DIAGNOSTICS
	// ========================================================================

	/**
	 * Get diagnostic information for debugging.
	 */
	virtual FString GetDiagnosticInfo() const = 0;

	/**
	 * Validate configuration and connection.
	 * @return Array of error/warning messages.
	 */
	virtual TArray<FString> Validate() const = 0;

	// ========================================================================
	// EVENTS (implemented by derived classes)
	// ========================================================================

	// Derived classes should implement these as delegates:
	// - OnConnectionStateChanged(EProcessorConnectionState NewState)
	// - OnError(const FString& ErrorMessage)
	// - OnOSCMessageReceived(const FSpatialOSCMessage& Message)
};

/**
 * Base implementation of IExternalSpatialProcessor with common functionality.
 * Derived classes should implement processor-specific behavior.
 */
class RSHIPSPATIALAUDIORUNTIME_API FExternalSpatialProcessorBase : public IExternalSpatialProcessor
{
public:
	FExternalSpatialProcessorBase();
	virtual ~FExternalSpatialProcessorBase() override;

	// ========================================================================
	// IExternalSpatialProcessor - Common Implementation
	// ========================================================================

	virtual bool Initialize(const FExternalProcessorConfig& Config) override;
	virtual void Shutdown() override;
	virtual bool IsInitialized() const override { return bInitialized; }
	virtual const FExternalProcessorConfig& GetConfig() const override { return Config; }

	virtual bool IsConnected() const override;
	virtual FExternalProcessorStatus GetStatus() const override;

	virtual bool RegisterObjectMapping(const FExternalObjectMapping& Mapping) override;
	virtual bool UnregisterObjectMapping(const FGuid& InternalObjectId) override;
	virtual int32 GetExternalObjectNumber(const FGuid& InternalObjectId) const override;
	virtual bool IsObjectMapped(const FGuid& ObjectId) const override;
	virtual TArray<FExternalObjectMapping> GetAllMappings() const override;

	virtual void BeginBatch() override;
	virtual void EndBatch() override;
	virtual int32 SetObjectPositionsBatch(const TMap<FGuid, FVector>& Updates) override;

	virtual TArray<FString> GetCapabilities() const override;
	virtual TArray<FString> Validate() const override;

	// ========================================================================
	// DELEGATES
	// ========================================================================

	/** Called when connection state changes */
	FOnProcessorConnectionStateChanged OnConnectionStateChanged;

	/** Called on errors */
	FOnProcessorError OnError;

	/** Called when OSC message is received */
	FOnOSCMessageReceived OnOSCMessageReceived;

protected:
	// ========================================================================
	// INTERNAL STATE
	// ========================================================================

	bool bInitialized;
	FExternalProcessorConfig Config;
	EProcessorConnectionState ConnectionState;
	mutable FCriticalSection StateLock;

	// Object mappings
	TMap<FGuid, FExternalObjectMapping> ObjectMappings;
	mutable FCriticalSection MappingsLock;

	// Batch state
	bool bInBatch;
	TArray<FSpatialOSCMessage> BatchedMessages;
	FCriticalSection BatchLock;

	// Statistics
	int64 MessagesSent;
	int64 MessagesReceived;
	FDateTime LastCommunicationTime;

	// Last known positions (for change detection)
	TMap<FGuid, FVector> LastPositions;
	mutable FCriticalSection PositionsLock;

	// ========================================================================
	// INTERNAL HELPERS
	// ========================================================================

	/** Set connection state and broadcast delegate */
	void SetConnectionState(EProcessorConnectionState NewState);

	/** Report an error */
	void ReportError(const FString& Error);

	/** Check if position change exceeds threshold */
	bool ShouldSendPositionUpdate(const FGuid& ObjectId, const FVector& NewPosition) const;

	/** Queue a message (or add to batch if batching) */
	virtual bool QueueMessage(const FSpatialOSCMessage& Message);

	/** Actually send queued messages - implemented by derived class */
	virtual bool SendQueuedMessages(const TArray<FSpatialOSCMessage>& Messages) = 0;
};
