// Copyright Rocketship. All Rights Reserved.

#include "IPMX/RshipIPMXService.h"
#include "Rivermax/Rship2110VideoSender.h"
#include "Rship2110Subsystem.h"
#include "Rship2110Settings.h"
#include "Rship2110.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Guid.h"

bool URshipIPMXService::Initialize(URship2110Subsystem* InSubsystem)
{
    if (!InSubsystem)
    {
        UE_LOG(LogRship2110, Error, TEXT("IPMXService: Invalid subsystem"));
        return false;
    }

    Subsystem = InSubsystem;

    // Initialize node configuration
    InitializeNodeConfig();
    InitializeDeviceConfig();

    // Load settings
    URship2110Settings* Settings = URship2110Settings::Get();
    if (Settings)
    {
        if (!Settings->IPMXNodeLabel.IsEmpty())
        {
            NodeConfig.Label = Settings->IPMXNodeLabel;
        }
        if (!Settings->IPMXNodeDescription.IsEmpty())
        {
            NodeConfig.Description = Settings->IPMXNodeDescription;
        }
        HeartbeatInterval = static_cast<double>(Settings->IPMXHeartbeatIntervalSeconds);
        LocalAPIPort = Settings->IPMXNodeAPIPort;
    }

    UE_LOG(LogRship2110, Log, TEXT("IPMXService: Initialized with node ID %s"), *NodeConfig.Id);

    return true;
}

void URshipIPMXService::Shutdown()
{
    // Disconnect from registry
    DisconnectFromRegistry();

    // Stop local API server
    StopLocalAPIServer();

    // Clear registered resources
    RegisteredSenders.Empty();
    SenderToVideoSenderId.Empty();

    Subsystem = nullptr;

    UE_LOG(LogRship2110, Log, TEXT("IPMXService: Shutdown complete"));
}

void URshipIPMXService::Tick(float DeltaTime)
{
    if (State != ERshipIPMXConnectionState::Registered &&
        State != ERshipIPMXConnectionState::Active)
    {
        return;
    }

    // Send heartbeat if needed
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastHeartbeatTime >= HeartbeatInterval)
    {
        SendHeartbeat();
        LastHeartbeatTime = CurrentTime;
    }
}

bool URshipIPMXService::ConnectToRegistry(const FString& InRegistryUrl)
{
    if (State == ERshipIPMXConnectionState::Registered ||
        State == ERshipIPMXConnectionState::Active)
    {
        UE_LOG(LogRship2110, Warning, TEXT("IPMXService: Already connected"));
        return true;
    }

    if (InRegistryUrl.IsEmpty())
    {
        // Try mDNS discovery
        if (!DiscoverRegistryViaMDNS())
        {
            UE_LOG(LogRship2110, Warning, TEXT("IPMXService: No registry URL and mDNS discovery failed"));
            // Continue anyway - can operate in peer-to-peer mode
        }
    }
    else
    {
        RegistryUrl = InRegistryUrl;
    }

    SetState(ERshipIPMXConnectionState::Connecting);

    // Register node with registry
    RegisterNode();

    return true;
}

void URshipIPMXService::DisconnectFromRegistry()
{
    if (State == ERshipIPMXConnectionState::Disconnected)
    {
        return;
    }

    // Unregister all senders
    TArray<FString> SenderIds;
    RegisteredSenders.GetKeys(SenderIds);
    for (const FString& SenderId : SenderIds)
    {
        UnregisterResource(TEXT("senders"), SenderId);
    }

    // Unregister device and node
    UnregisterResource(TEXT("devices"), DeviceId);
    UnregisterResource(TEXT("nodes"), NodeConfig.Id);

    SetState(ERshipIPMXConnectionState::Disconnected);
    RegistryUrl.Empty();

    UE_LOG(LogRship2110, Log, TEXT("IPMXService: Disconnected from registry"));
}

bool URshipIPMXService::IsConnected() const
{
    return State == ERshipIPMXConnectionState::Registered ||
           State == ERshipIPMXConnectionState::Active;
}

