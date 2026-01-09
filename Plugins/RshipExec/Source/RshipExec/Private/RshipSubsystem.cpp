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

#if WITH_EDITOR
#include "Editor.h"
#endif

using namespace std;

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

    // Start subsystem tick ticker (60Hz for smooth updates, works in editor)
    SubsystemTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnSubsystemTick),
        1.0f / 60.0f  // 60Hz tick rate
    );
    UE_LOG(LogRshipExec, Log, TEXT("Started subsystem ticker (60Hz)"));
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

        FString PingJson;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&PingJson);
        FJsonSerializer::Serialize(PingPayload.ToSharedRef(), JsonWriter);

        UE_LOG(LogRshipExec, Log, TEXT("*** SENDING DIAGNOSTIC PING *** %s"), *PingJson);

        // Send directly to bypass rate limiter for diagnostic
        if (WebSocket.IsValid())
        {
            WebSocket->Send(PingJson);
        }
    }

    // Send registration data
    SendAll();

    // Force immediate queue processing - the timer may not be running yet
    // (world timer manager may not be ready at subsystem init time)
    UE_LOG(LogRshipExec, Log, TEXT("Forcing immediate queue processing after SendAll"));
    ProcessMessageQueue();

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

    WebSocket->Send(JsonString);
}

void URshipSubsystem::ProcessMessage(const FString &message)
{
    TSharedPtr<FJsonObject> obj = MakeShareable(new FJsonObject);
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(message);

    if (!(FJsonSerializer::Deserialize(Reader, obj) && obj.IsValid()))
    {
        return;
    }

    TSharedRef<FJsonObject> objRef = obj.ToSharedRef();

    FString type = objRef->GetStringField(TEXT("event"));
    UE_LOG(LogRshipExec, Log, TEXT("Received message: event=%s"), *type);

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
            UE_LOG(LogRshipExec, Warning, TEXT("Received ClientId %s"), *ClientId);
            SendAll();
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
        obj.Reset();
    }
    else if (type == "ws:m:event")
    {
        // Entity event - route to appropriate manager
        // Myko protocol: { event: "ws:m:event", data: { changeType: "SET"|"DEL", itemType, item, tx, createdAt } }
        TSharedPtr<FJsonObject> data = obj->GetObjectField(TEXT("data"));

        if (!data.IsValid())
        {
            return;
        }

        FString changeType = data->GetStringField(TEXT("changeType"));
        bool bIsDelete = (changeType == TEXT("DEL"));

        FString itemType = data->GetStringField(TEXT("itemType"));
        TSharedPtr<FJsonObject> item = data->GetObjectField(TEXT("item"));

        if (!item.IsValid())
        {
            return;
        }

        UE_LOG(LogRshipExec, Log, TEXT("Entity event: %s %s"), *changeType, *itemType);

        // Route to fixture manager
        if (itemType == TEXT("Fixture"))
        {
            if (FixtureManager)
            {
                FixtureManager->ProcessFixtureEvent(item, bIsDelete);
            }
        }
        else if (itemType == TEXT("FixtureType"))
        {
            if (FixtureManager)
            {
                FixtureManager->ProcessFixtureTypeEvent(item, bIsDelete);
            }
        }
        else if (itemType == TEXT("FixtureCalibration"))
        {
            if (FixtureManager)
            {
                FixtureManager->ProcessCalibrationEvent(item, bIsDelete);
            }
        }
        // Route to camera manager
        else if (itemType == TEXT("Camera"))
        {
            if (CameraManager)
            {
                CameraManager->ProcessCameraEvent(item, bIsDelete);
            }
        }
        else if (itemType == TEXT("Calibration"))
        {
            // OpenCV camera calibration result
            if (CameraManager)
            {
                CameraManager->ProcessCalibrationEvent(item, bIsDelete);
            }
        }
        else if (itemType == TEXT("ColorProfile"))
        {
            if (CameraManager)
            {
                CameraManager->ProcessColorProfileEvent(item, bIsDelete);
            }
        }
        // Route pulses to pulse receiver
        else if (itemType == TEXT("Pulse") && !bIsDelete)
        {
            if (PulseReceiver)
            {
                // Extract emitterId and data from the pulse
                FString EmitterId = item->GetStringField(TEXT("emitterId"));
                TSharedPtr<FJsonObject> PulseData = item->GetObjectField(TEXT("data"));

                if (!EmitterId.IsEmpty() && PulseData.IsValid())
                {
                    PulseReceiver->ProcessPulseEvent(EmitterId, PulseData);
                }
            }
        }
        // Route timecode events
        else if (itemType == TEXT("Timecode") && !bIsDelete)
        {
            if (TimecodeSync)
            {
                TimecodeSync->ProcessTimecodeEvent(item);
            }
        }
        // Route event track events
        else if (itemType == TEXT("EventTrack"))
        {
            if (TimecodeSync && !bIsDelete)
            {
                TimecodeSync->ProcessEventTrackEvent(item);
            }
        }
        // Route fixture profile events to library
        else if (itemType == TEXT("FixtureProfile"))
        {
            if (FixtureLibrary)
            {
                FixtureLibrary->ProcessProfileEvent(item, bIsDelete);
            }
        }
        // Route camera switch commands
        else if (itemType == TEXT("CameraSwitch") && !bIsDelete)
        {
            if (MultiCameraManager)
            {
                MultiCameraManager->ProcessCameraSwitchCommand(item);
            }
        }
        // Route camera view events
        else if (itemType == TEXT("CameraView"))
        {
            if (MultiCameraManager && !bIsDelete)
            {
                // Sync camera views from rship
                FRshipCameraView View;
                View.Id = item->GetStringField(TEXT("id"));
                View.Name = item->GetStringField(TEXT("name"));
                MultiCameraManager->AddView(View);
            }
        }
    }
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
            1.0f / 60.0f
        );
        UE_LOG(LogRshipExec, Log, TEXT("Restarted subsystem ticker"));
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
    UE_LOG(LogRshipExec, Log, TEXT("SendTarget: %s - %d actions, %d emitters"),
        *target->GetId(),
        target->GetActions().Num(),
        target->GetEmitters().Num());

    TArray<TSharedPtr<FJsonValue>> EmitterIdsJson;
    TArray<TSharedPtr<FJsonValue>> ActionIdsJson;

    for (auto &Elem : target->GetActions())
    {
        UE_LOG(LogRshipExec, Log, TEXT("  Action: %s"), *Elem.Key);
        ActionIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));
        SendAction(Elem.Value, target->GetId());
    }

    for (auto &Elem : target->GetEmitters())
    {
        UE_LOG(LogRshipExec, Log, TEXT("  Emitter: %s"), *Elem.Key);
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

    // Hash for optimistic concurrency control (myko protocol requirement)
    Target->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    // Target registration - HIGH priority, coalesce by target ID
    SetItem("Target", Target, ERshipMessagePriority::High, target->GetId());

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
    // Hash for optimistic concurrency control (myko protocol requirement)
    Action->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    // Action registration - HIGH priority, coalesce by action ID
    SetItem("Action", Action, ERshipMessagePriority::High, action->GetId());
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
    // Hash for optimistic concurrency control (myko protocol requirement)
    Emitter->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

    // Emitter registration - HIGH priority, coalesce by emitter ID
    SetItem("Emitter", Emitter, ERshipMessagePriority::High, emitter->GetId());
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

void URshipSubsystem::SendAll()
{
    UE_LOG(LogRshipExec, Log, TEXT("SendAll: MachineId=%s, ServiceId=%s, InstanceId=%s, ClusterId=%s, ClientId=%s"),
        *MachineId, *ServiceId, *InstanceId, *ClusterId, *ClientId);
    UE_LOG(LogRshipExec, Log, TEXT("SendAll: %d TargetComponents registered"), TargetComponents ? TargetComponents->Num() : 0);

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
    if (itemType == TEXT("Machine") || itemType == TEXT("Instance") || itemType == TEXT("Target") || itemType == TEXT("TargetStatus"))
    {
        FString JsonString;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
        FJsonSerializer::Serialize(payload.ToSharedRef(), JsonWriter);
        UE_LOG(LogRshipExec, Log, TEXT("SetItem [%s]: %s"), *itemType, *JsonString);
    }

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

    // Emitter pulses are LOW priority and coalesce by emitter ID
    // This means rapid pulses from the same emitter will be coalesced
    SetItem("Pulse", pulse, ERshipMessagePriority::Low, fullEmitterId);

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
