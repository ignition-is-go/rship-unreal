#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RshipSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Rocketship Settings"))
class URshipSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, config, Category = "RshipExec", meta = (DisplayName = "Rship Server Address"))
    FString rshipHostAddress = "localhost";

    UPROPERTY(EditAnywhere, config, Category = "RshipExec", meta = (DisplayName = "Rship Server Port"))
    int32 rshipServerPort = 5155;

    UPROPERTY(EditAnywhere, config, Category = "RshipExec", meta = (DisplayName = "Service Color"))
    FLinearColor ServiceColor = FLinearColor::Gray;
};