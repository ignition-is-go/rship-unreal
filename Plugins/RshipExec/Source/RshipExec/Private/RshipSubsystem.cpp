// Fill out your copyright notice in the Description page of Project Settings
#include "RshipSubsystem.h"
#include "RshipSettings.h"
#include "WebSocketsModule.h"
#include "EngineUtils.h"
#include "RshipActorRegistrationComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Json.h"
#include "JsonObjectWrapper.h"
#include "JsonObjectConverter.h"
#include "Util.h"
#include "Logging/LogMacros.h"
#include "Subsystems/SubsystemCollection.h"
#include "Algo/Sort.h"
#include "GameFramework/Actor.h"
#include "UObject/FieldPath.h"
#include "Misc/StructBuilder.h"
#include "UObject/UnrealTypePrivate.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Core/Target.h"
#include "Core/RshipEntityRecords.h"
#include "Core/RshipEntitySerializer.h"
#include "Transport/RshipMykoTransport.h"



#include "Logs.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

using namespace std;

namespace
{
FString GetActorDisplayName(const AActor* Actor)
{
	if (!Actor)
	{
		return FString();
	}

#if WITH_EDITOR
	return Actor->GetActorLabel();
#else
	return Actor->GetName();
#endif
}

}

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipSubsystem::Initialize"));

    // Initialize connection state
    ConnectionState = ERshipConnectionState::Disconnected;
    ReconnectAttempts = 0;
    LastTickTime = 0.0;

    // Initialize rate limiter
    InitializeRateLimiter();

    // Connect to server (if globally enabled)
    if (bRemoteCommunicationEnabled)
    {
        Reconnect();
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Remote communication is disabled; skipping initial connect"));
    }

    this->TargetComponents = new TMultiMap<FString, URshipActorRegistrationComponent*>;

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

#if WITH_EDITOR
    RegisterEditorDelegates();
#endif
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
    if (!bRemoteCommunicationEnabled)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Reconnect skipped: remote communication is disabled"));
        ConnectionState = ERshipConnectionState::Disconnected;
        return;
    }
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
    MachineId = FRshipMykoTransport::GetUniqueMachineId();
    ServiceId = FApp::GetProjectName();

    InstanceId = MachineId + ":" + ServiceId;
    ClusterId = ServiceId;

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
    if (DeferredOnDataReceivedTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(DeferredOnDataReceivedTickerHandle);
        DeferredOnDataReceivedTickerHandle.Reset();
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

void URshipSubsystem::SetRemoteCommunicationEnabled(bool bEnabled)
{
    if (bRemoteCommunicationEnabled == bEnabled)
    {
        return;
    }
    bRemoteCommunicationEnabled = bEnabled;
    if (!bRemoteCommunicationEnabled)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Remote communication disabled - disconnecting and stopping all remote traffic"));
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
        if (WebSocket)
        {
            WebSocket->Close();
            WebSocket.Reset();
        }
        ConnectionState = ERshipConnectionState::Disconnected;
        ReconnectAttempts = 0;
        if (RateLimiter)
        {
            RateLimiter->ClearQueue();
        }
    }
    else
    {
        UE_LOG(LogRshipExec, Log, TEXT("Remote communication enabled - reconnecting"));
        ReconnectAttempts = 0;
        Reconnect();
    }

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
        Settings->TryUpdateDefaultConfigFile();  // Also update DefaultGame.ini

        UE_LOG(LogRshipExec, Log, TEXT("Saved server settings to config: %s:%d"), *Host, Port);
    }

    // Force reconnect with new settings when remote communication is enabled
    ReconnectAttempts = 0;
    ConnectionState = ERshipConnectionState::Disconnected;

    if (bRemoteCommunicationEnabled)
    {
        Reconnect();
    }
    else
    {
        UE_LOG(LogRshipExec, Log, TEXT("Saved server settings while remote communication is disabled"));
    }
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
        PingData->SetStringField(TEXT("id"), FRshipMykoTransport::GenerateTransactionId());
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
    if (Settings->bAutoReconnect && bRemoteCommunicationEnabled)
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
    if (Settings->bAutoReconnect && bRemoteCommunicationEnabled && !bWasClean && !bIsManuallyReconnecting)
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
    if (!bRemoteCommunicationEnabled)
    {
        ConnectionState = ERshipConnectionState::Disconnected;
        return;
    }
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
    if (Settings->bAutoReconnect && bRemoteCommunicationEnabled)
    {
        ScheduleReconnect();
    }
}

#if WITH_EDITOR
void URshipSubsystem::RegisterEditorDelegates()
{
    if (!GEditor)
    {
        return;
    }

    if (!BeginPIEHandle.IsValid())
    {
        BeginPIEHandle = FEditorDelegates::BeginPIE.AddUObject(this, &URshipSubsystem::HandleBeginPIE);
    }
    if (!EndPIEHandle.IsValid())
    {
        EndPIEHandle = FEditorDelegates::EndPIE.AddUObject(this, &URshipSubsystem::HandleEndPIE);
    }
    if (!MapChangedHandle.IsValid())
    {
        MapChangedHandle = FEditorDelegates::MapChange.AddUObject(this, &URshipSubsystem::HandleMapChanged);
    }
    if (!BlueprintCompiledHandle.IsValid())
    {
        BlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddUObject(this, &URshipSubsystem::HandleBlueprintCompiled);
    }

    UE_LOG(LogRshipExec, Log, TEXT("Registered editor delegates for target refresh."));
}

void URshipSubsystem::UnregisterEditorDelegates()
{
    if (!GEditor)
    {
        return;
    }

    if (BeginPIEHandle.IsValid())
    {
        FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
        BeginPIEHandle.Reset();
    }
    if (EndPIEHandle.IsValid())
    {
        FEditorDelegates::EndPIE.Remove(EndPIEHandle);
        EndPIEHandle.Reset();
    }
    if (MapChangedHandle.IsValid())
    {
        FEditorDelegates::MapChange.Remove(MapChangedHandle);
        MapChangedHandle.Reset();
    }
    if (BlueprintCompiledHandle.IsValid())
    {
        GEditor->OnBlueprintCompiled().Remove(BlueprintCompiledHandle);
        BlueprintCompiledHandle.Reset();
    }
}

void URshipSubsystem::RefreshAllTargetComponents(const TCHAR* Reason)
{
    if (!TargetComponents)
    {
        UE_LOG(LogRshipExec, Error, TEXT("RefreshAllTargetComponents failed: TargetComponents not initialized (reason=%s)"), Reason);
        return;
    }

    TSet<URshipActorRegistrationComponent*> UniqueComponents;
    TArray<URshipActorRegistrationComponent*> ToRefresh;
    int32 Removed = 0;

    for (auto It = TargetComponents->CreateIterator(); It; ++It)
    {
        URshipActorRegistrationComponent* Component = It.Value();
        if (!IsValid(Component))
        {
            It.RemoveCurrent();
            ++Removed;
            continue;
        }
        if (!UniqueComponents.Contains(Component))
        {
            UniqueComponents.Add(Component);
            ToRefresh.Add(Component);
        }
    }

    int32 Refreshed = 0;
    for (URshipActorRegistrationComponent* Component : ToRefresh)
    {
        if (!IsValid(Component))
        {
            continue;
        }
        Component->Register();
        ++Refreshed;
    }

    UE_LOG(LogRshipExec, Log, TEXT("RefreshAllTargetComponents: reason=%s refreshed=%d removed=%d tracked=%d"),
        Reason, Refreshed, Removed, TargetComponents->Num());
}

void URshipSubsystem::HandleBeginPIE(bool bIsSimulating)
{
    RefreshAllTargetComponents(bIsSimulating ? TEXT("BeginSimulate") : TEXT("BeginPIE"));
}

void URshipSubsystem::HandleEndPIE(bool bIsSimulating)
{
    RefreshAllTargetComponents(bIsSimulating ? TEXT("EndSimulate") : TEXT("EndPIE"));
}

void URshipSubsystem::HandleMapChanged(uint32 MapChangeFlags)
{
    RefreshAllTargetComponents(TEXT("MapChanged"));
}

void URshipSubsystem::HandleBlueprintCompiled()
{
    RefreshAllTargetComponents(TEXT("BlueprintCompiled"));
}
#endif

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
        UE_LOG(LogRshipExec, VeryVerbose, TEXT("ProcessMessageQueue: Queue has %d messages, processing..."), QueueSize);
    }

    int32 Sent = RateLimiter->ProcessQueue();

    if (Sent > 0 || QueueSize > 0)
    {
        UE_LOG(LogRshipExec, VeryVerbose, TEXT("ProcessMessageQueue: Sent %d messages, %d remaining"), Sent, RateLimiter->GetQueueLength());
    }
}

