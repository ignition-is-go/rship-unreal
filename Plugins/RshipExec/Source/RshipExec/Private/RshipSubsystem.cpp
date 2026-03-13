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
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "UObject/FieldPath.h"
#include "Misc/StructBuilder.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "UObject/UnrealTypePrivate.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/Target.h"
#include "Core/RshipEntityRecords.h"
#include "Core/RshipEntitySerializer.h"
#include "Transport/RshipMykoTransport.h"



#include "Logs.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#if RSHIP_HAS_DISPLAY_CLUSTER
#include "IDisplayCluster.h"
#include "DisplayClusterRootActor.h"
#include "Cluster/IDisplayClusterClusterManager.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#endif

using namespace std;

namespace
{
static TAutoConsoleVariable<float> CVarRshipNDisplayRenderDomainRefreshIntervalSeconds(
    TEXT("r.Rship.NDisplay.RenderDomainRefreshIntervalSeconds"),
    1.0f,
    TEXT("How often (seconds) to rebuild cached nDisplay-derived render domains from config.")
);

static TAutoConsoleVariable<float> CVarRshipNDisplayRenderDomainPublishIntervalSeconds(
    TEXT("r.Rship.NDisplay.RenderDomainPublishIntervalSeconds"),
    0.25f,
    TEXT("How often (seconds) to publish this instance's nDisplay-derived render domain metadata to Rship.")
);

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

#if RSHIP_HAS_DISPLAY_CLUSTER
UWorld* FindRuntimeWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (!World)
		{
			continue;
		}

		if (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
		{
			return World;
		}
	}

	return nullptr;
}

FString ExtractCommandLineValueBestEffort(const FString& Cmd, const FString& Key)
{
	auto ExtractAfter = [&Cmd](const FString& Marker) -> FString
	{
		const int32 Start = Cmd.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (Start == INDEX_NONE)
		{
			return FString();
		}

		int32 ValueStart = Start + Marker.Len();
		while (ValueStart < Cmd.Len() && FChar::IsWhitespace(Cmd[ValueStart]))
		{
			++ValueStart;
		}
		if (ValueStart >= Cmd.Len())
		{
			return FString();
		}

		const bool bQuoted = Cmd[ValueStart] == TEXT('"');
		if (bQuoted)
		{
			++ValueStart;
		}

		int32 ValueEnd = ValueStart;
		while (ValueEnd < Cmd.Len())
		{
			const TCHAR C = Cmd[ValueEnd];
			if (bQuoted)
			{
				if (C == TEXT('"'))
				{
					break;
				}
			}
			else if (FChar::IsWhitespace(C))
			{
				break;
			}
			++ValueEnd;
		}

		return Cmd.Mid(ValueStart, ValueEnd - ValueStart).TrimStartAndEnd();
	};

	FString Value = ExtractAfter(FString::Printf(TEXT("-%s="), *Key));
	if (Value.IsEmpty())
	{
		Value = ExtractAfter(FString::Printf(TEXT("-%s "), *Key));
	}
	if (Value.IsEmpty())
	{
		Value = ExtractAfter(FString::Printf(TEXT("%s="), *Key));
	}
	if (Value.IsEmpty())
	{
		Value = ExtractAfter(FString::Printf(TEXT("%s "), *Key));
	}

	return Value;
}

FString ResolveDisplayClusterNodeIdBestEffort()
{
	FString NodeId;

	if (IDisplayCluster::IsAvailable())
	{
		IDisplayCluster& DisplayCluster = IDisplayCluster::Get();
		if (IDisplayClusterClusterManager* ClusterManager = DisplayCluster.GetClusterMgr())
		{
			NodeId = ClusterManager->GetNodeId();
		}

		if (NodeId.IsEmpty())
		{
			if (IDisplayClusterConfigManager* ConfigManager = DisplayCluster.GetConfigMgr())
			{
				NodeId = ConfigManager->GetLocalNodeId();
			}
		}
	}

	// nDisplay launcher commonly passes -dc_node=<NodeId>.
	if (NodeId.IsEmpty())
	{
		FParse::Value(FCommandLine::Get(), TEXT("dc_node="), NodeId);
		if (NodeId.IsEmpty())
		{
			FParse::Value(FCommandLine::Get(), TEXT("dc_node"), NodeId);
		}

		if (NodeId.IsEmpty())
		{
			NodeId = ExtractCommandLineValueBestEffort(FCommandLine::Get(), TEXT("dc_node"));
		}
	}

	return NodeId;
}

bool IsDisplayClusterProcessBestEffort()
{
	const TCHAR* CmdLine = FCommandLine::Get();

	if (FParse::Param(CmdLine, TEXT("dc_cluster")))
	{
		return true;
	}

	FString ParsedValue;
	if (FParse::Value(CmdLine, TEXT("dc_node="), ParsedValue) && !ParsedValue.IsEmpty())
	{
		return true;
	}
	if (FParse::Value(CmdLine, TEXT("dc_cfg="), ParsedValue) && !ParsedValue.IsEmpty())
	{
		return true;
	}
	if (!ExtractCommandLineValueBestEffort(CmdLine, TEXT("dc_node")).IsEmpty())
	{
		return true;
	}
	if (!ExtractCommandLineValueBestEffort(CmdLine, TEXT("dc_cfg")).IsEmpty())
	{
		return true;
	}

	FString Cmd(CmdLine);
	Cmd.ToLowerInline();
	return Cmd.Contains(TEXT("-dc_cluster"))
		|| Cmd.Contains(TEXT("-dc_cfg"))
		|| Cmd.Contains(TEXT("-dc_node"))
		|| Cmd.Contains(TEXT("displaycluster.displayclustergameengine"))
		|| Cmd.Contains(TEXT("displaycluster.displayclusterviewportclient"));
}
#else
bool IsDisplayClusterProcessBestEffort()
{
    return false;
}
#endif

TSharedPtr<FJsonValue> CloneJsonValueWithoutVolatileFields(const TSharedPtr<FJsonValue>& Value);

TSharedPtr<FJsonObject> CloneJsonObjectWithoutVolatileFields(const TSharedPtr<FJsonObject>& Object)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Object.IsValid())
    {
        return Result;
    }

    TArray<FString> Keys;
    Object->Values.GetKeys(Keys);
    Keys.Sort();

    for (const FString& Key : Keys)
    {
        if (Key == TEXT("hash") || Key == TEXT("tx") || Key == TEXT("createdAt") || Key == TEXT("sourceId"))
        {
            continue;
        }

        const TSharedPtr<FJsonValue>* FieldValue = Object->Values.Find(Key);
        if (!FieldValue || !FieldValue->IsValid())
        {
            continue;
        }

        Result->SetField(Key, CloneJsonValueWithoutVolatileFields(*FieldValue));
    }

    return Result;
}

TSharedPtr<FJsonValue> CloneJsonValueWithoutVolatileFields(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return MakeShared<FJsonValueNull>();
    }

    switch (Value->Type)
    {
    case EJson::Object:
        return MakeShared<FJsonValueObject>(CloneJsonObjectWithoutVolatileFields(Value->AsObject()));
    case EJson::Array:
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const TSharedPtr<FJsonValue>& Elem : Value->AsArray())
        {
            Values.Add(CloneJsonValueWithoutVolatileFields(Elem));
        }
        return MakeShared<FJsonValueArray>(Values);
    }
    case EJson::String:
        return MakeShared<FJsonValueString>(Value->AsString());
    case EJson::Number:
        return MakeShared<FJsonValueNumber>(Value->AsNumber());
    case EJson::Boolean:
        return MakeShared<FJsonValueBoolean>(Value->AsBool());
    case EJson::Null:
    default:
        return MakeShared<FJsonValueNull>();
    }
}

FString CanonicalizeJsonObject(const TSharedPtr<FJsonObject>& Object)
{
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(CloneJsonObjectWithoutVolatileFields(Object).ToSharedRef(), Writer);
    return Output;
}

void AddRemoteItemsById(
    const TArray<TSharedPtr<FJsonValue>>& Upserts,
    TMap<FString, TSharedPtr<FJsonObject>>& OutItems)
{
    for (const TSharedPtr<FJsonValue>& WrappedValue : Upserts)
    {
        if (!WrappedValue.IsValid() || WrappedValue->Type != EJson::Object)
        {
            continue;
        }

        TSharedPtr<FJsonObject> Wrapped = WrappedValue->AsObject();
        if (!Wrapped.IsValid())
        {
            continue;
        }

        const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
        if (!Wrapped->TryGetObjectField(TEXT("item"), ItemPtr) || !ItemPtr || !ItemPtr->IsValid())
        {
            continue;
        }

        FString ItemId;
        if (!(*ItemPtr)->TryGetStringField(TEXT("id"), ItemId) || ItemId.IsEmpty())
        {
            continue;
        }

        OutItems.Add(ItemId, *ItemPtr);
    }
}

int32 EstimateJsonValueSize(const TSharedPtr<FJsonValue>& Value)
{
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
    return Output.Len();
}

