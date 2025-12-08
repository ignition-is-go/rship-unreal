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

using namespace std;

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipSubsystem::Initialize"));

    // Initialize connection state
    ConnectionState = ERshipConnectionState::Disconnected;
    ReconnectAttempts = 0;
    bUsingHighPerfWebSocket = false;

    // Initialize rate limiter
    InitializeRateLimiter();

    // Connect to server
    Reconnect();

    auto world = GetWorld();

    if (world != nullptr)
    {
        this->EmitterHandler = GetWorld()->SpawnActor<AEmitterHandler>();
    }

    this->TargetComponents = new TSet<URshipTargetComponent *>;

    // Start queue processing timer
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (Settings->bEnableRateLimiting && world != nullptr)
    {
        world->GetTimerManager().SetTimer(
            QueueProcessTimerHandle,
            this,
            &URshipSubsystem::ProcessMessageQueue,
            Settings->QueueProcessInterval,
            true  // Looping
        );
    }
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
    // Don't reconnect if already connecting or backing off
    if (ConnectionState == ERshipConnectionState::Connecting ||
        ConnectionState == ERshipConnectionState::BackingOff)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Reconnect called while already connecting or backing off, ignoring"));
        return;
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
    FString rshipHostAddress = *Settings->rshipHostAddress;
    int32 rshipServerPort = Settings->rshipServerPort;

    if (rshipHostAddress.IsEmpty() || rshipHostAddress.Len() == 0)
    {
        rshipHostAddress = FString("localhost");
    }

    // Close existing connections
    if (WebSocket)
    {
        WebSocket->Close();
        WebSocket.Reset();
    }
    if (HighPerfWebSocket)
    {
        HighPerfWebSocket->Close();
        HighPerfWebSocket.Reset();
    }

    ConnectionState = ERshipConnectionState::Connecting;

    FString WebSocketUrl = "ws://" + rshipHostAddress + ":" + FString::FromInt(rshipServerPort) + "/myko";
    UE_LOG(LogRshipExec, Log, TEXT("Connecting to %s (HighPerf=%d)"), *WebSocketUrl, Settings->bUseHighPerformanceWebSocket);

    // Choose WebSocket implementation based on settings
    if (Settings->bUseHighPerformanceWebSocket)
    {
        // Use high-performance WebSocket with dedicated send thread
        bUsingHighPerfWebSocket = true;
        HighPerfWebSocket = MakeShared<FRshipWebSocket>();

        // Bind event handlers
        HighPerfWebSocket->OnConnected.BindUObject(this, &URshipSubsystem::OnWebSocketConnected);
        HighPerfWebSocket->OnConnectionError.BindUObject(this, &URshipSubsystem::OnWebSocketConnectionError);
        HighPerfWebSocket->OnClosed.BindUObject(this, &URshipSubsystem::OnWebSocketClosed);
        HighPerfWebSocket->OnMessage.BindUObject(this, &URshipSubsystem::OnWebSocketMessage);

        // Configure and connect
        FRshipWebSocketConfig Config;
        Config.bTcpNoDelay = Settings->bTcpNoDelay;
        Config.bDisableCompression = Settings->bDisableCompression;
        Config.PingIntervalSeconds = Settings->PingIntervalSeconds;
        Config.bAutoReconnect = false;  // We handle reconnection ourselves

        HighPerfWebSocket->Connect(WebSocketUrl, Config);
    }
    else
    {
        // Use standard UE WebSocket
        bUsingHighPerfWebSocket = false;
        WebSocket = FWebSocketsModule::Get().CreateWebSocket(WebSocketUrl);

        // Bind event handlers
        WebSocket->OnConnected().AddLambda([this]()
        {
            OnWebSocketConnected();
        });

        WebSocket->OnConnectionError().AddLambda([this](const FString &Error)
        {
            OnWebSocketConnectionError(Error);
        });

        WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString &Reason, bool bWasClean)
        {
            OnWebSocketClosed(StatusCode, Reason, bWasClean);
        });

        WebSocket->OnMessage().AddLambda([this](const FString &MessageString)
        {
            OnWebSocketMessage(MessageString);
        });

        WebSocket->OnMessageSent().AddLambda([](const FString &MessageString)
        {
            // Optional: track sent messages for debugging
        });

        WebSocket->Connect();
    }
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

    // Clear any pending reconnect timer
    if (auto world = GetWorld())
    {
        world->GetTimerManager().ClearTimer(ReconnectTimerHandle);
    }

    // Send registration data
    SendAll();
}

