#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RshipSettings.generated.h"

/**
 * Configuration settings for Rocketship WebSocket plugin.
 *
 * These settings control connection behavior, rate limiting, batching,
 * and backpressure handling for high-throughput WebSocket communication.
 *
 * Settings are accessed via Project Settings > Game > Rocketship Settings
 * or modified in DefaultGame.ini under [/Script/RshipExec.RshipSettings]
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Rocketship Settings"))
class URshipSettings : public UObject
{
    GENERATED_BODY()

public:
    // ============================================================================
    // CONNECTION SETTINGS
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Rship Server Address"))
    FString rshipHostAddress = "localhost";

    UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Rship Server Port"))
    int32 rshipServerPort = 5155;

    UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Service Color"))
    FLinearColor ServiceColor = FLinearColor::Gray;

    UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Use High-Performance WebSocket",
        ToolTip = "Use dedicated send thread to bypass UE's 30Hz WebSocket throttle. Recommended for high-throughput scenarios."))
    bool bUseHighPerformanceWebSocket = true;

    UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "TCP No Delay",
        ToolTip = "Disable Nagle's algorithm for lower latency. Recommended for real-time data."))
    bool bTcpNoDelay = true;

    UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Disable WebSocket Compression",
        ToolTip = "Disable permessage-deflate compression for lower latency."))
    bool bDisableCompression = true;

    UPROPERTY(EditAnywhere, config, Category = "Connection", meta = (DisplayName = "Ping Interval (Seconds)",
        ClampMin = "0", ClampMax = "300",
        ToolTip = "WebSocket ping interval for keepalive. 0 = disabled."))
    int32 PingIntervalSeconds = 30;

    // ============================================================================
    // RATE LIMITING SETTINGS
    // These control the token bucket algorithm for smoothing outbound message rate
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Rate Limiting", meta = (DisplayName = "Enable Rate Limiting",
        ToolTip = "Enable client-side rate limiting to prevent overwhelming the server"))
    bool bEnableRateLimiting = true;

    UPROPERTY(EditAnywhere, config, Category = "Rate Limiting", meta = (DisplayName = "Max Messages Per Second",
        ClampMin = "1.0", ClampMax = "1000.0",
        ToolTip = "Maximum number of messages that can be sent per second. Higher values increase throughput but may trigger server rate limits."))
    float MaxMessagesPerSecond = 50.0f;

    UPROPERTY(EditAnywhere, config, Category = "Rate Limiting", meta = (DisplayName = "Max Burst Size",
        ClampMin = "1", ClampMax = "100",
        ToolTip = "Maximum number of messages that can be sent in a burst before rate limiting kicks in. Useful for initial registration."))
    int32 MaxBurstSize = 20;

    UPROPERTY(EditAnywhere, config, Category = "Rate Limiting", meta = (DisplayName = "Max Queue Length",
        ClampMin = "10", ClampMax = "10000",
        ToolTip = "Maximum number of messages that can be queued. When exceeded, low-priority messages will be dropped."))
    int32 MaxQueueLength = 500;

    UPROPERTY(EditAnywhere, config, Category = "Rate Limiting", meta = (DisplayName = "Message Timeout (Seconds)",
        ClampMin = "0.0", ClampMax = "300.0",
        ToolTip = "Messages older than this will be dropped (0 = never timeout). Critical messages are never timed out."))
    float MessageTimeoutSeconds = 30.0f;

    UPROPERTY(EditAnywhere, config, Category = "Rate Limiting", meta = (DisplayName = "Enable Message Coalescing",
        ToolTip = "When enabled, duplicate messages (e.g., rapid emitter pulses from same source) will be coalesced into a single send."))
    bool bEnableCoalescing = true;

    // ============================================================================
    // MESSAGE BATCHING SETTINGS
    // Batching combines multiple logical messages into fewer WebSocket frames
    // This dramatically reduces per-message overhead and improves throughput
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Batching", meta = (DisplayName = "Enable Message Batching",
        ToolTip = "Combine multiple messages into single WebSocket frames to reduce overhead. Highly recommended for high-throughput scenarios."))
    bool bEnableBatching = true;

    UPROPERTY(EditAnywhere, config, Category = "Batching", meta = (DisplayName = "Max Batch Size (Messages)",
        ClampMin = "1", ClampMax = "100",
        ToolTip = "Maximum number of messages to combine into a single batch. Higher values reduce overhead but increase latency."))
    int32 MaxBatchMessages = 10;

    UPROPERTY(EditAnywhere, config, Category = "Batching", meta = (DisplayName = "Max Batch Size (Bytes)",
        ClampMin = "1024", ClampMax = "1048576",
        ToolTip = "Maximum batch size in bytes. Prevents excessively large WebSocket frames. Default 64KB."))
    int32 MaxBatchBytes = 65536;

    UPROPERTY(EditAnywhere, config, Category = "Batching", meta = (DisplayName = "Max Batch Interval (ms)",
        ClampMin = "1", ClampMax = "1000",
        ToolTip = "Maximum time to wait for batch to fill before sending. Lower = less latency, higher = better batching efficiency."))
    int32 MaxBatchIntervalMs = 16;

    UPROPERTY(EditAnywhere, config, Category = "Batching", meta = (DisplayName = "Critical Messages Bypass Batching",
        ToolTip = "When enabled, Critical priority messages are sent immediately without waiting for batch to fill."))
    bool bCriticalBypassBatching = true;

    // ============================================================================
    // BYTES-AWARE RATE LIMITING
    // Additional rate limiting based on bytes per second
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Bandwidth", meta = (DisplayName = "Enable Bytes Rate Limiting",
        ToolTip = "Enable rate limiting based on bytes per second in addition to messages per second."))
    bool bEnableBytesRateLimiting = true;

    UPROPERTY(EditAnywhere, config, Category = "Bandwidth", meta = (DisplayName = "Max Bytes Per Second",
        ClampMin = "1024", ClampMax = "104857600",
        ToolTip = "Maximum bytes per second to send. Default 1MB/s. Increase for local/high-bandwidth servers."))
    int32 MaxBytesPerSecond = 1048576;

    UPROPERTY(EditAnywhere, config, Category = "Bandwidth", meta = (DisplayName = "Max Burst Bytes",
        ClampMin = "1024", ClampMax = "10485760",
        ToolTip = "Maximum bytes that can be sent in a burst. Default 256KB."))
    int32 MaxBurstBytes = 262144;

    // ============================================================================
    // PRIORITY AND DROPPING POLICY
    // Controls how messages are dropped under backpressure
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Priority", meta = (DisplayName = "Enable Downsampling",
        ToolTip = "Instead of dropping all low-priority messages under pressure, keep every Nth sample."))
    bool bEnableDownsampling = true;

    UPROPERTY(EditAnywhere, config, Category = "Priority", meta = (DisplayName = "Low Priority Sample Rate",
        ClampMin = "1", ClampMax = "100",
        ToolTip = "Under heavy load, keep 1 in N low-priority messages. 1 = keep all, 10 = keep every 10th."))
    int32 LowPrioritySampleRate = 5;

    UPROPERTY(EditAnywhere, config, Category = "Priority", meta = (DisplayName = "Normal Priority Sample Rate",
        ClampMin = "1", ClampMax = "100",
        ToolTip = "Under heavy load, keep 1 in N normal-priority messages. 1 = keep all, 5 = keep every 5th."))
    int32 NormalPrioritySampleRate = 2;

    UPROPERTY(EditAnywhere, config, Category = "Priority", meta = (DisplayName = "Queue Pressure Threshold",
        ClampMin = "0.1", ClampMax = "1.0",
        ToolTip = "Queue fullness ratio at which downsampling kicks in. 0.5 = start downsampling at 50% queue capacity."))
    float QueuePressureThreshold = 0.7f;

    // ============================================================================
    // ADAPTIVE RATE CONTROL
    // Dynamically adjusts send rate based on observed backpressure
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Adaptive", meta = (DisplayName = "Enable Adaptive Rate Control",
        ToolTip = "Dynamically adjust send rate based on observed server behavior and backpressure."))
    bool bEnableAdaptiveRate = true;

    UPROPERTY(EditAnywhere, config, Category = "Adaptive", meta = (DisplayName = "Rate Increase Factor",
        ClampMin = "1.01", ClampMax = "2.0",
        ToolTip = "Factor to increase rate when no backpressure detected. 1.1 = increase by 10% per interval."))
    float RateIncreaseFactor = 1.1f;

    UPROPERTY(EditAnywhere, config, Category = "Adaptive", meta = (DisplayName = "Rate Decrease Factor",
        ClampMin = "0.1", ClampMax = "0.99",
        ToolTip = "Factor to decrease rate when backpressure detected. 0.5 = halve the rate."))
    float RateDecreaseFactor = 0.5f;

    UPROPERTY(EditAnywhere, config, Category = "Adaptive", meta = (DisplayName = "Min Rate Fraction",
        ClampMin = "0.01", ClampMax = "0.5",
        ToolTip = "Minimum rate as fraction of MaxMessagesPerSecond. Prevents rate from dropping too low."))
    float MinRateFraction = 0.1f;

    UPROPERTY(EditAnywhere, config, Category = "Adaptive", meta = (DisplayName = "Rate Adjustment Interval (Seconds)",
        ClampMin = "0.1", ClampMax = "10.0",
        ToolTip = "How often to evaluate and adjust the send rate."))
    float RateAdjustmentInterval = 1.0f;

    // ============================================================================
    // CONTENT MAPPING SETTINGS
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Content Mapping", meta = (DisplayName = "Enable Content Mapping"))
    bool bEnableContentMapping = true;

    UPROPERTY(EditAnywhere, config, Category = "Content Mapping", meta = (DisplayName = "Asset Store URL"))
    FString AssetStoreUrl = TEXT("http://localhost:3100");

    UPROPERTY(EditAnywhere, config, Category = "Content Mapping", meta = (DisplayName = "Content Mapping Cache Path",
        ToolTip = "Optional override for content mapping cache file location."))
    FString ContentMappingCachePath;

    UPROPERTY(EditAnywhere, config, Category = "Content Mapping", meta = (DisplayName = "Content Mapping Material Path",
        ToolTip = "Optional override for the content mapping material instance asset path."))
    FString ContentMappingMaterialPath;

    UPROPERTY(EditAnywhere, config, Category = "Content Mapping", meta = (DisplayName = "Spawn Debug Actors (Editor Only)"))
    bool bSpawnContentMappingDebugActors = false;

    // ============================================================================
    // DISPLAY MANAGEMENT SETTINGS
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Display Management", meta = (DisplayName = "Enable Display Management"))
    bool bEnableDisplayManagement = true;

    UPROPERTY(EditAnywhere, config, Category = "Display Management", meta = (DisplayName = "Collect Snapshot On Startup"))
    bool bDisplayManagementCollectOnStartup = true;

    UPROPERTY(EditAnywhere, config, Category = "Display Management", meta = (DisplayName = "Display Profile Path",
        ToolTip = "Optional path to a JSON display profile loaded by the display manager at startup."))
    FString DisplayManagementProfilePath;

    UPROPERTY(EditAnywhere, config, Category = "Display Management", meta = (DisplayName = "Display State Cache Path",
        ToolTip = "Optional path to persist canonical display identity state between runs."))
    FString DisplayManagementStateCachePath;

    UPROPERTY(EditAnywhere, config, Category = "Display Management", meta = (DisplayName = "Guarded Apply Mode",
        ToolTip = "When enabled, apply operations run in guarded mode and avoid destructive topology mutations."))
    bool bDisplayManagementGuardedApply = true;

    UPROPERTY(EditAnywhere, config, Category = "Display Management", meta = (DisplayName = "Display Debug Overlay"))
    bool bDisplayManagementDebugOverlay = false;

    // ============================================================================
    // BACKOFF SETTINGS
    // Controls reconnection and rate-limit recovery behavior
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Backoff", meta = (DisplayName = "Initial Backoff (Seconds)",
        ClampMin = "0.1", ClampMax = "10.0",
        ToolTip = "Initial backoff time when a rate limit or connection error occurs."))
    float InitialBackoffSeconds = 1.0f;

    UPROPERTY(EditAnywhere, config, Category = "Backoff", meta = (DisplayName = "Reconnect Jitter (%)",
        ClampMin = "0.0", ClampMax = "100.0",
        ToolTip = "Randomize reconnect delay by ±N%. Helps avoid reconnection thundering across fleet nodes."))
    float ReconnectJitterPercent = 10.0f;

    UPROPERTY(EditAnywhere, config, Category = "Backoff", meta = (DisplayName = "Max Backoff (Seconds)",
        ClampMin = "1.0", ClampMax = "300.0",
        ToolTip = "Maximum backoff time. Backoff increases exponentially but will not exceed this value."))
    float MaxBackoffSeconds = 60.0f;

    UPROPERTY(EditAnywhere, config, Category = "Backoff", meta = (DisplayName = "Backoff Multiplier",
        ClampMin = "1.1", ClampMax = "5.0",
        ToolTip = "Multiplier applied to backoff time on each consecutive error. 2.0 = double the wait each time."))
    float BackoffMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, config, Category = "Backoff", meta = (DisplayName = "Max Retry Count",
        ClampMin = "0", ClampMax = "100",
        ToolTip = "Maximum number of retries before dropping a message (0 = unlimited retries)."))
    int32 MaxRetryCount = 5;

    UPROPERTY(EditAnywhere, config, Category = "Backoff", meta = (DisplayName = "Auto Reconnect",
        ToolTip = "Automatically attempt to reconnect when connection is lost."))
    bool bAutoReconnect = true;

    UPROPERTY(EditAnywhere, config, Category = "Backoff", meta = (DisplayName = "Max Reconnect Attempts",
        ClampMin = "0", ClampMax = "100",
        ToolTip = "Maximum number of reconnection attempts (0 = unlimited)."))
    int32 MaxReconnectAttempts = 10;

    UPROPERTY(EditAnywhere, config, Category = "Backoff", meta = (DisplayName = "Critical Bypass Backoff",
        ToolTip = "Allow Critical messages to send even during backoff period. Use with caution."))
    bool bCriticalBypassBackoff = false;

    // ============================================================================
    // DIAGNOSTICS SETTINGS
    // Controls logging, metrics collection, and debug output
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Diagnostics", meta = (DisplayName = "Log Verbosity",
        ClampMin = "0", ClampMax = "3",
        ToolTip = "Logging verbosity: 0=Errors only, 1=Warnings, 2=Info, 3=Verbose (all messages)"))
    int32 LogVerbosity = 1;

    UPROPERTY(EditAnywhere, config, Category = "Diagnostics", meta = (DisplayName = "Enable Metrics",
        ToolTip = "Enable collection of detailed metrics (messages/second, bytes/second, queue stats, etc.)"))
    bool bEnableMetrics = true;

    UPROPERTY(EditAnywhere, config, Category = "Diagnostics", meta = (DisplayName = "Metrics Log Interval (Seconds)",
        ClampMin = "0.0", ClampMax = "60.0",
        ToolTip = "How often to log metrics summary (0 = disable periodic logging). Metrics are still available via Blueprint."))
    float MetricsLogInterval = 5.0f;

    UPROPERTY(EditAnywhere, config, Category = "Diagnostics", meta = (DisplayName = "Log Rate Limit Events",
        ToolTip = "Log when rate limiting or dropping messages due to backpressure."))
    bool bLogRateLimitEvents = true;

    UPROPERTY(EditAnywhere, config, Category = "Diagnostics", meta = (DisplayName = "Log Batch Details",
        ToolTip = "Log details about batch formation (size, message count, efficiency)."))
    bool bLogBatchDetails = false;

    // ============================================================================
    // PROCESSING SETTINGS
    // Controls timing and threading behavior
    // ============================================================================

    UPROPERTY(EditAnywhere, config, Category = "Processing", meta = (DisplayName = "Control Sync Rate (Hz)",
        ClampMin = "1.0", ClampMax = "240.0",
        ToolTip = "Deterministic control/apply tick rate shared across nodes. Keep this identical across the cluster."))
    float ControlSyncRateHz = 60.0f;

    UPROPERTY(EditAnywhere, config, Category = "Processing", meta = (DisplayName = "Inbound Apply Lead Frames",
        ClampMin = "1", ClampMax = "16",
        ToolTip = "Minimum sync-frame lead time before applying inbound payloads. Higher values improve jitter tolerance at the cost of control latency."))
    int32 InboundApplyLeadFrames = 1;

    UPROPERTY(EditAnywhere, config, Category = "Processing", meta = (DisplayName = "Inbound Require Exact Frame",
        ToolTip = "When enabled, inbound payloads with explicit frame metadata are dropped if they arrive after their target frame. When disabled (legacy), payloads are clamped forward to the next frame when requested frame is behind."))
    bool bInboundRequireExactFrame = false;

    UPROPERTY(EditAnywhere, config, Category = "Processing", meta = (DisplayName = "Queue Process Interval (Seconds)",
        ClampMin = "0.001", ClampMax = "1.0",
        ToolTip = "How often to process the message queue. Lower values = more responsive but higher CPU. Default 0.016 (~60Hz)."))
    float QueueProcessInterval = 0.016f;
};
