// Fill out your copyright notice in the Description page of Project Settings
#include "RshipSubsystem.h"
#include "RshipSettings.h"
#include "WebSocketsModule.h"
#include "EngineUtils.h"
#include "RshipTargetComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Json.h"
#include "JsonObjectWrapper.h"
#include "JsonObjectConverter.h"
#include "Util.h"
#include "Logging/LogMacros.h"
#include "Subsystems/SubsystemCollection.h"
#include "GameFramework/Actor.h"
#include "UObject/FieldPath.h"
#include "Misc/StructBuilder.h"
#include "UObject/UnrealTypePrivate.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Action.h"
#include "Target.h"

#include "Myko.h"
#include "EmitterHandler.h"
#include "Logs.h"
#include "RshipMsgPack.h"
#include "Async/Async.h"

// Profiling stats for performance analysis
DECLARE_STATS_GROUP(TEXT("Rship"), STATGROUP_Rship, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ProcessBatchActions"), STAT_Rship_ProcessBatchActions, STATGROUP_Rship);
DECLARE_CYCLE_STAT(TEXT("ProcessBatchActions_CacheBuild"), STAT_Rship_BatchCacheBuild, STATGROUP_Rship);
DECLARE_CYCLE_STAT(TEXT("ProcessBatchActions_Execute"), STAT_Rship_BatchExecute, STATGROUP_Rship);
DECLARE_CYCLE_STAT(TEXT("MsgpackDecode"), STAT_Rship_MsgpackDecode, STATGROUP_Rship);
DECLARE_CYCLE_STAT(TEXT("ProcessMessageDirect"), STAT_Rship_ProcessMessageDirect, STATGROUP_Rship);
DECLARE_CYCLE_STAT(TEXT("TakeAction"), STAT_Rship_TakeAction, STATGROUP_Rship);

#if WITH_EDITOR
#include "Editor.h"
#endif

using namespace std;

// ============================================================================
// FRshipDecoderThread Implementation
// ============================================================================

FRshipDecoderThread::FRshipDecoderThread(URshipSubsystem* InSubsystem)
    : Subsystem(InSubsystem)
    , bShouldStop(false)
    , Thread(nullptr)
    , WakeEvent(nullptr)
{
    WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
    Thread = FRunnableThread::Create(this, TEXT("RshipDecoderThread"), 0, TPri_AboveNormal);
    UE_LOG(LogRshipExec, Log, TEXT("FRshipDecoderThread: Started background decoder thread"));
}

FRshipDecoderThread::~FRshipDecoderThread()
{
    Stop();

    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }

    if (WakeEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
        WakeEvent = nullptr;
    }
}

bool FRshipDecoderThread::Init()
{
    return true;
}

uint32 FRshipDecoderThread::Run()
{
    while (!bShouldStop)
    {
        // Wait for data (1ms timeout for responsive shutdown)
        WakeEvent->Wait(1);

        if (bShouldStop)
        {
            break;
        }

        // Process all queued binary data
        TArray<uint8> BinaryData;
        while (InputQueue.Dequeue(BinaryData))
        {
            if (bShouldStop) break;

            FRshipBatchCommand BatchCommand;
            if (FRshipMsgPack::TryDecodeBatchCommand(BinaryData, BatchCommand))
            {
                // Dispatch batch command directly to game thread via AsyncTask
                // This bypasses ticker-based polling for lower latency
                AsyncTask(ENamedThreads::GameThread, [this, Cmd = MoveTemp(BatchCommand)]() mutable
                {
                    if (Subsystem && IsValid(Subsystem))
                    {
                        Subsystem->ProcessBatchActionsFast(Cmd);
                    }
                });
            }
            else
            {
                // Not a batch command - decode as generic JSON and dispatch
                TSharedPtr<FJsonObject> JsonObject;
                if (FRshipMsgPack::Decode(BinaryData, JsonObject) && JsonObject.IsValid())
                {
                    AsyncTask(ENamedThreads::GameThread, [this, Json = JsonObject]()
                    {
                        if (Subsystem && IsValid(Subsystem))
                        {
                            Subsystem->ProcessMessageDirect(Json);
                        }
                    });
                }
            }
        }
    }

    return 0;
}

void FRshipDecoderThread::Stop()
{
    bShouldStop = true;
    Subsystem = nullptr;  // Clear reference to prevent use-after-free
    if (WakeEvent)
    {
        WakeEvent->Trigger();
    }
}

void FRshipDecoderThread::Exit()
{
    UE_LOG(LogRshipExec, Log, TEXT("FRshipDecoderThread: Decoder thread exiting"));
}

void FRshipDecoderThread::QueueBinaryData(TArray<uint8>&& Data)
{
    InputQueue.Enqueue(MoveTemp(Data));
    if (WakeEvent)
    {
        WakeEvent->Trigger();
    }
}

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipSubsystem::Initialize"));

    // Initialize connection state
    ConnectionState = ERshipConnectionState::Disconnected;
    ReconnectAttempts = 0;
    GroupManager = nullptr;
    HealthMonitor = nullptr;
    PresetManager = nullptr;
    TemplateManager = nullptr;
    LevelManager = nullptr;
    EditorSelection = nullptr;
    DataLayerManager = nullptr;
    FixtureManager = nullptr;
    CameraManager = nullptr;
    IESProfileService = nullptr;
    SceneConverter = nullptr;
    EditorTransformSync = nullptr;
    PulseReceiver = nullptr;
    FeedbackReporter = nullptr;
    VisualizationManager = nullptr;
    TimecodeSync = nullptr;
    FixtureLibrary = nullptr;
    MultiCameraManager = nullptr;
    SceneValidator = nullptr;
    NiagaraManager = nullptr;
    SequencerSync = nullptr;
    MaterialManager = nullptr;
    SubstrateMaterialManager = nullptr;
    DMXOutput = nullptr;
    OSCBridge = nullptr;
    LiveLinkService = nullptr;
    AudioManager = nullptr;
    Recorder = nullptr;
    ControlRigManager = nullptr;
    PCGManager = nullptr;
    LastTickTime = 0.0;

    // Initialize rate limiter
    InitializeRateLimiter();

    // Start background decoder thread for msgpack processing
    DecoderThread = MakeUnique<FRshipDecoderThread>(this);

    // Connect to server
    Reconnect();

    auto world = GetWorld();

    if (world != nullptr)
    {
        this->EmitterHandler = GetWorld()->SpawnActor<AEmitterHandler>();
    }

    this->TargetComponents = new TMultiMap<FString, URshipTargetComponent*>;

    // Start queue processing ticker (works in editor without a world)
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (Settings->bEnableRateLimiting)
    {
        QueueProcessTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnQueueProcessTick),
            Settings->QueueProcessInterval
        );
        UE_LOG(LogRshipExec, Log, TEXT("Started queue processing ticker (interval=%.3fs)"), Settings->QueueProcessInterval);
    }

    // Start subsystem tick ticker (1000Hz for high-frequency message pumping)
    SubsystemTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnSubsystemTick),
        0.001f  // 1000Hz tick rate (1ms)
    );
    UE_LOG(LogRshipExec, Log, TEXT("Started subsystem ticker (1000Hz)"));
}

void URshipSubsystem::InitializeRateLimiter()
{
    const URshipSettings *Settings = GetDefault<URshipSettings>();

    RateLimiter = MakeUnique<FRshipRateLimiter>();

    FRshipRateLimiterConfig Config;

    // Token bucket (messages)
    Config.MaxMessagesPerSecond = Settings->MaxMessagesPerSecond;
    Config.MaxBurstSize = Settings->MaxBurstSize;

    // Token bucket (bytes)
    Config.bEnableBytesRateLimiting = Settings->bEnableBytesRateLimiting;
    Config.MaxBytesPerSecond = Settings->MaxBytesPerSecond;
    Config.MaxBurstBytes = Settings->MaxBurstBytes;

    // Queue settings
    Config.MaxQueueLength = Settings->MaxQueueLength;
    Config.MessageTimeoutSeconds = Settings->MessageTimeoutSeconds;
    Config.bEnableCoalescing = Settings->bEnableCoalescing;

    // Batching settings
    Config.bEnableBatching = Settings->bEnableBatching;
    Config.MaxBatchMessages = Settings->MaxBatchMessages;
    Config.MaxBatchBytes = Settings->MaxBatchBytes;
    Config.MaxBatchIntervalMs = Settings->MaxBatchIntervalMs;
    Config.bCriticalBypassBatching = Settings->bCriticalBypassBatching;

    // Downsampling settings
    Config.bEnableDownsampling = Settings->bEnableDownsampling;
    Config.LowPrioritySampleRate = Settings->LowPrioritySampleRate;
    Config.NormalPrioritySampleRate = Settings->NormalPrioritySampleRate;
    Config.QueuePressureThreshold = Settings->QueuePressureThreshold;

    // Adaptive rate control
    Config.bEnableAdaptiveRate = Settings->bEnableAdaptiveRate;
    Config.RateIncreaseFactor = Settings->RateIncreaseFactor;
    Config.RateDecreaseFactor = Settings->RateDecreaseFactor;
    Config.MinRateFraction = Settings->MinRateFraction;
    Config.RateAdjustmentInterval = Settings->RateAdjustmentInterval;

    // Backoff settings
    Config.InitialBackoffSeconds = Settings->InitialBackoffSeconds;
    Config.MaxBackoffSeconds = Settings->MaxBackoffSeconds;
    Config.BackoffMultiplier = Settings->BackoffMultiplier;
    Config.MaxRetryCount = Settings->MaxRetryCount;
    Config.bCriticalBypassBackoff = Settings->bCriticalBypassBackoff;

    // Diagnostics settings
    Config.LogVerbosity = Settings->LogVerbosity;
    Config.bEnableMetrics = Settings->bEnableMetrics;
    Config.MetricsLogInterval = Settings->MetricsLogInterval;
    Config.bLogRateLimitEvents = Settings->bLogRateLimitEvents;
    Config.bLogBatchDetails = Settings->bLogBatchDetails;

    RateLimiter->Initialize(Config);

    // Bind the send callback
    RateLimiter->OnMessageReadyToSend.BindUObject(this, &URshipSubsystem::SendJsonDirect);
    RateLimiter->OnRateLimiterStatus.BindUObject(this, &URshipSubsystem::OnRateLimiterStatusChanged);

    UE_LOG(LogRshipExec, Log, TEXT("Rate limiter initialized: %.1f msg/s, burst=%d, queue=%d, batching=%s, adaptive=%s"),
        Config.MaxMessagesPerSecond, Config.MaxBurstSize, Config.MaxQueueLength,
        Config.bEnableBatching ? TEXT("ON") : TEXT("OFF"),
        Config.bEnableAdaptiveRate ? TEXT("ON") : TEXT("OFF"));
}

void URshipSubsystem::Reconnect()
{
    // Set flag to prevent OnWebSocketClosed from scheduling auto-reconnect
    bIsManuallyReconnecting = true;

    // If we're backing off, cancel the timer and proceed with manual reconnect
    if (ConnectionState == ERshipConnectionState::BackingOff)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Manual reconnect requested during backoff - cancelling scheduled reconnect"));
        if (ReconnectTickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
            ReconnectTickerHandle.Reset();
        }
        ReconnectAttempts = 0;  // Reset attempts on manual reconnect
    }
    // If already connecting, cancel current attempt and start fresh
    else if (ConnectionState == ERshipConnectionState::Connecting)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Manual reconnect requested while connecting - cancelling current attempt"));
        if (ConnectionTimeoutTickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTimeoutTickerHandle);
            ConnectionTimeoutTickerHandle.Reset();
        }
        // Close any pending connections
        if (WebSocket)
        {
            WebSocket->Close();
            WebSocket.Reset();
        }
        ConnectionState = ERshipConnectionState::Disconnected;
        ReconnectAttempts = 0;
    }

    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    // Send Exec
    MachineId = GetUniqueMachineId();
    ServiceId = FApp::GetProjectName();

    ClusterId = MachineId + ":" + ServiceId;
    InstanceId = ClusterId;

    const URshipSettings *Settings = GetDefault<URshipSettings>();
    FString rshipHostAddress = Settings->rshipHostAddress;
    int32 rshipServerPort = Settings->rshipServerPort;

    UE_LOG(LogRshipExec, Log, TEXT("Settings loaded - Address: [%s], Port: [%d]"), *rshipHostAddress, rshipServerPort);

    if (rshipHostAddress.IsEmpty())
    {
        UE_LOG(LogRshipExec, Warning, TEXT("rshipHostAddress is empty, defaulting to localhost"));
        rshipHostAddress = TEXT("localhost");
    }

    // Close existing connection
    if (WebSocket)
    {
        WebSocket->Close();
        WebSocket.Reset();
    }

    ConnectionState = ERshipConnectionState::Connecting;

    // Set connection timeout (10 seconds) - uses ticker which works in editor
    if (ConnectionTimeoutTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTimeoutTickerHandle);
        ConnectionTimeoutTickerHandle.Reset();
    }
    ConnectionTimeoutTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnConnectionTimeoutTick),
        10.0f  // 10 second timeout (one-shot, callback returns false)
    );

    FString WebSocketUrl = "ws://" + rshipHostAddress + ":" + FString::FromInt(rshipServerPort) + "/myko";
    UE_LOG(LogRshipExec, Log, TEXT("Connecting to %s"), *WebSocketUrl);

    // Create high-performance WebSocket with dedicated send thread
    WebSocket = MakeShared<FRshipWebSocket>();

    // Bind event handlers
    WebSocket->OnConnected.BindUObject(this, &URshipSubsystem::OnWebSocketConnected);
    WebSocket->OnConnectionError.BindUObject(this, &URshipSubsystem::OnWebSocketConnectionError);
    WebSocket->OnClosed.BindUObject(this, &URshipSubsystem::OnWebSocketClosed);
    WebSocket->OnMessage.BindUObject(this, &URshipSubsystem::OnWebSocketMessage);
    WebSocket->OnBinaryMessage.BindUObject(this, &URshipSubsystem::OnWebSocketBinaryMessage);

    // Configure and connect
    FRshipWebSocketConfig Config;
    Config.bTcpNoDelay = Settings->bTcpNoDelay;
    Config.bDisableCompression = Settings->bDisableCompression;
    Config.PingIntervalSeconds = Settings->PingIntervalSeconds;
    Config.bAutoReconnect = false;  // We handle reconnection ourselves

    WebSocket->Connect(WebSocketUrl, Config);

    // Clear the manual reconnect flag now that new connection is started
    bIsManuallyReconnecting = false;
}

