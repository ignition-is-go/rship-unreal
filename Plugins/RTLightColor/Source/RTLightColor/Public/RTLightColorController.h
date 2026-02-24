#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/RectLightComponent.h"
#include "RTLightColorController.generated.h"

UCLASS(ClassGroup = (RTLightColor), meta = (BlueprintSpawnableComponent))
class RTLIGHTCOLOR_API URTLightColorController : public UActorComponent
{
	GENERATED_BODY()

public:
	URTLightColorController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTLightColor")
	UTextureRenderTarget2D* ColorRenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTLightColor")
	TArray<URectLightComponent*> Lights;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TArray<FColor> CachedPixels;
};
