// Optional per-actor overrides for content mapping surfaces

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipContentMappingTargetComponent.generated.h"

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Content Mapping Target"))
class RSHIPEXEC_API URshipContentMappingTargetComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ContentMapping")
    FString MeshComponentNameOverride;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ContentMapping")
    TArray<int32> MaterialSlotsOverride;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ContentMapping")
    int32 UVChannelOverride = -1;
};