FRshipIPMXStatus URshipIPMXService::GetStatus() const
{
    FRshipIPMXStatus Status;
    Status.State = State;
    Status.RegistryUrl = RegistryUrl;
    Status.NodeId = NodeConfig.Id;
    Status.RegisteredSenders = RegisteredSenders.Num();
    Status.RegisteredReceivers = 0;  // Not implemented
    Status.LastHeartbeatTime = LastHeartbeatTime;
    Status.LastError = LastError;
    return Status;
}

void URshipIPMXService::SetNodeLabel(const FString& Label)
{
    NodeConfig.Label = Label;
}

void URshipIPMXService::SetNodeDescription(const FString& Description)
{
    NodeConfig.Description = Description;
}

void URshipIPMXService::AddNodeTag(const FString& Key, const FString& Value)
{
    NodeConfig.Tags.Add(Key, Value);
}

FString URshipIPMXService::RegisterSender(URship2110VideoSender* VideoSender)
{
    if (!VideoSender)
    {
        UE_LOG(LogRship2110, Error, TEXT("IPMXService: Invalid video sender"));
        return TEXT("");
    }

    // Generate sender ID
    FString SenderId = GenerateUUID();

    // Create sender resource
    FRshipNMOSSender Sender;
    Sender.Id = SenderId;
    Sender.Label = FString::Printf(TEXT("Sender %s"), *VideoSender->GetStreamId());
    Sender.Description = FString::Printf(TEXT("%dx%d @ %.2f fps"),
                                          VideoSender->GetVideoFormat().Width,
                                          VideoSender->GetVideoFormat().Height,
                                          VideoSender->GetVideoFormat().GetFrameRateDecimal());
    Sender.DeviceId = DeviceId;
    Sender.FlowId = GenerateUUID();  // Flow created alongside sender
    Sender.Transport = TEXT("urn:x-nmos:transport:rtp.mcast");

    // Store mapping
    RegisteredSenders.Add(SenderId, Sender);
    SenderToVideoSenderId.Add(SenderId, VideoSender->GetStreamId());

    // Register with registry if connected
    if (IsConnected() && !RegistryUrl.IsEmpty())
    {
        RegisterSourceAndFlow(SenderId, VideoSender);
        RegisterSenderResource(SenderId);
    }

    UE_LOG(LogRship2110, Log, TEXT("IPMXService: Registered sender %s for stream %s"),
           *SenderId, *VideoSender->GetStreamId());

    return SenderId;
}

bool URshipIPMXService::UnregisterSender(const FString& SenderId)
{
    if (!RegisteredSenders.Contains(SenderId))
    {
        UE_LOG(LogRship2110, Warning, TEXT("IPMXService: Sender %s not found"), *SenderId);
        return false;
    }

    // Unregister from registry
    if (IsConnected() && !RegistryUrl.IsEmpty())
    {
        UnregisterResource(TEXT("senders"), SenderId);
    }

    RegisteredSenders.Remove(SenderId);
    SenderToVideoSenderId.Remove(SenderId);

    UE_LOG(LogRship2110, Log, TEXT("IPMXService: Unregistered sender %s"), *SenderId);
    return true;
}

bool URshipIPMXService::GetSender(const FString& SenderId, FRshipNMOSSender& OutSender) const
{
    const FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (Sender)
    {
        OutSender = *Sender;
        return true;
    }
    return false;
}

TArray<FString> URshipIPMXService::GetRegisteredSenderIds() const
{
    TArray<FString> Ids;
    RegisteredSenders.GetKeys(Ids);
    return Ids;
}

bool URshipIPMXService::UpdateSenderTransport(const FString& SenderId, const FRship2110TransportParams& NewParams)
{
    FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (!Sender)
    {
        return false;
    }

    // Update the sender's transport info would go here
    // This typically involves updating the SDP/manifest

    return true;
}

bool URshipIPMXService::ActivateSender(const FString& SenderId)
{
    FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (!Sender)
    {
        return false;
    }

    Sender->bActive = true;

    // Start the associated video sender
    FString* StreamIdPtr = SenderToVideoSenderId.Find(SenderId);
    if (StreamIdPtr && Subsystem)
    {
        URship2110VideoSender* VideoSender = Subsystem->GetVideoSender(*StreamIdPtr);
        if (VideoSender)
        {
            VideoSender->StartStream();
        }
    }

    return true;
}

