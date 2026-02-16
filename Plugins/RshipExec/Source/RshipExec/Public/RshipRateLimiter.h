/**
 * Adaptive Outbound Pipeline for Rocketship WebSocket Communication
 *
 * This module implements a sophisticated rate limiting and message batching system
 * designed to maximize throughput through bandwidth-constrained WebSocket connections.
 *
 * Key Features:
 * - Token bucket rate limiting (messages/second and bytes/second)
 * - Message batching to reduce per-message overhead
 * - Priority queue with 4 levels (Critical > High > Normal > Low)
 * - Message coalescing for deduplication
 * - Adaptive rate control based on observed backpressure
 * - Downsampling under heavy load
 * - Exponential backoff on rate limit errors
 * - Comprehensive instrumentation and metrics
 *
 * Architecture:
 *
 *   [High-Speed Input]
 *          |
 *          v
 *   +----------------+
 *   | EnqueueMessage | <-- Thread-safe ingress
 *   +----------------+
 *          |
 *          v
 *   +----------------+
 *   | Priority Queue | <-- Sorted by priority, coalesced by key
 *   +----------------+
 *          |
 *          v
 *   +----------------+
 *   | Downsampling   | <-- Under pressure: keep every Nth low-priority
 *   +----------------+
 *          |
 *          v
 *   +----------------+
 *   | Rate Limiter   | <-- Token bucket (messages + bytes)
 *   +----------------+
 *          |
 *          v
 *   +----------------+
 *   | Batch Builder  | <-- Combine messages into frames
 *   +----------------+
 *          |
 *          v
 *   [WebSocket Send]
 */

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"

// ============================================================================
// MESSAGE PRIORITY LEVELS
// Higher priority messages are sent first and protected from dropping
// ============================================================================

enum class ERshipMessagePriority : uint8
{
    // Critical messages that should never be dropped (command responses)
    // These may bypass batching and backoff if configured
    Critical = 0,

    // High priority (registration, status updates)
    // Protected from downsampling but can be dropped if queue overflows
    High = 1,

    // Normal priority (most messages)
    // Subject to downsampling under pressure
    Normal = 2,

    // Low priority (telemetry, frequent emitter pulses)
    // First to be downsampled or dropped
    Low = 3
};

// ============================================================================
// MESSAGE TYPES
// Used for coalescing logic and metrics breakdown
// ============================================================================

enum class ERshipMessageType : uint8
{
    // Generic message - no special handling
    Generic,

    // Command response - critical, never drop
    CommandResponse,

    // Registration messages - can coalesce duplicates
    Registration,

    // Emitter pulse - can drop older duplicates, subject to downsampling
    EmitterPulse,

    // Machine/Instance info - can coalesce
    InstanceInfo
};

// ============================================================================
// QUEUED MESSAGE STRUCTURE
// ============================================================================

struct FRshipQueuedMessage
{
    TSharedPtr<FJsonObject> Payload;
    ERshipMessagePriority Priority;
    ERshipMessageType Type;
    FString CoalesceKey;  // For deduplication (e.g., emitter ID)
    double QueuedTime;    // When the message was queued
    int32 RetryCount;     // Number of times this message has been retried
    int32 EstimatedBytes; // Estimated serialized size (cached for efficiency)

    FRshipQueuedMessage()
        : Priority(ERshipMessagePriority::Normal)
        , Type(ERshipMessageType::Generic)
        , QueuedTime(0.0)
        , RetryCount(0)
        , EstimatedBytes(0)
    {}

    FRshipQueuedMessage(TSharedPtr<FJsonObject> InPayload, ERshipMessagePriority InPriority,
                        ERshipMessageType InType, const FString& InCoalesceKey = TEXT(""))
        : Payload(InPayload)
        , Priority(InPriority)
        , Type(InType)
        , CoalesceKey(InCoalesceKey)
        , QueuedTime(FPlatformTime::Seconds())
        , RetryCount(0)
        , EstimatedBytes(0)
    {}

