#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "FrameRate.h"
#include "RshipFrameSyncSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FRshipPTPTimestamp
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    int64 Seconds = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    int32 Nanoseconds = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    int64 FrameNumber = 0;

    double AsSeconds() const
    {
        return static_cast<double>(Seconds) + static_cast<double>(Nanoseconds) * 1e-9;
    }
};

USTRUCT(BlueprintType)
struct FRshipFrameSyncConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    bool bUseFixedFrameRate = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    FFrameRate ExpectedFrameRate = FFrameRate(60, 1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    float AllowableDriftMicroseconds = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    bool bRecordHistory = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    int32 HistorySize = 120;
};

USTRUCT(BlueprintType)
struct FRshipFrameTimingRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    int64 FrameNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    double LocalFrameStartSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    double ExpectedFrameStartSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    double ErrorMicroseconds = 0.0;
};

USTRUCT(BlueprintType)
struct FRshipFrameSyncStatus
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    bool bIsLocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    double DriftMicroseconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    int64 ReferenceFrameNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    double ReferencePTPTimeSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    FRshipPTPTimestamp LastTimestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|FrameSync")
    TArray<FRshipFrameTimingRecord> RecentHistory;
};

UCLASS()
class RSHIPEXEC_API URshipFrameSyncSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Rship|FrameSync")
    void Configure(const FRshipFrameSyncConfig& InConfig);

    UFUNCTION(BlueprintCallable, Category = "Rship|FrameSync")
    void PushPTPTimestamp(const FRshipPTPTimestamp& Timestamp);

    UFUNCTION(BlueprintCallable, Category = "Rship|FrameSync")
    FRshipFrameSyncStatus GetFrameSyncStatus() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|FrameSync")
    void ResetFrameHistory();

private:
    void HandleBeginFrame();
    void HandleEndFrame();
    void TrimHistory();
    void ApplyFixedFrameRate() const;

    FRshipFrameSyncConfig Config;
    FRshipPTPTimestamp LastTimestamp;
    double ReferencePTPSeconds = 0.0;
    int64 ReferenceFrameNumber = 0;
    double FrameDurationSeconds = 1.0 / 60.0;
    TArray<FRshipFrameTimingRecord> History;
    double LastFrameErrorMicros = 0.0;
    FDelegateHandle BeginFrameHandle;
    FDelegateHandle EndFrameHandle;
};

