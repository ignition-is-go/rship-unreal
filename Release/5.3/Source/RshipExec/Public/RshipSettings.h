#pragma once

#include "CoreMinimal.h"
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
};