TArray<TSharedPtr<FJsonObject>> BuildChunkedEventBatches(
    const TArray<TSharedPtr<FJsonValue>>& PayloadArray)
{
    TArray<TSharedPtr<FJsonObject>> Result;
    if (PayloadArray.Num() == 0)
    {
        return Result;
    }

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const int32 MaxBatchBytes = Settings ? FMath::Max(Settings->MaxBatchBytes, 16 * 1024) : 64 * 1024;
    const int32 WrapperOverheadBytes = 128;

    TArray<TSharedPtr<FJsonValue>> CurrentChunk;
    int32 CurrentChunkBytes = WrapperOverheadBytes;

    auto FlushChunk = [&]()
    {
        if (CurrentChunk.Num() == 0)
        {
            return;
        }

        TSharedPtr<FJsonObject> BatchWrapper = MakeShareable(new FJsonObject);
        BatchWrapper->SetStringField(TEXT("event"), RshipMykoEventNames::EventBatch);
        BatchWrapper->SetArrayField(TEXT("data"), CurrentChunk);
        Result.Add(BatchWrapper);

        CurrentChunk.Reset();
        CurrentChunkBytes = WrapperOverheadBytes;
    };

    for (const TSharedPtr<FJsonValue>& EventValue : PayloadArray)
    {
        if (!EventValue.IsValid())
        {
            continue;
        }

        const int32 EventBytes = EstimateJsonValueSize(EventValue);
        const bool bWouldOverflow = CurrentChunk.Num() > 0 && (CurrentChunkBytes + EventBytes + 1) > MaxBatchBytes;
        if (bWouldOverflow)
        {
            FlushChunk();
        }

        CurrentChunk.Add(EventValue);
        CurrentChunkBytes += EventBytes + 1;

        if (CurrentChunkBytes >= MaxBatchBytes)
        {
            FlushChunk();
        }
    }

    FlushChunk();
    return Result;
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

    const bool bShouldAutoConnect = bRemoteCommunicationEnabled;

    // Connect to server (if globally enabled and eligible in this process)
    if (bShouldAutoConnect)
    {
        Reconnect();
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Skipping initial connect"));
    }

    this->TargetComponents = new TMultiMap<FString, URshipActorRegistrationComponent*>;

    // Start queue processing ticker (works in editor without a world)
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (!QueueProcessTickerHandle.IsValid())
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
    RateLimiter.Reset();
    PendingOutboundMessages.Reset();
    PendingOutboundBytes = 0;
    MessagesSentPerSecondSnapshot = 0;
    BytesSentPerSecondSnapshot = 0;
    UE_LOG(LogRshipExec, Log, TEXT("Outbound pipeline initialized: rate limiting removed, direct send when connected, lossless queue while disconnected"));
}

void URshipSubsystem::Reconnect()
{
    const bool bIsDisplayClusterProcess = IsDisplayClusterProcessBestEffort();

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

    FString InstanceNodeSuffix;
#if RSHIP_HAS_DISPLAY_CLUSTER
    InstanceNodeSuffix = ResolveDisplayClusterNodeIdBestEffort();
#endif

    if (bIsDisplayClusterProcess && InstanceNodeSuffix.IsEmpty())
    {
        UE_LOG(LogRshipExec, Warning, TEXT("nDisplay process detected but dc_node unresolved. CmdLine='%s'"), FCommandLine::Get());
        ++DisplayClusterNodeResolveRetries;
        if (DisplayClusterNodeResolveRetries <= 10)
        {
            UE_LOG(LogRshipExec, Warning,
                TEXT("Reconnect deferred: running with -dc_cluster but node id not resolved yet (attempt %d)."),
                DisplayClusterNodeResolveRetries);

            if (ReconnectTickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
                ReconnectTickerHandle.Reset();
            }

            ReconnectTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateUObject(this, &URshipSubsystem::OnReconnectTick),
                0.5f
            );
            ConnectionState = ERshipConnectionState::Connecting;
            return;
        }

        UE_LOG(LogRshipExec, Warning,
            TEXT("Reconnect continuing without node id after %d attempts; instance id will omit node suffix."),
            DisplayClusterNodeResolveRetries);

        const uint32 ProcessId = FPlatformProcess::GetCurrentProcessId();
        InstanceNodeSuffix = FString::Printf(TEXT("unknown-%u"), ProcessId);
        UE_LOG(LogRshipExec, Warning, TEXT("Using fallback node suffix '%s' to avoid instance id collision."), *InstanceNodeSuffix);
    }
    else
    {
        DisplayClusterNodeResolveRetries = 0;
    }

    if (!InstanceNodeSuffix.IsEmpty())
    {
        InstanceId = MachineId + TEXT(":") + InstanceNodeSuffix + TEXT(":") + ServiceId;
    }
    else
    {
        InstanceId = MachineId + TEXT(":") + ServiceId;
    }
    UE_LOG(LogRshipExec, Log, TEXT("Resolved instance identity: isDC=%s node='%s' instanceId='%s'"),
        bIsDisplayClusterProcess ? TEXT("true") : TEXT("false"),
        *InstanceNodeSuffix,
        *InstanceId);
    ClusterId = FPlatformMisc::GetEnvironmentVariable(TEXT("RS_CLUSTER_ID"));
    ClusterId.TrimStartAndEndInline();
    if (ClusterId.IsEmpty())
    {
        UE_LOG(LogRshipExec, Log, TEXT("ClusterId unset (RS_CLUSTER_ID not provided)"));
    }
    else
    {
        UE_LOG(LogRshipExec, Log, TEXT("ClusterId resolved from RS_CLUSTER_ID='%s'"), *ClusterId);
    }

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
    WebSocket->OnBinaryMessage.BindUObject(this, &URshipSubsystem::OnWebSocketBinaryMessage);

    // Configure and connect
    FRshipWebSocketConfig Config;
    Config.bTcpNoDelay = Settings->bTcpNoDelay;
    Config.bDisableCompression = Settings->bDisableCompression;
    // Disable client-side heartbeat during topology replay and bulk sends.
    Config.PingIntervalSeconds = 0;
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
        UE_LOG(LogRshipExec, Log, TEXT("Preserving outbound queue while remote communication is disabled"));
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

void URshipSubsystem::RefreshTargetCache()
{
    UE_LOG(LogRshipExec, Log, TEXT("RefreshTargetCache: rebuilding tracked target/action/emitter state"));

    RefreshAllTargetComponents(TEXT("ManualRefresh"));
    SendInstanceInfo();
    StartTopologySync(TEXT("ManualRefresh"));
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
    LastWebSocketSendStatsLogTime = FPlatformTime::Seconds();
    WebSocketSendAttemptsSinceLastLog = 0;
    WebSocketSendSuccessSinceLastLog = 0;
    WebSocketSendFailuresSinceLastLog = 0;
    WebSocketSendBytesSinceLastLog = 0;

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

    // Send instance identity immediately, then query server state before replaying topology.
    SendInstanceInfo();
    StartTopologySync(TEXT("OnWebSocketConnected"));

    // Force immediate queue processing - the timer may not be running yet.
    UE_LOG(LogRshipExec, Log, TEXT("Forcing immediate queue processing after topology sync start"));
    ProcessMessageQueue();

    // Ensure queue processing ticker is running (may have failed during early init)
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (!QueueProcessTickerHandle.IsValid())
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
    TopologySyncState = FRshipTopologySyncState();

    // Clear connection timeout
    if (ConnectionTimeoutTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ConnectionTimeoutTickerHandle);
        ConnectionTimeoutTickerHandle.Reset();
    }

    // Schedule reconnection if enabled
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (Settings->bAutoReconnect && bRemoteCommunicationEnabled && !bIsManuallyReconnecting)
    {
        ScheduleReconnect();
    }
}

void URshipSubsystem::OnWebSocketClosed(int32 StatusCode, const FString &Reason, bool bWasClean)
{
    UE_LOG(LogRshipExec, Warning, TEXT("WebSocket closed: Code=%d, Reason=%s, Clean=%d"),
        StatusCode, *Reason, bWasClean);

    ConnectionState = ERshipConnectionState::Disconnected;
    TopologySyncState = FRshipTopologySyncState();

    // Schedule reconnection for any socket close while remote communication is enabled.
    // Skip only if we're in the middle of a manual reconnect (user called Reconnect()).
    const URshipSettings *Settings = GetDefault<URshipSettings>();
    if (Settings->bAutoReconnect && bRemoteCommunicationEnabled && !bIsManuallyReconnecting)
    {
        ScheduleReconnect();
    }
}

void URshipSubsystem::OnWebSocketMessage(const FString &Message)
{
    ProcessMessage(Message);
}

void URshipSubsystem::OnWebSocketBinaryMessage(const TArray<uint8>& Message)
{
    FString JsonMessage;
    if (!FRshipMykoTransport::DecodeMsgPackToJsonString(Message, JsonMessage))
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Failed to decode msgpack websocket message (%d bytes)"), Message.Num());
        return;
    }

    ProcessMessage(JsonMessage);
}