void URshipSubsystem::TickSubsystems()
{
    // Calculate delta time
    double CurrentTime = FPlatformTime::Seconds();
    float DeltaTime = (LastTickTime > 0.0) ? (float)(CurrentTime - LastTickTime) : 0.0f;
    LastTickTime = CurrentTime;

    // Apply coalesced target actions once per frame.
    ProcessPendingExecTargetActions();

    // Fire OnRshipData once per target/component at end-of-frame for all successful Take() calls.
    FlushPendingOnDataReceived();

    // Process message queue every tick to ensure messages are sent
    ProcessMessageQueue();
}

bool URshipSubsystem::OnDeferredOnDataReceivedTick(float DeltaTime)
{
    if (!IsValid(this))
    {
        return false;
    }

    FlushPendingOnDataReceived();
    DeferredOnDataReceivedTickerHandle.Reset();
    return false; // one-shot
}

void URshipSubsystem::QueueOnDataReceived(URshipActorRegistrationComponent* Component)
{
    if (Component)
    {
        PendingOnDataReceivedComponents.Add(Component);

        // Ensure this also fires in editor paths even if subsystem tick is temporarily idle.
        if (!DeferredOnDataReceivedTickerHandle.IsValid())
        {
            DeferredOnDataReceivedTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnDeferredOnDataReceivedTick),
                0.0f
            );
        }
    }
}

void URshipSubsystem::FlushPendingOnDataReceived()
{
    if (PendingOnDataReceivedComponents.Num() == 0)
    {
        return;
    }

    for (const TWeakObjectPtr<URshipActorRegistrationComponent>& WeakComp : PendingOnDataReceivedComponents)
    {
        if (URshipActorRegistrationComponent* Comp = WeakComp.Get())
        {
            Comp->OnDataReceived();
        }
    }
    PendingOnDataReceivedComponents.Reset();
}

void URshipSubsystem::EnqueueExecTargetAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data, const FString& TxId)
{
    const FString Key = TargetId + TEXT("|") + ActionId;
    FRshipPendingExecTargetAction& Pending = PendingExecTargetActions.FindOrAdd(Key);
    Pending.TargetId = TargetId;
    Pending.ActionId = ActionId;
    Pending.Data = Data;
    Pending.TxIds.Add(TxId);
}

void URshipSubsystem::EnqueueBatchTargetAction(const FString& TxId, TArray<FRshipPendingBatchActionItem>&& Actions)
{
    if (TxId.IsEmpty())
    {
        UE_LOG(LogRshipExec, Error, TEXT("BatchTargetAction enqueue failed: missing tx id."));
        return;
    }

    if (Actions.Num() == 0)
    {
        UE_LOG(LogRshipExec, Error, TEXT("BatchTargetAction enqueue failed: no actions for tx '%s'."), *TxId);
        return;
    }

    FRshipPendingBatchTargetAction Pending;
    Pending.TxId = TxId;
    Pending.Actions = MoveTemp(Actions);
    PendingBatchTargetActions.Add(MoveTemp(Pending));
}

void URshipSubsystem::QueueCommandResponse(const FString& TxId, bool bOk, const FString& CommandId, const FString& ErrorMessage)
{
    if (bOk)
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("Command success: commandId=%s tx=%s"), *CommandId, *TxId);

        TSharedPtr<FJsonObject> ResponseData = MakeShareable(new FJsonObject);
        ResponseData->SetStringField(TEXT("tx"), TxId);
        TSharedPtr<FJsonObject> CommandResponse = MakeShareable(new FJsonObject);
        CommandResponse->SetBoolField(TEXT("ok"), true);
        ResponseData->SetObjectField(TEXT("response"), CommandResponse);

        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
        Response->SetStringField(TEXT("event"), TEXT("ws:m:command-response"));
        Response->SetObjectField(TEXT("data"), ResponseData);

        QueueMessage(Response, ERshipMessagePriority::Critical, ERshipMessageType::CommandResponse);
        return;
    }

    UE_LOG(LogRshipExec, Error, TEXT("Command failure: commandId=%s tx=%s error=%s"),
        *CommandId,
        *TxId,
        ErrorMessage.IsEmpty() ? TEXT("Action was not handled by any target") : *ErrorMessage);

    TSharedPtr<FJsonObject> ResponseData = MakeShareable(new FJsonObject);
    ResponseData->SetStringField(TEXT("tx"), TxId);
    ResponseData->SetStringField(TEXT("commandId"), CommandId);
    ResponseData->SetStringField(TEXT("message"), ErrorMessage.IsEmpty() ? TEXT("Action was not handled by any target") : ErrorMessage);

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetStringField(TEXT("event"), TEXT("ws:m:command-error"));
    Response->SetObjectField(TEXT("data"), ResponseData);

    QueueMessage(Response, ERshipMessagePriority::Critical, ERshipMessageType::CommandResponse);
}

