#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipFieldSamplerComponent.generated.h"

UCLASS(Abstract, ClassGroup = (Rship))
class RSHIPFIELD_API URshipFieldSamplerComponent : public URshipControllerComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString ChildTargetSuffix = TEXT("fieldSampler");
};
