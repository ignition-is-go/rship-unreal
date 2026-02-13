// Copyright Rocketship. All Rights Reserved.

#include "Rship2110Subsystem.h"
#include "Rship2110Settings.h"
#include "Rship2110.h"
#include "PTP/RshipPTPService.h"
#include "Rivermax/RivermaxManager.h"
#include "Rivermax/Rship2110VideoSender.h"
#include "IPMX/RshipIPMXService.h"
#include "Capture/Rship2110VideoCapture.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "Containers/Set.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/IConsoleManager.h"

// Include RshipExec for integration
#include "RshipSubsystem.h"
#include "RshipContentMappingManager.h"

DECLARE_STATS_GROUP(TEXT("Rship2110"), STATGROUP_Rship2110, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Rship2110 Tick"), STAT_Rship2110Tick, STATGROUP_Rship2110);

static TAutoConsoleVariable<float> CVarRship2110ClusterSyncRateHz(
    TEXT("r.Rship2110.ClusterSyncRateHz"),
    0.0f,
    TEXT("Override default cluster sync rate in Hz. <=0 uses project settings/runtime API."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarRship2110LocalRenderSubsteps(
    TEXT("r.Rship2110.LocalRenderSubsteps"),
    0,
    TEXT("Override local render substeps. <=0 uses project settings/runtime API."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarRship2110MaxSyncCatchupSteps(
    TEXT("r.Rship2110.MaxSyncCatchupSteps"),
    0,
    TEXT("Override max cluster sync catch-up steps. <=0 uses project settings/runtime API."),
    ECVF_Default);

namespace
{
FString ExtractSyncDomainIdFromPayload(const FString& Payload)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return FString();
    }

    FString SyncDomainId;
    if (JsonObject->TryGetStringField(TEXT("syncDomainId"), SyncDomainId))
    {
        return SyncDomainId.TrimStartAndEnd();
    }

    const TSharedPtr<FJsonObject>* DataObjectPtr = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("data"), DataObjectPtr) && DataObjectPtr && DataObjectPtr->IsValid())
    {
        if ((*DataObjectPtr)->TryGetStringField(TEXT("syncDomainId"), SyncDomainId))
        {
            return SyncDomainId.TrimStartAndEnd();
        }
    }

    return FString();
}
}

void URship2110Subsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogRship2110, Log, TEXT("Rship2110Subsystem: Initializing..."));

    // Load settings
    URship2110Settings* Settings = URship2110Settings::Get();
    if (Settings)
    {
        ClusterSyncRateHz = FMath::Max(1.0f, Settings->ClusterSyncRateHz);
        LocalRenderSubsteps = FMath::Max(1, Settings->LocalRenderSubsteps);
        MaxSyncCatchupSteps = FMath::Max(1, Settings->MaxSyncCatchupSteps);
    }
    ClusterSyncFrameAccumulator = 0.0;
    LocalRenderStepCounter = 0;
    ActiveSyncDomainId = DefaultSyncDomainId;

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
    InitializeClusterState();

    if (URshipSubsystem* RshipSubsystem = GetRshipSubsystem())
    {
        RshipAuthoritativeInboundHandle = RshipSubsystem->OnAuthoritativeInboundQueued().AddUObject(
            this,
            &URship2110Subsystem::HandleAuthoritativeRshipInbound);
    }

    bIsInitialized = true;

    UE_LOG(LogRship2110, Log, TEXT("Cluster timing: syncRate=%.2fHz renderSubsteps=%d maxCatchup=%d"),
        ClusterSyncRateHz, LocalRenderSubsteps, MaxSyncCatchupSteps);

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
    PendingClusterStates.Empty();
    SyncDomains.Empty();
    StreamOwnerNodeCache.Empty();
    ActiveClusterState = FRship2110ClusterState();
    ActiveSyncDomainId = DefaultSyncDomainId;
    LocalRenderStepCounter = 0;
    ClusterFrameCounter = 0;
    ClusterDataSequenceCounter = 0;
    ClusterSyncFrameAccumulator = 0.0;
    ClusterSyncRateHz = 60.0f;
    LocalRenderSubsteps = 1;
    MaxSyncCatchupSteps = 4;

    if (URshipSubsystem* RshipSubsystem = GetRshipSubsystem())
    {
        if (RshipAuthoritativeInboundHandle.IsValid())
        {
            RshipSubsystem->OnAuthoritativeInboundQueued().Remove(RshipAuthoritativeInboundHandle);
            RshipAuthoritativeInboundHandle.Reset();
        }
    }

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
    const float CVarSyncRateHz = CVarRship2110ClusterSyncRateHz.GetValueOnGameThread();
    if (CVarSyncRateHz > 0.0f && !FMath::IsNearlyEqual(ClusterSyncRateHz, FMath::Max(1.0f, CVarSyncRateHz)))
    {
        SetClusterSyncRateHz(CVarSyncRateHz);
    }
    const int32 CVarRenderSubsteps = CVarRship2110LocalRenderSubsteps.GetValueOnGameThread();
    if (CVarRenderSubsteps > 0 && LocalRenderSubsteps != FMath::Max(1, CVarRenderSubsteps))
    {
        SetLocalRenderSubsteps(CVarRenderSubsteps);
    }
    const int32 CVarCatchupSteps = CVarRship2110MaxSyncCatchupSteps.GetValueOnGameThread();
    if (CVarCatchupSteps > 0 && MaxSyncCatchupSteps != FMath::Max(1, CVarCatchupSteps))
    {
        SetMaxSyncCatchupSteps(CVarCatchupSteps);
    }

    const int32 RenderSubsteps = FMath::Max(1, LocalRenderSubsteps);

    if (bIsInitialized)
    {
        LocalRenderStepCounter += RenderSubsteps;

        const double SyncRateHz = FMath::Max(1.0, static_cast<double>(ClusterSyncRateHz));
        ClusterSyncFrameAccumulator += static_cast<double>(DeltaTime) * SyncRateHz;
        const int32 RawSyncSteps = FMath::FloorToInt(ClusterSyncFrameAccumulator);
        const int32 MaxSteps = FMath::Max(1, MaxSyncCatchupSteps);
        int32 SyncSteps = FMath::Min(RawSyncSteps, MaxSteps);

        if (RawSyncSteps > MaxSteps)
        {
            UE_LOG(LogRship2110, VeryVerbose, TEXT("Cluster sync catch-up clamped (requested=%d applied=%d)"), RawSyncSteps, SyncSteps);
            ClusterSyncFrameAccumulator = 0.0;
        }
        else
        {
            ClusterSyncFrameAccumulator -= static_cast<double>(SyncSteps);
        }

        for (int32 Step = 0; Step < SyncSteps; ++Step)
        {
            ++ClusterFrameCounter;
            PurgeExpiredPreparedStates();
            ProcessPendingClusterStates();
            ProcessPendingClusterDataMessages();
        }

        TickNonDefaultSyncDomains(DeltaTime);
    }

    if (PTPService)
    {
        PTPService->Tick(DeltaTime);
    }

    if (RivermaxManager)
    {
        const float SubstepDelta = DeltaTime / static_cast<float>(RenderSubsteps);
        for (int32 Step = 0; Step < RenderSubsteps; ++Step)
        {
            RivermaxManager->Tick(SubstepDelta);
        }
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
        EvaluateClusterFailover();
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

    if (ActiveClusterState.bStrictNodeOwnership && LocalClusterNodeId.Len() > 0)
    {
        // Default newly created streams to local ownership unless explicitly assigned otherwise.
        if (GetClusterOwnershipForStream(StreamId).IsEmpty())
        {
            SetClusterOwnershipForStream(StreamId, LocalClusterNodeId, false);
        }
    }

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
        if (bIsInitialized)
        {
            SetClusterOwnershipForStream(StreamId, TEXT(""), false);
        }
        return bDestroyed;
    }

    StreamToContextBinding.Remove(StreamId);
    if (bIsInitialized)
    {
        SetClusterOwnershipForStream(StreamId, TEXT(""), false);
    }
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
    if (!IsStreamOwnedByLocalNode(StreamId))
    {
        UE_LOG(LogRship2110, Verbose, TEXT("Bound stream %s to context %s, but local node %s does not own this stream"), *StreamId, *RenderContextId, *LocalClusterNodeId);
    }
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
    if (!IsStreamOwnedByLocalNode(StreamId))
    {
        UE_LOG(LogRship2110, Warning, TEXT("StartStream denied for %s: local node %s is not owner"), *StreamId, *LocalClusterNodeId);
        return false;
    }

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

    TMap<FString, UTextureRenderTarget2D*> RenderContextTargets;
    RenderContextTargets.Reserve(RenderContexts.Num());
    for (const FRshipRenderContextState& ContextState : RenderContexts)
    {
        if (UTextureRenderTarget2D* RenderTarget = Cast<UTextureRenderTarget2D>(ContextState.ResolvedTexture))
        {
            RenderContextTargets.Add(ContextState.Id, RenderTarget);
        }
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

        if (!IsStreamOwnedByLocalNode(StreamId))
        {
            if (Sender->IsStreaming())
            {
                Sender->StopStream();
            }
            continue;
        }

        UTextureRenderTarget2D* const* FoundRenderTarget = RenderContextTargets.Find(RenderContextId);
        if (!FoundRenderTarget || !*FoundRenderTarget)
        {
            continue;
        }

        Sender->SetRenderTarget(*FoundRenderTarget);
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

void URship2110Subsystem::SetLocalClusterNodeId(const FString& NodeId)
{
    const FString Trimmed = NodeId.TrimStartAndEnd();
    if (Trimmed.IsEmpty() || Trimmed == LocalClusterNodeId)
    {
        return;
    }

    LocalClusterNodeId = Trimmed;
    UE_LOG(LogRship2110, Log, TEXT("Cluster local node ID set to %s"), *LocalClusterNodeId);
}

ERship2110ClusterRole URship2110Subsystem::GetLocalClusterRole() const
{
    if (ActiveClusterState.ActiveAuthorityNodeId.IsEmpty())
    {
        return ERship2110ClusterRole::Unknown;
    }
    return IsLocalNodeAuthority() ? ERship2110ClusterRole::Primary : ERship2110ClusterRole::Secondary;
}

bool URship2110Subsystem::IsLocalNodeAuthority() const
{
    return !LocalClusterNodeId.IsEmpty()
        && !ActiveClusterState.ActiveAuthorityNodeId.IsEmpty()
        && LocalClusterNodeId.Equals(ActiveClusterState.ActiveAuthorityNodeId, ESearchCase::CaseSensitive);
}

bool URship2110Subsystem::QueueClusterStateUpdate(const FRship2110ClusterState& ClusterState)
{
    if (IsClusterStateStale(ClusterState))
    {
        return false;
    }

    FRship2110ClusterState Update = ClusterState;
    if (Update.ApplyFrame <= ClusterFrameCounter)
    {
        Update.ApplyFrame = ClusterFrameCounter + 1;
    }
    if (Update.ActiveAuthorityNodeId.IsEmpty())
    {
        Update.ActiveAuthorityNodeId = ActiveClusterState.ActiveAuthorityNodeId.IsEmpty()
            ? LocalClusterNodeId
            : ActiveClusterState.ActiveAuthorityNodeId;
    }
    if (Update.FailoverTimeoutSeconds < 0.1f)
    {
        Update.FailoverTimeoutSeconds = 0.1f;
    }

    PendingClusterStates.RemoveAll([&Update](const FRship2110ClusterState& Existing)
    {
        return Existing.Epoch == Update.Epoch && Existing.Version == Update.Version;
    });
    PendingClusterStates.Add(Update);
    PendingClusterStates.Sort([](const FRship2110ClusterState& A, const FRship2110ClusterState& B)
    {
        if (A.ApplyFrame != B.ApplyFrame)
        {
            return A.ApplyFrame < B.ApplyFrame;
        }
        if (A.Epoch != B.Epoch)
        {
            return A.Epoch < B.Epoch;
        }
        return A.Version < B.Version;
    });
    return true;
}

void URship2110Subsystem::SetClusterOwnershipForStream(const FString& StreamId, const FString& OwnerNodeId, bool bApplyNextFrame)
{
    if (!IsLocalNodeAuthority())
    {
        UE_LOG(LogRship2110, Warning, TEXT("SetClusterOwnershipForStream ignored: local node %s is not authority"), *LocalClusterNodeId);
        return;
    }

    const FString TrimmedStreamId = StreamId.TrimStartAndEnd();
    const FString TrimmedOwner = OwnerNodeId.TrimStartAndEnd();
    if (TrimmedStreamId.IsEmpty())
    {
        return;
    }

    FRship2110ClusterState Updated = ActiveClusterState;
    if (Updated.Epoch <= 0)
    {
        Updated.Epoch = 1;
    }
    Updated.Version = FMath::Max(Updated.Version + 1, 1);
    Updated.ApplyFrame = bApplyNextFrame ? (ClusterFrameCounter + 1) : ClusterFrameCounter;
    if (Updated.ActiveAuthorityNodeId.IsEmpty())
    {
        Updated.ActiveAuthorityNodeId = LocalClusterNodeId;
    }

    for (FRship2110ClusterNodeStreams& Assignment : Updated.NodeStreamAssignments)
    {
        Assignment.StreamIds.RemoveAll([&TrimmedStreamId](const FString& Existing)
        {
            return Existing == TrimmedStreamId;
        });
    }
    Updated.NodeStreamAssignments.RemoveAll([](const FRship2110ClusterNodeStreams& Assignment)
    {
        return Assignment.NodeId.IsEmpty() || Assignment.StreamIds.Num() == 0;
    });

    if (!TrimmedOwner.IsEmpty())
    {
        FRship2110ClusterNodeStreams* Assignment = Updated.NodeStreamAssignments.FindByPredicate([&TrimmedOwner](const FRship2110ClusterNodeStreams& Existing)
        {
            return Existing.NodeId == TrimmedOwner;
        });
        if (!Assignment)
        {
            FRship2110ClusterNodeStreams NewAssignment;
            NewAssignment.NodeId = TrimmedOwner;
            Updated.NodeStreamAssignments.Add(NewAssignment);
            Assignment = &Updated.NodeStreamAssignments.Last();
        }
        Assignment->StreamIds.AddUnique(TrimmedStreamId);
    }

    Updated.ApplyFrame = bApplyNextFrame ? FMath::Max<int64>(Updated.ApplyFrame, ClusterFrameCounter + 2) : (ClusterFrameCounter + 1);
    SubmitAuthorityClusterStatePrepare(Updated, true);
}

bool URship2110Subsystem::UpdateClusterFailoverConfig(
    bool bFailoverEnabled,
    bool bAllowAutoPromotion,
    float FailoverTimeoutSeconds,
    bool bStrictNodeOwnership,
    bool bApplyNextFrame)
{
    if (!IsLocalNodeAuthority())
    {
        UE_LOG(LogRship2110, Warning, TEXT("UpdateClusterFailoverConfig ignored: local node %s is not authority"), *LocalClusterNodeId);
        return false;
    }

    FRship2110ClusterState Updated = ActiveClusterState;
    if (Updated.Epoch <= 0)
    {
        Updated.Epoch = 1;
    }
    Updated.Version = FMath::Max(Updated.Version + 1, 1);
    Updated.ApplyFrame = bApplyNextFrame ? (ClusterFrameCounter + 2) : (ClusterFrameCounter + 1);
    Updated.ActiveAuthorityNodeId = LocalClusterNodeId;
    Updated.bFailoverEnabled = bFailoverEnabled;
    Updated.bAllowAutoPromotion = bAllowAutoPromotion;
    Updated.FailoverTimeoutSeconds = FMath::Clamp(FailoverTimeoutSeconds, 0.1f, 60.0f);
    Updated.bStrictNodeOwnership = bStrictNodeOwnership;

    return SubmitAuthorityClusterStatePrepare(Updated, true);
}

FString URship2110Subsystem::GetClusterOwnershipForStream(const FString& StreamId) const
{
    const FString* OwnerNode = StreamOwnerNodeCache.Find(StreamId);
    return OwnerNode ? *OwnerNode : FString();
}

TArray<FString> URship2110Subsystem::GetLocallyOwnedStreams() const
{
    if (!ActiveClusterState.bStrictNodeOwnership)
    {
        return GetActiveStreamIds();
    }

    TArray<FString> Owned;
    for (const TPair<FString, FString>& Pair : StreamOwnerNodeCache)
    {
        if (Pair.Value == LocalClusterNodeId)
        {
            Owned.Add(Pair.Key);
        }
    }
    return Owned;
}

void URship2110Subsystem::NotifyClusterAuthorityHeartbeat(const FString& AuthorityNodeId, int32 Epoch, int32 Version)
{
    if (AuthorityNodeId.IsEmpty())
    {
        return;
    }

    if (Epoch == ActiveClusterState.Epoch
        && Version == ActiveClusterState.Version
        && AuthorityNodeId == ActiveClusterState.ActiveAuthorityNodeId)
    {
        LastAuthorityHeartbeatTime = FPlatformTime::Seconds();
    }
}

void URship2110Subsystem::PromoteLocalNodeToPrimary(bool bApplyNextFrame)
{
    if (LocalClusterNodeId.IsEmpty())
    {
        return;
    }

    FRship2110ClusterState Update = ActiveClusterState;
    Update.Epoch = FMath::Max(Update.Epoch + 1, 1);
    Update.Version = 1;
    Update.ActiveAuthorityNodeId = LocalClusterNodeId;
    Update.ApplyFrame = bApplyNextFrame ? (ClusterFrameCounter + 1) : ClusterFrameCounter;
    if (!Update.FailoverPriority.Contains(LocalClusterNodeId))
    {
        Update.FailoverPriority.Insert(LocalClusterNodeId, 0);
    }

    if (bApplyNextFrame)
    {
        QueueClusterStateUpdate(Update);
    }
    else
    {
        ApplyClusterStateNow(Update);
    }
}

FString URship2110Subsystem::ComputeClusterStateHash(const FRship2110ClusterState& ClusterState) const
{
    const FString Canonical = BuildClusterStateCanonicalString(ClusterState);
    const uint32 Crc = FCrc::StrCrc32(*Canonical);
    return FString::Printf(TEXT("%08X"), Crc);
}

bool URship2110Subsystem::SubmitAuthorityClusterStatePrepare(FRship2110ClusterState ClusterState, bool bAutoCommitOnQuorum)
{
    if (!IsLocalNodeAuthority())
    {
        UE_LOG(LogRship2110, Warning, TEXT("SubmitAuthorityClusterStatePrepare denied: local node %s is not authority"), *LocalClusterNodeId);
        return false;
    }

    if (ClusterState.ActiveAuthorityNodeId.IsEmpty())
    {
        ClusterState.ActiveAuthorityNodeId = LocalClusterNodeId;
    }
    if (ClusterState.ActiveAuthorityNodeId != LocalClusterNodeId)
    {
        UE_LOG(LogRship2110, Warning, TEXT("SubmitAuthorityClusterStatePrepare denied: authority mismatch (%s vs %s)"),
            *ClusterState.ActiveAuthorityNodeId, *LocalClusterNodeId);
        return false;
    }

    ClusterState.Epoch = FMath::Max(ClusterState.Epoch, ActiveClusterState.Epoch);
    if (ClusterState.Epoch == ActiveClusterState.Epoch && ClusterState.Version <= ActiveClusterState.Version)
    {
        ClusterState.Version = ActiveClusterState.Version + 1;
    }
    ClusterState.ApplyFrame = FMath::Max(ClusterState.ApplyFrame, ClusterFrameCounter + 2);
    ClusterState.PrepareTimeoutSeconds = FMath::Max(0.1f, ClusterState.PrepareTimeoutSeconds);

    FRship2110ClusterPrepareMessage PrepareMessage;
    PrepareMessage.AuthorityNodeId = LocalClusterNodeId;
    PrepareMessage.Epoch = ClusterState.Epoch;
    PrepareMessage.Version = ClusterState.Version;
    PrepareMessage.ApplyFrame = ClusterState.ApplyFrame;
    PrepareMessage.ClusterState = ClusterState;
    PrepareMessage.StateHash = ComputeClusterStateHash(ClusterState);
    PrepareMessage.RequiredAckCount = ClusterState.RequiredAckCount;

    const FString StateKey = MakePreparedStateKey(PrepareMessage.Epoch, PrepareMessage.Version, PrepareMessage.StateHash);

    FPreparedClusterStateEntry Entry;
    Entry.Prepare = PrepareMessage;
    Entry.AckedNodeIds.Add(LocalClusterNodeId);
    Entry.bAutoCommitOnQuorum = bAutoCommitOnQuorum;
    Entry.bCommitBroadcast = false;
    Entry.CreatedTimeSeconds = FPlatformTime::Seconds();
    PreparedClusterStates.Add(StateKey, Entry);

    OnClusterPrepareOutbound.Broadcast(PrepareMessage);
    if (bAutoCommitOnQuorum)
    {
        FinalizePreparedStateCommit(StateKey);
    }
    return true;
}

bool URship2110Subsystem::ReceiveClusterStatePrepare(const FRship2110ClusterPrepareMessage& PrepareMessage)
{
    if (PrepareMessage.AuthorityNodeId.IsEmpty() || PrepareMessage.StateHash.IsEmpty())
    {
        return false;
    }

    const FString ExpectedHash = ComputeClusterStateHash(PrepareMessage.ClusterState);
    if (ExpectedHash != PrepareMessage.StateHash)
    {
        UE_LOG(LogRship2110, Warning, TEXT("Rejected cluster prepare due to hash mismatch (expected %s got %s)"), *ExpectedHash, *PrepareMessage.StateHash);
        return false;
    }

    FRship2110ClusterState Candidate = PrepareMessage.ClusterState;
    Candidate.ActiveAuthorityNodeId = PrepareMessage.AuthorityNodeId;
    Candidate.Epoch = PrepareMessage.Epoch;
    Candidate.Version = PrepareMessage.Version;
    Candidate.ApplyFrame = PrepareMessage.ApplyFrame;
    Candidate.RequiredAckCount = PrepareMessage.RequiredAckCount;

    if (IsClusterStateStale(Candidate))
    {
        return false;
    }

    const FString StateKey = MakePreparedStateKey(PrepareMessage.Epoch, PrepareMessage.Version, PrepareMessage.StateHash);
    FPreparedClusterStateEntry Entry;
    Entry.Prepare = PrepareMessage;
    Entry.Prepare.ClusterState = Candidate;
    Entry.Prepare.RequiredAckCount = Candidate.RequiredAckCount;
    Entry.CreatedTimeSeconds = FPlatformTime::Seconds();
    Entry.bAutoCommitOnQuorum = false;

    if (const FPreparedClusterStateEntry* Existing = PreparedClusterStates.Find(StateKey))
    {
        Entry = *Existing;
        Entry.Prepare = PrepareMessage;
        Entry.Prepare.ClusterState = Candidate;
        Entry.CreatedTimeSeconds = FPlatformTime::Seconds();
    }

    if (!LocalClusterNodeId.IsEmpty())
    {
        Entry.AckedNodeIds.Add(LocalClusterNodeId);
    }
    PreparedClusterStates.Add(StateKey, Entry);

    if (!LocalClusterNodeId.IsEmpty())
    {
        FRship2110ClusterAckMessage AckMessage;
        AckMessage.NodeId = LocalClusterNodeId;
        AckMessage.AuthorityNodeId = PrepareMessage.AuthorityNodeId;
        AckMessage.Epoch = PrepareMessage.Epoch;
        AckMessage.Version = PrepareMessage.Version;
        AckMessage.StateHash = PrepareMessage.StateHash;
        OnClusterAckOutbound.Broadcast(AckMessage);
    }

    return true;
}

bool URship2110Subsystem::ReceiveClusterStateAck(const FRship2110ClusterAckMessage& AckMessage)
{
    if (!IsLocalNodeAuthority())
    {
        return false;
    }
    if (AckMessage.NodeId.IsEmpty() || AckMessage.StateHash.IsEmpty())
    {
        return false;
    }
    if (AckMessage.AuthorityNodeId != LocalClusterNodeId)
    {
        return false;
    }

    const FString StateKey = MakePreparedStateKey(AckMessage.Epoch, AckMessage.Version, AckMessage.StateHash);
    FPreparedClusterStateEntry* Entry = PreparedClusterStates.Find(StateKey);
    if (!Entry)
    {
        return false;
    }
    Entry->AckedNodeIds.Add(AckMessage.NodeId);

    if (Entry->bAutoCommitOnQuorum)
    {
        FinalizePreparedStateCommit(StateKey);
    }
    return true;
}

bool URship2110Subsystem::ReceiveClusterStateCommit(const FRship2110ClusterCommitMessage& CommitMessage)
{
    if (CommitMessage.AuthorityNodeId.IsEmpty() || CommitMessage.StateHash.IsEmpty())
    {
        return false;
    }

    const FString StateKey = MakePreparedStateKey(CommitMessage.Epoch, CommitMessage.Version, CommitMessage.StateHash);
    const FPreparedClusterStateEntry* Entry = PreparedClusterStates.Find(StateKey);
    if (!Entry)
    {
        return false;
    }
    if (Entry->Prepare.AuthorityNodeId != CommitMessage.AuthorityNodeId)
    {
        return false;
    }

    FRship2110ClusterState StateToApply = Entry->Prepare.ClusterState;
    StateToApply.Epoch = CommitMessage.Epoch;
    StateToApply.Version = CommitMessage.Version;
    StateToApply.ApplyFrame = CommitMessage.ApplyFrame;
    StateToApply.ActiveAuthorityNodeId = CommitMessage.AuthorityNodeId;

    const bool bQueued = QueueClusterStateUpdate(StateToApply);
    if (bQueued)
    {
        PreparedClusterStates.Remove(StateKey);
    }
    return bQueued;
}

bool URship2110Subsystem::SubmitAuthorityClusterDataMessage(const FString& Payload, int64 ApplyFrame)
{
    return SubmitAuthorityClusterDataMessageForDomain(Payload, ActiveSyncDomainId, ApplyFrame);
}

bool URship2110Subsystem::SubmitAuthorityClusterDataMessageForDomain(const FString& Payload, const FString& SyncDomainId, int64 ApplyFrame)
{
    if (!IsLocalNodeAuthority())
    {
        return false;
    }

    const FString TrimmedPayload = Payload.TrimStartAndEnd();
    if (TrimmedPayload.IsEmpty())
    {
        return false;
    }

    FRship2110ClusterDataMessage DataMessage;
    const FString ResolvedSyncDomainId = ResolveSyncDomainId(SyncDomainId.IsEmpty() ? ActiveSyncDomainId : SyncDomainId);
    FRship2110SyncDomainRuntime& SyncDomain = GetOrCreateSyncDomain(ResolvedSyncDomainId);
    const int64 DomainFrameCounter = ResolvedSyncDomainId.Equals(DefaultSyncDomainId, ESearchCase::IgnoreCase)
        ? ClusterFrameCounter
        : SyncDomain.FrameCounter;

    DataMessage.AuthorityNodeId = LocalClusterNodeId;
    DataMessage.Epoch = ActiveClusterState.Epoch;
    DataMessage.Sequence = ++ClusterDataSequenceCounter;
    DataMessage.SyncDomainId = ResolvedSyncDomainId;
    DataMessage.ApplyFrame = (ApplyFrame == INDEX_NONE)
        ? (DomainFrameCounter + 1)
        : FMath::Max<int64>(ApplyFrame, DomainFrameCounter + 1);
    DataMessage.Payload = TrimmedPayload;

    OnClusterDataOutbound.Broadcast(DataMessage);
    return true;
}

bool URship2110Subsystem::ReceiveClusterDataMessage(const FRship2110ClusterDataMessage& DataMessage)
{
    if (DataMessage.AuthorityNodeId.IsEmpty()
        || DataMessage.Payload.IsEmpty()
        || DataMessage.Epoch <= 0
        || DataMessage.Sequence <= 0)
    {
        return false;
    }

    if (DataMessage.Epoch < ActiveClusterState.Epoch)
    {
        return false;
    }

    if (!ActiveClusterState.ActiveAuthorityNodeId.IsEmpty()
        && DataMessage.AuthorityNodeId != ActiveClusterState.ActiveAuthorityNodeId)
    {
        return false;
    }

    if (!DataMessage.TargetNodeId.IsEmpty()
        && !LocalClusterNodeId.IsEmpty()
        && !DataMessage.TargetNodeId.Equals(LocalClusterNodeId, ESearchCase::IgnoreCase))
    {
        return false;
    }

    if (DataMessage.AuthorityNodeId == LocalClusterNodeId)
    {
        // Local authority already applied this payload through local ingest path.
        return false;
    }

    const FString ResolvedSyncDomainId = ResolveSyncDomainId(DataMessage.SyncDomainId);
    FRship2110SyncDomainRuntime& SyncDomain = GetOrCreateSyncDomain(ResolvedSyncDomainId);
    const int64 LastAppliedSequence = SyncDomain.LastAppliedSequenceByAuthority.FindRef(DataMessage.AuthorityNodeId);
    if (DataMessage.Sequence <= LastAppliedSequence)
    {
        return false;
    }

    FRship2110ClusterDataMessage Queued = DataMessage;
    Queued.SyncDomainId = ResolvedSyncDomainId;

    const int64 DomainFrameCounter = ResolvedSyncDomainId.Equals(DefaultSyncDomainId, ESearchCase::IgnoreCase)
        ? ClusterFrameCounter
        : SyncDomain.FrameCounter;
    if (Queued.ApplyFrame <= DomainFrameCounter)
    {
        Queued.ApplyFrame = DomainFrameCounter + 1;
    }

    SyncDomain.PendingDataMessages.RemoveAll([&Queued](const FRship2110ClusterDataMessage& Existing)
    {
        return Existing.AuthorityNodeId == Queued.AuthorityNodeId && Existing.Sequence == Queued.Sequence;
    });
    SyncDomain.PendingDataMessages.Add(MoveTemp(Queued));
    SyncDomain.PendingDataMessages.Sort([](const FRship2110ClusterDataMessage& A, const FRship2110ClusterDataMessage& B)
    {
        if (A.ApplyFrame != B.ApplyFrame)
        {
            return A.ApplyFrame < B.ApplyFrame;
        }
        if (A.AuthorityNodeId != B.AuthorityNodeId)
        {
            return A.AuthorityNodeId < B.AuthorityNodeId;
        }
        return A.Sequence < B.Sequence;
    });
    return true;
}

FString URship2110Subsystem::MakePreparedStateKey(int32 Epoch, int32 Version, const FString& StateHash) const
{
    return FString::Printf(TEXT("%d:%d:%s"), Epoch, Version, *StateHash);
}

FString URship2110Subsystem::BuildClusterStateCanonicalString(const FRship2110ClusterState& ClusterState) const
{
    FString Canonical;
    Canonical += FString::Printf(
        TEXT("e=%d|v=%d|f=%lld|a=%s|strict=%d|fo=%d|to=%.4f|auto=%d|acks=%d|pto=%.4f|"),
        ClusterState.Epoch,
        ClusterState.Version,
        ClusterState.ApplyFrame,
        *ClusterState.ActiveAuthorityNodeId,
        ClusterState.bStrictNodeOwnership ? 1 : 0,
        ClusterState.bFailoverEnabled ? 1 : 0,
        ClusterState.FailoverTimeoutSeconds,
        ClusterState.bAllowAutoPromotion ? 1 : 0,
        ClusterState.RequiredAckCount,
        ClusterState.PrepareTimeoutSeconds);

    TArray<FString> Priority = ClusterState.FailoverPriority;
    Priority.Sort();
    Canonical += TEXT("prio=");
    for (const FString& NodeId : Priority)
    {
        Canonical += NodeId;
        Canonical += TEXT(",");
    }
    Canonical += TEXT("|assign=");

    TArray<FRship2110ClusterNodeStreams> Assignments = ClusterState.NodeStreamAssignments;
    Assignments.Sort([](const FRship2110ClusterNodeStreams& A, const FRship2110ClusterNodeStreams& B)
    {
        return A.NodeId < B.NodeId;
    });

    for (const FRship2110ClusterNodeStreams& Assignment : Assignments)
    {
        Canonical += Assignment.NodeId;
        Canonical += TEXT(":");

        TArray<FString> StreamIds = Assignment.StreamIds;
        StreamIds.Sort();
        for (const FString& StreamId : StreamIds)
        {
            Canonical += StreamId;
            Canonical += TEXT(",");
        }
        Canonical += TEXT(";");
    }

    return Canonical;
}

int32 URship2110Subsystem::GetDiscoveredClusterNodeCount(const FRship2110ClusterState& ClusterState) const
{
    TSet<FString> NodeIds;
    if (!LocalClusterNodeId.IsEmpty())
    {
        NodeIds.Add(LocalClusterNodeId);
    }
    if (!ClusterState.ActiveAuthorityNodeId.IsEmpty())
    {
        NodeIds.Add(ClusterState.ActiveAuthorityNodeId);
    }
    for (const FString& NodeId : ClusterState.FailoverPriority)
    {
        if (!NodeId.IsEmpty())
        {
            NodeIds.Add(NodeId);
        }
    }
    for (const FRship2110ClusterNodeStreams& Assignment : ClusterState.NodeStreamAssignments)
    {
        if (!Assignment.NodeId.IsEmpty())
        {
            NodeIds.Add(Assignment.NodeId);
        }
    }
    return NodeIds.Num();
}

int32 URship2110Subsystem::ResolveRequiredAckCount(const FRship2110ClusterPrepareMessage& PrepareMessage) const
{
    const int32 DiscoveredNodeCount = FMath::Max(1, GetDiscoveredClusterNodeCount(PrepareMessage.ClusterState));
    if (PrepareMessage.RequiredAckCount > 0)
    {
        return FMath::Clamp(PrepareMessage.RequiredAckCount, 1, DiscoveredNodeCount);
    }
    return DiscoveredNodeCount;
}

bool URship2110Subsystem::HasPrepareAckQuorum(const FPreparedClusterStateEntry& PreparedEntry) const
{
    return PreparedEntry.AckedNodeIds.Num() >= ResolveRequiredAckCount(PreparedEntry.Prepare);
}

bool URship2110Subsystem::FinalizePreparedStateCommit(const FString& StateKey)
{
    FPreparedClusterStateEntry* Entry = PreparedClusterStates.Find(StateKey);
    if (!Entry || Entry->bCommitBroadcast || !HasPrepareAckQuorum(*Entry))
    {
        return false;
    }

    FRship2110ClusterCommitMessage CommitMessage;
    CommitMessage.AuthorityNodeId = Entry->Prepare.AuthorityNodeId;
    CommitMessage.Epoch = Entry->Prepare.Epoch;
    CommitMessage.Version = Entry->Prepare.Version;
    CommitMessage.ApplyFrame = Entry->Prepare.ApplyFrame;
    CommitMessage.StateHash = Entry->Prepare.StateHash;

    Entry->bCommitBroadcast = true;
    OnClusterCommitOutbound.Broadcast(CommitMessage);
    ReceiveClusterStateCommit(CommitMessage);
    return true;
}

void URship2110Subsystem::PurgeExpiredPreparedStates()
{
    if (PreparedClusterStates.Num() == 0)
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    TArray<FString> ExpiredKeys;
    for (const TPair<FString, FPreparedClusterStateEntry>& Pair : PreparedClusterStates)
    {
        const float Timeout = FMath::Max(0.1f, Pair.Value.Prepare.ClusterState.PrepareTimeoutSeconds);
        if ((Now - Pair.Value.CreatedTimeSeconds) >= static_cast<double>(Timeout))
        {
            ExpiredKeys.Add(Pair.Key);
        }
    }

    for (const FString& Key : ExpiredKeys)
    {
        PreparedClusterStates.Remove(Key);
    }
}

void URship2110Subsystem::InitializeClusterState()
{
    LocalClusterNodeId = ResolveLocalClusterNodeId();

    ActiveClusterState = FRship2110ClusterState();
    ActiveClusterState.Epoch = 1;
    ActiveClusterState.Version = 1;
    ActiveClusterState.ApplyFrame = 0;
    ActiveClusterState.ActiveAuthorityNodeId = LocalClusterNodeId;
    ActiveClusterState.bStrictNodeOwnership = true;
    ActiveClusterState.bFailoverEnabled = true;
    ActiveClusterState.FailoverTimeoutSeconds = 2.0f;
    ActiveClusterState.bAllowAutoPromotion = true;
    ActiveClusterState.RequiredAckCount = 0;
    ActiveClusterState.PrepareTimeoutSeconds = 3.0f;
    ActiveClusterState.FailoverPriority = { LocalClusterNodeId };

    PendingClusterStates.Empty();
    SyncDomains.Empty();
    PreparedClusterStates.Empty();
    FRship2110SyncDomainRuntime& DefaultDomain = GetOrCreateSyncDomain(DefaultSyncDomainId);
    DefaultDomain.SyncRateHz = ClusterSyncRateHz;
    DefaultDomain.FrameCounter = 0;
    DefaultDomain.FrameAccumulator = 0.0;
    DefaultDomain.PendingDataMessages.Empty();
    DefaultDomain.LastAppliedSequenceByAuthority.Empty();
    ActiveSyncDomainId = DefaultSyncDomainId;
    ClusterFrameCounter = 0;
    ClusterDataSequenceCounter = 0;
    LastAuthorityHeartbeatTime = FPlatformTime::Seconds();
    RebuildStreamOwnershipCache();

    UE_LOG(LogRship2110, Log, TEXT("Cluster state initialized. Node=%s Authority=%s"),
        *LocalClusterNodeId, *ActiveClusterState.ActiveAuthorityNodeId);
}

FString URship2110Subsystem::ResolveLocalClusterNodeId() const
{
    FString CommandLineNodeId;
    if (FParse::Value(FCommandLine::Get(), TEXT("dc_node="), CommandLineNodeId)
        || FParse::Value(FCommandLine::Get(), TEXT("DC_NODE="), CommandLineNodeId)
        || FParse::Value(FCommandLine::Get(), TEXT("rship_node="), CommandLineNodeId))
    {
        CommandLineNodeId = CommandLineNodeId.TrimStartAndEnd();
        if (!CommandLineNodeId.IsEmpty())
        {
            return CommandLineNodeId;
        }
    }

    const FString EnvNodeId = FPlatformMisc::GetEnvironmentVariable(TEXT("RSHIP_CLUSTER_NODE_ID")).TrimStartAndEnd();
    if (!EnvNodeId.IsEmpty())
    {
        return EnvNodeId;
    }

    const FString SessionName = FApp::GetSessionName().TrimStartAndEnd();
    if (!SessionName.IsEmpty())
    {
        return SessionName;
    }

    return FPlatformProcess::ComputerName();
}

FString URship2110Subsystem::ResolveSyncDomainId(const FString& SyncDomainId) const
{
    const FString Trimmed = SyncDomainId.TrimStartAndEnd();
    return Trimmed.IsEmpty() ? DefaultSyncDomainId : Trimmed;
}

URship2110Subsystem::FRship2110SyncDomainRuntime& URship2110Subsystem::GetOrCreateSyncDomain(const FString& SyncDomainId)
{
    const FString ResolvedSyncDomainId = ResolveSyncDomainId(SyncDomainId);
    FRship2110SyncDomainRuntime& Domain = SyncDomains.FindOrAdd(ResolvedSyncDomainId);
    if (Domain.SyncRateHz <= 0.0f)
    {
        Domain.SyncRateHz = ClusterSyncRateHz;
    }
    return Domain;
}

const URship2110Subsystem::FRship2110SyncDomainRuntime* URship2110Subsystem::FindSyncDomain(const FString& SyncDomainId) const
{
    return SyncDomains.Find(ResolveSyncDomainId(SyncDomainId));
}

void URship2110Subsystem::TickNonDefaultSyncDomains(float DeltaTime)
{
    for (TPair<FString, FRship2110SyncDomainRuntime>& Pair : SyncDomains)
    {
        if (Pair.Key.Equals(DefaultSyncDomainId, ESearchCase::IgnoreCase))
        {
            continue;
        }

        FRship2110SyncDomainRuntime& Domain = Pair.Value;
        const double DomainRateHz = FMath::Max(1.0, static_cast<double>(Domain.SyncRateHz));
        Domain.FrameAccumulator += static_cast<double>(DeltaTime) * DomainRateHz;

        const int32 RawSyncSteps = FMath::FloorToInt(Domain.FrameAccumulator);
        const int32 MaxSteps = FMath::Max(1, MaxSyncCatchupSteps);
        const int32 SyncSteps = FMath::Min(RawSyncSteps, MaxSteps);

        if (RawSyncSteps > MaxSteps)
        {
            Domain.FrameAccumulator = 0.0;
        }
        else
        {
            Domain.FrameAccumulator -= static_cast<double>(SyncSteps);
        }

        Domain.FrameCounter += SyncSteps;
        ProcessPendingClusterDataMessagesForDomain(Pair.Key, Domain);
    }
}

int64 URship2110Subsystem::GetClusterFrameCounterForDomain(const FString& SyncDomainId) const
{
    const FString ResolvedSyncDomainId = ResolveSyncDomainId(SyncDomainId);
    if (ResolvedSyncDomainId.Equals(DefaultSyncDomainId, ESearchCase::IgnoreCase))
    {
        return ClusterFrameCounter;
    }

    const FRship2110SyncDomainRuntime* Domain = FindSyncDomain(ResolvedSyncDomainId);
    return Domain ? Domain->FrameCounter : 0;
}

void URship2110Subsystem::SetClusterSyncRateHz(float InSyncRateHz)
{
    ClusterSyncRateHz = FMath::Max(1.0f, InSyncRateHz);
    FRship2110SyncDomainRuntime& DefaultDomain = GetOrCreateSyncDomain(DefaultSyncDomainId);
    DefaultDomain.SyncRateHz = ClusterSyncRateHz;
}

void URship2110Subsystem::SetLocalRenderSubsteps(int32 InSubsteps)
{
    LocalRenderSubsteps = FMath::Max(1, InSubsteps);
}

void URship2110Subsystem::SetMaxSyncCatchupSteps(int32 InMaxSteps)
{
    MaxSyncCatchupSteps = FMath::Max(1, InMaxSteps);
}

void URship2110Subsystem::SetActiveSyncDomainId(const FString& SyncDomainId)
{
    ActiveSyncDomainId = ResolveSyncDomainId(SyncDomainId);
    GetOrCreateSyncDomain(ActiveSyncDomainId);
}

bool URship2110Subsystem::SetSyncDomainRateHz(const FString& SyncDomainId, float SyncRateHz)
{
    if (SyncRateHz <= 0.0f)
    {
        return false;
    }

    const FString ResolvedSyncDomainId = ResolveSyncDomainId(SyncDomainId);
    if (ResolvedSyncDomainId.Equals(DefaultSyncDomainId, ESearchCase::IgnoreCase))
    {
        SetClusterSyncRateHz(SyncRateHz);
        return true;
    }

    FRship2110SyncDomainRuntime& Domain = GetOrCreateSyncDomain(ResolvedSyncDomainId);
    Domain.SyncRateHz = FMath::Max(1.0f, SyncRateHz);
    return true;
}

float URship2110Subsystem::GetSyncDomainRateHz(const FString& SyncDomainId) const
{
    const FString ResolvedSyncDomainId = ResolveSyncDomainId(SyncDomainId);
    if (ResolvedSyncDomainId.Equals(DefaultSyncDomainId, ESearchCase::IgnoreCase))
    {
        return ClusterSyncRateHz;
    }

    const FRship2110SyncDomainRuntime* Domain = FindSyncDomain(ResolvedSyncDomainId);
    return Domain ? Domain->SyncRateHz : 0.0f;
}

TArray<FString> URship2110Subsystem::GetSyncDomainIds() const
{
    TArray<FString> DomainIds;
    SyncDomains.GetKeys(DomainIds);
    DomainIds.Sort();
    return DomainIds;
}

void URship2110Subsystem::ProcessPendingClusterStates()
{
    if (PendingClusterStates.Num() == 0)
    {
        return;
    }

    TArray<FRship2110ClusterState> Remaining;
    Remaining.Reserve(PendingClusterStates.Num());

    for (const FRship2110ClusterState& State : PendingClusterStates)
    {
        if (State.ApplyFrame <= ClusterFrameCounter)
        {
            ApplyClusterStateNow(State);
        }
        else
        {
            Remaining.Add(State);
        }
    }
    PendingClusterStates = MoveTemp(Remaining);
}

void URship2110Subsystem::ProcessPendingClusterDataMessages()
{
    FRship2110SyncDomainRuntime& DefaultDomain = GetOrCreateSyncDomain(DefaultSyncDomainId);
    DefaultDomain.SyncRateHz = ClusterSyncRateHz;
    DefaultDomain.FrameCounter = ClusterFrameCounter;
    ProcessPendingClusterDataMessagesForDomain(DefaultSyncDomainId, DefaultDomain);
}

void URship2110Subsystem::ProcessPendingClusterDataMessagesForDomain(const FString& SyncDomainId, FRship2110SyncDomainRuntime& DomainRuntime)
{
    if (DomainRuntime.PendingDataMessages.Num() == 0)
    {
        return;
    }

    const bool bIsDefaultDomain = ResolveSyncDomainId(SyncDomainId).Equals(DefaultSyncDomainId, ESearchCase::IgnoreCase);
    const int64 DomainFrameCounter = bIsDefaultDomain ? ClusterFrameCounter : DomainRuntime.FrameCounter;

    TArray<FRship2110ClusterDataMessage> Remaining;
    Remaining.Reserve(DomainRuntime.PendingDataMessages.Num());

    for (const FRship2110ClusterDataMessage& DataMessage : DomainRuntime.PendingDataMessages)
    {
        if (DataMessage.ApplyFrame > DomainFrameCounter)
        {
            Remaining.Add(DataMessage);
            continue;
        }

        URshipSubsystem* RshipSubsystem = GetRshipSubsystem();
        if (!RshipSubsystem)
        {
            Remaining.Add(DataMessage);
            continue;
        }

        const int64 RshipApplyFrame = bIsDefaultDomain ? DataMessage.ApplyFrame : INDEX_NONE;
        RshipSubsystem->EnqueueReplicatedInboundMessage(DataMessage.Payload, RshipApplyFrame);
        DomainRuntime.LastAppliedSequenceByAuthority.Add(DataMessage.AuthorityNodeId, DataMessage.Sequence);
        OnClusterDataApplied.Broadcast(
            DataMessage.AuthorityNodeId,
            DataMessage.Epoch,
            DataMessage.Sequence,
            DataMessage.ApplyFrame);
    }

    DomainRuntime.PendingDataMessages = MoveTemp(Remaining);
}

bool URship2110Subsystem::ApplyClusterStateNow(const FRship2110ClusterState& ClusterState)
{
    if (IsClusterStateStale(ClusterState))
    {
        return false;
    }

    ActiveClusterState = ClusterState;
    if (ActiveClusterState.FailoverTimeoutSeconds < 0.1f)
    {
        ActiveClusterState.FailoverTimeoutSeconds = 0.1f;
    }
    if (ActiveClusterState.ActiveAuthorityNodeId.IsEmpty())
    {
        ActiveClusterState.ActiveAuthorityNodeId = LocalClusterNodeId;
    }
    if (!ActiveClusterState.FailoverPriority.Contains(ActiveClusterState.ActiveAuthorityNodeId))
    {
        ActiveClusterState.FailoverPriority.Insert(ActiveClusterState.ActiveAuthorityNodeId, 0);
    }

    TArray<FString> ConsumedPrepareKeys;
    for (const TPair<FString, FPreparedClusterStateEntry>& Pair : PreparedClusterStates)
    {
        const FRship2110ClusterPrepareMessage& Prepare = Pair.Value.Prepare;
        if (Prepare.Epoch < ActiveClusterState.Epoch
            || (Prepare.Epoch == ActiveClusterState.Epoch && Prepare.Version <= ActiveClusterState.Version))
        {
            ConsumedPrepareKeys.Add(Pair.Key);
        }
    }
    for (const FString& Key : ConsumedPrepareKeys)
    {
        PreparedClusterStates.Remove(Key);
    }

    RebuildStreamOwnershipCache();
    LastAuthorityHeartbeatTime = FPlatformTime::Seconds();

    const TArray<FString> ActiveStreams = GetActiveStreamIds();
    for (const FString& StreamId : ActiveStreams)
    {
        URship2110VideoSender* Sender = GetVideoSender(StreamId);
        if (Sender && Sender->IsStreaming() && !IsStreamOwnedByLocalNode(StreamId))
        {
            Sender->StopStream();
        }
    }

    OnClusterStateApplied.Broadcast(
        ActiveClusterState.Epoch,
        ActiveClusterState.Version,
        ActiveClusterState.ApplyFrame,
        ActiveClusterState.ActiveAuthorityNodeId);

    UE_LOG(LogRship2110, Log, TEXT("Applied cluster state epoch=%d version=%d frame=%lld authority=%s"),
        ActiveClusterState.Epoch,
        ActiveClusterState.Version,
        ActiveClusterState.ApplyFrame,
        *ActiveClusterState.ActiveAuthorityNodeId);
    return true;
}

bool URship2110Subsystem::IsClusterStateStale(const FRship2110ClusterState& ClusterState) const
{
    if (ClusterState.Epoch < ActiveClusterState.Epoch)
    {
        return true;
    }
    if (ClusterState.Epoch == ActiveClusterState.Epoch && ClusterState.Version <= ActiveClusterState.Version)
    {
        return true;
    }
    return false;
}

void URship2110Subsystem::RebuildStreamOwnershipCache()
{
    StreamOwnerNodeCache.Empty();
    for (const FRship2110ClusterNodeStreams& Assignment : ActiveClusterState.NodeStreamAssignments)
    {
        if (Assignment.NodeId.IsEmpty())
        {
            continue;
        }
        for (const FString& StreamId : Assignment.StreamIds)
        {
            if (!StreamId.IsEmpty())
            {
                StreamOwnerNodeCache.Add(StreamId, Assignment.NodeId);
            }
        }
    }
}

bool URship2110Subsystem::IsStreamOwnedByLocalNode(const FString& StreamId) const
{
    if (!ActiveClusterState.bStrictNodeOwnership)
    {
        return true;
    }
    if (LocalClusterNodeId.IsEmpty())
    {
        return false;
    }
    const FString* OwnerNode = StreamOwnerNodeCache.Find(StreamId);
    if (!OwnerNode)
    {
        return false;
    }
    return *OwnerNode == LocalClusterNodeId;
}

void URship2110Subsystem::HandleAuthoritativeRshipInbound(const FString& Payload, int64 SuggestedApplyFrame)
{
    const FString PayloadSyncDomainId = ResolveSyncDomainId(ExtractSyncDomainIdFromPayload(Payload));
    const int64 DomainApplyFrame = PayloadSyncDomainId.Equals(DefaultSyncDomainId, ESearchCase::IgnoreCase)
        ? SuggestedApplyFrame
        : INDEX_NONE;
    SubmitAuthorityClusterDataMessageForDomain(Payload, PayloadSyncDomainId, DomainApplyFrame);
}

FString URship2110Subsystem::GetFailoverCandidateNodeId(const FRship2110ClusterState& ClusterState) const
{
    for (const FString& NodeId : ClusterState.FailoverPriority)
    {
        if (!NodeId.IsEmpty())
        {
            return NodeId;
        }
    }

    TSet<FString> UniqueNodes;
    if (!ClusterState.ActiveAuthorityNodeId.IsEmpty())
    {
        UniqueNodes.Add(ClusterState.ActiveAuthorityNodeId);
    }
    for (const FRship2110ClusterNodeStreams& Assignment : ClusterState.NodeStreamAssignments)
    {
        if (!Assignment.NodeId.IsEmpty())
        {
            UniqueNodes.Add(Assignment.NodeId);
        }
    }

    TArray<FString> Nodes = UniqueNodes.Array();
    Nodes.Sort();
    if (Nodes.Num() > 0)
    {
        return Nodes[0];
    }

    return LocalClusterNodeId;
}

void URship2110Subsystem::EvaluateClusterFailover()
{
    if (!ActiveClusterState.bFailoverEnabled || !ActiveClusterState.bAllowAutoPromotion)
    {
        return;
    }
    if (IsLocalNodeAuthority())
    {
        return;
    }
    if (ActiveClusterState.ActiveAuthorityNodeId.IsEmpty())
    {
        return;
    }

    const double TimeoutSeconds = FMath::Max(0.1, static_cast<double>(ActiveClusterState.FailoverTimeoutSeconds));
    const double Now = FPlatformTime::Seconds();
    if ((Now - LastAuthorityHeartbeatTime) < TimeoutSeconds)
    {
        return;
    }

    const FString CandidateNode = GetFailoverCandidateNodeId(ActiveClusterState);
    if (CandidateNode != LocalClusterNodeId)
    {
        return;
    }

    UE_LOG(LogRship2110, Warning, TEXT("Authority heartbeat timeout. Promoting local node %s"), *LocalClusterNodeId);
    PromoteLocalNodeToPrimary(true);
    LastAuthorityHeartbeatTime = Now;
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