void URshipSubsystem::ProcessPendingExecTargetActions()
{
    const bool bHasSingleActions = PendingExecTargetActions.Num() > 0;
    const bool bHasBatchActions = PendingBatchTargetActions.Num() > 0;
    if (!bHasSingleActions && !bHasBatchActions)
    {
        return;
    }

    TMap<FString, FRshipPendingExecTargetAction> PendingSingleBatch = MoveTemp(PendingExecTargetActions);
    PendingExecTargetActions.Reset();
    TArray<FRshipPendingBatchTargetAction> PendingBatchCommands = MoveTemp(PendingBatchTargetActions);
    PendingBatchTargetActions.Reset();

    for (TPair<FString, FRshipPendingExecTargetAction>& Pair : PendingSingleBatch)
    {
        FRshipPendingExecTargetAction& Pending = Pair.Value;
        if (!Pending.Data.IsValid())
        {
            for (const FString& TxId : Pending.TxIds)
            {
                QueueCommandResponse(TxId, false, TEXT("ExecTargetAction"), TEXT("Action payload missing"));
            }
            continue;
        }

        const bool bResult = ExecuteTargetAction(Pending.TargetId, Pending.ActionId, Pending.Data.ToSharedRef());
        for (const FString& TxId : Pending.TxIds)
        {
            QueueCommandResponse(TxId, bResult, TEXT("ExecTargetAction"), TEXT("Action was not handled by any target"));
        }
    }

    for (FRshipPendingBatchTargetAction& PendingBatchCommand : PendingBatchCommands)
    {
        if (PendingBatchCommand.TxId.IsEmpty())
        {
            UE_LOG(LogRshipExec, Error, TEXT("BatchTargetAction processing skipped: empty tx id."));
            continue;
        }

        if (PendingBatchCommand.Actions.Num() == 0)
        {
            QueueCommandResponse(PendingBatchCommand.TxId, false, TEXT("BatchTargetAction"), TEXT("BatchTargetAction has no actions"));
            continue;
        }

        bool bAllSucceeded = true;
        int32 FailedIndex = INDEX_NONE;
        FString FailedReason;

        for (int32 ActionIndex = 0; ActionIndex < PendingBatchCommand.Actions.Num(); ++ActionIndex)
        {
            FRshipPendingBatchActionItem& ActionItem = PendingBatchCommand.Actions[ActionIndex];
            if (!ActionItem.Data.IsValid())
            {
                bAllSucceeded = false;
                FailedIndex = ActionIndex;
                FailedReason = TEXT("Action payload missing");
                UE_LOG(LogRshipExec, Error, TEXT("BatchTargetAction tx=%s failed at index=%d: payload missing."),
                    *PendingBatchCommand.TxId, ActionIndex);
                break;
            }

            const bool bResult = ExecuteTargetAction(ActionItem.TargetId, ActionItem.ActionId, ActionItem.Data.ToSharedRef());
            if (!bResult)
            {
                bAllSucceeded = false;
                FailedIndex = ActionIndex;
                FailedReason = FString::Printf(TEXT("Action was not handled by any target (index=%d target=%s action=%s)"),
                    ActionIndex, *ActionItem.TargetId, *ActionItem.ActionId);
                UE_LOG(LogRshipExec, Error, TEXT("BatchTargetAction tx=%s failed at index=%d target=%s action=%s."),
                    *PendingBatchCommand.TxId, ActionIndex, *ActionItem.TargetId, *ActionItem.ActionId);
                break;
            }
        }

        if (bAllSucceeded)
        {
            QueueCommandResponse(PendingBatchCommand.TxId, true, TEXT("BatchTargetAction"));
        }
        else
        {
            const FString ErrorMessage = FailedIndex == INDEX_NONE
                ? TEXT("BatchTargetAction failed")
                : FString::Printf(TEXT("BatchTargetAction failed at index %d: %s"), FailedIndex, *FailedReason);
            QueueCommandResponse(PendingBatchCommand.TxId, false, TEXT("BatchTargetAction"), ErrorMessage);
        }
    }
}
void URshipSubsystem::QueueMessage(TSharedPtr<FJsonObject> Payload, ERshipMessagePriority Priority,
                                    ERshipMessageType Type, const FString& CoalesceKey)
{
    if (!bRemoteCommunicationEnabled)
    {
        return;
    }
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
        UE_LOG(LogRshipExec, VeryVerbose, TEXT("Enqueued message (Key=%s, QueueLen=%d)"), *CoalesceKey, RateLimiter->GetQueueLength());
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
    if (!bRemoteCommunicationEnabled)
    {
        return;
    }
    bool bConnected = WebSocket.IsValid() && WebSocket->IsConnected();

    if (!bConnected)
    {
        // Don't spam reconnect attempts - let the scheduled reconnect handle it
        if (ConnectionState == ERshipConnectionState::Disconnected)
        {
            const URshipSettings *Settings = GetDefault<URshipSettings>();
            if (Settings->bAutoReconnect && bRemoteCommunicationEnabled && !ReconnectTickerHandle.IsValid())
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
    UE_LOG(LogRshipExec, VeryVerbose, TEXT("Received message: event=%s"), *type);

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
        const TSharedPtr<FJsonObject>* DataPtr = nullptr;
        if (!obj->TryGetObjectField(TEXT("data"), DataPtr) || DataPtr == nullptr || !DataPtr->IsValid())
        {
            UE_LOG(LogRshipExec, Error, TEXT("Command rejected: missing object field 'data'."));
            return;
        }

        const TSharedPtr<FJsonObject>& DataObj = *DataPtr;
        FString CommandId;
        if (!DataObj->TryGetStringField(TEXT("commandId"), CommandId) || CommandId.IsEmpty())
        {
            UE_LOG(LogRshipExec, Error, TEXT("Command rejected: missing string field 'data.commandId'."));
            return;
        }

        const TSharedPtr<FJsonObject>* CommandPtr = nullptr;
        if (!DataObj->TryGetObjectField(TEXT("command"), CommandPtr) || CommandPtr == nullptr || !CommandPtr->IsValid())
        {
            UE_LOG(LogRshipExec, Error, TEXT("Command '%s' rejected: missing object field 'data.command'."), *CommandId);
            return;
        }

        const TSharedPtr<FJsonObject>& CommandObj = *CommandPtr;
        FString TxId;
        CommandObj->TryGetStringField(TEXT("tx"), TxId);
        if (TxId.IsEmpty())
        {
            UE_LOG(LogRshipExec, Warning, TEXT("Command '%s' missing tx id; response correlation may fail."), *CommandId);
        }

        UE_LOG(LogRshipExec, Verbose, TEXT("Received command: commandId=%s tx=%s"), *CommandId, *TxId);

        if (CommandId == "SetClientId")
        {
            FString NewClientId;
            if (!CommandObj->TryGetStringField(TEXT("clientId"), NewClientId) || NewClientId.IsEmpty())
            {
                QueueCommandResponse(TxId, false, CommandId, TEXT("Missing clientId"));
                return;
            }

            ClientId = NewClientId;
            UE_LOG(LogRshipExec, Warning, TEXT("Received ClientId %s"), *ClientId);
            SendAll();
            return;
        }

        if (CommandId == "ExecTargetAction")
        {
            const TSharedPtr<FJsonObject>* ExecActionPtr = nullptr;
            const TSharedPtr<FJsonObject>* ExecDataPtr = nullptr;
            if (!CommandObj->TryGetObjectField(TEXT("action"), ExecActionPtr) || ExecActionPtr == nullptr || !ExecActionPtr->IsValid())
            {
                QueueCommandResponse(TxId, false, CommandId, TEXT("Missing action object"));
                return;
            }
            if (!CommandObj->TryGetObjectField(TEXT("data"), ExecDataPtr) || ExecDataPtr == nullptr || !ExecDataPtr->IsValid())
            {
                QueueCommandResponse(TxId, false, CommandId, TEXT("Missing data object"));
                return;
            }

            FString ActionId;
            FString TargetId;
            if (!(*ExecActionPtr)->TryGetStringField(TEXT("id"), ActionId) || ActionId.IsEmpty())
            {
                QueueCommandResponse(TxId, false, CommandId, TEXT("Missing action.id"));
                return;
            }
            if (!(*ExecActionPtr)->TryGetStringField(TEXT("targetId"), TargetId) || TargetId.IsEmpty())
            {
                QueueCommandResponse(TxId, false, CommandId, TEXT("Missing action.targetId"));
                return;
            }

            EnqueueExecTargetAction(TargetId, ActionId, (*ExecDataPtr).ToSharedRef(), TxId);
        }
        else if (CommandId == "BatchTargetAction")
        {
            const TArray<TSharedPtr<FJsonValue>>* ActionsArray = nullptr;
            if (!CommandObj->TryGetArrayField(TEXT("actions"), ActionsArray) || ActionsArray == nullptr)
            {
                UE_LOG(LogRshipExec, Error, TEXT("BatchTargetAction rejected: missing 'actions' array."));
                QueueCommandResponse(TxId, false, TEXT("BatchTargetAction"), TEXT("Missing actions array"));
                return;
            }

            TArray<FRshipPendingBatchActionItem> ParsedActions;
            ParsedActions.Reserve(ActionsArray->Num());

            bool bParseFailed = false;
            FString ParseError = TEXT("Invalid actions payload");

            for (int32 ActionIndex = 0; ActionIndex < ActionsArray->Num(); ++ActionIndex)
            {
                const TSharedPtr<FJsonValue>& ActionValue = (*ActionsArray)[ActionIndex];
                if (!ActionValue.IsValid() || ActionValue->Type != EJson::Object)
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Action at index %d is not an object"), ActionIndex);
                    break;
                }

                TSharedPtr<FJsonObject> ActionObj = ActionValue->AsObject();
                if (!ActionObj.IsValid())
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Action at index %d is null"), ActionIndex);
                    break;
                }

                const TSharedPtr<FJsonObject>* ExecActionPtr = nullptr;
                const TSharedPtr<FJsonObject>* ExecDataPtr = nullptr;
                if (!ActionObj->TryGetObjectField(TEXT("action"), ExecActionPtr) || ExecActionPtr == nullptr || !ExecActionPtr->IsValid())
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Action at index %d missing object field 'action'"), ActionIndex);
                    break;
                }
                if (!ActionObj->TryGetObjectField(TEXT("data"), ExecDataPtr) || ExecDataPtr == nullptr || !ExecDataPtr->IsValid())
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Action at index %d missing object field 'data'"), ActionIndex);
                    break;
                }

                FString ActionId;
                FString TargetId;
                if (!(*ExecActionPtr)->TryGetStringField(TEXT("id"), ActionId) || ActionId.IsEmpty())
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Action at index %d missing string field 'action.id'"), ActionIndex);
                    break;
                }
                if (!(*ExecActionPtr)->TryGetStringField(TEXT("targetId"), TargetId) || TargetId.IsEmpty())
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Action at index %d missing string field 'action.targetId'"), ActionIndex);
                    break;
                }

                FRshipPendingBatchActionItem Parsed;
                Parsed.TargetId = TargetId;
                Parsed.ActionId = ActionId;
                Parsed.Data = *ExecDataPtr;
                ParsedActions.Add(MoveTemp(Parsed));
            }

            if (bParseFailed)
            {
                UE_LOG(LogRshipExec, Error, TEXT("BatchTargetAction rejected: %s (tx=%s)."), *ParseError, *TxId);
                QueueCommandResponse(TxId, false, TEXT("BatchTargetAction"), ParseError);
                return;
            }

            if (ParsedActions.Num() == 0)
            {
                UE_LOG(LogRshipExec, Error, TEXT("BatchTargetAction rejected: empty actions list (tx=%s)."), *TxId);
                QueueCommandResponse(TxId, false, TEXT("BatchTargetAction"), TEXT("BatchTargetAction has no actions"));
                return;
            }

            EnqueueBatchTargetAction(TxId, MoveTemp(ParsedActions));
        }
        else
        {
            UE_LOG(LogRshipExec, Error, TEXT("Unsupported commandId '%s' (tx=%s)."), *CommandId, *TxId);
            QueueCommandResponse(TxId, false, CommandId, FString::Printf(TEXT("Unsupported commandId '%s'"), *CommandId));
            return;
        }

        obj.Reset();
    }
    auto ProcessEntityEvent = [this](const TSharedPtr<FJsonObject>& data) -> void
    {
        if (!data.IsValid())
        {
            return;
        }

        FString changeType;
        FString itemType;
        const TSharedPtr<FJsonObject>* itemPtr = nullptr;

        if (!data->TryGetStringField(TEXT("changeType"), changeType) ||
            !data->TryGetStringField(TEXT("itemType"), itemType) ||
            !data->TryGetObjectField(TEXT("item"), itemPtr) ||
            itemPtr == nullptr ||
            !itemPtr->IsValid())
        {
            UE_LOG(LogRshipExec, Verbose, TEXT("Ignoring malformed ws:m:event payload"));
            return;
        }

        TSharedPtr<FJsonObject> item = *itemPtr;
        bool bIsDelete = (changeType == TEXT("DEL"));
        UE_LOG(LogRshipExec, Log, TEXT("Entity event: %s %s"), *changeType, *itemType);
        (void)item;
        (void)bIsDelete;
    };

    if (type == "ws:m:event")
    {
        // Myko protocol: { event: "ws:m:event", data: MEvent }
        const TSharedPtr<FJsonObject>* dataPtr = nullptr;
        if (obj->TryGetObjectField(TEXT("data"), dataPtr) &&
            dataPtr != nullptr &&
            dataPtr->IsValid())
        {
            ProcessEntityEvent(*dataPtr);
        }
    }
    else if (type == RshipMykoEventNames::EventBatch)
    {
        // Myko protocol: { event: "ws:m:event-batch", data: MEvent[] }
        const TArray<TSharedPtr<FJsonValue>>* events = nullptr;
        if (obj->TryGetArrayField(TEXT("data"), events) && events != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& eventValue : *events)
            {
                if (!eventValue.IsValid() || eventValue->Type != EJson::Object)
                {
                    continue;
                }

                TSharedPtr<FJsonObject> eventObj = eventValue->AsObject();
                ProcessEntityEvent(eventObj);
            }
        }
    }
}