void URshipSubsystem::ScheduleReconnect()
{
    if (!bRemoteCommunicationEnabled)
    {
        ConnectionState = ERshipConnectionState::Disconnected;
        return;
    }
    const URshipSettings *Settings = GetDefault<URshipSettings>();

    // Calculate backoff delay
    constexpr float MaxReconnectBackoffSeconds = 5.0f;
    const float InitialBackoffSeconds = Settings ? FMath::Max(Settings->InitialBackoffSeconds, 0.1f) : 1.0f;
    const float BackoffMultiplier = Settings ? FMath::Max(Settings->BackoffMultiplier, 1.0f) : 2.0f;
    float BackoffDelay = InitialBackoffSeconds *
        FMath::Pow(BackoffMultiplier, static_cast<float>(ReconnectAttempts));
    BackoffDelay = FMath::Min(BackoffDelay, MaxReconnectBackoffSeconds);

    ReconnectAttempts++;
    ConnectionState = ERshipConnectionState::BackingOff;

    UE_LOG(LogRshipExec, Log, TEXT("Scheduling reconnect attempt %d in %.1f seconds (continuous retry, capped at %.1fs)"),
        ReconnectAttempts, BackoffDelay, MaxReconnectBackoffSeconds);

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
    if (Settings->bAutoReconnect && bRemoteCommunicationEnabled && !bIsManuallyReconnecting)
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
        bool bRemoveEntry = !IsValid(Component);
#if WITH_EDITOR
        if (!bRemoveEntry && GEditor && GEditor->PlayWorld == nullptr)
        {
            UWorld* ComponentWorld = Component->GetWorld();
            const EWorldType::Type WorldType = ComponentWorld ? static_cast<EWorldType::Type>(ComponentWorld->WorldType.GetValue()) : EWorldType::None;
            const bool bIsPlayWorld =
                WorldType == EWorldType::PIE ||
                WorldType == EWorldType::Game ||
                WorldType == EWorldType::GamePreview ||
                WorldType == EWorldType::GameRPC;

            if (!ComponentWorld || bIsPlayWorld)
            {
                bRemoveEntry = true;
            }
        }
#endif
        if (bRemoveEntry)
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

    const TWeakObjectPtr<URshipSubsystem> WeakThis(this);
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([WeakThis, bIsSimulating](float)
        {
            URshipSubsystem* StrongThis = WeakThis.Get();
            if (!IsValid(StrongThis))
            {
                return false;
            }

            StrongThis->RefreshAllTargetComponents(
                bIsSimulating ? TEXT("EndSimulateDeferred") : TEXT("EndPIEDeferred"));
            return false;
        }),
        0.0f);
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
    if (!IsConnected())
    {
        const int32 QueueSize = PendingOutboundMessages.Num();
        if (QueueSize > 0)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("ProcessMessageQueue: Not connected (State=%d), %d messages waiting"),
                (int32)ConnectionState, QueueSize);
        }
        return;
    }

    const int32 QueueSize = PendingOutboundMessages.Num();
    if (QueueSize > 0)
    {
        UE_LOG(LogRshipExec, VeryVerbose, TEXT("ProcessMessageQueue: Queue has %d messages, processing..."), QueueSize);
    }

    int32 Sent = 0;
    while (PendingOutboundMessages.Num() > 0)
    {
        FRshipQueuedMessage Message = PendingOutboundMessages[0];
        FString JsonString;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
        if (!Message.Payload.IsValid() || !FJsonSerializer::Serialize(Message.Payload.ToSharedRef(), JsonWriter))
        {
            PendingOutboundBytes = FMath::Max(0, PendingOutboundBytes - Message.EstimatedBytes);
            PendingOutboundMessages.RemoveAt(0);
            continue;
        }

        if (!SendJsonDirect(JsonString))
        {
            break;
        }

        PendingOutboundBytes = FMath::Max(0, PendingOutboundBytes - Message.EstimatedBytes);
        PendingOutboundMessages.RemoveAt(0);
        ++Sent;
    }

    if (Sent > 0 || QueueSize > 0)
    {
        UE_LOG(LogRshipExec, VeryVerbose, TEXT("ProcessMessageQueue: Sent %d messages, %d remaining"), Sent, PendingOutboundMessages.Num());
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

#if RSHIP_HAS_DISPLAY_CLUSTER
    UpdateRenderDomainMetadata();
#endif

    CheckTopologySyncTimeout();

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

void URshipSubsystem::EnqueueBatchTargetAction(const FString& TxId, TArray<FRshipPendingBatchActionItem>&& Actions, const FString& CommandId)
{
    if (TxId.IsEmpty())
    {
        UE_LOG(LogRshipExec, Error, TEXT("%s enqueue failed: missing tx id."), *CommandId);
        return;
    }

    if (Actions.Num() == 0)
    {
        UE_LOG(LogRshipExec, Error, TEXT("%s enqueue failed: no actions for tx '%s'."), *CommandId, *TxId);
        return;
    }

    FRshipPendingBatchTargetAction Pending;
    Pending.TxId = TxId;
    Pending.CommandId = CommandId;
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
            const FString EffectiveCommandId = PendingBatchCommand.CommandId.IsEmpty()
                ? TEXT("BatchTargetAction")
                : PendingBatchCommand.CommandId;
            QueueCommandResponse(PendingBatchCommand.TxId, false, EffectiveCommandId, FString::Printf(TEXT("%s has no actions"), *EffectiveCommandId));
            continue;
        }

        bool bAllSucceeded = true;
        int32 FailedIndex = INDEX_NONE;
        FString FailedReason;
        const FString EffectiveCommandId = PendingBatchCommand.CommandId.IsEmpty()
            ? TEXT("BatchTargetAction")
            : PendingBatchCommand.CommandId;

        for (int32 ActionIndex = 0; ActionIndex < PendingBatchCommand.Actions.Num(); ++ActionIndex)
        {
            FRshipPendingBatchActionItem& ActionItem = PendingBatchCommand.Actions[ActionIndex];
            if (!ActionItem.Data.IsValid())
            {
                bAllSucceeded = false;
                FailedIndex = ActionIndex;
                FailedReason = TEXT("Action payload missing");
                UE_LOG(LogRshipExec, Error, TEXT("%s tx=%s failed at index=%d: payload missing."),
                    *EffectiveCommandId, *PendingBatchCommand.TxId, ActionIndex);
                break;
            }

            const bool bResult = ExecuteTargetAction(ActionItem.TargetId, ActionItem.ActionId, ActionItem.Data.ToSharedRef());
            if (!bResult)
            {
                bAllSucceeded = false;
                FailedIndex = ActionIndex;
                FailedReason = FString::Printf(TEXT("Action was not handled by any target (index=%d target=%s action=%s)"),
                    ActionIndex, *ActionItem.TargetId, *ActionItem.ActionId);
                UE_LOG(LogRshipExec, Error, TEXT("%s tx=%s failed at index=%d target=%s action=%s."),
                    *EffectiveCommandId, *PendingBatchCommand.TxId, ActionIndex, *ActionItem.TargetId, *ActionItem.ActionId);
                break;
            }
        }

        if (bAllSucceeded)
        {
            QueueCommandResponse(PendingBatchCommand.TxId, true, EffectiveCommandId);
        }
        else
        {
            const FString ErrorMessage = FailedIndex == INDEX_NONE
                ? FString::Printf(TEXT("%s failed"), *EffectiveCommandId)
                : FString::Printf(TEXT("%s failed at index %d: %s"), *EffectiveCommandId, FailedIndex, *FailedReason);
            QueueCommandResponse(PendingBatchCommand.TxId, false, EffectiveCommandId, ErrorMessage);
        }
    }
}
void URshipSubsystem::QueueMessage(TSharedPtr<FJsonObject> Payload, ERshipMessagePriority Priority,
                                    ERshipMessageType Type, const FString& CoalesceKey)
{
    if (IsConnected())
    {
        FString JsonString;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
        if (FJsonSerializer::Serialize(Payload.ToSharedRef(), JsonWriter) && SendJsonDirect(JsonString))
        {
            return;
        }
    }

    FRshipQueuedMessage Message(Payload, Priority, Type, CoalesceKey);
    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    if (Payload.IsValid() && FJsonSerializer::Serialize(Payload.ToSharedRef(), JsonWriter))
    {
        Message.EstimatedBytes = JsonString.Len();
    }
    PendingOutboundBytes += Message.EstimatedBytes;
    PendingOutboundMessages.Add(MoveTemp(Message));
    UE_LOG(LogRshipExec, VeryVerbose, TEXT("Enqueued message (Key=%s, QueueLen=%d)"), *CoalesceKey, PendingOutboundMessages.Num());

    if (!QueueProcessTickerHandle.IsValid() && IsConnected())
    {
        ProcessMessageQueue();
    }
}

bool URshipSubsystem::SendJsonDirect(const FString& JsonString)
{
    if (!bRemoteCommunicationEnabled)
    {
        ++WebSocketSendFailuresSinceLastLog;
        MaybeLogWebSocketSendStats();
        return false;
    }
    bool bConnected = WebSocket.IsValid() && WebSocket->IsConnected();
    ++WebSocketSendAttemptsSinceLastLog;

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
        ++WebSocketSendFailuresSinceLastLog;
        MaybeLogWebSocketSendStats();
        return false;
    }

    UE_LOG(LogRshipExec, Verbose, TEXT("Sending json frame (%d chars)"), JsonString.Len());
    const bool bSent = WebSocket->Send(JsonString);

    if (bSent)
    {
        ++WebSocketSendSuccessSinceLastLog;
        WebSocketSendBytesSinceLastLog += JsonString.Len();
    }
    else
    {
        ++WebSocketSendFailuresSinceLastLog;
    }
    MaybeLogWebSocketSendStats();
    return bSent;
}

void URshipSubsystem::MaybeLogWebSocketSendStats()
{
    const double Now = FPlatformTime::Seconds();
    if (LastWebSocketSendStatsLogTime <= 0.0)
    {
        LastWebSocketSendStatsLogTime = Now;
        return;
    }

    const double Elapsed = Now - LastWebSocketSendStatsLogTime;
    if (Elapsed < 1.0)
    {
        return;
    }

    UE_LOG(
        LogRshipExec,
        Log,
        TEXT("WebSocket send last_%0.1fs attempts=%lld success=%lld failed=%lld bytes=%lld queue=%d pending_socket=%d state=%d"),
        Elapsed,
        WebSocketSendAttemptsSinceLastLog,
        WebSocketSendSuccessSinceLastLog,
        WebSocketSendFailuresSinceLastLog,
        WebSocketSendBytesSinceLastLog,
        PendingOutboundMessages.Num(),
        WebSocket.IsValid() ? WebSocket->GetPendingSendCount() : 0,
        static_cast<int32>(ConnectionState)
    );

    LastWebSocketSendStatsLogTime = Now;
    MessagesSentPerSecondSnapshot = static_cast<int32>(WebSocketSendSuccessSinceLastLog);
    BytesSentPerSecondSnapshot = static_cast<int32>(WebSocketSendBytesSinceLastLog);
    WebSocketSendAttemptsSinceLastLog = 0;
    WebSocketSendSuccessSinceLastLog = 0;
    WebSocketSendFailuresSinceLastLog = 0;
    WebSocketSendBytesSinceLastLog = 0;
}

