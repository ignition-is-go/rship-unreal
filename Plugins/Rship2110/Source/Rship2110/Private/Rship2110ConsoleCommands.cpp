// Copyright Rocketship. All Rights Reserved.
// Console commands for debugging and testing SMPTE 2110 functionality

#include "Rship2110.h"
#include "Rship2110Subsystem.h"
#include "Rship2110Settings.h"
#include "PTP/RshipPTPService.h"
#include "Rivermax/RivermaxManager.h"
#include "Rivermax/Rship2110VideoSender.h"
#include "IPMX/RshipIPMXService.h"
#include "HAL/IConsoleManager.h"

// ============================================================================
// PTP COMMANDS
// ============================================================================

static FAutoConsoleCommand Rship2110PTPStatusCmd(
    TEXT("rship.ptp.status"),
    TEXT("Display PTP synchronization status"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem || !Subsystem->GetPTPService())
        {
            UE_LOG(LogRship2110, Log, TEXT("PTP service not available"));
            return;
        }

        FRshipPTPStatus Status = Subsystem->GetPTPStatus();

        FString StateStr;
        switch (Status.State)
        {
            case ERshipPTPState::Disabled: StateStr = TEXT("Disabled"); break;
            case ERshipPTPState::Listening: StateStr = TEXT("Listening"); break;
            case ERshipPTPState::Acquiring: StateStr = TEXT("Acquiring"); break;
            case ERshipPTPState::Locked: StateStr = TEXT("Locked"); break;
            case ERshipPTPState::Holdover: StateStr = TEXT("Holdover"); break;
            case ERshipPTPState::Error: StateStr = TEXT("Error"); break;
        }

        UE_LOG(LogRship2110, Log, TEXT("=== PTP Status ==="));
        UE_LOG(LogRship2110, Log, TEXT("State: %s"), *StateStr);
        UE_LOG(LogRship2110, Log, TEXT("PTP Time: %lld.%09d"), Status.CurrentTime.Seconds, Status.CurrentTime.Nanoseconds);
        UE_LOG(LogRship2110, Log, TEXT("Offset from System: %lld ns"), Status.OffsetFromSystemNs);
        UE_LOG(LogRship2110, Log, TEXT("Path Delay: %lld ns"), Status.PathDelayNs);
        UE_LOG(LogRship2110, Log, TEXT("Drift: %.3f ppb"), Status.DriftPPB);
        UE_LOG(LogRship2110, Log, TEXT("Jitter: %.3f ns"), Status.JitterNs);
        UE_LOG(LogRship2110, Log, TEXT("Grandmaster: %s (Domain %d)"),
               *Status.Grandmaster.ClockIdentity, Status.Grandmaster.Domain);
    }));

static FAutoConsoleCommand Rship2110PTPForceResyncCmd(
    TEXT("rship.ptp.resync"),
    TEXT("Force PTP resynchronization"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (Subsystem && Subsystem->GetPTPService())
        {
            Subsystem->GetPTPService()->ForceResync();
            UE_LOG(LogRship2110, Log, TEXT("PTP resync initiated"));
        }
    }));

// ============================================================================
// RIVERMAX COMMANDS
// ============================================================================

static FAutoConsoleCommand Rship2110RivermaxStatusCmd(
    TEXT("rship.rivermax.status"),
    TEXT("Display Rivermax status and device information"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem || !Subsystem->GetRivermaxManager())
        {
            UE_LOG(LogRship2110, Log, TEXT("Rivermax manager not available"));
            return;
        }

        FRshipRivermaxStatus Status = Subsystem->GetRivermaxStatus();

        UE_LOG(LogRship2110, Log, TEXT("=== Rivermax Status ==="));
        UE_LOG(LogRship2110, Log, TEXT("Initialized: %s"), Status.bIsInitialized ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogRship2110, Log, TEXT("SDK Version: %s"), *Status.SDKVersion);
        UE_LOG(LogRship2110, Log, TEXT("Active Device: %d"), Status.ActiveDeviceIndex);
        UE_LOG(LogRship2110, Log, TEXT("Active Streams: %d"), Status.ActiveStreamCount);

        UE_LOG(LogRship2110, Log, TEXT("--- Devices ---"));
        for (int32 i = 0; i < Status.Devices.Num(); i++)
        {
            const FRshipRivermaxDevice& Device = Status.Devices[i];
            UE_LOG(LogRship2110, Log, TEXT("  [%d] %s (%s) %s"),
                   i, *Device.Name, *Device.IPAddress,
                   Device.bIsActive ? TEXT("[ACTIVE]") : TEXT(""));
            UE_LOG(LogRship2110, Log, TEXT("      MAC: %s, GPUDirect: %s, PTP HW: %s"),
                   *Device.MACAddress,
                   Device.bSupportsGPUDirect ? TEXT("Yes") : TEXT("No"),
                   Device.bSupportsPTPHardware ? TEXT("Yes") : TEXT("No"));
        }
    }));