bool URshipSubsystem::ExecuteTargetAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data)
{
    if (RegisteredTargetsById.Num() == 0)
    {
        return false;
    }

    PruneInvalidManagedTargetRefs(TargetId);

    bool bFoundAny = false;
    bool bResult = false;

    // Iterate key matches directly from subsystem-owned target registry.
    for (auto It = RegisteredTargetsById.CreateKeyIterator(TargetId); It; ++It)
    {
        Target* RegisteredTarget = It.Value();
        if (!RegisteredTarget)
        {
            continue;
        }

        AActor* Owner = nullptr;
        if (URshipActorRegistrationComponent* BoundComp = RegisteredTarget->GetBoundTargetComponent())
        {
            Owner = BoundComp->GetOwner();
        }

        bFoundAny = true;

#if WITH_EDITOR
        UWorld* World = Owner ? Owner->GetWorld() : nullptr;
        if (World && World->WorldType == EWorldType::Editor)
        {
            FEditorScriptExecutionGuard ScriptGuard;
            bResult |= RegisteredTarget->TakeAction(Owner, ActionId, Data);
            continue;
        }
#endif

        bResult |= RegisteredTarget->TakeAction(Owner, ActionId, Data);
    }

    if (!bFoundAny)
    {
        UE_LOG(LogRshipExec, Error, TEXT("Target not found: %s (action=%s)"), *TargetId, *ActionId);
    }
    else if (!bResult)
    {
        UE_LOG(LogRshipExec, Error, TEXT("Target action failed: target=%s action=%s"), *TargetId, *ActionId);
    }

    return bResult;
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
    if (DeferredOnDataReceivedTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(DeferredOnDataReceivedTickerHandle);
        DeferredOnDataReceivedTickerHandle.Reset();
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

    ManagedTargetSnapshots.Reset();
    RegisteredTargetsById.Reset();

#if WITH_EDITOR
    UnregisterEditorDelegates();
#endif

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
    if (DeferredOnDataReceivedTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(DeferredOnDataReceivedTickerHandle);
        DeferredOnDataReceivedTickerHandle.Reset();
    }

    // Clean up WebSocket connection without callbacks (object is being destroyed)
    if (WebSocket.IsValid())
    {
        // Don't call Close() as it may trigger callbacks - just reset
        WebSocket.Reset();
    }

#if WITH_EDITOR
    UnregisterEditorDelegates();
#endif

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
    if (DeferredOnDataReceivedTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(DeferredOnDataReceivedTickerHandle);
        DeferredOnDataReceivedTickerHandle.Reset();
    }

#if WITH_EDITOR
    UnregisterEditorDelegates();
#endif

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

    TArray<FString> EmitterIds;
    TArray<FString> ActionIds;
    TArray<TSharedPtr<FJsonObject>> BatchEvents;
    BatchEvents.Reserve(target->GetActions().Num() + target->GetEmitters().Num() + 2);

    for (auto &Elem : target->GetActions())
    {
        UE_LOG(LogRshipExec, Log, TEXT("  Action: %s"), *Elem.Key);
        ActionIds.Add(Elem.Key);

        FRshipActionRecord Record;
        Record.Id = Elem.Value.Id;
        Record.Name = Elem.Value.Name;
        Record.TargetId = target->GetId();
        Record.ServiceId = ServiceId;
        Record.Schema = Elem.Value.GetSchema();
        Record.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

        BatchEvents.Add(FRshipMykoTransport::MakeSet("Action", FRshipEntitySerializer::ToJson(Record), MachineId));
    }

    for (auto &Elem : target->GetEmitters())
    {
        UE_LOG(LogRshipExec, Log, TEXT("  Emitter: %s"), *Elem.Key);
        EmitterIds.Add(Elem.Key);

        FRshipEmitterRecord Record;
        Record.Id = Elem.Value.Id;
        Record.Name = Elem.Value.Name;
        Record.TargetId = target->GetId();
        Record.ServiceId = ServiceId;
        Record.Schema = Elem.Value.GetSchema();
        Record.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

        BatchEvents.Add(FRshipMykoTransport::MakeSet("Emitter", FRshipEntitySerializer::ToJson(Record), MachineId));
    }

    const URshipSettings *Settings = GetDefault<URshipSettings>();

    FColor SRGBColor = Settings->ServiceColor.ToFColor(true);
    FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);
    FRshipTargetRecord TargetRecord;
    TargetRecord.Id = target->GetId();
    TargetRecord.Name = target->GetName();
    TargetRecord.ServiceId = ServiceId;
    TargetRecord.Category = TEXT("default");
    TargetRecord.ForegroundColor = ColorHex;
    TargetRecord.BackgroundColor = ColorHex;
    TargetRecord.ActionIds = MoveTemp(ActionIds);
    TargetRecord.EmitterIds = MoveTemp(EmitterIds);
    TargetRecord.ParentTargetIds = target->GetParentTargetIds();
    TargetRecord.bRootLevel = TargetRecord.ParentTargetIds.Num() == 0;
    TargetRecord.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

    // Add tags and groups from bound registration component (if present).
    if (URshipActorRegistrationComponent* TargetComp = target->GetBoundTargetComponent())
    {
        TargetRecord.Category = TargetComp->Category.IsEmpty() ? TEXT("default") : TargetComp->Category;
        TargetRecord.Tags = TargetComp->Tags;
        TargetRecord.GroupIds = TargetComp->GroupIds;
    }

    TSharedPtr<FJsonObject> TargetJson = FRshipEntitySerializer::ToJson(TargetRecord);

    // Target registration - batched with actions/emitters/status
    BatchEvents.Add(FRshipMykoTransport::MakeSet("Target", TargetJson, MachineId));

    FRshipTargetStatusRecord TargetStatusRecord;
    TargetStatusRecord.TargetId = target->GetId();
    TargetStatusRecord.InstanceId = InstanceId;
    TargetStatusRecord.Status = TEXT("online");
    TargetStatusRecord.Id = target->GetId();
    TargetStatusRecord.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    BatchEvents.Add(FRshipMykoTransport::MakeSet("TargetStatus", FRshipEntitySerializer::ToJson(TargetStatusRecord), MachineId));

    if (RegistrationBatchDepth > 0)
    {
        PendingRegistrationEvents.Append(BatchEvents);
    }
    else
    {
        QueueEventBatch(BatchEvents, ERshipMessagePriority::High, ERshipMessageType::Registration, target->GetId());
    }
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
    FRshipTargetStatusRecord Record;
    Record.TargetId = target->GetId();
    Record.InstanceId = InstanceId;
    Record.Status = TEXT("offline");
    Record.Id = target->GetId();
    Record.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    SetItem("TargetStatus", FRshipEntitySerializer::ToJson(Record), ERshipMessagePriority::High, target->GetId() + ":status");

    UE_LOG(LogRshipExec, Log, TEXT("DeleteTarget: %s - offline status sent"), *target->GetId());
}