void URshipSubsystem::StartTopologySync(const FString& Reason)
{
    if (!IsConnected())
    {
        UE_LOG(LogRshipExec, Warning, TEXT("Topology sync skipped: socket not connected (reason=%s); falling back to SendAll"), *Reason);
        SendAll();
        return;
    }

    if (TopologySyncState.bInFlight)
    {
        TopologySyncState.PendingReason = Reason;
        UE_LOG(LogRshipExec, Log, TEXT("Topology sync already in flight; queued follow-up reason=%s current=%s"),
            *Reason, *TopologySyncState.Reason);
        return;
    }

    TopologySyncState = FRshipTopologySyncState();
    TopologySyncState.bInFlight = true;
    TopologySyncState.Reason = Reason;
    TopologySyncState.StartedAtSeconds = FPlatformTime::Seconds();
    TopologySyncSnapshot = FRshipTopologySyncSnapshot();
    TopologySyncSnapshot.bInFlight = true;
    TopologySyncSnapshot.bLastSyncSucceeded = false;
    TopologySyncSnapshot.Reason = Reason;
    TopologySyncSnapshot.StartedAtSeconds = TopologySyncState.StartedAtSeconds;

    TSharedRef<FJsonObject> TargetsQuery = MakeShared<FJsonObject>();
    TargetsQuery->SetStringField(TEXT("serviceId"), ServiceId);

    TSharedRef<FJsonObject> ActionsQuery = MakeShared<FJsonObject>();
    ActionsQuery->SetStringField(TEXT("serviceId"), ServiceId);

    TSharedRef<FJsonObject> EmittersQuery = MakeShared<FJsonObject>();
    EmittersQuery->SetStringField(TEXT("serviceId"), ServiceId);

    TSharedRef<FJsonObject> StatusQuery = MakeShared<FJsonObject>();
    StatusQuery->SetStringField(TEXT("instanceId"), InstanceId);

    const bool bSentAll =
        SendQueryRequest(TEXT("GetTargetsByQuery"), TEXT("Target"), TargetsQuery, ERshipSyncQueryKind::Targets) &&
        SendQueryRequest(TEXT("GetActionsByQuery"), TEXT("Action"), ActionsQuery, ERshipSyncQueryKind::Actions) &&
        SendQueryRequest(TEXT("GetEmittersByQuery"), TEXT("Emitter"), EmittersQuery, ERshipSyncQueryKind::Emitters) &&
        SendQueryRequest(TEXT("GetTargetStatussByQuery"), TEXT("TargetStatus"), StatusQuery, ERshipSyncQueryKind::TargetStatuses);

    if (!bSentAll)
    {
        FailTopologySync(TEXT("Failed to send one or more topology queries"));
        return;
    }

    UE_LOG(LogRshipExec, Log, TEXT("Topology sync started: reason=%s queries=%d service=%s instance=%s"),
        *Reason, TopologySyncState.PendingQueries.Num(), *ServiceId, *InstanceId);
}

bool URshipSubsystem::SendQueryRequest(
    const FString& QueryId,
    const FString& QueryItemType,
    const TSharedRef<FJsonObject>& QueryPayload,
    ERshipSyncQueryKind Kind)
{
    const FString TxId = FRshipMykoTransport::GenerateTransactionId();
    QueryPayload->SetStringField(TEXT("tx"), TxId);
    QueryPayload->SetStringField(TEXT("createdAt"), FRshipMykoTransport::GetIso8601Timestamp());

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("queryId"), QueryId);
    Data->SetStringField(TEXT("queryItemType"), QueryItemType);
    Data->SetObjectField(TEXT("query"), QueryPayload);

    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("event"), RshipMykoEventNames::Query);
    Payload->SetObjectField(TEXT("data"), Data);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);

    FRshipPendingSyncQuery Pending;
    Pending.TxId = TxId;
    Pending.QueryId = QueryId;
    Pending.QueryItemType = QueryItemType;
    Pending.Kind = Kind;
    Pending.StartedAtSeconds = FPlatformTime::Seconds();
    TopologySyncState.PendingQueries.Add(TxId, Pending);

    const bool bSent = SendJsonDirect(JsonString);
    if (!bSent)
    {
        TopologySyncState.PendingQueries.Remove(TxId);
        UE_LOG(LogRshipExec, Warning, TEXT("Topology query send failed: queryId=%s tx=%s"), *QueryId, *TxId);
    }

    return bSent;
}

void URshipSubsystem::CancelQuerySubscription(const FString& TxId)
{
    if (TxId.IsEmpty())
    {
        return;
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("tx"), TxId);

    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("event"), RshipMykoEventNames::QueryCancel);
    Payload->SetObjectField(TEXT("data"), Data);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
    SendJsonDirect(JsonString);
}

void URshipSubsystem::HandleQueryResponse(const TSharedPtr<FJsonObject>& DataObj)
{
    if (!TopologySyncState.bInFlight || !DataObj.IsValid())
    {
        return;
    }

    FString TxId;
    if (!DataObj->TryGetStringField(TEXT("tx"), TxId) || TxId.IsEmpty())
    {
        return;
    }

    FRshipPendingSyncQuery* Pending = TopologySyncState.PendingQueries.Find(TxId);
    if (!Pending || Pending->bCompleted)
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Upserts = nullptr;
    if (DataObj->TryGetArrayField(TEXT("upserts"), Upserts) && Upserts)
    {
        switch (Pending->Kind)
        {
        case ERshipSyncQueryKind::Targets:
            AddRemoteItemsById(*Upserts, TopologySyncState.RemoteTargets);
            break;
        case ERshipSyncQueryKind::Actions:
            AddRemoteItemsById(*Upserts, TopologySyncState.RemoteActions);
            break;
        case ERshipSyncQueryKind::Emitters:
            AddRemoteItemsById(*Upserts, TopologySyncState.RemoteEmitters);
            break;
        case ERshipSyncQueryKind::TargetStatuses:
            AddRemoteItemsById(*Upserts, TopologySyncState.RemoteTargetStatuses);
            break;
        }
    }

    Pending->bCompleted = true;
    UE_LOG(LogRshipExec, Log, TEXT("Topology query response: reason=%s queryId=%s tx=%s upserts=%d"),
        *TopologySyncState.Reason, *Pending->QueryId, *TxId, Upserts ? Upserts->Num() : 0);
    CancelQuerySubscription(TxId);
    CompleteTopologySyncIfReady();
}

void URshipSubsystem::HandleQueryError(const TSharedPtr<FJsonObject>& DataObj)
{
    if (!TopologySyncState.bInFlight || !DataObj.IsValid())
    {
        return;
    }

    FString TxId;
    FString Message;
    DataObj->TryGetStringField(TEXT("tx"), TxId);
    DataObj->TryGetStringField(TEXT("message"), Message);

    FRshipPendingSyncQuery* Pending = TopologySyncState.PendingQueries.Find(TxId);
    if (!Pending)
    {
        return;
    }

    FailTopologySync(FString::Printf(TEXT("Query error for %s: %s"), *Pending->QueryId, *Message));
}

void URshipSubsystem::FailTopologySync(const FString& Reason)
{
    const FString CurrentReason = TopologySyncState.Reason;
    TArray<FString> PendingTxIds;
    TopologySyncState.PendingQueries.GetKeys(PendingTxIds);
    for (const FString& TxId : PendingTxIds)
    {
        CancelQuerySubscription(TxId);
    }

    const FString QueuedReason = TopologySyncState.PendingReason;
    TopologySyncSnapshot.bInFlight = false;
    TopologySyncSnapshot.bLastSyncSucceeded = false;
    TopologySyncSnapshot.Reason = CurrentReason;
    TopologySyncSnapshot.Detail = Reason;
    TopologySyncSnapshot.CompletedAtSeconds = FPlatformTime::Seconds();
    TopologySyncState = FRshipTopologySyncState();

    UE_LOG(LogRshipExec, Warning, TEXT("Topology sync failed: reason=%s detail=%s; falling back to SendAll"),
        *CurrentReason, *Reason);
    SendAll();

    if (!QueuedReason.IsEmpty())
    {
        StartTopologySync(QueuedReason);
    }
}

void URshipSubsystem::CompleteTopologySyncIfReady()
{
    if (!TopologySyncState.bInFlight)
    {
        return;
    }

    for (const TPair<FString, FRshipPendingSyncQuery>& Pair : TopologySyncState.PendingQueries)
    {
        if (!Pair.Value.bCompleted)
        {
            return;
        }
    }

    FlushTopologyDiff();

    const FString QueuedReason = TopologySyncState.PendingReason;
    TopologySyncState = FRshipTopologySyncState();
    if (!QueuedReason.IsEmpty())
    {
        StartTopologySync(QueuedReason);
    }
}

