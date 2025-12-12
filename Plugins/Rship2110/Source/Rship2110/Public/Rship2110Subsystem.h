// Copyright Rocketship. All Rights Reserved.
// Main Subsystem for SMPTE 2110 / PTP / IPMX Integration
//
// This is the primary entry point for 2110 streaming functionality.
// It orchestrates:
// - PTP time synchronization
// - Rivermax device management
// - Video/Audio/Ancillary stream lifecycle
// - IPMX registration and discovery
//
// Integrates with the existing RshipSubsystem for timecode sync.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tickable.h"
#include "Rship2110Types.h"
#include "Rship2110Subsystem.generated.h"

// Forward declarations
class URshipPTPService;
class URivermaxManager;
class URshipIPMXService;
class URship2110VideoSender;
class URship2110VideoCapture;
class URship2110Settings;
class URshipSubsystem;

/**
 * Main subsystem for SMPTE 2110 streaming.
 *
 * Provides a unified API for:
 * - PTP-disciplined timing
 * - Rivermax-based 2110 streaming
 * - IPMX/NMOS discovery and registration
 *
 * Automatically initializes based on project settings and integrates
 * with the existing Rship subsystem for timecode synchronization.
 */
UCLASS()
class RSHIP2110_API URship2110Subsystem : public UEngineSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // ========================================================================
    // UEngineSubsystem Interface
    // ========================================================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // ========================================================================
    // FTickableGameObject Interface
    // ========================================================================

    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return bIsInitialized; }

    // ========================================================================
    // SERVICE ACCESS
    // ========================================================================

    /**
     * Get the PTP service for time synchronization.
     * @return PTP service or nullptr if not enabled
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URshipPTPService* GetPTPService() const { return PTPService; }

    /**
     * Get the Rivermax manager for device and stream management.
     * @return Rivermax manager or nullptr if not available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URivermaxManager* GetRivermaxManager() const { return RivermaxManager; }

    /**
     * Get the IPMX service for discovery and registration.
     * @return IPMX service or nullptr if not enabled
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URshipIPMXService* GetIPMXService() const { return IPMXService; }

    /**
     * Get the video capture helper.
     * @return Video capture or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URship2110VideoCapture* GetVideoCapture() const { return VideoCapture; }

    /**
     * Get the Rship main subsystem (for timecode integration).
     * @return Rship subsystem or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URshipSubsystem* GetRshipSubsystem() const;

    // ========================================================================
    // QUICK ACCESS - PTP
    // ========================================================================

    /**
     * Get current PTP time.
     * @return PTP timestamp or zero if not locked
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|PTP")
    FRshipPTPTimestamp GetPTPTime() const;

    /**
     * Check if PTP is locked to grandmaster.
     * @return true if locked
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|PTP")
    bool IsPTPLocked() const;

    /**
     * Get PTP status.
     * @return PTP status
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|PTP")
    FRshipPTPStatus GetPTPStatus() const;

    // ========================================================================
    // QUICK ACCESS - STREAMS
    // ========================================================================

    /**
     * Create and start a video sender stream.
     * Convenience method that creates stream, registers with IPMX, and starts.
     * @param VideoFormat Video format
     * @param TransportParams Transport parameters
     * @param bAutoRegisterIPMX Auto-register with IPMX
     * @return Stream ID or empty string on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    FString CreateVideoStream(
        const FRship2110VideoFormat& VideoFormat,
        const FRship2110TransportParams& TransportParams,
        bool bAutoRegisterIPMX = true);

    /**
     * Stop and destroy a video stream.
     * @param StreamId Stream to destroy
     * @return true if destroyed
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool DestroyVideoStream(const FString& StreamId);

    /**
     * Get a video sender by ID.
     * @param StreamId Stream ID
     * @return Video sender or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    URship2110VideoSender* GetVideoSender(const FString& StreamId) const;

    /**
     * Get all active stream IDs.
     * @return Array of stream IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    TArray<FString> GetActiveStreamIds() const;

    /**
     * Start streaming on a video sender.
     * @param StreamId Stream ID
     * @return true if started
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool StartStream(const FString& StreamId);

    /**
     * Stop streaming on a video sender.
     * @param StreamId Stream ID
     * @return true if stopped
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool StopStream(const FString& StreamId);

    // ========================================================================
    // QUICK ACCESS - IPMX
    // ========================================================================

    /**
     * Connect to IPMX registry.
     * @param RegistryUrl Registry URL (empty = auto-discover)
     * @return true if connection initiated
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|IPMX")
    bool ConnectIPMX(const FString& RegistryUrl = TEXT(""));

    /**
     * Disconnect from IPMX registry.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|IPMX")
    void DisconnectIPMX();

    /**
     * Check if connected to IPMX registry.
     * @return true if connected
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|IPMX")
    bool IsIPMXConnected() const;

    /**
     * Get IPMX status.
     * @return IPMX status
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|IPMX")
    FRshipIPMXStatus GetIPMXStatus() const;

    // ========================================================================
    // QUICK ACCESS - RIVERMAX
    // ========================================================================

    /**
     * Get Rivermax status.
     * @return Rivermax status
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Rivermax")
    FRshipRivermaxStatus GetRivermaxStatus() const;

    /**
     * Get available Rivermax devices.
     * @return Array of devices
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Rivermax")
    TArray<FRshipRivermaxDevice> GetRivermaxDevices() const;

    /**
     * Select Rivermax device by IP.
     * @param IPAddress Device IP
     * @return true if selected
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Rivermax")
    bool SelectRivermaxDevice(const FString& IPAddress);

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * Get settings.
     * @return Settings object
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URship2110Settings* GetSettings() const;

    /**
     * Check if subsystem is fully initialized.
     * @return true if initialized
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsInitialized() const { return bIsInitialized; }

    /**
     * Check if Rivermax SDK is available.
     * @return true if available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsRivermaxAvailable() const;

    /**
     * Check if PTP is available.
     * @return true if available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsPTPAvailable() const;

    /**
     * Check if IPMX is available.
     * @return true if available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsIPMXAvailable() const;

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when PTP state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOnPTPStateChanged OnPTPStateChanged;

    /** Fired when a stream state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110StreamStateChanged OnStreamStateChanged;

    /** Fired when IPMX connection state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOnIPMXConnectionStateChanged OnIPMXConnectionStateChanged;

    /** Fired when Rivermax device changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOnRivermaxDeviceChanged OnRivermaxDeviceChanged;

private:
    // Services
    UPROPERTY()
    URshipPTPService* PTPService = nullptr;

    UPROPERTY()
    URivermaxManager* RivermaxManager = nullptr;

    UPROPERTY()
    URshipIPMXService* IPMXService = nullptr;

    UPROPERTY()
    URship2110VideoCapture* VideoCapture = nullptr;

    // State
    bool bIsInitialized = false;

    // Stream to IPMX mapping
    TMap<FString, FString> StreamToIPMXSender;  // Stream ID -> IPMX Sender ID

    // Initialization helpers
    void InitializePTPService();
    void InitializeRivermaxManager();
    void InitializeIPMXService();
    void InitializeVideoCapture();

    // Event handlers
    void OnPTPStateChangedInternal(ERshipPTPState NewState);
    void OnStreamStateChangedInternal(const FString& StreamId, ERship2110StreamState NewState);
    void OnIPMXStateChangedInternal(ERshipIPMXConnectionState NewState);
    void OnRivermaxDeviceChangedInternal(int32 DeviceIndex, const FRshipRivermaxDevice& Device);
};

// ============================================================================
// BLUEPRINT FUNCTION LIBRARY
// ============================================================================

/**
 * Blueprint function library for 2110 functionality.
 * Provides static functions for convenient Blueprint access.
 */
