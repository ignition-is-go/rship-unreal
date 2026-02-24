#include "RTLightColorController.h"

URTLightColorController::URTLightColorController()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URTLightColorController::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("RTLightColorController: Driving %d lights"), Lights.Num());
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

	const int32 Count = FMath::Min(CachedPixels.Num(), Lights.Num());
	for (int32 i = 0; i < Count; i++)
	{
		Lights[i]->SetLightColor(FLinearColor(CachedPixels[i]));
	}
}
