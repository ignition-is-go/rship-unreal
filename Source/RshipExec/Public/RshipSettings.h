#pragma once

#include "CoreMinimal.h"
#include "FrameRate.h"
#include "UObject/Object.h"
#include "RshipSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Rocketship Settings"))
class URshipSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, config, Category = "RshipExec", meta = (DisplayName = "Rocketship Host"))
    FString rshipHostAddress;

    UPROPERTY(EditAnywhere, config, Category = "RshipExec", meta = (DisplayName = "Service Color"))
    FLinearColor ServiceColor = FLinearColor::Gray;

    UPROPERTY(EditAnywhere, config, Category = "RshipExec|Frame Sync", meta = (DisplayName = "Enable PTP Frame Sync"))
    bool bEnableFrameSync = true;

    UPROPERTY(EditAnywhere, config, Category = "RshipExec|Frame Sync", meta = (DisplayName = "Target Frame Rate"))
    FFrameRate TargetFrameRate = FFrameRate(60, 1);

    UPROPERTY(EditAnywhere, config, Category = "RshipExec|Frame Sync", meta = (DisplayName = "Allowable Drift (microseconds)"))
    float AllowableDriftMicroseconds = 500.0f;

    UPROPERTY(EditAnywhere, config, Category = "RshipExec|Frame Sync", meta = (DisplayName = "Frame History Length"))
    int32 FrameHistoryLength = 240;
};