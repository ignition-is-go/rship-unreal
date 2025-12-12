// Copyright Rocketship. All Rights Reserved.
// PTP (IEEE 1588 / SMPTE 2059) Time Service
//
// This service manages PTP synchronization with a grandmaster clock,
// providing high-precision timestamps for SMPTE 2110 streaming.
//
// Key features:
// - Synchronizes to PTP grandmaster (SMPTE 2059, domain 127)
// - Provides nanosecond-resolution timestamps
// - Calculates frame boundaries for precise video timing
// - Generates RTP timestamps aligned to PTP
// - Supports hardware timestamping on compatible NICs

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Rship2110Types.h"
#include "IPTPProvider.h"
#include "RshipPTPService.generated.h"

class URship2110Subsystem;

/**
 * PTP Time Service for SMPTE 2110 synchronization.
 *
 * Provides PTP-disciplined timing for video frame alignment and
 * RTP timestamp generation. Works with platform-specific PTP
 * implementations (Windows PTP, linuxptp, etc.).
 */
UCLASS(BlueprintType)
class RSHIP2110_API URshipPTPService : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initialize the PTP service.
     * @param InSubsystem Parent subsystem
     * @return true if initialization succeeded
     */
    bool Initialize(URship2110Subsystem* InSubsystem);

    /**
     * Shutdown and release resources.
     */
    void Shutdown();

    /**
     * Tick update - must be called each frame.
     * @param DeltaTime Time since last tick
     */
    void Tick(float DeltaTime);

    // ========================================================================
    // TIME QUERIES
    // ========================================================================

    /**
     * Get current PTP time.
     * @return Current PTP timestamp (TAI epoch, nanosecond precision)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    FRshipPTPTimestamp GetPTPTime() const;

    /**
     * Get current PTP time as seconds since epoch.
     * @return PTP time as double precision seconds
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    double GetPTPTimeSeconds() const;

    /**
     * Get offset from system clock.
     * @return Offset (PTP - System) in nanoseconds
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    int64 GetOffsetFromSystemNs() const;

    /**
     * Get offset from system clock in milliseconds.
     * @return Offset in milliseconds (more convenient for BP)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    double GetOffsetFromSystemMs() const;

    // ========================================================================
    // FRAME TIMING
    // ========================================================================

    /**
     * Get the timestamp of the next frame boundary.
     * Frame boundaries are aligned to PTP epoch based on frame rate.
     * @param FrameRate Frame rate to use for calculation
     * @return PTP timestamp of next frame boundary
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    FRshipPTPTimestamp GetNextFrameBoundary(const FFrameRate& FrameRate) const;

    /**
     * Get the timestamp of the next frame boundary using video format.
     * @param VideoFormat Video format containing frame rate
     * @return PTP timestamp of next frame boundary
     */
    FRshipPTPTimestamp GetNextFrameBoundaryForFormat(const FRship2110VideoFormat& VideoFormat) const;

    /**
     * Get time until next frame boundary in nanoseconds.
     * @param FrameRate Frame rate to use
     * @return Nanoseconds until next frame boundary
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    int64 GetTimeUntilNextFrameNs(const FFrameRate& FrameRate) const;

    /**
     * Get current frame number since PTP epoch.
     * @param FrameRate Frame rate to use
     * @return Frame number (may be very large)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    int64 GetCurrentFrameNumber(const FFrameRate& FrameRate) const;

    // ========================================================================
    // RTP TIMESTAMP GENERATION
    // ========================================================================

    /**
     * Get RTP timestamp for current PTP time.
     * Uses 90kHz clock rate by default (standard for video).
     * @param ClockRate RTP clock rate (default 90000)
     * @return 32-bit RTP timestamp
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    int64 GetRTPTimestamp(int32 ClockRate = 90000) const;

    /**
     * Get RTP timestamp for a specific PTP time.
     * @param PTPTime PTP timestamp to convert
     * @param ClockRate RTP clock rate
     * @return 32-bit RTP timestamp
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    int64 GetRTPTimestampForTime(const FRshipPTPTimestamp& PTPTime, int32 ClockRate = 90000) const;

    /**
     * Get RTP timestamp increment per frame.
     * @param FrameRate Frame rate
     * @param ClockRate RTP clock rate
     * @return RTP timestamp increment
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    int32 GetRTPTimestampIncrement(const FFrameRate& FrameRate, int32 ClockRate = 90000) const;

    // ========================================================================
    // STATUS
    // ========================================================================

    /**
     * Get current PTP state.
     * @return Synchronization state
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    ERshipPTPState GetState() const;

    /**
     * Get full PTP status.
     * @return Complete status structure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    FRshipPTPStatus GetStatus() const;

    /**
     * Check if synchronized to grandmaster.
     * @return true if locked to grandmaster
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    bool IsLocked() const;

    /**
     * Check if hardware timestamping is active.
     * @return true if using hardware timestamps
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    bool IsHardwareTimestampingEnabled() const;

    /**
     * Get grandmaster clock identity.
     * @return Grandmaster info structure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    FRshipPTPGrandmaster GetGrandmaster() const;

    // ========================================================================
    // CONTROL
    // ========================================================================

    /**
     * Force re-synchronization with grandmaster.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    void ForceResync();

    /**
     * Set PTP domain.
     * @param Domain New domain number
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    void SetDomain(int32 Domain);

    /**
     * Set network interface for PTP.
     * @param InterfaceIP IP address of interface (empty = auto)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    void SetInterface(const FString& InterfaceIP);

    /**
     * Enable/disable hardware timestamping.
     * @param bEnable true to enable
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|PTP")
    void SetHardwareTimestamping(bool bEnable);

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when PTP state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|PTP")
    FOnPTPStateChanged OnStateChanged;

    /** Fired when PTP status is updated (every tick when locked) */
    UPROPERTY(BlueprintAssignable, Category = "Rship|PTP")
    FOnPTPStatusUpdated OnStatusUpdated;

private:
    UPROPERTY()
    URship2110Subsystem* Subsystem = nullptr;

    // Platform-specific PTP provider
    TUniquePtr<IPTPProvider> Provider;

    // Configuration
    FString ConfiguredInterfaceIP;
    int32 ConfiguredDomain = 127;
    bool bHardwareTimestampingRequested = true;

    // State tracking
    ERshipPTPState LastState = ERshipPTPState::Disabled;
    double LastStatusBroadcast = 0.0;
    static constexpr double StatusBroadcastInterval = 0.1;  // 100ms

    // Statistics
    TArray<int64> RecentOffsets;
    static constexpr int32 MaxOffsetSamples = 100;

    // Internal methods
    void UpdateStatistics();
    void BroadcastStatusIfNeeded();
    bool CreateProvider();
};