void URshipSubsystem::SendAction(const FRshipActionProxy& action, FString targetId)
{
    UE_LOG(LogRshipExec, Log, TEXT("SendAction: id=%s target=%s"), *action.Id, *targetId);

    FRshipActionRecord Record;
    Record.Id = action.Id;
    Record.Name = action.Name;
    Record.TargetId = targetId;
    Record.ServiceId = ServiceId;
    Record.Schema = action.GetSchema();
    Record.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

    // Action registration - HIGH priority, coalesce by action ID
    SetItem("Action", FRshipEntitySerializer::ToJson(Record), ERshipMessagePriority::High, action.Id);
}

void URshipSubsystem::SendEmitter(const FRshipEmitterProxy& emitter, FString targetId)
{
    UE_LOG(LogRshipExec, Log, TEXT("SendEmitter: id=%s target=%s"), *emitter.Id, *targetId);

    FRshipEmitterRecord Record;
    Record.Id = emitter.Id;
    Record.Name = emitter.Name;
    Record.TargetId = targetId;
    Record.ServiceId = ServiceId;
    Record.Schema = emitter.GetSchema();
    Record.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

    // Emitter registration - HIGH priority, coalesce by emitter ID
    SetItem("Emitter", FRshipEntitySerializer::ToJson(Record), ERshipMessagePriority::High, emitter.Id);
}

