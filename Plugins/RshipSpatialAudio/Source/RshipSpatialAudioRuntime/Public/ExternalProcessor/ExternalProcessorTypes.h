// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SpatialAudioTypes.h"
#include "ExternalProcessorTypes.generated.h"

// ============================================================================
// EXTERNAL PROCESSOR TYPES
// ============================================================================

/**
 * Types of external spatial audio processors.
 */
UENUM(BlueprintType)
enum class EExternalProcessorType : uint8
{
	None			UMETA(DisplayName = "None"),
	DS100			UMETA(DisplayName = "d&b DS100"),
	P1				UMETA(DisplayName = "d&b P1 Processor"),
	LISA			UMETA(DisplayName = "L-Acoustics L-ISA"),
	SpacemapGo		UMETA(DisplayName = "Meyer Spacemap Go"),
	Custom			UMETA(DisplayName = "Custom OSC")
};

/**
 * Connection state for external processors.
 */
UENUM(BlueprintType)
enum class EProcessorConnectionState : uint8
{
	Disconnected	UMETA(DisplayName = "Disconnected"),
	Connecting		UMETA(DisplayName = "Connecting"),
	Connected		UMETA(DisplayName = "Connected"),
	Error			UMETA(DisplayName = "Error"),
	Reconnecting	UMETA(DisplayName = "Reconnecting")
};

/**
 * Coordinate system used by the external processor.
 */
UENUM(BlueprintType)
enum class EProcessorCoordinateSystem : uint8
{
	Cartesian		UMETA(DisplayName = "Cartesian XYZ"),
	Spherical		UMETA(DisplayName = "Spherical (Azimuth, Elevation, Distance)"),
	Polar			UMETA(DisplayName = "Polar 2D (Angle, Distance)"),
	Normalized		UMETA(DisplayName = "Normalized 0-1")
};

/**
 * Protocol used to communicate with external processor.
 */
UENUM(BlueprintType)
enum class EProcessorProtocol : uint8
{
	OSC_UDP			UMETA(DisplayName = "OSC over UDP"),
	OSC_TCP			UMETA(DisplayName = "OSC over TCP"),
	OCA				UMETA(DisplayName = "OCA/AES70"),
	Custom			UMETA(DisplayName = "Custom Protocol")
};

// ============================================================================
// OSC MESSAGE TYPES
// ============================================================================

/**
 * OSC argument type.
 */
UENUM(BlueprintType)
enum class ESpatialOSCArgumentType : uint8
{
	Int32,
	Float,
	String,
	Blob,
	BoolTrue	UMETA(DisplayName = "True"),
	BoolFalse	UMETA(DisplayName = "False"),
	Nil,
	Int64,
	Double,
	Char,
	Color,
	Midi,
	Array
};

/**
 * Single OSC argument.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialOSCArgument
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	ESpatialOSCArgumentType Type = ESpatialOSCArgumentType::Float;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	TArray<uint8> BlobValue;

	// Convenience constructors (non-USTRUCT)
	static FSpatialOSCArgument MakeInt(int32 Value);
	static FSpatialOSCArgument MakeFloat(float Value);
	static FSpatialOSCArgument MakeString(const FString& Value);
};

/**
 * Complete OSC message.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialOSCMessage
{
	GENERATED_BODY()

	/** OSC address pattern (e.g., /dbaudio1/coordinatemapping/source_position_xy) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	FString Address;

	/** Message arguments */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	TArray<FSpatialOSCArgument> Arguments;

	/** Timestamp (0 = immediate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	int64 TimeTag = 0;

	// Convenience methods
	void AddInt(int32 Value);
	void AddFloat(float Value);
	void AddString(const FString& Value);

	/** Serialize to OSC binary format */
	TArray<uint8> Serialize() const;

	/** Parse from OSC binary format */
	static bool Parse(const TArray<uint8>& Data, FSpatialOSCMessage& OutMessage);
};