static FAutoConsoleCommand Rship2110RivermaxEnumerateCmd(
    TEXT("rship.rivermax.enumerate"),
    TEXT("Re-enumerate Rivermax devices"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (Subsystem && Subsystem->GetRivermaxManager())
        {
            int32 Count = Subsystem->GetRivermaxManager()->EnumerateDevices();
            UE_LOG(LogRship2110, Log, TEXT("Found %d Rivermax devices"), Count);
        }
    }));

static FAutoConsoleCommand Rship2110RivermaxSelectCmd(
    TEXT("rship.rivermax.select"),
    TEXT("Select Rivermax device by index - Usage: rship.rivermax.select <index>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.rivermax.select <index>"));
            return;
        }

        int32 Index = FCString::Atoi(*Args[0]);
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (Subsystem && Subsystem->GetRivermaxManager())
        {
            if (Subsystem->GetRivermaxManager()->SelectDevice(Index))
            {
                UE_LOG(LogRship2110, Log, TEXT("Selected device %d"), Index);
            }
            else
            {
                UE_LOG(LogRship2110, Warning, TEXT("Failed to select device %d"), Index);
            }
        }
    }));

// ============================================================================
// STREAM COMMANDS
// ============================================================================

static FAutoConsoleCommand Rship2110StreamListCmd(
    TEXT("rship.stream.list"),
    TEXT("List all active streams"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            UE_LOG(LogRship2110, Log, TEXT("2110 subsystem not available"));
            return;
        }

        TArray<FString> StreamIds = Subsystem->GetActiveStreamIds();

        UE_LOG(LogRship2110, Log, TEXT("=== Active Streams (%d) ==="), StreamIds.Num());
        for (const FString& StreamId : StreamIds)
        {
            URship2110VideoSender* Sender = Subsystem->GetVideoSender(StreamId);
            if (Sender)
            {
                FRship2110VideoFormat Format = Sender->GetVideoFormat();
                FRship2110StreamStats Stats = Sender->GetStatistics();

                FString StateStr;
                switch (Sender->GetState())
                {
                    case ERship2110StreamState::Stopped: StateStr = TEXT("Stopped"); break;
                    case ERship2110StreamState::Starting: StateStr = TEXT("Starting"); break;
                    case ERship2110StreamState::Running: StateStr = TEXT("Running"); break;
                    case ERship2110StreamState::Paused: StateStr = TEXT("Paused"); break;
                    case ERship2110StreamState::Error: StateStr = TEXT("Error"); break;
                }

                UE_LOG(LogRship2110, Log, TEXT("  [%s] %s"), *StreamId, *StateStr);
                UE_LOG(LogRship2110, Log, TEXT("    Format: %dx%d @ %.2f fps"),
                       Format.Width, Format.Height, Format.GetFrameRateDecimal());
                UE_LOG(LogRship2110, Log, TEXT("    Frames: %lld sent, %lld dropped, %lld late"),
                       Stats.FramesSent, Stats.FramesDropped, Stats.LateFrames);
                UE_LOG(LogRship2110, Log, TEXT("    Bitrate: %.2f Mbps"), Sender->GetBitrateMbps());
            }
        }
    }));

static FAutoConsoleCommand Rship2110StreamStartTestCmd(
    TEXT("rship.stream.starttest"),
    TEXT("Start a test 1080p60 stream to 239.0.0.1:5004"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            UE_LOG(LogRship2110, Log, TEXT("2110 subsystem not available"));
            return;
        }

        FRship2110VideoFormat Format;
        Format.Width = 1920;
        Format.Height = 1080;
        Format.FrameRateNumerator = 60;
        Format.FrameRateDenominator = 1;

        FRship2110TransportParams Transport;
        Transport.DestinationIP = TEXT("239.0.0.1");
        Transport.DestinationPort = 5004;

        FString StreamId = Subsystem->CreateVideoStream(Format, Transport, true);
        if (!StreamId.IsEmpty())
        {
            Subsystem->StartStream(StreamId);
            UE_LOG(LogRship2110, Log, TEXT("Started test stream: %s"), *StreamId);
        }
        else
        {
            UE_LOG(LogRship2110, Error, TEXT("Failed to create test stream"));
        }
    }));