void URshipSubsystem::FlushTopologyDiff()
{
    TMap<FString, TSharedPtr<FJsonObject>> DesiredTargets;
    TMap<FString, TSharedPtr<FJsonObject>> DesiredActions;
    TMap<FString, TSharedPtr<FJsonObject>> DesiredEmitters;
    TMap<FString, TSharedPtr<FJsonObject>> DesiredStatuses;

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const FColor SRGBColor = Settings->ServiceColor.ToFColor(true);
    const FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);

    for (const TPair<Target*, FManagedTargetSnapshot>& Pair : ManagedTargetSnapshots)
    {
        Target* ManagedTarget = Pair.Key;
        if (!ManagedTarget)
        {
            continue;
        }

        TArray<FString> ActionIds;
        TArray<FString> EmitterIds;
        ActionIds.Reserve(ManagedTarget->GetActions().Num());
        EmitterIds.Reserve(ManagedTarget->GetEmitters().Num());

        for (const auto& ActionPair : ManagedTarget->GetActions())
        {
            FRshipActionRecord Record;
            Record.Id = ActionPair.Value.Id;
            Record.Name = ActionPair.Value.Name;
            Record.TargetId = ManagedTarget->GetId();
            Record.ServiceId = ServiceId;
            Record.Schema = ActionPair.Value.GetSchema();
            DesiredActions.Add(Record.Id, FRshipEntitySerializer::ToJson(Record));
            ActionIds.Add(Record.Id);
        }

        for (const auto& EmitterPair : ManagedTarget->GetEmitters())
        {
            FRshipEmitterRecord Record;
            Record.Id = EmitterPair.Value.Id;
            Record.Name = EmitterPair.Value.Name;
            Record.TargetId = ManagedTarget->GetId();
            Record.ServiceId = ServiceId;
            Record.Schema = EmitterPair.Value.GetSchema();
            DesiredEmitters.Add(Record.Id, FRshipEntitySerializer::ToJson(Record));
            EmitterIds.Add(Record.Id);
        }

        ActionIds.Sort();
        EmitterIds.Sort();

        FRshipTargetRecord TargetRecord;
        TargetRecord.Id = ManagedTarget->GetId();
        TargetRecord.Name = ManagedTarget->GetName();
        TargetRecord.ServiceId = ServiceId;
        TargetRecord.Category = TEXT("default");
        TargetRecord.ForegroundColor = ColorHex;
        TargetRecord.BackgroundColor = ColorHex;
        TargetRecord.ActionIds = ActionIds;
        TargetRecord.EmitterIds = EmitterIds;
        TargetRecord.ParentTargetIds = ManagedTarget->GetParentTargetIds();
        TargetRecord.bRootLevel = TargetRecord.ParentTargetIds.Num() == 0;

        if (URshipActorRegistrationComponent* TargetComp = ManagedTarget->GetBoundTargetComponent())
        {
            TargetRecord.Category = TargetComp->Category.IsEmpty() ? TEXT("default") : TargetComp->Category;
            TargetRecord.Tags = TargetComp->Tags;
            TargetRecord.GroupIds = TargetComp->GroupIds;
            TargetRecord.Tags.Sort();
            TargetRecord.GroupIds.Sort();
        }

        DesiredTargets.Add(TargetRecord.Id, FRshipEntitySerializer::ToJson(TargetRecord));

        FRshipTargetStatusRecord StatusRecord;
        StatusRecord.Id = ManagedTarget->GetId();
        StatusRecord.TargetId = ManagedTarget->GetId();
        StatusRecord.InstanceId = InstanceId;
        StatusRecord.Status = TEXT("online");
        DesiredStatuses.Add(StatusRecord.Id, FRshipEntitySerializer::ToJson(StatusRecord));
    }

    int32 TargetsSent = 0;
    int32 ActionsSent = 0;
    int32 EmittersSent = 0;
    int32 StatusesSent = 0;

    BeginRegistrationBatch();

    for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : DesiredTargets)
    {
        const TSharedPtr<FJsonObject>* Remote = TopologySyncState.RemoteTargets.Find(Pair.Key);
        if (!Remote || CanonicalizeJsonObject(*Remote) != CanonicalizeJsonObject(Pair.Value))
        {
            SetItem(TEXT("Target"), Pair.Value, ERshipMessagePriority::High, Pair.Key);
            ++TargetsSent;
        }
    }

    for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : DesiredActions)
    {
        const TSharedPtr<FJsonObject>* Remote = TopologySyncState.RemoteActions.Find(Pair.Key);
        if (!Remote || CanonicalizeJsonObject(*Remote) != CanonicalizeJsonObject(Pair.Value))
        {
            SetItem(TEXT("Action"), Pair.Value, ERshipMessagePriority::High, Pair.Key);
            ++ActionsSent;
        }
    }

    for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : DesiredEmitters)
    {
        const TSharedPtr<FJsonObject>* Remote = TopologySyncState.RemoteEmitters.Find(Pair.Key);
        if (!Remote || CanonicalizeJsonObject(*Remote) != CanonicalizeJsonObject(Pair.Value))
        {
            SetItem(TEXT("Emitter"), Pair.Value, ERshipMessagePriority::High, Pair.Key);
            ++EmittersSent;
        }
    }

    for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : DesiredStatuses)
    {
        const TSharedPtr<FJsonObject>* Remote = TopologySyncState.RemoteTargetStatuses.Find(Pair.Key);
        if (!Remote || CanonicalizeJsonObject(*Remote) != CanonicalizeJsonObject(Pair.Value))
        {
            SetItem(TEXT("TargetStatus"), Pair.Value, ERshipMessagePriority::High, Pair.Key + TEXT(":status"));
            ++StatusesSent;
        }
    }

    EndRegistrationBatch();
    ProcessMessageQueue();

    TopologySyncSnapshot.bInFlight = false;
    TopologySyncSnapshot.bLastSyncSucceeded = true;
    TopologySyncSnapshot.Reason = TopologySyncState.Reason;
    TopologySyncSnapshot.Detail = TEXT("Sync complete");
    TopologySyncSnapshot.CompletedAtSeconds = FPlatformTime::Seconds();
    TopologySyncSnapshot.LocalTargets = DesiredTargets.Num();
    TopologySyncSnapshot.RemoteTargets = TopologySyncState.RemoteTargets.Num();
    TopologySyncSnapshot.SentTargets = TargetsSent;
    TopologySyncSnapshot.LocalActions = DesiredActions.Num();
    TopologySyncSnapshot.RemoteActions = TopologySyncState.RemoteActions.Num();
    TopologySyncSnapshot.SentActions = ActionsSent;
    TopologySyncSnapshot.LocalEmitters = DesiredEmitters.Num();
    TopologySyncSnapshot.RemoteEmitters = TopologySyncState.RemoteEmitters.Num();
    TopologySyncSnapshot.SentEmitters = EmittersSent;
    TopologySyncSnapshot.LocalTargetStatuses = DesiredStatuses.Num();
    TopologySyncSnapshot.RemoteTargetStatuses = TopologySyncState.RemoteTargetStatuses.Num();
    TopologySyncSnapshot.SentTargetStatuses = StatusesSent;

    UE_LOG(LogRshipExec, Log, TEXT("Topology sync complete: reason=%s targets=%d/%d actions=%d/%d emitters=%d/%d statuses=%d/%d"),
        *TopologySyncState.Reason,
        TargetsSent, DesiredTargets.Num(),
        ActionsSent, DesiredActions.Num(),
        EmittersSent, DesiredEmitters.Num(),
        StatusesSent, DesiredStatuses.Num());
}

void URshipSubsystem::CheckTopologySyncTimeout()
{
    // Topology sync is query-driven and authoritative. Do not apply a local timeout:
    // wait for query completion or an actual socket/query error.
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
            UE_LOG(LogRshipExec, Warning, TEXT("Ignoring deprecated SetClientId command"));
            QueueCommandResponse(TxId, true, CommandId, TEXT("SetClientId ignored"));
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
        else if (CommandId == "CompactBatchTargetAction")
        {
            const TArray<TSharedPtr<FJsonValue>>* GroupsArray = nullptr;
            if (!CommandObj->TryGetArrayField(TEXT("groups"), GroupsArray) || GroupsArray == nullptr)
            {
                UE_LOG(LogRshipExec, Error, TEXT("CompactBatchTargetAction rejected: missing 'groups' array."));
                QueueCommandResponse(TxId, false, TEXT("CompactBatchTargetAction"), TEXT("Missing groups array"));
                return;
            }

            TArray<FRshipPendingBatchActionItem> ParsedActions;
            bool bParseFailed = false;
            FString ParseError = TEXT("Invalid groups payload");

            for (int32 GroupIndex = 0; GroupIndex < GroupsArray->Num() && !bParseFailed; ++GroupIndex)
            {
                const TSharedPtr<FJsonValue>& GroupValue = (*GroupsArray)[GroupIndex];
                if (!GroupValue.IsValid() || GroupValue->Type != EJson::Object)
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Group at index %d is not an object"), GroupIndex);
                    break;
                }

                TSharedPtr<FJsonObject> GroupObj = GroupValue->AsObject();
                if (!GroupObj.IsValid())
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Group at index %d is null"), GroupIndex);
                    break;
                }

                FString ActionId;
                if (!GroupObj->TryGetStringField(TEXT("actionId"), ActionId) || ActionId.IsEmpty())
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Group at index %d missing string field 'actionId'"), GroupIndex);
                    break;
                }

                const TArray<TSharedPtr<FJsonValue>>* PayloadsArray = nullptr;
                if (!GroupObj->TryGetArrayField(TEXT("payloads"), PayloadsArray) || PayloadsArray == nullptr)
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Group at index %d missing 'payloads' array"), GroupIndex);
                    break;
                }

                TArray<TSharedPtr<FJsonObject>> PayloadObjects;
                PayloadObjects.Reserve(PayloadsArray->Num());
                for (int32 PayloadIndex = 0; PayloadIndex < PayloadsArray->Num(); ++PayloadIndex)
                {
                    const TSharedPtr<FJsonValue>& PayloadValue = (*PayloadsArray)[PayloadIndex];
                    if (!PayloadValue.IsValid() || PayloadValue->Type != EJson::Object)
                    {
                        bParseFailed = true;
                        ParseError = FString::Printf(TEXT("Group %d payload %d is not an object"), GroupIndex, PayloadIndex);
                        break;
                    }

                    TSharedPtr<FJsonObject> PayloadObj = PayloadValue->AsObject();
                    if (!PayloadObj.IsValid())
                    {
                        bParseFailed = true;
                        ParseError = FString::Printf(TEXT("Group %d payload %d is null"), GroupIndex, PayloadIndex);
                        break;
                    }

                    PayloadObjects.Add(PayloadObj);
                }
                if (bParseFailed)
                {
                    break;
                }

                const TArray<TSharedPtr<FJsonValue>>* AssignmentsArray = nullptr;
                if (!GroupObj->TryGetArrayField(TEXT("assignments"), AssignmentsArray) || AssignmentsArray == nullptr)
                {
                    bParseFailed = true;
                    ParseError = FString::Printf(TEXT("Group at index %d missing 'assignments' array"), GroupIndex);
                    break;
                }

                for (int32 AssignmentIndex = 0; AssignmentIndex < AssignmentsArray->Num(); ++AssignmentIndex)
                {
                    const TSharedPtr<FJsonValue>& AssignmentValue = (*AssignmentsArray)[AssignmentIndex];
                    if (!AssignmentValue.IsValid() || AssignmentValue->Type != EJson::Object)
                    {
                        bParseFailed = true;
                        ParseError = FString::Printf(TEXT("Group %d assignment %d is not an object"), GroupIndex, AssignmentIndex);
                        break;
                    }

                    TSharedPtr<FJsonObject> AssignmentObj = AssignmentValue->AsObject();
                    if (!AssignmentObj.IsValid())
                    {
                        bParseFailed = true;
                        ParseError = FString::Printf(TEXT("Group %d assignment %d is null"), GroupIndex, AssignmentIndex);
                        break;
                    }

                    FString TargetId;
                    int32 PayloadIndex = INDEX_NONE;
                    if (!AssignmentObj->TryGetStringField(TEXT("targetId"), TargetId) || TargetId.IsEmpty())
                    {
                        bParseFailed = true;
                        ParseError = FString::Printf(TEXT("Group %d assignment %d missing string field 'targetId'"), GroupIndex, AssignmentIndex);
                        break;
                    }
                    if (!AssignmentObj->TryGetNumberField(TEXT("payloadIndex"), PayloadIndex) || PayloadIndex < 0 || PayloadIndex >= PayloadObjects.Num())
                    {
                        bParseFailed = true;
                        ParseError = FString::Printf(TEXT("Group %d assignment %d has invalid payloadIndex"), GroupIndex, AssignmentIndex);
                        break;
                    }

                    FRshipPendingBatchActionItem Parsed;
                    Parsed.TargetId = TargetId;
                    Parsed.ActionId = ActionId;
                    Parsed.Data = PayloadObjects[PayloadIndex];
                    ParsedActions.Add(MoveTemp(Parsed));
                }
            }

            if (bParseFailed)
            {
                UE_LOG(LogRshipExec, Error, TEXT("CompactBatchTargetAction rejected: %s (tx=%s)."), *ParseError, *TxId);
                QueueCommandResponse(TxId, false, TEXT("CompactBatchTargetAction"), ParseError);
                return;
            }

            if (ParsedActions.Num() == 0)
            {
                UE_LOG(LogRshipExec, Error, TEXT("CompactBatchTargetAction rejected: empty actions list (tx=%s)."), *TxId);
                QueueCommandResponse(TxId, false, TEXT("CompactBatchTargetAction"), TEXT("CompactBatchTargetAction has no actions"));
                return;
            }

            EnqueueBatchTargetAction(TxId, MoveTemp(ParsedActions), TEXT("CompactBatchTargetAction"));
        }
        else
        {
            UE_LOG(LogRshipExec, Error, TEXT("Unsupported commandId '%s' (tx=%s)."), *CommandId, *TxId);
            QueueCommandResponse(TxId, false, CommandId, FString::Printf(TEXT("Unsupported commandId '%s'"), *CommandId));
            return;
        }

        obj.Reset();
    }
    else if (type == RshipMykoEventNames::QueryResponse)
    {
        const TSharedPtr<FJsonObject>* DataPtr = nullptr;
        if (obj->TryGetObjectField(TEXT("data"), DataPtr) && DataPtr && DataPtr->IsValid())
        {
            HandleQueryResponse(*DataPtr);
        }
        return;
    }
    else if (type == RshipMykoEventNames::QueryError)
    {
        const TSharedPtr<FJsonObject>* DataPtr = nullptr;
        if (obj->TryGetObjectField(TEXT("data"), DataPtr) && DataPtr && DataPtr->IsValid())
        {
            HandleQueryError(*DataPtr);
        }
        return;
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
    TopologySyncState = FRshipTopologySyncState();

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
    PendingOutboundMessages.Reset();
    PendingOutboundBytes = 0;
    RateLimiter.Reset();

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
    TopologySyncState = FRshipTopologySyncState();

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
    TopologySyncState = FRshipTopologySyncState();

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

    RateLimiter.Reset();

    ConnectionState = ERshipConnectionState::Disconnected;

    UE_LOG(LogRshipExec, Log, TEXT("PrepareForHotReload complete - subsystem will reinitialize after module reload"));
}