void URshipSubsystem::ConnectTo(const FString& Host, int32 Port)
{
    // Update settings with new values
    URshipSettings* Settings = GetMutableDefault<URshipSettings>();
    if (Settings)
    {
        Settings->rshipHostAddress = Host;
        Settings->rshipServerPort = Port;
        Settings->SaveConfig();
        Settings->UpdateDefaultConfigFile();  // Also update DefaultGame.ini

        UE_LOG(LogRshipExec, Log, TEXT("Saved server settings to config: %s:%d"), *Host, Port);
    }

    // Force reconnect with new settings
    ReconnectAttempts = 0;
    ConnectionState = ERshipConnectionState::Disconnected;
    Reconnect();
}

FString URshipSubsystem::GetServerAddress() const
{
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    return Settings ? Settings->rshipHostAddress : TEXT("localhost");
}

int32 URshipSubsystem::GetServerPort() const
{
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    return Settings ? Settings->rshipServerPort : 5155;
}

void URshipSubsystem::OnWebSocketConnected()
{
    UE_LOG(LogRshipExec, Log, TEXT("WebSocket connected"));

    ConnectionState = ERshipConnectionState::Connected;
    ReconnectAttempts = 0;

    // Notify rate limiter of successful connection
    if (RateLimiter)
    {
        RateLimiter->OnConnectionSuccess();
    }

    // Clear any pending reconnect ticker and connection timeout
    if (ReconnectTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
        ReconnectTickerHandle.Reset();
    }
    if (ConnectionTimeoutTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTimeoutTickerHandle);
        ConnectionTimeoutTickerHandle.Reset();
    }

    // DIAGNOSTIC: Send a ping immediately to verify WebSocket send path works
    // The server will echo this back as ws:m:ping - if we receive it, send/receive is working
    bPingResponseReceived = false;
    {
        int64 Timestamp = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();

        TSharedPtr<FJsonObject> PingData = MakeShareable(new FJsonObject);
        PingData->SetNumberField(TEXT("timestamp"), (double)Timestamp);

        TSharedPtr<FJsonObject> PingPayload = MakeShareable(new FJsonObject);
        PingPayload->SetStringField(TEXT("event"), TEXT("ws:m:ping"));
        PingPayload->SetObjectField(TEXT("data"), PingData);

        UE_LOG(LogRshipExec, Verbose, TEXT("Sending diagnostic ping"));

        // Send directly to bypass rate limiter for diagnostic
        if (WebSocket.IsValid())
        {
            if (bUseMsgpack)
            {
                // Send ping as msgpack - this also establishes msgpack protocol with server
                TArray<uint8> BinaryData;
                if (FRshipMsgPack::Encode(PingPayload, BinaryData))
                {
                    WebSocket->SendBinary(BinaryData);
                }
            }
            else
            {
                FString PingJson;
                TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&PingJson);
                FJsonSerializer::Serialize(PingPayload.ToSharedRef(), JsonWriter);
                WebSocket->Send(PingJson);
            }
        }
    }

    // Clear entity cache and sync from server
    bEntityCacheSynced = false;
    ServerTargetHashes.Empty();
    ServerActionHashes.Empty();
    ServerEmitterHashes.Empty();
    PendingQueries.Empty();  // Clear any stale queries from previous connection

    // Sync entity cache from server, then send all entities
    // This queries existing entities and skips unchanged ones on reconnect
    SyncEntityCacheFromServer();

    // Ensure queue processing ticker is running (may have failed during early init)
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (Settings->bEnableRateLimiting && !QueueProcessTickerHandle.IsValid())
    {
        UE_LOG(LogRshipExec, Log, TEXT("Starting queue processing ticker (was not running)"));
        QueueProcessTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnQueueProcessTick),
            Settings->QueueProcessInterval
        );
    }
}

void URshipSubsystem::OnWebSocketConnectionError(const FString &Error)
{
    UE_LOG(LogRshipExec, Warning, TEXT("WebSocket connection error: %s"), *Error);

    ConnectionState = ERshipConnectionState::Disconnected;

    // Clear connection timeout
    if (ConnectionTimeoutTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTimeoutTickerHandle);
        ConnectionTimeoutTickerHandle.Reset();
    }

    // Notify rate limiter
    if (RateLimiter)
    {
        RateLimiter->OnConnectionError();
    }

    // Schedule reconnection if enabled
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (Settings->bAutoReconnect)
    {
        ScheduleReconnect();
    }
}

void URshipSubsystem::OnWebSocketClosed(int32 StatusCode, const FString &Reason, bool bWasClean)
{
    UE_LOG(LogRshipExec, Warning, TEXT("WebSocket closed: Code=%d, Reason=%s, Clean=%d"),
        StatusCode, *Reason, bWasClean);

    ConnectionState = ERshipConnectionState::Disconnected;

    // Clear pending queries - subscriptions are invalid after disconnect
    PendingQueries.Empty();
    bEntityCacheSynced = false;

    // Handle rate limit response (HTTP 429 or similar status codes indicating rate limiting)
    if (StatusCode == 429 || StatusCode == 1008)  // 1008 = Policy Violation
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Rate limit detected from server (code %d)"), StatusCode);
        if (RateLimiter)
        {
            RateLimiter->OnRateLimitError();
        }
    }

    // Schedule reconnection if enabled and this wasn't a clean close
    // Skip if we're in the middle of a manual reconnect (user called Reconnect())
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (Settings->bAutoReconnect && !bWasClean && !bIsManuallyReconnecting)
    {
        ScheduleReconnect();
    }
}

void URshipSubsystem::OnWebSocketMessage(const FString &Message)
{
    ProcessMessage(Message);
}

void URshipSubsystem::OnWebSocketBinaryMessage(const TArray<uint8>& Data)
{
    // Queue binary data to background decoder thread
    // The decoder thread will parse msgpack and queue results for game thread processing
    if (DecoderThread)
    {
        // Make a copy for the decoder thread (Data is const ref)
        TArray<uint8> DataCopy = Data;
        DecoderThread->QueueBinaryData(MoveTemp(DataCopy));
    }
    else
    {
        // Fallback: process directly on game thread if decoder thread not available
        FRshipBatchCommand BatchCommand;
        if (FRshipMsgPack::TryDecodeBatchCommand(Data, BatchCommand))
        {
            ProcessBatchActionsFast(BatchCommand);
            return;
        }

        TSharedPtr<FJsonObject> JsonObject;
        if (FRshipMsgPack::Decode(Data, JsonObject) && JsonObject.IsValid())
        {
            ProcessMessageDirect(JsonObject);
        }
    }
}

void URshipSubsystem::ScheduleReconnect()
{
    const URshipSettings *Settings = GetDefault<URshipSettings>();

    // Check max reconnect attempts
    if (Settings->MaxReconnectAttempts > 0 && ReconnectAttempts >= Settings->MaxReconnectAttempts)
    {
        UE_LOG(LogRshipExec, Error, TEXT("Max reconnect attempts (%d) reached, giving up"), Settings->MaxReconnectAttempts);
        ConnectionState = ERshipConnectionState::Disconnected;
        return;
    }

    // Calculate backoff delay
    float BackoffDelay = Settings->InitialBackoffSeconds *
        FMath::Pow(Settings->BackoffMultiplier, static_cast<float>(ReconnectAttempts));
    BackoffDelay = FMath::Min(BackoffDelay, Settings->MaxBackoffSeconds);

    ReconnectAttempts++;
    ConnectionState = ERshipConnectionState::BackingOff;

    UE_LOG(LogRshipExec, Log, TEXT("Scheduling reconnect attempt %d in %.1f seconds"),
        ReconnectAttempts, BackoffDelay);

    // Schedule reconnect using ticker (works in editor without a world)
    if (ReconnectTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
        ReconnectTickerHandle.Reset();
    }
    ReconnectTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnReconnectTick),
        BackoffDelay  // One-shot, callback returns false
    );
}

void URshipSubsystem::AttemptReconnect()
{
    UE_LOG(LogRshipExec, Log, TEXT("Attempting reconnect..."));
    ConnectionState = ERshipConnectionState::Reconnecting;
    Reconnect();
}

void URshipSubsystem::OnConnectionTimeout()
{
    if (ConnectionState != ERshipConnectionState::Connecting)
    {
        // Already transitioned to another state (connected, error, etc.)
        return;
    }

    UE_LOG(LogRshipExec, Warning, TEXT("Connection attempt timed out after 10 seconds"));

    // Close any pending connection
    if (WebSocket)
    {
        WebSocket->Close();
        WebSocket.Reset();
    }

    ConnectionState = ERshipConnectionState::Disconnected;

    // Schedule reconnection if enabled
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (Settings->bAutoReconnect)
    {
        ScheduleReconnect();
    }
}

void URshipSubsystem::OnRateLimiterStatusChanged(bool bIsBackingOff, float BackoffSeconds)
{
    if (bIsBackingOff)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Rate limiter backing off for %.1f seconds"), BackoffSeconds);
    }
    else
    {
        UE_LOG(LogRshipExec, Log, TEXT("Rate limiter backoff ended"));
    }
}

// Ticker callbacks - return true to keep ticking, false to stop
// These check IsValid() to handle hot reload safely

bool URshipSubsystem::OnQueueProcessTick(float DeltaTime)
{
    if (!IsValid(this))
    {
        return false;  // Stop ticking, object is being destroyed
    }
    ProcessMessageQueue();
    return true;  // Keep ticking
}

bool URshipSubsystem::OnReconnectTick(float DeltaTime)
{
    if (!IsValid(this))
    {
        return false;  // Stop ticking, object is being destroyed
    }
    AttemptReconnect();
    ReconnectTickerHandle.Reset();  // Clear handle since this is a one-shot
    return false;  // Stop ticking (one-shot)
}

bool URshipSubsystem::OnSubsystemTick(float DeltaTime)
{
    if (!IsValid(this))
    {
        return false;  // Stop ticking, object is being destroyed
    }

    // High-frequency WebSocket message pump - process all pending messages
    // This triggers OnBinaryMessage which queues to decoder thread
    if (WebSocket.IsValid())
    {
        WebSocket->ProcessPendingMessages();
    }

    // Note: Batch commands are now dispatched directly from decoder thread via AsyncTask
    // No polling needed here

    TickSubsystems();
    return true;  // Keep ticking
}

bool URshipSubsystem::OnConnectionTimeoutTick(float DeltaTime)
{
    if (!IsValid(this))
    {
        return false;  // Stop ticking, object is being destroyed
    }
    OnConnectionTimeout();
    ConnectionTimeoutTickerHandle.Reset();  // Clear handle since this is a one-shot
    return false;  // Stop ticking (one-shot)
}

void URshipSubsystem::ProcessMessageQueue()
{
    if (!RateLimiter)
    {
        return;
    }

    // Use actual WebSocket connection state, not internal enum (they can get out of sync)
    if (!IsConnected())
    {
        int32 QueueSize = RateLimiter->GetQueueLength();
        if (QueueSize > 0)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("ProcessMessageQueue: Not connected (State=%d), %d messages waiting"),
                (int32)ConnectionState, QueueSize);
        }
        return;
    }

    int32 QueueSize = RateLimiter->GetQueueLength();
    if (QueueSize > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("ProcessMessageQueue: Queue has %d messages, processing..."), QueueSize);
    }

    int32 Sent = RateLimiter->ProcessQueue();

    if (Sent > 0 || QueueSize > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("ProcessMessageQueue: Sent %d messages, %d remaining"), Sent, RateLimiter->GetQueueLength());
    }
}

