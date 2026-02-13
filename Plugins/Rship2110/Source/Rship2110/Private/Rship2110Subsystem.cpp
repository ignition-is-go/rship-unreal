// Copyright Rocketship. All Rights Reserved.

#include "Rship2110Subsystem.h"
#include "Rship2110Settings.h"
#include "Rship2110.h"
#include "PTP/RshipPTPService.h"
#include "Rivermax/RivermaxManager.h"
#include "Rivermax/Rship2110VideoSender.h"
#include "IPMX/RshipIPMXService.h"
#include "Capture/Rship2110VideoCapture.h"

// Include RshipExec for integration
#include "RshipSubsystem.h"
#include "RshipContentMappingManager.h"

DECLARE_STATS_GROUP(TEXT("Rship2110"), STATGROUP_Rship2110, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Rship2110 Tick"), STAT_Rship2110Tick, STATGROUP_Rship2110);

void URship2110Subsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: Initializing..."));

    // Load settings
    URship2110Settings* Settings = URship2110Settings::Get();

    // Initialize services based on settings
    if (Settings && Settings->bEnablePTP)
    {
        InitializePTPService();
    }

    if (Settings && Settings->bEnableRivermax)
    {
        InitializeRivermaxManager();
    }

    if (Settings && Settings->bEnableIPMX)
    {
        InitializeIPMXService();
    }

    // Initialize video capture
    InitializeVideoCapture();

    // Auto-connect IPMX if configured
    if (Settings && Settings->bIPMXAutoRegister && IPMXService)
    {
        IPMXService->ConnectToRegistry(Settings->IPMXRegistryUrl);
    }

    StreamToContextBinding.Empty();

    bIsInitialized = true;

    UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: Initialized"));
}

void URship2110Subsystem::Deinitialize()
{
    UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: Deinitializing..."));

    bIsInitialized = false;

    // Shutdown in reverse order
    if (VideoCapture)
    {
        VideoCapture->Shutdown();
        VideoCapture = nullptr;
    }

    if (IPMXService)
    {
        IPMXService->Shutdown();
        IPMXService = nullptr;
    }

    if (RivermaxManager)
    {
        RivermaxManager->Shutdown();
        RivermaxManager = nullptr;
    }

    if (PTPService)
    {
        PTPService->Shutdown();
        PTPService = nullptr;
    }

    StreamToIPMXSender.Empty();
    StreamToContextBinding.Empty();

    UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: Deinitialized"));

    Super::Deinitialize();
}

bool URship2110Subsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Always create - let settings determine what's enabled
    return true;
}

void URship2110Subsystem::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_Rship2110Tick);

    if (PTPService)
    {
        PTPService->Tick(DeltaTime);
    }

    if (RivermaxManager)
    {
        RivermaxManager->Tick(DeltaTime);
    }

    if (IPMXService)
    {
        IPMXService->Tick(DeltaTime);
    }

    if (VideoCapture)
    {
        VideoCapture->ProcessPendingCaptures();
    }

    if (bIsInitialized)
    {
        RefreshStreamRenderContextBindings();
    }
}

TStatId URship2110Subsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URship2110Subsystem, STATGROUP_Rship2110);
}

URshipSubsystem* URship2110Subsystem::GetRshipSubsystem() const
{
    return GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
}

FRshipPTPTimestamp URship2110Subsystem::GetPTPTime() const
{
    return PTPService ? PTPService->GetPTPTime() : FRshipPTPTimestamp();
}

bool URship2110Subsystem::IsPTPLocked() const
{
    return PTPService && PTPService->IsLocked();
}

FRshipPTPStatus URship2110Subsystem::GetPTPStatus() const
{
    return PTPService ? PTPService->GetStatus() : FRshipPTPStatus();
}

FString URship2110Subsystem::CreateVideoStream(
    const FRship2110VideoFormat& VideoFormat,
    const FRship2110TransportParams& TransportParams,
    bool bAutoRegisterIPMX)
{
    if (!RivermaxManager)
    {
        UE_LOG(LogRship2110, Error, TEXT("Rship2110Subsystem: Rivermax manager not available"));
        return TEXT("");
    }

    FString StreamId;
    URship2110VideoSender* Sender = RivermaxManager->CreateVideoSender(VideoFormat, TransportParams, StreamId);

    if (!Sender)
    {
        return TEXT("");
    }

    // Register with IPMX if requested
    if (bAutoRegisterIPMX && IPMXService)
    {
        FString IPMXSenderId = IPMXService->RegisterSender(Sender);
        if (!IPMXSenderId.IsEmpty())
        {
            StreamToIPMXSender.Add(StreamId, IPMXSenderId);
        }
    }

    // Bind event handler
    Sender->OnStateChanged.AddDynamic(this, &URship2110Subsystem::OnStreamStateChangedInternal);

    return StreamId;
}

