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
        UE_LOG(LogRship2110, Log, TEXT("IPMX Commands:"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ipmx.status          - Display IPMX status"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ipmx.connect [url]   - Connect to registry"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ipmx.disconnect      - Disconnect from registry"));
        UE_LOG(LogRship2110, Log, TEXT("  rship.ipmx.dumphandles     - Dump registered resources"));
    }));
