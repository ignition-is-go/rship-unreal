// Copyright Rocketship. All Rights Reserved.

#include "Rivermax/RivermaxManager.h"
#include "Rivermax/Rship2110VideoSender.h"
#include "Rship2110Subsystem.h"
#include "Rship2110Settings.h"
#include "Rship2110.h"

#if RSHIP_RIVERMAX_AVAILABLE
// Rivermax SDK 1.8+ headers (modern rmx_* API)
#include "rivermax_api.h"
#include "rivermax_defs.h"

// Platform includes for network interface enumeration
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "iphlpapi.lib")
#endif
#endif

bool URivermaxManager::Initialize(URship2110Subsystem* InSubsystem)
{
    if (InSubsystem == nullptr)
    {
        UE_LOG(LogRship2110, Error, TEXT("RivermaxManager: Invalid subsystem"));
        return false;
    }

    Subsystem = InSubsystem;

    // Initialize SDK
    if (!InitializeSDK())
    {
        UE_LOG(LogRship2110, Warning, TEXT("RivermaxManager: SDK initialization failed"));
        // Continue anyway - we can still enumerate devices
    }

    // Enumerate devices
    EnumerateDevices();

    // Try to select device from settings
    URship2110Settings* Settings = URship2110Settings::Get();
    if (Settings && !Settings->RivermaxInterfaceIP.IsEmpty())
    {
        SelectDeviceByIP(Settings->RivermaxInterfaceIP);
    }
    else if (Devices.Num() > 0)
    {
        SelectDevice(0);  // Select first device
    }

    // Configure GPUDirect
    if (Settings)
    {
        SetGPUDirectEnabled(Settings->bEnableGPUDirect);
    }

    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Initialized with %d devices"),
           Devices.Num());

    OnInitialized.Broadcast(bIsInitialized);

    return true;
}

void URivermaxManager::Shutdown()
{
    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Shutting down..."));

    // Destroy all streams
    TArray<FString> StreamIds;
    VideoSenders.GetKeys(StreamIds);
    for (const FString& StreamId : StreamIds)
    {
        DestroyStream(StreamId);
    }

    // Free any allocated memory
    for (auto& Pair : AllocatedMemory)
    {
        FMemory::Free(Pair.Key);
    }
    AllocatedMemory.Empty();
    TotalAllocatedBytes = 0;

    // Shutdown SDK
    ShutdownSDK();

    Devices.Empty();
    SelectedDeviceIndex = -1;
    Subsystem = nullptr;

    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Shutdown complete"));
}

void URivermaxManager::Tick(float DeltaTime)
{
    // Tick all active senders
    for (auto& Pair : VideoSenders)
    {
        if (Pair.Value)
        {
            Pair.Value->Tick();
        }
    }
}

int32 URivermaxManager::EnumerateDevices()
{
    Devices.Empty();

#if RSHIP_RIVERMAX_AVAILABLE
    // SDK 1.8+ uses rmx_* API - device enumeration happens via platform network APIs
    // The actual Rivermax device selection happens when creating streams
    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Using platform device enumeration for Rivermax 1.8+"));
#endif

    // Enumerate network interfaces using platform APIs
    // This works for both Rivermax mode and stub mode
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (SocketSubsystem)
    {
        TArray<TSharedPtr<FInternetAddr>> Addrs;
        if (SocketSubsystem->GetLocalAdapterAddresses(Addrs))
        {
            int32 DeviceIdx = 0;
            for (const TSharedPtr<FInternetAddr>& Addr : Addrs)
            {
                if (Addr.IsValid() && Addr->IsValid())
                {
                    FString IPStr = Addr->ToString(false);
                    // Skip loopback and link-local addresses
                    if (!IPStr.StartsWith(TEXT("127.")) && !IPStr.StartsWith(TEXT("169.254.")))
                    {
                        FRshipRivermaxDevice Device;
                        Device.DeviceIndex = DeviceIdx++;
                        Device.Name = FString::Printf(TEXT("Network Adapter %d"), Device.DeviceIndex);
                        Device.IPAddress = IPStr;
                        Device.MACAddress = TEXT("00:00:00:00:00:00");  // MAC not available from socket API
                        Device.bSupportsGPUDirect = false;  // Will be determined when stream is created
                        Device.bSupportsPTPHardware = false;
                        Device.MaxBandwidthGbps = 10.0f;  // Assume 10GbE

                        Devices.Add(Device);
                    }
                }
            }
        }
    }

    // If no devices found, add a placeholder
    if (Devices.Num() == 0)
    {
        FRshipRivermaxDevice StubDevice;
        StubDevice.DeviceIndex = 0;
        StubDevice.Name = TEXT("Network Adapter (Default)");
        StubDevice.IPAddress = TEXT("0.0.0.0");
        StubDevice.MACAddress = TEXT("00:00:00:00:00:00");
        StubDevice.bSupportsGPUDirect = false;
        StubDevice.bSupportsPTPHardware = false;
        StubDevice.MaxBandwidthGbps = 1.0f;
        Devices.Add(StubDevice);
    }

    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Enumerated %d devices"), Devices.Num());

    OnDevicesEnumerated.Broadcast(Devices.Num());

    return Devices.Num();
}

