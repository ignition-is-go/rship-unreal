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
 * Color primaries (colorimetry) for 2110-20 HDR support
 */
UENUM(BlueprintType)
enum class ERship2110Colorimetry : uint8
{
    /** BT.709 - Standard HD (sRGB primaries) */
    BT709           UMETA(DisplayName = "BT.709"),

    /** BT.2020 - Wide Color Gamut for UHD/HDR */
    BT2020          UMETA(DisplayName = "BT.2020"),

    /** BT.2100 - HDR with BT.2020 primaries */
    BT2100          UMETA(DisplayName = "BT.2100"),

    /** DCI-P3 - Digital Cinema */
    DCIP3           UMETA(DisplayName = "DCI-P3"),

    /** ST 2065-1 - ACES */
    ST2065_1        UMETA(DisplayName = "ACES (ST 2065-1)")
};

/**
 * Transfer function (EOTF/OETF) for 2110-20 HDR support
 */
UENUM(BlueprintType)
enum class ERship2110TransferFunction : uint8
{
    /** SDR gamma (~BT.1886) */
    SDR             UMETA(DisplayName = "SDR (BT.1886)"),

    /** PQ (Perceptual Quantizer) - ST.2084 for HDR10/Dolby Vision */
    PQ              UMETA(DisplayName = "PQ (ST.2084)"),

    /** HLG (Hybrid Log-Gamma) - ARIB STD-B67 for broadcast HDR */
    HLG             UMETA(DisplayName = "HLG (ARIB STD-B67)"),

    /** Linear (1.0 gamma, scene-referred) */
    Linear          UMETA(DisplayName = "Linear"),

    /** sRGB transfer function */
    sRGB            UMETA(DisplayName = "sRGB")
};

/**
 * HDR metadata for content light levels (ST.2086 / CTA-861.3)
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110HDRMetadata
{
    GENERATED_BODY()

    /** Enable HDR metadata in stream */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR")
    bool bEnabled = false;

    /** Maximum Content Light Level (MaxCLL) in nits */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0", ClampMax = "10000", EditCondition = "bEnabled"))
    int32 MaxContentLightLevel = 1000;

    /** Maximum Frame-Average Light Level (MaxFALL) in nits */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0", ClampMax = "10000", EditCondition = "bEnabled"))
    int32 MaxFrameAverageLightLevel = 400;

    /** Mastering display primaries - Red X (0.0-1.0, normalized to 0.00002) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
    float DisplayPrimariesRedX = 0.708f;  // BT.2020 default

    /** Mastering display primaries - Red Y */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
    float DisplayPrimariesRedY = 0.292f;

    /** Mastering display primaries - Green X */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
    float DisplayPrimariesGreenX = 0.170f;

    /** Mastering display primaries - Green Y */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
    float DisplayPrimariesGreenY = 0.797f;

    /** Mastering display primaries - Blue X */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
    float DisplayPrimariesBlueX = 0.131f;

    /** Mastering display primaries - Blue Y */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
    float DisplayPrimariesBlueY = 0.046f;

    /** White point X (D65 = 0.3127) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
    float WhitePointX = 0.3127f;

    /** White point Y (D65 = 0.3290) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
    float WhitePointY = 0.3290f;

    /** Mastering display maximum luminance in nits */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0", ClampMax = "10000", EditCondition = "bEnabled"))
    int32 MaxDisplayMasteringLuminance = 1000;

    /** Mastering display minimum luminance in nits (stored as 0.0001 nits units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR",
        meta = (ClampMin = "0.0", ClampMax = "10.0", EditCondition = "bEnabled"))
    float MinDisplayMasteringLuminance = 0.005f;

    /** Set to BT.2020 HDR10 defaults */
    void SetHDR10Defaults()
    {
        bEnabled = true;
        MaxContentLightLevel = 1000;
        MaxFrameAverageLightLevel = 400;
        // BT.2020 primaries
        DisplayPrimariesRedX = 0.708f;
        DisplayPrimariesRedY = 0.292f;
        DisplayPrimariesGreenX = 0.170f;
        DisplayPrimariesGreenY = 0.797f;
        DisplayPrimariesBlueX = 0.131f;
        DisplayPrimariesBlueY = 0.046f;
        WhitePointX = 0.3127f;
        WhitePointY = 0.3290f;
        MaxDisplayMasteringLuminance = 1000;
        MinDisplayMasteringLuminance = 0.005f;
    }

    /** Set to HLG broadcast defaults */
    void SetHLGDefaults()
    {
        bEnabled = true;
        MaxContentLightLevel = 1000;
        MaxFrameAverageLightLevel = 400;
        // BT.2020 primaries for HLG
        DisplayPrimariesRedX = 0.708f;
        DisplayPrimariesRedY = 0.292f;
        DisplayPrimariesGreenX = 0.170f;
        DisplayPrimariesGreenY = 0.797f;
        DisplayPrimariesBlueX = 0.131f;
        DisplayPrimariesBlueY = 0.046f;
        WhitePointX = 0.3127f;
        WhitePointY = 0.3290f;
        MaxDisplayMasteringLuminance = 1000;
        MinDisplayMasteringLuminance = 0.005f;
    }
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
 * Cluster role for this node in distributed 2110 control.
 */
