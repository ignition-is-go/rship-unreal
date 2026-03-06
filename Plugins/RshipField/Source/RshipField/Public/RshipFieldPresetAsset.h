#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RshipFieldTypes.h"
#include "RshipFieldPresetAsset.generated.h"

UCLASS(BlueprintType)
class RSHIPFIELD_API URshipFieldPresetAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    FRshipFieldGlobalParams Globals;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    FRshipFieldTransportState Transport;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    TArray<FRshipFieldLayerDesc> Layers;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    TArray<FRshipFieldPhaseGroupDesc> PhaseGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    TArray<FRshipFieldEmitterDesc> Emitters;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    TArray<FRshipFieldSplineEmitterDesc> Splines;
};
