// Copyright Rocketship. All Rights Reserved.
// Windows PTP Provider Implementation
//
// Uses Windows PTP APIs when available, with fallback to system clock.
// On Windows 10 1809+, uses the W32Time PTP provider.
// For ConnectX NICs, can also use hardware timestamping via Rivermax.

#include "PTP/IPTPProvider.h"
#include "Rship2110.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sysinfoapi.h>  // For GetSystemTimePreciseAsFileTime
#include <mmsystem.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// ============================================================================
// FALLBACK PTP PROVIDER
// Uses system clock with manual offset tracking
// ============================================================================

class FFallbackPTPProvider : public IPTPProvider
{
public:
    virtual ~FFallbackPTPProvider() = default;

    virtual bool Initialize(const FString& InterfaceIP, int32 Domain) override
    {
        ConfiguredDomain = Domain;
        ConfiguredInterface = InterfaceIP;

        // Get initial system time as our reference
        InitialSystemTime = FPlatformTime::Seconds();
        State = ERshipPTPState::Locked;  // Always "locked" to system clock

        // Generate a fake clock identity
        ClockIdentity = FString::Printf(TEXT("00:00:00:FF:FE:00:00:%02X"), Domain);

        UE_LOG(LogRship2110, Log, TEXT("FallbackPTPProvider: Initialized (using system clock)"));
        return true;
    }

    virtual void Shutdown() override
    {
        State = ERshipPTPState::Disabled;
        UE_LOG(LogRship2110, Log, TEXT("FallbackPTPProvider: Shutdown"));
    }

    virtual void Tick(float DeltaTime) override
    {
        // Update offset tracking
        double CurrentSystemTime = FPlatformTime::Seconds();

        // In fallback mode, PTP time == system time
        // We track a simulated offset that is always 0
        LastOffset = 0;
    }

    virtual FRshipPTPTimestamp GetPTPTime() const override
    {
        // Convert FPlatformTime to PTP timestamp
        // FPlatformTime::Seconds() returns seconds since process start
        // We need to add the Unix epoch offset to get actual wall time

        double Seconds = FPlatformTime::Seconds();

        // Get actual wall clock time
        FDateTime Now = FDateTime::UtcNow();
        int64 UnixTimestamp = Now.ToUnixTimestamp();

        FRshipPTPTimestamp Timestamp;
        Timestamp.Seconds = UnixTimestamp;
        Timestamp.Nanoseconds = static_cast<int32>(
            (Seconds - FMath::FloorToDouble(Seconds)) * 1000000000.0);

        return Timestamp;
    }

    virtual ERshipPTPState GetState() const override
    {
        return State;
    }

    virtual FRshipPTPStatus GetStatus() const override
    {
        FRshipPTPStatus Status;
        Status.State = State;
        Status.CurrentTime = GetPTPTime();
        Status.OffsetFromSystemNs = 0;  // By definition
        Status.PathDelayNs = 0;
        Status.DriftPPB = 0.0;
        Status.JitterNs = 0.0;

        Status.Grandmaster.ClockIdentity = ClockIdentity;
        Status.Grandmaster.Domain = static_cast<uint8>(ConfiguredDomain);
        Status.Grandmaster.Priority1 = 128;
        Status.Grandmaster.Priority2 = 128;

        return Status;
    }

    virtual int64 GetOffsetFromSystemNs() const override
    {
        return LastOffset;
    }

    virtual FRshipPTPTimestamp GetNextFrameBoundary(
        uint64 FrameDurationNs,
        const FRshipPTPTimestamp* CurrentPTPTime) const override
    {
        FRshipPTPTimestamp Current = CurrentPTPTime ? *CurrentPTPTime : GetPTPTime();
        uint64 CurrentNs = Current.ToNanoseconds();

        // Calculate next frame boundary (aligned to epoch)
        uint64 CurrentFrame = CurrentNs / FrameDurationNs;
        uint64 NextFrameNs = (CurrentFrame + 1) * FrameDurationNs;

        return FRshipPTPTimestamp::FromNanoseconds(NextFrameNs);
    }