void URshipSubsystem::TickSubsystems()
{
    // Calculate delta time
    double CurrentTime = FPlatformTime::Seconds();
    float DeltaTime = (LastTickTime > 0.0) ? (float)(CurrentTime - LastTickTime) : 0.0f;
    LastTickTime = CurrentTime;

    // Tick timecode sync for playback and cue points
    if (TimecodeSync)
    {
        TimecodeSync->Tick(DeltaTime);
    }

    // Tick multi-camera manager for transitions
    if (MultiCameraManager)
    {
        MultiCameraManager->Tick(DeltaTime);
    }

    // Tick visualization manager for beam updates
    if (VisualizationManager)
    {
        VisualizationManager->Tick(DeltaTime);
    }

    // Tick Niagara manager for parameter updates
    if (NiagaraManager)
    {
        NiagaraManager->Tick(DeltaTime);
    }

    // Tick sequencer sync for timeline integration
    if (SequencerSync)
    {
        SequencerSync->Tick(DeltaTime);
    }

    // Tick material manager for global updates
    if (MaterialManager)
    {
        MaterialManager->Tick(DeltaTime);
    }

    // Tick Substrate material manager for transitions
    if (SubstrateMaterialManager)
    {
        SubstrateMaterialManager->Tick(DeltaTime);
    }

    // Tick DMX output for continuous transmission
    if (DMXOutput)
    {
        DMXOutput->Tick(DeltaTime);
    }

    // Tick OSC bridge for message processing
    if (OSCBridge)
    {
        OSCBridge->Tick(DeltaTime);
    }

    // Tick Live Link service for smoothing
    if (LiveLinkService)
    {
        LiveLinkService->Tick(DeltaTime);
    }

    // Tick Recorder for playback
    if (Recorder)
    {
        Recorder->Tick(DeltaTime);
    }

    // Tick PCG manager for binding lifecycle
    if (PCGManager)
    {
        PCGManager->Tick(DeltaTime);
    }

    // Process message queue every tick to ensure messages are sent
    ProcessMessageQueue();
}

void URshipSubsystem::QueueMessage(TSharedPtr<FJsonObject> Payload, ERshipMessagePriority Priority,
                                    ERshipMessageType Type, const FString& CoalesceKey)
{
    const URshipSettings *Settings = GetDefault<URshipSettings>();

    // If rate limiting is disabled, send directly
    if (!Settings->bEnableRateLimiting || !RateLimiter)
    {
        FString JsonString;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
        if (FJsonSerializer::Serialize(Payload.ToSharedRef(), JsonWriter))
        {
            SendJsonDirect(JsonString);
        }
        return;
    }

    // Queue through rate limiter
    if (!RateLimiter->EnqueueMessage(Payload, Priority, Type, CoalesceKey))
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Failed to enqueue message (queue full)"));
    }
    else
    {
        UE_LOG(LogRshipExec, Log, TEXT("Enqueued message (Key=%s, QueueLen=%d)"), *CoalesceKey, RateLimiter->GetQueueLength());
    }

    // If the queue processing ticker isn't running, immediately process the queue
    // Use IsConnected() to check actual WebSocket state
    if (!QueueProcessTickerHandle.IsValid() && IsConnected())
    {
        ProcessMessageQueue();
    }
}

void URshipSubsystem::SendJsonDirect(const FString& JsonString)
{
    bool bConnected = WebSocket.IsValid() && WebSocket->IsConnected();

    if (!bConnected)
    {
        // Don't spam reconnect attempts - let the scheduled reconnect handle it
        if (ConnectionState == ERshipConnectionState::Disconnected)
        {
            const URshipSettings *Settings = GetDefault<URshipSettings>();
            if (Settings->bAutoReconnect && !ReconnectTickerHandle.IsValid())
            {
                ScheduleReconnect();
            }
        }
        return;
    }

    UE_LOG(LogRshipExec, Verbose, TEXT("Sending: %s"), *JsonString);

    if (bUseMsgpack)
    {
        // Parse JSON string and encode as msgpack binary
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            TArray<uint8> BinaryData;
            if (FRshipMsgPack::Encode(JsonObject, BinaryData))
            {
                WebSocket->SendBinary(BinaryData);
                return;
            }
        }

        // Fallback to JSON if msgpack encoding failed
        UE_LOG(LogRshipExec, Warning, TEXT("Msgpack encoding failed, falling back to JSON"));
    }

    WebSocket->Send(JsonString);
}

void URshipSubsystem::ProcessMessage(const FString &message)
{
    // Parse JSON string and delegate to ProcessMessageDirect
    TSharedPtr<FJsonObject> obj = MakeShareable(new FJsonObject);
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(message);

    if (!(FJsonSerializer::Deserialize(Reader, obj) && obj.IsValid()))
    {
        return;
    }

    ProcessMessageDirect(obj);
}

void URshipSubsystem::ProcessMessageDirect(TSharedPtr<FJsonObject> obj)
{
    SCOPE_CYCLE_COUNTER(STAT_Rship_ProcessMessageDirect);

    if (!obj.IsValid())
    {
        return;
    }

    TSharedRef<FJsonObject> objRef = obj.ToSharedRef();

    FString type = objRef->GetStringField(TEXT("event"));
    UE_LOG(LogRshipExec, Verbose, TEXT("Received message: event=%s"), *type);  // Changed to Verbose for performance

    // Handle ping response - diagnostic for verifying WebSocket send/receive path
    if (type == "ws:m:ping")
    {
        TSharedPtr<FJsonObject> data = obj->GetObjectField(TEXT("data"));
        if (data.IsValid())
        {
            int64 SentTimestamp = (int64)data->GetNumberField(TEXT("timestamp"));
            int64 NowTimestamp = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
            int64 RoundTripMs = NowTimestamp - SentTimestamp;
            UE_LOG(LogRshipExec, Log, TEXT("*** PING RESPONSE RECEIVED *** Round-trip: %lldms - WebSocket send/receive verified!"), RoundTripMs);
            bPingResponseReceived = true;
        }
        return;
    }

    if (type == "ws:m:command")
    {
        TSharedRef<FJsonObject> data = obj->GetObjectField(TEXT("data")).ToSharedRef();

        FString commandId = data->GetStringField(TEXT("commandId"));
        TSharedRef<FJsonObject> command = data->GetObjectField(TEXT("command")).ToSharedRef();

        FString txId = command->GetStringField(TEXT("tx"));

        if (commandId == "SetClientId")
        {
            ClientId = command->GetStringField(TEXT("clientId"));
            UE_LOG(LogRshipExec, Log, TEXT("Received ClientId %s"), *ClientId);
            // Cache sync already triggered by OnWebSocketConnected
            return;
        }

        if (commandId == "ExecTargetAction")
        {
            TSharedRef<FJsonObject> execAction = command->GetObjectField(TEXT("action")).ToSharedRef();
            TSharedRef<FJsonObject> execData = command->GetObjectField(TEXT("data")).ToSharedRef();

            FString actionId = execAction->GetStringField(TEXT("id"));
            FString targetId = execAction->GetStringField(TEXT("targetId"));

            bool result = false;

            // Check if this is a PCG target (paths start with "/pcg/")
            if (targetId.StartsWith(TEXT("/pcg/")))
            {
                if (PCGManager)
                {
                    result = PCGManager->RouteAction(targetId, actionId, execData);
                }
                else
                {
                    UE_LOG(LogRshipExec, Warning, TEXT("PCG target action received but PCGManager not initialized: %s"), *targetId);
                }
            }
            else
            {
                // Standard target component routing - get ALL components with this target ID
                TArray<URshipTargetComponent*> comps = FindAllTargetComponents(targetId);
                if (comps.Num() > 0)
                {
                    for (URshipTargetComponent* comp : comps)
                    {
                        if (!comp) continue;

                        Target* target = comp->TargetData;
                        AActor* owner = comp->GetOwner();

                        // Determine world type for logging
                        FString WorldTypeStr = TEXT("Unknown");
                        if (owner)
                        {
                            if (UWorld* World = owner->GetWorld())
                            {
                                switch (World->WorldType)
                                {
                                    case EWorldType::Editor: WorldTypeStr = TEXT("Editor"); break;
                                    case EWorldType::PIE:
#if WITH_EDITOR
                                        WorldTypeStr = (GEditor && GEditor->bIsSimulatingInEditor) ? TEXT("Simulate") : TEXT("PIE");
#else
                                        WorldTypeStr = TEXT("PIE");
#endif
                                        break;
                                    case EWorldType::Game: WorldTypeStr = TEXT("Game"); break;
                                    case EWorldType::EditorPreview: WorldTypeStr = TEXT("EditorPreview"); break;
                                    default: WorldTypeStr = TEXT("Other"); break;
                                }
                            }
                        }

                        if (target != nullptr)
                        {
                            // Skip action execution in Editor world - only run in PIE/Simulate/Game
                            if (owner)
                            {
                                if (UWorld* World = owner->GetWorld())
                                {
                                    if (World->WorldType == EWorldType::Editor)
                                    {
                                        UE_LOG(LogRshipExec, Verbose, TEXT("Skipping action [%s] on target [%s] (Editor)"), *actionId, *targetId);
                                        continue;
                                    }
                                }
                            }

                            UE_LOG(LogRshipExec, Log, TEXT("Executing action [%s] on target [%s] (%s)"), *actionId, *targetId, *WorldTypeStr);
                            bool takeResult = target->TakeAction(owner, actionId, execData);
                            result |= takeResult;
                            comp->OnDataReceived();
                        }
                        else
                        {
                            UE_LOG(LogRshipExec, Warning, TEXT("Target data null for: %s (%s)"), *targetId, *WorldTypeStr);
                        }
                    }
                }
                else
                {
                    UE_LOG(LogRshipExec, Warning, TEXT("Target not found: %s"), *targetId);
                }
            }

            TSharedPtr<FJsonObject> responseData = MakeShareable(new FJsonObject);
            responseData->SetStringField(TEXT("commandId"), commandId);
            responseData->SetStringField(TEXT("tx"), txId);

            if (result)
            {
                // Send success response - CRITICAL priority
                TSharedPtr<FJsonObject> response = MakeShareable(new FJsonObject);
                response->SetStringField(TEXT("event"), TEXT("ws:m:command-response"));
                response->SetObjectField(TEXT("data"), responseData);

                QueueMessage(response, ERshipMessagePriority::Critical, ERshipMessageType::CommandResponse);
            }
            else
            {
                // Send failure response - CRITICAL priority
                UE_LOG(LogRshipExec, Warning, TEXT("Action not taken: %s on Target %s"), *actionId, *targetId);
                TSharedPtr<FJsonObject> response = MakeShareable(new FJsonObject);
                response->SetStringField(TEXT("event"), TEXT("ws:m:command-error"));
                response->SetObjectField(TEXT("data"), responseData);

                QueueMessage(response, ERshipMessagePriority::Critical, ERshipMessageType::CommandResponse);
            }
        }
        else if (commandId == "BatchExecTargetActions")
        {
            // Batch action command - use optimized processing
            const TArray<TSharedPtr<FJsonValue>>* ActionsArray;
            if (command->TryGetArrayField(TEXT("actions"), ActionsArray))
            {
                ProcessBatchActions(*ActionsArray, txId, commandId);
            }
        }
        obj.Reset();
    }
    else if (type == MQUERY_RESPONSE_EVENT)
    {
        // Query response - route to callback
        ProcessQueryResponse(obj->GetObjectField(TEXT("data")));
    }
}