UCLASS()
class RSHIP2110_API URship2110BlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Get the 2110 subsystem.
     * @return Subsystem or nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110", meta = (WorldContext = "WorldContextObject"))
    static URship2110Subsystem* GetRship2110Subsystem(UObject* WorldContextObject);

    /**
     * Get current PTP time as seconds.
     * @return PTP time in seconds since epoch
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|PTP", meta = (WorldContext = "WorldContextObject"))
    static double GetPTPTimeSeconds(UObject* WorldContextObject);

    /**
     * Check if PTP is locked.
     * @return true if locked to grandmaster
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|PTP", meta = (WorldContext = "WorldContextObject"))
    static bool IsPTPLocked(UObject* WorldContextObject);

    /**
     * Convert frame rate to frame duration in nanoseconds.
     * @param FrameRate Frame rate
     * @return Duration in nanoseconds
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|Util")
    static int64 FrameRateToNanoseconds(const FFrameRate& FrameRate);

    /**
     * Convert video format to bitrate in Mbps.
     * @param VideoFormat Video format
     * @return Bitrate in Mbps
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|Util")
    static double VideoFormatToBitrate(const FRship2110VideoFormat& VideoFormat);

    /**
     * Create default video format for common resolution.
     * @param Width Resolution width
     * @param Height Resolution height
     * @param FrameRate Frame rate
     * @return Default video format
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|Util")
    static FRship2110VideoFormat CreateVideoFormat(
        int32 Width,
        int32 Height,
        const FFrameRate& FrameRate);

    /**
     * Create default transport params for multicast.
     * @param MulticastIP Multicast IP address
     * @param Port UDP port
     * @return Default transport params
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|Util")
    static FRship2110TransportParams CreateTransportParams(
        const FString& MulticastIP,
        int32 Port = 5004);
};