bool URshipIPMXService::DeactivateSender(const FString& SenderId)
{
    FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (!Sender)
    {
        return false;
    }

    Sender->bActive = false;

    // Stop the associated video sender
    FString* StreamIdPtr = SenderToVideoSenderId.Find(SenderId);
    if (StreamIdPtr && Subsystem)
    {
        URship2110VideoSender* VideoSender = Subsystem->GetVideoSender(*StreamIdPtr);
        if (VideoSender)
        {
            VideoSender->StopStream();
        }
    }

    return true;
}

FString URshipIPMXService::GetSenderSDP(const FString& SenderId) const
{
    const FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (!Sender)
    {
        return TEXT("");
    }

    const FString* StreamIdPtr = SenderToVideoSenderId.Find(SenderId);
    if (!StreamIdPtr || !Subsystem)
    {
        return TEXT("");
    }

    URship2110VideoSender* VideoSender = Subsystem->GetVideoSender(*StreamIdPtr);
    if (!VideoSender)
    {
        return TEXT("");
    }

    return VideoSender->GenerateSDP();
}

FString URshipIPMXService::GetSenderManifestUrl(const FString& SenderId) const
{
    const FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (!Sender)
    {
        return TEXT("");
    }

    if (!Sender->ManifestHref.IsEmpty())
    {
        return Sender->ManifestHref;
    }

    // Generate local manifest URL
    FString LocalIP = TEXT("127.0.0.1");
    // TODO: Get actual local IP
    return FString::Printf(TEXT("http://%s:%d/x-nmos/node/v1.3/senders/%s/sdp"),
                           *LocalIP, LocalAPIPort, *SenderId);
}

bool URshipIPMXService::StartLocalAPIServer(int32 Port)
{
    if (bLocalAPIRunning)
    {
        UE_LOG(LogRship2110, Warning, TEXT("IPMXService: Local API server already running"));
        return true;
    }

    LocalAPIPort = Port;

    // TODO: Implement HTTP server for IS-04/IS-05 APIs
    // This would typically use FHttpServerModule or a custom TCP server

    bLocalAPIRunning = true;
    UE_LOG(LogRship2110, Log, TEXT("IPMXService: Local API server started on port %d"), Port);
    return true;
}

void URshipIPMXService::StopLocalAPIServer()
{
    if (!bLocalAPIRunning)
    {
        return;
    }

    // TODO: Stop HTTP server

    bLocalAPIRunning = false;
    UE_LOG(LogRship2110, Log, TEXT("IPMXService: Local API server stopped"));
}

void URshipIPMXService::SetState(ERshipIPMXConnectionState NewState)
{
    if (State != NewState)
    {
        State = NewState;
        OnStateChanged.Broadcast(NewState);
    }
}