/**
 * OSC bundle (collection of messages with shared timetag).
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialOSCBundle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	int64 TimeTag = 1;  // 1 = immediate

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OSC")
	TArray<FSpatialOSCMessage> Messages;

	/** Serialize to OSC binary format */
	TArray<uint8> Serialize() const;

	/** Parse from OSC binary format */
	static bool Parse(const TArray<uint8>& Data, FSpatialOSCBundle& OutBundle);
};

// ============================================================================
// EXTERNAL OBJECT MAPPING
// ============================================================================

/**
 * Mapping between internal audio object and external processor object.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FExternalObjectMapping
{
	GENERATED_BODY()

	/** Internal audio object ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FGuid InternalObjectId;

	/** External processor object number (1-based typically) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	int32 ExternalObjectNumber = 1;

	/** External processor mapping number (for multi-mapping systems like DS100) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	int32 MappingNumber = 1;

	/** Whether this mapping is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	bool bEnabled = true;

	/** Custom name for this mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FString DisplayName;
};

/**
 * External processor coordinate mapping configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FProcessorCoordinateMapping
{
	GENERATED_BODY()

	/** Coordinate system used by processor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	EProcessorCoordinateSystem CoordinateSystem = EProcessorCoordinateSystem::Cartesian;

	/** Scale factor to convert from Unreal units (cm) to processor units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	float ScaleFactor = 0.01f;  // cm to meters

	/** Origin offset in Unreal coordinates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FVector OriginOffset = FVector::ZeroVector;

	/** Rotation to apply (Unreal to processor coordinate system) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FRotator CoordinateRotation = FRotator::ZeroRotator;

	/** Axis mapping (which Unreal axis maps to which processor axis) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FIntVector AxisMapping = FIntVector(0, 1, 2);  // X->X, Y->Y, Z->Z

	/** Axis inversion flags */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FIntVector AxisInvert = FIntVector(1, 1, 1);  // No inversion

	/** Convert Unreal position to processor position */
	FVector ConvertPosition(const FVector& UnrealPosition) const;

	/** Convert processor position to Unreal position */
	FVector ConvertPositionToUnreal(const FVector& ProcessorPosition) const;
};

// ============================================================================
// PROCESSOR CONFIGURATION
// ============================================================================

/**
 * Network connection settings for external processor.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FProcessorNetworkConfig
{
	GENERATED_BODY()

	/** Processor hostname or IP address */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FString Host = TEXT("127.0.0.1");

	/** Send port (commands to processor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	int32 SendPort = 50010;

	/** Receive port (replies from processor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	int32 ReceivePort = 50011;

	/** Protocol to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	EProcessorProtocol Protocol = EProcessorProtocol::OSC_UDP;

	/** Connection timeout in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	float ConnectionTimeoutSec = 5.0f;

	/** Heartbeat interval in seconds (0 = disabled) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	float HeartbeatIntervalSec = 1.0f;

	/** Auto-reconnect on connection loss */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	bool bAutoReconnect = true;

	/** Reconnect delay in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	float ReconnectDelaySec = 2.0f;
};

/**
 * Rate limiting settings for processor communication.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FProcessorRateLimitConfig
{
	GENERATED_BODY()

	/** Maximum messages per second (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor", meta = (ClampMin = "0"))
	int32 MaxMessagesPerSecond = 100;

	/** Minimum interval between position updates in ms */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor", meta = (ClampMin = "0"))
	float MinPositionUpdateIntervalMs = 10.0f;

	/** Bundle multiple messages into single UDP packet */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	bool bUseBundling = true;

	/** Maximum bundle size in bytes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	int32 MaxBundleSizeBytes = 1472;  // MTU - headers

	/** Position change threshold (processor units) - skip updates below this */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	float PositionChangeThreshold = 0.001f;
};