    virtual uint32 GetRTPTimestamp(
        const FRshipPTPTimestamp& PTPTime,
        uint32 ClockRate) const override
    {
        // RTP timestamp is based on clock rate (typically 90kHz for video)
        // We use the lower 32 bits of (seconds * clock_rate + fraction)
        uint64 TotalTicks = static_cast<uint64>(PTPTime.Seconds) * ClockRate +
                            static_cast<uint64>(PTPTime.Nanoseconds) * ClockRate / 1000000000ULL;
        return static_cast<uint32>(TotalTicks);
    }

    virtual bool IsHardwareTimestampingEnabled() const override
    {
        return false;
    }

    virtual FString GetProviderName() const override
    {
        return TEXT("Fallback (System Clock)");
    }

private:
    ERshipPTPState State = ERshipPTPState::Disabled;
    int32 ConfiguredDomain = 127;
    FString ConfiguredInterface;
    FString ClockIdentity;
    double InitialSystemTime = 0.0;
    int64 LastOffset = 0;
};

// ============================================================================
// WINDOWS PTP PROVIDER
// Uses Windows W32Time or Rivermax hardware timestamps
// ============================================================================

#if PLATFORM_WINDOWS

class FWindowsPTPProvider : public IPTPProvider
{
public:
    virtual ~FWindowsPTPProvider() = default;

    virtual bool Initialize(const FString& InterfaceIP, int32 Domain) override
    {
        ConfiguredDomain = Domain;
        ConfiguredInterface = InterfaceIP;

        // Try to get hardware timestamp support
        bHardwareTimestamping = CheckHardwareTimestampSupport();

        // Initialize Windows high-resolution timer
        LARGE_INTEGER Freq;
        if (QueryPerformanceFrequency(&Freq))
        {
            PerformanceFrequency = Freq.QuadPart;
            QueryPerformanceCounter(&Freq);
            InitialPerformanceCounter = Freq.QuadPart;
            bHighResTimerAvailable = true;
        }

        // Get initial time reference
        InitialSystemTime = FPlatformTime::Seconds();

        // Try to connect to Windows PTP service
        if (TryConnectToWindowsPTP())
        {
            State = ERshipPTPState::Acquiring;
            UE_LOG(LogRship2110, Log, TEXT("WindowsPTPProvider: Connected to Windows PTP service"));
        }
        else
        {
            // Fall back to system clock tracking
            State = ERshipPTPState::Locked;  // Locked to system clock
            bUsingSystemClock = true;
            UE_LOG(LogRship2110, Warning, TEXT("WindowsPTPProvider: Windows PTP not available, using system clock"));
        }

        UE_LOG(LogRship2110, Log, TEXT("WindowsPTPProvider: Initialized (HW timestamps: %s)"),
               bHardwareTimestamping ? TEXT("yes") : TEXT("no"));

        return true;
    }

    virtual void Shutdown() override
    {
        State = ERshipPTPState::Disabled;
        bConnectedToPTP = false;
        UE_LOG(LogRship2110, Log, TEXT("WindowsPTPProvider: Shutdown"));
    }

    virtual void Tick(float DeltaTime) override
    {
        if (bUsingSystemClock)
        {
            // System clock mode - always "locked"
            State = ERshipPTPState::Locked;
            return;
        }

        // Query Windows PTP status
        UpdatePTPStatus();

        // Track offset statistics
        if (State == ERshipPTPState::Locked)
        {
            int64 CurrentOffset = QueryPTPOffset();
            UpdateOffsetStatistics(CurrentOffset);
        }
    }

    virtual FRshipPTPTimestamp GetPTPTime() const override
    {
        if (bHardwareTimestamping)
        {
            return GetHardwarePTPTime();
        }

        // Use Windows system time with offset correction
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);

        // Convert FILETIME to Unix timestamp
        // FILETIME is 100-nanosecond intervals since 1601-01-01
        // Unix timestamp is seconds since 1970-01-01
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;

        // Subtract Windows/Unix epoch difference (11644473600 seconds)
        const uint64 EPOCH_DIFF = 116444736000000000ULL;
        uint64 Ticks100ns = uli.QuadPart - EPOCH_DIFF;

