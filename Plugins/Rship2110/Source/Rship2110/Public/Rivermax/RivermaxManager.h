// Copyright Rocketship. All Rights Reserved.
// Rivermax SDK Manager
//
// Manages initialization, device enumeration, and lifecycle of the
// NVIDIA Rivermax SDK for SMPTE 2110 streaming.
//
// Key responsibilities:
// - Initialize/shutdown Rivermax SDK
// - Enumerate and select ConnectX NICs
// - Create and manage stream contexts
// - Coordinate GPUDirect RDMA setup

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Rship2110Types.h"
#include "RivermaxManager.generated.h"

class URship2110Subsystem;
class URship2110VideoSender;

// Forward declarations for Rivermax types (SDK 1.8+)
#if RSHIP_RIVERMAX_AVAILABLE
// SDK 1.8+ uses rmx_* types - no forward declarations needed here
// Stream handles are opaque void* in the new API
#endif

/**
 * Rivermax device event delegate
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRivermaxInitialized, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRivermaxDevicesEnumerated, int32, DeviceCount);

/**
 * Rivermax SDK Manager.
 *
 * Handles all low-level Rivermax SDK operations including device
 * management, memory allocation, and stream coordination.
 */
UCLASS(BlueprintType)
class RSHIP2110_API URivermaxManager : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initialize Rivermax SDK.
     * @param InSubsystem Parent subsystem
     * @return true if initialization succeeded
     */
    bool Initialize(URship2110Subsystem* InSubsystem);

    /**
     * Shutdown and release all resources.
     */
    void Shutdown();

    /**
     * Tick update for maintenance tasks.
     * @param DeltaTime Time since last tick
     */
    void Tick(float DeltaTime);

    // ========================================================================
    // DEVICE MANAGEMENT
    // ========================================================================

    /**
     * Enumerate available Rivermax-capable devices (NICs).
     * @return Number of devices found
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    int32 EnumerateDevices();

    /**
     * Get list of available devices.
     * @return Array of device information
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    TArray<FRshipRivermaxDevice> GetDevices() const { return Devices; }

    /**
     * Get device by index.
     * @param Index Device index
     * @param OutDevice Device information
     * @return true if device exists
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool GetDevice(int32 Index, FRshipRivermaxDevice& OutDevice) const;

    /**
     * Select device by index.
     * @param Index Device index to select
     * @return true if selection succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool SelectDevice(int32 Index);

    /**
     * Select device by IP address.
     * @param IPAddress IP address of device
     * @return true if device found and selected
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool SelectDeviceByIP(const FString& IPAddress);

    /**
     * Get currently selected device.
     * @param OutDevice Device information
     * @return true if a device is selected
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool GetSelectedDevice(FRshipRivermaxDevice& OutDevice) const;

    /**
     * Get index of selected device.
     * @return Device index or -1 if none selected
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    int32 GetSelectedDeviceIndex() const { return SelectedDeviceIndex; }

    // ========================================================================
    // STATUS
    // ========================================================================

    /**
     * Check if Rivermax SDK is initialized.
     * @return true if initialized
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool IsInitialized() const { return bIsInitialized; }

    /**
     * Check if Rivermax SDK is available (compiled in).
     * @return true if SDK is available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool IsAvailable() const;

    /**
     * Get SDK version string.
     * @return Version string or "Not Available"
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    FString GetSDKVersion() const;

    /**
     * Get full status.
     * @return Status structure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    FRshipRivermaxStatus GetStatus() const;

    /**
     * Get number of active streams.
     * @return Stream count
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    int32 GetActiveStreamCount() const { return ActiveStreamCount; }

    /**
     * Get last error message.
     * @return Error string
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    FString GetLastError() const { return LastError; }

    // ========================================================================
    // STREAM MANAGEMENT
    // ========================================================================

    /**
     * Create a video sender stream.
     * @param VideoFormat Video format specification
     * @param TransportParams Transport parameters
     * @param OutStreamId Unique stream identifier
     * @return Created video sender or nullptr on failure
     */
    URship2110VideoSender* CreateVideoSender(
        const FRship2110VideoFormat& VideoFormat,
        const FRship2110TransportParams& TransportParams,
        FString& OutStreamId);

    /**
     * Destroy a stream by ID.
     * @param StreamId Stream identifier
     * @return true if stream was found and destroyed
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool DestroyStream(const FString& StreamId);

    /**
     * Get a video sender by stream ID.
     * @param StreamId Stream identifier
     * @return Video sender or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    URship2110VideoSender* GetVideoSender(const FString& StreamId);

    /**
     * Get all active stream IDs.
     * @return Array of stream identifiers
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    TArray<FString> GetActiveStreamIds() const;

    // ========================================================================
    // GPUDIRECT
    // ========================================================================

    /**
     * Check if GPUDirect RDMA is available.
     * @return true if GPUDirect can be used
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool IsGPUDirectAvailable() const;

    /**
     * Check if GPUDirect is currently enabled.
     * @return true if enabled
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool IsGPUDirectEnabled() const { return bGPUDirectEnabled; }

    /**
     * Enable or disable GPUDirect RDMA.
     * @param bEnable true to enable
     * @return true if setting was applied
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Rivermax")
    bool SetGPUDirectEnabled(bool bEnable);

    // ========================================================================
    // MEMORY MANAGEMENT
    // ========================================================================

    /**
     * Allocate memory for Rivermax operations.
     * Uses appropriate allocation method (GPUDirect or system memory).
     * @param SizeBytes Size in bytes
     * @param Alignment Memory alignment
     * @return Pointer to allocated memory or nullptr
     */
    void* AllocateStreamMemory(size_t SizeBytes, size_t Alignment = 4096);

    /**
     * Free Rivermax-allocated memory.
     * @param Ptr Memory to free
     */
    void FreeStreamMemory(void* Ptr);

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when Rivermax initialization completes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Rivermax")
    FOnRivermaxInitialized OnInitialized;

    /** Fired when device enumeration completes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Rivermax")
    FOnRivermaxDevicesEnumerated OnDevicesEnumerated;

    /** Fired when active device changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Rivermax")
    FOnRivermaxDeviceChanged OnDeviceChanged;

private:
    UPROPERTY()
    URship2110Subsystem* Subsystem = nullptr;

    // SDK state
    bool bIsInitialized = false;
    bool bSDKLoaded = false;  // True only if Rivermax DLLs actually loaded
    bool bGPUDirectAvailable = false;
    bool bGPUDirectEnabled = false;
    FString SDKVersion;
    FString LastError;

    // Device management
    TArray<FRshipRivermaxDevice> Devices;
    int32 SelectedDeviceIndex = -1;

    // Stream management
    UPROPERTY()
    TMap<FString, URship2110VideoSender*> VideoSenders;
    int32 ActiveStreamCount = 0;
    int32 StreamIdCounter = 0;

    // Memory tracking
    TMap<void*, size_t> AllocatedMemory;
    size_t TotalAllocatedBytes = 0;

    // Internal methods
    bool InitializeSDK();
    void ShutdownSDK();
    bool QueryDeviceCapabilities(int32 DeviceIndex, FRshipRivermaxDevice& OutDevice);
    FString GenerateStreamId();

#if RSHIP_RIVERMAX_AVAILABLE
    // Rivermax-specific internal state
    void* RivermaxContext = nullptr;
#endif
};
