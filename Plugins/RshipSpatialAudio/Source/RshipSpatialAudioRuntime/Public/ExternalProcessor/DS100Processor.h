// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IExternalSpatialProcessor.h"
#include "OSCClient.h"

/**
 * d&b DS100 Signal Engine spatial audio processor.
 *
 * Implements control of the d&b DS100 matrix processor via OSC,
 * supporting:
 * - Object position control (XY and XYZ)
 * - Spread/Source Width
 * - Delay mode
 * - En-Space reverb send
 * - Matrix input/output levels
 * - Coordinate mapping areas
 *
 * OSC Address Format:
 *   /dbaudio1/coordinatemapping/source_position_xy <mapping> <source> <x> <y>
 *   /dbaudio1/coordinatemapping/source_position <mapping> <source> <x> <y> <z>
 *   /dbaudio1/coordinatemapping/source_spread <mapping> <source> <spread>
 *   /dbaudio1/matrixinput/reverbsendgain <source> <gain>
 *   /dbaudio1/matrixinput/delaymode <source> <mode>
 *
 * Coordinate System:
 * - DS100 uses meters, Unreal uses centimeters
 * - DS100 coordinate range depends on mapping area configuration
 * - Default: X right, Y forward, Z up (same as Unreal but different scale)
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Callbacks fire on the network thread
 *
 * Usage:
 *   auto Processor = MakeUnique<FDS100Processor>();
 *   FExternalProcessorConfig Config;
 *   Config.Network.Host = TEXT("192.168.1.100");
 *   Config.Network.SendPort = 50010;
 *   Processor->Initialize(Config);
 *   Processor->Connect();
 *
 *   // Set source position
 *   Processor->SetObjectPosition(ObjectGuid, FVector(100, 50, 0));
 */
class RSHIPSPATIALAUDIORUNTIME_API FDS100Processor : public FExternalSpatialProcessorBase
{
public:
	FDS100Processor();
	virtual ~FDS100Processor() override;

	// ========================================================================
	// IExternalSpatialProcessor OVERRIDES
	// ========================================================================

	virtual bool Initialize(const FExternalProcessorConfig& Config) override;
	virtual void Shutdown() override;

	virtual bool Connect() override;
	virtual void Disconnect() override;
	virtual bool IsConnected() const override;

	virtual bool SetObjectPosition(const FGuid& ObjectId, const FVector& Position) override;
	virtual bool SetObjectPositionAndSpread(const FGuid& ObjectId, const FVector& Position, float Spread) override;
	virtual bool SetObjectSpread(const FGuid& ObjectId, float Spread) override;
	virtual bool SetObjectGain(const FGuid& ObjectId, float GainDb) override;
	virtual bool SetObjectReverbSend(const FGuid& ObjectId, float SendLevel) override;
	virtual bool SetObjectMute(const FGuid& ObjectId, bool bMute) override;

	virtual bool SendOSCMessage(const FRshipOSCMessage& Message) override;
	virtual bool SendOSCBundle(const FRshipOSCBundle& Bundle) override;

	virtual EExternalProcessorType GetType() const override { return EExternalProcessorType::DS100; }
	virtual FString GetName() const override { return TEXT("d&b DS100"); }
	virtual int32 GetMaxObjects() const override { return 64; }  // DS100 supports 64 sources

	virtual FString GetDiagnosticInfo() const override;
	virtual TArray<FString> GetCapabilities() const override;

	// ========================================================================
	// DS100-SPECIFIC METHODS
	// ========================================================================

	/**
	 * Set DS100-specific configuration.
	 */
	void SetDS100Config(const FDS100Config& InConfig);

	/**
	 * Get current DS100 configuration.
	 */
	const FDS100Config& GetDS100Config() const { return DS100Config; }

	/**
	 * Set source delay mode.
	 * @param SourceId DS100 source ID (1-64).
	 * @param DelayMode 0=Off, 1=Tight, 2=Full.
	 */
	bool SetSourceDelayMode(int32 SourceId, int32 DelayMode);

	/**
	 * Set source En-Space reverb send.
	 * @param SourceId DS100 source ID (1-64).
	 * @param SendLevel Send level 0-1.
	 */
	bool SetSourceEnSpaceSend(int32 SourceId, float SendLevel);

	/**
	 * Set matrix input gain.
	 * @param InputChannel Matrix input channel (1-64).
	 * @param GainDb Gain in dB.
	 */
	bool SetMatrixInputGain(int32 InputChannel, float GainDb);

	/**
	 * Set matrix input mute.
	 * @param InputChannel Matrix input channel (1-64).
	 * @param bMute Mute state.
	 */
	bool SetMatrixInputMute(int32 InputChannel, bool bMute);

	/**
	 * Set matrix output gain.
	 * @param OutputChannel Matrix output channel.
	 * @param GainDb Gain in dB.
	 */
	bool SetMatrixOutputGain(int32 OutputChannel, float GainDb);

	/**
	 * Request current source position from DS100.
	 * Response will come via OnOSCMessageReceived delegate.
	 */
	bool RequestSourcePosition(int32 SourceId, int32 MappingArea = 1);

	/**
	 * Set global En-Space room size.
	 * @param RoomId Room ID (1-9).
	 */
	bool SetEnSpaceRoom(int32 RoomId);

	/**
	 * Set coordinate mapping area.
	 * @param ObjectId Internal object ID.
	 * @param MappingArea Mapping area (1-4).
	 */
	bool SetObjectMappingArea(const FGuid& ObjectId, EDS100MappingArea MappingArea);

	/**
	 * Get DS100 source ID for internal object.
	 * @param ObjectId Internal object ID.
	 * @return DS100 source ID (1-64) or -1 if not mapped.
	 */
	int32 GetDS100SourceId(const FGuid& ObjectId) const;

	/**
	 * Get mapping area for internal object.
	 */
	EDS100MappingArea GetObjectMappingArea(const FGuid& ObjectId) const;

protected:
	// ========================================================================
	// INTERNAL IMPLEMENTATION
	// ========================================================================

	virtual bool SendQueuedMessages(const TArray<FRshipOSCMessage>& Messages) override;

private:
	// DS100 configuration
	FDS100Config DS100Config;

	// OSC client
	TUniquePtr<FOSCClient> OSCClient;

	// Cached source data
	TMap<FGuid, FDS100ObjectParams> SourceParamsCache;
	mutable FCriticalSection SourceParamsLock;

	// Heartbeat
	FTimerHandle HeartbeatTimerHandle;
	void SendHeartbeat();

	// OSC message handlers
	void HandleReceivedOSCMessage(const FRshipOSCMessage& Message);
	void HandlePositionResponse(const FRshipOSCMessage& Message);

	// OSC address builders
	FString BuildPositionXYAddress(int32 MappingArea, int32 SourceId) const;
	FString BuildPositionXYZAddress(int32 MappingArea, int32 SourceId) const;
	FString BuildSpreadAddress(int32 MappingArea, int32 SourceId) const;
	FString BuildDelayModeAddress(int32 SourceId) const;
	FString BuildReverbSendAddress(int32 SourceId) const;
	FString BuildMatrixInputGainAddress(int32 Channel) const;
	FString BuildMatrixInputMuteAddress(int32 Channel) const;
	FString BuildMatrixOutputGainAddress(int32 Channel) const;

	// Coordinate conversion
	FVector ConvertToDS100Coordinates(const FVector& UnrealPosition) const;
	FVector ConvertFromDS100Coordinates(const FVector& DS100Position) const;

	// Validation
	bool ValidateSourceId(int32 SourceId) const;
	bool ValidateMappingArea(int32 MappingArea) const;
};
