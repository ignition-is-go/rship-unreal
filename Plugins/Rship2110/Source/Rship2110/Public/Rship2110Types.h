// Copyright Rocketship. All Rights Reserved.
// Core type definitions for SMPTE 2110 / PTP / IPMX integration

#pragma once

#include "CoreMinimal.h"
#include "Rship2110Types.generated.h"

// ============================================================================
// PTP (IEEE 1588 / SMPTE 2059) TYPES
// ============================================================================

/**
 * PTP clock quality as defined in IEEE 1588
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipPTPClockQuality
{
    GENERATED_BODY()

    /** Clock class (255 = slave-only, 248 = default) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    uint8 ClockClass = 255;

    /** Clock accuracy enumeration (IEEE 1588 Table 6) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    uint8 ClockAccuracy = 0xFE;  // Unknown

    /** Variance of clock (IEEE 1588 format, stored as int32 for Blueprint compatibility) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    int32 OffsetScaledLogVariance = 0xFFFF;
};

/**
 * PTP grandmaster identity
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipPTPGrandmaster
{
    GENERATED_BODY()

    /** 8-byte grandmaster clock identity (displayed as hex string) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    FString ClockIdentity;

    /** Domain number (SMPTE 2059 uses domain 127) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    uint8 Domain = 127;

    /** Priority 1 value */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    uint8 Priority1 = 128;

    /** Priority 2 value */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    uint8 Priority2 = 128;

    /** Clock quality */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    FRshipPTPClockQuality Quality;

    /** Steps removed from GM (stored as int32 for Blueprint compatibility) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    int32 StepsRemoved = 0;
};

/**
 * PTP synchronization state
 */
UENUM(BlueprintType)
enum class ERshipPTPState : uint8
{
    /** PTP service not initialized */
    Disabled        UMETA(DisplayName = "Disabled"),

    /** Searching for grandmaster */
    Listening       UMETA(DisplayName = "Listening"),

    /** Grandmaster found, acquiring lock */
    Acquiring       UMETA(DisplayName = "Acquiring Lock"),

    /** Synchronized to grandmaster */
    Locked          UMETA(DisplayName = "Locked"),

    /** Lost synchronization */
    Holdover        UMETA(DisplayName = "Holdover"),

    /** Error state */
    Error           UMETA(DisplayName = "Error")
};

/**
 * High-precision PTP timestamp (TAI epoch)
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipPTPTimestamp
{
    GENERATED_BODY()

    /** Seconds since TAI epoch (1970-01-01 00:00:00 TAI) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    int64 Seconds = 0;

    /** Nanoseconds within the second [0, 999999999] */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    int32 Nanoseconds = 0;

    /** Convert to total nanoseconds */
    uint64 ToNanoseconds() const
    {
        return static_cast<uint64>(Seconds) * 1000000000ULL + static_cast<uint64>(Nanoseconds);
    }

    /** Create from total nanoseconds */
    static FRshipPTPTimestamp FromNanoseconds(uint64 TotalNs)
    {
        FRshipPTPTimestamp TS;
        TS.Seconds = TotalNs / 1000000000ULL;
        TS.Nanoseconds = TotalNs % 1000000000ULL;
        return TS;
    }

    /** Get as floating-point seconds */
    double ToSeconds() const
    {
        return static_cast<double>(Seconds) + static_cast<double>(Nanoseconds) * 1e-9;
    }
};

/**
 * PTP service status
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipPTPStatus
{
    GENERATED_BODY()

    /** Current PTP state */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    ERshipPTPState State = ERshipPTPState::Disabled;

    /** Current grandmaster information */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    FRshipPTPGrandmaster Grandmaster;

    /** Current PTP time */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    FRshipPTPTimestamp CurrentTime;

    /** Offset from system clock in nanoseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    int64 OffsetFromSystemNs = 0;

    /** Path delay to grandmaster in nanoseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    int64 PathDelayNs = 0;

    /** Current drift rate in parts per billion */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    double DriftPPB = 0.0;

    /** Jitter (standard deviation of offset) in nanoseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|PTP")
    double JitterNs = 0.0;

    /** Is locked to grandmaster */
    bool IsLocked() const { return State == ERshipPTPState::Locked; }
};

// ============================================================================
// SMPTE 2110 TYPES
// ============================================================================

/**
 * Video color format for 2110-20
 */
UENUM(BlueprintType)
enum class ERship2110ColorFormat : uint8
{
    YCbCr_422       UMETA(DisplayName = "YCbCr 4:2:2"),
    YCbCr_444       UMETA(DisplayName = "YCbCr 4:4:4"),
    RGB_444         UMETA(DisplayName = "RGB 4:4:4"),
    RGBA_4444       UMETA(DisplayName = "RGBA 4:4:4:4")
};

/**
 * Bit depth for video samples
 */