static FAutoConsoleCommand Rship2110StreamStopCmd(
    TEXT("rship.stream.stop"),
    TEXT("Stop a stream - Usage: rship.stream.stop <stream_id>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.stream.stop <stream_id>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (Subsystem)
        {
            if (Subsystem->StopStream(Args[0]))
            {
                UE_LOG(LogRship2110, Log, TEXT("Stopped stream: %s"), *Args[0]);
            }
            else
            {
                UE_LOG(LogRship2110, Warning, TEXT("Failed to stop stream: %s"), *Args[0]);
            }
        }
    }));

// ============================================================================
// CLUSTER COMMANDS
// ============================================================================

static FAutoConsoleCommand Rship2110ClusterStatusCmd(
    TEXT("rship.cluster.status"),
    TEXT("Display cluster control state and local ownership"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            UE_LOG(LogRship2110, Log, TEXT("2110 subsystem not available"));
            return;
        }

        const FRship2110ClusterState State = Subsystem->GetClusterState();
        const FString LocalNodeId = Subsystem->GetLocalClusterNodeId();
        const FString Role = Subsystem->IsLocalNodeAuthority() ? TEXT("Primary") : TEXT("Secondary");
        const FString ActiveSyncDomain = Subsystem->GetActiveSyncDomainId();

        UE_LOG(LogRship2110, Log, TEXT("=== Cluster Status ==="));
        UE_LOG(LogRship2110, Log, TEXT("Local Node: %s"), *LocalNodeId);
        UE_LOG(LogRship2110, Log, TEXT("Role: %s"), *Role);
        UE_LOG(LogRship2110, Log, TEXT("Frame: %lld"), Subsystem->GetClusterFrameCounter());
        UE_LOG(LogRship2110, Log, TEXT("Active Sync Domain: %s"), *ActiveSyncDomain);
        UE_LOG(LogRship2110, Log, TEXT("Default Sync Rate: %.2f Hz"), Subsystem->GetClusterSyncRateHz());
        UE_LOG(LogRship2110, Log, TEXT("Local Render Substeps: %d"), Subsystem->GetLocalRenderSubsteps());
        UE_LOG(LogRship2110, Log, TEXT("Max Catch-up Steps: %d"), Subsystem->GetMaxSyncCatchupSteps());
        UE_LOG(LogRship2110, Log, TEXT("Epoch/Version: %d/%d"), State.Epoch, State.Version);
        UE_LOG(LogRship2110, Log, TEXT("Authority: %s"), *State.ActiveAuthorityNodeId);
        UE_LOG(LogRship2110, Log, TEXT("Strict Ownership: %s"), State.bStrictNodeOwnership ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogRship2110, Log, TEXT("Failover: %s (timeout %.2fs)"),
            State.bFailoverEnabled ? TEXT("Enabled") : TEXT("Disabled"),
            State.FailoverTimeoutSeconds);

        const TArray<FString> SyncDomains = Subsystem->GetSyncDomainIds();
        UE_LOG(LogRship2110, Log, TEXT("Sync Domains (%d):"), SyncDomains.Num());
        for (const FString& DomainId : SyncDomains)
        {
            UE_LOG(LogRship2110, Log, TEXT("  %s frame=%lld rate=%.2f"),
                *DomainId,
                Subsystem->GetClusterFrameCounterForDomain(DomainId),
                Subsystem->GetSyncDomainRateHz(DomainId));
        }

        const TArray<FString> OwnedStreams = Subsystem->GetLocallyOwnedStreams();
        UE_LOG(LogRship2110, Log, TEXT("Owned Streams (%d):"), OwnedStreams.Num());
        for (const FString& StreamId : OwnedStreams)
        {
            UE_LOG(LogRship2110, Log, TEXT("  %s"), *StreamId);
        }
    }));

static FAutoConsoleCommand Rship2110ClusterSetNodeCmd(
    TEXT("rship.cluster.node"),
    TEXT("Set local cluster node id - Usage: rship.cluster.node <node_id>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.node <node_id>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        Subsystem->SetLocalClusterNodeId(Args[0]);
        UE_LOG(LogRship2110, Log, TEXT("Local cluster node id updated to %s"), *Subsystem->GetLocalClusterNodeId());
    }));

