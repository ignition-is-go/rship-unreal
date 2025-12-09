// Rship Pulse Applicator Component
// Receives pulses from PulseReceiver and applies them to the owning fixture's light

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipPulseReceiver.h"
#include "RshipCalibrationTypes.h"
#include "RshipPulseApplicator.generated.h"

class URshipSubsystem;
class URshipFixtureManager;
class ULightComponent;
class USpotLightComponent;

/**
 * Component that subscribes to pulse data and applies it to light components.
 * Handles calibration application (dimmer curves, color temperature, etc.)
 */
UCLASS(ClassGroup=(Rship), meta=(BlueprintSpawnableComponent))
class RSHIPEXEC_API URshipPulseApplicator : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipPulseApplicator();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** The fixture ID to receive pulses for */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Pulse")
    FString FixtureId;

    /** Light component to control (auto-detected if not set) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Pulse")
    ULightComponent* TargetLight;

    /** Whether to apply calibration (dimmer curve, color correction) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Pulse")
    bool bApplyCalibration = true;

    /** Whether to auto-subscribe on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Pulse")
    bool bAutoSubscribe = true;

    /** Max intensity for the light (applied after calibration) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Pulse")
    float MaxIntensity = 100000.0f;  // Lumens

    /** Smoothing factor for value changes (0 = instant, 1 = very smooth) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Pulse", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float SmoothingFactor = 0.0f;

    // ========================================================================
    // RUNTIME CONTROL
    // ========================================================================

    /** Start receiving pulses */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    void Subscribe();

    /** Stop receiving pulses */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    void Unsubscribe();

    /** Check if currently subscribed */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    bool IsSubscribed() const { return bIsSubscribed; }

    /** Get the last received pulse */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    FRshipFixturePulse GetLastPulse() const { return LastPulse; }

    /** Get the current output intensity (after calibration) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    float GetCurrentIntensity() const { return CurrentIntensity; }

    /** Get the current output color (after calibration) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    FLinearColor GetCurrentColor() const { return CurrentColor; }

    /** Manually apply a pulse (useful for preview/testing) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    void ApplyPulse(const FRshipFixturePulse& Pulse);

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Called when a pulse is applied */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPulseApplied, const FRshipFixturePulse&, Pulse);

    UPROPERTY(BlueprintAssignable, Category = "Rship|Pulse")
    FOnPulseApplied OnPulseApplied;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    URshipPulseReceiver* PulseReceiver;

    UPROPERTY()
    URshipFixtureManager* FixtureManager;

    bool bIsSubscribed = false;
    FRshipFixturePulse LastPulse;

    // Cached calibration (refreshed on calibration change)
    FRshipFixtureCalibration CachedCalibration;
    bool bHasCalibration = false;

    // Current output values (may be smoothed)
    float CurrentIntensity = 0.0f;
    FLinearColor CurrentColor = FLinearColor::White;
    float CurrentZoom = 0.5f;

    // Target values (before smoothing)
    float TargetIntensity = 0.0f;
    FLinearColor TargetColor = FLinearColor::White;
    float TargetZoom = 0.5f;

    // Delegate handle for pulse events
    FDelegateHandle PulseReceivedHandle;

    // Find and cache the light component
    void FindLightComponent();

    // Refresh calibration data
    void RefreshCalibration();

    // Called when a pulse is received
    void OnPulseReceived(const FString& InFixtureId, const FRshipFixturePulse& Pulse);

    // Apply calibration to a raw intensity value
    float ApplyDimmerCurve(float RawIntensity) const;

    // Apply color temperature correction
    FLinearColor ApplyColorTemperature(float Kelvin) const;

    // Apply current values to the light component
    void ApplyToLight();

    // Lerp toward target values (for smoothing)
    void UpdateSmoothing(float DeltaTime);
};
