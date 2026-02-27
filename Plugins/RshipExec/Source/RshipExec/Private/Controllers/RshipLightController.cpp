#include "Controllers/RshipLightController.h"

#include "RshipSubsystem.h"
#include "GameFramework/Actor.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/DirectionalLightComponent.h"

ULightComponent* URshipLightController::ResolveLightComponent() const
{
	if (AActor* Owner = GetOwner())
	{
		return Owner->FindComponentByClass<ULightComponent>();
	}

	return nullptr;
}

void URshipLightController::RegisterOrRefreshTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner || !GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FRshipTargetProxy ParentIdentity = Subsystem->EnsureActorIdentity(Owner);
	if (!ParentIdentity.IsValid())
	{
		return;
	}

	const FString Suffix = ChildTargetSuffix.IsEmpty() ? TEXT("light") : ChildTargetSuffix;
	FRshipTargetProxy Target = ParentIdentity.AddTarget(Suffix, Suffix);
	if (!Target.IsValid())
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
		Target
			.AddPropertyAction(TargetLight, TEXT("Intensity"))
			.AddPropertyAction(TargetLight, TEXT("LightColor"))
			.AddPropertyAction(TargetLight, TEXT("Temperature"))
			.AddPropertyAction(TargetLight, TEXT("bUseTemperature"))
			.AddPropertyAction(TargetLight, TEXT("CastShadows"))
			.AddPropertyAction(TargetLight, TEXT("IndirectLightingIntensity"))
			.AddPropertyAction(TargetLight, TEXT("VolumetricScatteringIntensity"))
			.AddPropertyAction(TargetLight, TEXT("bAffectsWorld"));
	}

	if (!bIncludeTypeSpecificProperties)
	{
		return;
	}

	if (UPointLightComponent* Point = Cast<UPointLightComponent>(TargetLight))
	{
		Target
			.AddPropertyAction(Point, TEXT("AttenuationRadius"))
			.AddPropertyAction(Point, TEXT("SourceRadius"))
			.AddPropertyAction(Point, TEXT("SoftSourceRadius"))
			.AddPropertyAction(Point, TEXT("SourceLength"));
	}

	if (USpotLightComponent* Spot = Cast<USpotLightComponent>(TargetLight))
	{
		Target
			.AddPropertyAction(Spot, TEXT("InnerConeAngle"))
			.AddPropertyAction(Spot, TEXT("OuterConeAngle"));
	}

	if (URectLightComponent* Rect = Cast<URectLightComponent>(TargetLight))
	{
		Target
			.AddPropertyAction(Rect, TEXT("SourceWidth"))
			.AddPropertyAction(Rect, TEXT("SourceHeight"))
			.AddPropertyAction(Rect, TEXT("BarnDoorAngle"))
			.AddPropertyAction(Rect, TEXT("BarnDoorLength"));
	}

	if (UDirectionalLightComponent* Directional = Cast<UDirectionalLightComponent>(TargetLight))
	{
		Target
			.AddPropertyAction(Directional, TEXT("LightSourceAngle"))
			.AddPropertyAction(Directional, TEXT("LightSourceSoftAngle"))
			.AddPropertyAction(Directional, TEXT("bUsedAsAtmosphereSunLight"));
	}
}
