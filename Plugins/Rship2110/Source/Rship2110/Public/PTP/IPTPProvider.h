// Copyright Rocketship. All Rights Reserved.
// Abstract interface for PTP time providers
//
// This allows swapping PTP implementations based on platform:
// - Windows: Uses Windows PTP client APIs
// - Linux: Uses linuxptp / ptp4l integration
// - Fallback: System clock with offset estimation

#pragma once

#include "CoreMinimal.h"
#include "Rship2110Types.h"

/**
 * Abstract interface for PTP time providers.
 * Platform-specific implementations derive from this.
 */
class RSHIP2110_API IPTPProvider
{
public:
    virtual ~IPTPProvider() = default;

    /**
     * Initialize the PTP provider.
     * @param InterfaceIP Network interface IP for PTP (empty = auto-detect)
     * @param Domain PTP domain number
     * @return true if initialization succeeded
     */
    virtual bool Initialize(const FString& InterfaceIP, int32 Domain) = 0;

    /**
     * Shutdown the PTP provider and release resources.
     */
    virtual void Shutdown() = 0;

    /**
     * Tick update - called each frame to update state.
     * @param DeltaTime Time since last tick
     */
    virtual void Tick(float DeltaTime) = 0;

    /**
     * Get current PTP time.
     * @return Current PTP timestamp (TAI epoch)
     */
    virtual FRshipPTPTimestamp GetPTPTime() const = 0;

    /**
     * Get current synchronization state.
     * @return PTP state enum
     */
    virtual ERshipPTPState GetState() const = 0;

    /**
     * Get full status information.
     * @return Complete PTP status structure
     */
    virtual FRshipPTPStatus GetStatus() const = 0;

    /**
     * Get offset from system clock in nanoseconds.
     * @return Offset (PTP time - system time) in nanoseconds
     */
    virtual int64 GetOffsetFromSystemNs() const = 0;

    /**
     * Get the next frame boundary timestamp.
     * @param FrameDurationNs Frame duration in nanoseconds
     * @param CurrentPTPTime Current PTP time (or will query if default)
     * @return PTP timestamp of next frame boundary
     */
    virtual FRshipPTPTimestamp GetNextFrameBoundary(
        uint64 FrameDurationNs,
        const FRshipPTPTimestamp* CurrentPTPTime = nullptr) const = 0;

    /**
     * Get RTP timestamp for a given PTP time.
     * RTP uses 90kHz clock for video by convention.
     * @param PTPTime PTP timestamp to convert
     * @param ClockRate RTP clock rate (default 90000 for video)
     * @return RTP timestamp value
     */
    virtual uint32 GetRTPTimestamp(
        const FRshipPTPTimestamp& PTPTime,
        uint32 ClockRate = 90000) const = 0;

    /**
     * Check if hardware timestamping is available and enabled.
     * @return true if using hardware timestamps
     */
    virtual bool IsHardwareTimestampingEnabled() const = 0;

    /**
     * Get provider name for logging.
     * @return Provider implementation name
     */
    virtual FString GetProviderName() const = 0;
};

/**
 * Factory for creating platform-appropriate PTP providers.
 */
class RSHIP2110_API FPTPProviderFactory
{
public:
    /**
     * Create the appropriate PTP provider for the current platform.
     * @return Unique pointer to PTP provider (caller owns)
     */
    static TUniquePtr<IPTPProvider> Create();

    /**
     * Create a fallback PTP provider that uses system clock.
     * Useful when no PTP grandmaster is available.
     * @return Unique pointer to fallback provider
     */
    static TUniquePtr<IPTPProvider> CreateFallback();
};