UENUM(BlueprintType)
enum class ERship2110ClusterRole : uint8
{
    Unknown         UMETA(DisplayName = "Unknown"),
    Primary         UMETA(DisplayName = "Primary"),
    Secondary       UMETA(DisplayName = "Secondary")
};

/**
 * Per-node stream ownership assignment.
 * Streams listed here are allowed to transmit from NodeId when strict ownership is enabled.
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110ClusterNodeStreams
{
    GENERATED_BODY()

    /** Cluster node identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString NodeId;

    /** Stream IDs owned by this node */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    TArray<FString> StreamIds;
};

/**
 * Authoritative cluster control state for distributed 2110 ownership/failover.
 * This state is intended to be replicated through a cluster-synced control channel.
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110ClusterState
{
    GENERATED_BODY()

    /** Monotonic failover epoch (increment on authority handoff) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Epoch = 0;

    /** Monotonic version within the current epoch */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Version = 0;

    /** Frame index at which this state should take effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int64 ApplyFrame = 0;

    /** Node ID currently acting as cluster authority */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString ActiveAuthorityNodeId;

    /** Enforce stream ownership strictly per node assignment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    bool bStrictNodeOwnership = true;

    /** Enable heartbeat-based automatic failover */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    bool bFailoverEnabled = true;

    /** Heartbeat timeout before failover evaluation (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster",
        meta = (ClampMin = "0.1", ClampMax = "60.0"))
    float FailoverTimeoutSeconds = 2.0f;

    /** Allow automatic local promotion when this node is deterministic failover candidate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    bool bAllowAutoPromotion = true;

    /** Required ACK count for prepare/commit quorum (0 = all discovered nodes in this state) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 RequiredAckCount = 0;

    /** Maximum age for prepared states before discard (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster",
        meta = (ClampMin = "0.1", ClampMax = "60.0"))
    float PrepareTimeoutSeconds = 3.0f;

    /** Deterministic priority list for authority promotion (first item wins) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    TArray<FString> FailoverPriority;

    /** Node-to-stream ownership assignments */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    TArray<FRship2110ClusterNodeStreams> NodeStreamAssignments;

    /** Returns true if this state is newer than Other using (epoch, version) ordering */
    bool IsNewerThan(const FRship2110ClusterState& Other) const
    {
        if (Epoch != Other.Epoch)
        {
            return Epoch > Other.Epoch;
        }
        return Version > Other.Version;
    }
};

/**
 * Prepare message for two-phase cluster state delivery.
 * Authority broadcasts this first; receivers validate and ACK.
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110ClusterPrepareMessage
{
    GENERATED_BODY()

    /** Authority node emitting this prepare */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString AuthorityNodeId;

    /** Proposed state epoch */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Epoch = 0;

    /** Proposed state version */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Version = 0;

    /** Frame at which state should apply */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int64 ApplyFrame = 0;

    /** Deterministic hash of ClusterState payload */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString StateHash;

    /** Quorum threshold carried with this prepare */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 RequiredAckCount = 0;

    /** Full cluster state payload */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FRship2110ClusterState ClusterState;
};