    // Priority comparison for queue ordering
    bool operator<(const FRshipQueuedMessage& Other) const
    {
        // Lower priority value = higher priority (Critical=0 is highest)
        return static_cast<uint8>(Priority) < static_cast<uint8>(Other.Priority);
    }
};

// ============================================================================
// RATE LIMITER CONFIGURATION
// ============================================================================

struct FRshipRateLimiterConfig
{
    // --- Token Bucket (Messages) ---
    float MaxMessagesPerSecond = 50.0f;
    int32 MaxBurstSize = 20;

    // --- Token Bucket (Bytes) ---
    bool bEnableBytesRateLimiting = true;
    int32 MaxBytesPerSecond = 1048576;  // 1 MB/s
    int32 MaxBurstBytes = 262144;        // 256 KB

    // --- Queue ---
    int32 MaxQueueLength = 500;
    float MessageTimeoutSeconds = 30.0f;
    bool bEnableCoalescing = true;

    // --- Batching ---
    bool bEnableBatching = true;
    int32 MaxBatchMessages = 10;
    int32 MaxBatchBytes = 65536;         // 64 KB
    int32 MaxBatchIntervalMs = 16;
    bool bCriticalBypassBatching = true;

    // --- Downsampling ---
    bool bEnableDownsampling = true;
    int32 LowPrioritySampleRate = 5;     // Keep 1 in 5
    int32 NormalPrioritySampleRate = 2;  // Keep 1 in 2
    float QueuePressureThreshold = 0.7f;

    // --- Adaptive Rate Control ---
    bool bEnableAdaptiveRate = true;
    float RateIncreaseFactor = 1.1f;
    float RateDecreaseFactor = 0.5f;
    float MinRateFraction = 0.1f;
    float RateAdjustmentInterval = 1.0f;

    // --- Backoff ---
    float InitialBackoffSeconds = 1.0f;
    float MaxBackoffSeconds = 60.0f;
    float BackoffMultiplier = 2.0f;
    float BackoffJitterPercent = 10.0f;
    int32 MaxRetryCount = 5;
    bool bCriticalBypassBackoff = false;

    // --- Diagnostics ---
    int32 LogVerbosity = 1;
    bool bEnableMetrics = true;
    float MetricsLogInterval = 5.0f;
    bool bLogRateLimitEvents = true;
    bool bLogBatchDetails = false;
};

// ============================================================================
// METRICS STRUCTURE
// Comprehensive statistics for monitoring and debugging
// ============================================================================

struct FRshipRateLimiterMetrics
{
    // --- Throughput ---
    int32 MessagesSentLastSecond = 0;
    int32 BytesSentLastSecond = 0;
    int32 BatchesSentLastSecond = 0;

    // --- Queue ---
    int32 CurrentQueueLength = 0;
    int32 CurrentQueueBytes = 0;
    float QueuePressure = 0.0f;  // 0.0 - 1.0

    // --- Drops ---
    int32 MessagesDroppedTotal = 0;
    int32 MessagesDroppedLastSecond = 0;
    int32 MessagesDownsampledTotal = 0;
    int32 MessagesCoalescedTotal = 0;

    // --- Drops by priority ---
    int32 DroppedCritical = 0;
    int32 DroppedHigh = 0;
    int32 DroppedNormal = 0;
    int32 DroppedLow = 0;

    // --- Rate Control ---
    float CurrentRateLimit = 0.0f;  // Current effective msg/s limit
    float AvailableTokens = 0.0f;
    int32 AvailableBytesTokens = 0;

    // --- Backoff ---
    bool bIsBackingOff = false;
    float BackoffRemaining = 0.0f;
    int32 BackoffCount = 0;

    // --- Batching ---
    float AverageBatchSize = 0.0f;
    float AverageBatchBytes = 0.0f;
    float BatchingEfficiency = 0.0f;  // Messages batched / Total messages

    // Helpers
    void Reset()
    {
        *this = FRshipRateLimiterMetrics();
    }
};