static FAutoConsoleCommand Rship2110ClusterAssignCmd(
    TEXT("rship.cluster.assign"),
    TEXT("Assign stream ownership - Usage: rship.cluster.assign <stream_id> <node_id>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 2)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.assign <stream_id> <node_id>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        Subsystem->SetClusterOwnershipForStream(Args[0], Args[1], true);
        UE_LOG(LogRship2110, Log, TEXT("Queued ownership update: %s -> %s"), *Args[0], *Args[1]);
    }));

static FAutoConsoleCommand Rship2110ClusterPromoteCmd(
    TEXT("rship.cluster.promote"),
    TEXT("Promote local node to authority on next frame"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        Subsystem->PromoteLocalNodeToPrimary(true);
        UE_LOG(LogRship2110, Warning, TEXT("Queued local authority promotion for node %s"), *Subsystem->GetLocalClusterNodeId());
    }));

static FAutoConsoleCommand Rship2110ClusterHeartbeatCmd(
    TEXT("rship.cluster.heartbeat"),
    TEXT("Record authority heartbeat - Usage: rship.cluster.heartbeat <authority_node> <epoch> <version>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 3)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.heartbeat <authority_node> <epoch> <version>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        const int32 Epoch = FCString::Atoi(*Args[1]);
        const int32 Version = FCString::Atoi(*Args[2]);
        Subsystem->NotifyClusterAuthorityHeartbeat(Args[0], Epoch, Version);
    }));

static FAutoConsoleCommand Rship2110ClusterPrepareCmd(
    TEXT("rship.cluster.prepare"),
    TEXT("Authority: create and broadcast prepare for current state with incremented version"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        FRship2110ClusterState State = Subsystem->GetClusterState();
        State.Version += 1;
        State.ApplyFrame = Subsystem->GetClusterFrameCounter() + 3;
        if (!Subsystem->SubmitAuthorityClusterStatePrepare(State, true))
        {
            UE_LOG(LogRship2110, Warning, TEXT("Prepare submit failed (node is likely not authority)"));
        }
    }));

static FAutoConsoleCommand Rship2110ClusterAckCmd(
    TEXT("rship.cluster.ack"),
    TEXT("Inject ACK - Usage: rship.cluster.ack <node> <epoch> <version> <hash>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 4)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.ack <node> <epoch> <version> <hash>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        FRship2110ClusterAckMessage Ack;
        Ack.NodeId = Args[0];
        Ack.AuthorityNodeId = Subsystem->GetClusterState().ActiveAuthorityNodeId;
        Ack.Epoch = FCString::Atoi(*Args[1]);
        Ack.Version = FCString::Atoi(*Args[2]);
        Ack.StateHash = Args[3];
        const bool bAccepted = Subsystem->ReceiveClusterStateAck(Ack);
        UE_LOG(LogRship2110, Log, TEXT("ACK %s"), bAccepted ? TEXT("accepted") : TEXT("rejected"));
    }));

static FAutoConsoleCommand Rship2110ClusterSyncRateCmd(
    TEXT("rship.cluster.timing.sync"),
    TEXT("Set default cluster sync rate in Hz - Usage: rship.cluster.timing.sync <hz>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.timing.sync <hz>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        const float SyncRateHz = FCString::Atof(*Args[0]);
        Subsystem->SetClusterSyncRateHz(SyncRateHz);
        if (IConsoleVariable* SyncRateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Rship2110.ClusterSyncRateHz")))
        {
            SyncRateCVar->Set(Subsystem->GetClusterSyncRateHz());
        }
        UE_LOG(LogRship2110, Log, TEXT("Default cluster sync rate set to %.2f Hz"), Subsystem->GetClusterSyncRateHz());
    }));

static FAutoConsoleCommand Rship2110ClusterSubstepsCmd(
    TEXT("rship.cluster.timing.substeps"),
    TEXT("Set local render substeps - Usage: rship.cluster.timing.substeps <n>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.timing.substeps <n>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        const int32 Substeps = FCString::Atoi(*Args[0]);
        Subsystem->SetLocalRenderSubsteps(Substeps);
        if (IConsoleVariable* SubstepsCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Rship2110.LocalRenderSubsteps")))
        {
            SubstepsCVar->Set(Subsystem->GetLocalRenderSubsteps());
        }
        UE_LOG(LogRship2110, Log, TEXT("Local render substeps set to %d"), Subsystem->GetLocalRenderSubsteps());
    }));

