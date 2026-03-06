#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PulseDeformCacheAsset.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class PULSERDGDEFORM_API UPulseDeformCacheAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Grid", meta = (ClampMin = "1"))
    int32 GridWidth = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Grid", meta = (ClampMin = "1"))
    int32 GridHeight = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Cache")
    TObjectPtr<UTexture2D> RestPositionTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Cache")
    TObjectPtr<UTexture2D> RestNormalTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse|Cache")
    TObjectPtr<UTexture2D> MaskTexture = nullptr;

    bool IsValidForDispatch() const
    {
        return GridWidth > 0
            && GridHeight > 0
            && RestPositionTexture != nullptr
            && RestNormalTexture != nullptr
            && MaskTexture != nullptr;
    }
};