void URshipSubsystem::ProcessBatchActions(const TArray<TSharedPtr<FJsonValue>>& ActionsArray, const FString& TxId, const FString& CommandId)
{
    SCOPE_CYCLE_COUNTER(STAT_Rship_ProcessBatchActions);

    const int32 TotalCount = ActionsArray.Num();
    if (TotalCount == 0)
    {
        return;
    }

    const double StartTime = FPlatformTime::Seconds();

    // ========================================================================
    // PHASE 1: Pre-cache target lookups for all unique targets in this batch
    // This avoids repeated TMultiMap lookups (O(log n) each) by doing one pass
    // ========================================================================

    // Cache: TargetId -> Component (for single-component targets, which is the common case)
    // Also track which components are in valid (non-Editor) worlds
    struct FCachedTarget
    {
        URshipTargetComponent* Component = nullptr;
        Target* TargetData = nullptr;
        AActor* Owner = nullptr;
        bool bIsValidWorld = false;
    };
    TMap<FString, FCachedTarget> TargetCache;
    TargetCache.Reserve(TotalCount);  // Reserve for worst case (all unique targets)

    double Phase1Start = FPlatformTime::Seconds();
    {
        SCOPE_CYCLE_COUNTER(STAT_Rship_BatchCacheBuild);

        // First pass: collect unique target IDs and pre-cache lookups
        for (const TSharedPtr<FJsonValue>& ActionValue : ActionsArray)
        {
            TSharedPtr<FJsonObject> ActionItem = ActionValue->AsObject();
            if (!ActionItem.IsValid()) continue;

            TSharedPtr<FJsonObject> ActionObj = ActionItem->GetObjectField(TEXT("action"));
            if (!ActionObj.IsValid()) continue;

            FString TargetId = ActionObj->GetStringField(TEXT("targetId"));

            // Skip if already cached or PCG target
            if (TargetCache.Contains(TargetId) || TargetId.StartsWith(TEXT("/pcg/")))
            {
                continue;
            }

            // Look up the target component once
            URshipTargetComponent* Comp = FindTargetComponent(TargetId);
            if (Comp)
            {
                FCachedTarget Cached;
                Cached.Component = Comp;
                Cached.TargetData = Comp->TargetData;
                Cached.Owner = Comp->GetOwner();

                // Pre-check world type (avoid checking in tight loop)
                if (Cached.Owner)
                {
                    if (UWorld* World = Cached.Owner->GetWorld())
                    {
                        Cached.bIsValidWorld = (World->WorldType != EWorldType::Editor);
                    }
                }

                TargetCache.Add(TargetId, Cached);
            }
            else
            {
                // Add null entry to avoid repeated failed lookups
                TargetCache.Add(TargetId, FCachedTarget());
            }
        }
    }
    double Phase1Time = (FPlatformTime::Seconds() - Phase1Start) * 1000.0;

    // ========================================================================
    // PHASE 2: Execute actions using cached lookups
    // ========================================================================

    int32 SuccessCount = 0;
    double TakeActionTotalMs = 0.0;

    double Phase2Start = FPlatformTime::Seconds();
    {
        SCOPE_CYCLE_COUNTER(STAT_Rship_BatchExecute);

        // Check if all actions use the same actionId (common case for batch updates)
        // This allows us to skip repeated string parsing
        FString CommonActionId;
        bool bSameActionId = true;
        if (TotalCount > 1)
        {
            TSharedPtr<FJsonObject> FirstItem = ActionsArray[0]->AsObject();
            if (FirstItem.IsValid())
            {
                TSharedPtr<FJsonObject> FirstAction = FirstItem->GetObjectField(TEXT("action"));
                if (FirstAction.IsValid())
                {
                    CommonActionId = FirstAction->GetStringField(TEXT("id"));
                }
            }

            // Quick check: sample a few items to see if actionId is consistent
            for (int32 i = 1; i < FMath::Min(5, TotalCount) && bSameActionId; i++)
            {
                TSharedPtr<FJsonObject> Item = ActionsArray[i]->AsObject();
                if (Item.IsValid())
                {
                    TSharedPtr<FJsonObject> ActionObj = Item->GetObjectField(TEXT("action"));
                    if (ActionObj.IsValid() && ActionObj->GetStringField(TEXT("id")) != CommonActionId)
                    {
                        bSameActionId = false;
                    }
                }
            }
        }

        for (const TSharedPtr<FJsonValue>& ActionValue : ActionsArray)
        {
            TSharedPtr<FJsonObject> ActionItem = ActionValue->AsObject();
            if (!ActionItem.IsValid()) continue;

            TSharedPtr<FJsonObject> ActionObj = ActionItem->GetObjectField(TEXT("action"));
            TSharedPtr<FJsonObject> ActionData = ActionItem->GetObjectField(TEXT("data"));

            if (!ActionObj.IsValid()) continue;

            // Use pre-extracted actionId if all actions are the same (skip string parsing)
            const FString& ActionId = bSameActionId ? CommonActionId : ActionObj->GetStringField(TEXT("id"));
            const FString& TargetId = ActionObj->GetStringField(TEXT("targetId"));

            bool ActionResult = false;

            // Handle PCG targets separately (they have their own routing)
            if (TargetId.StartsWith(TEXT("/pcg/")))
            {
                if (PCGManager && ActionData.IsValid())
                {
                    ActionResult = PCGManager->RouteAction(TargetId, ActionId, ActionData);
                }
            }
            else
            {
                // Use cached lookup - O(1) instead of O(log n)
                FCachedTarget* Cached = TargetCache.Find(TargetId);
                if (Cached && Cached->Component && Cached->TargetData && Cached->bIsValidWorld && ActionData.IsValid())
                {
                    double ActionStart = FPlatformTime::Seconds();
                    {
                        SCOPE_CYCLE_COUNTER(STAT_Rship_TakeAction);
                        ActionResult = Cached->TargetData->TakeAction(Cached->Owner, ActionId, ActionData);
                    }
                    TakeActionTotalMs += (FPlatformTime::Seconds() - ActionStart) * 1000.0;

                    if (ActionResult)
                    {
                        Cached->Component->OnDataReceived();
                    }
                }
            }

            if (ActionResult)
            {
                SuccessCount++;
            }
        }
    }
    double Phase2Time = (FPlatformTime::Seconds() - Phase2Start) * 1000.0;
    double TotalTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;

    // Log timing breakdown for performance analysis
    UE_LOG(LogRshipExec, Log, TEXT("BatchActions: %d actions, %d targets | Cache=%.2fms Execute=%.2fms (TakeAction=%.2fms) Total=%.2fms"),
        TotalCount, TargetCache.Num(), Phase1Time, Phase2Time, TakeActionTotalMs, TotalTime);

    // Send single response for the batch
    TSharedPtr<FJsonObject> ResponseData = MakeShareable(new FJsonObject);
    ResponseData->SetStringField(TEXT("commandId"), CommandId);
    ResponseData->SetStringField(TEXT("tx"), TxId);
    ResponseData->SetNumberField(TEXT("successCount"), SuccessCount);
    ResponseData->SetNumberField(TEXT("totalCount"), TotalCount);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("event"), TEXT("ws:m:command-response"));
    Response->SetObjectField(TEXT("data"), ResponseData);

    QueueMessage(Response, ERshipMessagePriority::Critical, ERshipMessageType::CommandResponse);
}

void URshipSubsystem::ProcessBatchActionsFast(const FRshipBatchCommand& BatchCommand)
{
    SCOPE_CYCLE_COUNTER(STAT_Rship_ProcessBatchActions);

    const int32 TotalCount = BatchCommand.Actions.Num();
    if (TotalCount == 0)
    {
        return;
    }

    const double StartTime = FPlatformTime::Seconds();

    // Track time since last batch to identify delays
    static double LastBatchTime = 0.0;
    double GapTime = (LastBatchTime > 0.0) ? (StartTime - LastBatchTime) * 1000.0 : 0.0;
    LastBatchTime = StartTime;

    // ========================================================================
    // PHASE 1: Pre-cache target lookups with PCG flag
    // ========================================================================

    struct FCachedTarget
    {
        URshipTargetComponent* Component = nullptr;
        Target* TargetData = nullptr;
        AActor* Owner = nullptr;
        bool bIsValidWorld = false;
        bool bIsPCG = false;  // Cache the PCG check to avoid per-action StartsWith
    };
    TMap<FString, FCachedTarget> TargetCache;
    TargetCache.Reserve(TotalCount);

    // Collect components for batched OnDataReceived calls
    TSet<URshipTargetComponent*> ComponentsToNotify;
    ComponentsToNotify.Reserve(TotalCount);

    double Phase1Start = FPlatformTime::Seconds();
    {
        SCOPE_CYCLE_COUNTER(STAT_Rship_BatchCacheBuild);

        for (const FRshipBatchActionItem& Item : BatchCommand.Actions)
        {
            if (TargetCache.Contains(Item.TargetId))
            {
                continue;
            }

            // Check PCG prefix once per unique target
            const bool bIsPCG = Item.TargetId.StartsWith(TEXT("/pcg/"));

            if (bIsPCG)
            {
                // For PCG targets, just mark the flag
                FCachedTarget Cached;
                Cached.bIsPCG = true;
                TargetCache.Add(Item.TargetId, Cached);
            }
            else
            {
                URshipTargetComponent* Comp = FindTargetComponent(Item.TargetId);
                FCachedTarget Cached;
                Cached.bIsPCG = false;

                if (Comp)
                {
                    Cached.Component = Comp;
                    Cached.TargetData = Comp->TargetData;
                    Cached.Owner = Comp->GetOwner();

                    if (Cached.Owner)
                    {
                        if (UWorld* World = Cached.Owner->GetWorld())
                        {
                            Cached.bIsValidWorld = (World->WorldType != EWorldType::Editor);
                        }
                    }
                }

                TargetCache.Add(Item.TargetId, Cached);
            }
        }
    }
    double Phase1Time = (FPlatformTime::Seconds() - Phase1Start) * 1000.0;

    // ========================================================================
    // PHASE 2: Execute actions - FAST PATH
    // ========================================================================

    int32 SuccessCount = 0;
    const FRshipBatchActionItem* ActionsPtr = BatchCommand.Actions.GetData();
    const int32 NumActions = BatchCommand.Actions.Num();

    double Phase2Start = FPlatformTime::Seconds();
    {
        SCOPE_CYCLE_COUNTER(STAT_Rship_BatchExecute);

        for (int32 i = 0; i < NumActions; ++i)
        {
            const FRshipBatchActionItem& Item = ActionsPtr[i];

            if (!Item.Data.IsValid())
            {
                continue;
            }

            FCachedTarget* Cached = TargetCache.Find(Item.TargetId);
            if (!Cached)
            {
                continue;
            }

            bool ActionResult = false;

            if (Cached->bIsPCG)
            {
                if (PCGManager)
                {
                    ActionResult = PCGManager->RouteAction(Item.TargetId, Item.ActionId, Item.Data);
                }
            }
            else if (Cached->Component && Cached->TargetData && Cached->bIsValidWorld)
            {
                ActionResult = Cached->TargetData->TakeAction(Cached->Owner, Item.ActionId, Item.Data);

                if (ActionResult)
                {
                    // Defer notification - add to set (automatically dedupes)
                    ComponentsToNotify.Add(Cached->Component);
                }
            }

            if (ActionResult)
            {
                SuccessCount++;
            }
        }
    }
    double Phase2Time = (FPlatformTime::Seconds() - Phase2Start) * 1000.0;

    // ========================================================================
    // PHASE 3: Batch notify components (once per component, not per action)
    // ========================================================================
    double Phase3Start = FPlatformTime::Seconds();
    for (URshipTargetComponent* Comp : ComponentsToNotify)
    {
        Comp->OnDataReceived();
    }
    double Phase3Time = (FPlatformTime::Seconds() - Phase3Start) * 1000.0;

    double TotalTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;

    // Log timing breakdown (Gap = time since last batch arrived)
    UE_LOG(LogRshipExec, Log, TEXT("BatchActionsFAST: %d actions | Gap=%.1fms Process=%.2fms (Cache=%.2f Exec=%.2f Notify=%.2f)"),
        TotalCount, GapTime, TotalTime, Phase1Time, Phase2Time, Phase3Time);

    // Send response
    TSharedPtr<FJsonObject> ResponseData = MakeShareable(new FJsonObject);
    ResponseData->SetStringField(TEXT("commandId"), BatchCommand.CommandId);
    ResponseData->SetStringField(TEXT("tx"), BatchCommand.TxId);
    ResponseData->SetNumberField(TEXT("successCount"), SuccessCount);
    ResponseData->SetNumberField(TEXT("totalCount"), TotalCount);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("event"), TEXT("ws:m:command-response"));
    Response->SetObjectField(TEXT("data"), ResponseData);

    QueueMessage(Response, ERshipMessagePriority::Critical, ERshipMessageType::CommandResponse);
}

void URshipSubsystem::Deinitialize()
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipSubsystem::Deinitialize"));

    // Remove tickers
    if (QueueProcessTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(QueueProcessTickerHandle);
        QueueProcessTickerHandle.Reset();
    }
    if (ReconnectTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
        ReconnectTickerHandle.Reset();
    }
    if (SubsystemTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(SubsystemTickerHandle);
        SubsystemTickerHandle.Reset();
    }
    if (ConnectionTimeoutTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTimeoutTickerHandle);
        ConnectionTimeoutTickerHandle.Reset();
    }

    // Stop decoder thread
    if (DecoderThread)
    {
        DecoderThread.Reset();
    }

    // Shutdown health monitor
    if (HealthMonitor)
    {
        HealthMonitor->Shutdown();
        HealthMonitor = nullptr;
    }

    // Shutdown preset manager
    if (PresetManager)
    {
        PresetManager->Shutdown();
        PresetManager = nullptr;
    }

    // Shutdown template manager
    if (TemplateManager)
    {
        TemplateManager->Shutdown();
        TemplateManager = nullptr;
    }

    // Shutdown level manager
    if (LevelManager)
    {
        LevelManager->Shutdown();
        LevelManager = nullptr;
    }

    // Shutdown editor selection
    if (EditorSelection)
    {
        EditorSelection->Shutdown();
        EditorSelection = nullptr;
    }

    // Shutdown Data Layer manager
    if (DataLayerManager)
    {
        DataLayerManager->Shutdown();
        DataLayerManager = nullptr;
    }

    // Shutdown Fixture manager
    if (FixtureManager)
    {
        FixtureManager->Shutdown();
        FixtureManager = nullptr;
    }

    // Shutdown Camera manager
    if (CameraManager)
    {
        CameraManager->Shutdown();
        CameraManager = nullptr;
    }

    // Shutdown IES profile service
    if (IESProfileService)
    {
        IESProfileService->Shutdown();
        IESProfileService = nullptr;
    }

    // Shutdown Scene converter
    if (SceneConverter)
    {
        SceneConverter->Shutdown();
        SceneConverter = nullptr;
    }

    // Shutdown Editor transform sync
    if (EditorTransformSync)
    {
        EditorTransformSync->Shutdown();
        EditorTransformSync = nullptr;
    }

    // Shutdown Pulse receiver
    if (PulseReceiver)
    {
        PulseReceiver->Shutdown();
        PulseReceiver = nullptr;
    }

    // Shutdown Feedback reporter
    if (FeedbackReporter)
    {
        FeedbackReporter->Shutdown();
        FeedbackReporter = nullptr;
    }

    // Shutdown Visualization manager
    if (VisualizationManager)
    {
        VisualizationManager->Shutdown();
        VisualizationManager = nullptr;
    }

    // Shutdown Timecode sync
    if (TimecodeSync)
    {
        TimecodeSync->Shutdown();
        TimecodeSync = nullptr;
    }

    // Shutdown Fixture library
    if (FixtureLibrary)
    {
        FixtureLibrary->Shutdown();
        FixtureLibrary = nullptr;
    }

    // Shutdown Multi-camera manager
    if (MultiCameraManager)
    {
        MultiCameraManager->Shutdown();
        MultiCameraManager = nullptr;
    }

    // Shutdown Scene validator
    if (SceneValidator)
    {
        SceneValidator->Shutdown();
        SceneValidator = nullptr;
    }

    // Shutdown Niagara manager
    if (NiagaraManager)
    {
        NiagaraManager->Shutdown();
        NiagaraManager = nullptr;
    }

    // Shutdown Sequencer sync
    if (SequencerSync)
    {
        SequencerSync->Shutdown();
        SequencerSync = nullptr;
    }

    // Shutdown Material manager
    if (MaterialManager)
    {
        MaterialManager->Shutdown();
        MaterialManager = nullptr;
    }

    // Shutdown Substrate Material manager
    if (SubstrateMaterialManager)
    {
        SubstrateMaterialManager->Shutdown();
        SubstrateMaterialManager = nullptr;
    }

    // Shutdown DMX output
    if (DMXOutput)
    {
        DMXOutput->Shutdown();
        DMXOutput = nullptr;
    }

    // Shutdown OSC bridge
    if (OSCBridge)
    {
        OSCBridge->Shutdown();
        OSCBridge = nullptr;
    }

    // Shutdown Live Link service
    if (LiveLinkService)
    {
        LiveLinkService->Shutdown();
        LiveLinkService = nullptr;
    }

    // Shutdown Audio manager
    if (AudioManager)
    {
        AudioManager->Shutdown();
        AudioManager = nullptr;
    }

    // Shutdown Recorder
    if (Recorder)
    {
        Recorder->Shutdown();
        Recorder = nullptr;
    }

    // Shutdown Control Rig manager
    if (ControlRigManager)
    {
        ControlRigManager->Shutdown();
        ControlRigManager = nullptr;
    }

    // Shutdown PCG manager
    if (PCGManager)
    {
        PCGManager->Shutdown();
        PCGManager = nullptr;
    }

    // Clear rate limiter
    if (RateLimiter)
    {
        RateLimiter->ClearQueue();
        RateLimiter.Reset();
    }

    // Close WebSocket
    if (WebSocket)
    {
        WebSocket->Close();
        WebSocket.Reset();
    }

    Super::Deinitialize();
}