void URshipSubsystem::QueueEventBatch(const TArray<TSharedPtr<FJsonObject>>& Events,
                                      ERshipMessagePriority Priority,
                                      ERshipMessageType Type,
                                      const FString& CoalesceKey)
{
    if (Events.Num() == 0)
    {
        return;
    }

    TArray<TSharedPtr<FJsonValue>> PayloadArray;
    PayloadArray.Reserve(Events.Num());

    for (const TSharedPtr<FJsonObject>& EventEnvelope : Events)
    {
        if (!EventEnvelope.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> EventData;
        if (!FRshipMykoTransport::TryGetMykoEventData(EventEnvelope, EventData))
        {
            UE_LOG(LogRshipExec, Error, TEXT("QueueEventBatch received non-Myko payload; dropping from batch."));
            continue;
        }

        PayloadArray.Add(MakeShareable(new FJsonValueObject(EventData)));
    }

    if (PayloadArray.Num() == 0)
    {
        UE_LOG(LogRshipExec, Error, TEXT("QueueEventBatch produced empty payload."));
        return;
    }

    TSharedPtr<FJsonObject> BatchWrapper = MakeShareable(new FJsonObject);
    BatchWrapper->SetStringField(TEXT("event"), RshipMykoEventNames::EventBatch);
    BatchWrapper->SetArrayField(TEXT("data"), PayloadArray);

    QueueMessage(BatchWrapper, Priority, Type, CoalesceKey);
}

void URshipSubsystem::BeginRegistrationBatch()
{
    ++RegistrationBatchDepth;
}

void URshipSubsystem::EndRegistrationBatch()
{
    if (RegistrationBatchDepth <= 0)
    {
        RegistrationBatchDepth = 0;
        return;
    }

    --RegistrationBatchDepth;
    if (RegistrationBatchDepth > 0)
    {
        return;
    }

    if (PendingRegistrationEvents.Num() == 0)
    {
        return;
    }

    struct FTargetEvent
    {
        int32 Depth = 0;
        TSharedPtr<FJsonObject> Data;
    };

    TArray<FTargetEvent> TargetEvents;
    TArray<TSharedPtr<FJsonObject>> ActionEvents;
    TArray<TSharedPtr<FJsonObject>> EmitterEvents;
    TArray<TSharedPtr<FJsonObject>> StatusEvents;
    TArray<TSharedPtr<FJsonObject>> OtherEvents;

    TargetEvents.Reserve(PendingRegistrationEvents.Num());

    for (const TSharedPtr<FJsonObject>& Envelope : PendingRegistrationEvents)
    {
        if (!Envelope.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> EventData;
        if (!FRshipMykoTransport::TryGetMykoEventData(Envelope, EventData))
        {
            continue;
        }

        FString ItemType;
        if (!EventData->TryGetStringField(TEXT("itemType"), ItemType))
        {
            OtherEvents.Add(EventData);
            continue;
        }

        if (ItemType == TEXT("Target"))
        {
            int32 Depth = 0;
            const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
            if (EventData->TryGetObjectField(TEXT("item"), ItemPtr) && ItemPtr && ItemPtr->IsValid())
            {
                const TArray<TSharedPtr<FJsonValue>>* ParentIds = nullptr;
                if ((*ItemPtr)->TryGetArrayField(TEXT("parentTargetIds"), ParentIds) && ParentIds)
                {
                    Depth = ParentIds->Num();
                }
            }
            TargetEvents.Add({ Depth, EventData });
        }
        else if (ItemType == TEXT("Action"))
        {
            ActionEvents.Add(EventData);
        }
        else if (ItemType == TEXT("Emitter"))
        {
            EmitterEvents.Add(EventData);
        }
        else if (ItemType == TEXT("TargetStatus"))
        {
            StatusEvents.Add(EventData);
        }
        else
        {
            OtherEvents.Add(EventData);
        }
    }

    Algo::StableSort(TargetEvents, [](const FTargetEvent& A, const FTargetEvent& B)
    {
        return A.Depth < B.Depth;
    });

    TArray<TSharedPtr<FJsonValue>> PayloadArray;
    PayloadArray.Reserve(TargetEvents.Num() + ActionEvents.Num() + EmitterEvents.Num() + StatusEvents.Num() + OtherEvents.Num());

    for (const FTargetEvent& Event : TargetEvents)
    {
        if (Event.Data.IsValid())
        {
            PayloadArray.Add(MakeShareable(new FJsonValueObject(Event.Data)));
        }
    }
    for (const TSharedPtr<FJsonObject>& Event : ActionEvents)
    {
        PayloadArray.Add(MakeShareable(new FJsonValueObject(Event)));
    }
    for (const TSharedPtr<FJsonObject>& Event : EmitterEvents)
    {
        PayloadArray.Add(MakeShareable(new FJsonValueObject(Event)));
    }
    for (const TSharedPtr<FJsonObject>& Event : StatusEvents)
    {
        PayloadArray.Add(MakeShareable(new FJsonValueObject(Event)));
    }
    for (const TSharedPtr<FJsonObject>& Event : OtherEvents)
    {
        PayloadArray.Add(MakeShareable(new FJsonValueObject(Event)));
    }

    PendingRegistrationEvents.Reset();

    if (PayloadArray.Num() == 0)
    {
        UE_LOG(LogRshipExec, Error, TEXT("EndRegistrationBatch produced empty payload."));
        return;
    }

    TSharedPtr<FJsonObject> BatchWrapper = MakeShareable(new FJsonObject);
    BatchWrapper->SetStringField(TEXT("event"), RshipMykoEventNames::EventBatch);
    BatchWrapper->SetArrayField(TEXT("data"), PayloadArray);

    QueueMessage(BatchWrapper, ERshipMessagePriority::High, ERshipMessageType::Registration, TEXT(""));
}

void URshipSubsystem::SendTargetStatus(Target *target, bool online)
{
    if (!target) return;

    FRshipTargetStatusRecord Record;
    Record.TargetId = target->GetId();
    Record.InstanceId = InstanceId;
    Record.Status = online ? TEXT("online") : TEXT("offline");
    Record.Id = target->GetId();
    Record.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

    SetItem("TargetStatus", FRshipEntitySerializer::ToJson(Record), ERshipMessagePriority::High, target->GetId() + TEXT(":status"));

    UE_LOG(LogRshipExec, Log, TEXT("Sent target status: %s = %s"), *target->GetId(), online ? TEXT("online") : TEXT("offline"));
}

URshipSubsystem::FManagedTargetSnapshot URshipSubsystem::BuildManagedTargetSnapshot(Target* ManagedTarget) const
{
    FManagedTargetSnapshot Snapshot;
    if (!ManagedTarget)
    {
        return Snapshot;
    }

    Snapshot.Id = ManagedTarget->GetId();
    Snapshot.Name = ManagedTarget->GetName();
    Snapshot.ParentTargetIds = ManagedTarget->GetParentTargetIds();

    for (const auto& ActionPair : ManagedTarget->GetActions())
    {
        Snapshot.ActionIds.Add(ActionPair.Key);
    }

    for (const auto& EmitterPair : ManagedTarget->GetEmitters())
    {
        Snapshot.EmitterIds.Add(EmitterPair.Key);
    }

    URshipActorRegistrationComponent* BoundComponent = ManagedTarget->GetBoundTargetComponent();
    Snapshot.BoundTargetComponent = BoundComponent;
    Snapshot.bBoundToComponent = BoundComponent != nullptr;

    return Snapshot;
}

int32 URshipSubsystem::PruneInvalidManagedTargetRefs(const FString& TargetId)
{
    if (TargetId.IsEmpty())
    {
        return 0;
    }

    int32 RemovedCount = 0;
    for (auto It = RegisteredTargetsById.CreateKeyIterator(TargetId); It; ++It)
    {
        Target* Candidate = It.Value();
        const FManagedTargetSnapshot* Snapshot = Candidate ? ManagedTargetSnapshots.Find(Candidate) : nullptr;

        bool bRemove = (Candidate == nullptr || Snapshot == nullptr);
        if (!bRemove && Snapshot->bBoundToComponent)
        {
            URshipActorRegistrationComponent* BoundComponent = Snapshot->BoundTargetComponent.Get();
            AActor* Owner = BoundComponent ? BoundComponent->GetOwner() : nullptr;
            bRemove = !IsValid(BoundComponent) || !IsValid(Owner) || Owner->IsActorBeingDestroyed();
        }

        if (bRemove)
        {
            if (Candidate)
            {
                ManagedTargetSnapshots.Remove(Candidate);
            }

            It.RemoveCurrent();
            ++RemovedCount;
        }
    }

    if (RemovedCount > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Pruned stale target refs: id=%s removed=%d"), *TargetId, RemovedCount);
    }

    return RemovedCount;
}

FRshipTargetProxy URshipSubsystem::EnsureTargetIdentity(const FString& FullTargetId, const FString& DisplayName, const TArray<FString>& ParentTargetIds)
{
	if (FullTargetId.IsEmpty())
	{
		return FRshipTargetProxy();
	}

	Target* TargetRef = EnsureAutomationTarget(
		FullTargetId,
		DisplayName.IsEmpty() ? FullTargetId : DisplayName,
		ParentTargetIds);

	return TargetRef ? FRshipTargetProxy(this, FullTargetId) : FRshipTargetProxy();
}

FRshipTargetProxy URshipSubsystem::EnsureActorIdentity(AActor* Actor)
{
	if (!Actor)
	{
		return FRshipTargetProxy();
	}

	URshipActorRegistrationComponent* Registration = Actor->FindComponentByClass<URshipActorRegistrationComponent>();
	if (Registration)
	{
		// When a registration component exists, it is the canonical source of identity.
		// Do not fall back to ad-hoc identity generation from actor labels/names.
		if (!Registration->TargetData)
		{
			Registration->Register();
		}

		if (Registration->TargetData)
		{
			return FRshipTargetProxy(this, Registration->TargetData->GetId());
		}

		UE_LOG(LogRshipExec, Verbose,
			TEXT("EnsureActorIdentity deferred: registration component exists but target not ready for actor '%s'"),
			*GetNameSafe(Actor));
		return FRshipTargetProxy();
	}

	const FString DisplayName = GetActorDisplayName(Actor);
	const FString FullTargetId = GetServiceId() + TEXT(":") + DisplayName;
	TArray<FString> ParentTargetIds;
	return EnsureTargetIdentity(FullTargetId, DisplayName, ParentTargetIds);
}

FRshipTargetProxy URshipSubsystem::GetTargetProxyForActor(AActor* Actor)
{
	FRshipTargetProxy RootTarget = EnsureActorIdentity(Actor);
	if (!RootTarget.IsValid())
	{
		return FRshipTargetProxy();
	}

	return FRshipTargetProxy(this, RootTarget.GetId());
}

void URshipSubsystem::RegisterManagedTarget(Target* ManagedTarget)
{
    if (!ManagedTarget)
    {
        return;
    }

    const FString TargetId = ManagedTarget->GetId();
    PruneInvalidManagedTargetRefs(TargetId);

    bool bHasCurrentRef = false;
    int32 RemovedStaleKeyRefs = 0;
    for (auto It = RegisteredTargetsById.CreateIterator(); It; ++It)
    {
        if (It.Value() != ManagedTarget)
        {
            continue;
        }

        if (It.Key() == TargetId)
        {
            bHasCurrentRef = true;
            continue;
        }

        It.RemoveCurrent();
        ++RemovedStaleKeyRefs;
    }

    if (!bHasCurrentRef)
    {
        RegisteredTargetsById.Add(TargetId, ManagedTarget);
    }

    ManagedTargetSnapshots.Add(ManagedTarget, BuildManagedTargetSnapshot(ManagedTarget));

    UE_LOG(LogRshipExec, Verbose, TEXT("RegisterManagedTarget: '%s' registered (addedRef=%s removedStaleRefs=%d)"),
        *TargetId,
        bHasCurrentRef ? TEXT("false") : TEXT("true"),
        RemovedStaleKeyRefs);

    // Always publish this registration so every proxy refreshes metadata/action bindings.
    SendTarget(ManagedTarget);
    ProcessMessageQueue();
}

void URshipSubsystem::UnregisterManagedTarget(Target* ManagedTarget)
{
    if (!ManagedTarget)
    {
        return;
    }

    FString TargetId = ManagedTarget->GetId();
    if (const FManagedTargetSnapshot* ExistingSnapshot = ManagedTargetSnapshots.Find(ManagedTarget))
    {
        if (!ExistingSnapshot->Id.IsEmpty())
        {
            TargetId = ExistingSnapshot->Id;
        }
    }

    int32 RemovedTargetRefs = 0;
    for (auto It = RegisteredTargetsById.CreateIterator(); It; ++It)
    {
        if (It.Value() == ManagedTarget)
        {
            It.RemoveCurrent();
            ++RemovedTargetRefs;
        }
    }

    if (ManagedTargetSnapshots.Remove(ManagedTarget) > 0 || RemovedTargetRefs > 0)
    {
        PruneInvalidManagedTargetRefs(TargetId);

        TArray<Target*> RemainingRefs;
        RegisteredTargetsById.MultiFind(TargetId, RemainingRefs);
        const int32 RemainingRefCount = RemainingRefs.Num();

        if (RemainingRefCount == 0)
        {
            DeleteTarget(ManagedTarget);
            ProcessMessageQueue();
        }
        else
        {
            UE_LOG(LogRshipExec, Verbose, TEXT("UnregisterManagedTarget: '%s' ref-- (remaining=%d), skipping offline publish"),
                *TargetId, RemainingRefCount);
        }
    }
}

void URshipSubsystem::OnManagedTargetChanged(Target* ManagedTarget)
{
    if (!ManagedTarget)
    {
        return;
    }

    FManagedTargetSnapshot* ExistingSnapshot = ManagedTargetSnapshots.Find(ManagedTarget);
    if (!ExistingSnapshot)
    {
        RegisterManagedTarget(ManagedTarget);
        return;
    }

    const TMap<FString, FRshipActionProxy>& CurrentActions = ManagedTarget->GetActions();
    const TMap<FString, FRshipEmitterProxy>& CurrentEmitters = ManagedTarget->GetEmitters();

    bool bBindingsChanged = false;

    for (const auto& ActionPair : CurrentActions)
    {
        if (!ExistingSnapshot->ActionIds.Contains(ActionPair.Key))
        {
            bBindingsChanged = true;
        }
    }

    for (const auto& EmitterPair : CurrentEmitters)
    {
        if (!ExistingSnapshot->EmitterIds.Contains(EmitterPair.Key))
        {
            bBindingsChanged = true;
        }
    }

    const bool bIdentityChanged =
        ExistingSnapshot->Id != ManagedTarget->GetId() ||
        ExistingSnapshot->Name != ManagedTarget->GetName() ||
        ExistingSnapshot->ParentTargetIds != ManagedTarget->GetParentTargetIds();

    if (ExistingSnapshot->Id != ManagedTarget->GetId())
    {
        const FString NewTargetId = ManagedTarget->GetId();
        PruneInvalidManagedTargetRefs(ExistingSnapshot->Id);
        PruneInvalidManagedTargetRefs(NewTargetId);

        bool bHasCurrentRef = false;
        for (auto It = RegisteredTargetsById.CreateIterator(); It; ++It)
        {
            if (It.Value() != ManagedTarget)
            {
                continue;
            }

            if (It.Key() == NewTargetId)
            {
                bHasCurrentRef = true;
                continue;
            }

            It.RemoveCurrent();
        }

        if (!bHasCurrentRef)
        {
            RegisteredTargetsById.Add(NewTargetId, ManagedTarget);
        }
    }

    if (bBindingsChanged || bIdentityChanged)
    {
        // Republish full target + action/emitter definitions so metadata stays consistent server-side.
        SendTarget(ManagedTarget);
    }

    *ExistingSnapshot = BuildManagedTargetSnapshot(ManagedTarget);

    if (bBindingsChanged || bIdentityChanged)
    {
        ProcessMessageQueue();
    }
}

Target* URshipSubsystem::EnsureAutomationTarget(const FString& FullTargetId, const FString& Name, const TArray<FString>& ParentTargetIds)
{
    if (FullTargetId.IsEmpty())
    {
        return nullptr;
    }

    if (Target* const* Existing = RegisteredTargetsById.Find(FullTargetId))
    {
        Target* ExistingTarget = *Existing;
        if (ExistingTarget)
        {
            ExistingTarget->SetName(Name.IsEmpty() ? FullTargetId : Name);
            ExistingTarget->SetParentTargetIds(ParentTargetIds);
            return ExistingTarget;
        }
    }

    // Identity move: if the same logical automation target is found under an old id,
    // migrate it to the new id so all existing actions/emitters can be resent with the new targetId.
    FString PreviousAutomationId;
    for (const auto& Pair : AutomationOwnedTargets)
    {
        Target* Candidate = Pair.Value.Get();
        if (!Candidate)
        {
            continue;
        }

        if (Candidate->GetName() == (Name.IsEmpty() ? FullTargetId : Name) &&
            Candidate->GetParentTargetIds() == ParentTargetIds)
        {
            PreviousAutomationId = Pair.Key;
            break;
        }
    }

    if (!PreviousAutomationId.IsEmpty() && PreviousAutomationId != FullTargetId)
    {
        TUniquePtr<Target> MovedTarget = MoveTemp(AutomationOwnedTargets.FindChecked(PreviousAutomationId));
        AutomationOwnedTargets.Remove(PreviousAutomationId);

        if (MovedTarget.IsValid())
        {
            Target* RawTarget = MovedTarget.Get();

            for (auto It = RegisteredTargetsById.CreateIterator(); It; ++It)
            {
                if (It.Value() == RawTarget)
                {
                    It.RemoveCurrent();
                }
            }

            RegisteredTargetsById.Add(FullTargetId, RawTarget);

            MovedTarget->SetId(FullTargetId);
            MovedTarget->SetName(Name.IsEmpty() ? FullTargetId : Name);
            MovedTarget->SetParentTargetIds(ParentTargetIds);

            AutomationOwnedTargets.Add(FullTargetId, MoveTemp(MovedTarget));
            return RawTarget;
        }
    }

    TUniquePtr<Target>& OwnedTarget = AutomationOwnedTargets.FindOrAdd(FullTargetId);
    if (!OwnedTarget.IsValid())
    {
        OwnedTarget = MakeUnique<Target>(FullTargetId, this);
    }

    OwnedTarget->SetName(Name.IsEmpty() ? FullTargetId : Name);
    OwnedTarget->SetParentTargetIds(ParentTargetIds);
    return OwnedTarget.Get();
}

bool URshipSubsystem::RemoveAutomationTarget(const FString& FullTargetId)
{
    if (FullTargetId.IsEmpty())
    {
        return false;
    }
    return AutomationOwnedTargets.Remove(FullTargetId) > 0;
}

bool URshipSubsystem::RegisterFunctionActionForTarget(const FString& FullTargetId, UObject* Owner, const FName& FunctionName, const FString& ExposedActionName)
{
    if (!Owner)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterFunctionActionForTarget failed: owner is null (target=%s, function=%s)"),
            *FullTargetId, *FunctionName.ToString());
        return false;
    }

    Target* const* TargetPtr = RegisteredTargetsById.Find(FullTargetId);
    if (!TargetPtr || !*TargetPtr)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterFunctionActionForTarget failed: target not found (%s)"), *FullTargetId);
        return false;
    }

    UFunction* Func = Owner->FindFunction(FunctionName);
    if (!Func)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterFunctionActionForTarget failed: function '%s' not found on '%s'"),
            *FunctionName.ToString(), *GetNameSafe(Owner));
        return false;
    }

    const FString RawName = Func->GetName();
    if (RawName.Contains(TEXT("__DelegateSignature")))
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("RegisterFunctionActionForTarget skipped delegate signature function '%s'"), *RawName);
        return false;
    }

    const FString FinalName = ExposedActionName.IsEmpty() ? RawName : ExposedActionName;
    const FString FullActionId = FullTargetId + TEXT(":") + FinalName;

    Target* TargetRef = *TargetPtr;
    if (const FRshipActionProxy* ExistingAction = TargetRef->GetActions().Find(FullActionId))
    {
        UObject* ExistingOwner = ExistingAction->GetOwnerObject();
        const bool bExistingOwnerValid = IsValid(ExistingOwner);
        if (!bExistingOwnerValid || ExistingOwner != Owner)
        {
            if (!bExistingOwnerValid)
            {
                UE_LOG(LogRshipExec, Error,
                    TEXT("RegisterFunctionActionForTarget replacing stale action '%s' (invalid owner)."),
                    *FullActionId);
            }
            else
            {
                UE_LOG(LogRshipExec, Warning,
                    TEXT("RegisterFunctionActionForTarget replacing action '%s' owned by '%s' with new owner '%s'."),
                    *FullActionId,
                    *GetNameSafe(ExistingOwner),
                    *GetNameSafe(Owner));
            }
            TargetRef->AddAction(FRshipActionProxy::FromFunction(FullActionId, FinalName, Func, Owner));
            return true;
        }

        UE_LOG(LogRshipExec, Verbose, TEXT("RegisterFunctionActionForTarget skipped duplicate action '%s'"), *FullActionId);
        return false;
    }

    TargetRef->AddAction(FRshipActionProxy::FromFunction(FullActionId, FinalName, Func, Owner));
    return true;
}

