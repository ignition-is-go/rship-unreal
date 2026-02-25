/**
 * Adaptive Outbound Pipeline Implementation
 *
 * This implementation provides high-throughput WebSocket communication with:
 * - Message batching to reduce per-message overhead
 * - Dual token bucket (messages + bytes)
 * - Priority-based queue with downsampling
 * - Adaptive rate control based on observed backpressure
 * - Comprehensive instrumentation
 */

#include "RshipRateLimiter.h"
#include "Logs.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// ============================================================================
// CONSTANTS
// ============================================================================

namespace RshipRateLimiterConstants
{
    // Batch wrapper event name for server-side batch processing
    constexpr const TCHAR* BatchEventName = TEXT("ws:m:batch");

    // Minimum bytes estimate for a JSON message (object wrapper overhead)
    constexpr int32 MinMessageBytes = 20;

    // Default bytes per character estimate for JSON serialization
    constexpr float BytesPerJsonChar = 1.5f;

    // Metrics window for calculating per-second rates
    constexpr double MetricsWindowSeconds = 1.0;

    // Cleanup old metrics entries after this many seconds
    constexpr double MetricsCleanupThreshold = 2.0;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

FRshipRateLimiter::FRshipRateLimiter()
    : MessageTokens(0.0f)
    , BytesTokens(0)
    , LastTokenRefill(0.0)
    , CurrentRateMultiplier(1.0f)
    , LastRateAdjustment(0.0)
    , bBackpressureDetected(false)
    , QueueBytesEstimate(0)
    , CurrentBatchBytes(0)
    , BatchStartTime(0.0)
    , bIsBackingOff(false)
    , CurrentBackoffSeconds(0.0f)
    , BackoffStartTime(0.0)
    , ConsecutiveBackoffs(0)
    , LastMetricsLog(0.0)
{
}

FRshipRateLimiter::~FRshipRateLimiter()
{
    ClearQueue();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void FRshipRateLimiter::Initialize(const FRshipRateLimiterConfig& InConfig)
{
    FScopeLock Lock(&QueueLock);

    Config = InConfig;

    // Initialize message token bucket
    MessageTokens = static_cast<float>(Config.MaxBurstSize);
    LastTokenRefill = FPlatformTime::Seconds();

    // Initialize bytes token bucket
    BytesTokens = Config.MaxBurstBytes;

    // Reset adaptive rate
    CurrentRateMultiplier = 1.0f;
    LastRateAdjustment = FPlatformTime::Seconds();
    bBackpressureDetected = false;

    // Reset backoff
    bIsBackingOff = false;
    CurrentBackoffSeconds = 0.0f;
    BackoffStartTime = 0.0;
    ConsecutiveBackoffs = 0;

    // Reset metrics
    Metrics.Reset();
    LastMetricsLog = FPlatformTime::Seconds();

    LogMessage(2, FString::Printf(TEXT("RateLimiter initialized: %.1f msg/s, burst=%d, batching=%s, adaptive=%s"),
        Config.MaxMessagesPerSecond,
        Config.MaxBurstSize,
        Config.bEnableBatching ? TEXT("ON") : TEXT("OFF"),
        Config.bEnableAdaptiveRate ? TEXT("ON") : TEXT("OFF")));
}

void FRshipRateLimiter::UpdateConfig(const FRshipRateLimiterConfig& InConfig)
{
    FScopeLock Lock(&QueueLock);

    Config = InConfig;

    LogMessage(2, FString::Printf(TEXT("RateLimiter config updated: %.1f msg/s, burst=%d"),
        Config.MaxMessagesPerSecond, Config.MaxBurstSize));
}

// ============================================================================
// MESSAGE ENQUEUE
// ============================================================================

bool FRshipRateLimiter::EnqueueMessage(TSharedPtr<FJsonObject> Payload,
                                        ERshipMessagePriority Priority,
                                        ERshipMessageType Type,
                                        const FString& CoalesceKey)
{
    FScopeLock Lock(&QueueLock);

    // Calculate queue pressure
    const int32 ActiveMessageCount = GetActiveMessageQueueCount();
    float QueuePressure = Config.MaxQueueLength > 0
        ? static_cast<float>(ActiveMessageCount) / static_cast<float>(Config.MaxQueueLength)
        : 0.0f;

    // Check if we should downsample this message
    if (Config.bEnableDownsampling && QueuePressure >= Config.QueuePressureThreshold)
    {
        // Create a temporary message for downsampling check
        FRshipQueuedMessage TempMsg(Payload, Priority, Type, CoalesceKey);
        if (ShouldDownsample(TempMsg))
        {
            Metrics.MessagesDownsampledTotal++;
            LogMessage(3, FString::Printf(TEXT("Downsampled message (Priority: %d, Key: %s, Pressure: %.1f%%)"),
                static_cast<int32>(Priority), *CoalesceKey, QueuePressure * 100.0f));
            return false;
        }
    }

    // Check queue capacity
    if (Config.MaxQueueLength > 0 && ActiveMessageCount >= Config.MaxQueueLength)
    {
        // Try to make room by dropping low priority messages
        bool bDropped = false;
        for (int32 i = MessageQueue.Num() - 1; i >= MessageQueueHead; --i)
        {
            if (MessageQueue[i].Priority > Priority)  // Lower priority (higher enum value)
            {
                const ERshipMessagePriority DroppedPriority = MessageQueue[i].Priority;
                if (Config.bLogRateLimitEvents)
                {
                    LogMessage(1, FString::Printf(TEXT("Dropping queued message to make room (Priority: %d -> %d, Key: %s)"),
                        static_cast<int32>(MessageQueue[i].Priority), static_cast<int32>(Priority), *MessageQueue[i].CoalesceKey));
                }

                QueueBytesEstimate -= MessageQueue[i].EstimatedBytes;
                MessageQueue.RemoveAt(i);
                Metrics.MessagesDroppedTotal++;

                // Track drops by priority
                switch (DroppedPriority)
                {
                case ERshipMessagePriority::Critical: Metrics.DroppedCritical++; break;
                case ERshipMessagePriority::High: Metrics.DroppedHigh++; break;
                case ERshipMessagePriority::Normal: Metrics.DroppedNormal++; break;
                case ERshipMessagePriority::Low: Metrics.DroppedLow++; break;
                }

                bDropped = true;
                break;
            }
        }

        // If we couldn't drop anything and this isn't critical, reject the new message
        if (!bDropped && Priority > ERshipMessagePriority::Critical)
        {
            if (Config.bLogRateLimitEvents)
            {
                LogMessage(1, FString::Printf(TEXT("Queue full, dropping incoming message (Priority: %d, Key: %s)"),
                    static_cast<int32>(Priority), *CoalesceKey));
            }

            Metrics.MessagesDroppedTotal++;
            RecentDropTimes.Add(FPlatformTime::Seconds());

            // Track drops by priority
            switch (Priority)
            {
            case ERshipMessagePriority::Critical: Metrics.DroppedCritical++; break;
            case ERshipMessagePriority::High: Metrics.DroppedHigh++; break;
            case ERshipMessagePriority::Normal: Metrics.DroppedNormal++; break;
            case ERshipMessagePriority::Low: Metrics.DroppedLow++; break;
            }

            return false;
        }
    }

    // Create queued message
    FRshipQueuedMessage QueuedMsg(Payload, Priority, Type, CoalesceKey);
    QueuedMsg.EstimatedBytes = EstimateMessageBytes(Payload);

    // Handle coalescing for messages with the same key
    if (Config.bEnableCoalescing && !CoalesceKey.IsEmpty())
    {
        for (int32 i = MessageQueueHead; i < MessageQueue.Num(); ++i)
        {
            if (MessageQueue[i].CoalesceKey == CoalesceKey && MessageQueue[i].Type == Type)
            {
                // Replace older message with newer one
                LogMessage(3, FString::Printf(TEXT("Coalescing message with key: %s"), *CoalesceKey));

                QueueBytesEstimate -= MessageQueue[i].EstimatedBytes;
                MessageQueue[i] = QueuedMsg;
                QueueBytesEstimate += QueuedMsg.EstimatedBytes;
                Metrics.MessagesCoalescedTotal++;
                return true;
            }
        }
    }

    const int32 ActiveCountAfterDrop = GetActiveMessageQueueCount();
    int32 Low = 0;
    int32 High = ActiveCountAfterDrop;
    auto QueueSortLess = [](const FRshipQueuedMessage& A, const FRshipQueuedMessage& B)
    {
        if (A.Priority != B.Priority)
        {
            return static_cast<uint8>(A.Priority) < static_cast<uint8>(B.Priority);
        }
        return A.QueuedTime < B.QueuedTime;
    };
    while (Low < High)
    {
        const int32 Mid = (Low + High) / 2;
        if (QueueSortLess(MessageQueue[MessageQueueHead + Mid], QueuedMsg))
        {
            Low = Mid + 1;
        }
        else
        {
            High = Mid;
        }
    }
    const int32 InsertIndex = MessageQueueHead + FMath::Clamp(Low, 0, ActiveCountAfterDrop);
    MessageQueue.Insert(MoveTemp(QueuedMsg), InsertIndex);
    QueueBytesEstimate += QueuedMsg.EstimatedBytes;

    LogMessage(3, FString::Printf(TEXT("Enqueued message (Priority: %d, Type: %d, Queue: %d, Bytes: %d)"),
        static_cast<int32>(Priority), static_cast<int32>(Type), GetActiveMessageQueueCount(), QueueBytesEstimate));

    return true;
}

// ============================================================================
// QUEUE PROCESSING
// ============================================================================

int32 FRshipRateLimiter::ProcessQueue()
{
    FScopeLock Lock(&QueueLock);

    double Now = FPlatformTime::Seconds();
    int32 ActiveMessageCount = GetActiveMessageQueueCount();
    if (ActiveMessageCount == 0)
    {
        return 0;
    }

    // Update adaptive rate control
    if (Config.bEnableAdaptiveRate)
    {
        UpdateAdaptiveRate();
    }

    // Check backoff state
    if (bIsBackingOff)
    {
        float Elapsed = static_cast<float>(Now - BackoffStartTime);

        if (Elapsed < CurrentBackoffSeconds)
        {
            // Still in backoff period - but check for critical bypass
            if (Config.bCriticalBypassBackoff)
            {
                // Process only critical messages
                int32 CriticalSent = 0;
                while (ActiveMessageCount > 0 && MessageQueue[MessageQueueHead].Priority == ERshipMessagePriority::Critical)
                {
                    if (!HasSufficientTokens(MessageQueue[MessageQueueHead].EstimatedBytes))
                    {
                        bBackpressureDetected = true;
                        break;
                    }

                    ConsumeMessageToken();
                    ConsumeBytesTokens(MessageQueue[MessageQueueHead].EstimatedBytes);

                    // Send critical message immediately
                    FString JsonString = SerializeMessage(MessageQueue[MessageQueueHead].Payload);
                    if (!JsonString.IsEmpty() && OnMessageReadyToSend.IsBound())
                    {
                        OnMessageReadyToSend.Execute(JsonString);
                        CriticalSent++;

                        int32 BytesSent = JsonString.Len();
                        RecentSendTimes.Add(Now);
                        RecentSendBytes.Add(BytesSent);
                    }

                    QueueBytesEstimate -= MessageQueue[MessageQueueHead].EstimatedBytes;
                    ++MessageQueueHead;
                    --ActiveMessageCount;
                }

                if (MessageQueueHead > 0 && ActiveMessageCount > 0 &&
                    MessageQueueHead > FMath::Max(256, ActiveMessageCount / 2))
                {
                    CompactMessageQueue_NoLock();
                }

                if (CriticalSent > 0)
                {
                    LogMessage(2, FString::Printf(TEXT("Sent %d critical messages during backoff"), CriticalSent));
                }

                return CriticalSent;
            }

            // Not bypassing - return
            return 0;
        }

        // Backoff period ended
        ResetBackoff();
    }

    // Refill tokens
    RefillTokens();

    // Drop expired messages
    DropExpiredMessages();
    ActiveMessageCount = GetActiveMessageQueueCount();
    if (ActiveMessageCount == 0)
    {
        if (CurrentBatch.Num() > 0 && ShouldFlushBatch())
        {
            FlushBatch();
        }
        UpdateMetrics();
        return 0;
    }

    // Process messages
    int32 MessagesSent = 0;

    while (ActiveMessageCount > 0)
    {
        FRshipQueuedMessage& Msg = MessageQueue[MessageQueueHead];

        // Check for critical bypass batching
        if (Config.bEnableBatching && Config.bCriticalBypassBatching &&
            Msg.Priority == ERshipMessagePriority::Critical)
        {
            // Flush any existing batch first
            if (CurrentBatch.Num() > 0)
            {
                if (!FlushBatch())
                {
                    bBackpressureDetected = true;
                    break;
                }
            }

            // Send critical message immediately without batching
            if (HasSufficientTokens(Msg.EstimatedBytes))
            {
                ConsumeMessageToken();
                ConsumeBytesTokens(Msg.EstimatedBytes);

                FString JsonString = SerializeMessage(Msg.Payload);
                if (!JsonString.IsEmpty() && OnMessageReadyToSend.IsBound())
                {
                    OnMessageReadyToSend.Execute(JsonString);
                    MessagesSent++;

                    int32 BytesSent = JsonString.Len();
                    RecentSendTimes.Add(Now);
                    RecentSendBytes.Add(BytesSent);

                    LogMessage(3, FString::Printf(TEXT("Sent critical message immediately (%d bytes)"), BytesSent));
                }

                QueueBytesEstimate -= Msg.EstimatedBytes;
                ++MessageQueueHead;
                --ActiveMessageCount;
                continue;
            }
            else
            {
                // No tokens - mark backpressure
                bBackpressureDetected = true;
                break;
            }
        }

        // Check if we have tokens
        if (!HasSufficientTokens(Msg.EstimatedBytes))
        {
            // No tokens available - check if we should flush partial batch
            if (Config.bEnableBatching && CurrentBatch.Num() > 0)
            {
                if (!FlushBatch())
                {
                    bBackpressureDetected = true;
                    break;
                }
            }
            bBackpressureDetected = true;
            break;
        }

        // Batching logic
        if (Config.bEnableBatching)
        {
            // Flush if the next message would overflow batch limits
            if (CurrentBatch.Num() > 0 &&
                CurrentBatchBytes + Msg.EstimatedBytes > Config.MaxBatchBytes)
            {
                if (!FlushBatch())
                {
                    bBackpressureDetected = true;
                    break;
                }

                // Re-evaluate this message against fresh token state after flush
                continue;
            }

            // Check if batch should be flushed before adding this message
            if (ShouldFlushBatch())
            {
                if (!FlushBatch())
                {
                    bBackpressureDetected = true;
                    break;
                }
            }

            // New batch entries consume only the batch token once and
            // must respect cumulative byte budget on the current batch.
            if (!HasSufficientBatchAppendTokens(Msg))
            {
                bBackpressureDetected = true;
                break;
            }

            // Add to batch
            AddToBatch(Msg);
            QueueBytesEstimate -= Msg.EstimatedBytes;
            ++MessageQueueHead;
            --ActiveMessageCount;
            MessagesSent++;
        }
        else
        {
            // No batching - send directly
            ConsumeMessageToken();
            ConsumeBytesTokens(Msg.EstimatedBytes);

            FString JsonString = SerializeMessage(Msg.Payload);
            if (!JsonString.IsEmpty() && OnMessageReadyToSend.IsBound())
            {
                OnMessageReadyToSend.Execute(JsonString);
                MessagesSent++;

                int32 BytesSent = JsonString.Len();
                RecentSendTimes.Add(Now);
                RecentSendBytes.Add(BytesSent);
            }

            QueueBytesEstimate -= Msg.EstimatedBytes;
            ++MessageQueueHead;
            --ActiveMessageCount;
        }
    }

    if (ActiveMessageCount == 0)
    {
        MessageQueue.Reset();
        MessageQueueHead = 0;
    }
    else if (MessageQueueHead > 0 && MessageQueueHead > FMath::Max(256, ActiveMessageCount / 2))
    {
        CompactMessageQueue_NoLock();
    }

    // Check if batch should be flushed due to time
    if (Config.bEnableBatching && ShouldFlushBatch())
    {
        if (!FlushBatch())
        {
            bBackpressureDetected = true;
        }
    }

    // Update metrics
    UpdateMetrics();

    // Periodic metrics logging
    if (Config.bEnableMetrics && Config.MetricsLogInterval > 0.0f)
    {
        if (Now - LastMetricsLog >= Config.MetricsLogInterval)
        {
            LogMetricsSummary();
            LastMetricsLog = Now;
        }
    }

    return MessagesSent;
}

// ============================================================================
// QUEUE MAINTENANCE
// ============================================================================

void FRshipRateLimiter::DropExpiredMessages()
{
    if (Config.MessageTimeoutSeconds <= 0.0f)
    {
        return;
    }

    double Now = FPlatformTime::Seconds();
    double ExpiryThreshold = Now - Config.MessageTimeoutSeconds;

    for (int32 i = MessageQueue.Num() - 1; i >= MessageQueueHead; --i)
    {
        // Don't drop critical messages due to timeout
        if (MessageQueue[i].Priority == ERshipMessagePriority::Critical)
        {
            continue;
        }

        if (MessageQueue[i].QueuedTime < ExpiryThreshold)
        {
            if (Config.bLogRateLimitEvents)
            {
                LogMessage(1, FString::Printf(TEXT("Dropping expired message (age: %.1fs, priority: %d, key: %s)"),
                    Now - MessageQueue[i].QueuedTime, static_cast<int32>(MessageQueue[i].Priority), *MessageQueue[i].CoalesceKey));
            }

            // Track drops by priority
            switch (MessageQueue[i].Priority)
            {
            case ERshipMessagePriority::High: Metrics.DroppedHigh++; break;
            case ERshipMessagePriority::Normal: Metrics.DroppedNormal++; break;
            case ERshipMessagePriority::Low: Metrics.DroppedLow++; break;
            default: break;
            }

            QueueBytesEstimate -= MessageQueue[i].EstimatedBytes;
            MessageQueue.RemoveAt(i);
            Metrics.MessagesDroppedTotal++;
            RecentDropTimes.Add(Now);
        }
    }
}

void FRshipRateLimiter::ClearQueue()
{
    FScopeLock Lock(&QueueLock);

    int32 DroppedCount = GetActiveMessageQueueCount();

    // Flush any pending batch
    CurrentBatch.Empty();
    CurrentBatchBytes = 0;
    BatchStartTime = 0.0;

    // Clear main queue
    MessageQueue.Empty();
    MessageQueueHead = 0;
    QueueBytesEstimate = 0;

    // Clear downsampling counters
    DownsampleCounters.Empty();

    if (DroppedCount > 0)
    {
        LogMessage(2, FString::Printf(TEXT("Queue cleared, dropped %d messages"), DroppedCount));
    }
}
// ============================================================================
// BATCHING
// ============================================================================

void FRshipRateLimiter::AddToBatch(FRshipQueuedMessage& Msg)
{
    if (CurrentBatch.Num() == 0)
    {
        BatchStartTime = FPlatformTime::Seconds();
    }

    CurrentBatch.Add(Msg);
    CurrentBatchBytes += Msg.EstimatedBytes;

    LogMessage(3, FString::Printf(TEXT("Added to batch (Count: %d, Bytes: %d)"),
        CurrentBatch.Num(), CurrentBatchBytes));
}

bool FRshipRateLimiter::ShouldFlushBatch() const
{
    if (CurrentBatch.Num() == 0)
    {
        return false;
    }

    // Flush if max messages reached
    if (CurrentBatch.Num() >= Config.MaxBatchMessages)
    {
        return true;
    }

    // Flush if max bytes reached
    if (CurrentBatchBytes >= Config.MaxBatchBytes)
    {
        return true;
    }

    // Flush if max interval reached
    double Now = FPlatformTime::Seconds();
    double ElapsedMs = (Now - BatchStartTime) * 1000.0;
    if (ElapsedMs >= Config.MaxBatchIntervalMs)
    {
        return true;
    }

    return false;
}

bool FRshipRateLimiter::HasSufficientBatchAppendTokens(const FRshipQueuedMessage& Msg) const
{
    if (CurrentBatch.Num() == 0)
    {
        return HasSufficientTokens(Msg.EstimatedBytes);
    }

    if (!Config.bEnableBytesRateLimiting)
    {
        return true;
    }

    return (CurrentBatchBytes + Msg.EstimatedBytes) <= BytesTokens;
}

bool FRshipRateLimiter::HasSufficientBatchTokens() const
{
    return CurrentBatch.Num() > 0 &&
        MessageTokens >= 1.0f &&
        (!Config.bEnableBytesRateLimiting || BytesTokens >= CurrentBatchBytes);
}

bool FRshipRateLimiter::FlushBatch()
{
    if (CurrentBatch.Num() == 0)
    {
        return false;
    }

    if (!HasSufficientBatchTokens())
    {
        bBackpressureDetected = true;
        return false;
    }

    if (!ConsumeMessageToken())
    {
        bBackpressureDetected = true;
        return false;
    }
    ConsumeBytesTokens(CurrentBatchBytes);

    // ConsumeBytesTokens is validated by HasSufficientBatchTokens, safe here.

    // Serialize and send
    FString BatchJson = SerializeBatch(CurrentBatch);
    if (!BatchJson.IsEmpty() && OnMessageReadyToSend.IsBound())
    {
        OnMessageReadyToSend.Execute(BatchJson);

        double Now = FPlatformTime::Seconds();
        int32 BytesSent = BatchJson.Len();
        RecentSendTimes.Add(Now);
        RecentSendBytes.Add(BytesSent);

        if (Config.bLogBatchDetails)
        {
            LogMessage(2, FString::Printf(TEXT("Sent batch: %d messages, %d bytes (efficiency: %.1f msg/frame)"),
                CurrentBatch.Num(), BytesSent, static_cast<float>(CurrentBatch.Num())));
        }
    }

    // Clear batch state
    CurrentBatch.Empty();
    CurrentBatchBytes = 0;
    BatchStartTime = 0.0;

    return true;
}

// ============================================================================
// DOWNSAMPLING
// ============================================================================

bool FRshipRateLimiter::ShouldDownsample(const FRshipQueuedMessage& Msg)
{
    // Never downsample Critical or High priority
    if (Msg.Priority <= ERshipMessagePriority::High)
    {
        return false;
    }

    // Get sample rate based on priority
    int32 SampleRate = 1;
    switch (Msg.Priority)
    {
    case ERshipMessagePriority::Normal:
        SampleRate = Config.NormalPrioritySampleRate;
        break;
    case ERshipMessagePriority::Low:
        SampleRate = Config.LowPrioritySampleRate;
        break;
    default:
        return false;
    }

    if (SampleRate <= 1)
    {
        return false;  // No downsampling (keep all)
    }

    // Use coalesce key for per-source sampling, or empty key for global sampling
    FString SampleKey = Msg.CoalesceKey.IsEmpty()
        ? FString::Printf(TEXT("_global_%d"), static_cast<int32>(Msg.Priority))
        : Msg.CoalesceKey;

    // Increment counter
    int32& Counter = DownsampleCounters.FindOrAdd(SampleKey);
    Counter++;

    // Keep every Nth sample
    if (Counter >= SampleRate)
    {
        Counter = 0;
        return false;  // Keep this one
    }

    return true;  // Downsample (skip) this one
}

// ============================================================================
// ADAPTIVE RATE CONTROL
// ============================================================================

void FRshipRateLimiter::UpdateAdaptiveRate()
{
    double Now = FPlatformTime::Seconds();

    // Only adjust at configured intervals
    if (Now - LastRateAdjustment < Config.RateAdjustmentInterval)
    {
        return;
    }

    float OldMultiplier = CurrentRateMultiplier;

    if (bBackpressureDetected || bIsBackingOff)
    {
        // Decrease rate
        CurrentRateMultiplier *= Config.RateDecreaseFactor;
        CurrentRateMultiplier = FMath::Max(CurrentRateMultiplier, Config.MinRateFraction);

        LogMessage(2, FString::Printf(TEXT("Adaptive rate decreased: %.1f%% -> %.1f%% (backpressure detected)"),
            OldMultiplier * 100.0f, CurrentRateMultiplier * 100.0f));
    }
    else
    {
        // Gradually increase rate
        CurrentRateMultiplier *= Config.RateIncreaseFactor;
        CurrentRateMultiplier = FMath::Min(CurrentRateMultiplier, 1.0f);

        if (CurrentRateMultiplier != OldMultiplier)
        {
            LogMessage(3, FString::Printf(TEXT("Adaptive rate increased: %.1f%% -> %.1f%%"),
                OldMultiplier * 100.0f, CurrentRateMultiplier * 100.0f));
        }
    }

    // Reset backpressure flag for next interval
    bBackpressureDetected = false;
    LastRateAdjustment = Now;
}

// ============================================================================
// TOKEN BUCKET
// ============================================================================

void FRshipRateLimiter::RefillTokens()
{
    double Now = FPlatformTime::Seconds();
    float DeltaTime = static_cast<float>(Now - LastTokenRefill);
    LastTokenRefill = Now;

    // Calculate effective rate with adaptive multiplier
    float EffectiveRate = Config.MaxMessagesPerSecond * CurrentRateMultiplier;

    // Refill message tokens
    float MessageTokensToAdd = DeltaTime * EffectiveRate;
    MessageTokens = FMath::Min(MessageTokens + MessageTokensToAdd, static_cast<float>(Config.MaxBurstSize));

    // Refill bytes tokens
    if (Config.bEnableBytesRateLimiting)
    {
        float EffectiveBytesRate = static_cast<float>(Config.MaxBytesPerSecond) * CurrentRateMultiplier;
        int32 BytesTokensToAdd = static_cast<int32>(DeltaTime * EffectiveBytesRate);
        BytesTokens = FMath::Min(BytesTokens + BytesTokensToAdd, Config.MaxBurstBytes);
    }
    else
    {
        BytesTokens = Config.MaxBurstBytes;  // Effectively unlimited
    }
}

bool FRshipRateLimiter::ConsumeMessageToken()
{
    if (MessageTokens >= 1.0f)
    {
        MessageTokens -= 1.0f;
        return true;
    }
    return false;
}

bool FRshipRateLimiter::ConsumeBytesTokens(int32 Bytes)
{
    if (!Config.bEnableBytesRateLimiting)
    {
        return true;
    }

    if (BytesTokens >= Bytes)
    {
        BytesTokens -= Bytes;
        return true;
    }
    return false;
}

bool FRshipRateLimiter::HasSufficientTokens(int32 Bytes) const
{
    if (MessageTokens < 1.0f)
    {
        return false;
    }

    if (Config.bEnableBytesRateLimiting && BytesTokens < Bytes)
    {
        return false;
    }

    return true;
}

// ============================================================================
// BACKOFF
// ============================================================================

void FRshipRateLimiter::OnRateLimitError(float RetryAfterSeconds)
{
    FScopeLock Lock(&QueueLock);

    float BackoffTime = RetryAfterSeconds;

    if (BackoffTime <= 0.0f)
    {
        // No Retry-After header, use exponential backoff
        if (bIsBackingOff)
        {
            BackoffTime = FMath::Min(CurrentBackoffSeconds * Config.BackoffMultiplier, Config.MaxBackoffSeconds);
        }
        else
        {
            BackoffTime = Config.InitialBackoffSeconds;
        }
    }

    ApplyBackoff(BackoffTime);

    if (Config.bLogRateLimitEvents)
    {
        LogMessage(0, FString::Printf(TEXT("Rate limit error - backing off for %.1f seconds (consecutive: %d)"),
            CurrentBackoffSeconds, ConsecutiveBackoffs));
    }
}

void FRshipRateLimiter::OnConnectionSuccess()
{
    FScopeLock Lock(&QueueLock);

    if (bIsBackingOff)
    {
        LogMessage(2, TEXT("Connection successful, resetting backoff"));
        ResetBackoff();
    }
}

void FRshipRateLimiter::OnConnectionError()
{
    FScopeLock Lock(&QueueLock);

    float BackoffTime;
    if (bIsBackingOff)
    {
        BackoffTime = FMath::Min(CurrentBackoffSeconds * Config.BackoffMultiplier, Config.MaxBackoffSeconds);
    }
    else
    {
        BackoffTime = Config.InitialBackoffSeconds;
    }

    ApplyBackoff(BackoffTime);

    LogMessage(1, FString::Printf(TEXT("Connection error - backing off for %.1f seconds"), CurrentBackoffSeconds));
}

void FRshipRateLimiter::ApplyBackoff(float Seconds)
{
    if (Seconds < 0.0f)
    {
        Seconds = 0.0f;
    }

    const float JitterPercent = FMath::Clamp(Config.BackoffJitterPercent, 0.0f, 100.0f);
    if (JitterPercent > 0.0f)
    {
        const float JitterWindow = Seconds * (JitterPercent * 0.01f);
        const float MinDelay = FMath::Max(0.05f, Seconds - JitterWindow);
        const float MaxDelay = FMath::Max(MinDelay, Seconds + JitterWindow);
        Seconds = FMath::FRandRange(MinDelay, MaxDelay);
    }

    bIsBackingOff = true;
    CurrentBackoffSeconds = Seconds;
    BackoffStartTime = FPlatformTime::Seconds();
    ConsecutiveBackoffs++;
    Metrics.BackoffCount = ConsecutiveBackoffs;

    // Also trigger adaptive rate decrease
    bBackpressureDetected = true;

    if (OnRateLimiterStatus.IsBound())
    {
        OnRateLimiterStatus.Execute(true, CurrentBackoffSeconds);
    }
}

void FRshipRateLimiter::ResetBackoff()
{
    bool WasBackingOff = bIsBackingOff;

    bIsBackingOff = false;
    CurrentBackoffSeconds = 0.0f;
    BackoffStartTime = 0.0;
    ConsecutiveBackoffs = 0;

    if (WasBackingOff && OnRateLimiterStatus.IsBound())
    {
        OnRateLimiterStatus.Execute(false, 0.0f);
    }
}

// ============================================================================
// SERIALIZATION
// ============================================================================

int32 FRshipRateLimiter::EstimateMessageBytes(const TSharedPtr<FJsonObject>& Payload)
{
    if (!Payload.IsValid())
    {
        return RshipRateLimiterConstants::MinMessageBytes;
    }

    // Quick estimate based on field count and values
    int32 Estimate = RshipRateLimiterConstants::MinMessageBytes;

    for (const auto& Pair : Payload->Values)
    {
        Estimate += Pair.Key.Len() * 2;  // Key + quotes

        const TSharedPtr<FJsonValue>& Value = Pair.Value;
        if (Value.IsValid())
        {
            switch (Value->Type)
            {
            case EJson::String:
                Estimate += Value->AsString().Len() + 2;
                break;
            case EJson::Number:
                Estimate += 10;  // Average number length
                break;
            case EJson::Boolean:
                Estimate += 5;
                break;
            case EJson::Object:
                Estimate += 50;  // Nested object estimate
                break;
            case EJson::Array:
                Estimate += 50;  // Array estimate
                break;
            default:
                Estimate += 4;
                break;
            }
        }
    }

    return Estimate;
}

FString FRshipRateLimiter::SerializeMessage(const TSharedPtr<FJsonObject>& Payload)
{
    if (!Payload.IsValid())
    {
        return FString();
    }

    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);

    if (!FJsonSerializer::Serialize(Payload.ToSharedRef(), JsonWriter))
    {
        LogMessage(0, TEXT("Failed to serialize message JSON"));
        return FString();
    }

    return JsonString;
}

FString FRshipRateLimiter::SerializeBatch(const TArray<FRshipQueuedMessage>& Batch)
{
    if (Batch.Num() == 0)
    {
        return FString();
    }

    // If only one message, send it directly without batch wrapper
    if (Batch.Num() == 1)
    {
        return SerializeMessage(Batch[0].Payload);
    }

    // Create batch wrapper
    TSharedPtr<FJsonObject> BatchWrapper = MakeShareable(new FJsonObject);
    BatchWrapper->SetStringField(TEXT("event"), RshipRateLimiterConstants::BatchEventName);

    // Create array of payloads
    TArray<TSharedPtr<FJsonValue>> PayloadArray;
    for (const FRshipQueuedMessage& Msg : Batch)
    {
        if (Msg.Payload.IsValid())
        {
            PayloadArray.Add(MakeShareable(new FJsonValueObject(Msg.Payload)));
        }
    }

    BatchWrapper->SetArrayField(TEXT("data"), PayloadArray);

    return SerializeMessage(BatchWrapper);
}

// ============================================================================
// STATE QUERIES
// ============================================================================

bool FRshipRateLimiter::IsBackingOff() const
{
    FScopeLock Lock(&QueueLock);
    return bIsBackingOff;
}

float FRshipRateLimiter::GetBackoffRemaining() const
{
    FScopeLock Lock(&QueueLock);

    if (!bIsBackingOff)
    {
        return 0.0f;
    }

    double Now = FPlatformTime::Seconds();
    float Elapsed = static_cast<float>(Now - BackoffStartTime);
    return FMath::Max(0.0f, CurrentBackoffSeconds - Elapsed);
}

int32 FRshipRateLimiter::GetQueueLength() const
{
    FScopeLock Lock(&QueueLock);
    return GetActiveMessageQueueCount() + CurrentBatch.Num();
}

int32 FRshipRateLimiter::GetQueueBytes() const
{
    FScopeLock Lock(&QueueLock);
    return QueueBytesEstimate + CurrentBatchBytes;
}

float FRshipRateLimiter::GetAvailableTokens() const
{
    FScopeLock Lock(&QueueLock);
    return MessageTokens;
}

int32 FRshipRateLimiter::GetAvailableBytesTokens() const
{
    FScopeLock Lock(&QueueLock);
    return BytesTokens;
}

int32 FRshipRateLimiter::GetMessagesSentLastSecond() const
{
    FScopeLock Lock(&QueueLock);
    return Metrics.MessagesSentLastSecond;
}

int32 FRshipRateLimiter::GetBytesSentLastSecond() const
{
    FScopeLock Lock(&QueueLock);
    return Metrics.BytesSentLastSecond;
}

int32 FRshipRateLimiter::GetMessagesDropped() const
{
    FScopeLock Lock(&QueueLock);
    return Metrics.MessagesDroppedTotal;
}

float FRshipRateLimiter::GetCurrentRateLimit() const
{
    FScopeLock Lock(&QueueLock);
    return Config.MaxMessagesPerSecond * CurrentRateMultiplier;
}

float FRshipRateLimiter::GetQueuePressure() const
{
    FScopeLock Lock(&QueueLock);
    if (Config.MaxQueueLength <= 0)
    {
        return 0.0f;
    }
    return static_cast<float>(GetActiveMessageQueueCount()) / static_cast<float>(Config.MaxQueueLength);
}

FRshipRateLimiterMetrics FRshipRateLimiter::GetMetrics() const
{
    FScopeLock Lock(&QueueLock);
    return Metrics;
}

void FRshipRateLimiter::ResetStats()
{
    FScopeLock Lock(&QueueLock);

    Metrics.Reset();
    RecentSendTimes.Empty();
    RecentSendBytes.Empty();
    RecentDropTimes.Empty();
    DownsampleCounters.Empty();
}

// ============================================================================
// METRICS
// ============================================================================

void FRshipRateLimiter::UpdateMetrics()
{
    double Now = FPlatformTime::Seconds();

    // Clean up old entries
    while (RecentSendTimes.Num() > 0 &&
           (Now - RecentSendTimes[0]) > RshipRateLimiterConstants::MetricsCleanupThreshold)
    {
        RecentSendTimes.RemoveAt(0);
        if (RecentSendBytes.Num() > 0)
        {
            RecentSendBytes.RemoveAt(0);
        }
    }

    while (RecentDropTimes.Num() > 0 &&
           (Now - RecentDropTimes[0]) > RshipRateLimiterConstants::MetricsCleanupThreshold)
    {
        RecentDropTimes.RemoveAt(0);
    }

    // Count entries in last second
    int32 MessagesInWindow = 0;
    int32 BytesInWindow = 0;
    int32 DropsInWindow = 0;

    for (int32 i = 0; i < RecentSendTimes.Num(); ++i)
    {
        if (Now - RecentSendTimes[i] <= RshipRateLimiterConstants::MetricsWindowSeconds)
        {
            MessagesInWindow++;
            if (i < RecentSendBytes.Num())
            {
                BytesInWindow += RecentSendBytes[i];
            }
        }
    }

    for (double DropTime : RecentDropTimes)
    {
        if (Now - DropTime <= RshipRateLimiterConstants::MetricsWindowSeconds)
        {
            DropsInWindow++;
        }
    }

    // Update metrics
    Metrics.MessagesSentLastSecond = MessagesInWindow;
    Metrics.BytesSentLastSecond = BytesInWindow;
    Metrics.MessagesDroppedLastSecond = DropsInWindow;
    Metrics.CurrentQueueLength = GetActiveMessageQueueCount();
    Metrics.CurrentQueueBytes = QueueBytesEstimate;
    Metrics.QueuePressure = GetQueuePressure();
    Metrics.CurrentRateLimit = Config.MaxMessagesPerSecond * CurrentRateMultiplier;
    Metrics.AvailableTokens = MessageTokens;
    Metrics.AvailableBytesTokens = BytesTokens;
    Metrics.bIsBackingOff = bIsBackingOff;
    Metrics.BackoffRemaining = GetBackoffRemaining();

    // Fire metrics delegate
    if (OnMetricsUpdated.IsBound())
    {
        OnMetricsUpdated.Execute(Metrics);
    }
}

void FRshipRateLimiter::LogMetricsSummary()
{
    LogMessage(2, FString::Printf(
        TEXT("Metrics: %d msg/s, %d B/s, queue=%d (%.0f%%), drops=%d, rate=%.1f/s%s%s"),
        Metrics.MessagesSentLastSecond,
        Metrics.BytesSentLastSecond,
        Metrics.CurrentQueueLength,
        Metrics.QueuePressure * 100.0f,
        Metrics.MessagesDroppedLastSecond,
        Metrics.CurrentRateLimit,
        bIsBackingOff ? TEXT(" [BACKOFF]") : TEXT(""),
        Config.bEnableBatching ? TEXT(" [BATCH]") : TEXT("")
    ));

    // Log drop breakdown if there are drops
    if (Metrics.MessagesDroppedTotal > 0)
    {
        LogMessage(2, FString::Printf(
            TEXT("  Drops total: %d (Critical=%d, High=%d, Normal=%d, Low=%d) | Downsampled=%d | Coalesced=%d"),
            Metrics.MessagesDroppedTotal,
            Metrics.DroppedCritical,
            Metrics.DroppedHigh,
            Metrics.DroppedNormal,
            Metrics.DroppedLow,
            Metrics.MessagesDownsampledTotal,
            Metrics.MessagesCoalescedTotal
        ));
    }
}

// ============================================================================
// LOGGING
// ============================================================================

void FRshipRateLimiter::LogMessage(int32 Verbosity, const FString& Message)
{
    if (Verbosity > Config.LogVerbosity)
    {
        return;
    }

    switch (Verbosity)
    {
    case 0:
        UE_LOG(LogRshipExec, Error, TEXT("RateLimiter: %s"), *Message);
        break;
    case 1:
        UE_LOG(LogRshipExec, Warning, TEXT("RateLimiter: %s"), *Message);
        break;
    case 2:
        UE_LOG(LogRshipExec, Log, TEXT("RateLimiter: %s"), *Message);
        break;
    default:
        UE_LOG(LogRshipExec, Verbose, TEXT("RateLimiter: %s"), *Message);
        break;
    }
}