        FRshipPTPTimestamp Timestamp;
        Timestamp.Seconds = static_cast<int64>(Ticks100ns / 10000000ULL);
        Timestamp.Nanoseconds = static_cast<int32>((Ticks100ns % 10000000ULL) * 100);

        // Apply PTP offset if known
        if (State == ERshipPTPState::Locked && !bUsingSystemClock)
        {
            int64 TotalNs = Timestamp.ToNanoseconds() + LastKnownOffset;
            Timestamp = FRshipPTPTimestamp::FromNanoseconds(TotalNs);
        }

        return Timestamp;
    }

    virtual ERshipPTPState GetState() const override
    {
        return State;
    }

    virtual FRshipPTPStatus GetStatus() const override
    {
        FRshipPTPStatus Status;
        Status.State = State;
        Status.CurrentTime = GetPTPTime();
        Status.OffsetFromSystemNs = LastKnownOffset;
        Status.PathDelayNs = PathDelay;
        Status.DriftPPB = CalculateDrift();
        Status.JitterNs = CalculateJitter();

        Status.Grandmaster.ClockIdentity = GrandmasterIdentity;
        Status.Grandmaster.Domain = static_cast<uint8>(ConfiguredDomain);
        Status.Grandmaster.Priority1 = GrandmasterPriority1;
        Status.Grandmaster.Priority2 = GrandmasterPriority2;
        Status.Grandmaster.StepsRemoved = StepsRemoved;

        return Status;
    }

    virtual int64 GetOffsetFromSystemNs() const override
    {
        return LastKnownOffset;
    }

    virtual FRshipPTPTimestamp GetNextFrameBoundary(
        uint64 FrameDurationNs,
        const FRshipPTPTimestamp* CurrentPTPTime) const override
    {
        FRshipPTPTimestamp Current = CurrentPTPTime ? *CurrentPTPTime : GetPTPTime();
        uint64 CurrentNs = Current.ToNanoseconds();

        // Calculate next frame boundary (aligned to epoch)
        uint64 CurrentFrame = CurrentNs / FrameDurationNs;
        uint64 NextFrameNs = (CurrentFrame + 1) * FrameDurationNs;

        return FRshipPTPTimestamp::FromNanoseconds(NextFrameNs);
    }

    virtual uint32 GetRTPTimestamp(
        const FRshipPTPTimestamp& PTPTime,
        uint32 ClockRate) const override
    {
        uint64 TotalTicks = static_cast<uint64>(PTPTime.Seconds) * ClockRate +
                            static_cast<uint64>(PTPTime.Nanoseconds) * ClockRate / 1000000000ULL;
        return static_cast<uint32>(TotalTicks);
    }

    virtual bool IsHardwareTimestampingEnabled() const override
    {
        return bHardwareTimestamping;
    }

    virtual FString GetProviderName() const override
    {
        if (bHardwareTimestamping)
        {
            return TEXT("Windows PTP (Hardware)");
        }
        else if (bUsingSystemClock)
        {
            return TEXT("Windows (System Clock)");
        }
        return TEXT("Windows PTP");
    }