UENUM(BlueprintType)
enum class ERship2110BitDepth : uint8
{
    Bits_8          UMETA(DisplayName = "8-bit"),
    Bits_10         UMETA(DisplayName = "10-bit"),
    Bits_12         UMETA(DisplayName = "12-bit"),
    Bits_16         UMETA(DisplayName = "16-bit")
};

/**
 * 2110 stream type
 */
UENUM(BlueprintType)
enum class ERship2110StreamType : uint8
{
    Video_2110_20   UMETA(DisplayName = "ST 2110-20 (Uncompressed Video)"),
    Video_2110_22   UMETA(DisplayName = "ST 2110-22 (Compressed Video)"),
    Audio_2110_30   UMETA(DisplayName = "ST 2110-30 (PCM Audio)"),
    Audio_2110_31   UMETA(DisplayName = "ST 2110-31 (AES3 Audio)"),
    Ancillary_2110_40 UMETA(DisplayName = "ST 2110-40 (Ancillary Data)")
};

/**
 * Sender (transmit) stream state
 */
UENUM(BlueprintType)
enum class ERship2110StreamState : uint8
{
    Stopped         UMETA(DisplayName = "Stopped"),
    Starting        UMETA(DisplayName = "Starting"),
    Running         UMETA(DisplayName = "Running"),
    Paused          UMETA(DisplayName = "Paused"),
    Error           UMETA(DisplayName = "Error")
};

/**
 * Video format specification for 2110-20 streams
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110VideoFormat
{
    GENERATED_BODY()

    /** Horizontal resolution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "720", ClampMax = "8192"))
    int32 Width = 1920;

    /** Vertical resolution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "480", ClampMax = "4320"))
    int32 Height = 1080;

    /** Frame rate numerator */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "1", ClampMax = "240"))
    int32 FrameRateNumerator = 60;

    /** Frame rate denominator (1 for integer rates, 1001 for NTSC drop-frame) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "1", ClampMax = "1001"))
    int32 FrameRateDenominator = 1;

    /** Color format */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    ERship2110ColorFormat ColorFormat = ERship2110ColorFormat::YCbCr_422;

    /** Bit depth per sample */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    ERship2110BitDepth BitDepth = ERship2110BitDepth::Bits_10;

    /** Interlaced (false = progressive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    bool bInterlaced = false;

    /** Get frame rate as decimal */
    double GetFrameRateDecimal() const
    {
        return static_cast<double>(FrameRateNumerator) / static_cast<double>(FrameRateDenominator);
    }

    /** Get frame duration in nanoseconds */
    uint64 GetFrameDurationNs() const
    {
        return static_cast<uint64>(1000000000.0 * FrameRateDenominator / FrameRateNumerator);
    }

    /** Get bytes per line for given format */
    int32 GetBytesPerLine() const;

    /** Get total frame size in bytes */
    int64 GetFrameSizeBytes() const;

    /** Generate SDP media type string */
    FString GetSDPMediaType() const;

    /** Get sampling string for SDP (e.g., "YCbCr-4:2:2") */
    FString GetSampling() const;

    /** Get bit depth as integer */
    int32 GetBitDepthInt() const
    {
        switch (BitDepth)
        {
            case ERship2110BitDepth::Bits_8: return 8;
            case ERship2110BitDepth::Bits_10: return 10;
            case ERship2110BitDepth::Bits_12: return 12;
            case ERship2110BitDepth::Bits_16: return 16;
            default: return 10;
        }
    }
};

/**
 * RTP transport parameters for 2110 streams
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110TransportParams
{
    GENERATED_BODY()

    /** Source IP address (local NIC) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    FString SourceIP;

    /** Destination multicast IP address */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    FString DestinationIP = TEXT("239.0.0.1");

    /** Destination UDP port */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "1024", ClampMax = "65535"))
    int32 DestinationPort = 5004;

    /** Source UDP port */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "1024", ClampMax = "65535"))
    int32 SourcePort = 5004;

    /** RTP payload type [96-127 for dynamic] */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "96", ClampMax = "127"))
    int32 PayloadType = 96;

    /** SSRC (Synchronization Source Identifier, stored as int64 for Blueprint compatibility) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    int64 SSRC = 0;

    /** DSCP value for QoS (default 46 = EF/Expedited Forwarding) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "0", ClampMax = "63"))
    int32 DSCP = 46;

    /** TTL for multicast */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110",
        meta = (ClampMin = "1", ClampMax = "255"))
    int32 TTL = 64;
};

/**
 * Statistics for a 2110 stream
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110StreamStats
{
    GENERATED_BODY()

    /** Total frames sent */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    int64 FramesSent = 0;

    /** Total packets sent */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    int64 PacketsSent = 0;

    /** Total bytes sent */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    int64 BytesSent = 0;

    /** Frames dropped (missed deadline) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    int64 FramesDropped = 0;

    /** Late frames (sent after deadline) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    int64 LateFrames = 0;

    /** Current bitrate in Mbps */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    double CurrentBitrateMbps = 0.0;

    /** Average inter-packet gap in microseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    double AverageIPGMicroseconds = 0.0;

    /** Maximum jitter observed in microseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    double MaxJitterMicroseconds = 0.0;

    /** Last RTP timestamp sent */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    int64 LastRTPTimestamp = 0;

    /** Last sequence number sent */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|2110")
    int32 LastSequenceNumber = 0;
};

