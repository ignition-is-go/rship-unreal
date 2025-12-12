// Rship PCG (Procedural Content Generation) Binding
// Drive PCG graph parameters from rship pulse data for reactive procedural content
//
// NOTE: This file is excluded from compilation when PCG plugin is not enabled.
// See RshipExec.Build.cs for conditional compilation logic.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Curves/CurveFloat.h"
#include "RshipPCGBinding.generated.h"

class URshipSubsystem;
class URshipPulseReceiver;

// ============================================================================
// BINDING MODES
// ============================================================================

/** How to map pulse values to PCG parameters */
UENUM(BlueprintType)
enum class ERshipPCGBindingMode : uint8
{
    Direct          UMETA(DisplayName = "Direct"),          // 1:1 mapping
    Normalized      UMETA(DisplayName = "Normalized"),      // Map to 0-1 range
    Scaled          UMETA(DisplayName = "Scaled"),          // Apply scale factor
    Mapped          UMETA(DisplayName = "Range Mapped"),    // Map input range to output range
    Curve           UMETA(DisplayName = "Curve"),           // Apply response curve
    Trigger         UMETA(DisplayName = "Trigger")          // Binary on/off based on threshold
};

/** Regeneration strategy for PCG updates */
UENUM(BlueprintType)
enum class ERshipPCGRegenStrategy : uint8
{
    Immediate       UMETA(DisplayName = "Immediate"),       // Regenerate on every change (rate-limited)
    Debounced       UMETA(DisplayName = "Debounced"),       // Wait for changes to settle before regenerating
    Threshold       UMETA(DisplayName = "Threshold"),       // Only regenerate on significant changes
    Manual          UMETA(DisplayName = "Manual")           // Only regenerate on explicit ForceRegenerate() call
};

// ============================================================================
// PARAMETER BINDING STRUCTS
// ============================================================================