// ============================================================================
// DELEGATES
// ============================================================================

// Called when a batch is ready to send (single JSON string, may contain array)
DECLARE_DELEGATE_OneParam(FOnMessageReadyToSend, const FString& /* JsonString */);

// Called when rate limiter status changes (backoff, rate adjustment)
DECLARE_DELEGATE_TwoParams(FOnRateLimiterStatus, bool /* bIsBackingOff */, float /* CurrentBackoffSeconds */);

// Called when metrics are updated (for periodic logging or Blueprint access)
DECLARE_DELEGATE_OneParam(FOnMetricsUpdated, const FRshipRateLimiterMetrics& /* Metrics */);

// ============================================================================
// RATE LIMITER CLASS
// ============================================================================

/**
 * Adaptive outbound pipeline with rate limiting, batching, and backpressure handling.
 *
 * Thread Safety:
 * - EnqueueMessage() is fully thread-safe
 * - ProcessQueue() should be called from game thread only
 * - Metrics getters are thread-safe
 */
class RSHIPEXEC_API FRshipRateLimiter
{
public:
    FRshipRateLimiter();
    ~FRshipRateLimiter();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    // Initialize with configuration
    void Initialize(const FRshipRateLimiterConfig& InConfig);

    // Update configuration at runtime (thread-safe)
    void UpdateConfig(const FRshipRateLimiterConfig& InConfig);

    // ========================================================================
    // MESSAGE OPERATIONS
    // ========================================================================

    /**
     * Enqueue a message for sending (thread-safe).
     *
     * @param Payload       JSON payload to send
     * @param Priority      Message priority (Critical/High/Normal/Low)
     * @param Type          Message type for coalescing
     * @param CoalesceKey   Key for deduplication (empty = no coalescing)
     * @return true if message was accepted, false if dropped
     */
    bool EnqueueMessage(TSharedPtr<FJsonObject> Payload,
                        ERshipMessagePriority Priority = ERshipMessagePriority::Normal,
                        ERshipMessageType Type = ERshipMessageType::Generic,
                        const FString& CoalesceKey = TEXT(""));

    /**
     * Process the queue and send messages (call from game thread timer).
     * Handles batching, rate limiting, and adaptive rate control.
     *
     * @return Number of messages sent (individual messages, not batches)
     */
    int32 ProcessQueue();

    // Clear the queue (e.g., on disconnect)
    void ClearQueue();

    // ========================================================================
    // RATE LIMIT AND BACKOFF SIGNALS
    // ========================================================================

    // Signal that we received a rate limit error from server
    // RetryAfterSeconds: from Retry-After header, or -1 for exponential backoff
    void OnRateLimitError(float RetryAfterSeconds = -1.0f);

    // Signal that connection was successful (reset backoff)
    void OnConnectionSuccess();

    // Signal connection error (apply backoff)
    void OnConnectionError();

    // ========================================================================
    // STATE QUERIES
    // ========================================================================

    // Check if we're currently in backoff state
    bool IsBackingOff() const;

    // Get current backoff time remaining (seconds)
    float GetBackoffRemaining() const;

    // Get current queue length
    int32 GetQueueLength() const;

    // Get current queue size in bytes (estimated)
    int32 GetQueueBytes() const;

    // Get current token count (messages)
    float GetAvailableTokens() const;

    // Get current bytes token count
    int32 GetAvailableBytesTokens() const;

    // Get messages sent in the last second
    int32 GetMessagesSentLastSecond() const;

    // Get bytes sent in the last second
    int32 GetBytesSentLastSecond() const;

    // Get messages dropped count
    int32 GetMessagesDropped() const;

    // Get current effective rate limit (may be adjusted by adaptive control)
    float GetCurrentRateLimit() const;

    // Get queue pressure (0.0 - 1.0)
    float GetQueuePressure() const;

    // Get full metrics snapshot
    FRshipRateLimiterMetrics GetMetrics() const;

    // Reset statistics
    void ResetStats();