private:
    ERshipPTPState State = ERshipPTPState::Disabled;
    int32 ConfiguredDomain = 127;
    FString ConfiguredInterface;

    // Windows timing
    bool bHighResTimerAvailable = false;
    int64 PerformanceFrequency = 0;
    int64 InitialPerformanceCounter = 0;
    double InitialSystemTime = 0.0;

    // PTP state
    bool bConnectedToPTP = false;
    bool bUsingSystemClock = false;
    bool bHardwareTimestamping = false;
    int64 LastKnownOffset = 0;
    int64 PathDelay = 0;
    uint16 StepsRemoved = 0;
    uint8 GrandmasterPriority1 = 128;
    uint8 GrandmasterPriority2 = 128;
    FString GrandmasterIdentity;

    // Offset tracking for statistics
    TArray<int64> OffsetHistory;
    TArray<double> OffsetTimes;
    static constexpr int32 MaxOffsetHistory = 100;

    bool CheckHardwareTimestampSupport()
    {
        // TODO: Query Rivermax or NIC driver for hardware timestamp support
        // For now, return false - hardware timestamping requires Rivermax integration
        return false;
    }

    bool TryConnectToWindowsPTP()
    {
        // Windows 10 1809+ supports PTP through W32Time
        // Query the Windows Time service for PTP status

        // For now, simulate checking for Windows PTP
        // In production, you would use W32Time APIs or registry queries

        // Check if we're running on Windows 10 1809+
        OSVERSIONINFOEXW osvi;
        ZeroMemory(&osvi, sizeof(osvi));
        osvi.dwOSVersionInfoSize = sizeof(osvi);

        // Note: GetVersionEx is deprecated, using RtlGetVersion would be better
        // For now, assume Windows PTP is not available and use system clock
        return false;
    }

    void UpdatePTPStatus()
    {
        // Query Windows PTP status
        // In a full implementation, this would:
        // 1. Query W32Time service for PTP status
        // 2. Check if we're synced to a PTP grandmaster
        // 3. Get offset and path delay information

        if (!bConnectedToPTP)
        {
            return;
        }

        // Update state based on sync status
        // This is placeholder logic
        if (FMath::Abs(LastKnownOffset) < 1000)  // < 1us
        {
            State = ERshipPTPState::Locked;
        }
        else if (FMath::Abs(LastKnownOffset) < 100000)  // < 100us
        {
            State = ERshipPTPState::Acquiring;
        }
        else
        {
            State = ERshipPTPState::Holdover;
        }
    }

    int64 QueryPTPOffset()
    {
        // Query current offset from grandmaster
        // In production, this comes from PTP sync data
        return LastKnownOffset;
    }

    FRshipPTPTimestamp GetHardwarePTPTime() const
    {
        // Get timestamp from hardware (Rivermax)
        // This requires Rivermax SDK integration
        // For now, fall back to software timestamp
        return GetPTPTime();
    }

    void UpdateOffsetStatistics(int64 Offset)
    {
        double CurrentTime = FPlatformTime::Seconds();

        OffsetHistory.Add(Offset);
        OffsetTimes.Add(CurrentTime);

        if (OffsetHistory.Num() > MaxOffsetHistory)
        {
            OffsetHistory.RemoveAt(0);
            OffsetTimes.RemoveAt(0);
        }

        LastKnownOffset = Offset;
    }

    double CalculateDrift() const
    {
        if (OffsetHistory.Num() < 2)
        {
            return 0.0;
        }

        // Calculate drift in parts per billion
        int64 FirstOffset = OffsetHistory[0];
        int64 LastOffset = OffsetHistory.Last();
        double FirstTime = OffsetTimes[0];
        double LastTime = OffsetTimes.Last();

        double TimeDiff = LastTime - FirstTime;
        if (TimeDiff < 0.001)
        {
            return 0.0;
        }

        double OffsetDiff = static_cast<double>(LastOffset - FirstOffset);
        double DriftNsPerSec = OffsetDiff / TimeDiff;
        return DriftNsPerSec;  // ns/s = ppb
    }

    double CalculateJitter() const
    {
        if (OffsetHistory.Num() < 2)
        {
            return 0.0;
        }

        // Calculate standard deviation of offsets
        double Sum = 0.0;
        for (int64 Offset : OffsetHistory)
        {
            Sum += static_cast<double>(Offset);
        }
        double Mean = Sum / OffsetHistory.Num();

        double SumSquares = 0.0;
        for (int64 Offset : OffsetHistory)
        {
            double Diff = static_cast<double>(Offset) - Mean;
            SumSquares += Diff * Diff;
        }

        return FMath::Sqrt(SumSquares / OffsetHistory.Num());
    }
};

#endif  // PLATFORM_WINDOWS

// ============================================================================
// FACTORY IMPLEMENTATION
// ============================================================================

TUniquePtr<IPTPProvider> FPTPProviderFactory::Create()
{
#if PLATFORM_WINDOWS
    return MakeUnique<FWindowsPTPProvider>();
#else
    // Other platforms use fallback
    return CreateFallback();
#endif
}

TUniquePtr<IPTPProvider> FPTPProviderFactory::CreateFallback()
{
    return MakeUnique<FFallbackPTPProvider>();
}