/** Binding for scalar (float) PCG parameters */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipPCGParameterBinding
{
    GENERATED_BODY()

    /** Whether this binding is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    bool bEnabled = true;

    /** Emitter ID to listen for (e.g., "targetId:emitterId") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString EmitterId;

    /** Field path in pulse data (e.g., "intensity", "values.dimmer") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString PulseField = TEXT("intensity");

    /** Name of the PCG graph parameter to control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FName ParameterName;

    /** Binding mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    ERshipPCGBindingMode Mode = ERshipPCGBindingMode::Direct;

    /** Scale factor (for Scaled mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Scaled"))
    float ScaleFactor = 1.0f;

    /** Input range minimum (for Mapped/Normalized modes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Mapped || Mode == ERshipPCGBindingMode::Normalized"))
    float InputMin = 0.0f;

    /** Input range maximum (for Mapped/Normalized modes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Mapped || Mode == ERshipPCGBindingMode::Normalized"))
    float InputMax = 1.0f;

    /** Output range minimum (for Mapped mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Mapped"))
    float OutputMin = 0.0f;

    /** Output range maximum (for Mapped mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Mapped"))
    float OutputMax = 1.0f;

    /** Response curve (for Curve mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Curve"))
    UCurveFloat* ResponseCurve = nullptr;

    /** Threshold for Trigger mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Trigger"))
    float TriggerThreshold = 0.5f;

    /** Value when triggered (on) - for Trigger mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Trigger"))
    float OnValue = 1.0f;

    /** Value when not triggered (off) - for Trigger mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (EditCondition = "Mode == ERshipPCGBindingMode::Trigger"))
    float OffValue = 0.0f;

    /** Minimum change to mark binding dirty (prevents regeneration on tiny changes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ChangeThreshold = 0.01f;

    /** Smoothing factor (0 = instant, higher = smoother) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float Smoothing = 0.0f;

    /** Offset added after all processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    float Offset = 0.0f;

    // Runtime state (not serialized)
    float LastRawValue = 0.0f;
    float TargetValue = 0.0f;
    float SmoothedValue = 0.0f;
    bool bDirty = false;
};

/** Binding for vector PCG parameters */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipPCGVectorBinding
{
    GENERATED_BODY()

    /** Whether this binding is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    bool bEnabled = true;

    /** Emitter ID to listen for */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString EmitterId;

    /** Field prefix for vector data (e.g., "position" for position.x, position.y, position.z) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString VectorFieldPrefix = TEXT("position");

    /** Name of the PCG graph parameter to control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FName ParameterName;

    /** Scale factor for X component */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    float ScaleX = 1.0f;

    /** Scale factor for Y component */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    float ScaleY = 1.0f;

    /** Scale factor for Z component */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    float ScaleZ = 1.0f;

    /** Offset added to result */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FVector Offset = FVector::ZeroVector;

    /** Distance threshold to mark binding dirty */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (ClampMin = "0.0"))
    float ChangeThreshold = 1.0f;

    /** Smoothing factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float Smoothing = 0.0f;

    // Runtime state
    FVector LastRawValue = FVector::ZeroVector;
    FVector TargetValue = FVector::ZeroVector;
    FVector SmoothedValue = FVector::ZeroVector;
    bool bDirty = false;
};

/** Binding for color PCG parameters */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipPCGColorBinding
{
    GENERATED_BODY()

    /** Whether this binding is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    bool bEnabled = true;

    /** Emitter ID to listen for */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString EmitterId;

    /** Field path for color data (e.g., "color" for color.r, color.g, color.b) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString ColorField = TEXT("color");

    /** Optional intensity field to multiply color by */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString IntensityField;

    /** Name of the PCG graph parameter to control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FName ParameterName;

    /** Color multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    float ColorMultiplier = 1.0f;

    /** Allow HDR values (greater than 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    bool bAllowHDR = true;

    /** Color distance threshold to mark binding dirty */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ChangeThreshold = 0.01f;

    /** Smoothing factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float Smoothing = 0.0f;

    // Runtime state
    FLinearColor LastRawValue = FLinearColor::Black;
    FLinearColor TargetValue = FLinearColor::Black;
    FLinearColor SmoothedValue = FLinearColor::Black;
    bool bDirty = false;
};

/** Binding for seed/integer PCG parameters (converts float to int) */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipPCGSeedBinding
{
    GENERATED_BODY()

    /** Whether this binding is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    bool bEnabled = true;

    /** Emitter ID to listen for */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString EmitterId;

    /** Field path in pulse data (typically "intensity" mapped to seed range) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FString PulseField = TEXT("intensity");

    /** Name of the PCG graph parameter to control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    FName ParameterName = TEXT("Seed");

    /** Minimum seed value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    int32 SeedMin = 0;

    /** Maximum seed value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    int32 SeedMax = 999999;

    /** Input range minimum (pulse value that maps to SeedMin) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    float InputMin = 0.0f;

    /** Input range maximum (pulse value that maps to SeedMax) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    float InputMax = 1.0f;

    // Runtime state
    int32 LastSeed = 0;
    int32 CurrentSeed = 0;
    bool bDirty = false;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPCGParameterUpdated, FName, ParameterName, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPCGVectorUpdated, FName, ParameterName, FVector, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPCGColorUpdated, FName, ParameterName, FLinearColor, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPCGRegenerated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPCGRegenSkipped, FString, Reason);

// ============================================================================
// PCG BINDING COMPONENT
// ============================================================================

/**
 * Component that binds rship pulse data to PCG graph parameters.
 * Attach to an actor with a PCGComponent to enable reactive procedural content generation.
 *
 * Key features:
 * - Multiple regeneration strategies (immediate, debounced, threshold, manual)
 * - Rate limiting to prevent excessive regeneration
 * - Smoothing for gradual parameter changes
 * - Support for scalar, vector, color, and seed parameters
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship PCG Binding"))
class RSHIPEXEC_API URshipPCGBinding : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipPCGBinding();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** PCG Component to control (auto-discovered if not set) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    UPCGComponent* PCGComponent;

    /** Auto-discover PCG component on same actor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG")
    bool bAutoDiscoverPCGComponent = true;

    /** Scalar parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Bindings")
    TArray<FRshipPCGParameterBinding> ScalarBindings;

    /** Vector parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Bindings")
    TArray<FRshipPCGVectorBinding> VectorBindings;

    /** Color parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Bindings")
    TArray<FRshipPCGColorBinding> ColorBindings;

    /** Seed/Integer parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Bindings")
    TArray<FRshipPCGSeedBinding> SeedBindings;

    // ========================================================================
    // REGENERATION CONTROL
    // ========================================================================

    /** How to handle PCG regeneration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Regeneration")
    ERshipPCGRegenStrategy RegenStrategy = ERshipPCGRegenStrategy::Debounced;

    /** Debounce time in seconds (waits for this quiet period before regenerating) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Regeneration", meta = (EditCondition = "RegenStrategy == ERshipPCGRegenStrategy::Debounced", ClampMin = "0.01", ClampMax = "5.0"))
    float DebounceTime = 0.1f;

    /** Maximum regenerations per second (rate limiting) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Regeneration", meta = (ClampMin = "0.1", ClampMax = "60.0"))
    float MaxRegensPerSecond = 10.0f;

    /** Clean up existing generated content before regenerating */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Regeneration")
    bool bCleanupBeforeRegen = true;

    /** Generate even if no bindings are dirty (respects rate limit) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|PCG|Regeneration")
    bool bAllowEmptyRegen = false;

    // ========================================================================
    // BINDING MANAGEMENT
    // ========================================================================

    /** Add a scalar parameter binding */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void AddScalarBinding(const FRshipPCGParameterBinding& Binding);

    /** Add a vector parameter binding */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void AddVectorBinding(const FRshipPCGVectorBinding& Binding);

    /** Add a color parameter binding */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void AddColorBinding(const FRshipPCGColorBinding& Binding);

    /** Add a seed parameter binding */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void AddSeedBinding(const FRshipPCGSeedBinding& Binding);

    /** Remove binding by parameter name */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void RemoveBinding(FName ParameterName);

    /** Clear all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void ClearAllBindings();

    /** Enable/disable all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void SetAllBindingsEnabled(bool bEnabled);

    // ========================================================================
    // RUNTIME CONTROL
    // ========================================================================

    /** Force immediate regeneration (bypasses strategy) */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void ForceRegenerate();

    /** Mark all bindings as dirty (will trigger regen according to strategy) */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void MarkAllDirty();

    /** Pause regeneration (parameters still update, but no regen) */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void SetRegenerationPaused(bool bPaused);

    /** Check if regeneration is paused */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    bool IsRegenerationPaused() const { return bRegenPaused; }

    /** Set a scalar parameter directly (bypasses binding, marks dirty) */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void SetScalarParameter(FName Name, float Value);

    /** Set a vector parameter directly (bypasses binding, marks dirty) */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void SetVectorParameter(FName Name, FVector Value);

    /** Set a color parameter directly (bypasses binding, marks dirty) */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void SetColorParameter(FName Name, FLinearColor Value);

    /** Set a seed parameter directly (bypasses binding, marks dirty) */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void SetSeedParameter(FName Name, int32 Value);

    // ========================================================================
    // DISCOVERY
    // ========================================================================

    /** Get available parameter names from the PCG graph */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    TArray<FName> GetAvailableParameters() const;

    /** Check if a parameter exists in the PCG graph */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    bool HasParameter(FName ParameterName) const;

    // ========================================================================
    // STATUS
    // ========================================================================

    /** Get time since last regeneration */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    float GetTimeSinceLastRegen() const { return TimeSinceLastRegen; }

    /** Check if any binding is dirty */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    bool HasDirtyBindings() const { return bAnyDirty; }

    /** Get number of regenerations since BeginPlay */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    int32 GetRegenCount() const { return RegenCount; }

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when a scalar parameter is updated */
    UPROPERTY(BlueprintAssignable, Category = "Rship|PCG")
    FOnPCGParameterUpdated OnScalarParameterUpdated;

    /** Fired when a vector parameter is updated */
    UPROPERTY(BlueprintAssignable, Category = "Rship|PCG")
    FOnPCGVectorUpdated OnVectorParameterUpdated;

    /** Fired when a color parameter is updated */
    UPROPERTY(BlueprintAssignable, Category = "Rship|PCG")
    FOnPCGColorUpdated OnColorParameterUpdated;

    /** Fired after PCG regeneration completes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|PCG")
    FOnPCGRegenerated OnRegenerated;

    /** Fired when regeneration is skipped (with reason) */
    UPROPERTY(BlueprintAssignable, Category = "Rship|PCG")
    FOnPCGRegenSkipped OnRegenSkipped;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    FDelegateHandle PulseHandle;

    // Regeneration timing
    float TimeSinceLastRegen = 0.0f;
    float TimeSinceLastDirty = 0.0f;
    bool bAnyDirty = false;
    bool bRegenPaused = false;
    int32 RegenCount = 0;

    // Direct parameter overrides (bypass bindings)
    TMap<FName, float> DirectScalarValues;
    TMap<FName, FVector> DirectVectorValues;
    TMap<FName, FLinearColor> DirectColorValues;
    TMap<FName, int32> DirectSeedValues;
    bool bHasDirectOverrides = false;

    void DiscoverPCGComponent();
    void BindToPulseReceiver();
    void UnbindFromPulseReceiver();

    void OnPulseReceived(const FString& EmitterId, TSharedPtr<FJsonObject> Data);

    void UpdateScalarBindings(TSharedPtr<FJsonObject> Data, const FString& EmitterId, float DeltaTime);
    void UpdateVectorBindings(TSharedPtr<FJsonObject> Data, const FString& EmitterId, float DeltaTime);
    void UpdateColorBindings(TSharedPtr<FJsonObject> Data, const FString& EmitterId, float DeltaTime);
    void UpdateSeedBindings(TSharedPtr<FJsonObject> Data, const FString& EmitterId);

    void UpdateSmoothing(float DeltaTime);
    void CheckAndTriggerRegen(float DeltaTime);
    void DoRegenerate();
    void ApplyParametersToGraph();

    float ProcessScalarValue(FRshipPCGParameterBinding& Binding, float RawValue);
    float ExtractFloatValue(TSharedPtr<FJsonObject> Data, const FString& FieldPath, float Default = 0.0f);
    FVector ExtractVectorValue(TSharedPtr<FJsonObject> Data, const FString& Prefix);
    FLinearColor ExtractColorValue(TSharedPtr<FJsonObject> Data, const FString& Prefix);

    bool MatchesEmitterId(const FString& IncomingId, const FString& Pattern) const;
};

// ============================================================================
// PCG BINDING MANAGER
// ============================================================================

/**
 * Manager for all PCG bindings in the scene.
 * Provides global control and coordination for PCG regeneration.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipPCGManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    // ========================================================================
    // REGISTRATION
    // ========================================================================

    /** Register a PCG binding component */
    void RegisterBinding(URshipPCGBinding* Binding);

    /** Unregister a PCG binding component */
    void UnregisterBinding(URshipPCGBinding* Binding);

    /** Get all registered PCG binding components */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    TArray<URshipPCGBinding*> GetAllBindings() const { return RegisteredBindings; }

    /** Get number of registered bindings */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    int32 GetBindingCount() const { return RegisteredBindings.Num(); }

    // ========================================================================
    // BULK OPERATIONS
    // ========================================================================

    /** Pause regeneration on all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void PauseAllRegeneration();

    /** Resume regeneration on all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void ResumeAllRegeneration();

    /** Force regenerate all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void ForceRegenerateAll();

    /** Mark all bindings dirty */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void MarkAllDirty();

    // ========================================================================
    // GLOBAL SETTINGS
    // ========================================================================

    /** Set global maximum regenerations per second across all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|PCG")
    void SetGlobalMaxRegensPerSecond(float MaxRegen);

    /** Get global maximum regenerations per second */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    float GetGlobalMaxRegensPerSecond() const { return GlobalMaxRegensPerSecond; }

    /** Get total regenerations this frame across all bindings */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    int32 GetRegensThisFrame() const { return RegensThisFrame; }

    /** Get total regenerations since initialization */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|PCG")
    int32 GetTotalRegenCount() const { return TotalRegenCount; }

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    TArray<URshipPCGBinding*> RegisteredBindings;

    float GlobalMaxRegensPerSecond = 30.0f;
    float RegenBudget = 0.0f;
    int32 RegensThisFrame = 0;
    int32 TotalRegenCount = 0;
};