void URshipSubsystem::ReinitializeAfterHotReload()
{
    UE_LOG(LogRshipExec, Log, TEXT("ReinitializeAfterHotReload - setting up tickers and reconnecting"));

    const URshipSettings* Settings = GetDefault<URshipSettings>();

    // Restart queue processing ticker
    if (!QueueProcessTickerHandle.IsValid())
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

    RateLimiter.Reset();

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

    ActionIds.Sort();
    EmitterIds.Sort();

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
        TargetRecord.Tags.Sort();
        TargetRecord.GroupIds.Sort();
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

    for (const TSharedPtr<FJsonObject>& BatchWrapper : BuildChunkedEventBatches(PayloadArray))
    {
        QueueMessage(BatchWrapper, Priority, Type, CoalesceKey);
    }
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

    for (const TSharedPtr<FJsonObject>& BatchWrapper : BuildChunkedEventBatches(PayloadArray))
    {
        QueueMessage(BatchWrapper, ERshipMessagePriority::High, ERshipMessageType::Registration, TEXT(""));
    }
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

#if RSHIP_HAS_DISPLAY_CLUSTER
void URshipSubsystem::RefreshRenderDomains()
{
    CachedRenderDomains.Reset();
    CachedDisplayClusterNodeId.Reset();
    CachedDisplayClusterRootActor.Reset();
    bHasLocalDisplayClusterNodeConfig = false;

    if (!IDisplayCluster::IsAvailable())
    {
        return;
    }

    IDisplayCluster& DisplayCluster = IDisplayCluster::Get();
    IDisplayClusterClusterManager* ClusterManager = DisplayCluster.GetClusterMgr();
    IDisplayClusterConfigManager* ConfigManager = DisplayCluster.GetConfigMgr();
    if (!ClusterManager || !ConfigManager)
    {
        return;
    }

    FString LocalNodeId = ClusterManager->GetNodeId();
    if (LocalNodeId.IsEmpty())
    {
        LocalNodeId = ConfigManager->GetLocalNodeId();
    }
#if RSHIP_HAS_DISPLAY_CLUSTER
    if (LocalNodeId.IsEmpty())
    {
        LocalNodeId = ResolveDisplayClusterNodeIdBestEffort();
    }
#endif
    if (LocalNodeId.IsEmpty())
    {
        return;
    }

    UDisplayClusterConfigurationData* ConfigData = ConfigManager->GetConfig();
    if (!ConfigData)
    {
        return;
    }

    ADisplayClusterRootActor* RootActor = nullptr;
    if (UWorld* RuntimeWorld = FindRuntimeWorld())
    {
        for (TActorIterator<ADisplayClusterRootActor> It(RuntimeWorld); It; ++It)
        {
            RootActor = *It;
            if (RootActor && RootActor->IsPrimaryRootActor())
            {
                break;
            }
        }
    }

    UDisplayClusterConfigurationClusterNode* ClusterNode = ConfigData->GetNode(LocalNodeId);
    if (!ClusterNode)
    {
        return;
    }
    bHasLocalDisplayClusterNodeConfig = true;

    USceneComponent* FallbackViewPoint = nullptr;
    if (RootActor)
    {
        FallbackViewPoint = RootActor->GetCommonViewPoint();
        if (!FallbackViewPoint)
        {
            FallbackViewPoint = RootActor->GetRootComponent();
        }
    }

    for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& Pair : ClusterNode->Viewports)
    {
        const FString& ViewportId = Pair.Key;
        const UDisplayClusterConfigurationViewport* Viewport = Pair.Value.Get();
        if (!Viewport || !Viewport->bAllowRendering)
        {
            continue;
        }

        const FString ProjectionType = Viewport->ProjectionPolicy.Type.ToLower();
        const TMap<FString, FString>& Params = Viewport->ProjectionPolicy.Parameters;

        FString ProjectionComponentName;
        if (ProjectionType == TEXT("mesh"))
        {
            ProjectionComponentName = Params.FindRef(TEXT("mesh_component"));
        }
        else if (ProjectionType == TEXT("simple"))
        {
            ProjectionComponentName = Params.FindRef(TEXT("screen"));
        }
        else if (ProjectionType == TEXT("mpcdi"))
        {
            ProjectionComponentName = Params.FindRef(TEXT("screen_component"));
        }

        UPrimitiveComponent* ProjectionComponent = nullptr;
        if (RootActor && !ProjectionComponentName.IsEmpty())
        {
            ProjectionComponent = RootActor->GetComponentByName<UPrimitiveComponent>(ProjectionComponentName);
        }

        USceneComponent* ViewPoint = FallbackViewPoint;
        if (RootActor && !Viewport->Camera.IsEmpty())
        {
            if (USceneComponent* CameraComponent = RootActor->GetComponentByName<USceneComponent>(Viewport->Camera))
            {
                ViewPoint = CameraComponent;
            }
        }

        FDisplayClusterRenderDomain Domain;
        Domain.ViewportId = ViewportId;
        Domain.ProjectionType = ProjectionType;
        Domain.ProjectionComponent = ProjectionComponent;
        Domain.ViewPointComponent = ViewPoint;
        CachedRenderDomains.Add(MoveTemp(Domain));
    }

    CachedDisplayClusterNodeId = LocalNodeId;
    CachedDisplayClusterRootActor = RootActor;
}

void URshipSubsystem::PublishRenderDomains()
{
    if (!bHasLocalDisplayClusterNodeConfig || CachedDisplayClusterNodeId.IsEmpty())
    {
        return;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    const double PublishIntervalSeconds = FMath::Max(0.05, static_cast<double>(CVarRshipNDisplayRenderDomainPublishIntervalSeconds.GetValueOnGameThread()));
    if ((NowSeconds - LastRenderDomainPublishTimeSeconds) < PublishIntervalSeconds)
    {
        return;
    }
    LastRenderDomainPublishTimeSeconds = NowSeconds;

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    FColor SRGBColor = Settings->ServiceColor.ToFColor(true);
    const FString ColorHex = FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);

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
    InstanceRecord.RenderDomain = BuildRenderDomainJson();
    InstanceRecord.CoordinateSpace = BuildCoordinateSpaceJson();
    InstanceRecord.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

    SetItem(TEXT("Instance"), FRshipEntitySerializer::ToJson(InstanceRecord), ERshipMessagePriority::High, TEXT("instance:") + InstanceId);
}

