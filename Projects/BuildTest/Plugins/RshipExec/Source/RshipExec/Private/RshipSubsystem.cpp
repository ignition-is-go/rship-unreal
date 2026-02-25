// Fill out your copyright notice in the Description page of Project Settings
#include "RshipSubsystem.h"
#include "RshipSettings.h"
#include "WebSocketsModule.h"
#include "EngineUtils.h"
#include "RshipTargetComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Action.h"
#include "Target.h"
#include "RshipContentMappingManager.h"
#include "RshipDisplayManager.h"

#include "Myko.h"
#include "EmitterHandler.h"
#include "Logs.h"

static TAutoConsoleVariable<int32> CVarRshipInboundMaxMessagesPerTick(
    TEXT("r.Rship.Inbound.MaxMessagesPerTick"),
    256,
    TEXT("Maximum number of inbound rship payloads applied per TickSubsystems frame."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarRshipInboundAuthorityOnly(
    TEXT("r.Rship.Inbound.AuthorityOnly"),
    1,
    TEXT("If non-zero, only the configured authoritative node ingests live websocket state."),
    ECVF_Default);

static TAutoConsoleVariable<float> CVarRshipControlSyncRateHz(
    TEXT("r.Rship.ControlSyncRateHz"),
    0.0f,
    TEXT("Override deterministic control/apply sync tick rate in Hz. <=0 uses project settings."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarRshipInboundApplyLeadFrames(
    TEXT("r.Rship.Inbound.ApplyLeadFrames"),
    0,
    TEXT("Override inbound apply lead frames. <=0 uses project settings."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarRshipInboundRequireExactFrame(
    TEXT("r.Rship.Inbound.RequireExactFrame"),
    -1,
    TEXT("Override whether inbound payloads with explicit apply-frame metadata require exact-frame match. 1=exact, 0=legacy, <=0 uses project settings."),
    ECVF_Default);

namespace
{
bool TryGetJsonInt64(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, int64& OutValue)
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    double NumberValue = 0.0;
    if (JsonObject->TryGetNumberField(FieldName, NumberValue))
    {
        OutValue = static_cast<int64>(FMath::FloorToDouble(NumberValue));
        return true;
    }

    FString StringValue;
    if (JsonObject->TryGetStringField(FieldName, StringValue))
    {
        StringValue.TrimStartAndEndInline();
        if (StringValue.IsNumeric())
        {
            OutValue = FCString::Atoi64(*StringValue);
            return true;
        }
    }

    return false;
}

bool TryGetExplicitApplyFrameFromObject(const TSharedPtr<FJsonObject>& JsonObject, int64& OutApplyFrame)
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    static const TCHAR* CandidateFields[] =
    {
        TEXT("applyFrame"),
        TEXT("targetFrame"),
        TEXT("frame"),
        TEXT("frameNumber"),
        TEXT("frameIndex"),
        TEXT("target_frame"),
        TEXT("apply_frame")
    };

    for (const TCHAR* FieldName : CandidateFields)
    {
        if (TryGetJsonInt64(JsonObject, FieldName, OutApplyFrame))
        {
            return true;
        }
    }

    const TSharedPtr<FJsonObject>* NestedObject = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("data"), NestedObject) && NestedObject && NestedObject->IsValid())
    {
        for (const TCHAR* FieldName : CandidateFields)
        {
            if (TryGetJsonInt64(*NestedObject, FieldName, OutApplyFrame))
            {
                return true;
            }
        }
    }

    if (JsonObject->TryGetObjectField(TEXT("meta"), NestedObject) && NestedObject && NestedObject->IsValid())
    {
        for (const TCHAR* FieldName : CandidateFields)
        {
            if (TryGetJsonInt64(*NestedObject, FieldName, OutApplyFrame))
            {
                return true;
            }
        }

        const TSharedPtr<FJsonObject>* MetadataData = nullptr;
        if ((*NestedObject)->TryGetObjectField(TEXT("data"), MetadataData) && MetadataData && MetadataData->IsValid())
        {
            for (const TCHAR* FieldName : CandidateFields)
            {
                if (TryGetJsonInt64(*MetadataData, FieldName, OutApplyFrame))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool RshipTargetTokenMatchesLocalNode(const FString& RawValue, const FString& NodeId)
{
    FString Trimmed = RawValue.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return false;
    }

    Trimmed.ReplaceInline(TEXT(";"), TEXT(","));
    TArray<FString> Tokens;
    Trimmed.ParseIntoArray(Tokens, TEXT(","), true);
    if (Tokens.Num() == 0)
    {
        Tokens.Add(Trimmed);
    }

    for (FString Token : Tokens)
    {
        const FString TrimmedToken = Token.TrimStartAndEnd();
        if (TrimmedToken.IsEmpty())
        {
            continue;
        }
        if (TrimmedToken == TEXT("*") || TrimmedToken.Equals(TEXT("all"), ESearchCase::IgnoreCase))
        {
            return true;
        }
        if (TrimmedToken.Equals(NodeId, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }

    return false;
}

bool RshipObjectTargetsLocalNode(
    const TSharedPtr<FJsonObject>& JsonObject,
    const FString& NodeId,
    bool& bHasTargetFilter)
{
    static const TCHAR* TargetFields[] = { TEXT("targetNodeId"), TEXT("targetNodeIds"), TEXT("targetIds") };

    for (const TCHAR* FieldName : TargetFields)
    {
        FString StringValue;
        if (JsonObject->TryGetStringField(FieldName, StringValue))
        {
            bHasTargetFilter = true;
            if (RshipTargetTokenMatchesLocalNode(StringValue, NodeId))
            {
                return true;
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ArrayValues = nullptr;
        if (JsonObject->TryGetArrayField(FieldName, ArrayValues) && ArrayValues)
        {
            bHasTargetFilter = true;
            for (const TSharedPtr<FJsonValue>& Value : *ArrayValues)
            {
                FString Element;
                if (Value.IsValid() && Value->TryGetString(Element) &&
                    RshipTargetTokenMatchesLocalNode(Element, NodeId))
                {
                    return true;
                }
            }
        }
    }

    return false;
}


}

void URshipSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipSubsystem::Initialize"));

    // Initialize connection state
    ConnectionState = ERshipConnectionState::Disconnected;
    ReconnectAttempts = 0;
    bUsingHighPerfWebSocket = false;
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
    ContentMappingManager = nullptr;
    DisplayManager = nullptr;
    LastTickTime = 0.0;
    InboundQueueHead = 0;
    InboundQueue.Reset();
    InboundFrameCounter = 0;
    NextInboundSequence = 1;
    InboundDroppedMessages = 0;
    InboundTargetFilteredMessages = 0;
    InboundDroppedExactFrameMessages = 0;
    InboundAppliedMessages = 0;
    InboundAppliedLatencyMsTotal = 0.0;
    bLoggedInboundAuthorityDrop = false;
    bLoggedInboundExactFrameDrop = false;
    bLoggedInboundQueueCapacityDrop = false;
    InitializeInboundMessagePolicy();
    const URshipSettings* InitialSettings = GetDefault<URshipSettings>();
    bInboundRequireExactFrame = InitialSettings ? InitialSettings->bInboundRequireExactFrame : false;

    const int32 CVarRequireExactFrame = CVarRshipInboundRequireExactFrame.GetValueOnGameThread();
    if (CVarRequireExactFrame >= 0)
    {
        bInboundRequireExactFrame = CVarRequireExactFrame != 0;
    }

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    ControlSyncRateHz = FMath::Max(1.0f, Settings->ControlSyncRateHz);
    InboundApplyLeadFrames = FMath::Max(1, Settings->InboundApplyLeadFrames);
    InboundQueueMaxLength = FMath::Max(1, Settings->MaxQueueLength);

    const float CVarSyncRateHz = CVarRshipControlSyncRateHz.GetValueOnGameThread();
    if (CVarSyncRateHz > 0.0f)
    {
        ControlSyncRateHz = FMath::Max(1.0f, CVarSyncRateHz);
    }
    const int32 CVarLeadFrames = CVarRshipInboundApplyLeadFrames.GetValueOnGameThread();
    if (CVarLeadFrames > 0)
    {
        InboundApplyLeadFrames = FMath::Max(1, CVarLeadFrames);
    }

    // Initialize rate limiter
    InitializeRateLimiter();

    // Connect to server
    Reconnect();

    auto world = GetWorld();

    if (world != nullptr)
    {
        this->EmitterHandler = GetWorld()->SpawnActor<AEmitterHandler>();
    }

    this->TargetComponents = new TMap<FString, URshipTargetComponent*>;

    // Start subsystem tick timer (60Hz for smooth updates)
    if (world != nullptr)
    {
        world->GetTimerManager().SetTimer(
            SubsystemTickTimerHandle,
            this,
            &URshipSubsystem::TickSubsystems,
            1.0f / ControlSyncRateHz,
            true  // Looping
        );
    }

    if (Settings->bEnableContentMapping)
    {
        GetContentMappingManager();
    }
    if (Settings->bEnableDisplayManagement)
    {
        GetDisplayManager();
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
    Config.BackoffJitterPercent = Settings->ReconnectJitterPercent;
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
    // If we're backing off, cancel the timer and proceed with manual reconnect
    if (ConnectionState == ERshipConnectionState::BackingOff)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Manual reconnect requested during backoff - cancelling scheduled reconnect"));
        if (auto World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(ReconnectTimerHandle);
        }
        ReconnectAttempts = 0;  // Reset attempts on manual reconnect
    }
    // Don't reconnect if already actively connecting
    else if (ConnectionState == ERshipConnectionState::Connecting)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Reconnect called while already connecting, ignoring"));
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

    // Set connection timeout (10 seconds)
    if (auto World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ConnectionTimeoutHandle);
        World->GetTimerManager().SetTimer(
            ConnectionTimeoutHandle,
            this,
            &URshipSubsystem::OnConnectionTimeout,
            10.0f,  // 10 second timeout
            false   // Not looping
        );
    }

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

void URshipSubsystem::ConnectTo(const FString& Host, int32 Port)
{
    // Update settings with new values
    URshipSettings* Settings = GetMutableDefault<URshipSettings>();
    if (Settings)
    {
        Settings->rshipHostAddress = Host;
        Settings->rshipServerPort = Port;
        Settings->SaveConfig();

        UE_LOG(LogRshipExec, Log, TEXT("Updated server to %s:%d, reconnecting..."), *Host, Port);
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

void URshipSubsystem::EnqueueReplicatedInboundMessage(const FString& Message, int64 TargetApplyFrame,
                                                    bool bTargetApplyFrameWasExplicit)
{
    EnqueueInboundMessage(Message, true, TargetApplyFrame, nullptr, bTargetApplyFrameWasExplicit);
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

    // Clear any pending reconnect timer and connection timeout
    if (auto World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReconnectTimerHandle);
        World->GetTimerManager().ClearTimer(ConnectionTimeoutHandle);
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
        if (bUsingHighPerfWebSocket && HighPerfWebSocket.IsValid())
        {
            HighPerfWebSocket->Send(PingJson);
        }
        else if (WebSocket.IsValid())
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

    // Ensure queue processing timer is running (may have failed during early init)
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const int32 PendingOutboundMessages = RateLimiter ? RateLimiter->GetQueueLength() : 0;
    if (Settings->bEnableRateLimiting && PendingOutboundMessages > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("Starting queue processing timer (was not running)"));
        ScheduleQueueProcessTimer(Settings->QueueProcessInterval, true);
    }
}

void URshipSubsystem::OnWebSocketConnectionError(const FString &Error)
{
    UE_LOG(LogRshipExec, Warning, TEXT("WebSocket connection error: %s"), *Error);

    ConnectionState = ERshipConnectionState::Disconnected;

    // Clear connection timeout
    ClearQueueProcessTimer();
    if (auto World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ConnectionTimeoutHandle);
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
        ClearQueueProcessTimer();
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

    ClearQueueProcessTimer();

    // Schedule reconnection if enabled and this wasn't a clean close
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (Settings->bAutoReconnect && !bWasClean)
    {
        ScheduleReconnect();
    }
}

void URshipSubsystem::OnWebSocketMessage(const FString &Message)
{
    if (Message.IsEmpty())
    {
        return;
    }

    TSharedPtr<FJsonObject> ParsedPayload;
    int64 ExplicitApplyFrame = INDEX_NONE;
    bool bTargetApplyFrameWasExplicit = false;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (FJsonSerializer::Deserialize(Reader, ParsedPayload) && ParsedPayload.IsValid())
    {
        bTargetApplyFrameWasExplicit = TryGetExplicitApplyFrameFromObject(ParsedPayload, ExplicitApplyFrame);
        EnqueueInboundMessage(
            Message,
            false,
            bTargetApplyFrameWasExplicit ? ExplicitApplyFrame : INDEX_NONE,
            ParsedPayload,
            bTargetApplyFrameWasExplicit);
        return;
    }

    EnqueueInboundMessage(Message, false, INDEX_NONE, nullptr, false);
}

void URshipSubsystem::InitializeInboundMessagePolicy()
{
    bInboundAuthorityOnly = CVarRshipInboundAuthorityOnly.GetValueOnGameThread() != 0;

    FString ParsedNodeId;
    FParse::Value(FCommandLine::Get(), TEXT("dc_node="), ParsedNodeId);
    if (ParsedNodeId.IsEmpty())
    {
        ParsedNodeId = FPlatformMisc::GetEnvironmentVariable(TEXT("RSHIP_NODE_ID"));
    }

    FString ParsedAuthorityId;
    FParse::Value(FCommandLine::Get(), TEXT("rship_authority_node="), ParsedAuthorityId);
    if (ParsedAuthorityId.IsEmpty())
    {
        ParsedAuthorityId = FPlatformMisc::GetEnvironmentVariable(TEXT("RSHIP_AUTHORITY_NODE"));
    }
    if (ParsedAuthorityId.IsEmpty())
    {
        ParsedAuthorityId = TEXT("node_0");
    }

    if (ParsedNodeId.IsEmpty())
    {
        ParsedNodeId = ParsedAuthorityId;
    }

    InboundNodeId = ParsedNodeId;
    InboundAuthorityNodeId = ParsedAuthorityId;
    bIsAuthorityIngestNode = InboundNodeId.Equals(InboundAuthorityNodeId, ESearchCase::IgnoreCase);

    UE_LOG(LogRshipExec, Log,
        TEXT("Inbound policy: authorityOnly=%d nodeId=%s authorityNode=%s ingest=%s"),
        bInboundAuthorityOnly ? 1 : 0,
        *InboundNodeId,
        *InboundAuthorityNodeId,
        bIsAuthorityIngestNode ? TEXT("ENABLED") : TEXT("DISABLED"));
}

bool URshipSubsystem::IsInboundMessageTargetedToLocalNode(const FString& Message) const
{
    if (InboundNodeId.IsEmpty())
    {
        return true;
    }

    if (Message.IsEmpty())
    {
        return true;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return true;
    }

    return IsInboundMessageTargetedToLocalNode(JsonObject);
}

bool URshipSubsystem::IsInboundMessageTargetedToLocalNode(const TSharedPtr<FJsonObject>& JsonObject) const
{
    if (InboundNodeId.IsEmpty() || !JsonObject.IsValid())
    {
        return true;
    }

    bool bHasTargetFilter = false;
    if (RshipObjectTargetsLocalNode(JsonObject, InboundNodeId, bHasTargetFilter))
    {
        return true;
    }

    const TSharedPtr<FJsonObject>* DataObjectPtr = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("data"), DataObjectPtr) && DataObjectPtr && DataObjectPtr->IsValid())
    {
        if (RshipObjectTargetsLocalNode(*DataObjectPtr, InboundNodeId, bHasTargetFilter))
        {
            return true;
        }
    }

    return !bHasTargetFilter;
}

void URshipSubsystem::EnqueueInboundMessage(const FString& Message, bool bBypassAuthorityGate, int64 TargetApplyFrame,
                                           const TSharedPtr<FJsonObject>& ParsedPayload,
                                           bool bTargetApplyFrameWasExplicit)
{
    if (Message.IsEmpty())
    {
        return;
    }

    TSharedPtr<FJsonObject> EffectivePayload = ParsedPayload;
    if (!bBypassAuthorityGate && !EffectivePayload.IsValid())
    {
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
        FJsonSerializer::Deserialize(Reader, EffectivePayload);
    }

    if (!bBypassAuthorityGate && !IsInboundMessageTargetedToLocalNode(EffectivePayload))
    {
        FScopeLock Lock(&InboundQueueMutex);
        ++InboundDroppedMessages;
        ++InboundTargetFilteredMessages;
        return;
    }

    if (!bBypassAuthorityGate && bInboundAuthorityOnly && !bIsAuthorityIngestNode)
    {
        bool bShouldLogDrop = false;
        {
            FScopeLock Lock(&InboundQueueMutex);
            ++InboundDroppedMessages;
            if (!bLoggedInboundAuthorityDrop)
            {
                bLoggedInboundAuthorityDrop = true;
                bShouldLogDrop = true;
            }
        }

        if (bShouldLogDrop)
        {
            UE_LOG(LogRshipExec, Warning,
                TEXT("Dropping inbound websocket payloads on non-authority node %s (authority=%s). Use replicated authoritative events instead."),
                *InboundNodeId,
                *InboundAuthorityNodeId);
        }
        return;
    }

    FRshipInboundQueuedMessage Queued;
    int64 AssignedApplyFrame = 0;
    bool bShouldLogDrop = false;

    if (bInboundRequireExactFrame && bTargetApplyFrameWasExplicit && TargetApplyFrame != INDEX_NONE
        && TargetApplyFrame <= InboundFrameCounter)
    {
        bool bShouldLogExactFrameDrop = false;
        {
            FScopeLock Lock(&InboundQueueMutex);
            ++InboundDroppedMessages;
            ++InboundDroppedExactFrameMessages;
            if (!bLoggedInboundExactFrameDrop)
            {
                bLoggedInboundExactFrameDrop = true;
                bShouldLogExactFrameDrop = true;
            }
        }
        if (bShouldLogExactFrameDrop)
        {
            UE_LOG(LogRshipExec, Warning,
                TEXT("Exact-frame enforcement dropped stale inbound payload for frame %lld (current frame %lld)."),
                TargetApplyFrame,
                InboundFrameCounter);
        }
        return;
    }

    {
        FScopeLock Lock(&InboundQueueMutex);
        int32 ActiveMessageCount = GetActiveInboundQueueCount();
        if (ActiveMessageCount >= InboundQueueMaxLength)
        {
            const int32 NumToDrop = ActiveMessageCount - InboundQueueMaxLength + 1;
            if (NumToDrop > 0)
                {
                    InboundQueueHead += NumToDrop;
                    InboundDroppedMessages += NumToDrop;

                if (!bLoggedInboundQueueCapacityDrop)
                {
                    bLoggedInboundQueueCapacityDrop = true;
                    bShouldLogDrop = true;
                }
            }
        }

        if (InboundQueueHead >= InboundQueue.Num())
        {
            InboundQueue.Reset();
            InboundQueueHead = 0;
            ActiveMessageCount = 0;
        }

        Queued.Sequence = NextInboundSequence++;
        const int64 NextFrame = InboundFrameCounter + FMath::Max<int64>(1, InboundApplyLeadFrames);
        if (TargetApplyFrame == INDEX_NONE || !bTargetApplyFrameWasExplicit)
        {
            Queued.ApplyFrame = (TargetApplyFrame == INDEX_NONE)
                ? NextFrame
                : FMath::Max<int64>(TargetApplyFrame, NextFrame);
        }
        else
        {
            // Prevent stale explicit frames from slipping behind scheduling and causing cross-node divergence.
            // Non-exact mode still attempts deterministic ordering, but always aligns work to at least the next scheduled frame.
            Queued.ApplyFrame = FMath::Max<int64>(TargetApplyFrame, NextFrame);
        }
        AssignedApplyFrame = Queued.ApplyFrame;
        Queued.EnqueueTimeSeconds = FPlatformTime::Seconds();
        Queued.Payload = Message;
        Queued.ParsedPayload = EffectivePayload;

        int32 Low = 0;
        int32 High = ActiveMessageCount;
        auto QueueSortLess = [](const FRshipInboundQueuedMessage& A, const FRshipInboundQueuedMessage& B)
        {
            if (A.ApplyFrame != B.ApplyFrame)
            {
                return A.ApplyFrame < B.ApplyFrame;
            }
            return A.Sequence < B.Sequence;
        };
        while (Low < High)
        {
            const int32 Mid = (Low + High) / 2;
            if (QueueSortLess(InboundQueue[InboundQueueHead + Mid], Queued))
            {
                Low = Mid + 1;
            }
            else
            {
                High = Mid;
            }
        }
        const int32 InsertIndex = InboundQueueHead + FMath::Clamp(Low, 0, ActiveMessageCount);
        InboundQueue.Insert(MoveTemp(Queued), InsertIndex);
        ActiveMessageCount = GetActiveInboundQueueCount();

        if (InboundQueueHead > 0 && ActiveMessageCount > 0 && InboundQueueHead > FMath::Max(256, ActiveMessageCount / 2))
        {
            CompactInboundQueue_NoLock();
        }
    }

    if (bShouldLogDrop)
    {
        UE_LOG(LogRshipExec, Warning,
            TEXT("Inbound queue full (max=%d), dropped oldest message(s). Enqueuing continues at capacity."), InboundQueueMaxLength);
    }

    if (!bBypassAuthorityGate && bInboundAuthorityOnly && bIsAuthorityIngestNode)
    {
        OnAuthoritativeInboundQueuedDelegate.Broadcast(Message, AssignedApplyFrame);
    }
}

void URshipSubsystem::ProcessInboundMessageQueue()
{
    const int32 MaxMessagesPerTick = FMath::Max(1, CVarRshipInboundMaxMessagesPerTick.GetValueOnGameThread());
    TArray<FRshipInboundQueuedMessage> MessagesToApply;
    int32 RemainingCount = 0;

    {
        FScopeLock Lock(&InboundQueueMutex);
        const int32 ActiveMessageCount = GetActiveInboundQueueCount();
        if (ActiveMessageCount == 0)
        {
            return;
        }

        int32 ApplyCount = 0;
        const int32 MaxCandidates = FMath::Min(MaxMessagesPerTick, ActiveMessageCount);
        MessagesToApply.Reserve(MaxCandidates);
        for (int32 Index = 0; Index < MaxCandidates; ++Index)
        {
            FRshipInboundQueuedMessage& Message = InboundQueue[InboundQueueHead + Index];
            if (Message.ApplyFrame > InboundFrameCounter)
            {
                break;
            }
            MessagesToApply.Add(MoveTemp(Message));
            ++ApplyCount;
        }

        if (ApplyCount == 0)
        {
            return;
        }

        InboundQueueHead += ApplyCount;
        if (ApplyCount == ActiveMessageCount)
        {
            InboundQueue.Reset();
            InboundQueueHead = 0;
            RemainingCount = 0;
        }
        else
        {
            if (InboundQueueHead > 0 && InboundQueueHead > FMath::Max(256, ActiveMessageCount / 2))
            {
                CompactInboundQueue_NoLock();
            }

            RemainingCount = GetActiveInboundQueueCount();
        }
    }

    const double ApplyTimeSeconds = FPlatformTime::Seconds();
    double LatencyAccumMs = 0.0;
    int64 AppliedCount = 0;
    for (const FRshipInboundQueuedMessage& Message : MessagesToApply)
    {
        LatencyAccumMs += FMath::Max(0.0, (ApplyTimeSeconds - Message.EnqueueTimeSeconds) * 1000.0);
        ++AppliedCount;
        ProcessMessage(Message.Payload, Message.ParsedPayload);
    }

    if (AppliedCount > 0)
    {
        FScopeLock Lock(&InboundQueueMutex);
        InboundAppliedMessages += AppliedCount;
        InboundAppliedLatencyMsTotal += LatencyAccumMs;
    }

    if (RemainingCount > 0)
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("Inbound queue backlog=%d after apply (%d this tick)"),
            RemainingCount, MessagesToApply.Num());
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

    const float JitterPercent = FMath::Clamp(Settings->ReconnectJitterPercent, 0.0f, 100.0f);
    if (JitterPercent > 0.0f)
    {
        const float JitterWindow = BackoffDelay * (JitterPercent * 0.01f);
        const float MinDelay = FMath::Max(0.05f, BackoffDelay - JitterWindow);
        const float MaxDelay = FMath::Max(MinDelay, BackoffDelay + JitterWindow);
        BackoffDelay = FMath::FRandRange(MinDelay, MaxDelay);
    }

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
    if (HighPerfWebSocket)
    {
        HighPerfWebSocket->Close();
        HighPerfWebSocket.Reset();
    }

    ConnectionState = ERshipConnectionState::Disconnected;
    ClearQueueProcessTimer();
    if (auto World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ConnectionTimeoutHandle);
    }

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

void URshipSubsystem::ProcessMessageQueue()
{
    if (!RateLimiter)
    {
        return;
    }

    const int32 QueueSize = RateLimiter->GetQueueLength();
    const bool bConnected = (ConnectionState == ERshipConnectionState::Connected);
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const float Interval = Settings ? Settings->QueueProcessInterval : 0.016f;
    if (!bConnected)
    {
        ClearQueueProcessTimer();
        return;
    }

    if (QueueSize > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("ProcessMessageQueue: Queue has %d messages, processing..."), QueueSize);
    }

    int32 Sent = RateLimiter->ProcessQueue();
    const int32 RemainingQueueSize = RateLimiter->GetQueueLength();

    if (Sent > 0 || QueueSize > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("ProcessMessageQueue: Sent %d messages, %d remaining"), Sent, RemainingQueueSize);
    }

    if (RemainingQueueSize == 0)
    {
        ClearQueueProcessTimer();
        return;
    }

    if (RateLimiter && RateLimiter->IsBackingOff())
    {
        const float BackoffRemaining = FMath::Max(0.05f, RateLimiter->GetBackoffRemaining());
        ScheduleQueueProcessTimer(BackoffRemaining, false);
    }
    else
    {
        ScheduleQueueProcessTimer(Interval, true);
    }
}

void URshipSubsystem::TickSubsystems()
{
    const float CVarSyncRateHz = CVarRshipControlSyncRateHz.GetValueOnGameThread();
    if (CVarSyncRateHz > 0.0f && !FMath::IsNearlyEqual(ControlSyncRateHz, FMath::Max(1.0f, CVarSyncRateHz)))
    {
        SetControlSyncRateHz(CVarSyncRateHz);
    }

    const int32 CVarLeadFrames = CVarRshipInboundApplyLeadFrames.GetValueOnGameThread();
    if (CVarLeadFrames > 0 && InboundApplyLeadFrames != FMath::Max(1, CVarLeadFrames))
    {
        SetInboundApplyLeadFrames(CVarLeadFrames);
    }

    const int32 CVarRequireExactFrame = CVarRshipInboundRequireExactFrame.GetValueOnGameThread();
    if (CVarRequireExactFrame >= 0)
    {
        const bool bRequireExactFrame = CVarRequireExactFrame != 0;
        if (bInboundRequireExactFrame != bRequireExactFrame)
        {
            SetInboundRequireExactFrame(bRequireExactFrame);
        }
    }

    ++InboundFrameCounter;
    ProcessInboundMessageQueue();

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

    // Tick Content Mapping manager for render contexts and mappings
    if (ContentMappingManager)
    {
        ContentMappingManager->Tick(DeltaTime);
    }

    // Tick Display manager for monitor topology and pixel routing state
    if (DisplayManager)
    {
        DisplayManager->Tick(DeltaTime);
    }
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
        if (ConnectionState == ERshipConnectionState::Connected)
        {
            ScheduleQueueProcessTimer(Settings->QueueProcessInterval, true);
        }

        UE_LOG(LogRshipExec, Log, TEXT("Enqueued message (Key=%s, QueueLen=%d)"), *CoalesceKey, RateLimiter->GetQueueLength());
    }
}

void URshipSubsystem::ClearQueueProcessTimer()
{
    if (auto World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(QueueProcessTimerHandle);
    }
}

void URshipSubsystem::ScheduleQueueProcessTimer(float IntervalSeconds, bool bLooping)
{
    const float SafeInterval = FMath::Max(0.001f, IntervalSeconds);
    if (auto World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            QueueProcessTimerHandle,
            this,
            &URshipSubsystem::ProcessMessageQueue,
            SafeInterval,
            bLooping
        );
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

void URshipSubsystem::ProcessMessage(const FString &message, const TSharedPtr<FJsonObject>& ParsedPayload)
{
    TSharedPtr<FJsonObject> obj = ParsedPayload;
    if (!obj.IsValid())
    {
        obj = MakeShareable(new FJsonObject);
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(message);

        if (!(FJsonSerializer::Deserialize(Reader, obj) && obj.IsValid()))
        {
            return;
        }
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

            // Route display management targets
            if (targetId.StartsWith(TEXT("/display-management/")))
            {
                if (URshipDisplayManager* Manager = GetDisplayManager())
                {
                    result = Manager->RouteAction(targetId, actionId, execData);
                }
                else
                {
                    UE_LOG(LogRshipExec, Warning, TEXT("Display action received but manager not initialized: %s"), *targetId);
                }
            }
            // Route content mapping targets
            else if (targetId.StartsWith(TEXT("/content-mapping/")))
            {
                if (URshipContentMappingManager* MappingManager = GetContentMappingManager())
                {
                    result = MappingManager->RouteAction(targetId, actionId, execData);
                }
                else
                {
                    UE_LOG(LogRshipExec, Warning, TEXT("Content mapping action received but manager not initialized: %s"), *targetId);
                }
            }
            // Check if this is a PCG target (paths start with "/pcg/")
            else if (targetId.StartsWith(TEXT("/pcg/")))
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
                // Standard target component routing - O(1) lookup by target ID
                URshipTargetComponent* comp = FindTargetComponent(targetId);
                if (comp)
                {
                    Target* target = comp->TargetData;
                    AActor* owner = comp->GetOwner();

                    if (target != nullptr)
                    {
                        bool takeResult = target->TakeAction(owner, actionId, execData);
                        result |= takeResult;
                        comp->OnDataReceived();
                    }
                    else
                    {
                        UE_LOG(LogRshipExec, Warning, TEXT("Target data null for: %s"), *targetId);
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
        else if (itemType == TEXT("RenderContext"))
        {
            if (URshipContentMappingManager* MappingManager = GetContentMappingManager())
            {
                MappingManager->ProcessRenderContextEvent(item, bIsDelete);
            }
        }
        else if (itemType == TEXT("MappingSurface"))
        {
            if (URshipContentMappingManager* MappingManager = GetContentMappingManager())
            {
                MappingManager->ProcessMappingSurfaceEvent(item, bIsDelete);
            }
        }
        else if (itemType == TEXT("Mapping"))
        {
            if (URshipContentMappingManager* MappingManager = GetContentMappingManager())
            {
                MappingManager->ProcessMappingEvent(item, bIsDelete);
            }
        }
        else if (itemType == TEXT("DisplayProfile"))
        {
            if (URshipDisplayManager* Manager = GetDisplayManager())
            {
                Manager->ProcessProfileEvent(item, bIsDelete);
            }
        }
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
        world->GetTimerManager().ClearTimer(SubsystemTickTimerHandle);
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

    // Shutdown Content Mapping manager
    if (ContentMappingManager)
    {
        ContentMappingManager->Shutdown();
        ContentMappingManager = nullptr;
    }

    // Shutdown Display manager
    if (DisplayManager)
    {
        DisplayManager->Shutdown();
        DisplayManager = nullptr;
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
    const auto& Actions = target->GetActions();
    const auto& Emitters = target->GetEmitters();

    UE_LOG(LogRshipExec, Log, TEXT("SendTarget: %s - %d actions, %d emitters"),
        *target->GetId(),
        Actions.Num(),
        Emitters.Num());

    TArray<TSharedPtr<FJsonValue>> EmitterIdsJson;
    TArray<TSharedPtr<FJsonValue>> ActionIdsJson;

    for (auto &Elem : Actions)
    {
        UE_LOG(LogRshipExec, Log, TEXT("  Action: %s"), *Elem.Key);
        ActionIdsJson.Push(MakeShareable(new FJsonValueString(Elem.Key)));
        SendAction(Elem.Value, target->GetId());
    }

    for (auto &Elem : Emitters)
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

    const auto& Actions = target->GetActions();
    const auto& Emitters = target->GetEmitters();

    UE_LOG(LogRshipExec, Log, TEXT("DeleteTarget: %s - removing %d actions, %d emitters"),
        *target->GetId(),
        Actions.Num(),
        Emitters.Num());

    // Send DEL events for all actions
    for (auto& Elem : Actions)
    {
        TSharedPtr<FJsonObject> ActionDel = MakeShareable(new FJsonObject);
        ActionDel->SetStringField(TEXT("id"), Elem.Key);
        DelItem("Action", ActionDel, ERshipMessagePriority::High, Elem.Key + ":del");
    }

    // Send DEL events for all emitters
    for (auto& Elem : Emitters)
    {
        TSharedPtr<FJsonObject> EmitterDel = MakeShareable(new FJsonObject);
        EmitterDel->SetStringField(TEXT("id"), Elem.Key);
        DelItem("Emitter", EmitterDel, ERshipMessagePriority::High, Elem.Key + ":del");
    }

    // Send TargetStatus offline
    TSharedPtr<FJsonObject> TargetStatus = MakeShareable(new FJsonObject);
    TargetStatus->SetStringField(TEXT("targetId"), target->GetId());
    TargetStatus->SetStringField(TEXT("instanceId"), InstanceId);
    TargetStatus->SetStringField(TEXT("status"), TEXT("offline"));
    TargetStatus->SetStringField(TEXT("id"), target->GetId());
    TargetStatus->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    SetItem("TargetStatus", TargetStatus, ERshipMessagePriority::High, target->GetId() + ":status");

    // Send DEL event for the target itself
    TSharedPtr<FJsonObject> TargetDel = MakeShareable(new FJsonObject);
    TargetDel->SetStringField(TEXT("id"), target->GetId());
    DelItem("Target", TargetDel, ERshipMessagePriority::High, target->GetId() + ":del");

    UE_LOG(LogRshipExec, Log, TEXT("DeleteTarget: %s - deletion events sent"), *target->GetId());
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

void URshipSubsystem::DelItem(FString itemType, TSharedPtr<FJsonObject> data, ERshipMessagePriority Priority, const FString& CoalesceKey)
{
    // MakeDel produces the complete WSMEvent format with changeType: "DEL"
    TSharedPtr<FJsonObject> payload = MakeDel(itemType, data);

    // Debug: Log deletion events
    if (itemType == TEXT("Target") || itemType == TEXT("Action") || itemType == TEXT("Emitter"))
    {
        FString JsonString;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
        FJsonSerializer::Serialize(payload.ToSharedRef(), JsonWriter);
        UE_LOG(LogRshipExec, Log, TEXT("DelItem [%s]: %s"), *itemType, *JsonString);
    }

    QueueMessage(payload, Priority, ERshipMessageType::Registration, CoalesceKey);
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

    const auto& emitters = target->GetEmitters();
    EmitterContainer* const* FoundEmitter = emitters.Find(fullEmitterId);
    if (!FoundEmitter)
    {
        return nullptr;
    }

    return *FoundEmitter;
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

// ============================================================================
// CONTENT MAPPING MANAGER
// ============================================================================

URshipContentMappingManager* URshipSubsystem::GetContentMappingManager()
{
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (Settings && !Settings->bEnableContentMapping)
    {
        return nullptr;
    }

    if (!ContentMappingManager)
    {
        ContentMappingManager = NewObject<URshipContentMappingManager>(this);
        ContentMappingManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("ContentMappingManager initialized"));
    }
    return ContentMappingManager;
}

URshipDisplayManager* URshipSubsystem::GetDisplayManager()
{
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (Settings && !Settings->bEnableDisplayManagement)
    {
        return nullptr;
    }

    if (!DisplayManager)
    {
        DisplayManager = NewObject<URshipDisplayManager>(this);
        DisplayManager->Initialize(this);

        UE_LOG(LogRshipExec, Log, TEXT("DisplayManager initialized"));
    }
    return DisplayManager;
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
        TargetComponents->Remove(KeyToRemove);
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

int32 URshipSubsystem::GetInboundQueueLength() const
{
    FScopeLock Lock(&InboundQueueMutex);
    return GetActiveInboundQueueCount();
}

int32 URshipSubsystem::GetInboundDroppedMessages() const
{
    FScopeLock Lock(&InboundQueueMutex);
    return InboundDroppedMessages;
}

int32 URshipSubsystem::GetInboundTargetFilteredMessages() const
{
    FScopeLock Lock(&InboundQueueMutex);
    return InboundTargetFilteredMessages;
}

int32 URshipSubsystem::GetInboundExactFrameDroppedMessages() const
{
    FScopeLock Lock(&InboundQueueMutex);
    return InboundDroppedExactFrameMessages;
}

int64 URshipSubsystem::GetInboundFrameCounter() const
{
    FScopeLock Lock(&InboundQueueMutex);
    return InboundFrameCounter;
}

int64 URshipSubsystem::GetInboundNextPlannedApplyFrame() const
{
    FScopeLock Lock(&InboundQueueMutex);
    return InboundFrameCounter + FMath::Max<int64>(1, InboundApplyLeadFrames);
}

int64 URshipSubsystem::GetInboundQueuedOldestApplyFrame() const
{
    FScopeLock Lock(&InboundQueueMutex);
    if (GetActiveInboundQueueCount() == 0)
    {
        return INDEX_NONE;
    }
    return InboundQueue[InboundQueueHead].ApplyFrame;
}

int64 URshipSubsystem::GetInboundQueuedNewestApplyFrame() const
{
    FScopeLock Lock(&InboundQueueMutex);
    if (GetActiveInboundQueueCount() == 0)
    {
        return INDEX_NONE;
    }
    return InboundQueue.Last().ApplyFrame;
}

float URshipSubsystem::GetInboundAverageApplyLatencyMs() const
{
    FScopeLock Lock(&InboundQueueMutex);
    if (InboundAppliedMessages <= 0)
    {
        return 0.0f;
    }
    return static_cast<float>(InboundAppliedLatencyMsTotal / static_cast<double>(InboundAppliedMessages));
}

bool URshipSubsystem::IsAuthoritativeIngestNode() const
{
    return !bInboundAuthorityOnly || bIsAuthorityIngestNode;
}

bool URshipSubsystem::IsInboundRequireExactFrame() const
{
    return bInboundRequireExactFrame;
}

void URshipSubsystem::SetControlSyncRateHz(float SyncRateHz)
{
    ControlSyncRateHz = FMath::Max(1.0f, SyncRateHz);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            SubsystemTickTimerHandle,
            this,
            &URshipSubsystem::TickSubsystems,
            1.0f / ControlSyncRateHz,
            true);
    }
}

void URshipSubsystem::SetInboundApplyLeadFrames(int32 LeadFrames)
{
    InboundApplyLeadFrames = FMath::Max(1, LeadFrames);
}

void URshipSubsystem::SetInboundRequireExactFrame(bool bRequireExactFrame)
{
    bInboundRequireExactFrame = bRequireExactFrame;
}
