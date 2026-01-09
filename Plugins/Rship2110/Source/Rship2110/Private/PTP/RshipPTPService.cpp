// Copyright Rocketship. All Rights Reserved.

#include "PTP/RshipPTPService.h"
#include "Rship2110Subsystem.h"
#include "Rship2110Settings.h"
#include "Rship2110.h"

bool URshipPTPService::Initialize(URship2110Subsystem* InSubsystem)
{
    if (InSubsystem == nullptr)
    {
        UE_LOG(LogRship2110, Error, TEXT("PTPService: Invalid subsystem"));
        return false;
    }

    Subsystem = InSubsystem;

    // Load settings
    URship2110Settings* Settings = URship2110Settings::Get();
    if (Settings)
    {
        ConfiguredInterfaceIP = Settings->PTPInterfaceIP;
        ConfiguredDomain = Settings->PTPDomain;
        bHardwareTimestampingRequested = Settings->bUseHardwareTimestamping;
    }

    // Create the platform-specific provider
    if (!CreateProvider())
    {
        UE_LOG(LogRship2110, Warning, TEXT("PTPService: Failed to create provider, using fallback"));
        Provider = FPTPProviderFactory::CreateFallback();
    }

    if (!Provider)
    {
        UE_LOG(LogRship2110, Error, TEXT("PTPService: Failed to create any PTP provider"));
        return false;
    }

    // Initialize the provider
    if (!Provider->Initialize(ConfiguredInterfaceIP, ConfiguredDomain))
    {
        UE_LOG(LogRship2110, Warning, TEXT("PTPService: Provider initialization failed"));
        // Continue anyway - may be able to sync later
    }

    UE_LOG(LogRship2110, Log, TEXT("PTPService: Initialized with provider %s, domain %d"),
           *Provider->GetProviderName(), ConfiguredDomain);

    return true;
}

void URshipPTPService::Shutdown()
{
    if (Provider)
    {
        Provider->Shutdown();
        Provider.Reset();
    }

    Subsystem = nullptr;
    LastState = ERshipPTPState::Disabled;
    RecentOffsets.Empty();

    UE_LOG(LogRship2110, Log, TEXT("PTPService: Shutdown complete"));
}

void URshipPTPService::Tick(float DeltaTime)
{
    if (!Provider)
    {
        return;
    }

    // Update the provider
    Provider->Tick(DeltaTime);

    // Check for state changes
    ERshipPTPState CurrentState = Provider->GetState();
    if (CurrentState != LastState)
    {
        UE_LOG(LogRship2110, Log, TEXT("PTPService: State changed from %d to %d"),
               static_cast<int32>(LastState), static_cast<int32>(CurrentState));
        LastState = CurrentState;
        OnStateChanged.Broadcast(CurrentState);
    }

    // Update statistics
    UpdateStatistics();

    // Broadcast status periodically
    BroadcastStatusIfNeeded();
}

FRshipPTPTimestamp URshipPTPService::GetPTPTime() const
{
    if (!Provider)
    {
        return FRshipPTPTimestamp();
    }
    return Provider->GetPTPTime();
}

double URshipPTPService::GetPTPTimeSeconds() const
{
    return GetPTPTime().ToSeconds();
}

int64 URshipPTPService::GetOffsetFromSystemNs() const
{
    if (!Provider)
    {
        return 0;
    }
    return Provider->GetOffsetFromSystemNs();
}

double URshipPTPService::GetOffsetFromSystemMs() const
{
    return static_cast<double>(GetOffsetFromSystemNs()) / 1000000.0;
}

FRshipPTPTimestamp URshipPTPService::GetNextFrameBoundary(const FFrameRate& FrameRate) const
{
    if (!Provider)
    {
        return FRshipPTPTimestamp();
    }

    uint64 FrameDurationNs = static_cast<uint64>(
        1000000000.0 * FrameRate.Denominator / FrameRate.Numerator);

    return Provider->GetNextFrameBoundary(FrameDurationNs);
}

FRshipPTPTimestamp URshipPTPService::GetNextFrameBoundaryForFormat(const FRship2110VideoFormat& VideoFormat) const
{
    if (!Provider)
    {
        return FRshipPTPTimestamp();
    }

    return Provider->GetNextFrameBoundary(VideoFormat.GetFrameDurationNs());
}

int64 URshipPTPService::GetTimeUntilNextFrameNs(const FFrameRate& FrameRate) const
{
    if (!Provider)
    {
        return 0;
    }

    FRshipPTPTimestamp CurrentTime = Provider->GetPTPTime();
    FRshipPTPTimestamp NextBoundary = GetNextFrameBoundary(FrameRate);

    return static_cast<int64>(NextBoundary.ToNanoseconds() - CurrentTime.ToNanoseconds());
}

