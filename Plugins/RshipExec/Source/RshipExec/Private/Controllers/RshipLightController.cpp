#include "Controllers/RshipLightController.h"

#include "RshipTargetComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/DirectionalLightComponent.h"

namespace
{
	void RequestLightControllerRescan(AActor* Owner, const bool bOnlyIfRegistered)
	{
		if (!Owner)
		{
			return;
		}

		if (URshipTargetComponent* TargetComponent = Owner->FindComponentByClass<URshipTargetComponent>())
		{
			if (!bOnlyIfRegistered || TargetComponent->IsRegistered())
			{
				TargetComponent->RescanSiblingComponents();
			}
		}
	}
}

void URshipLightController::OnRegister()
{
	Super::OnRegister();
	RequestLightControllerRescan(GetOwner(), false);
}

void URshipLightController::BeginPlay()
{
	Super::BeginPlay();
	RequestLightControllerRescan(GetOwner(), false);
}

ULightComponent* URshipLightController::ResolveLightComponent() const
{
	if (AActor* Owner = GetOwner())
	{
		return Owner->FindComponentByClass<ULightComponent>();
	}

	return nullptr;
}

void URshipLightController::NotifyLightEdited(ULightComponent* InLight) const
{
	if (!InLight)
	{
		return;
	}

	InLight->MarkRenderStateDirty();

#if WITH_EDITOR
#endif
}

void URshipLightController::RegisterRshipWhitelistedActions(URshipTargetComponent* TargetComponent)
{
	if (!TargetComponent)
	{
		return;
	}

	ULightComponent* TargetLight = ResolveLightComponent();
	if (!TargetLight)
	{
		return;
	}

	if (bIncludeCommonProperties)
	{
		TargetComponent->RegisterWhitelistedProperty(TargetLight, TEXT("Intensity"));
		TargetComponent->RegisterWhitelistedProperty(TargetLight, TEXT("LightColor"));
		TargetComponent->RegisterWhitelistedProperty(TargetLight, TEXT("Temperature"));
		TargetComponent->RegisterWhitelistedProperty(TargetLight, TEXT("bUseTemperature"));
		TargetComponent->RegisterWhitelistedProperty(TargetLight, TEXT("CastShadows"));
		TargetComponent->RegisterWhitelistedProperty(TargetLight, TEXT("IndirectLightingIntensity"));
		TargetComponent->RegisterWhitelistedProperty(TargetLight, TEXT("VolumetricScatteringIntensity"));
		TargetComponent->RegisterWhitelistedProperty(TargetLight, TEXT("bAffectsWorld"));
	}

	if (!bIncludeTypeSpecificProperties)
	{
		return;
	}

	if (UPointLightComponent* Point = Cast<UPointLightComponent>(TargetLight))
	{
		TargetComponent->RegisterWhitelistedProperty(Point, TEXT("AttenuationRadius"));
		TargetComponent->RegisterWhitelistedProperty(Point, TEXT("SourceRadius"));
		TargetComponent->RegisterWhitelistedProperty(Point, TEXT("SoftSourceRadius"));
		TargetComponent->RegisterWhitelistedProperty(Point, TEXT("SourceLength"));
	}

	if (USpotLightComponent* Spot = Cast<USpotLightComponent>(TargetLight))
	{
		TargetComponent->RegisterWhitelistedProperty(Spot, TEXT("InnerConeAngle"));
		TargetComponent->RegisterWhitelistedProperty(Spot, TEXT("OuterConeAngle"));
	}

	if (URectLightComponent* Rect = Cast<URectLightComponent>(TargetLight))
	{
		TargetComponent->RegisterWhitelistedProperty(Rect, TEXT("SourceWidth"));
		TargetComponent->RegisterWhitelistedProperty(Rect, TEXT("SourceHeight"));
		TargetComponent->RegisterWhitelistedProperty(Rect, TEXT("BarnDoorAngle"));
		TargetComponent->RegisterWhitelistedProperty(Rect, TEXT("BarnDoorLength"));
	}

	if (UDirectionalLightComponent* Directional = Cast<UDirectionalLightComponent>(TargetLight))
	{
		TargetComponent->RegisterWhitelistedProperty(Directional, TEXT("LightSourceAngle"));
		TargetComponent->RegisterWhitelistedProperty(Directional, TEXT("LightSourceSoftAngle"));
		TargetComponent->RegisterWhitelistedProperty(Directional, TEXT("bUsedAsAtmosphereSunLight"));
	}
}

void URshipLightController::OnRshipAfterTake(URshipTargetComponent* TargetComponent, const FString& ActionName, UObject* ActionOwner)
{
	(void)TargetComponent;
	(void)ActionName;
	if (ActionOwner == ResolveLightComponent())
	{
		NotifyLightEdited(Cast<ULightComponent>(ActionOwner));
	}
}