TSharedPtr<FJsonObject> URshipSubsystem::BuildRenderDomainJson() const
{
    if (!bHasLocalDisplayClusterNodeConfig)
    {
        return nullptr;
    }

    TArray<TSharedPtr<FJsonValue>> DomainsJson;
    DomainsJson.Reserve(CachedRenderDomains.Num());

    auto MakePoint = [](const FVector& V) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetNumberField(TEXT("x"), static_cast<double>(V.X));
        P->SetNumberField(TEXT("y"), static_cast<double>(V.Y));
        P->SetNumberField(TEXT("z"), static_cast<double>(V.Z));
        return P;
    };

    auto MakeInfiniteFrustumDomain = [&MakePoint](
        const FVector& Origin,
        const FVector& TopLeft,
        const FVector& TopRight,
        const FVector& BottomLeft,
        const FVector& BottomRight) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Frustum = MakeShared<FJsonObject>();
        Frustum->SetObjectField(TEXT("origin"), MakePoint(Origin));
        Frustum->SetObjectField(TEXT("topLeft"), MakePoint(TopLeft));
        Frustum->SetObjectField(TEXT("topRight"), MakePoint(TopRight));
        Frustum->SetObjectField(TEXT("bottomLeft"), MakePoint(BottomLeft));
        Frustum->SetObjectField(TEXT("bottomRight"), MakePoint(BottomRight));

        TSharedPtr<FJsonObject> DomainObject = MakeShared<FJsonObject>();
        DomainObject->SetStringField(TEXT("type"), TEXT("infiniteFrustum"));
        DomainObject->SetObjectField(TEXT("frustum"), Frustum);
        return DomainObject;
    };

    for (const FDisplayClusterRenderDomain& Domain : CachedRenderDomains)
    {
        if (const USceneComponent* ViewPoint = Domain.ViewPointComponent.Get())
        {
            const FVector Origin = ViewPoint->GetComponentLocation();
            const FVector Forward = ViewPoint->GetForwardVector().GetSafeNormal();
            const FVector Up = ViewPoint->GetUpVector().GetSafeNormal();
            const FVector Right = ViewPoint->GetRightVector().GetSafeNormal();

            float FarDistance = 50000.0f;
            FVector TopLeft = Origin + Forward * FarDistance + Up * 10000.0f - Right * 10000.0f;
            FVector TopRight = Origin + Forward * FarDistance + Up * 10000.0f + Right * 10000.0f;
            FVector BottomLeft = Origin + Forward * FarDistance - Up * 10000.0f - Right * 10000.0f;
            FVector BottomRight = Origin + Forward * FarDistance - Up * 10000.0f + Right * 10000.0f;

            if (const UPrimitiveComponent* ProjectionComponent = Domain.ProjectionComponent.Get())
            {
                const FBoxSphereBounds LocalBounds = ProjectionComponent->CalcBounds(FTransform::Identity);
                const FVector LocalExtent = LocalBounds.BoxExtent;
                const FVector LocalCenter = LocalBounds.Origin;
                const FTransform ComponentToWorld = ProjectionComponent->GetComponentTransform();
                const FVector ViewPointLocal = ComponentToWorld.InverseTransformPosition(Origin);

                int32 ThicknessAxis = 0;
                float ThicknessExtent = LocalExtent.X;
                if (LocalExtent.Y < ThicknessExtent)
                {
                    ThicknessAxis = 1;
                    ThicknessExtent = LocalExtent.Y;
                }
                if (LocalExtent.Z < ThicknessExtent)
                {
                    ThicknessAxis = 2;
                    ThicknessExtent = LocalExtent.Z;
                }

                const int32 WidthAxis = (ThicknessAxis + 1) % 3;
                const int32 HeightAxis = (ThicknessAxis + 2) % 3;

                auto GetAxisValue = [](const FVector& Vector, int32 Axis) -> float
                {
                    switch (Axis)
                    {
                    case 0:
                        return Vector.X;
                    case 1:
                        return Vector.Y;
                    default:
                        return Vector.Z;
                    }
                };

                auto SetAxisValue = [](FVector& Vector, int32 Axis, float Value)
                {
                    switch (Axis)
                    {
                    case 0:
                        Vector.X = Value;
                        break;
                    case 1:
                        Vector.Y = Value;
                        break;
                    default:
                        Vector.Z = Value;
                        break;
                    }
                };

                const float FaceSign = GetAxisValue(ViewPointLocal, ThicknessAxis) >= GetAxisValue(LocalCenter, ThicknessAxis) ? 1.0f : -1.0f;
                const float FaceCoord = GetAxisValue(LocalCenter, ThicknessAxis) + FaceSign * ThicknessExtent;
                const float WidthCenter = GetAxisValue(LocalCenter, WidthAxis);
                const float HeightCenter = GetAxisValue(LocalCenter, HeightAxis);
                const float WidthExtent = GetAxisValue(LocalExtent, WidthAxis);
                const float HeightExtent = GetAxisValue(LocalExtent, HeightAxis);

                auto MakeLocalFaceCorner = [&](float WidthSign, float HeightSign) -> FVector
                {
                    FVector LocalCorner = LocalCenter;
                    SetAxisValue(LocalCorner, ThicknessAxis, FaceCoord);
                    SetAxisValue(LocalCorner, WidthAxis, WidthCenter + WidthSign * WidthExtent);
                    SetAxisValue(LocalCorner, HeightAxis, HeightCenter + HeightSign * HeightExtent);
                    return ComponentToWorld.TransformPosition(LocalCorner);
                };

                const FVector FaceCorners[4] =
                {
                    MakeLocalFaceCorner(-1.0f, 1.0f),
                    MakeLocalFaceCorner(1.0f, 1.0f),
                    MakeLocalFaceCorner(-1.0f, -1.0f),
                    MakeLocalFaceCorner(1.0f, -1.0f),
                };

                struct FOrderedCorner
                {
                    FVector WorldPoint;
                    float RightDot = 0.0f;
                    float UpDot = 0.0f;
                };

                FVector ScreenCenter = FVector::ZeroVector;
                for (const FVector& Corner : FaceCorners)
                {
                    ScreenCenter += Corner;
                }
                ScreenCenter /= UE_ARRAY_COUNT(FaceCorners);

                TArray<FOrderedCorner> OrderedCorners;
                OrderedCorners.Reserve(UE_ARRAY_COUNT(FaceCorners));
                for (const FVector& Corner : FaceCorners)
                {
                    const FVector Relative = Corner - ScreenCenter;
                    FOrderedCorner OrderedCorner;
                    OrderedCorner.WorldPoint = Corner;
                    OrderedCorner.RightDot = FVector::DotProduct(Relative, Right);
                    OrderedCorner.UpDot = FVector::DotProduct(Relative, Up);
                    OrderedCorners.Add(OrderedCorner);
                }

                OrderedCorners.Sort([](const FOrderedCorner& A, const FOrderedCorner& B)
                {
                    if (!FMath::IsNearlyEqual(A.UpDot, B.UpDot))
                    {
                        return A.UpDot > B.UpDot;
                    }
                    return A.RightDot < B.RightDot;
                });

                if (OrderedCorners.Num() == 4)
                {
                    const FOrderedCorner& TopA = OrderedCorners[0];
                    const FOrderedCorner& TopB = OrderedCorners[1];
                    const FOrderedCorner& BottomA = OrderedCorners[2];
                    const FOrderedCorner& BottomB = OrderedCorners[3];

                    TopLeft = TopA.RightDot <= TopB.RightDot ? TopA.WorldPoint : TopB.WorldPoint;
                    TopRight = TopA.RightDot <= TopB.RightDot ? TopB.WorldPoint : TopA.WorldPoint;
                    BottomLeft = BottomA.RightDot <= BottomB.RightDot ? BottomA.WorldPoint : BottomB.WorldPoint;
                    BottomRight = BottomA.RightDot <= BottomB.RightDot ? BottomB.WorldPoint : BottomA.WorldPoint;
                }
            }

            DomainsJson.Add(MakeShared<FJsonValueObject>(
                MakeInfiniteFrustumDomain(Origin, TopLeft, TopRight, BottomLeft, BottomRight)));
        }
        else
        {
            // Config-only fallback when runtime viewpoint/component cannot be resolved.
            const FVector Origin = FVector::ZeroVector;
            const FVector Forward = FVector::ForwardVector;
            const FVector Up = FVector::UpVector;
            const FVector Right = FVector::RightVector;
            const float Distance = 50000.0f;
            const float HalfWidth = 10000.0f;
            const float HalfHeight = 10000.0f;
            const FVector Center = Origin + Forward * Distance;
            DomainsJson.Add(MakeShared<FJsonValueObject>(MakeInfiniteFrustumDomain(
                Origin,
                Center + Up * HalfHeight - Right * HalfWidth,
                Center + Up * HalfHeight + Right * HalfWidth,
                Center - Up * HalfHeight - Right * HalfWidth,
                Center - Up * HalfHeight + Right * HalfWidth)));
        }
    }

    if (DomainsJson.Num() == 0)
    {
        TSharedPtr<FJsonObject> Sphere = MakeShared<FJsonObject>();
        Sphere->SetObjectField(TEXT("center"), MakePoint(FVector::ZeroVector));
        Sphere->SetNumberField(TEXT("radius"), 1000000.0);

        TSharedPtr<FJsonObject> SphereDomain = MakeShared<FJsonObject>();
        SphereDomain->SetStringField(TEXT("type"), TEXT("sphere"));
        SphereDomain->SetObjectField(TEXT("sphere"), Sphere);
        return SphereDomain;
    }

    TSharedPtr<FJsonObject> AnyOf = MakeShared<FJsonObject>();
    AnyOf->SetStringField(TEXT("type"), TEXT("anyOf"));
    AnyOf->SetArrayField(TEXT("domains"), DomainsJson);
    return AnyOf;
}

void URshipSubsystem::UpdateRenderDomainMetadata()
{
    const double NowSeconds = FPlatformTime::Seconds();
    const double RefreshIntervalSeconds = FMath::Max(0.1, static_cast<double>(CVarRshipNDisplayRenderDomainRefreshIntervalSeconds.GetValueOnGameThread()));
    if ((NowSeconds - LastRenderDomainRefreshTimeSeconds) >= RefreshIntervalSeconds)
    {
        RefreshRenderDomains();
        LastRenderDomainRefreshTimeSeconds = NowSeconds;
    }

    PublishRenderDomains();
}
#endif

TSharedPtr<FJsonObject> URshipSubsystem::BuildCoordinateSpaceJson() const
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("right"), TEXT("PositiveY"));
    Json->SetStringField(TEXT("up"), TEXT("PositiveZ"));
    Json->SetStringField(TEXT("forward"), TEXT("PositiveX"));
    Json->SetNumberField(TEXT("unitsPerMeter"), 100.0);
    return Json;
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

    TArray<Target*> MatchingTargets;
    RegisteredTargetsById.MultiFind(FullTargetId, MatchingTargets);
    MatchingTargets.RemoveAll([](Target* Candidate) { return Candidate == nullptr; });
    if (MatchingTargets.Num() == 0)
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

    bool bRegisteredAny = false;
    bool bFoundDuplicateOnly = false;

    for (Target* TargetRef : MatchingTargets)
    {
        if (const FRshipActionProxy* ExistingAction = TargetRef->GetActions().Find(FullActionId))
        {
            UObject* ExistingOwner = ExistingAction->GetOwnerObject();
            const bool bExistingOwnerValid = IsValid(ExistingOwner);
            if (!bExistingOwnerValid || ExistingOwner != Owner)
            {
                if (!bExistingOwnerValid)
                {
                    UE_LOG(LogRshipExec, Verbose,
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
                bRegisteredAny = true;
            }
            else
            {
                bFoundDuplicateOnly = true;
            }
            continue;
        }

        TargetRef->AddAction(FRshipActionProxy::FromFunction(FullActionId, FinalName, Func, Owner));
        bRegisteredAny = true;
    }

    if (!bRegisteredAny && bFoundDuplicateOnly)
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("RegisterFunctionActionForTarget skipped duplicate action '%s'"), *FullActionId);
    }

    return bRegisteredAny;
}

