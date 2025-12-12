// Rship Niagara VFX Binding
// Drive Niagara particle systems from rship pulse data

#pragma once

#include "CoreMinimal.h"
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
 * Component that binds rship pulse data to Niagara particle system parameters.
 * Attach to an actor with a Niagara component to drive VFX from fixture data.
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
    // METHODS
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

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    FDelegateHandle PulseReceivedHandle;
    double LastPulseTime = 0.0;

    void OnPulseReceivedInternal(const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data);
    void ApplyBindings(TSharedPtr<FJsonObject> Data);
    float ProcessBindingValue(FRshipNiagaraParameterBinding& Binding, float RawValue, float DeltaTime);
    float GetFloatFromJson(TSharedPtr<FJsonObject> Data, const FString& FieldPath);
    FLinearColor GetColorFromJson(TSharedPtr<FJsonObject> Data, const FString& Prefix);
    FString GetFullEmitterId() const;
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