static FAutoConsoleCommand Rship2110ClusterCatchupCmd(
    TEXT("rship.cluster.timing.catchup"),
    TEXT("Set max sync catch-up steps - Usage: rship.cluster.timing.catchup <n>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.timing.catchup <n>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        const int32 CatchupSteps = FCString::Atoi(*Args[0]);
        Subsystem->SetMaxSyncCatchupSteps(CatchupSteps);
        if (IConsoleVariable* CatchupCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Rship2110.MaxSyncCatchupSteps")))
        {
            CatchupCVar->Set(Subsystem->GetMaxSyncCatchupSteps());
        }
        UE_LOG(LogRship2110, Log, TEXT("Max sync catch-up steps set to %d"), Subsystem->GetMaxSyncCatchupSteps());
    }));

static FAutoConsoleCommand Rship2110ClusterDomainActiveCmd(
    TEXT("rship.cluster.domain.active"),
    TEXT("Set active sync domain for authoritative outbound payloads - Usage: rship.cluster.domain.active <domain_id>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.domain.active <domain_id>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        Subsystem->SetActiveSyncDomainId(Args[0]);
        UE_LOG(LogRship2110, Log, TEXT("Active sync domain set to %s"), *Subsystem->GetActiveSyncDomainId());
    }));

static FAutoConsoleCommand Rship2110ClusterDomainRateCmd(
    TEXT("rship.cluster.domain.rate"),
    TEXT("Set sync rate for a specific domain - Usage: rship.cluster.domain.rate <domain_id> <hz>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 2)
        {
            UE_LOG(LogRship2110, Log, TEXT("Usage: rship.cluster.domain.rate <domain_id> <hz>"));
            return;
        }

        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem)
        {
            return;
        }

        const float DomainRateHz = FCString::Atof(*Args[1]);
        if (!Subsystem->SetSyncDomainRateHz(Args[0], DomainRateHz))
        {
            UE_LOG(LogRship2110, Warning, TEXT("Failed to set sync rate for domain %s"), *Args[0]);
            return;
        }

        UE_LOG(LogRship2110, Log, TEXT("Sync domain %s rate set to %.2f Hz"),
            *Args[0], Subsystem->GetSyncDomainRateHz(Args[0]));
    }));

// ============================================================================
// IPMX COMMANDS
// ============================================================================

static FAutoConsoleCommand Rship2110IPMXStatusCmd(
    TEXT("rship.ipmx.status"),
    TEXT("Display IPMX/NMOS connection status"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem || !Subsystem->GetIPMXService())
        {
            UE_LOG(LogRship2110, Log, TEXT("IPMX service not available"));
            return;
        }

        FRshipIPMXStatus Status = Subsystem->GetIPMXStatus();

        FString StateStr;
        switch (Status.State)
        {
            case ERshipIPMXConnectionState::Disconnected: StateStr = TEXT("Disconnected"); break;
            case ERshipIPMXConnectionState::Connecting: StateStr = TEXT("Connecting"); break;
            case ERshipIPMXConnectionState::Registered: StateStr = TEXT("Registered"); break;
            case ERshipIPMXConnectionState::Active: StateStr = TEXT("Active"); break;
            case ERshipIPMXConnectionState::Error: StateStr = TEXT("Error"); break;
        }

        UE_LOG(LogRship2110, Log, TEXT("=== IPMX Status ==="));
        UE_LOG(LogRship2110, Log, TEXT("State: %s"), *StateStr);
        UE_LOG(LogRship2110, Log, TEXT("Registry URL: %s"), *Status.RegistryUrl);
        UE_LOG(LogRship2110, Log, TEXT("Node ID: %s"), *Status.NodeId);
        UE_LOG(LogRship2110, Log, TEXT("Registered Senders: %d"), Status.RegisteredSenders);
        if (!Status.LastError.IsEmpty())
        {
            UE_LOG(LogRship2110, Log, TEXT("Last Error: %s"), *Status.LastError);
        }
    }));