FString URshipIPMXService::GenerateUUID() const
{
    return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

void URshipIPMXService::InitializeNodeConfig()
{
    NodeConfig.Id = GenerateUUID();
    NodeConfig.Version = TEXT("v1.3");
    NodeConfig.Label = TEXT("Unreal Engine IPMX Node");
    NodeConfig.Description = TEXT("SMPTE 2110 streaming from Unreal Engine");

    // Get hostname
    NodeConfig.Hostname = FPlatformProcess::ComputerName();

    // Generate clock reference
    // In production, this would come from the PTP service
    NodeConfig.Clocks.Add(TEXT("clk0"));
}

void URshipIPMXService::InitializeDeviceConfig()
{
    DeviceId = GenerateUUID();
}

void URshipIPMXService::RegisterNode()
{
    if (RegistryUrl.IsEmpty())
    {
        SetState(ERshipIPMXConnectionState::Registered);
        return;
    }

    TSharedPtr<FJsonObject> NodeJson = BuildNodeJson();

    SendRegistryRequest(
        TEXT("POST"),
        TEXT("/x-nmos/registration/v1.3/resource"),
        NodeJson,
        [this](bool bSuccess, const FString& Response)
        {
            if (bSuccess)
            {
                UE_LOG(LogRship2110, Log, TEXT("IPMXService: Node registered"));
                RegisterDevice();
            }
            else
            {
                UE_LOG(LogRship2110, Error, TEXT("IPMXService: Node registration failed: %s"), *Response);
                LastError = Response;
                SetState(ERshipIPMXConnectionState::Error);
            }
        });
}

void URshipIPMXService::RegisterDevice()
{
    if (RegistryUrl.IsEmpty())
    {
        return;
    }

    TSharedPtr<FJsonObject> DeviceJson = BuildDeviceJson();

    SendRegistryRequest(
        TEXT("POST"),
        TEXT("/x-nmos/registration/v1.3/resource"),
        DeviceJson,
        [this](bool bSuccess, const FString& Response)
        {
            if (bSuccess)
            {
                UE_LOG(LogRship2110, Log, TEXT("IPMXService: Device registered"));
                SetState(ERshipIPMXConnectionState::Registered);
                LastHeartbeatTime = FPlatformTime::Seconds();
            }
            else
            {
                UE_LOG(LogRship2110, Error, TEXT("IPMXService: Device registration failed: %s"), *Response);
                LastError = Response;
                SetState(ERshipIPMXConnectionState::Error);
            }
        });
}

void URshipIPMXService::RegisterSourceAndFlow(const FString& SenderId, URship2110VideoSender* VideoSender)
{
    // Register source and flow for the sender
    // These are created alongside the sender in NMOS

    TSharedPtr<FJsonObject> SourceJson = BuildSourceJson(SenderId, VideoSender);
    TSharedPtr<FJsonObject> FlowJson = BuildFlowJson(SenderId, VideoSender);

    // Register source first, then flow
    SendRegistryRequest(
        TEXT("POST"),
        TEXT("/x-nmos/registration/v1.3/resource"),
        SourceJson,
        [this, SenderId, FlowJson](bool bSuccess, const FString& Response)
        {
            if (bSuccess)
            {
                // Now register flow
                SendRegistryRequest(
                    TEXT("POST"),
                    TEXT("/x-nmos/registration/v1.3/resource"),
                    FlowJson,
                    [](bool bSuccess2, const FString& Response2)
                    {
                        if (!bSuccess2)
                        {
                            UE_LOG(LogRship2110, Warning, TEXT("IPMXService: Flow registration failed"));
                        }
                    });
            }
            else
            {
                UE_LOG(LogRship2110, Warning, TEXT("IPMXService: Source registration failed"));
            }
        });
}

void URshipIPMXService::RegisterSenderResource(const FString& SenderId)
{
    TSharedPtr<FJsonObject> SenderJson = BuildSenderJson(SenderId);

    SendRegistryRequest(
        TEXT("POST"),
        TEXT("/x-nmos/registration/v1.3/resource"),
        SenderJson,
        [SenderId](bool bSuccess, const FString& Response)
        {
            if (bSuccess)
            {
                UE_LOG(LogRship2110, Log, TEXT("IPMXService: Sender %s registered with registry"), *SenderId);
            }
            else
            {
                UE_LOG(LogRship2110, Warning, TEXT("IPMXService: Sender registration failed: %s"), *Response);
            }
        });
}

void URshipIPMXService::UnregisterResource(const FString& ResourceType, const FString& ResourceId)
{
    if (RegistryUrl.IsEmpty())
    {
        return;
    }

    FString Endpoint = FString::Printf(TEXT("/x-nmos/registration/v1.3/resource/%s/%s"),
                                        *ResourceType, *ResourceId);

    SendRegistryRequest(
        TEXT("DELETE"),
        Endpoint,
        nullptr,
        [ResourceType, ResourceId](bool bSuccess, const FString& Response)
        {
            if (!bSuccess)
            {
                UE_LOG(LogRship2110, Warning, TEXT("IPMXService: Failed to unregister %s/%s"),
                       *ResourceType, *ResourceId);
            }
        });
}

void URshipIPMXService::SendHeartbeat()
{
    if (RegistryUrl.IsEmpty())
    {
        return;
    }

    FString Endpoint = FString::Printf(TEXT("/x-nmos/registration/v1.3/health/nodes/%s"),
                                        *NodeConfig.Id);

    SendRegistryRequest(
        TEXT("POST"),
        Endpoint,
        nullptr,
        [this](bool bSuccess, const FString& Response)
        {
            if (!bSuccess)
            {
                UE_LOG(LogRship2110, Warning, TEXT("IPMXService: Heartbeat failed"));
                // Could transition to error state after multiple failures
            }
        });
}

void URshipIPMXService::SendRegistryRequest(
    const FString& Method,
    const FString& Endpoint,
    const TSharedPtr<FJsonObject>& Body,
    TFunction<void(bool, const FString&)> Callback)
{
    FString FullUrl = RegistryUrl + Endpoint;

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(FullUrl);
    Request->SetVerb(Method);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    if (Body.IsValid())
    {
        FString BodyString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
        FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
        Request->SetContentAsString(BodyString);
    }

    Request->OnProcessRequestComplete().BindLambda(
        [Callback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
        {
            if (bSuccess && Response.IsValid())
            {
                int32 ResponseCode = Response->GetResponseCode();
                if (ResponseCode >= 200 && ResponseCode < 300)
                {
                    Callback(true, Response->GetContentAsString());
                }
                else
                {
                    Callback(false, FString::Printf(TEXT("HTTP %d: %s"),
                                                    ResponseCode, *Response->GetContentAsString()));
                }
            }
            else
            {
                Callback(false, TEXT("Request failed"));
            }
        });

    Request->ProcessRequest();
}

TSharedPtr<FJsonObject> URshipIPMXService::BuildNodeJson() const
{
    TSharedPtr<FJsonObject> Node = MakeShareable(new FJsonObject());
    Node->SetStringField(TEXT("type"), TEXT("node"));

    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
    Data->SetStringField(TEXT("id"), NodeConfig.Id);
    Data->SetStringField(TEXT("version"), FString::Printf(TEXT("%lld:0"), FDateTime::UtcNow().ToUnixTimestamp()));
    Data->SetStringField(TEXT("label"), NodeConfig.Label);
    Data->SetStringField(TEXT("description"), NodeConfig.Description);
    Data->SetStringField(TEXT("hostname"), NodeConfig.Hostname);

    // Tags
    TSharedPtr<FJsonObject> Tags = MakeShareable(new FJsonObject());
    for (const auto& Pair : NodeConfig.Tags)
    {
        TArray<TSharedPtr<FJsonValue>> TagValues;
        TagValues.Add(MakeShareable(new FJsonValueString(Pair.Value)));
        Tags->SetArrayField(Pair.Key, TagValues);
    }
    Data->SetObjectField(TEXT("tags"), Tags);

    // Clocks
    TArray<TSharedPtr<FJsonValue>> Clocks;
    for (const FString& Clock : NodeConfig.Clocks)
    {
        TSharedPtr<FJsonObject> ClockObj = MakeShareable(new FJsonObject());
        ClockObj->SetStringField(TEXT("name"), Clock);
        ClockObj->SetStringField(TEXT("ref_type"), TEXT("ptp"));
        Clocks.Add(MakeShareable(new FJsonValueObject(ClockObj)));
    }
    Data->SetArrayField(TEXT("clocks"), Clocks);

    // API endpoints (empty array for now)
    Data->SetArrayField(TEXT("api"), TArray<TSharedPtr<FJsonValue>>());

    // Services (empty array)
    Data->SetArrayField(TEXT("services"), TArray<TSharedPtr<FJsonValue>>());

    // Interfaces (empty array)
    Data->SetArrayField(TEXT("interfaces"), TArray<TSharedPtr<FJsonValue>>());

    Node->SetObjectField(TEXT("data"), Data);
    return Node;
}

TSharedPtr<FJsonObject> URshipIPMXService::BuildDeviceJson() const
{
    TSharedPtr<FJsonObject> Device = MakeShareable(new FJsonObject());
    Device->SetStringField(TEXT("type"), TEXT("device"));

    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
    Data->SetStringField(TEXT("id"), DeviceId);
    Data->SetStringField(TEXT("version"), FString::Printf(TEXT("%lld:0"), FDateTime::UtcNow().ToUnixTimestamp()));
    Data->SetStringField(TEXT("label"), TEXT("Unreal Engine Video Device"));
    Data->SetStringField(TEXT("description"), TEXT("Video output device"));
    Data->SetStringField(TEXT("node_id"), NodeConfig.Id);
    Data->SetStringField(TEXT("type"), TEXT("urn:x-nmos:device:generic"));

    // Empty arrays
    Data->SetArrayField(TEXT("tags"), TArray<TSharedPtr<FJsonValue>>());
    Data->SetArrayField(TEXT("senders"), TArray<TSharedPtr<FJsonValue>>());
    Data->SetArrayField(TEXT("receivers"), TArray<TSharedPtr<FJsonValue>>());
    Data->SetArrayField(TEXT("controls"), TArray<TSharedPtr<FJsonValue>>());

    Device->SetObjectField(TEXT("data"), Data);
    return Device;
}

TSharedPtr<FJsonObject> URshipIPMXService::BuildSourceJson(const FString& SenderId, URship2110VideoSender* VideoSender) const
{
    const FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (!Sender)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Source = MakeShareable(new FJsonObject());
    Source->SetStringField(TEXT("type"), TEXT("source"));

    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
    FString SourceId = GenerateUUID();
    Data->SetStringField(TEXT("id"), SourceId);
    Data->SetStringField(TEXT("version"), FString::Printf(TEXT("%lld:0"), FDateTime::UtcNow().ToUnixTimestamp()));
    Data->SetStringField(TEXT("label"), FString::Printf(TEXT("Source for %s"), *Sender->Label));
    Data->SetStringField(TEXT("description"), TEXT("Video source"));
    Data->SetStringField(TEXT("device_id"), DeviceId);
    Data->SetStringField(TEXT("format"), TEXT("urn:x-nmos:format:video"));

    // Clock reference
    TSharedPtr<FJsonObject> Clock = MakeShareable(new FJsonObject());
    Clock->SetStringField(TEXT("name"), TEXT("clk0"));
    Data->SetObjectField(TEXT("clock_name"), Clock);

    Data->SetArrayField(TEXT("tags"), TArray<TSharedPtr<FJsonValue>>());
    Data->SetArrayField(TEXT("parents"), TArray<TSharedPtr<FJsonValue>>());

    Source->SetObjectField(TEXT("data"), Data);
    return Source;
}

TSharedPtr<FJsonObject> URshipIPMXService::BuildFlowJson(const FString& SenderId, URship2110VideoSender* VideoSender) const
{
    const FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (!Sender || !VideoSender)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Flow = MakeShareable(new FJsonObject());
    Flow->SetStringField(TEXT("type"), TEXT("flow"));

    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
    Data->SetStringField(TEXT("id"), Sender->FlowId);
    Data->SetStringField(TEXT("version"), FString::Printf(TEXT("%lld:0"), FDateTime::UtcNow().ToUnixTimestamp()));
    Data->SetStringField(TEXT("label"), FString::Printf(TEXT("Flow for %s"), *Sender->Label));
    Data->SetStringField(TEXT("description"), TEXT("Video flow"));
    Data->SetStringField(TEXT("format"), TEXT("urn:x-nmos:format:video"));
    Data->SetStringField(TEXT("device_id"), DeviceId);

    // Video format info
    const FRship2110VideoFormat& Format = VideoSender->GetVideoFormat();
    Data->SetNumberField(TEXT("frame_width"), Format.Width);
    Data->SetNumberField(TEXT("frame_height"), Format.Height);
    Data->SetStringField(TEXT("colorspace"), Format.GetColorimetryString());

    // Frame rate
    TSharedPtr<FJsonObject> FrameRate = MakeShareable(new FJsonObject());
    FrameRate->SetNumberField(TEXT("numerator"), Format.FrameRateNumerator);
    FrameRate->SetNumberField(TEXT("denominator"), Format.FrameRateDenominator);
    Data->SetObjectField(TEXT("grain_rate"), FrameRate);

    Data->SetStringField(TEXT("media_type"), TEXT("video/raw"));

    Data->SetArrayField(TEXT("tags"), TArray<TSharedPtr<FJsonValue>>());
    Data->SetArrayField(TEXT("parents"), TArray<TSharedPtr<FJsonValue>>());

    Flow->SetObjectField(TEXT("data"), Data);
    return Flow;
}

TSharedPtr<FJsonObject> URshipIPMXService::BuildSenderJson(const FString& SenderId) const
{
    const FRshipNMOSSender* Sender = RegisteredSenders.Find(SenderId);
    if (!Sender)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> SenderObj = MakeShareable(new FJsonObject());
    SenderObj->SetStringField(TEXT("type"), TEXT("sender"));

    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
    Data->SetStringField(TEXT("id"), Sender->Id);
    Data->SetStringField(TEXT("version"), FString::Printf(TEXT("%lld:0"), FDateTime::UtcNow().ToUnixTimestamp()));
    Data->SetStringField(TEXT("label"), Sender->Label);
    Data->SetStringField(TEXT("description"), Sender->Description);
    Data->SetStringField(TEXT("flow_id"), Sender->FlowId);
    Data->SetStringField(TEXT("transport"), Sender->Transport);
    Data->SetStringField(TEXT("device_id"), DeviceId);
    Data->SetStringField(TEXT("manifest_href"), GetSenderManifestUrl(SenderId));

    // Interface bindings
    Data->SetArrayField(TEXT("interface_bindings"), TArray<TSharedPtr<FJsonValue>>());
    Data->SetArrayField(TEXT("tags"), TArray<TSharedPtr<FJsonValue>>());

    // Subscription (null when not subscribed)
    Data->SetObjectField(TEXT("subscription"), MakeShareable(new FJsonObject()));

    SenderObj->SetObjectField(TEXT("data"), Data);
    return SenderObj;
}

bool URshipIPMXService::DiscoverRegistryViaMDNS()
{
    // mDNS discovery for NMOS registry
    // Service type: _nmos-register._tcp

    // TODO: Implement mDNS discovery
    // This would use platform-specific mDNS APIs or a library like dns-sd

    UE_LOG(LogRship2110, Log, TEXT("IPMXService: mDNS discovery not yet implemented"));
    return false;
}

void URshipIPMXService::HandleAPIRequest(const FString& Path, const FString& Method, const FString& Body, FString& OutResponse)
{
    // Handle local IS-04 Node API requests
    if (Path.StartsWith(TEXT("/x-nmos/node/v1.3")))
    {
        OutResponse = HandleNodeAPI(Path);
    }
    else
    {
        OutResponse = TEXT("{\"error\": \"Not Found\"}");
    }
}

FString URshipIPMXService::HandleNodeAPI(const FString& Path) const
{
    // IS-04 Node API endpoints
    if (Path.EndsWith(TEXT("/self")))
    {
        // Return node info
        TSharedPtr<FJsonObject> NodeJson = BuildNodeJson();
        if (NodeJson.IsValid())
        {
            FString Result;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
            FJsonSerializer::Serialize(NodeJson->GetObjectField(TEXT("data")).ToSharedRef(), Writer);
            return Result;
        }
    }
    else if (Path.Contains(TEXT("/senders")))
    {
        return HandleSendersAPI(Path);
    }

    return TEXT("{}");
}

FString URshipIPMXService::HandleSendersAPI(const FString& Path) const
{
    // Check if requesting specific sender
    int32 SendersIndex = Path.Find(TEXT("/senders/"));
    if (SendersIndex != INDEX_NONE)
    {
        FString Remainder = Path.Mid(SendersIndex + 9);
        int32 SlashIndex;
        if (Remainder.FindChar('/', SlashIndex))
        {
            FString SenderId = Remainder.Left(SlashIndex);
            if (Remainder.EndsWith(TEXT("/sdp")))
            {
                // Return SDP
                return GetSenderSDP(SenderId);
            }
            return HandleSingleSenderAPI(SenderId);
        }
        else
        {
            return HandleSingleSenderAPI(Remainder);
        }
    }

    // Return list of all senders
    TArray<TSharedPtr<FJsonValue>> SenderList;
    for (const auto& Pair : RegisteredSenders)
    {
        TSharedPtr<FJsonObject> SenderJson = BuildSenderJson(Pair.Key);
        if (SenderJson.IsValid())
        {
            SenderList.Add(MakeShareable(new FJsonValueObject(SenderJson->GetObjectField(TEXT("data")))));
        }
    }

    FString Result;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(SenderList, Writer);
    return Result;
}

FString URshipIPMXService::HandleSingleSenderAPI(const FString& SenderId) const
{
    TSharedPtr<FJsonObject> SenderJson = BuildSenderJson(SenderId);
    if (SenderJson.IsValid())
    {
        FString Result;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
        FJsonSerializer::Serialize(SenderJson->GetObjectField(TEXT("data")).ToSharedRef(), Writer);
        return Result;
    }
    return TEXT("{\"error\": \"Sender not found\"}");
}