bool URshipSubsystem::RegisterPropertyActionForTarget(const FString& FullTargetId, UObject* Owner, const FName& PropertyName, const FString& ExposedActionName)
{
    if (!Owner)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterPropertyActionForTarget failed: owner is null (target=%s, property=%s)"),
            *FullTargetId, *PropertyName.ToString());
        return false;
    }

    Target* const* TargetPtr = RegisteredTargetsById.Find(FullTargetId);
    if (!TargetPtr || !*TargetPtr)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterPropertyActionForTarget failed: target not found (%s)"), *FullTargetId);
        return false;
    }

    FProperty* Prop = Owner->GetClass()->FindPropertyByName(PropertyName);
    if (!Prop || Prop->IsA<FMulticastDelegateProperty>())
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterPropertyActionForTarget failed: property '%s' not found/unsupported on '%s'"),
            *PropertyName.ToString(), *GetNameSafe(Owner));
        return false;
    }

    const FString RawName = Prop->GetName();
    const FString FinalName = ExposedActionName.IsEmpty() ? RawName : ExposedActionName;
    const FString FullActionId = FullTargetId + TEXT(":") + FinalName;

    Target* TargetRef = *TargetPtr;
    if (const FRshipActionProxy* ExistingAction = TargetRef->GetActions().Find(FullActionId))
    {
        UObject* ExistingOwner = ExistingAction->GetOwnerObject();
        const bool bExistingOwnerValid = IsValid(ExistingOwner);
        if (!bExistingOwnerValid || ExistingOwner != Owner)
        {
            if (!bExistingOwnerValid)
            {
                UE_LOG(LogRshipExec, Error,
                    TEXT("RegisterPropertyActionForTarget replacing stale action '%s' (invalid owner)."),
                    *FullActionId);
            }
            else
            {
                UE_LOG(LogRshipExec, Warning,
                    TEXT("RegisterPropertyActionForTarget replacing action '%s' owned by '%s' with new owner '%s'."),
                    *FullActionId,
                    *GetNameSafe(ExistingOwner),
                    *GetNameSafe(Owner));
            }
            TargetRef->AddAction(FRshipActionProxy::FromProperty(FullActionId, FinalName, Prop, Owner));
            return true;
        }

        UE_LOG(LogRshipExec, Verbose, TEXT("RegisterPropertyActionForTarget skipped duplicate action '%s'"), *FullActionId);
        return false;
    }

    TargetRef->AddAction(FRshipActionProxy::FromProperty(FullActionId, FinalName, Prop, Owner));
    return true;
}