void URshipSubsystem::BeginDestroy()
{
    UE_LOG(LogRshipExec, Log, TEXT("BeginDestroy called - cleaning up tickers and connections"));

    // Remove all tickers before destruction (critical for live coding re-instancing)
    if (QueueProcessTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(QueueProcessTickerHandle);
        QueueProcessTickerHandle.Reset();
    }

    if (ReconnectTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
        ReconnectTickerHandle.Reset();
    }

    if (SubsystemTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(SubsystemTickerHandle);
        SubsystemTickerHandle.Reset();
    }

    if (ConnectionTimeoutTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTimeoutTickerHandle);
        ConnectionTimeoutTickerHandle.Reset();
    }

    // Clean up WebSocket connection without callbacks (object is being destroyed)
    if (WebSocket.IsValid())
    {
        // Don't call Close() as it may trigger callbacks - just reset
        WebSocket.Reset();
    }

    Super::BeginDestroy();
}

void URshipSubsystem::PrepareForHotReload()
{
    UE_LOG(LogRshipExec, Log, TEXT("PrepareForHotReload - cleaning up tickers and connections before module reload"));

    // Remove all tickers - these hold function pointers that will become invalid after hot reload
    if (QueueProcessTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(QueueProcessTickerHandle);
        QueueProcessTickerHandle.Reset();
    }

    if (ReconnectTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
        ReconnectTickerHandle.Reset();
    }

    if (SubsystemTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(SubsystemTickerHandle);
        SubsystemTickerHandle.Reset();
    }

    if (ConnectionTimeoutTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTimeoutTickerHandle);
        ConnectionTimeoutTickerHandle.Reset();
    }

    // Close WebSocket - its callbacks also hold function pointers
    if (WebSocket.IsValid())
    {
        WebSocket->Close();
        WebSocket.Reset();
    }

    // Clear rate limiter callback
    if (RateLimiter)
    {
        RateLimiter->OnMessageReadyToSend.Unbind();
    }

    ConnectionState = ERshipConnectionState::Disconnected;

    UE_LOG(LogRshipExec, Log, TEXT("PrepareForHotReload complete - subsystem will reinitialize after module reload"));
}

void URshipSubsystem::ReinitializeAfterHotReload()
{
    UE_LOG(LogRshipExec, Log, TEXT("ReinitializeAfterHotReload - setting up tickers and reconnecting"));

    const URshipSettings* Settings = GetDefault<URshipSettings>();

    // Restart queue processing ticker
    if (Settings->bEnableRateLimiting && !QueueProcessTickerHandle.IsValid())
    {
        QueueProcessTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnQueueProcessTick),
            Settings->QueueProcessInterval
        );
        UE_LOG(LogRshipExec, Log, TEXT("Restarted queue processing ticker"));
    }

    // Restart subsystem tick ticker
    if (!SubsystemTickerHandle.IsValid())
    {
        SubsystemTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnSubsystemTick),
            0.001f  // 1000Hz
        );
        UE_LOG(LogRshipExec, Log, TEXT("Restarted subsystem ticker (1000Hz)"));
    }

    // Rebind rate limiter callback
    if (RateLimiter)
    {
        RateLimiter->OnMessageReadyToSend.BindUObject(this, &URshipSubsystem::SendJsonDirect);
    }

    // Reconnect to server
    Reconnect();

    UE_LOG(LogRshipExec, Log, TEXT("ReinitializeAfterHotReload complete"));
}

void URshipSubsystem::SendTarget(Target *target)
{
    // Buffer entities until cache is synced - SendAll() will send them after sync
    if (!bEntityCacheSynced)
    {
        UE_LOG(LogRshipExec, Log, TEXT("SendTarget: %s - BUFFERED (cache not yet synced, will send after sync)"), *target->GetId());
        return;
    }

    UE_LOG(LogRshipExec, Verbose, TEXT("SendTarget: %s - %d actions, %d emitters"),
        *target->GetId(),
        target->GetActions().Num(),
        target->GetEmitters().Num());

    TArray<TSharedPtr<FJsonValue>> EmitterIdsJson;
    TArray<TSharedPtr<FJsonValue>> ActionIdsJson;

    for (auto &Elem : target->GetActions())
    {
        ActionIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));
        SendAction(Elem.Value, target->GetId());
    }

    for (auto &Elem : target->GetEmitters())
    {
        EmitterIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));
        SendEmitter(Elem.Value, target->GetId());
    }

    const URshipSettings *Settings = GetDefault<URshipSettings>();

    FColor SRGBColor = Settings->ServiceColor.ToFColor(true);
    FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);
    TSharedPtr<FJsonObject> Target = MakeShareable(new FJsonObject);
    Target->SetStringField(TEXT("id"), target->GetId());

    Target->SetArrayField(TEXT("actionIds"), ActionIdsJson);
    Target->SetArrayField(TEXT("emitterIds"), EmitterIdsJson);
    Target->SetStringField(TEXT("fgColor"), ColorHex);
    Target->SetStringField(TEXT("bgColor"), ColorHex);
    Target->SetStringField(TEXT("name"), target->GetId());
    Target->SetStringField(TEXT("serviceId"), ServiceId);

    // Add tags and groups from the target component - O(1) lookup
    URshipTargetComponent* TargetComp = FindTargetComponent(target->GetId());

    if (TargetComp)
    {
        // Add category (myko protocol field for target organization) - REQUIRED
        Target->SetStringField(TEXT("category"), TargetComp->Category.IsEmpty() ? TEXT("default") : *TargetComp->Category);

        // Add tags array
        TArray<TSharedPtr<FJsonValue>> TagsJson;
        for (const FString& Tag : TargetComp->Tags)
        {
            TagsJson.Add(MakeShareable(new FJsonValueString(Tag)));
        }
        Target->SetArrayField(TEXT("tags"), TagsJson);

        // Add group IDs array
        TArray<TSharedPtr<FJsonValue>> GroupIdsJson;
        for (const FString& GroupId : TargetComp->GroupIds)
        {
            GroupIdsJson.Add(MakeShareable(new FJsonValueString(GroupId)));
        }
        Target->SetArrayField(TEXT("groupIds"), GroupIdsJson);
    }
    else
    {
        // No component, set default category - REQUIRED field
        Target->SetStringField(TEXT("category"), TEXT("default"));
    }

    // rootLevel is REQUIRED - all Unreal targets are root level (sub-targets not yet supported)
    Target->SetBoolField(TEXT("rootLevel"), true);

    // Compute deterministic hash for change detection (before adding hash field)
    FString TargetHash = ComputeEntityHash(Target);

    // Check if target needs to be sent (new or changed)
    if (!NeedsTargetUpdate(target->GetId(), TargetHash))
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("  Target %s unchanged, skipping"), *target->GetId());
    }
    else
    {
        // Set hash field for myko protocol (use the computed hash)
        Target->SetStringField(TEXT("hash"), TargetHash);

        // Target registration - HIGH priority, coalesce by target ID
        SetItem("Target", Target, ERshipMessagePriority::High, target->GetId());

        // NOTE: Cache is updated by server via live query subscription, not here
    }

    TSharedPtr<FJsonObject> TargetStatus = MakeShareable(new FJsonObject);

    TargetStatus->SetStringField(TEXT("targetId"), target->GetId());
    TargetStatus->SetStringField(TEXT("instanceId"), InstanceId);
    TargetStatus->SetStringField(TEXT("status"), TEXT("online"));
    // TargetStatus ID should match Target ID (per TS SDK: serviceId:short_id)
    TargetStatus->SetStringField(TEXT("id"), target->GetId());
    // Hash for optimistic concurrency control (myko protocol requirement)
    TargetStatus->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    SetItem("TargetStatus", TargetStatus, ERshipMessagePriority::High, target->GetId() + ":status");
}

void URshipSubsystem::DeleteTarget(Target* target)
{
    if (!target)
    {
        return;
    }

    UE_LOG(LogRshipExec, Log, TEXT("DeleteTarget: %s - setting target offline (not sending DEL commands)"),
        *target->GetId());

    // Only send TargetStatus offline - server manages target lifecycle
    // We do NOT send DEL events for actions, emitters, or target
    TSharedPtr<FJsonObject> TargetStatus = MakeShareable(new FJsonObject);
    TargetStatus->SetStringField(TEXT("targetId"), target->GetId());
    TargetStatus->SetStringField(TEXT("instanceId"), InstanceId);
    TargetStatus->SetStringField(TEXT("status"), TEXT("offline"));
    TargetStatus->SetStringField(TEXT("id"), target->GetId());
    TargetStatus->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    SetItem("TargetStatus", TargetStatus, ERshipMessagePriority::High, target->GetId() + ":status");

    UE_LOG(LogRshipExec, Log, TEXT("DeleteTarget: %s - offline status sent"), *target->GetId());
}

void URshipSubsystem::SendAction(Action *action, FString targetId)
{
    TSharedPtr<FJsonObject> Action = MakeShareable(new FJsonObject);

    Action->SetStringField(TEXT("id"), action->GetId());
    Action->SetStringField(TEXT("name"), action->GetName());
    Action->SetStringField(TEXT("targetId"), targetId);
    Action->SetStringField(TEXT("serviceId"), ServiceId);
    TSharedPtr<FJsonObject> schema = action->GetSchema();
    if (schema)
    {
        Action->SetObjectField(TEXT("schema"), schema);
    }

    // Compute deterministic hash for change detection (before adding hash field)
    FString ActionHash = ComputeEntityHash(Action);

    // Check if action needs to be sent (new or changed)
    if (!NeedsActionUpdate(action->GetId(), ActionHash))
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("    Action %s unchanged, skipping"), *action->GetId());
        return;
    }

    // Set hash field for myko protocol (use the computed hash)
    Action->SetStringField(TEXT("hash"), ActionHash);

    // Action registration - HIGH priority, coalesce by action ID
    SetItem("Action", Action, ERshipMessagePriority::High, action->GetId());

    // NOTE: Cache is updated by server via live query subscription, not here
}

void URshipSubsystem::SendEmitter(EmitterContainer *emitter, FString targetId)
{
    TSharedPtr<FJsonObject> Emitter = MakeShareable(new FJsonObject);

    Emitter->SetStringField(TEXT("id"), emitter->GetId());
    Emitter->SetStringField(TEXT("name"), emitter->GetName());
    Emitter->SetStringField(TEXT("targetId"), targetId);
    Emitter->SetStringField(TEXT("serviceId"), ServiceId);
    TSharedPtr<FJsonObject> schema = emitter->GetSchema();
    if (schema)
    {
        Emitter->SetObjectField(TEXT("schema"), schema);
    }

    // Compute deterministic hash for change detection (before adding hash field)
    FString EmitterHash = ComputeEntityHash(Emitter);

    // Check if emitter needs to be sent (new or changed)
    if (!NeedsEmitterUpdate(emitter->GetId(), EmitterHash))
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("    Emitter %s unchanged, skipping"), *emitter->GetId());
        return;
    }

    // Set hash field for myko protocol (use the computed hash)
    Emitter->SetStringField(TEXT("hash"), EmitterHash);

    // Emitter registration - HIGH priority, coalesce by emitter ID
    SetItem("Emitter", Emitter, ERshipMessagePriority::High, emitter->GetId());

    // NOTE: Cache is updated by server via live query subscription, not here
}