/**
 * ACK message for prepare phase.
 * Nodes send one ACK per (epoch, version, hash).
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110ClusterAckMessage
{
    GENERATED_BODY()

    /** Node that ACKed the prepare */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString NodeId;

    /** Authority node for this transaction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString AuthorityNodeId;

    /** Prepared state epoch */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Epoch = 0;

    /** Prepared state version */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Version = 0;

    /** Prepared state hash */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString StateHash;
};

/**
 * Commit message for two-phase state delivery.
 * Authority emits this after prepare ACK quorum.
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110ClusterCommitMessage
{
    GENERATED_BODY()

    /** Authority node committing this state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString AuthorityNodeId;

    /** Committed state epoch */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Epoch = 0;

    /** Committed state version */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Version = 0;

    /** Committed state apply frame */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int64 ApplyFrame = 0;

    /** Committed state hash */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString StateHash;
};

/**
 * Authoritative control payload for deterministic cross-node state delivery.
 * Intended for live control/event payloads that must apply on a specific frame.
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110ClusterDataMessage
{
    GENERATED_BODY()

    /** Authority node that emitted this payload */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString AuthorityNodeId;

    /** Authority epoch used for stale-message rejection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int32 Epoch = 0;

    /** Monotonic sequence issued by authority for ordering */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int64 Sequence = 0;

    /** Frame index at which this payload should apply */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    int64 ApplyFrame = 0;

    /** Whether ApplyFrame was explicitly provided by inbound metadata. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    bool bApplyFrameWasExplicit = false;

    /** Optional sync domain ID for independent deterministic frame timelines (empty = default domain) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString SyncDomainId;

    /** Optional target node. Empty means broadcast to all relevant nodes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString TargetNodeId;

    /** Opaque control payload (JSON string) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|Cluster")
    FString Payload;
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

    /** Color primaries / colorimetry */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR")
    ERship2110Colorimetry Colorimetry = ERship2110Colorimetry::BT709;

    /** Transfer function (EOTF/OETF) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR")
    ERship2110TransferFunction TransferFunction = ERship2110TransferFunction::SDR;

    /** HDR metadata (ST.2086 / CTA-861.3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110|HDR")
    FRship2110HDRMetadata HDRMetadata;

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

    /** Check if format uses HDR */
    bool IsHDR() const
    {
        return TransferFunction == ERship2110TransferFunction::PQ ||
               TransferFunction == ERship2110TransferFunction::HLG;
    }

    /** Check if format uses wide color gamut */
    bool IsWideColorGamut() const
    {
        return Colorimetry == ERship2110Colorimetry::BT2020 ||
               Colorimetry == ERship2110Colorimetry::BT2100 ||
               Colorimetry == ERship2110Colorimetry::DCIP3;
    }

    /** Get colorimetry string for SDP (e.g., "BT2020") */
    FString GetColorimetryString() const
    {
        switch (Colorimetry)
        {
            case ERship2110Colorimetry::BT709: return TEXT("BT709");
            case ERship2110Colorimetry::BT2020: return TEXT("BT2020");
            case ERship2110Colorimetry::BT2100: return TEXT("BT2100");
            case ERship2110Colorimetry::DCIP3: return TEXT("DCIP3");
            case ERship2110Colorimetry::ST2065_1: return TEXT("ST2065-1");
            default: return TEXT("BT709");
        }
    }

    /** Get transfer characteristic string for SDP (e.g., "SDR", "PQ", "HLG") */
    FString GetTransferCharacteristicString() const
    {
        switch (TransferFunction)
        {
            case ERship2110TransferFunction::SDR: return TEXT("SDR");
            case ERship2110TransferFunction::PQ: return TEXT("PQ");
            case ERship2110TransferFunction::HLG: return TEXT("HLG");
            case ERship2110TransferFunction::Linear: return TEXT("LINEAR");
            case ERship2110TransferFunction::sRGB: return TEXT("sRGB");
            default: return TEXT("SDR");
        }
    }

    /** Configure for HDR10 (BT.2020 + PQ) */
    void SetHDR10()
    {
        Colorimetry = ERship2110Colorimetry::BT2020;
        TransferFunction = ERship2110TransferFunction::PQ;
        BitDepth = ERship2110BitDepth::Bits_10;
        HDRMetadata.SetHDR10Defaults();
    }

    /** Configure for HLG broadcast (BT.2020 + HLG) */
    void SetHLG()
    {
        Colorimetry = ERship2110Colorimetry::BT2020;
        TransferFunction = ERship2110TransferFunction::HLG;
        BitDepth = ERship2110BitDepth::Bits_10;
        HDRMetadata.SetHLGDefaults();
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
// HDR COLOR CONVERSION UTILITIES
// ============================================================================

/**
 * HDR color conversion utilities for SMPTE ST.2084 (PQ) and ARIB STD-B67 (HLG).
 * These functions implement the EOTF (Electro-Optical Transfer Function) and
 * OETF (Opto-Electronic Transfer Function) for HDR standards.
 */
namespace Rship2110ColorUtils
{
    // ST.2084 (PQ) constants
    constexpr float PQ_M1 = 0.1593017578125f;       // 2610/16384
    constexpr float PQ_M2 = 78.84375f;              // 2523/32 * 128
    constexpr float PQ_C1 = 0.8359375f;             // 3424/4096
    constexpr float PQ_C2 = 18.8515625f;            // 2413/128
    constexpr float PQ_C3 = 18.6875f;               // 2392/128
    constexpr float PQ_MAX_LUMINANCE = 10000.0f;    // Peak luminance in nits

    // HLG constants (ARIB STD-B67)
    constexpr float HLG_A = 0.17883277f;
    constexpr float HLG_B = 0.28466892f;  // 1 - 4*a
    constexpr float HLG_C = 0.55991073f;  // 0.5 - a * ln(4*a)

    /**
     * PQ OETF: Linear light (normalized to 10000 nits) -> PQ encoded value [0,1]
     * Input: Linear light value normalized such that 1.0 = 10000 nits
     * Output: PQ encoded value [0,1]
     */
    inline float LinearToPQ(float LinearValue)
    {
        if (LinearValue <= 0.0f)
            return 0.0f;

        const float Ym1 = FMath::Pow(LinearValue, PQ_M1);
        const float Numerator = PQ_C1 + PQ_C2 * Ym1;
        const float Denominator = 1.0f + PQ_C3 * Ym1;
        return FMath::Pow(Numerator / Denominator, PQ_M2);
    }

    /**
     * PQ EOTF: PQ encoded value [0,1] -> Linear light (normalized to 10000 nits)
     * Input: PQ encoded value [0,1]
     * Output: Linear light value normalized such that 1.0 = 10000 nits
     */
    inline float PQToLinear(float PQValue)
    {
        if (PQValue <= 0.0f)
            return 0.0f;

        const float Em2 = FMath::Pow(PQValue, 1.0f / PQ_M2);
        const float Numerator = FMath::Max(Em2 - PQ_C1, 0.0f);
        const float Denominator = PQ_C2 - PQ_C3 * Em2;
        return FMath::Pow(Numerator / Denominator, 1.0f / PQ_M1);
    }

    /**
     * HLG OETF: Linear light [0,1] -> HLG encoded value [0,1]
     * Input: Scene-referred linear light (1.0 = diffuse white)
     * Output: HLG encoded signal [0,1]
     */
    inline float LinearToHLG(float LinearValue)
    {
        if (LinearValue <= 0.0f)
            return 0.0f;

        if (LinearValue <= 1.0f / 12.0f)
        {
            return FMath::Sqrt(3.0f * LinearValue);
        }
        else
        {
            return HLG_A * FMath::Loge(12.0f * LinearValue - HLG_B) + HLG_C;
        }
    }

    /**
     * HLG inverse OETF: HLG encoded value [0,1] -> Linear light [0,1]
     * Input: HLG encoded signal [0,1]
     * Output: Scene-referred linear light (1.0 = diffuse white)
     */
    inline float HLGToLinear(float HLGValue)
    {
        if (HLGValue <= 0.0f)
            return 0.0f;

        if (HLGValue <= 0.5f)
        {
            return (HLGValue * HLGValue) / 3.0f;
        }
        else
        {
            return (FMath::Exp((HLGValue - HLG_C) / HLG_A) + HLG_B) / 12.0f;
        }
    }

    /**
     * Convert linear light in nits to PQ normalized value
     * @param NitsValue Luminance in nits (cd/m²)
     * @return PQ encoded value [0,1]
     */
    inline float NitsToPQ(float NitsValue)
    {
        return LinearToPQ(NitsValue / PQ_MAX_LUMINANCE);
    }

    /**
     * Convert PQ encoded value to luminance in nits
     * @param PQValue PQ encoded value [0,1]
     * @return Luminance in nits (cd/m²)
     */
    inline float PQToNits(float PQValue)
    {
        return PQToLinear(PQValue) * PQ_MAX_LUMINANCE;
    }

    /**
     * BT.709 to BT.2020 color space conversion matrix (row-major)
     * Used for converting SDR content to wide color gamut
     */
    inline FLinearColor BT709ToBT2020(const FLinearColor& BT709Color)
    {
        // BT.709 RGB to BT.2020 RGB matrix
        const float R = 0.6274f * BT709Color.R + 0.3293f * BT709Color.G + 0.0433f * BT709Color.B;
        const float G = 0.0691f * BT709Color.R + 0.9195f * BT709Color.G + 0.0114f * BT709Color.B;
        const float B = 0.0164f * BT709Color.R + 0.0880f * BT709Color.G + 0.8956f * BT709Color.B;
        return FLinearColor(R, G, B, BT709Color.A);
    }

    /**
     * BT.2020 to BT.709 color space conversion matrix (row-major)
     * Used for converting WCG content back to SDR
     */
    inline FLinearColor BT2020ToBT709(const FLinearColor& BT2020Color)
    {
        // BT.2020 RGB to BT.709 RGB matrix
        const float R =  1.6605f * BT2020Color.R - 0.5877f * BT2020Color.G - 0.0728f * BT2020Color.B;
        const float G = -0.1246f * BT2020Color.R + 1.1330f * BT2020Color.G - 0.0084f * BT2020Color.B;
        const float B = -0.0182f * BT2020Color.R - 0.1006f * BT2020Color.G + 1.1187f * BT2020Color.B;
        return FLinearColor(R, G, B, BT2020Color.A);
    }

    /**
     * Convert 10-bit code value to normalized float
     */
    inline float Code10ToFloat(uint16 CodeValue)
    {
        return static_cast<float>(CodeValue) / 1023.0f;
    }

    /**
     * Convert normalized float to 10-bit code value
     */
    inline uint16 FloatToCode10(float NormalizedValue)
    {
        return static_cast<uint16>(FMath::Clamp(NormalizedValue * 1023.0f + 0.5f, 0.0f, 1023.0f));
    }

    /**
     * Convert 12-bit code value to normalized float
     */
    inline float Code12ToFloat(uint16 CodeValue)
    {
        return static_cast<float>(CodeValue) / 4095.0f;
    }

    /**
     * Convert normalized float to 12-bit code value
     */
    inline uint16 FloatToCode12(float NormalizedValue)
    {
        return static_cast<uint16>(FMath::Clamp(NormalizedValue * 4095.0f + 0.5f, 0.0f, 4095.0f));
    }
}

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPTPStateChanged, ERshipPTPState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPTPStatusUpdated, const FRshipPTPStatus&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOn2110StreamStateChanged, const FString&, StreamId, ERship2110StreamState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOn2110ClusterStateApplied, int32, Epoch, int32, Version, int64, ApplyFrame, const FString&, AuthorityNodeId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn2110ClusterPrepareOutbound, const FRship2110ClusterPrepareMessage&, PrepareMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn2110ClusterAckOutbound, const FRship2110ClusterAckMessage&, AckMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn2110ClusterCommitOutbound, const FRship2110ClusterCommitMessage&, CommitMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn2110ClusterDataOutbound, const FRship2110ClusterDataMessage&, DataMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOn2110ClusterDataApplied, const FString&, AuthorityNodeId, int32, Epoch, int64, Sequence, int64, ApplyFrame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIPMXConnectionStateChanged, ERshipIPMXConnectionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRivermaxDeviceChanged, int32, DeviceIndex, const FRshipRivermaxDevice&, Device);
