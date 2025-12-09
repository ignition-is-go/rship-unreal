// Rship Fixture Actor
// Visualizes a fixture with calibration-accurate rendering

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RshipCalibrationTypes.h"
#include "RshipIESProfileService.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureLightProfile.h"
#include "RshipFixtureActor.generated.h"

class URshipSubsystem;
class URshipFixtureManager;
class URshipIESProfileService;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFixtureDMXUpdated, const TMap<FString, float>&, DMXValues);

/**
 * Actor that visualizes a fixture from rship with calibration-accurate rendering.
 * Automatically subscribes to fixture data and DMX state via pulses.
 */
UCLASS(BlueprintType, Blueprintable)
class RSHIPEXEC_API ARshipFixtureActor : public AActor
{
    GENERATED_BODY()

public:
    ARshipFixtureActor();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** The rship fixture ID to visualize */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Fixture")
    FString FixtureId;

    /** Auto-sync position/rotation from rship fixture entity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Fixture")
    bool bSyncTransformFromServer = true;

    /** Show debug visualization (beam cone, DMX values) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Fixture")
    bool bShowDebugVisualization = false;

    /** Scale factor for position (rship units to UE units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Fixture")
    float PositionScale = 100.0f;  // cm per meter

    // ========================================================================
    // COMPONENTS
    // ========================================================================

    /** Root scene component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootSceneComponent;

    /** Optional body mesh (can be set in Blueprint) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* BodyMesh;

    /** Spot light for beam visualization */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USpotLightComponent* BeamLight;

    // ========================================================================
    // RUNTIME STATE
    // ========================================================================

    /** Current fixture info from server */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixture")
    FRshipFixtureInfo CachedFixtureInfo;

    /** Current fixture type info */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixture")
    FRshipFixtureTypeInfo CachedFixtureType;

    /** Current calibration data */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixture")
    FRshipFixtureCalibration CachedCalibration;

    /** Cached IES profile data */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixture")
    FRshipIESProfile CachedIESProfile;

    /** IES light profile texture for accurate beam distribution */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixture")
    UTextureLightProfile* IESLightProfileTexture;

    /** Whether IES profile has been loaded */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixture")
    bool bHasIESProfile = false;

    /** Current DMX channel values (channel name -> value 0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixture")
    TMap<FString, float> CurrentDMXValues;

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Called when DMX values are updated */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Fixture")
    FOnFixtureDMXUpdated OnDMXUpdated;

    // ========================================================================
    // BLUEPRINT CALLABLE
    // ========================================================================

    /** Manually refresh fixture data from server */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    void RefreshFixtureData();

    /** Get the current dimmer output (0-1) after calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    float GetCalibratedDimmerOutput() const;

    /** Get the current color after calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    FLinearColor GetCalibratedColor() const;

    /** Get beam angle after calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    float GetCalibratedBeamAngle() const;

    /** Get field angle after calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    float GetCalibratedFieldAngle() const;

    /** Set a DMX channel value directly (for testing/preview) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    void SetDMXChannel(const FString& ChannelName, float Value);

    /** Get current intensity DMX value (0-255) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    int32 GetDMXIntensity() const;

    /** Check if fixture has an IES profile loaded */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    bool HasIESProfile() const { return bHasIESProfile; }

    /** Get beam angle from IES profile (or fallback to calibrated value) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    float GetIESBeamAngle() const;

    /** Get field angle from IES profile (or fallback to calibrated value) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    float GetIESFieldAngle() const;

    /** Get intensity at a specific vertical angle from IES profile (0=down, 90=horizontal) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    float GetIESIntensityAtAngle(float VerticalAngle, float HorizontalAngle = 0.0f) const;

    /** Force reload of IES profile */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixture")
    void ReloadIESProfile();

protected:
    /** Called when fixture data is updated from server */
    UFUNCTION(BlueprintNativeEvent, Category = "Rship|Fixture")
    void OnFixtureDataUpdated();

    /** Called when calibration data is updated */
    UFUNCTION(BlueprintNativeEvent, Category = "Rship|Fixture")
    void OnCalibrationUpdated();

    /** Called when IES profile is loaded */
    UFUNCTION(BlueprintNativeEvent, Category = "Rship|Fixture")
    void OnIESProfileLoaded();

    /** Update light component based on current state */
    virtual void UpdateLightVisualization();

    /** Apply transform from fixture entity */
    virtual void ApplyServerTransform();

    /** Apply IES profile texture to light component */
    virtual void ApplyIESProfile();

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    URshipFixtureManager* FixtureManager;

    // Cached raw DMX intensity (0-255)
    int32 RawDMXIntensity = 0;

    // Cached color temperature
    float CurrentColorTemp = 6500.0f;

    // Delegate handles for cleanup
    FDelegateHandle FixtureUpdateHandle;
    FDelegateHandle CalibrationUpdateHandle;

    void BindToManager();
    void UnbindFromManager();

    void OnFixturesUpdatedInternal();
    void OnCalibrationUpdatedInternal(const FRshipFixtureCalibration& Calibration);

    void LoadIESProfile();
    void OnIESProfileLoadedInternal(const FRshipIESProfile& Profile, bool bSuccess);

    // URL of the currently loaded or loading IES profile
    FString LoadedIESProfileUrl;
};
