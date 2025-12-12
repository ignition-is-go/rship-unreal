// Rship Pulse Applicator Implementation

#include "RshipPulseApplicator.h"
#include "RshipSubsystem.h"
#include "RshipFixtureManager.h"
#include "RshipPulseReceiver.h"
#include "Logs.h"
#include "Components/LightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"

URshipPulseApplicator::URshipPulseApplicator()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;  // Only tick when smoothing
}

void URshipPulseApplicator::BeginPlay()
{
    Super::BeginPlay();

    // Get subsystem
    if (GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    }

    if (Subsystem)
    {
        PulseReceiver = Subsystem->GetPulseReceiver();
        FixtureManager = Subsystem->GetFixtureManager();
    }

    // Find light component if not set
    FindLightComponent();

    // Load calibration
    RefreshCalibration();

    // Auto-subscribe if enabled
    if (bAutoSubscribe && !FixtureId.IsEmpty())
    {
        Subscribe();
    }
}

void URshipPulseApplicator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Unsubscribe();
    Super::EndPlay(EndPlayReason);
}

void URshipPulseApplicator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Only tick if smoothing is active
    if (SmoothingFactor > 0.0f)
    {
        UpdateSmoothing(DeltaTime);
        ApplyToLight();
    }
}

void URshipPulseApplicator::FindLightComponent()
{
    if (TargetLight)
    {
        return;  // Already set
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Try to find a light component
    TargetLight = Owner->FindComponentByClass<USpotLightComponent>();
    if (!TargetLight)
    {
        TargetLight = Owner->FindComponentByClass<UPointLightComponent>();
    }
    if (!TargetLight)
    {
        TargetLight = Owner->FindComponentByClass<ULightComponent>();
    }

    if (TargetLight)
    {
        UE_LOG(LogRshipExec, Log, TEXT("PulseApplicator: Found light component %s"), *TargetLight->GetName());
    }
}

void URshipPulseApplicator::RefreshCalibration()
{
    bHasCalibration = false;

    if (!FixtureManager || FixtureId.IsEmpty())
    {
        return;
    }

    // Try to get calibration for this fixture
    if (FixtureManager->GetCalibrationForFixture(FixtureId, CachedCalibration))
    {
        bHasCalibration = true;
        UE_LOG(LogRshipExec, Log, TEXT("PulseApplicator: Loaded calibration for fixture %s"), *FixtureId);
    }
}

void URshipPulseApplicator::Subscribe()
{
    if (bIsSubscribed || !PulseReceiver || FixtureId.IsEmpty())
    {
        return;
    }

    // Subscribe to pulses for this fixture
    PulseReceiver->SubscribeToFixture(FixtureId);

    // Bind to pulse events (dynamic delegate requires AddDynamic)
    PulseReceiver->OnFixturePulseReceived.AddDynamic(this, &URshipPulseApplicator::OnPulseReceived);

    bIsSubscribed = true;

    // Enable tick if smoothing is active
    if (SmoothingFactor > 0.0f)
    {
        SetComponentTickEnabled(true);
    }

    UE_LOG(LogRshipExec, Log, TEXT("PulseApplicator: Subscribed to fixture %s"), *FixtureId);
}

void URshipPulseApplicator::Unsubscribe()
{
    if (!bIsSubscribed)
    {
        return;
    }

    if (PulseReceiver)
    {
        PulseReceiver->OnFixturePulseReceived.RemoveDynamic(this, &URshipPulseApplicator::OnPulseReceived);
        PulseReceiver->UnsubscribeFromFixture(FixtureId);
    }

    bIsSubscribed = false;
    SetComponentTickEnabled(false);

    UE_LOG(LogRshipExec, Log, TEXT("PulseApplicator: Unsubscribed from fixture %s"), *FixtureId);
}

void URshipPulseApplicator::OnPulseReceived(const FString& InFixtureId, const FRshipFixturePulse& Pulse)
{
    // Only process pulses for our fixture
    if (InFixtureId != FixtureId)
    {
        return;
    }

    ApplyPulse(Pulse);
}

void URshipPulseApplicator::ApplyPulse(const FRshipFixturePulse& Pulse)
{
    LastPulse = Pulse;

    // Calculate target intensity
    if (Pulse.bHasIntensity)
    {
        float RawIntensity = Pulse.Intensity;

        // Apply calibration if available
        if (bApplyCalibration && bHasCalibration)
        {
            RawIntensity = ApplyDimmerCurve(RawIntensity);
        }

        TargetIntensity = RawIntensity * MaxIntensity;
    }

    // Calculate target color
    if (Pulse.bHasColor)
    {
        TargetColor = Pulse.Color;
    }
    else if (Pulse.bHasColorTemperature)
    {
        TargetColor = ApplyColorTemperature(Pulse.ColorTemperature);
    }

    // Calculate target zoom
    if (Pulse.bHasZoom)
    {
        TargetZoom = Pulse.Zoom;
    }

    // Apply immediately if no smoothing
    if (SmoothingFactor <= 0.0f)
    {
        CurrentIntensity = TargetIntensity;
        CurrentColor = TargetColor;
        CurrentZoom = TargetZoom;
        ApplyToLight();
    }

    // Broadcast event
    OnPulseApplied.Broadcast(Pulse);
}

float URshipPulseApplicator::ApplyDimmerCurve(float RawIntensity) const
{
    if (!bHasCalibration || CachedCalibration.DimmerCurve.Num() < 2)
    {
        return RawIntensity;  // Linear fallback
    }

    // Interpolate through the dimmer curve
    // DimmerCurve is an array of FRshipDimmerCurvePoint where DmxValue = input (0-255), OutputPercent = output (0-1)
    const TArray<FRshipDimmerCurvePoint>& Curve = CachedCalibration.DimmerCurve;

    // Convert RawIntensity (0-1) to DMX scale (0-255) for lookup
    float DmxInput = RawIntensity * 255.0f;

    // Find surrounding points
    int32 LowerIdx = 0;
    for (int32 i = 0; i < Curve.Num() - 1; i++)
    {
        if (Curve[i + 1].DmxValue >= DmxInput)
        {
            LowerIdx = i;
            break;
        }
        LowerIdx = i;
    }

    int32 UpperIdx = FMath::Min(LowerIdx + 1, Curve.Num() - 1);

    // Handle edge cases
    if (LowerIdx == UpperIdx)
    {
        return Curve[LowerIdx].OutputPercent;
    }

    // Interpolate
    float T = (DmxInput - Curve[LowerIdx].DmxValue) / (float)(Curve[UpperIdx].DmxValue - Curve[LowerIdx].DmxValue);
    T = FMath::Clamp(T, 0.0f, 1.0f);

    return FMath::Lerp(Curve[LowerIdx].OutputPercent, Curve[UpperIdx].OutputPercent, T);
}

FLinearColor URshipPulseApplicator::ApplyColorTemperature(float Kelvin) const
{
    // Convert color temperature to RGB using approximation
    // Based on algorithm by Tanner Helland

    float Temperature = Kelvin / 100.0f;
    float Red, Green, Blue;

    // Red
    if (Temperature <= 66.0f)
    {
        Red = 1.0f;
    }
    else
    {
        Red = Temperature - 60.0f;
        Red = 329.698727446f * FMath::Pow(Red, -0.1332047592f);
        Red = FMath::Clamp(Red / 255.0f, 0.0f, 1.0f);
    }

    // Green
    if (Temperature <= 66.0f)
    {
        Green = Temperature;
        Green = 99.4708025861f * FMath::Loge(Green) - 161.1195681661f;
        Green = FMath::Clamp(Green / 255.0f, 0.0f, 1.0f);
    }
    else
    {
        Green = Temperature - 60.0f;
        Green = 288.1221695283f * FMath::Pow(Green, -0.0755148492f);
        Green = FMath::Clamp(Green / 255.0f, 0.0f, 1.0f);
    }

    // Blue
    if (Temperature >= 66.0f)
    {
        Blue = 1.0f;
    }
    else if (Temperature <= 19.0f)
    {
        Blue = 0.0f;
    }
    else
    {
        Blue = Temperature - 10.0f;
        Blue = 138.5177312231f * FMath::Loge(Blue) - 305.0447927307f;
        Blue = FMath::Clamp(Blue / 255.0f, 0.0f, 1.0f);
    }

    return FLinearColor(Red, Green, Blue, 1.0f);
}

void URshipPulseApplicator::ApplyToLight()
{
    if (!TargetLight)
    {
        return;
    }

    // Apply intensity
    TargetLight->SetIntensity(CurrentIntensity);

    // Apply color
    TargetLight->SetLightColor(CurrentColor);

    // Apply zoom/cone angle for spot lights
    if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(TargetLight))
    {
        if (bHasCalibration)
        {
            // Interpolate between beam and field angle based on zoom
            // Use multipliers applied to default angles (assuming 25/35 degree defaults)
            float MinAngle = 25.0f * CachedCalibration.BeamAngleMultiplier;
            float MaxAngle = 35.0f * CachedCalibration.FieldAngleMultiplier;

            float OuterAngle = FMath::Lerp(MinAngle, MaxAngle, CurrentZoom);
            float InnerAngle = OuterAngle * 0.7f;  // Inner cone is ~70% of outer

            SpotLight->SetInnerConeAngle(InnerAngle);
            SpotLight->SetOuterConeAngle(OuterAngle);
        }
    }
}

void URshipPulseApplicator::UpdateSmoothing(float DeltaTime)
{
    // Exponential smoothing
    float Alpha = 1.0f - FMath::Pow(SmoothingFactor, DeltaTime * 60.0f);  // Normalize to 60fps

    CurrentIntensity = FMath::Lerp(CurrentIntensity, TargetIntensity, Alpha);
    CurrentColor = FLinearColor::LerpUsingHSV(CurrentColor, TargetColor, Alpha);
    CurrentZoom = FMath::Lerp(CurrentZoom, TargetZoom, Alpha);
}