bool URivermaxManager::GetDevice(int32 Index, FRshipRivermaxDevice& OutDevice) const
{
    if (Index >= 0 && Index < Devices.Num())
    {
        OutDevice = Devices[Index];
        return true;
    }
    return false;
}

bool URivermaxManager::SelectDevice(int32 Index)
{
    if (Index < 0 || Index >= Devices.Num())
    {
        UE_LOG(LogRship2110, Warning, TEXT("RivermaxManager: Invalid device index %d"), Index);
        return false;
    }

    if (SelectedDeviceIndex == Index)
    {
        return true;  // Already selected
    }

    // Destroy any active streams on the old device
    if (SelectedDeviceIndex >= 0 && VideoSenders.Num() > 0)
    {
        UE_LOG(LogRship2110, Warning, TEXT("RivermaxManager: Changing device with active streams - streams will be destroyed"));
        TArray<FString> StreamIds;
        VideoSenders.GetKeys(StreamIds);
        for (const FString& StreamId : StreamIds)
        {
            DestroyStream(StreamId);
        }
    }

    // Mark old device as inactive
    if (SelectedDeviceIndex >= 0 && SelectedDeviceIndex < Devices.Num())
    {
        Devices[SelectedDeviceIndex].bIsActive = false;
    }

    SelectedDeviceIndex = Index;
    Devices[SelectedDeviceIndex].bIsActive = true;

    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Selected device %d (%s)"),
           Index, *Devices[Index].Name);

    OnDeviceChanged.Broadcast(Index, Devices[Index]);

    return true;
}

bool URivermaxManager::SelectDeviceByIP(const FString& IPAddress)
{
    for (int32 i = 0; i < Devices.Num(); i++)
    {
        if (Devices[i].IPAddress == IPAddress)
        {
            return SelectDevice(i);
        }
    }

    UE_LOG(LogRship2110, Warning, TEXT("RivermaxManager: Device with IP %s not found"), *IPAddress);
    return false;
}

bool URivermaxManager::GetSelectedDevice(FRshipRivermaxDevice& OutDevice) const
{
    return GetDevice(SelectedDeviceIndex, OutDevice);
}

bool URivermaxManager::IsAvailable() const
{
#if RSHIP_RIVERMAX_AVAILABLE
    return true;
#else
    return false;
#endif
}

FString URivermaxManager::GetSDKVersion() const
{
#if RSHIP_RIVERMAX_AVAILABLE
    // SDK 1.8+ uses rmx_get_version_numbers() which returns a const rmx_version*
    const rmx_version* version = rmx_get_version_numbers();
    if (version)
    {
        return FString::Printf(TEXT("%d.%d.%d"), version->major, version->minor, version->patch);
    }
#endif
    return TEXT("Not Available");
}

FRshipRivermaxStatus URivermaxManager::GetStatus() const
{
    FRshipRivermaxStatus Status;
    Status.bIsInitialized = bIsInitialized;
    Status.SDKVersion = GetSDKVersion();
    Status.Devices = Devices;
    Status.ActiveDeviceIndex = SelectedDeviceIndex;
    Status.ActiveStreamCount = ActiveStreamCount;
    Status.LastError = LastError;
    return Status;
}

URship2110VideoSender* URivermaxManager::CreateVideoSender(
    const FRship2110VideoFormat& VideoFormat,
    const FRship2110TransportParams& TransportParams,
    FString& OutStreamId)
{
    if (SelectedDeviceIndex < 0)
    {
        UE_LOG(LogRship2110, Error, TEXT("RivermaxManager: No device selected"));
        LastError = TEXT("No device selected");
        return nullptr;
    }

    URship2110Settings* Settings = URship2110Settings::Get();
    if (Settings && ActiveStreamCount >= Settings->MaxConcurrentStreams)
    {
        UE_LOG(LogRship2110, Error, TEXT("RivermaxManager: Max concurrent streams reached (%d)"),
               Settings->MaxConcurrentStreams);
        LastError = TEXT("Maximum concurrent streams reached");
        return nullptr;
    }

    // Generate stream ID
    OutStreamId = GenerateStreamId();

    // Create video sender
    URship2110VideoSender* Sender = NewObject<URship2110VideoSender>(this);

    // Set up transport params with source IP if not specified
    FRship2110TransportParams FinalTransportParams = TransportParams;
    if (FinalTransportParams.SourceIP.IsEmpty())
    {
        FinalTransportParams.SourceIP = Devices[SelectedDeviceIndex].IPAddress;
    }

    // Initialize sender
    URshipPTPService* PTPService = Subsystem ? Subsystem->GetPTPService() : nullptr;
    if (!Sender->Initialize(this, PTPService, VideoFormat, FinalTransportParams))
    {
        UE_LOG(LogRship2110, Error, TEXT("RivermaxManager: Failed to initialize video sender"));
        LastError = TEXT("Failed to initialize video sender");
        return nullptr;
    }

    Sender->SetStreamId(OutStreamId);
    VideoSenders.Add(OutStreamId, Sender);
    ActiveStreamCount++;

    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Created video sender %s (%dx%d @ %.2f fps)"),
           *OutStreamId, VideoFormat.Width, VideoFormat.Height, VideoFormat.GetFrameRateDecimal());

    return Sender;
}

