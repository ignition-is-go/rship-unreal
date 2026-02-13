// Rship Niagara VFX Binding
// Drive Niagara particle systems from rship pulse data

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Components/ActorComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "RshipPulseReceiver.h"
#include "RshipNiagaraBinding.generated.h"

class URshipSubsystem;

// ============================================================================
// BINDING TYPES
// ============================================================================

/** How to map pulse values to Niagara parameters */
UENUM(BlueprintType)
enum class ERshipNiagaraBindingMode : uint8
{
    Direct          UMETA(DisplayName = "Direct"),           // Value passed directly
    Normalized      UMETA(DisplayName = "Normalized"),       // Value normalized to 0-1
    Scaled          UMETA(DisplayName = "Scaled"),           // Value multiplied by scale factor
    Mapped          UMETA(DisplayName = "Mapped"),           // Value remapped from input to output range
    Curve           UMETA(DisplayName = "Curve"),            // Value passed through curve
    Trigger         UMETA(DisplayName = "Trigger")           // Boolean trigger on threshold
};

/** Single parameter binding configuration */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipNiagaraParameterBinding
{
    GENERATED_BODY()

    /** Name of the field in pulse data (e.g., "intensity", "color.r") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    FString PulseField;

    /** Name of the Niagara user parameter to set */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    FName NiagaraParameter;

    /** How to process the value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    ERshipNiagaraBindingMode Mode = ERshipNiagaraBindingMode::Direct;

    /** Scale factor for Scaled mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (EditCondition = "Mode == ERshipNiagaraBindingMode::Scaled"))
    float ScaleFactor = 1.0f;

    /** Input range min for Mapped mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (EditCondition = "Mode == ERshipNiagaraBindingMode::Mapped"))
    float InputMin = 0.0f;

    /** Input range max for Mapped mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (EditCondition = "Mode == ERshipNiagaraBindingMode::Mapped"))
    float InputMax = 1.0f;

    /** Output range min for Mapped mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (EditCondition = "Mode == ERshipNiagaraBindingMode::Mapped"))
    float OutputMin = 0.0f;

    /** Output range max for Mapped mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (EditCondition = "Mode == ERshipNiagaraBindingMode::Mapped"))
    float OutputMax = 1.0f;

    /** Curve for Curve mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (EditCondition = "Mode == ERshipNiagaraBindingMode::Curve"))
    FRuntimeFloatCurve ResponseCurve;

    /** Threshold for Trigger mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (EditCondition = "Mode == ERshipNiagaraBindingMode::Trigger"))
    float TriggerThreshold = 0.5f;

    /** Smoothing factor (0 = instant, 1 = very smooth) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float Smoothing = 0.0f;

    /** Whether this binding is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    bool bEnabled = true;

    // Runtime state
    float LastValue = 0.0f;
    float SmoothedValue = 0.0f;
};

/** Color binding for RGBA parameters */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipNiagaraColorBinding
{
    GENERATED_BODY()

    /** Field prefix for color (e.g., "color" for color.r, color.g, color.b) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    FString ColorFieldPrefix = TEXT("color");

    /** Niagara color parameter name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    FName NiagaraColorParameter;

    /** Multiply color by intensity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    bool bMultiplyByIntensity = true;

    /** Intensity field name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara", meta = (EditCondition = "bMultiplyByIntensity"))
    FString IntensityField = TEXT("intensity");

    /** Whether this binding is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Niagara")
    bool bEnabled = true;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNiagaraPulseReceived, const FString&, EmitterId, float, Intensity);

// ============================================================================
// NIAGARA BINDING COMPONENT
// ============================================================================

/**
 * Comprehensive binding for Niagara VFX systems to rship.
 * Exposes all common particle system controls:
 * - Spawn Rate, Lifetime, Size, Velocity
 * - Color, Emissive, Opacity
 * - User parameters (generic float/vector/color)
 * - System control (activate, deactivate, reset, burst)
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Niagara Binding"))
class RSHIPEXEC_API URshipNiagaraBinding : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipNiagaraBinding();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** The emitter ID to listen for (e.g., "targetId:emitterId") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    FString EmitterId;

    /** Alternatively, specify target and emitter separately */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    FString TargetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    FString EmitterName;

    /** The Niagara component to drive (auto-found if not set) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    UNiagaraComponent* NiagaraComponent;

    /** Float parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    TArray<FRshipNiagaraParameterBinding> FloatBindings;

    /** Color parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    TArray<FRshipNiagaraColorBinding> ColorBindings;

    /** Whether to auto-activate Niagara on first pulse */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    bool bAutoActivateOnPulse = true;

    /** Whether to deactivate Niagara when intensity drops below threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    bool bAutoDeactivate = false;

    /** Intensity threshold for auto-deactivate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding", meta = (EditCondition = "bAutoDeactivate"))
    float DeactivateThreshold = 0.01f;

    /** Publish rate in Hz (how often to publish VFX state as emitters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding", meta = (ClampMin = "1", ClampMax = "60"))
    int32 PublishRateHz = 10;

    /** Only publish when values change (reduces network traffic) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Binding")
    bool bOnlyPublishOnChange = true;

    // ========================================================================
    // RS_ ACTIONS - Generic Parameter Control
    // ========================================================================

    /** Set any float parameter by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Parameters")
    void RS_SetFloatParameter(FName ParameterName, float Value);

    /** Set any vector parameter by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Parameters")
    void RS_SetVectorParameter(FName ParameterName, float X, float Y, float Z);

    /** Set any color parameter by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Parameters")
    void RS_SetColorParameter(FName ParameterName, float R, float G, float B, float A);

    /** Set any int parameter by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Parameters")
    void RS_SetIntParameter(FName ParameterName, int32 Value);

    /** Set any bool parameter by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Parameters")
    void RS_SetBoolParameter(FName ParameterName, bool Value);

    // ========================================================================
    // RS_ ACTIONS - Spawn Control
    // ========================================================================

    /** Set spawn rate multiplier (1.0 = default, 0.0 = no spawning) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Spawn")
    void RS_SetSpawnRate(float Rate);

    /** Set spawn rate as absolute particles per second */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Spawn")
    void RS_SetSpawnRateAbsolute(float ParticlesPerSecond);

    /** Trigger a burst of particles */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Spawn")
    void RS_TriggerBurst(int32 Count);

    /** Set burst count for triggered bursts */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Spawn")
    void RS_SetBurstCount(int32 Count);

    // ========================================================================
    // RS_ ACTIONS - Particle Properties
    // ========================================================================

    /** Set particle lifetime multiplier */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Particles")
    void RS_SetLifetime(float Lifetime);

    /** Set particle size/scale multiplier */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Particles")
    void RS_SetSize(float Size);

    /** Set particle size as XYZ scale */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Particles")
    void RS_SetSizeXYZ(float X, float Y, float Z);

    /** Set particle velocity multiplier */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Particles")
    void RS_SetVelocity(float Velocity);

    /** Set particle velocity as direction vector */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Particles")
    void RS_SetVelocityXYZ(float X, float Y, float Z);

    /** Set particle mass */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Particles")
    void RS_SetMass(float Mass);

    /** Set particle drag coefficient */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Particles")
    void RS_SetDrag(float Drag);

    /** Set gravity multiplier */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Particles")
    void RS_SetGravity(float Gravity);

    // ========================================================================
    // RS_ ACTIONS - Visual Properties
    // ========================================================================

    /** Set particle color */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Visual")
    void RS_SetColor(float R, float G, float B);

    /** Set particle color with alpha */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Visual")
    void RS_SetColorWithAlpha(float R, float G, float B, float A);

    /** Set emissive/glow intensity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Visual")
    void RS_SetEmissive(float Intensity);

    /** Set emissive color with intensity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Visual")
    void RS_SetEmissiveColor(float R, float G, float B, float Intensity);

    /** Set particle opacity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Visual")
    void RS_SetOpacity(float Opacity);

    /** Set sprite rotation */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Visual")
    void RS_SetSpriteRotation(float Degrees);

    /** Set sprite alignment */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Visual")
    void RS_SetSpriteSize(float Width, float Height);

    // ========================================================================
    // RS_ ACTIONS - System Control
    // ========================================================================

    /** Activate the Niagara system */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|System")
    void RS_Activate();

    /** Deactivate the Niagara system */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|System")
    void RS_Deactivate();

    /** Reset and restart the Niagara system */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|System")
    void RS_Reset();

    /** Pause the Niagara system */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|System")
    void RS_Pause();

    /** Resume the Niagara system */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|System")
    void RS_Resume();

    /** Set system age (seek to time position) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|System")
    void RS_SetAge(float Age);

    /** Set overall intensity multiplier (affects spawn, size, emissive) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|System")
    void RS_SetGlobalIntensity(float Intensity);

    // ========================================================================
    // RS_ ACTIONS - Transform
    // ========================================================================

    /** Set emitter world location */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Transform")
    void RS_SetLocation(float X, float Y, float Z);

    /** Set emitter world rotation (degrees) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Transform")
    void RS_SetRotation(float Pitch, float Yaw, float Roll);

    /** Set emitter world scale */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Transform")
    void RS_SetScale(float Scale);

    /** Set emitter world scale XYZ */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara|Transform")
    void RS_SetScaleXYZ(float X, float Y, float Z);

    // ========================================================================
    // RS_ EMITTERS - State Publishing
    // ========================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_FloatEmitter, float, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRS_VectorEmitter, float, X, float, Y, float, Z);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRS_ColorEmitter, float, R, float, G, float, B);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_BoolEmitter, bool, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_IntEmitter, int32, Value);

    // System state emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_BoolEmitter RS_OnActiveChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_IntEmitter RS_OnParticleCountChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_FloatEmitter RS_OnAgeChanged;

    // Property emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_FloatEmitter RS_OnSpawnRateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_FloatEmitter RS_OnLifetimeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_FloatEmitter RS_OnSizeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_FloatEmitter RS_OnVelocityChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_ColorEmitter RS_OnColorChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_FloatEmitter RS_OnEmissiveChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_FloatEmitter RS_OnGlobalIntensityChanged;

    // Transform emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_VectorEmitter RS_OnLocationChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Niagara|Emitters")
    FRS_VectorEmitter RS_OnRotationChanged;

    // ========================================================================
    // RUNTIME STATE
    // ========================================================================

    /** Last received intensity value */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Binding")
    float CurrentIntensity = 0.0f;

    /** Last received color value */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Binding")
    FLinearColor CurrentColor = FLinearColor::White;

    /** Whether the component is actively receiving pulses */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Binding")
    bool bIsReceivingPulses = false;

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when a pulse is received for this binding */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Binding")
    FOnNiagaraPulseReceived OnPulseReceived;

    // ========================================================================
    // PUBLIC METHODS
    // ========================================================================

    /** Manually set a parameter value (bypasses pulse) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Binding")
    void SetFloatParameter(FName ParameterName, float Value);

    /** Manually set a color parameter (bypasses pulse) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Binding")
    void SetColorParameter(FName ParameterName, FLinearColor Color);

    /** Force update all bindings with current values */
    UFUNCTION(BlueprintCallable, Category = "Rship|Binding")
    void ForceUpdate();

    /** Enable/disable all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Binding")
    void SetBindingsEnabled(bool bEnabled);

    /** Force publish all current values */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara")
    void ForcePublish();

    /** Get current VFX state as JSON */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara")
    FString GetNiagaraStateJson() const;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    FDelegateHandle PulseReceivedHandle;
    double LastPulseTime = 0.0;

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

    void OnPulseReceivedInternal(const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data);
    void ApplyBindings(TSharedPtr<FJsonObject> Data);
    float ProcessBindingValue(FRshipNiagaraParameterBinding& Binding, float RawValue, float DeltaTime);
    float GetFloatFromJson(TSharedPtr<FJsonObject> Data, const FString& FieldPath);
    FLinearColor GetColorFromJson(TSharedPtr<FJsonObject> Data, const FString& Prefix);
    FString GetFullEmitterId() const;
    void ReadAndPublishState();
    bool HasValueChanged(float OldValue, float NewValue, float Threshold = 0.001f) const;
    bool HasColorChanged(const FLinearColor& OldColor, const FLinearColor& NewColor, float Threshold = 0.001f) const;
};

// ============================================================================
// NIAGARA DATA INTERFACE (Advanced)
// ============================================================================

/**
 * Manager for bulk Niagara bindings and GPU-side data.
 * Use for large numbers of particles driven by fixture data.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipNiagaraManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    /** Register a Niagara binding */
    void RegisterBinding(URshipNiagaraBinding* Binding);

    /** Unregister a Niagara binding */
    void UnregisterBinding(URshipNiagaraBinding* Binding);

    /** Get all active bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara")
    TArray<URshipNiagaraBinding*> GetAllBindings() const { return RegisteredBindings; }

    /** Set global intensity multiplier for all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara")
    void SetGlobalIntensityMultiplier(float Multiplier);

    /** Get global intensity multiplier */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Niagara")
    float GetGlobalIntensityMultiplier() const { return GlobalIntensityMultiplier; }

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    TArray<URshipNiagaraBinding*> RegisteredBindings;

    float GlobalIntensityMultiplier = 1.0f;
};