    // ========================================================================
    // DELEGATES
    // ========================================================================

    FOnMessageReadyToSend OnMessageReadyToSend;
    FOnRateLimiterStatus OnRateLimiterStatus;
    FOnMetricsUpdated OnMetricsUpdated;

private:
    // ========================================================================
    // TOKEN BUCKET STATE
    // ========================================================================

    float MessageTokens;              // Current message tokens
    int32 BytesTokens;                // Current bytes tokens
    double LastTokenRefill;           // Last token refill time

    // ========================================================================
    // ADAPTIVE RATE CONTROL
    // ========================================================================

    float CurrentRateMultiplier;      // 0.0 - 1.0, multiplier on MaxMessagesPerSecond
    double LastRateAdjustment;        // Last rate adjustment time
    bool bBackpressureDetected;       // True if backpressure detected this interval

    // ========================================================================
    // QUEUE STATE
    // ========================================================================

    TArray<FRshipQueuedMessage> MessageQueue;
    int32 MessageQueueHead = 0;
    int32 QueueBytesEstimate;         // Estimated total bytes in queue

    FORCEINLINE int32 GetActiveMessageQueueCount() const
    {
        return MessageQueue.Num() - MessageQueueHead;
    }

    void CompactMessageQueue_NoLock()
    {
        if (MessageQueueHead == 0)
        {
            return;
        }

        if (MessageQueueHead >= MessageQueue.Num())
        {
            MessageQueue.Reset();
            MessageQueueHead = 0;
            return;
        }

        MessageQueue.RemoveAt(0, MessageQueueHead, EAllowShrinking::No);
        MessageQueueHead = 0;
    }

    // ========================================================================
    // BATCHING STATE
    // ========================================================================

    TArray<FRshipQueuedMessage> CurrentBatch;
    int32 CurrentBatchBytes;
    double BatchStartTime;

    // ========================================================================
    // BACKOFF STATE
    // ========================================================================

    bool bIsBackingOff;
    float CurrentBackoffSeconds;
    double BackoffStartTime;
    int32 ConsecutiveBackoffs;

    // ========================================================================
    // DOWNSAMPLING STATE
    // ========================================================================

    TMap<FString, int32> DownsampleCounters;  // CoalesceKey -> counter

    // ========================================================================
    // THREAD SAFETY
    // ========================================================================

    mutable FCriticalSection QueueLock;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    FRshipRateLimiterConfig Config;

    // ========================================================================
    // METRICS
    // ========================================================================

    FRshipRateLimiterMetrics Metrics;
    TArray<double> RecentSendTimes;
    TArray<int32> RecentSendBytes;
    TArray<double> RecentDropTimes;
    double LastMetricsLog;

    // ========================================================================
    // INTERNAL HELPERS
    // ========================================================================

    // Token bucket operations
    void RefillTokens();
    bool ConsumeMessageToken();
    bool ConsumeBytesTokens(int32 Bytes);
    bool HasSufficientTokens(int32 Bytes) const;

    // Queue operations
    void DropExpiredMessages();
    bool ShouldDownsample(const FRshipQueuedMessage& Msg);
    int32 EstimateMessageBytes(const TSharedPtr<FJsonObject>& Payload);

    // Batching
    bool FlushBatch();
    bool ShouldFlushBatch() const;
    bool HasSufficientBatchAppendTokens(const FRshipQueuedMessage& Msg) const;
    bool HasSufficientBatchTokens() const;
    void AddToBatch(FRshipQueuedMessage& Msg);

    // Adaptive rate control
    void UpdateAdaptiveRate();

    // Backoff
    void ApplyBackoff(float Seconds);
    void ResetBackoff();

    // Serialization
    FString SerializeMessage(const TSharedPtr<FJsonObject>& Payload);
    FString SerializeBatch(const TArray<FRshipQueuedMessage>& Batch);

    // Logging and metrics
    void LogMessage(int32 Verbosity, const FString& Message);
    void UpdateMetrics();
    void LogMetricsSummary();
};