bool URivermaxManager::DestroyStream(const FString& StreamId)
{
    URship2110VideoSender** SenderPtr = VideoSenders.Find(StreamId);
    if (!SenderPtr || !*SenderPtr)
    {
        UE_LOG(LogRship2110, Warning, TEXT("RivermaxManager: Stream %s not found"), *StreamId);
        return false;
    }

    (*SenderPtr)->Shutdown();
    VideoSenders.Remove(StreamId);
    ActiveStreamCount--;

    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Destroyed stream %s"), *StreamId);
    return true;
}

URship2110VideoSender* URivermaxManager::GetVideoSender(const FString& StreamId)
{
    URship2110VideoSender** SenderPtr = VideoSenders.Find(StreamId);
    return SenderPtr ? *SenderPtr : nullptr;
}

TArray<FString> URivermaxManager::GetActiveStreamIds() const
{
    TArray<FString> Ids;
    VideoSenders.GetKeys(Ids);
    return Ids;
}

bool URivermaxManager::IsGPUDirectAvailable() const
{
#if RSHIP_GPUDIRECT_AVAILABLE
    if (SelectedDeviceIndex >= 0 && SelectedDeviceIndex < Devices.Num())
    {
        return Devices[SelectedDeviceIndex].bSupportsGPUDirect;
    }
#endif
    return false;
}

bool URivermaxManager::SetGPUDirectEnabled(bool bEnable)
{
    if (bEnable && !IsGPUDirectAvailable())
    {
        UE_LOG(LogRship2110, Warning, TEXT("RivermaxManager: GPUDirect not available on selected device"));
        return false;
    }

    bGPUDirectEnabled = bEnable;
    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: GPUDirect %s"),
           bEnable ? TEXT("enabled") : TEXT("disabled"));
    return true;
}

void* URivermaxManager::AllocateStreamMemory(size_t SizeBytes, size_t Alignment)
{
    // SDK 1.8+ memory management is different - memory blocks are assigned to streams
    // via rmx_output_media_assign_mem_blocks() after standard/CUDA allocation
    // For GPUDirect, CUDA memory allocation should be used and then assigned to the stream

    // Use UE's aligned allocation
    void* Ptr = FMemory::Malloc(SizeBytes, Alignment);
    if (Ptr)
    {
        AllocatedMemory.Add(Ptr, SizeBytes);
        TotalAllocatedBytes += SizeBytes;
    }
    return Ptr;
}

void URivermaxManager::FreeStreamMemory(void* Ptr)
{
    if (!Ptr)
    {
        return;
    }

    size_t* SizePtr = AllocatedMemory.Find(Ptr);
    if (SizePtr)
    {
        TotalAllocatedBytes -= *SizePtr;
        AllocatedMemory.Remove(Ptr);
    }

    // SDK 1.8+ uses standard memory management
    // For GPUDirect, CUDA memory should be freed via CUDA APIs
    FMemory::Free(Ptr);
}

bool URivermaxManager::InitializeSDK()
{
#if RSHIP_RIVERMAX_AVAILABLE
    // SDK 1.8+ uses rmx_init()
    rmx_status status = rmx_init();
    if (status != RMX_OK)
    {
        UE_LOG(LogRship2110, Error, TEXT("RivermaxManager: rmx_init failed: %d"), static_cast<int>(status));
        LastError = FString::Printf(TEXT("Rivermax init failed: %d"), static_cast<int>(status));
        return false;
    }

    bIsInitialized = true;
    SDKVersion = GetSDKVersion();
    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: SDK initialized, version %s"), *SDKVersion);
    return true;
#else
    // Stub mode - consider initialized
    bIsInitialized = true;
    SDKVersion = TEXT("Stub");
    UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: Running in stub mode (no SDK)"));
    return true;
#endif
}

void URivermaxManager::ShutdownSDK()
{
#if RSHIP_RIVERMAX_AVAILABLE
    if (bIsInitialized)
    {
        rmx_cleanup();
        UE_LOG(LogRship2110, Log, TEXT("RivermaxManager: SDK cleaned up"));
    }
#endif
    bIsInitialized = false;
}

bool URivermaxManager::QueryDeviceCapabilities(int32 DeviceIndex, FRshipRivermaxDevice& OutDevice)
{
#if RSHIP_RIVERMAX_AVAILABLE
    // Query additional capabilities from Rivermax
    // This would include things like RDMA support, max streams, etc.
#endif
    return true;
}

FString URivermaxManager::GenerateStreamId()
{
    StreamIdCounter++;
    return FString::Printf(TEXT("stream_%d_%d"), StreamIdCounter, FMath::Rand());
}