bool URship2110Subsystem::DestroyVideoStream(const FString& StreamId)
{
    // Unregister from IPMX
    FString* IPMXSenderId = StreamToIPMXSender.Find(StreamId);
    if (IPMXSenderId && IPMXService)
    {
        IPMXService->UnregisterSender(*IPMXSenderId);
        StreamToIPMXSender.Remove(StreamId);
    }

    // Destroy stream
    if (RivermaxManager)
    {
        const bool bDestroyed = RivermaxManager->DestroyStream(StreamId);
        StreamToContextBinding.Remove(StreamId);
        return bDestroyed;
    }

    StreamToContextBinding.Remove(StreamId);
    return false;
}

bool URship2110Subsystem::BindVideoStreamToRenderContext(const FString& StreamId, const FString& RenderContextId)
{
    return BindVideoStreamToRenderContextWithRect(StreamId, RenderContextId, FIntRect());
}

bool URship2110Subsystem::BindVideoStreamToRenderContextWithRect(const FString& StreamId, const FString& RenderContextId, const FIntRect& CaptureRect)
{
    URship2110VideoSender* Sender = GetVideoSender(StreamId);
    if (!Sender)
    {
        UE_LOG(LogRship2110, Warning, TEXT("BindVideoStreamToRenderContextWithRect: Stream %s not found"), *StreamId);
        return false;
    }

    UTextureRenderTarget2D* RenderTarget = nullptr;
    if (!ResolveRenderContextRenderTarget(RenderContextId, RenderTarget))
    {
        UE_LOG(LogRship2110, Warning, TEXT("BindVideoStreamToRenderContextWithRect: Context %s not found or has no render target"), *RenderContextId);
        return false;
    }

    Sender->SetRenderTarget(RenderTarget);
    FRship2110RenderContextBinding Binding;
    Binding.RenderContextId = RenderContextId;

    if (CaptureRect.Area() > 0)
    {
        Binding.bUseCaptureRect = true;
        Binding.CaptureRect = CaptureRect;
        Sender->SetCaptureRect(CaptureRect);
    }
    else
    {
        Binding.bUseCaptureRect = false;
        Binding.CaptureRect = FIntRect();
        Sender->ClearCaptureRect();
    }

    StreamToContextBinding.Add(StreamId, Binding);
    return true;
}

bool URship2110Subsystem::UnbindVideoStreamFromRenderContext(const FString& StreamId)
{
    return StreamToContextBinding.Remove(StreamId) > 0;
}

FString URship2110Subsystem::GetBoundRenderContextForStream(const FString& StreamId) const
{
    const FRship2110RenderContextBinding* Binding = StreamToContextBinding.Find(StreamId);
    return Binding ? Binding->RenderContextId : FString();
}

bool URship2110Subsystem::GetBoundRenderContextBinding(const FString& StreamId, FString& OutRenderContextId, FIntRect& OutCaptureRect, bool& bOutUseCaptureRect) const
{
    const FRship2110RenderContextBinding* Binding = StreamToContextBinding.Find(StreamId);
    if (!Binding)
    {
        return false;
    }

    OutRenderContextId = Binding->RenderContextId;
    OutCaptureRect = Binding->CaptureRect;
    bOutUseCaptureRect = Binding->bUseCaptureRect;
    return true;
}

URship2110VideoSender* URship2110Subsystem::GetVideoSender(const FString& StreamId) const
{
    return RivermaxManager ? RivermaxManager->GetVideoSender(StreamId) : nullptr;
}

TArray<FString> URship2110Subsystem::GetActiveStreamIds() const
{
    return RivermaxManager ? RivermaxManager->GetActiveStreamIds() : TArray<FString>();
}

bool URship2110Subsystem::StartStream(const FString& StreamId)
{
    URship2110VideoSender* Sender = GetVideoSender(StreamId);
    if (Sender)
    {
        return Sender->StartStream();
    }
    return false;
}

bool URship2110Subsystem::StopStream(const FString& StreamId)
{
    URship2110VideoSender* Sender = GetVideoSender(StreamId);
    if (Sender)
    {
        Sender->StopStream();
        return true;
    }
    return false;
}

