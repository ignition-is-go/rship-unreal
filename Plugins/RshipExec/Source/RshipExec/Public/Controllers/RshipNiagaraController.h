// Rship Niagara VFX Controller
// Action-driven Niagara VFX controller

#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "RshipNiagaraController.generated.h"


// ============================================================================
// NIAGARA CONTROLLER COMPONENT
// ============================================================================

/**
 * Comprehensive controller for Niagara VFX systems to rship.
 * Exposes all common particle system controls:
 * - Spawn Rate, Lifetime, Size, Velocity
 * - Color, Emissive, Opacity
 * - User parameters (generic float/vector/color)
 * - System control (activate, deactivate, reset, burst)
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Niagara Controller"))
class RSHIPEXEC_API URshipNiagaraController : public URshipControllerComponent
{
    GENERATED_BODY()

public:
    URshipNiagaraController();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** Child target suffix used for Niagara controls (defaults to "niagara") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    FString ChildTargetSuffix = TEXT("niagara");

    /** The Niagara component to drive (auto-found if not set) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    UNiagaraComponent* NiagaraComponent;

    /** Publish rate in Hz (how often to publish VFX state as emitters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (ClampMin = "1", ClampMax = "60"))
    int32 PublishRateHz = 10;

    /** Only publish when values change (reduces network traffic) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    bool bOnlyPublishOnChange = true;

    // ========================================================================
    //  ACTIONS - Generic Parameter Control
    // ========================================================================

    /** Set any float parameter by name */
    UFUNCTION()
    void SetFloatParameter(FName ParameterName, float Value);

    /** Set any vector parameter by name */
    UFUNCTION()
    void SetVectorParameter(FName ParameterName, float X, float Y, float Z);

    /** Set any color parameter by name */
    UFUNCTION()
    void SetColorParameter(FName ParameterName, float R, float G, float B, float A);

    /** Set any int parameter by name */
    UFUNCTION()
    void SetIntParameter(FName ParameterName, int32 Value);

    /** Set any bool parameter by name */
    UFUNCTION()
    void SetBoolParameter(FName ParameterName, bool Value);

    // ========================================================================
    //  ACTIONS - Spawn Control
    // ========================================================================

    /** Set spawn rate multiplier (1.0 = default, 0.0 = no spawning) */
    UFUNCTION()
    void SetSpawnRate(float Rate);

    /** Set spawn rate as absolute particles per second */
    UFUNCTION()
    void SetSpawnRateAbsolute(float ParticlesPerSecond);

    /** Trigger a burst of particles */
    UFUNCTION()
    void TriggerBurst(int32 Count);

    /** Set burst count for triggered bursts */
    UFUNCTION()
    void SetBurstCount(int32 Count);

    // ========================================================================
    //  ACTIONS - Particle Properties
    // ========================================================================

    /** Set particle lifetime multiplier */
    UFUNCTION()
    void SetLifetime(float Lifetime);

    /** Set particle size/scale multiplier */
    UFUNCTION()
    void SetSize(float Size);

    /** Set particle size as XYZ scale */
    UFUNCTION()
    void SetSizeXYZ(float X, float Y, float Z);

    /** Set particle velocity multiplier */
    UFUNCTION()
    void SetVelocity(float Velocity);

    /** Set particle velocity as direction vector */
    UFUNCTION()
    void SetVelocityXYZ(float X, float Y, float Z);

    /** Set particle mass */
    UFUNCTION()
    void SetMass(float Mass);

    /** Set particle drag coefficient */
    UFUNCTION()
    void SetDrag(float Drag);

    /** Set gravity multiplier */
    UFUNCTION()
    void SetGravity(float Gravity);

    // ========================================================================
    //  ACTIONS - Visual Properties
    // ========================================================================

    /** Set particle color */
    UFUNCTION()
    void SetColor(float R, float G, float B);

    /** Set particle color with alpha */
    UFUNCTION()
    void SetColorWithAlpha(float R, float G, float B, float A);

    /** Set emissive/glow intensity */
    UFUNCTION()
    void SetEmissive(float Intensity);

    /** Set emissive color with intensity */
    UFUNCTION()
    void SetEmissiveColor(float R, float G, float B, float Intensity);

    /** Set particle opacity */
    UFUNCTION()
    void SetOpacity(float Opacity);

    /** Set sprite rotation */
    UFUNCTION()
    void SetSpriteRotation(float Degrees);

    /** Set sprite alignment */
    UFUNCTION()
    void SetSpriteSize(float Width, float Height);

    // ========================================================================
    //  ACTIONS - System Control
    // ========================================================================

    /** Activate the Niagara system */
    UFUNCTION()
    void Activate();

    /** Deactivate the Niagara system */
    UFUNCTION()
    void Deactivate();

    /** Reset and restart the Niagara system */
    UFUNCTION()
    void Reset();

    /** Pause the Niagara system */
    UFUNCTION()
    void Pause();

    /** Resume the Niagara system */
    UFUNCTION()
    void Resume();

    /** Set system age (seek to time position) */
    UFUNCTION()
    void SetAge(float Age);

    /** Set overall intensity multiplier (affects spawn, size, emissive) */
    UFUNCTION()
    void SetGlobalIntensity(float Intensity);

    // ========================================================================
    //  ACTIONS - Transform
    // ========================================================================

    /** Set emitter world location */
    UFUNCTION()
    void SetLocation(float X, float Y, float Z);

    /** Set emitter world rotation (degrees) */
    UFUNCTION()
    void SetRotation(float Pitch, float Yaw, float Roll);

    /** Set emitter world scale */
    UFUNCTION()
    void SetScale(float Scale);

    /** Set emitter world scale XYZ */
    UFUNCTION()
    void SetScaleXYZ(float X, float Y, float Z);

    // ========================================================================
    //  EMITTERS - State Publishing
    // ========================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFloatEmitter, float, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVectorEmitter, float, X, float, Y, float, Z);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FColorEmitter, float, R, float, G, float, B);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBoolEmitter, bool, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIntEmitter, int32, Value);

    // System state emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FBoolEmitter OnActiveChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FIntEmitter OnParticleCountChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FFloatEmitter OnAgeChanged;

    // Property emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FFloatEmitter OnSpawnRateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FFloatEmitter OnLifetimeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FFloatEmitter OnSizeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FFloatEmitter OnVelocityChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FColorEmitter OnColorChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FFloatEmitter OnEmissiveChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FFloatEmitter OnGlobalIntensityChanged;

    // Transform emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FVectorEmitter OnLocationChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FVectorEmitter OnRotationChanged;

    // ========================================================================
    // PUBLIC METHODS
    // ========================================================================

    /** Manually set a color parameter directly (without action routing). */
    UFUNCTION()
    void SetColorValue(FName ParameterName, FLinearColor Color);

    /** Force publish all current values */
    UFUNCTION()
    void ForcePublish();

    /** Get current VFX state as JSON */
    UFUNCTION()
    FString GetNiagaraStateJson() const;

private:
    virtual void RegisterOrRefreshTarget() override;

    // State tracking for change detection and emitter publishing
    double LastPublishTime = 0.0;
    double PublishInterval = 0.1;
    float GlobalIntensityMultiplier = 1.0f;
    float LastSpawnRate = 1.0f;
    float LastLifetime = 1.0f;
    float LastSize = 1.0f;
    float LastVelocity = 1.0f;
    float LastEmissive = 1.0f;
    FLinearColor LastColor = FLinearColor::White;
    FVector LastLocation = FVector::ZeroVector;
    FRotator LastRotation = FRotator::ZeroRotator;
    bool bLastActive = false;
    int32 LastParticleCount = 0;

    void ReadAndPublishState();
    bool HasValueChanged(float OldValue, float NewValue, float Threshold = 0.001f) const;
    bool HasColorChanged(const FLinearColor& OldColor, const FLinearColor& NewColor, float Threshold = 0.001f) const;
};