int64 URshipPTPService::GetCurrentFrameNumber(const FFrameRate& FrameRate) const
{
    if (!Provider)
    {
        return 0;
    }

    FRshipPTPTimestamp CurrentTime = Provider->GetPTPTime();
    uint64 TotalNs = CurrentTime.ToNanoseconds();
    uint64 FrameDurationNs = static_cast<uint64>(
        1000000000.0 * FrameRate.Denominator / FrameRate.Numerator);

    return static_cast<int64>(TotalNs / FrameDurationNs);
}

int64 URshipPTPService::GetRTPTimestamp(int32 ClockRate) const
{
    if (!Provider)
    {
        return 0;
    }

    return static_cast<int64>(Provider->GetRTPTimestamp(Provider->GetPTPTime(), static_cast<uint32>(ClockRate)));
}

int64 URshipPTPService::GetRTPTimestampForTime(const FRshipPTPTimestamp& PTPTime, int32 ClockRate) const
{
    if (!Provider)
    {
        return 0;
    }

    return static_cast<int64>(Provider->GetRTPTimestamp(PTPTime, static_cast<uint32>(ClockRate)));
}

int32 URshipPTPService::GetRTPTimestampIncrement(const FFrameRate& FrameRate, int32 ClockRate) const
{
    // RTP timestamp increment per frame = ClockRate / FrameRate
    return static_cast<int32>(
        static_cast<double>(ClockRate) * FrameRate.Denominator / FrameRate.Numerator);
}

ERshipPTPState URshipPTPService::GetState() const
{
    if (!Provider)
    {
        return ERshipPTPState::Disabled;
    }
    return Provider->GetState();
}

FRshipPTPStatus URshipPTPService::GetStatus() const
{
    if (!Provider)
    {
        FRshipPTPStatus Status;
        Status.State = ERshipPTPState::Disabled;
        return Status;
    }
    return Provider->GetStatus();
}

bool URshipPTPService::IsLocked() const
{
    return GetState() == ERshipPTPState::Locked;
}

bool URshipPTPService::IsHardwareTimestampingEnabled() const
{
    if (!Provider)
    {
        return false;
    }
    return Provider->IsHardwareTimestampingEnabled();
}

FRshipPTPGrandmaster URshipPTPService::GetGrandmaster() const
{
    FRshipPTPStatus Status = GetStatus();
    return Status.Grandmaster;
}

void URshipPTPService::ForceResync()
{
    if (Provider)
    {
        // Re-initialize the provider to force resync
        Provider->Shutdown();
        Provider->Initialize(ConfiguredInterfaceIP, ConfiguredDomain);
    }
}

void URshipPTPService::SetDomain(int32 Domain)
{
    if (Domain < 0 || Domain > 127)
    {
        UE_LOG(LogRship2110, Warning, TEXT("PTPService: Invalid domain %d, must be 0-127"), Domain);
        return;
    }

    if (Domain != ConfiguredDomain)
    {
        ConfiguredDomain = Domain;
        if (Provider)
        {
            Provider->Shutdown();
            Provider->Initialize(ConfiguredInterfaceIP, ConfiguredDomain);
        }
        UE_LOG(LogRship2110, Log, TEXT("PTPService: Domain changed to %d"), Domain);
    }
}

void URshipPTPService::SetInterface(const FString& InterfaceIP)
{
    if (InterfaceIP != ConfiguredInterfaceIP)
    {
        ConfiguredInterfaceIP = InterfaceIP;
        if (Provider)
        {
            Provider->Shutdown();
            Provider->Initialize(ConfiguredInterfaceIP, ConfiguredDomain);
        }
        UE_LOG(LogRship2110, Log, TEXT("PTPService: Interface changed to %s"),
               ConfiguredInterfaceIP.IsEmpty() ? TEXT("auto") : *ConfiguredInterfaceIP);
    }
}

void URshipPTPService::SetHardwareTimestamping(bool bEnable)
{
    bHardwareTimestampingRequested = bEnable;
    UE_LOG(LogRship2110, Log, TEXT("PTPService: Hardware timestamping %s"),
           bEnable ? TEXT("requested") : TEXT("disabled"));
}

bool URshipPTPService::CreateProvider()
{
    Provider = FPTPProviderFactory::Create();
    return Provider.IsValid();
}

void URshipPTPService::UpdateStatistics()
{
    if (!Provider || Provider->GetState() != ERshipPTPState::Locked)
    {
        return;
    }

    int64 CurrentOffset = Provider->GetOffsetFromSystemNs();
    RecentOffsets.Add(CurrentOffset);

    if (RecentOffsets.Num() > MaxOffsetSamples)
    {
        RecentOffsets.RemoveAt(0);
    }
}

void URshipPTPService::BroadcastStatusIfNeeded()
{
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastStatusBroadcast >= StatusBroadcastInterval)
    {
        LastStatusBroadcast = CurrentTime;
        FRshipPTPStatus Status = GetStatus();
        OnStatusUpdated.Broadcast(Status);
    }
}