void URshipSubsystem::SendTargetStatus(Target *target, bool online)
{
    if (!target) return;

    TSharedPtr<FJsonObject> TargetStatus = MakeShareable(new FJsonObject);

    TargetStatus->SetStringField(TEXT("targetId"), target->GetId());
    TargetStatus->SetStringField(TEXT("instanceId"), InstanceId);
    TargetStatus->SetStringField(TEXT("status"), online ? TEXT("online") : TEXT("offline"));
    // TargetStatus ID should match Target ID (per TS SDK: serviceId:short_id)
    TargetStatus->SetStringField(TEXT("id"), target->GetId());
    // Hash for optimistic concurrency control (myko protocol requirement)
    TargetStatus->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    SetItem("TargetStatus", TargetStatus, ERshipMessagePriority::High, target->GetId() + TEXT(":status"));

    UE_LOG(LogRshipExec, Log, TEXT("Sent target status: %s = %s"), *target->GetId(), online ? TEXT("online") : TEXT("offline"));
}

void URshipSubsystem::SendInstanceInfo()
{
    UE_LOG(LogRshipExec, Log, TEXT("SendInstanceInfo: MachineId=%s, ServiceId=%s, InstanceId=%s, ClusterId=%s, ClientId=%s"),
        *MachineId, *ServiceId, *InstanceId, *ClusterId, *ClientId);

    // Send Machine - HIGH priority, coalesce
    TSharedPtr<FJsonObject> Machine = MakeShareable(new FJsonObject);
    Machine->SetStringField(TEXT("id"), MachineId);
    Machine->SetStringField(TEXT("name"), MachineId);
    Machine->SetStringField(TEXT("execName"), MachineId);
    // clientId is required but filled by server - send empty string
    Machine->SetStringField(TEXT("clientId"), TEXT(""));
    // addresses is required - send empty array (server may populate from connection)
    TArray<TSharedPtr<FJsonValue>> AddressesJson;
    Machine->SetArrayField(TEXT("addresses"), AddressesJson);
    // Hash for optimistic concurrency control (myko protocol requirement)
    Machine->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    SetItem("Machine", Machine, ERshipMessagePriority::High, "machine:" + MachineId);

    const URshipSettings *Settings = GetDefault<URshipSettings>();

    FColor SRGBColor = Settings->ServiceColor.ToFColor(true);
    FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);

    // Send Instance - HIGH priority, coalesce
    TSharedPtr<FJsonObject> Instance = MakeShareable(new FJsonObject);

    Instance->SetStringField(TEXT("clientId"), ClientId);
    Instance->SetStringField(TEXT("name"), ServiceId);
    Instance->SetStringField(TEXT("id"), InstanceId);
    Instance->SetStringField(TEXT("clusterId"), ClusterId);
    Instance->SetStringField(TEXT("serviceTypeCode"), TEXT("unreal"));
    Instance->SetStringField(TEXT("serviceId"), ServiceId);
    Instance->SetStringField(TEXT("machineId"), MachineId);
    Instance->SetStringField(TEXT("status"), "Available");
    Instance->SetStringField(TEXT("color"), ColorHex);
    // Hash for optimistic concurrency control (myko protocol requirement)
    Instance->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    SetItem("Instance", Instance, ERshipMessagePriority::High, "instance:" + InstanceId);
}

void URshipSubsystem::SendAll()
{
    UE_LOG(LogRshipExec, Log, TEXT("SendAll: %d TargetComponents registered"), TargetComponents ? TargetComponents->Num() : 0);

    // Send Machine and Instance info first
    SendInstanceInfo();

    // Send all targets
    for (auto& Pair : *this->TargetComponents)
    {
        if (Pair.Value && Pair.Value->TargetData)
        {
            SendTarget(Pair.Value->TargetData);
        }
    }

    // Force immediate queue processing to ensure messages are sent
    // This is especially important when called from Register()/SetTargetId()
    // where the queue process timer might not be running or might have delay
    ProcessMessageQueue();
}

void URshipSubsystem::SendJson(TSharedPtr<FJsonObject> Payload)
{
    // Legacy method - queue with normal priority
    QueueMessage(Payload, ERshipMessagePriority::Normal, ERshipMessageType::Generic);
}

void URshipSubsystem::SetItem(FString itemType, TSharedPtr<FJsonObject> data, ERshipMessagePriority Priority, const FString& CoalesceKey)
{
    // MakeSet produces the complete WSMEvent format: { event: "ws:m:event", data: { itemType, changeType, item, tx, createdAt } }
    TSharedPtr<FJsonObject> payload = MakeSet(itemType, data);

    // Debug: Log registration events to help diagnose protocol issues
    // Log entity sends at Verbose level to avoid noise - use NeedsXxxUpdate logs for dedup status
    UE_LOG(LogRshipExec, Verbose, TEXT("SetItem [%s]"), *itemType);

    // Determine message type for coalescing
    ERshipMessageType Type = ERshipMessageType::Registration;
    if (itemType == "Pulse")
    {
        Type = ERshipMessageType::EmitterPulse;
    }
    else if (itemType == "Machine" || itemType == "Instance")
    {
        Type = ERshipMessageType::InstanceInfo;
    }

    QueueMessage(payload, Priority, Type, CoalesceKey);
}

void URshipSubsystem::PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data)
{
    TSharedPtr<FJsonObject> pulse = MakeShareable(new FJsonObject);

    FString fullEmitterId = targetId + ":" + emitterId;

    pulse->SetStringField("emitterId", fullEmitterId);
    pulse->SetStringField("id", fullEmitterId);
    pulse->SetObjectField("data", data);
    // timestamp is REQUIRED - Unix timestamp in milliseconds
    const FDateTime Now = FDateTime::UtcNow();
    const int64 TimestampMs = Now.ToUnixTimestamp() * 1000LL + Now.GetMillisecond();
    pulse->SetNumberField("timestamp", static_cast<double>(TimestampMs));
    // clientId is REQUIRED but server fills it - send empty string
    pulse->SetStringField("clientId", TEXT(""));
    // hash for optimistic concurrency control (myko protocol requirement)
    pulse->SetStringField("hash", FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    // Emitter pulses coalesce by emitter ID to ensure latest value is always sent
    // This prevents stale data from queueing - only the most recent pulse per emitter is kept
    SetItem("Pulse", pulse, ERshipMessagePriority::Normal, fullEmitterId);

    // Record pulse in health monitor for activity tracking
    if (HealthMonitor)
    {
        HealthMonitor->RecordPulse(targetId);
    }

    // Cache emitter value for preset capture
    if (PresetManager)
    {
        PresetManager->CacheEmitterValue(targetId, emitterId, data);
    }
}

EmitterContainer *URshipSubsystem::GetEmitterInfo(FString fullTargetId, FString emitterId)
{
    // O(1) lookup by target ID
    URshipTargetComponent* comp = FindTargetComponent(fullTargetId);
    if (!comp || !comp->TargetData)
    {
        return nullptr;
    }

    Target* target = comp->TargetData;
    FString fullEmitterId = fullTargetId + ":" + emitterId;

    auto emitters = target->GetEmitters();

    if (!emitters.Contains(fullEmitterId))
    {
        return nullptr;
    }

    return emitters[fullEmitterId];
}

FString URshipSubsystem::GetServiceId()
{
    return ServiceId;
}

FString URshipSubsystem::GetInstanceId()
{
    return InstanceId;
}

// ============================================================================
// DIAGNOSTIC METHODS
// These provide runtime visibility into the adaptive outbound pipeline
// ============================================================================

bool URshipSubsystem::IsConnected() const
{
    return WebSocket.IsValid() && WebSocket->IsConnected();
}

int32 URshipSubsystem::GetQueueLength() const
{
    if (RateLimiter)
    {
        return RateLimiter->GetQueueLength();
    }
    return 0;
}

int32 URshipSubsystem::GetQueueBytes() const
{
    if (RateLimiter)
    {
        return RateLimiter->GetQueueBytes();
    }
    return 0;
}

float URshipSubsystem::GetQueuePressure() const
{
    if (RateLimiter)
    {
        return RateLimiter->GetQueuePressure();
    }
    return 0.0f;
}

int32 URshipSubsystem::GetMessagesSentPerSecond() const
{
    if (RateLimiter)
    {
        return RateLimiter->GetMessagesSentLastSecond();
    }
    return 0;
}

int32 URshipSubsystem::GetBytesSentPerSecond() const
{
    if (RateLimiter)
    {
        return RateLimiter->GetBytesSentLastSecond();
    }
    return 0;
}

int32 URshipSubsystem::GetMessagesDropped() const
{
    if (RateLimiter)
    {
        return RateLimiter->GetMessagesDropped();
    }
    return 0;
}

bool URshipSubsystem::IsRateLimiterBackingOff() const
{
    if (RateLimiter)
    {
        return RateLimiter->IsBackingOff();
    }
    return false;
}

float URshipSubsystem::GetBackoffRemaining() const
{
    if (RateLimiter)
    {
        return RateLimiter->GetBackoffRemaining();
    }
    return 0.0f;
}

float URshipSubsystem::GetCurrentRateLimit() const
{
    if (RateLimiter)
    {
        return RateLimiter->GetCurrentRateLimit();
    }
    return 0.0f;
}

void URshipSubsystem::ResetRateLimiterStats()
{
    if (RateLimiter)
    {
        RateLimiter->ResetStats();
        UE_LOG(LogRshipExec, Log, TEXT("Rate limiter statistics reset"));
    }
}

// ============================================================================
// GROUP MANAGEMENT
// ============================================================================

URshipTargetGroupManager* URshipSubsystem::GetGroupManager()
{
    // Lazy initialization
    if (!GroupManager)
    {
        GroupManager = NewObject<URshipTargetGroupManager>(this);

        // Register all existing targets with the group manager
        if (TargetComponents)
        {
            for (auto& Pair : *TargetComponents)
            {
                if (Pair.Value)
                {
                    GroupManager->RegisterTarget(Pair.Value);
                }
            }
        }

        UE_LOG(LogRshipExec, Log, TEXT("GroupManager initialized with %d targets"),
            TargetComponents ? TargetComponents->Num() : 0);
    }
    return GroupManager;
}

// ============================================================================
// HEALTH MONITORING
// ============================================================================

URshipHealthMonitor* URshipSubsystem::GetHealthMonitor()
{
    // Lazy initialization
    if (!HealthMonitor)
    {
        HealthMonitor = NewObject<URshipHealthMonitor>(this);
        HealthMonitor->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("HealthMonitor initialized"));
    }
    return HealthMonitor;
}

// ============================================================================
// PRESET MANAGEMENT
// ============================================================================

URshipPresetManager* URshipSubsystem::GetPresetManager()
{
    // Lazy initialization
    if (!PresetManager)
    {
        PresetManager = NewObject<URshipPresetManager>(this);
        PresetManager->Initialize(this);

        // Load saved presets
        PresetManager->LoadPresetsFromFile();

        UE_LOG(LogRshipExec, Log, TEXT("PresetManager initialized"));
    }
    return PresetManager;
}

// ============================================================================
// TEMPLATE MANAGEMENT
// ============================================================================

URshipTemplateManager* URshipSubsystem::GetTemplateManager()
{
    // Lazy initialization
    if (!TemplateManager)
    {
        TemplateManager = NewObject<URshipTemplateManager>(this);
        TemplateManager->Initialize(this);

        // Load saved templates
        TemplateManager->LoadTemplatesFromFile();

        UE_LOG(LogRshipExec, Log, TEXT("TemplateManager initialized"));
    }
    return TemplateManager;
}

// ============================================================================
// LEVEL MANAGEMENT
// ============================================================================

URshipLevelManager* URshipSubsystem::GetLevelManager()
{
    // Lazy initialization
    if (!LevelManager)
    {
        LevelManager = NewObject<URshipLevelManager>(this);
        LevelManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("LevelManager initialized"));
    }
    return LevelManager;
}

// ============================================================================
// EDITOR SELECTION
// ============================================================================

URshipEditorSelection* URshipSubsystem::GetEditorSelection()
{
    // Lazy initialization
    if (!EditorSelection)
    {
        EditorSelection = NewObject<URshipEditorSelection>(this);
        EditorSelection->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("EditorSelection initialized (available=%s)"),
            EditorSelection->IsEditorSyncAvailable() ? TEXT("Yes") : TEXT("No"));
    }
    return EditorSelection;
}

// ============================================================================
// DATA LAYER MANAGEMENT
// ============================================================================

URshipDataLayerManager* URshipSubsystem::GetDataLayerManager()
{
    // Lazy initialization
    if (!DataLayerManager)
    {
        DataLayerManager = NewObject<URshipDataLayerManager>(this);
        DataLayerManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("DataLayerManager initialized"));
    }
    return DataLayerManager;
}

// ============================================================================
// FIXTURE MANAGEMENT
// ============================================================================

URshipFixtureManager* URshipSubsystem::GetFixtureManager()
{
    // Lazy initialization
    if (!FixtureManager)
    {
        FixtureManager = NewObject<URshipFixtureManager>(this);
        FixtureManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("FixtureManager initialized"));
    }
    return FixtureManager;
}

// ============================================================================
// CAMERA MANAGEMENT
// ============================================================================