static FAutoConsoleCommand Rship2110IPMXConnectCmd(
    TEXT("rship.ipmx.connect"),
    TEXT("Connect to IPMX registry - Usage: rship.ipmx.connect [registry_url]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (Subsystem)
        {
            FString Url = Args.Num() > 0 ? Args[0] : TEXT("");
            if (Subsystem->ConnectIPMX(Url))
            {
                UE_LOG(LogRship2110, Log, TEXT("IPMX connection initiated"));
            }
            else
            {
                UE_LOG(LogRship2110, Warning, TEXT("Failed to initiate IPMX connection"));
            }
        }
    }));

static FAutoConsoleCommand Rship2110IPMXDisconnectCmd(
    TEXT("rship.ipmx.disconnect"),
    TEXT("Disconnect from IPMX registry"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (Subsystem)
        {
            Subsystem->DisconnectIPMX();
            UE_LOG(LogRship2110, Log, TEXT("IPMX disconnected"));
        }
    }));

static FAutoConsoleCommand Rship2110IPMXDumpHandlesCmd(
    TEXT("rship.ipmx.dumphandles"),
    TEXT("Dump all IPMX registered resources"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        URship2110Subsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
        if (!Subsystem || !Subsystem->GetIPMXService())
        {
            return;
        }

        URshipIPMXService* IPMX = Subsystem->GetIPMXService();
        TArray<FString> SenderIds = IPMX->GetRegisteredSenderIds();

        UE_LOG(LogRship2110, Log, TEXT("=== IPMX Registered Resources ==="));
        UE_LOG(LogRship2110, Log, TEXT("Node: %s"), *IPMX->GetNodeId());
        UE_LOG(LogRship2110, Log, TEXT("--- Senders (%d) ---"), SenderIds.Num());

        for (const FString& Id : SenderIds)
        {
            FRshipNMOSSender Sender;
            if (IPMX->GetSender(Id, Sender))
            {
                UE_LOG(LogRship2110, Log, TEXT("  [%s] %s"), *Id, *Sender.Label);
                UE_LOG(LogRship2110, Log, TEXT("    Flow: %s"), *Sender.FlowId);
                UE_LOG(LogRship2110, Log, TEXT("    Active: %s"), Sender.bActive ? TEXT("Yes") : TEXT("No"));
            }
        }
    }));

// ============================================================================
// GENERAL COMMANDS
// ============================================================================

static FAutoConsoleCommand Rship2110HelpCmd(
    TEXT("rship.2110.help"),
    TEXT("Display available Rship 2110 console commands"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogRship2110, Log, TEXT("=== Rship 2110 Console Commands ==="));
        UE_LOG(LogRship2110, Log, TEXT(""));
        UE_LOG(LogRship2110, Log, TEXT("PTP Commands:"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ptp.status      - Display PTP sync status"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ptp.resync      - Force PTP resynchronization"));
        UE_LOG(LogRship2110, Log, TEXT(""));
        UE_LOG(LogRship2110, Log, TEXT("Rivermax Commands:"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.rivermax.status     - Display Rivermax status"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.rivermax.enumerate  - Re-enumerate devices"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.rivermax.select <n> - Select device by index"));
        UE_LOG(LogRship2110, Log, TEXT(""));
        UE_LOG(LogRship2110, Log, TEXT("Stream Commands:"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.stream.list      - List active streams"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.stream.starttest - Start test 1080p60 stream"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.stream.stop <id> - Stop stream by ID"));
        UE_LOG(LogRship2110, Log, TEXT(""));
        UE_LOG(LogRship2110, Log, TEXT("Cluster Commands:"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.status                         - Display cluster state"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.node <node_id>                - Set local node id"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.assign <stream_id> <node_id>  - Assign stream ownership"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.promote                        - Promote local authority"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.heartbeat <node> <e> <v>      - Record authority heartbeat"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.prepare                        - Emit prepare for current state"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.ack <node> <e> <v> <hash>     - Inject ACK"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.timing.sync <hz>              - Set default sync rate"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.timing.substeps <n>           - Set local render substeps"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.timing.catchup <n>            - Set max catch-up steps"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.domain.active <id>            - Set active sync domain"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.cluster.domain.rate <id> <hz>         - Set domain sync rate"));
        UE_LOG(LogRship2110, Log, TEXT(""));
        UE_LOG(LogRship2110, Log, TEXT("IPMX Commands:"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ipmx.status          - Display IPMX status"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ipmx.connect [url]   - Connect to registry"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ipmx.disconnect      - Disconnect from registry"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ipmx.dumphandles     - Dump registered resources"));
    }));