bool URshipSubsystem::RegisterEmitterForTarget(const FString& FullTargetId, UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName)
{
    if (!Owner)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterEmitterForTarget failed: owner is null (target=%s, delegate=%s)"),
            *FullTargetId, *DelegateName.ToString());
        return false;
    }

    Target* const* TargetPtr = RegisteredTargetsById.Find(FullTargetId);
    if (!TargetPtr || !*TargetPtr)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterEmitterForTarget failed: target not found (%s)"), *FullTargetId);
        return false;
    }

    FProperty* Prop = Owner->GetClass()->FindPropertyByName(DelegateName);
    FMulticastInlineDelegateProperty* EmitterProp = CastField<FMulticastInlineDelegateProperty>(Prop);
    if (!EmitterProp)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterEmitterForTarget failed: delegate '%s' not found/unsupported on '%s'"),
            *DelegateName.ToString(), *GetNameSafe(Owner));
        return false;
    }

    const FString RawName = EmitterProp->GetName();
    const FString FinalName = ExposedEmitterName.IsEmpty() ? RawName : ExposedEmitterName;
    const FString FullEmitterId = FullTargetId + TEXT(":") + FinalName;

    Target* TargetRef = *TargetPtr;
    if (TargetRef->GetEmitters().Contains(FullEmitterId))
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("RegisterEmitterForTarget skipped duplicate emitter '%s'"), *FullEmitterId);
        return false;
    }

    TargetRef->AddEmitter(FRshipEmitterProxy::FromDelegateProperty(FullEmitterId, FinalName, EmitterProp));
    return true;
}

void URshipSubsystem::GetManagedTargetsSnapshot(TArray<FRshipManagedTargetView>& OutTargets) const
{
    OutTargets.Reset();
    OutTargets.Reserve(ManagedTargetSnapshots.Num());

    for (const TPair<Target*, FManagedTargetSnapshot>& Pair : ManagedTargetSnapshots)
    {
        Target* ManagedTarget = Pair.Key;
        if (!ManagedTarget)
        {
            continue;
        }

        FRshipManagedTargetView View;
        View.Id = Pair.Value.Id;
        View.Name = Pair.Value.Name;
        View.ParentTargetIds = Pair.Value.ParentTargetIds;
        View.ActionCount = ManagedTarget->GetActions().Num();
        View.EmitterCount = ManagedTarget->GetEmitters().Num();
        View.BoundTargetComponent = Pair.Value.BoundTargetComponent;
        View.bBoundToComponent = Pair.Value.bBoundToComponent;
        OutTargets.Add(MoveTemp(View));
    }
}

void URshipSubsystem::SendInstanceInfo()
{
    UE_LOG(LogRshipExec, Log, TEXT("SendInstanceInfo: MachineId=%s, ServiceId=%s, InstanceId=%s, ClusterId=%s, ClientId=%s"),
        *MachineId, *ServiceId, *InstanceId, *ClusterId, *ClientId);

    // Send Machine - HIGH priority, coalesce
    FRshipMachineRecord MachineRecord;
    MachineRecord.Id = MachineId;
    MachineRecord.Name = MachineId;
    MachineRecord.ExecName = MachineId;
    MachineRecord.ClientId = TEXT("");
    MachineRecord.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    SetItem("Machine", FRshipEntitySerializer::ToJson(MachineRecord), ERshipMessagePriority::High, "machine:" + MachineId);

    const URshipSettings *Settings = GetDefault<URshipSettings>();

    FColor SRGBColor = Settings->ServiceColor.ToFColor(true);
    FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);

    FRshipInstanceRecord InstanceRecord;
    InstanceRecord.ClientId = ClientId;
    InstanceRecord.Name = ServiceId;
    InstanceRecord.Id = InstanceId;
    InstanceRecord.ClusterId = ClusterId;
    InstanceRecord.ServiceTypeCode = TEXT("unreal");
    InstanceRecord.ServiceId = ServiceId;
    InstanceRecord.MachineId = MachineId;
    InstanceRecord.Status = TEXT("Available");
    InstanceRecord.Color = ColorHex;
    InstanceRecord.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

    SetItem("Instance", FRshipEntitySerializer::ToJson(InstanceRecord), ERshipMessagePriority::High, "instance:" + InstanceId);
}

void URshipSubsystem::SendAll()
{
    UE_LOG(LogRshipExec, Log, TEXT("SendAll: %d managed targets registered"), ManagedTargetSnapshots.Num());

    // Send Machine and Instance info first
    SendInstanceInfo();

    // Send all managed targets
    for (const auto& Pair : ManagedTargetSnapshots)
    {
        if (Pair.Key)
        {
            SendTarget(Pair.Key);
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
    TSharedPtr<FJsonObject> payload = FRshipMykoTransport::MakeSet(itemType, data, MachineId);

    // Compact logging: type + id only (skip high-frequency pulse traffic).
    if (itemType != TEXT("Pulse"))
    {
        FString ItemId = TEXT("<none>");
        if (data.IsValid())
        {
            data->TryGetStringField(TEXT("id"), ItemId);
        }

        UE_LOG(LogRshipExec, Log, TEXT("SetItem type=%s id=%s"), *itemType, *ItemId);
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
    FString fullEmitterId = targetId + ":" + emitterId;
    const FDateTime Now = FDateTime::UtcNow();
    const int64 TimestampMs = Now.ToUnixTimestamp() * 1000LL + Now.GetMillisecond();

    FRshipPulseRecord PulseRecord;
    PulseRecord.EmitterId = fullEmitterId;
    PulseRecord.Id = fullEmitterId;
    PulseRecord.Data = data;
    PulseRecord.TimestampMs = static_cast<double>(TimestampMs);
    PulseRecord.ClientId = TEXT("");
    PulseRecord.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

    // Emitter pulses coalesce by emitter ID to ensure latest value is always sent
    // This prevents stale data from queueing - only the most recent pulse per emitter is kept
    SetItem("Pulse", FRshipEntitySerializer::ToJson(PulseRecord), ERshipMessagePriority::Normal, fullEmitterId);


}

const FRshipEmitterProxy* URshipSubsystem::GetEmitterInfo(FString fullTargetId, FString emitterId)
{
    Target* FoundTarget = nullptr;
    if (Target* const* FoundPtr = RegisteredTargetsById.Find(fullTargetId))
    {
        FoundTarget = *FoundPtr;
    }

    if (!FoundTarget)
    {
        return nullptr;
    }

    FString fullEmitterId = fullTargetId + ":" + emitterId;

    return FoundTarget->GetEmitters().Find(fullEmitterId);
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
    return bRemoteCommunicationEnabled && WebSocket.IsValid() && WebSocket->IsConnected();
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

void URshipSubsystem::RegisterTargetComponent(URshipActorRegistrationComponent* Component)
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

    for (auto It = TargetComponents->CreateKeyIterator(TargetId); It; ++It)
    {
        if (It.Value() == Component)
        {
            return;
        }
    }

    TargetComponents->Add(TargetId, Component);

    UE_LOG(LogRshipExec, Log, TEXT("Registered target component: %s (total: %d)"),
        *TargetId, TargetComponents->Num());
}

void URshipSubsystem::UnregisterTargetComponent(URshipActorRegistrationComponent* Component)
{
    if (!Component || !TargetComponents)
    {
        return;
    }

    // Remove all entries that reference this exact component pointer.
    // A component can be duplicated in the map due to re-registration flows;
    // only removing one entry leaves stale references behind.
    int32 RemovedCount = 0;
    for (auto It = TargetComponents->CreateIterator(); It; ++It)
    {
        if (It.Value() == Component)
        {
            It.RemoveCurrent();
            RemovedCount++;
        }
    }

    if (RemovedCount > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Unregistered target component refs: %d (remaining: %d)"),
            RemovedCount, TargetComponents->Num());
    }
}

URshipActorRegistrationComponent* URshipSubsystem::FindTargetComponent(const FString& FullTargetId) const
{
    if (!TargetComponents)
    {
        return nullptr;
    }

    URshipActorRegistrationComponent* const* Found = TargetComponents->Find(FullTargetId);
    return Found ? *Found : nullptr;
}

TArray<URshipActorRegistrationComponent*> URshipSubsystem::FindAllTargetComponents(const FString& FullTargetId) const
{
    TArray<URshipActorRegistrationComponent*> Result;
    if (TargetComponents)
    {
        TargetComponents->MultiFind(FullTargetId, Result);
    }
    return Result;
}