URshipCameraManager* URshipSubsystem::GetCameraManager()
{
    // Lazy initialization
    if (!CameraManager)
    {
        CameraManager = NewObject<URshipCameraManager>(this);
        CameraManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("CameraManager initialized"));
    }
    return CameraManager;
}

// ============================================================================
// IES PROFILE SERVICE
// ============================================================================

URshipIESProfileService* URshipSubsystem::GetIESProfileService()
{
    // Lazy initialization
    if (!IESProfileService)
    {
        IESProfileService = NewObject<URshipIESProfileService>(this);
        IESProfileService->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("IESProfileService initialized"));
    }
    return IESProfileService;
}

// ============================================================================
// SCENE CONVERSION
// ============================================================================

URshipSceneConverter* URshipSubsystem::GetSceneConverter()
{
    // Lazy initialization
    if (!SceneConverter)
    {
        SceneConverter = NewObject<URshipSceneConverter>(this);
        SceneConverter->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("SceneConverter initialized"));
    }
    return SceneConverter;
}

// ============================================================================
// EDITOR TRANSFORM SYNC
// ============================================================================

URshipEditorTransformSync* URshipSubsystem::GetEditorTransformSync()
{
    // Lazy initialization
    if (!EditorTransformSync)
    {
        EditorTransformSync = NewObject<URshipEditorTransformSync>(this);
        EditorTransformSync->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("EditorTransformSync initialized"));
    }
    return EditorTransformSync;
}

// ============================================================================
// PULSE RECEIVER
// ============================================================================

URshipPulseReceiver* URshipSubsystem::GetPulseReceiver()
{
    // Lazy initialization
    if (!PulseReceiver)
    {
        PulseReceiver = NewObject<URshipPulseReceiver>(this);
        PulseReceiver->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("PulseReceiver initialized"));
    }
    return PulseReceiver;
}

// ============================================================================
// FEEDBACK REPORTER
// ============================================================================

URshipFeedbackReporter* URshipSubsystem::GetFeedbackReporter()
{
    // Lazy initialization
    if (!FeedbackReporter)
    {
        FeedbackReporter = NewObject<URshipFeedbackReporter>(this);
        FeedbackReporter->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("FeedbackReporter initialized"));
    }
    return FeedbackReporter;
}

// ============================================================================
// VISUALIZATION MANAGER
// ============================================================================

URshipVisualizationManager* URshipSubsystem::GetVisualizationManager()
{
    // Lazy initialization
    if (!VisualizationManager)
    {
        VisualizationManager = NewObject<URshipVisualizationManager>(this);
        VisualizationManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("VisualizationManager initialized"));
    }
    return VisualizationManager;
}

// ============================================================================
// TIMECODE SYNC
// ============================================================================

URshipTimecodeSync* URshipSubsystem::GetTimecodeSync()
{
    // Lazy initialization
    if (!TimecodeSync)
    {
        TimecodeSync = NewObject<URshipTimecodeSync>(this);
        TimecodeSync->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("TimecodeSync initialized"));
    }
    return TimecodeSync;
}

// ============================================================================
// FIXTURE LIBRARY
// ============================================================================

URshipFixtureLibrary* URshipSubsystem::GetFixtureLibrary()
{
    // Lazy initialization
    if (!FixtureLibrary)
    {
        FixtureLibrary = NewObject<URshipFixtureLibrary>(this);
        FixtureLibrary->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("FixtureLibrary initialized with %d profiles"), FixtureLibrary->GetAllProfiles().Num());
    }
    return FixtureLibrary;
}

// ============================================================================
// MULTI-CAMERA MANAGER
// ============================================================================

URshipMultiCameraManager* URshipSubsystem::GetMultiCameraManager()
{
    // Lazy initialization
    if (!MultiCameraManager)
    {
        MultiCameraManager = NewObject<URshipMultiCameraManager>(this);
        MultiCameraManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("MultiCameraManager initialized"));
    }
    return MultiCameraManager;
}

// ============================================================================
// SCENE VALIDATOR
// ============================================================================

URshipSceneValidator* URshipSubsystem::GetSceneValidator()
{
    // Lazy initialization
    if (!SceneValidator)
    {
        SceneValidator = NewObject<URshipSceneValidator>(this);
        SceneValidator->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("SceneValidator initialized"));
    }
    return SceneValidator;
}

// ============================================================================
// NIAGARA MANAGER
// ============================================================================

URshipNiagaraManager* URshipSubsystem::GetNiagaraManager()
{
    // Lazy initialization
    if (!NiagaraManager)
    {
        NiagaraManager = NewObject<URshipNiagaraManager>(this);
        NiagaraManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("NiagaraManager initialized"));
    }
    return NiagaraManager;
}

// ============================================================================
// SEQUENCER SYNC
// ============================================================================

URshipSequencerSync* URshipSubsystem::GetSequencerSync()
{
    // Lazy initialization
    if (!SequencerSync)
    {
        SequencerSync = NewObject<URshipSequencerSync>(this);
        SequencerSync->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("SequencerSync initialized"));
    }
    return SequencerSync;
}

// ============================================================================
// MATERIAL MANAGER
// ============================================================================

URshipMaterialManager* URshipSubsystem::GetMaterialManager()
{
    // Lazy initialization
    if (!MaterialManager)
    {
        MaterialManager = NewObject<URshipMaterialManager>(this);
        MaterialManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("MaterialManager initialized"));
    }
    return MaterialManager;
}

// ============================================================================
// SUBSTRATE MATERIAL MANAGER
// ============================================================================

URshipSubstrateMaterialManager* URshipSubsystem::GetSubstrateMaterialManager()
{
    // Lazy initialization
    if (!SubstrateMaterialManager)
    {
        SubstrateMaterialManager = NewObject<URshipSubstrateMaterialManager>(this);
        SubstrateMaterialManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("SubstrateMaterialManager initialized"));
    }
    return SubstrateMaterialManager;
}

// ============================================================================
// DMX OUTPUT
// ============================================================================

URshipDMXOutput* URshipSubsystem::GetDMXOutput()
{
    // Lazy initialization
    if (!DMXOutput)
    {
        DMXOutput = NewObject<URshipDMXOutput>(this);
        DMXOutput->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("DMXOutput initialized"));
    }
    return DMXOutput;
}

// ============================================================================
// OSC BRIDGE
// ============================================================================

URshipOSCBridge* URshipSubsystem::GetOSCBridge()
{
    // Lazy initialization
    if (!OSCBridge)
    {
        OSCBridge = NewObject<URshipOSCBridge>(this);
        OSCBridge->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("OSCBridge initialized"));
    }
    return OSCBridge;
}

// ============================================================================
// LIVE LINK SERVICE
// ============================================================================

URshipLiveLinkService* URshipSubsystem::GetLiveLinkService()
{
    // Lazy initialization
    if (!LiveLinkService)
    {
        LiveLinkService = NewObject<URshipLiveLinkService>(this);
        LiveLinkService->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("LiveLinkService initialized"));
    }
    return LiveLinkService;
}

// ============================================================================
// AUDIO MANAGER
// ============================================================================

URshipAudioManager* URshipSubsystem::GetAudioManager()
{
    // Lazy initialization
    if (!AudioManager)
    {
        AudioManager = NewObject<URshipAudioManager>(this);
        AudioManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("AudioManager initialized"));
    }
    return AudioManager;
}

// ============================================================================
// RECORDER
// ============================================================================

URshipRecorder* URshipSubsystem::GetRecorder()
{
    // Lazy initialization
    if (!Recorder)
    {
        Recorder = NewObject<URshipRecorder>(this);
        Recorder->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("Recorder initialized"));
    }
    return Recorder;
}

// ============================================================================
// CONTROL RIG MANAGER
// ============================================================================

URshipControlRigManager* URshipSubsystem::GetControlRigManager()
{
    // Lazy initialization
    if (!ControlRigManager)
    {
        ControlRigManager = NewObject<URshipControlRigManager>(this);
        ControlRigManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("ControlRigManager initialized"));
    }
    return ControlRigManager;
}

// ============================================================================
// PCG MANAGER
// ============================================================================

URshipPCGManager* URshipSubsystem::GetPCGManager()
{
    // Lazy initialization
    // PCGManager is always available - only the PCG graph nodes require PCG plugin
    if (!PCGManager)
    {
        PCGManager = NewObject<URshipPCGManager>(this);
        PCGManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("PCGManager initialized"));
    }
    return PCGManager;
}

URshipSpatialAudioManager* URshipSubsystem::GetSpatialAudioManager()
{
    // Lazy initialization - only if RshipSpatialAudio module is loaded
    // This is an optional plugin dependency - returns nullptr if plugin is not enabled
    if (!SpatialAudioManager)
    {
        // Check if the SpatialAudio module is available
        if (FModuleManager::Get().IsModuleLoaded("RshipSpatialAudioRuntime"))
        {
            // Use reflection to create the manager since RshipExec doesn't have
            // a compile-time dependency on RshipSpatialAudioRuntime
            UClass* ManagerClass = FindObject<UClass>(nullptr, TEXT("/Script/RshipSpatialAudioRuntime.RshipSpatialAudioManager"));
            if (ManagerClass)
            {
                // Use UObject-based NewObject since URshipSpatialAudioManager is forward-declared
                UObject* ManagerObj = NewObject<UObject>(this, ManagerClass);
                // reinterpret_cast needed since URshipSpatialAudioManager is only forward-declared
                SpatialAudioManager = reinterpret_cast<URshipSpatialAudioManager*>(ManagerObj);

                // Call Initialize via reflection (it's a UFUNCTION in the manager)
                UFunction* InitFunc = ManagerClass->FindFunctionByName(TEXT("Initialize"));
                if (InitFunc)
                {
                    struct { URshipSubsystem* Subsystem; } Params = { this };
                    ManagerObj->ProcessEvent(InitFunc, &Params);
                    UE_LOG(LogRshipExec, Log, TEXT("SpatialAudioManager initialized"));
                }
                else
                {
                    UE_LOG(LogRshipExec, Warning, TEXT("SpatialAudioManager::Initialize not found"));
                }
            }
            else
            {
                UE_LOG(LogRshipExec, Verbose, TEXT("SpatialAudioManager class not found - RshipSpatialAudio plugin may need rebuild"));
            }
        }
    }
    return SpatialAudioManager;
}

// ============================================================================
// TARGET COMPONENT REGISTRY (O(1) LOOKUPS)
// ============================================================================

void URshipSubsystem::RegisterTargetComponent(URshipTargetComponent* Component)
{
    if (!Component || !Component->TargetData)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterTargetComponent: Invalid component or null TargetData"));
        return;
    }

    if (!TargetComponents)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterTargetComponent: TargetComponents map not initialized"));
        return;
    }

    const FString& TargetId = Component->TargetData->GetId();
    TargetComponents->Add(TargetId, Component);

    UE_LOG(LogRshipExec, Log, TEXT("Registered target component: %s (total: %d)"),
        *TargetId, TargetComponents->Num());
}

void URshipSubsystem::UnregisterTargetComponent(URshipTargetComponent* Component)
{
    if (!Component || !TargetComponents)
    {
        return;
    }

    // Find and remove by value since we might not have TargetData anymore during destruction
    FString KeyToRemove;
    for (auto& Pair : *TargetComponents)
    {
        if (Pair.Value == Component)
        {
            KeyToRemove = Pair.Key;
            break;
        }
    }

    if (!KeyToRemove.IsEmpty())
    {
        // RemoveSingle removes exactly one entry matching both key AND value
        // This is important for TMultiMap where multiple components can share a target ID
        TargetComponents->RemoveSingle(KeyToRemove, Component);
        UE_LOG(LogRshipExec, Log, TEXT("Unregistered target component: %s (remaining: %d)"),
            *KeyToRemove, TargetComponents->Num());
    }
}

URshipTargetComponent* URshipSubsystem::FindTargetComponent(const FString& FullTargetId) const
{
    if (!TargetComponents)
    {
        return nullptr;
    }

    URshipTargetComponent* const* Found = TargetComponents->Find(FullTargetId);
    return Found ? *Found : nullptr;
}

TArray<URshipTargetComponent*> URshipSubsystem::FindAllTargetComponents(const FString& FullTargetId) const
{
    TArray<URshipTargetComponent*> Result;
    if (TargetComponents)
    {
        TargetComponents->MultiFind(FullTargetId, Result);
    }
    return Result;
}

// ============================================================================
// ENTITY CACHE AND QUERY SUPPORT
// Smart registration: query server on connect, skip unchanged entities
// ============================================================================

void URshipSubsystem::SendQuery(
    const FString& QueryId,
    const FString& QueryItemType,
    TSharedPtr<FJsonObject> QueryParams,
    TFunction<void(const TArray<TSharedPtr<FJsonValue>>&)> OnComplete)
{
    FString Tx;
    TSharedPtr<FJsonObject> QueryMessage = MakeQuery(QueryId, QueryItemType, QueryParams, Tx);

    // Register callback for this query
    FPendingQuery Pending;
    Pending.QueryId = QueryId;
    Pending.QueryItemType = QueryItemType;
    Pending.OnComplete = OnComplete;
    PendingQueries.Add(Tx, Pending);

    // Send the query message
    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(QueryMessage.ToSharedRef(), JsonWriter);

    UE_LOG(LogRshipExec, Log, TEXT("SendQuery: %s (%s) tx=%s params=%s"), *QueryId, *QueryItemType, *Tx, *JsonString);

    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        if (bUseMsgpack)
        {
            TArray<uint8> BinaryData;
            if (FRshipMsgPack::Encode(QueryMessage, BinaryData))
            {
                WebSocket->SendBinary(BinaryData);
            }
            else
            {
                UE_LOG(LogRshipExec, Warning, TEXT("SendQuery: Msgpack encoding failed, using JSON fallback"));
                WebSocket->Send(JsonString);
            }
        }
        else
        {
            WebSocket->Send(JsonString);
        }
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("SendQuery: WebSocket not connected, query not sent"));
    }
}

