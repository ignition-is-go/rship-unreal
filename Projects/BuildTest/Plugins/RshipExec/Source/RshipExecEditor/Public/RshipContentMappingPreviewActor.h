// Editor-only actor to visualize content mapping projectors/frustums

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ArrowComponent.h"
#include "RshipContentMappingPreviewActor.generated.h"

UCLASS(NotPlaceable)
class RSHIPEXECEDITOR_API ARshipContentMappingPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    ARshipContentMappingPreviewActor();

    UPROPERTY(VisibleAnywhere, Category = "Rship|ContentMapping")
    USceneComponent* Root = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Rship|ContentMapping")
    UArrowComponent* Arrow = nullptr;

    UPROPERTY(EditAnywhere, Category = "Rship|ContentMapping")
    FVector ProjectorPosition;

    UPROPERTY(EditAnywhere, Category = "Rship|ContentMapping")
    FRotator ProjectorRotation;

    UPROPERTY(EditAnywhere, Category = "Rship|ContentMapping")
    float FOV = 60.f;

    UPROPERTY(EditAnywhere, Category = "Rship|ContentMapping")
    float Aspect = 1.7778f;

    UPROPERTY(EditAnywhere, Category = "Rship|ContentMapping")
    float NearClip = 10.f;

    UPROPERTY(EditAnywhere, Category = "Rship|ContentMapping")
    float FarClip = 10000.f;

    UPROPERTY(EditAnywhere, Category = "Rship|ContentMapping")
    FColor LineColor = FColor::Cyan;

    virtual void Tick(float DeltaSeconds) override;
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }
};