// ============================================================================
// IPMX / NMOS TYPES
// ============================================================================

/**
 * NMOS resource type
 */
UENUM(BlueprintType)
enum class ERshipNMOSResourceType : uint8
{
    Node            UMETA(DisplayName = "Node"),
    Device          UMETA(DisplayName = "Device"),
    Source          UMETA(DisplayName = "Source"),
    Flow            UMETA(DisplayName = "Flow"),
    Sender          UMETA(DisplayName = "Sender"),
    Receiver        UMETA(DisplayName = "Receiver")
};

/**
 * NMOS/IPMX connection state
 */
UENUM(BlueprintType)
enum class ERshipIPMXConnectionState : uint8
{
    Disconnected    UMETA(DisplayName = "Disconnected"),
    Connecting      UMETA(DisplayName = "Connecting"),
    Registered      UMETA(DisplayName = "Registered"),
    Active          UMETA(DisplayName = "Active"),
    Error           UMETA(DisplayName = "Error")
};

/**
 * NMOS Node representation (IS-04)
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipNMOSNode
{
    GENERATED_BODY()

    /** Unique node ID (UUID) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Id;

    /** API version (e.g., "v1.3") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Version = TEXT("v1.3");

    /** Human-readable label */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Label;

    /** Longer description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Description;

    /** Key-value tags for filtering */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    TMap<FString, FString> Tags;

    /** Hostname */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Hostname;

    /** HTTP API endpoints */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    TArray<FString> APIEndpoints;

    /** Clock references (PTP clock IDs) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    TArray<FString> Clocks;
};

/**
 * NMOS Sender representation (IS-04)
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipNMOSSender
{
    GENERATED_BODY()

    /** Unique sender ID (UUID) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Id;

    /** Human-readable label */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Label;

    /** Description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Description;

    /** Flow ID this sender transmits */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString FlowId;

    /** Transport type (e.g., "urn:x-nmos:transport:rtp.mcast") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString Transport = TEXT("urn:x-nmos:transport:rtp.mcast");

    /** Device ID this sender belongs to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString DeviceId;

    /** Manifest URL (SDP) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    FString ManifestHref;

    /** Interface bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|IPMX")
    TArray<FString> InterfaceBindings;

    /** Subscription (IS-05 connection state) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IPMX")
    bool bActive = false;
};

/**
 * IPMX service status
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipIPMXStatus
{
    GENERATED_BODY()

    /** Connection state to registry */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IPMX")
    ERshipIPMXConnectionState State = ERshipIPMXConnectionState::Disconnected;

    /** Registry URL */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IPMX")
    FString RegistryUrl;

    /** Our node ID */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IPMX")
    FString NodeId;

    /** Number of registered senders */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IPMX")
    int32 RegisteredSenders = 0;

    /** Number of registered receivers */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IPMX")
    int32 RegisteredReceivers = 0;

    /** Last heartbeat time */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IPMX")
    double LastHeartbeatTime = 0.0;

    /** Last error message */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IPMX")
    FString LastError;
};

// ============================================================================
// RIVERMAX TYPES
// ============================================================================

/**
 * Rivermax device/NIC information
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipRivermaxDevice
{
    GENERATED_BODY()

    /** Device index */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    int32 DeviceIndex = -1;

    /** Device name/description */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    FString Name;

    /** IP address */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    FString IPAddress;

    /** MAC address */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    FString MACAddress;

    /** Supports GPUDirect RDMA */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    bool bSupportsGPUDirect = false;

    /** PTP hardware timestamping capable */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    bool bSupportsPTPHardware = false;

    /** Maximum send bandwidth in Gbps */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    float MaxBandwidthGbps = 0.0f;

    /** Is currently selected/active */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    bool bIsActive = false;
};

/**
 * Rivermax initialization status
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRshipRivermaxStatus
{
    GENERATED_BODY()

    /** Is Rivermax initialized */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    bool bIsInitialized = false;

    /** SDK version string */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    FString SDKVersion;

    /** Available devices */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    TArray<FRshipRivermaxDevice> Devices;

    /** Currently selected device index */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    int32 ActiveDeviceIndex = -1;

    /** Number of active streams */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    int32 ActiveStreamCount = 0;

    /** Last error message */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Rivermax")
    FString LastError;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPTPStateChanged, ERshipPTPState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPTPStatusUpdated, const FRshipPTPStatus&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOn2110StreamStateChanged, const FString&, StreamId, ERship2110StreamState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIPMXConnectionStateChanged, ERshipIPMXConnectionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRivermaxDeviceChanged, int32, DeviceIndex, const FRshipRivermaxDevice&, Device);