void URshipSubsystem::OnWebSocketConnectionError(const FString &Error)
{
    UE_LOG(LogRshipExec, Warning, TEXT("WebSocket connection error: %s"), *Error);

    ConnectionState = ERshipConnectionState::Disconnected;

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
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (Settings->bAutoReconnect && !bWasClean)
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

    // Schedule reconnect using timer
    if (auto world = GetWorld())
    {
        world->GetTimerManager().SetTimer(
            ReconnectTimerHandle,
            this,
            &URshipSubsystem::AttemptReconnect,
            BackoffDelay,
            false  // Not looping
        );
    }
}

void URshipSubsystem::AttemptReconnect()
{
    UE_LOG(LogRshipExec, Log, TEXT("Attempting reconnect..."));
    ConnectionState = ERshipConnectionState::Reconnecting;
    Reconnect();
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

void URshipSubsystem::ProcessMessageQueue()
{
    if (!RateLimiter || ConnectionState != ERshipConnectionState::Connected)
    {
        return;
    }

    RateLimiter->ProcessQueue();
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
}

void URshipSubsystem::SendJsonDirect(const FString& JsonString)
{
    // Check which WebSocket is active
    bool bConnected = false;
    if (bUsingHighPerfWebSocket)
    {
        bConnected = HighPerfWebSocket.IsValid() && HighPerfWebSocket->IsConnected();
    }
    else
    {
        bConnected = WebSocket.IsValid() && WebSocket->IsConnected();
    }

    if (!bConnected)
    {
        // Don't spam reconnect attempts - let the scheduled reconnect handle it
        if (ConnectionState == ERshipConnectionState::Disconnected)
        {
            const URshipSettings *Settings = GetDefault<URshipSettings>();
            if (Settings->bAutoReconnect && !ReconnectTimerHandle.IsValid())
            {
                ScheduleReconnect();
            }
        }
        return;
    }

    // Instrumentation: Track send timing to detect 30Hz throttle
    static double LastSendTime = 0.0;
    static int32 SendCount = 0;
    double Now = FPlatformTime::Seconds();

    SendCount++;
    if (LastSendTime > 0.0)
    {
        double DeltaMs = (Now - LastSendTime) * 1000.0;
        // Log if sends are being throttled (>30ms between sends suggests 30Hz limit)
        // Only warn in non-high-perf mode since high-perf should be fast
        if (!bUsingHighPerfWebSocket && DeltaMs > 30.0 && SendCount % 100 == 0)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("WebSocket send throttled: %.1fms between sends (send #%d) - enable High-Performance WebSocket"),
                DeltaMs, SendCount);
        }
    }
    LastSendTime = Now;

    UE_LOG(LogRshipExec, Verbose, TEXT("Sending: %s"), *JsonString);

    // Send via appropriate WebSocket
    if (bUsingHighPerfWebSocket)
    {
        HighPerfWebSocket->Send(JsonString);
    }
    else
    {
        WebSocket->Send(JsonString);
    }
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
    UE_LOG(LogRshipExec, Verbose, TEXT("Received Event %s"), *type);

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

            for (URshipTargetComponent *comp : *this->TargetComponents)
            {
                if (comp->TargetData->GetId() == targetId)
                {
                    Target *target = comp->TargetData;
                    AActor *owner = comp->GetOwner();

                    if (target != nullptr)
                    {
                        bool takeResult = target->TakeAction(owner, actionId, execData);
                        result |= takeResult;
                        comp->OnDataReceived();
                    }
                    else
                    {
                        UE_LOG(LogRshipExec, Warning, TEXT("Target not found: %s"), *targetId);
                    }
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
}

void URshipSubsystem::Deinitialize()
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipSubsystem::Deinitialize"));

    // Clear timers
    if (auto world = GetWorld())
    {
        world->GetTimerManager().ClearTimer(QueueProcessTimerHandle);
        world->GetTimerManager().ClearTimer(ReconnectTimerHandle);
    }

    // Clear rate limiter
    if (RateLimiter)
    {
        RateLimiter->ClearQueue();
        RateLimiter.Reset();
    }

    // Close appropriate WebSocket
    if (bUsingHighPerfWebSocket)
    {
        if (HighPerfWebSocket)
        {
            HighPerfWebSocket->Close();
            HighPerfWebSocket.Reset();
        }
    }
    else
    {
        if (WebSocket && WebSocket->IsConnected())
        {
            WebSocket->Close();
        }
        WebSocket.Reset();
    }

    Super::Deinitialize();
}

