// Copyright Rocketship. All Rights Reserved.
// SMPTE ST 2110 / IPMX / PTP Integration Module
//
// This module provides:
// - PTP (IEEE 1588 / SMPTE 2059) time synchronization
// - SMPTE ST 2110 video/audio/ancillary streaming via Rivermax
// - IPMX (NMOS IS-04/IS-05) discovery and connection management
//
// Architecture:
// - URship2110Subsystem: Main orchestrator (UEngineSubsystem)
// - URshipPTPService: PTP grandmaster synchronization
// - URivermaxManager: NIC device management and stream creation
// - URshipIPMXService: NMOS-style registration and discovery

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRship2110, Log, All);

/**
 * Module interface for Rship2110.
 * Provides SMPTE 2110 professional media streaming integrated with UE rendering.
 */
class FRship2110Module : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /**
     * Singleton access to module.
     * @return Reference to the module
     */
    static FRship2110Module& Get();

    /**
     * Check if the module is available.
     * @return true if module is loaded and available
     */
    static bool IsAvailable();

    /**
     * Check if Rivermax SDK is available.
     * @return true if Rivermax features are enabled
     */
    bool IsRivermaxAvailable() const { return bRivermaxAvailable; }

    /**
     * Check if PTP synchronization is available.
     * @return true if PTP features are enabled
     */
    bool IsPTPAvailable() const { return bPTPAvailable; }

    /**
     * Check if IPMX/NMOS features are available.
     * @return true if IPMX features are enabled
     */
    bool IsIPMXAvailable() const { return bIPMXAvailable; }

private:
    bool bRivermaxAvailable = false;
    bool bPTPAvailable = false;
    bool bIPMXAvailable = false;
};