void URshipSubsystem::SendQuery(
    const FMQuery& Query,
    TFunction<void(const TArray<TSharedPtr<FJsonValue>>&)> OnComplete)
{
    SendQuery(Query.GetQueryId(), Query.GetQueryItemType(), Query.ToJson(), OnComplete);
}

void URshipSubsystem::ProcessQueryResponse(TSharedPtr<FJsonObject> ResponseData)
{
    if (!ResponseData.IsValid())
    {
        return;
    }

    FString Tx = ResponseData->GetStringField(TEXT("tx"));
    int32 Sequence = (int32)ResponseData->GetNumberField(TEXT("sequence"));

    // Find the pending query callback
    FPendingQuery* Pending = PendingQueries.Find(Tx);
    if (!Pending)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("ProcessQueryResponse: Unknown tx=%s"), *Tx);
        return;
    }

    UE_LOG(LogRshipExec, Log, TEXT("ProcessQueryResponse: tx=%s seq=%d query=%s"),
        *Tx, Sequence, *Pending->QueryId);

    // Get upserts array (new or changed entities)
    const TArray<TSharedPtr<FJsonValue>>* Upserts = nullptr;
    ResponseData->TryGetArrayField(TEXT("upserts"), Upserts);

    // Get deletes array (removed entities) - for delta updates
    const TArray<TSharedPtr<FJsonValue>>* Deletes = nullptr;
    ResponseData->TryGetArrayField(TEXT("deletes"), Deletes);

    // Process upserts - call the callback for initial sync or delta updates
    if (Pending->OnComplete)
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        if (Upserts)
        {
            Items = *Upserts;
        }
        Pending->OnComplete(Items);
    }

    // Process deletes - remove from cache (delta updates only, sequence > 0)
    if (Deletes && Sequence > 0)
    {
        for (const TSharedPtr<FJsonValue>& DeleteValue : *Deletes)
        {
            FString DeletedId = DeleteValue->AsString();
            if (!DeletedId.IsEmpty())
            {
                // Remove from appropriate cache based on query type
                if (Pending->QueryItemType == TEXT("Target"))
                {
                    ServerTargetHashes.Remove(DeletedId);
                    UE_LOG(LogRshipExec, Verbose, TEXT("ProcessQueryResponse: Removed target %s from cache"), *DeletedId);
                }
                else if (Pending->QueryItemType == TEXT("Action"))
                {
                    ServerActionHashes.Remove(DeletedId);
                    UE_LOG(LogRshipExec, Verbose, TEXT("ProcessQueryResponse: Removed action %s from cache"), *DeletedId);
                }
                else if (Pending->QueryItemType == TEXT("Emitter"))
                {
                    ServerEmitterHashes.Remove(DeletedId);
                    UE_LOG(LogRshipExec, Verbose, TEXT("ProcessQueryResponse: Removed emitter %s from cache"), *DeletedId);
                }
            }
        }
    }

    // Keep query subscription live for delta updates (don't cancel)
    // The query will be cancelled when we disconnect or explicitly cancel
}

void URshipSubsystem::SyncEntityCacheFromServer()
{
    UE_LOG(LogRshipExec, Log, TEXT("SyncEntityCacheFromServer: Starting cache sync for serviceId=%s"), *ServiceId);

    // Clear existing cache and pending queries
    ServerTargetHashes.Empty();
    ServerActionHashes.Empty();
    ServerEmitterHashes.Empty();
    PendingQueries.Empty();  // Cancel any existing subscriptions before starting new ones
    bEntityCacheSynced = false;

    // Track initial sync completion using shared pointers (must survive async callbacks)
    // PendingCount tracks how many queries still need their initial response (sequence 0)
    // InitialSyncFlags track whether each query has received its first response
    TSharedPtr<int32> PendingCount = MakeShareable(new int32(3));
    TSharedPtr<bool> TargetSyncComplete = MakeShareable(new bool(false));
    TSharedPtr<bool> ActionSyncComplete = MakeShareable(new bool(false));
    TSharedPtr<bool> EmitterSyncComplete = MakeShareable(new bool(false));

    // Query targets - callback handles both initial sync and live delta updates
    SendQuery(FGetTargetsByServiceId(ServiceId),
        [this, PendingCount, TargetSyncComplete](const TArray<TSharedPtr<FJsonValue>>& Items)
        {
            bool bIsInitialSync = !(*TargetSyncComplete);
            UE_LOG(LogRshipExec, Log, TEXT("Target query response: %d items (initial=%d)"), Items.Num(), bIsInitialSync ? 1 : 0);

            // Update cache with upserts (works for both initial sync and deltas)
            // Query response structure: { item: { id, hash, ... }, itemType: "Target" }
            for (const TSharedPtr<FJsonValue>& ItemValue : Items)
            {
                TSharedPtr<FJsonObject> Wrapper = ItemValue->AsObject();
                if (Wrapper.IsValid())
                {
                    // Extract the actual item from the wrapper
                    TSharedPtr<FJsonObject> Item = Wrapper->GetObjectField(TEXT("item"));
                    if (Item.IsValid())
                    {
                        FString Id = Item->GetStringField(TEXT("id"));
                        FString Hash = Item->GetStringField(TEXT("hash"));
                        if (!Id.IsEmpty())
                        {
                            ServerTargetHashes.Add(Id, Hash);
                            UE_LOG(LogRshipExec, Log, TEXT("  Cache: Target %s = %s"), *Id, *Hash);
                        }
                    }
                }
            }

            // Only decrement PendingCount on first response (initial sync)
            if (bIsInitialSync)
            {
                *TargetSyncComplete = true;
                (*PendingCount)--;
                if (*PendingCount == 0)
                {
                    bEntityCacheSynced = true;
                    UE_LOG(LogRshipExec, Log, TEXT("=== CACHE SYNC COMPLETE === Targets=%d, Actions=%d, Emitters=%d"),
                        ServerTargetHashes.Num(), ServerActionHashes.Num(), ServerEmitterHashes.Num());
                    SendAll();
                }
            }
        });

    // Query actions - callback handles both initial sync and live delta updates
    SendQuery(*FGetActionsByQuery::ByServiceId(ServiceId),
        [this, PendingCount, ActionSyncComplete](const TArray<TSharedPtr<FJsonValue>>& Items)
        {
            bool bIsInitialSync = !(*ActionSyncComplete);
            UE_LOG(LogRshipExec, Log, TEXT("Action query response: %d items (initial=%d)"), Items.Num(), bIsInitialSync ? 1 : 0);

            // Update cache with upserts (works for both initial sync and deltas)
            // Query response structure: { item: { id, hash, ... }, itemType: "Action" }
            for (const TSharedPtr<FJsonValue>& ItemValue : Items)
            {
                TSharedPtr<FJsonObject> Wrapper = ItemValue->AsObject();
                if (Wrapper.IsValid())
                {
                    // Extract the actual item from the wrapper
                    TSharedPtr<FJsonObject> Item = Wrapper->GetObjectField(TEXT("item"));
                    if (Item.IsValid())
                    {
                        FString Id = Item->GetStringField(TEXT("id"));
                        FString Hash = Item->GetStringField(TEXT("hash"));
                        if (!Id.IsEmpty())
                        {
                            ServerActionHashes.Add(Id, Hash);
                            UE_LOG(LogRshipExec, Verbose, TEXT("  Cache: Action %s = %s"), *Id, *Hash);
                        }
                    }
                }
            }

            // Only decrement PendingCount on first response (initial sync)
            if (bIsInitialSync)
            {
                *ActionSyncComplete = true;
                (*PendingCount)--;
                if (*PendingCount == 0)
                {
                    bEntityCacheSynced = true;
                    UE_LOG(LogRshipExec, Log, TEXT("=== CACHE SYNC COMPLETE === Targets=%d, Actions=%d, Emitters=%d"),
                        ServerTargetHashes.Num(), ServerActionHashes.Num(), ServerEmitterHashes.Num());
                    SendAll();
                }
            }
        });

    // Query emitters - callback handles both initial sync and live delta updates
    SendQuery(*FGetEmittersByQuery::ByServiceId(ServiceId),
        [this, PendingCount, EmitterSyncComplete](const TArray<TSharedPtr<FJsonValue>>& Items)
        {
            bool bIsInitialSync = !(*EmitterSyncComplete);
            UE_LOG(LogRshipExec, Log, TEXT("Emitter query response: %d items (initial=%d)"), Items.Num(), bIsInitialSync ? 1 : 0);

            // Update cache with upserts (works for both initial sync and deltas)
            // Query response structure: { item: { id, hash, ... }, itemType: "Emitter" }
            for (const TSharedPtr<FJsonValue>& ItemValue : Items)
            {
                TSharedPtr<FJsonObject> Wrapper = ItemValue->AsObject();
                if (Wrapper.IsValid())
                {
                    // Extract the actual item from the wrapper
                    TSharedPtr<FJsonObject> Item = Wrapper->GetObjectField(TEXT("item"));
                    if (Item.IsValid())
                    {
                        FString Id = Item->GetStringField(TEXT("id"));
                        FString Hash = Item->GetStringField(TEXT("hash"));
                        if (!Id.IsEmpty())
                        {
                            ServerEmitterHashes.Add(Id, Hash);
                            UE_LOG(LogRshipExec, Verbose, TEXT("  Cache: Emitter %s = %s"), *Id, *Hash);
                        }
                    }
                }
            }

            // Only decrement PendingCount on first response (initial sync)
            if (bIsInitialSync)
            {
                *EmitterSyncComplete = true;
                (*PendingCount)--;
                if (*PendingCount == 0)
                {
                    bEntityCacheSynced = true;
                    UE_LOG(LogRshipExec, Log, TEXT("=== CACHE SYNC COMPLETE === Targets=%d, Actions=%d, Emitters=%d"),
                        ServerTargetHashes.Num(), ServerActionHashes.Num(), ServerEmitterHashes.Num());
                    SendAll();
                }
            }
        });
}

bool URshipSubsystem::NeedsTargetUpdate(const FString& TargetId, const FString& Hash) const
{
    if (!bEntityCacheSynced)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("NeedsTargetUpdate(%s): cache not synced, will send"), *TargetId);
        return true;
    }
    const FString* ServerHash = ServerTargetHashes.Find(TargetId);
    if (!ServerHash)
    {
        UE_LOG(LogRshipExec, Log, TEXT("NeedsTargetUpdate(%s): not in cache, will send (local=%s)"), *TargetId, *Hash);
        return true;
    }
    if (*ServerHash != Hash)
    {
        UE_LOG(LogRshipExec, Log, TEXT("NeedsTargetUpdate(%s): hash mismatch, will send (local=%s, server=%s)"), *TargetId, *Hash, **ServerHash);
        return true;
    }
    UE_LOG(LogRshipExec, Log, TEXT("NeedsTargetUpdate(%s): hash match, SKIPPING (hash=%s)"), *TargetId, *Hash);
    return false;
}

bool URshipSubsystem::NeedsActionUpdate(const FString& ActionId, const FString& Hash) const
{
    if (!bEntityCacheSynced)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("NeedsActionUpdate(%s): cache not synced, will send"), *ActionId);
        return true;
    }
    const FString* ServerHash = ServerActionHashes.Find(ActionId);
    if (!ServerHash)
    {
        UE_LOG(LogRshipExec, Log, TEXT("NeedsActionUpdate(%s): not in cache, will send (local=%s)"), *ActionId, *Hash);
        return true;
    }
    if (*ServerHash != Hash)
    {
        UE_LOG(LogRshipExec, Log, TEXT("NeedsActionUpdate(%s): hash mismatch, will send (local=%s, server=%s)"), *ActionId, *Hash, **ServerHash);
        return true;
    }
    UE_LOG(LogRshipExec, Log, TEXT("NeedsActionUpdate(%s): hash match, SKIPPING (hash=%s)"), *ActionId, *Hash);
    return false;
}

bool URshipSubsystem::NeedsEmitterUpdate(const FString& EmitterId, const FString& Hash) const
{
    if (!bEntityCacheSynced)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("NeedsEmitterUpdate(%s): cache not synced, will send"), *EmitterId);
        return true;
    }
    const FString* ServerHash = ServerEmitterHashes.Find(EmitterId);
    if (!ServerHash)
    {
        UE_LOG(LogRshipExec, Log, TEXT("NeedsEmitterUpdate(%s): not in cache, will send (local=%s)"), *EmitterId, *Hash);
        return true;
    }
    if (*ServerHash != Hash)
    {
        UE_LOG(LogRshipExec, Log, TEXT("NeedsEmitterUpdate(%s): hash mismatch, will send (local=%s, server=%s)"), *EmitterId, *Hash, **ServerHash);
        return true;
    }
    UE_LOG(LogRshipExec, Log, TEXT("NeedsEmitterUpdate(%s): hash match, SKIPPING (hash=%s)"), *EmitterId, *Hash);
    return false;
}