bool URshipSubsystem::RegisterPropertyActionForTarget(const FString& FullTargetId, UObject* Owner, const FName& PropertyName, const FString& ExposedActionName)
{
    if (!Owner)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterPropertyActionForTarget failed: owner is null (target=%s, property=%s)"),
            *FullTargetId, *PropertyName.ToString());
        return false;
    }

    TArray<Target*> MatchingTargets;
    RegisteredTargetsById.MultiFind(FullTargetId, MatchingTargets);
    MatchingTargets.RemoveAll([](Target* Candidate) { return Candidate == nullptr; });
    if (MatchingTargets.Num() == 0)
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

    bool bRegisteredAny = false;
    bool bFoundDuplicateOnly = false;

    for (Target* TargetRef : MatchingTargets)
    {
        if (const FRshipActionProxy* ExistingAction = TargetRef->GetActions().Find(FullActionId))
        {
            UObject* ExistingOwner = ExistingAction->GetOwnerObject();
            const bool bExistingOwnerValid = IsValid(ExistingOwner);
            if (!bExistingOwnerValid || ExistingOwner != Owner)
            {
                if (!bExistingOwnerValid)
                {
                    UE_LOG(LogRshipExec, Verbose,
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
                bRegisteredAny = true;
            }
            else
            {
                bFoundDuplicateOnly = true;
            }
            continue;
        }

        TargetRef->AddAction(FRshipActionProxy::FromProperty(FullActionId, FinalName, Prop, Owner));
        bRegisteredAny = true;
    }

    if (!bRegisteredAny && bFoundDuplicateOnly)
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("RegisterPropertyActionForTarget skipped duplicate action '%s'"), *FullActionId);
    }

    return bRegisteredAny;
}

bool URshipSubsystem::RegisterEmitterForTarget(const FString& FullTargetId, UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName)
{
    if (!Owner)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RegisterEmitterForTarget failed: owner is null (target=%s, delegate=%s)"),
            *FullTargetId, *DelegateName.ToString());
        return false;
    }

    TArray<Target*> MatchingTargets;
    RegisteredTargetsById.MultiFind(FullTargetId, MatchingTargets);
    MatchingTargets.RemoveAll([](Target* Candidate) { return Candidate == nullptr; });
    if (MatchingTargets.Num() == 0)
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

    bool bRegisteredAny = false;
    bool bFoundDuplicateOnly = false;

    for (Target* TargetRef : MatchingTargets)
    {
        if (TargetRef->GetEmitters().Contains(FullEmitterId))
        {
            bFoundDuplicateOnly = true;
            continue;
        }

        TargetRef->AddEmitter(FRshipEmitterProxy::FromDelegateProperty(FullEmitterId, FinalName, EmitterProp));
        bRegisteredAny = true;
    }

    if (!bRegisteredAny && bFoundDuplicateOnly)
    {
        UE_LOG(LogRshipExec, Verbose, TEXT("RegisterEmitterForTarget skipped duplicate emitter '%s'"), *FullEmitterId);
    }

    return bRegisteredAny;
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

#if RSHIP_HAS_DISPLAY_CLUSTER
    if (CachedRenderDomains.Num() == 0)
    {
        RefreshRenderDomains();
    }
#endif

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
#if RSHIP_HAS_DISPLAY_CLUSTER
    InstanceRecord.RenderDomain = BuildRenderDomainJson();
#endif
    InstanceRecord.CoordinateSpace = BuildCoordinateSpaceJson();
    InstanceRecord.Hash = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

    SetItem("Instance", FRshipEntitySerializer::ToJson(InstanceRecord), ERshipMessagePriority::High, "instance:" + InstanceId);
}

void URshipSubsystem::SendAll()
{
    UE_LOG(LogRshipExec, Log, TEXT("SendAll: %d managed targets registered"), ManagedTargetSnapshots.Num());

    BeginRegistrationBatch();

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

    EndRegistrationBatch();

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
    TArray<Target*> MatchingTargets;
    RegisteredTargetsById.MultiFind(fullTargetId, MatchingTargets);

    const FString fullEmitterId = fullTargetId + ":" + emitterId;
    for (Target* FoundTarget : MatchingTargets)
    {
        if (!FoundTarget)
        {
            continue;
        }

        if (const FRshipEmitterProxy* FoundEmitter = FoundTarget->GetEmitters().Find(fullEmitterId))
        {
            return FoundEmitter;
        }
    }

    return nullptr;
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

uint8 URshipSubsystem::GetConnectionStateValue() const
{
    return static_cast<uint8>(ConnectionState);
}

bool URshipSubsystem::IsTopologySyncInFlight() const
{
    return TopologySyncState.bInFlight;
}

FString URshipSubsystem::GetTopologySyncReason() const
{
    return TopologySyncState.bInFlight ? TopologySyncState.Reason : TopologySyncSnapshot.Reason;
}

FString URshipSubsystem::GetTopologySyncDetail() const
{
    if (TopologySyncState.bInFlight)
    {
        return FString::Printf(TEXT("Waiting for %d query response(s)"), TopologySyncState.PendingQueries.Num());
    }
    return TopologySyncSnapshot.Detail;
}

float URshipSubsystem::GetTopologySyncAgeSeconds() const
{
    const double StartedAt = TopologySyncState.bInFlight ? TopologySyncState.StartedAtSeconds : TopologySyncSnapshot.StartedAtSeconds;
    if (StartedAt <= 0.0)
    {
        return 0.0f;
    }
    const double EndedAt = (!TopologySyncState.bInFlight && TopologySyncSnapshot.CompletedAtSeconds > StartedAt)
        ? TopologySyncSnapshot.CompletedAtSeconds
        : FPlatformTime::Seconds();
    return static_cast<float>(FMath::Max(0.0, EndedAt - StartedAt));
}

int32 URshipSubsystem::GetLocalTargetCount() const
{
    return TopologySyncState.bInFlight ? ManagedTargetSnapshots.Num() : TopologySyncSnapshot.LocalTargets;
}

int32 URshipSubsystem::GetRemoteTargetCount() const
{
    return TopologySyncState.bInFlight ? TopologySyncState.RemoteTargets.Num() : TopologySyncSnapshot.RemoteTargets;
}

int32 URshipSubsystem::GetLocalActionCount() const
{
    if (!TopologySyncState.bInFlight)
    {
        return TopologySyncSnapshot.LocalActions;
    }

    int32 Count = 0;
    for (const TPair<Target*, FManagedTargetSnapshot>& Pair : ManagedTargetSnapshots)
    {
        if (Pair.Key)
        {
            Count += Pair.Key->GetActions().Num();
        }
    }
    return Count;
}

int32 URshipSubsystem::GetRemoteActionCount() const
{
    return TopologySyncState.bInFlight ? TopologySyncState.RemoteActions.Num() : TopologySyncSnapshot.RemoteActions;
}

int32 URshipSubsystem::GetLocalEmitterCount() const
{
    if (!TopologySyncState.bInFlight)
    {
        return TopologySyncSnapshot.LocalEmitters;
    }

    int32 Count = 0;
    for (const TPair<Target*, FManagedTargetSnapshot>& Pair : ManagedTargetSnapshots)
    {
        if (Pair.Key)
        {
            Count += Pair.Key->GetEmitters().Num();
        }
    }
    return Count;
}

int32 URshipSubsystem::GetRemoteEmitterCount() const
{
    return TopologySyncState.bInFlight ? TopologySyncState.RemoteEmitters.Num() : TopologySyncSnapshot.RemoteEmitters;
}

int32 URshipSubsystem::GetLocalTargetStatusCount() const
{
    return TopologySyncState.bInFlight ? ManagedTargetSnapshots.Num() : TopologySyncSnapshot.LocalTargetStatuses;
}

int32 URshipSubsystem::GetRemoteTargetStatusCount() const
{
    return TopologySyncState.bInFlight ? TopologySyncState.RemoteTargetStatuses.Num() : TopologySyncSnapshot.RemoteTargetStatuses;
}

int32 URshipSubsystem::GetQueueLength() const
{
    return PendingOutboundMessages.Num();
}

int32 URshipSubsystem::GetQueueBytes() const
{
    return PendingOutboundBytes;
}

float URshipSubsystem::GetQueuePressure() const
{
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    const int32 MaxQueueLength = Settings ? FMath::Max(Settings->MaxQueueLength, 1) : 1;
    return static_cast<float>(PendingOutboundMessages.Num()) / static_cast<float>(MaxQueueLength);
}

int32 URshipSubsystem::GetMessagesSentPerSecond() const
{
    return MessagesSentPerSecondSnapshot;
}

int32 URshipSubsystem::GetBytesSentPerSecond() const
{
    return BytesSentPerSecondSnapshot;
}

int32 URshipSubsystem::GetMessagesDropped() const
{
    return 0;
}

bool URshipSubsystem::IsRateLimiterBackingOff() const
{
    return false;
}

float URshipSubsystem::GetBackoffRemaining() const
{
    return 0.0f;
}

float URshipSubsystem::GetCurrentRateLimit() const
{
    return 0.0f;
}

void URshipSubsystem::ResetRateLimiterStats()
{
    MessagesSentPerSecondSnapshot = 0;
    BytesSentPerSecondSnapshot = 0;
    UE_LOG(LogRshipExec, Log, TEXT("Outbound statistics reset"));
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