void URship2110Subsystem::RefreshStreamRenderContextBindings()
{
    if (StreamToContextBinding.Num() == 0)
    {
        return;
    }

    URshipSubsystem* RshipSubsystem = GetRshipSubsystem();
    if (!RshipSubsystem)
    {
        return;
    }

    URshipContentMappingManager* MappingManager = RshipSubsystem->GetContentMappingManager();
    if (!MappingManager)
    {
        return;
    }

    const TArray<FRshipRenderContextState> RenderContexts = MappingManager->GetRenderContexts();
    if (RenderContexts.Num() == 0)
    {
        return;
    }

    TArray<FString> ToUnbind;
    for (const TPair<FString, FRship2110RenderContextBinding>& Binding : StreamToContextBinding)
    {
        const FString& StreamId = Binding.Key;
        const FString& RenderContextId = Binding.Value.RenderContextId;
        const FRship2110RenderContextBinding& BoundContext = Binding.Value;

        URship2110VideoSender* Sender = GetVideoSender(StreamId);
        if (!Sender)
        {
            ToUnbind.Add(StreamId);
            continue;
        }

        UTextureRenderTarget2D* RenderTarget = nullptr;
        for (const FRshipRenderContextState& ContextState : RenderContexts)
        {
            if (ContextState.Id == RenderContextId)
            {
                RenderTarget = Cast<UTextureRenderTarget2D>(ContextState.ResolvedTexture);
                break;
            }
        }

        if (!RenderTarget)
        {
            continue;
        }

        Sender->SetRenderTarget(RenderTarget);
        if (BoundContext.bUseCaptureRect)
        {
            Sender->SetCaptureRect(BoundContext.CaptureRect);
        }
        else
        {
            Sender->ClearCaptureRect();
        }
        Sender->SetCaptureSource(ERship2110CaptureSource::RenderTarget);
    }

    for (const FString& StreamId : ToUnbind)
    {
        StreamToContextBinding.Remove(StreamId);
        UE_LOG(LogRship2110, Log, TEXT("Removed render context binding for missing stream %s"), *StreamId);
    }
}

bool URship2110Subsystem::ResolveRenderContextRenderTarget(const FString& ContextId, UTextureRenderTarget2D*& OutRenderTarget)
{
    OutRenderTarget = nullptr;

    if (ContextId.IsEmpty())
    {
        return false;
    }

    URshipSubsystem* RshipSubsystem = GetRshipSubsystem();
    if (!RshipSubsystem)
    {
        return false;
    }

    URshipContentMappingManager* MappingManager = RshipSubsystem->GetContentMappingManager();
    if (!MappingManager)
    {
        return false;
    }

    const TArray<FRshipRenderContextState> RenderContexts = MappingManager->GetRenderContexts();
    for (const FRshipRenderContextState& ContextState : RenderContexts)
    {
        if (ContextState.Id == ContextId && ContextState.bEnabled)
        {
            OutRenderTarget = Cast<UTextureRenderTarget2D>(ContextState.ResolvedTexture);
            return OutRenderTarget != nullptr;
        }
    }

    return false;
}

bool URship2110Subsystem::ConnectIPMX(const FString& RegistryUrl)
{
    return IPMXService ? IPMXService->ConnectToRegistry(RegistryUrl) : false;
}

void URship2110Subsystem::DisconnectIPMX()
{
    if (IPMXService)
    {
        IPMXService->DisconnectFromRegistry();
    }
}

bool URship2110Subsystem::IsIPMXConnected() const
{
    return IPMXService && IPMXService->IsConnected();
}

FRshipIPMXStatus URship2110Subsystem::GetIPMXStatus() const
{
    return IPMXService ? IPMXService->GetStatus() : FRshipIPMXStatus();
}

FRshipRivermaxStatus URship2110Subsystem::GetRivermaxStatus() const
{
    return RivermaxManager ? RivermaxManager->GetStatus() : FRshipRivermaxStatus();
}

TArray<FRshipRivermaxDevice> URship2110Subsystem::GetRivermaxDevices() const
{
    return RivermaxManager ? RivermaxManager->GetDevices() : TArray<FRshipRivermaxDevice>();
}

bool URship2110Subsystem::SelectRivermaxDevice(const FString& IPAddress)
{
    return RivermaxManager ? RivermaxManager->SelectDeviceByIP(IPAddress) : false;
}

URship2110Settings* URship2110Subsystem::GetSettings() const
{
    return URship2110Settings::Get();
}

bool URship2110Subsystem::IsRivermaxAvailable() const
{
#if RSHIP_RIVERMAX_AVAILABLE
    return true;
#else
    return false;
#endif
}

bool URship2110Subsystem::IsPTPAvailable() const
{
#if RSHIP_PTP_AVAILABLE
    return true;
#else
    return false;
#endif
}

bool URship2110Subsystem::IsIPMXAvailable() const
{
#if RSHIP_IPMX_AVAILABLE
    return true;
#else
    return false;
#endif
}

void URship2110Subsystem::InitializePTPService()
{
    PTPService = NewObject<URshipPTPService>(this);
    if (PTPService->Initialize(this))
    {
        PTPService->OnStateChanged.AddDynamic(this, &URship2110Subsystem::OnPTPStateChangedInternal);
        UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: PTP service initialized"));
    }
    else
    {
        UE_LOG(LogRship2110, Warning, TEXT("Rship2110Subsystem: PTP service initialization failed"));
    }
}