void URshipSubsystem::SendTarget(Target *target)
{
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

    // Target registration - HIGH priority, coalesce by target ID
    SetItem("Target", Target, ERshipMessagePriority::High, target->GetId());

    TSharedPtr<FJsonObject> TargetStatus = MakeShareable(new FJsonObject);

    TargetStatus->SetStringField(TEXT("targetId"), target->GetId());
    TargetStatus->SetStringField(TEXT("instanceId"), InstanceId);
    TargetStatus->SetStringField(TEXT("status"), TEXT("online"));
    TargetStatus->SetStringField(TEXT("id"), InstanceId + ":" + target->GetId());

    SetItem("TargetStatus", TargetStatus, ERshipMessagePriority::High, target->GetId() + ":status");
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

    // Emitter registration - HIGH priority, coalesce by emitter ID
    SetItem("Emitter", Emitter, ERshipMessagePriority::High, emitter->GetId());
}

void URshipSubsystem::SendTargetStatus(Target *target, bool online)
{
    // TODO: Implement status update
}

void URshipSubsystem::SendAll()
{
    // Send Machine - HIGH priority, coalesce
    TSharedPtr<FJsonObject> Machine = MakeShareable(new FJsonObject);
    Machine->SetStringField(TEXT("id"), MachineId);
    Machine->SetStringField(TEXT("execName"), MachineId);

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

    SetItem("Instance", Instance, ERshipMessagePriority::High, "instance:" + InstanceId);

    // Send all targets
    for (auto &comp : *this->TargetComponents)
    {
        SendTarget(comp->TargetData);
    }
}

void URshipSubsystem::SendJson(TSharedPtr<FJsonObject> Payload)
{
    // Legacy method - queue with normal priority
    QueueMessage(Payload, ERshipMessagePriority::Normal, ERshipMessageType::Generic);
}

void URshipSubsystem::SetItem(FString itemType, TSharedPtr<FJsonObject> data, ERshipMessagePriority Priority, const FString& CoalesceKey)
{
    TSharedPtr<FJsonObject> wrapped = WrapWSEvent(MakeSet(itemType, data));

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

    QueueMessage(wrapped, Priority, Type, CoalesceKey);
}

void URshipSubsystem::PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data)
{
    TSharedPtr<FJsonObject> pulse = MakeShareable(new FJsonObject);

    FString fullEmitterId = targetId + ":" + emitterId;

    pulse->SetStringField("emitterId", fullEmitterId);
    pulse->SetStringField("id", fullEmitterId);
    pulse->SetObjectField("data", data);

    // Emitter pulses are LOW priority and coalesce by emitter ID
    // This means rapid pulses from the same emitter will be coalesced
    SetItem("Pulse", pulse, ERshipMessagePriority::Low, fullEmitterId);
}

EmitterContainer *URshipSubsystem::GetEmitterInfo(FString fullTargetId, FString emitterId)
{
    Target *target = nullptr;

    for (auto &comp : *this->TargetComponents)
    {
        if (comp->TargetData->GetId() == fullTargetId)
        {
            target = comp->TargetData;
            break;
        }
    }

    if (target == nullptr)
    {
        return nullptr;
    }

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

// ============================================================================
// DIAGNOSTIC METHODS
// These provide runtime visibility into the adaptive outbound pipeline
// ============================================================================

bool URshipSubsystem::IsConnected() const
{
    if (bUsingHighPerfWebSocket)
    {
        return HighPerfWebSocket.IsValid() && HighPerfWebSocket->IsConnected();
    }
    return WebSocket && WebSocket->IsConnected();
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