/**
 * Complete external processor configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FExternalProcessorConfig
{
	GENERATED_BODY()

	/** Processor type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	EExternalProcessorType ProcessorType = EExternalProcessorType::DS100;

	/** Display name for this processor instance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FString DisplayName = TEXT("External Processor");

	/** Network configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FProcessorNetworkConfig Network;

	/** Coordinate mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FProcessorCoordinateMapping CoordinateMapping;

	/** Rate limiting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	FProcessorRateLimitConfig RateLimit;

	/** Object mappings (internal to external) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	TArray<FExternalObjectMapping> ObjectMappings;

	/** Whether processor is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalProcessor")
	bool bEnabled = true;
};

// ============================================================================
// PROCESSOR STATUS
// ============================================================================

/**
 * Runtime status of an external processor.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FExternalProcessorStatus
{
	GENERATED_BODY()

	/** Current connection state */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ExternalProcessor")
	EProcessorConnectionState ConnectionState = EProcessorConnectionState::Disconnected;

	/** Last error message (if any) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ExternalProcessor")
	FString LastError;

	/** Time of last successful communication */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ExternalProcessor")
	FDateTime LastCommunicationTime;

	/** Messages sent this session */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ExternalProcessor")
	int64 MessagesSent = 0;

	/** Messages received this session */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ExternalProcessor")
	int64 MessagesReceived = 0;

	/** Current send rate (messages/sec) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ExternalProcessor")
	float CurrentSendRate = 0.0f;

	/** Average round-trip latency in ms */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ExternalProcessor")
	float AverageLatencyMs = 0.0f;

	/** Number of active object mappings */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ExternalProcessor")
	int32 ActiveMappings = 0;
};

// ============================================================================
// DS100 SPECIFIC TYPES
// ============================================================================

/**
 * DS100 coordinate mapping areas.
 */
UENUM(BlueprintType)
enum class EDS100MappingArea : uint8
{
	None = 0			UMETA(DisplayName = "None"),
	MappingArea1 = 1	UMETA(DisplayName = "Mapping Area 1"),
	MappingArea2 = 2	UMETA(DisplayName = "Mapping Area 2"),
	MappingArea3 = 3	UMETA(DisplayName = "Mapping Area 3"),
	MappingArea4 = 4	UMETA(DisplayName = "Mapping Area 4")
};

/**
 * DS100 matrix input/output types.
 */
UENUM(BlueprintType)
enum class EDS100MatrixIO : uint8
{
	Input		UMETA(DisplayName = "Matrix Input"),
	Output		UMETA(DisplayName = "Matrix Output")
};

/**
 * DS100-specific object parameters.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FDS100ObjectParams
{
	GENERATED_BODY()

	/** Source ID (1-64) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100", meta = (ClampMin = "1", ClampMax = "64"))
	int32 SourceId = 1;

	/** Mapping area (1-4) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100")
	EDS100MappingArea MappingArea = EDS100MappingArea::MappingArea1;

	/** En-Space send level (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EnSpaceSend = 0.0f;

	/** Spread factor (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Spread = 0.5f;

	/** Delay mode (0=Off, 1=Tight, 2=Full) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100", meta = (ClampMin = "0", ClampMax = "2"))
	int32 DelayMode = 1;
};

/**
 * DS100-specific configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FDS100Config
{
	GENERATED_BODY()

	/** Device name/identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100")
	FString DeviceName = TEXT("DS100");

	/** Primary or secondary device */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100")
	bool bIsPrimary = true;

	/** OSC command prefix */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100")
	FString OSCPrefix = TEXT("/dbaudio1");

	/** Use position XY (true) or XYZ (false) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100")
	bool bUseXYOnly = true;

	/** Default mapping area for new objects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100")
	EDS100MappingArea DefaultMappingArea = EDS100MappingArea::MappingArea1;

	/** Global En-Space reverb send for all sources */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlobalEnSpaceSend = 0.0f;

	/** Per-source parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DS100")
	TMap<int32, FDS100ObjectParams> SourceParams;
};

// ============================================================================
// DELEGATE DECLARATIONS
// ============================================================================

/** Delegate for processor connection state changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnProcessorConnectionStateChanged,
	EExternalProcessorType, ProcessorType,
	EProcessorConnectionState, NewState);

/** Delegate for processor errors */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnProcessorError,
	EExternalProcessorType, ProcessorType,
	const FString&, ErrorMessage);

/** Delegate for received OSC messages */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnOSCMessageReceived,
	EExternalProcessorType, ProcessorType,
	const FSpatialOSCMessage&, Message);
