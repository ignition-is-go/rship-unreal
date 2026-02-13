// Copyright Rocketship. All Rights Reserved.
// Configuration settings for SMPTE 2110 / PTP / IPMX integration

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Rship2110Types.h"
#include "Rship2110Settings.generated.h"

/**
 * Configuration settings for the Rship2110 module.
 *
 * These settings control PTP synchronization, Rivermax streaming,
 * and IPMX/NMOS discovery behavior.
 *
 * Access via Project Settings > Plugins > Rship 2110 Settings
 * or in DefaultGame.ini under [/Script/Rship2110.URship2110Settings]
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Rship 2110 Settings"))
class RSHIP2110_API URship2110Settings : public UObject
{
    GENERATED_BODY()

public:
    URship2110Settings();

    // ============================================================================
    // STATUS (Runtime Feature Availability)
    // ============================================================================

    /** Rivermax SDK availability status */
    UPROPERTY(VisibleAnywhere, Category = "Status",
        meta = (DisplayName = "Rivermax SDK"))
    FString RivermaxStatus;

    /** PTP synchronization availability status */
    UPROPERTY(VisibleAnywhere, Category = "Status",
        meta = (DisplayName = "PTP Support"))
    FString PTPStatus;

    /** IPMX/NMOS availability status */
    UPROPERTY(VisibleAnywhere, Category = "Status",
        meta = (DisplayName = "IPMX/NMOS Support"))
    FString IPMXStatus;

    /** GPUDirect RDMA availability status */
    UPROPERTY(VisibleAnywhere, Category = "Status",
        meta = (DisplayName = "GPUDirect RDMA"))
    FString GPUDirectStatus;

    /** Detected SDK version */
    UPROPERTY(VisibleAnywhere, Category = "Status",
        meta = (DisplayName = "SDK Version"))
    FString SDKVersion;

    /** Network interface status */
    UPROPERTY(VisibleAnywhere, Category = "Status",
        meta = (DisplayName = "Network Interfaces"))
    FString NetworkStatus;

    /** Refresh all status information */
    UFUNCTION(CallInEditor, Category = "Status",
        meta = (DisplayName = "Refresh Status"))
    void RefreshStatus();

    // ============================================================================
    // RIVERMAX LICENSE
    // ============================================================================

    /** Current Rivermax license file path (read-only display) */
    UPROPERTY(VisibleAnywhere, Category = "License",
        meta = (DisplayName = "Current License Path"))
    FString RivermaxLicensePath;

    /** License status message */
    UPROPERTY(VisibleAnywhere, Category = "License",
        meta = (DisplayName = "License Status"))
    FString LicenseStatus;

    /** Import a Rivermax license file (copies to plugin directory) */
    UFUNCTION(CallInEditor, Category = "License",
        meta = (DisplayName = "Import License File..."))
    void ImportLicenseFile();

    /** Refresh license detection */
    UFUNCTION(CallInEditor, Category = "License",
        meta = (DisplayName = "Refresh License Status"))
    void RefreshLicenseStatus();

    // ============================================================================
    // PTP SETTINGS
    // ============================================================================

    /** Enable PTP synchronization on startup */
    UPROPERTY(EditAnywhere, config, Category = "PTP",
        meta = (DisplayName = "Enable PTP"))
    bool bEnablePTP = true;

    /** PTP domain number (SMPTE 2059 uses 127) */
    UPROPERTY(EditAnywhere, config, Category = "PTP",
        meta = (DisplayName = "PTP Domain", ClampMin = "0", ClampMax = "127"))
    int32 PTPDomain = 127;

    /** Network interface for PTP (empty = auto-detect) */
    UPROPERTY(EditAnywhere, config, Category = "PTP",
        meta = (DisplayName = "PTP Interface IP"))
    FString PTPInterfaceIP;

    /** Use hardware timestamping if available */
    UPROPERTY(EditAnywhere, config, Category = "PTP",
        meta = (DisplayName = "Use Hardware Timestamping"))
    bool bUseHardwareTimestamping = true;

    /** PTP sync threshold in nanoseconds (consider locked if offset < threshold) */
    UPROPERTY(EditAnywhere, config, Category = "PTP",
        meta = (DisplayName = "Sync Threshold (ns)", ClampMin = "100", ClampMax = "1000000"))
    int32 PTPSyncThresholdNs = 1000;

    /** Maximum holdover time in seconds before declaring sync lost */
    UPROPERTY(EditAnywhere, config, Category = "PTP",
        meta = (DisplayName = "Max Holdover Time (s)", ClampMin = "1", ClampMax = "300"))
    int32 PTPMaxHoldoverSeconds = 10;

    /** Log PTP timing statistics periodically */
    UPROPERTY(EditAnywhere, config, Category = "PTP",
        meta = (DisplayName = "Log PTP Statistics"))
    bool bLogPTPStatistics = false;

    /** PTP statistics logging interval in seconds */
    UPROPERTY(EditAnywhere, config, Category = "PTP",
        meta = (DisplayName = "Stats Log Interval (s)", ClampMin = "1", ClampMax = "60",
        EditCondition = "bLogPTPStatistics"))
    int32 PTPStatsLogIntervalSeconds = 5;

    // ============================================================================
    // RIVERMAX SETTINGS
    // ============================================================================

    /** Enable Rivermax streaming on startup */
    UPROPERTY(EditAnywhere, config, Category = "Rivermax",
        meta = (DisplayName = "Enable Rivermax"))
    bool bEnableRivermax = true;

    /** Preferred network interface IP for Rivermax (empty = auto-detect) */
    UPROPERTY(EditAnywhere, config, Category = "Rivermax",
        meta = (DisplayName = "Rivermax Interface IP"))
    FString RivermaxInterfaceIP;

    /** Enable GPUDirect RDMA if available */
    UPROPERTY(EditAnywhere, config, Category = "Rivermax",
        meta = (DisplayName = "Enable GPUDirect RDMA"))
    bool bEnableGPUDirect = true;

    /** Maximum number of concurrent streams */
    UPROPERTY(EditAnywhere, config, Category = "Rivermax",
        meta = (DisplayName = "Max Concurrent Streams", ClampMin = "1", ClampMax = "16"))
    int32 MaxConcurrentStreams = 4;

    /** Memory pool size for Rivermax buffers (MB) */
    UPROPERTY(EditAnywhere, config, Category = "Rivermax",
        meta = (DisplayName = "Buffer Pool Size (MB)", ClampMin = "64", ClampMax = "4096"))
    int32 BufferPoolSizeMB = 256;

    /** Number of chunk buffers per stream (for pipelining) */
    UPROPERTY(EditAnywhere, config, Category = "Rivermax",
        meta = (DisplayName = "Chunks Per Stream", ClampMin = "2", ClampMax = "16"))
    int32 ChunksPerStream = 4;

    // ============================================================================
    // DEFAULT VIDEO FORMAT
    // ============================================================================

    /** Default video format for new streams */
    UPROPERTY(EditAnywhere, config, Category = "Video",
        meta = (DisplayName = "Default Video Format"))
    FRship2110VideoFormat DefaultVideoFormat;

    /** Default transport parameters */
    UPROPERTY(EditAnywhere, config, Category = "Video",
        meta = (DisplayName = "Default Transport Parameters"))
    FRship2110TransportParams DefaultTransportParams;

    // ============================================================================
    // IPMX / NMOS SETTINGS
    // ============================================================================

    /** Enable IPMX/NMOS discovery and registration */
    UPROPERTY(EditAnywhere, config, Category = "IPMX",
        meta = (DisplayName = "Enable IPMX"))
    bool bEnableIPMX = true;

    /** NMOS Registry URL (empty = use mDNS discovery) */
    UPROPERTY(EditAnywhere, config, Category = "IPMX",
        meta = (DisplayName = "Registry URL"))
    FString IPMXRegistryUrl;

    /** Node label for NMOS registration */
    UPROPERTY(EditAnywhere, config, Category = "IPMX",
        meta = (DisplayName = "Node Label"))
    FString IPMXNodeLabel = TEXT("Unreal Engine IPMX Node");

    /** Node description */
    UPROPERTY(EditAnywhere, config, Category = "IPMX",
        meta = (DisplayName = "Node Description"))
    FString IPMXNodeDescription = TEXT("Unreal Engine 5 SMPTE 2110 Sender");

    /** Auto-register node on startup */
    UPROPERTY(EditAnywhere, config, Category = "IPMX",
        meta = (DisplayName = "Auto-Register on Startup"))
    bool bIPMXAutoRegister = true;

    /** Heartbeat interval in seconds */
    UPROPERTY(EditAnywhere, config, Category = "IPMX",
        meta = (DisplayName = "Heartbeat Interval (s)", ClampMin = "1", ClampMax = "30"))
    int32 IPMXHeartbeatIntervalSeconds = 5;

    /** HTTP API port for local NMOS node API */
    UPROPERTY(EditAnywhere, config, Category = "IPMX",
        meta = (DisplayName = "Node API Port", ClampMin = "1024", ClampMax = "65535"))
    int32 IPMXNodeAPIPort = 3212;

    /** HTTP API port for IS-05 connection management */
    UPROPERTY(EditAnywhere, config, Category = "IPMX",
        meta = (DisplayName = "Connection API Port", ClampMin = "1024", ClampMax = "65535"))
    int32 IPMXConnectionAPIPort = 3215;

    // ============================================================================
    // TIMING & SYNCHRONIZATION
    // ============================================================================

    /** Frame alignment mode */
    UPROPERTY(EditAnywhere, config, Category = "Timing",
        meta = (DisplayName = "Align Frames to PTP"))
    bool bAlignFramesToPTP = true;

    /** Shared deterministic control/apply frame rate for cluster data/state */
    UPROPERTY(EditAnywhere, config, Category = "Timing",
        meta = (DisplayName = "Cluster Sync Rate (Hz)", ClampMin = "1.0", ClampMax = "240.0"))
    float ClusterSyncRateHz = 60.0f;

    /** Local output/render pacing multiplier relative to sync frames */
    UPROPERTY(EditAnywhere, config, Category = "Timing",
        meta = (DisplayName = "Local Render Substeps", ClampMin = "1", ClampMax = "8",
        ToolTip = "Allows faster local output pacing while deterministic cluster apply still runs on the shared sync timeline."))
    int32 LocalRenderSubsteps = 1;

    /** Upper bound on deterministic sync-frame catch-up steps per engine tick */
    UPROPERTY(EditAnywhere, config, Category = "Timing",
        meta = (DisplayName = "Max Sync Catch-up Steps", ClampMin = "1", ClampMax = "16",
        ToolTip = "Limits per-tick catch-up work when a node stalls, preventing long hitches."))
    int32 MaxSyncCatchupSteps = 4;

    /** Maximum allowed frame latency in milliseconds before dropping */
    UPROPERTY(EditAnywhere, config, Category = "Timing",
        meta = (DisplayName = "Max Frame Latency (ms)", ClampMin = "1", ClampMax = "100"))
    int32 MaxFrameLatencyMs = 16;

    /** Enable pre-roll buffering (frames rendered ahead of transmission) */
    UPROPERTY(EditAnywhere, config, Category = "Timing",
        meta = (DisplayName = "Enable Pre-roll Buffering"))
    bool bEnablePrerollBuffering = true;

    /** Pre-roll buffer depth in frames */
    UPROPERTY(EditAnywhere, config, Category = "Timing",
        meta = (DisplayName = "Pre-roll Frames", ClampMin = "0", ClampMax = "10",
        EditCondition = "bEnablePrerollBuffering"))
    int32 PrerollFrames = 2;

    // ============================================================================
    // DIAGNOSTICS
    // ============================================================================

    /** Log verbosity level (0=errors, 1=warnings, 2=info, 3=verbose) */
    UPROPERTY(EditAnywhere, config, Category = "Diagnostics",
        meta = (DisplayName = "Log Verbosity", ClampMin = "0", ClampMax = "3"))
    int32 LogVerbosity = 1;

    /** Enable on-screen debug overlay */
    UPROPERTY(EditAnywhere, config, Category = "Diagnostics",
        meta = (DisplayName = "Show Debug Overlay"))
    bool bShowDebugOverlay = false;

    /** Log stream statistics periodically */
    UPROPERTY(EditAnywhere, config, Category = "Diagnostics",
        meta = (DisplayName = "Log Stream Statistics"))
    bool bLogStreamStatistics = false;

    /** Stream statistics logging interval in seconds */
    UPROPERTY(EditAnywhere, config, Category = "Diagnostics",
        meta = (DisplayName = "Stats Log Interval (s)", ClampMin = "1", ClampMax = "60",
        EditCondition = "bLogStreamStatistics"))
    int32 StreamStatsLogIntervalSeconds = 5;

    // ============================================================================
    // UTILITY METHODS
    // ============================================================================

    /** Get singleton settings object */
    static URship2110Settings* Get();
};
