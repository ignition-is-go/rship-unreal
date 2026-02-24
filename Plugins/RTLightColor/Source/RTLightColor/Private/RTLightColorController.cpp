#include "RTLightColorController.h"

URTLightColorController::URTLightColorController()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URTLightColorController::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("RTLightColorController: Driving %d lights (Grid: %dx%d)"), Lights.Num(), GridWidth, GridHeight);
}

void URTLightColorController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ColorRenderTarget || Lights.Num() == 0)
	{
		return;
	}

	FRenderTarget* RT = ColorRenderTarget->GameThread_GetRenderTargetResource();
	if (!RT)
	{
		return;
	}

	RT->ReadPixels(CachedPixels);

	const int32 RTWidth = ColorRenderTarget->SizeX;
	const int32 RTHeight = ColorRenderTarget->SizeY;
	const int32 PixelCount = CachedPixels.Num();
	const int32 LightCount = Lights.Num();

	for (int32 i = 0; i < LightCount; i++)
	{
		if (!Lights[i])
		{
			continue;
		}

		const int32 GridCol = i % GridWidth;
		const int32 GridRow = i / GridWidth;

		const int32 PixelX = GridCol * RTWidth / GridWidth;
		const int32 PixelY = GridRow * RTHeight / GridHeight;
		const int32 PixelIndex = PixelY * RTWidth + PixelX;

		if (PixelIndex >= 0 && PixelIndex < PixelCount)
		{
			Lights[i]->SetLightColor(FLinearColor(CachedPixels[PixelIndex]));
		}
	}
}
