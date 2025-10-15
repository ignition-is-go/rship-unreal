#include "RshipFrameSyncSubsystem.h"

#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformTime.h"
#include "Logging/LogMacros.h"

void URshipFrameSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Config = FRshipFrameSyncConfig();
    FrameDurationSeconds = Config.ExpectedFrameRate.AsDecimal() > 0.0 ? 1.0 / Config.ExpectedFrameRate.AsDecimal() : 0.0;
    ApplyFixedFrameRate();

    BeginFrameHandle = FCoreDelegates::OnBeginFrame.AddUObject(this, &URshipFrameSyncSubsystem::HandleBeginFrame);
    EndFrameHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &URshipFrameSyncSubsystem::HandleEndFrame);
}

void URshipFrameSyncSubsystem::Deinitialize()
{
    if (BeginFrameHandle.IsValid())
    {
        FCoreDelegates::OnBeginFrame.Remove(BeginFrameHandle);
        BeginFrameHandle.Reset();
    }

    if (EndFrameHandle.IsValid())
    {
        FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
        EndFrameHandle.Reset();
    }

    Super::Deinitialize();
}

void URshipFrameSyncSubsystem::Configure(const FRshipFrameSyncConfig& InConfig)
{
    Config = InConfig;
    FrameDurationSeconds = Config.ExpectedFrameRate.AsDecimal() > 0.0 ? 1.0 / Config.ExpectedFrameRate.AsDecimal() : 0.0;
    TrimHistory();
    ApplyFixedFrameRate();
}

void URshipFrameSyncSubsystem::PushPTPTimestamp(const FRshipPTPTimestamp& Timestamp)
{
    LastTimestamp = Timestamp;
    ReferencePTPSeconds = Timestamp.AsSeconds();
    ReferenceFrameNumber = Timestamp.FrameNumber;
    UE_LOG(LogTemp, Display, TEXT("PTP Timestamp received: Frame %lld at %.9f"), ReferenceFrameNumber, ReferencePTPSeconds);
}

FRshipFrameSyncStatus URshipFrameSyncSubsystem::GetFrameSyncStatus() const
{
    FRshipFrameSyncStatus Status;
    Status.bIsLocked = FMath::Abs(LastFrameErrorMicros) <= Config.AllowableDriftMicroseconds;
    Status.DriftMicroseconds = LastFrameErrorMicros;
    Status.ReferenceFrameNumber = ReferenceFrameNumber;
    Status.ReferencePTPTimeSeconds = ReferencePTPSeconds;
    Status.LastTimestamp = LastTimestamp;

    if (Config.bRecordHistory)
    {
        Status.RecentHistory = History;
    }

    return Status;
}

void URshipFrameSyncSubsystem::ResetFrameHistory()
{
    History.Reset();
    LastFrameErrorMicros = 0.0;
}

void URshipFrameSyncSubsystem::HandleBeginFrame()
{
    const int64 CurrentFrameNumber = GFrameCounter;
    const double LocalSeconds = FPlatformTime::Seconds();

    double ExpectedSeconds = ReferencePTPSeconds;
    if (ReferenceFrameNumber > 0 && FrameDurationSeconds > 0.0)
    {
        ExpectedSeconds += static_cast<double>(CurrentFrameNumber - ReferenceFrameNumber) * FrameDurationSeconds;
    }
    else
    {
        ExpectedSeconds = LocalSeconds;
    }

    const double ErrorSeconds = LocalSeconds - ExpectedSeconds;
    LastFrameErrorMicros = ErrorSeconds * 1e6;

    if (Config.bRecordHistory)
    {
        FRshipFrameTimingRecord Record;
        Record.FrameNumber = CurrentFrameNumber;
        Record.LocalFrameStartSeconds = LocalSeconds;
        Record.ExpectedFrameStartSeconds = ExpectedSeconds;
        Record.ErrorMicroseconds = LastFrameErrorMicros;
        History.Add(Record);
        TrimHistory();
    }

    if (Config.AllowableDriftMicroseconds > 0.0f && FMath::Abs(LastFrameErrorMicros) > Config.AllowableDriftMicroseconds)
    {
        UE_LOG(LogTemp, Warning, TEXT("Frame %lld drifted %.3f microseconds from PTP schedule"), CurrentFrameNumber, LastFrameErrorMicros);
    }
}

void URshipFrameSyncSubsystem::HandleEndFrame()
{
    // Reserved for future metrics such as GPU fence capture.
}

void URshipFrameSyncSubsystem::TrimHistory()
{
    if (!Config.bRecordHistory)
    {
        History.Reset();
        return;
    }

    const int32 MaxEntries = FMath::Max(Config.HistorySize, 0);
    while (History.Num() > MaxEntries)
    {
        History.RemoveAt(0);
    }
}

void URshipFrameSyncSubsystem::ApplyFixedFrameRate() const
{
    if (Config.bUseFixedFrameRate && FrameDurationSeconds > 0.0)
    {
        FApp::SetUseFixedTimeStep(true);
        FApp::SetFixedDeltaTime(FrameDurationSeconds);
    }
    else
    {
        FApp::SetUseFixedTimeStep(false);
    }
}