void URship2110Subsystem::InitializeRivermaxManager()
{
    RivermaxManager = NewObject<URivermaxManager>(this);
    if (RivermaxManager->Initialize(this))
    {
        RivermaxManager->OnDeviceChanged.AddDynamic(this, &URship2110Subsystem::OnRivermaxDeviceChangedInternal);
        UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: Rivermax manager initialized"));
    }
    else
    {
        UE_LOG(LogRship2110, Warning, TEXT("Rship2110Subsystem: Rivermax manager initialization failed"));
    }
}

void URship2110Subsystem::InitializeIPMXService()
{
    IPMXService = NewObject<URshipIPMXService>(this);
    if (IPMXService->Initialize(this))
    {
        IPMXService->OnStateChanged.AddDynamic(this, &URship2110Subsystem::OnIPMXStateChangedInternal);
        UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: IPMX service initialized"));
    }
    else
    {
        UE_LOG(LogRship2110, Warning, TEXT("Rship2110Subsystem: IPMX service initialization failed"));
    }
}

void URship2110Subsystem::InitializeVideoCapture()
{
    VideoCapture = NewObject<URship2110VideoCapture>(this);

    URship2110Settings* Settings = URship2110Settings::Get();
    if (Settings)
    {
        VideoCapture->Initialize(Settings->DefaultVideoFormat);
    }
    else
    {
        FRship2110VideoFormat DefaultFormat;
        VideoCapture->Initialize(DefaultFormat);
    }

    UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: Video capture initialized"));
}

void URship2110Subsystem::OnPTPStateChangedInternal(ERshipPTPState NewState)
{
    OnPTPStateChanged.Broadcast(NewState);
}

void URship2110Subsystem::OnStreamStateChangedInternal(const FString& StreamId, ERship2110StreamState NewState)
{
    OnStreamStateChanged.Broadcast(StreamId, NewState);
}

void URship2110Subsystem::OnIPMXStateChangedInternal(ERshipIPMXConnectionState NewState)
{
    OnIPMXConnectionStateChanged.Broadcast(NewState);
}

void URship2110Subsystem::OnRivermaxDeviceChangedInternal(int32 DeviceIndex, const FRshipRivermaxDevice& Device)
{
    OnRivermaxDeviceChanged.Broadcast(DeviceIndex, Device);
}

// ============================================================================
// BLUEPRINT FUNCTION LIBRARY
// ============================================================================

URship2110Subsystem* URship2110BlueprintLibrary::GetRship2110Subsystem(UObject* WorldContextObject)
{
    return GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
}

double URship2110BlueprintLibrary::GetPTPTimeSeconds(UObject* WorldContextObject)
{
    URship2110Subsystem* Subsystem = GetRship2110Subsystem(WorldContextObject);
    if (Subsystem)
    {
        return Subsystem->GetPTPTime().ToSeconds();
    }
    return 0.0;
}

bool URship2110BlueprintLibrary::IsPTPLocked(UObject* WorldContextObject)
{
    URship2110Subsystem* Subsystem = GetRship2110Subsystem(WorldContextObject);
    return Subsystem && Subsystem->IsPTPLocked();
}

int64 URship2110BlueprintLibrary::FrameRateToNanoseconds(const FFrameRate& FrameRate)
{
    if (FrameRate.Numerator == 0)
    {
        return 0;
    }
    return static_cast<int64>(1000000000.0 * FrameRate.Denominator / FrameRate.Numerator);
}

double URship2110BlueprintLibrary::VideoFormatToBitrate(const FRship2110VideoFormat& VideoFormat)
{
    double FrameSizeBits = VideoFormat.GetFrameSizeBytes() * 8.0;
    double FrameRate = VideoFormat.GetFrameRateDecimal();
    return (FrameSizeBits * FrameRate) / 1000000.0;
}

FRship2110VideoFormat URship2110BlueprintLibrary::CreateVideoFormat(
    int32 Width,
    int32 Height,
    const FFrameRate& FrameRate)
{
    FRship2110VideoFormat Format;
    Format.Width = Width;
    Format.Height = Height;
    Format.FrameRateNumerator = FrameRate.Numerator;
    Format.FrameRateDenominator = FrameRate.Denominator;
    return Format;
}

FRship2110TransportParams URship2110BlueprintLibrary::CreateTransportParams(
    const FString& MulticastIP,
    int32 Port)
{
    FRship2110TransportParams Params;
    Params.DestinationIP = MulticastIP;
    Params.DestinationPort = Port;
    Params.SourcePort = Port;
    return Params;
}
