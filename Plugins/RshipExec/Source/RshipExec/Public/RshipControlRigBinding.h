// Rship Control Rig Binding
// Bind pulse data to Control Rig parameters for procedural animation

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipControlRigBinding.generated.h"

class URshipSubsystem;
class URshipPulseReceiver;
class UControlRig;
class UControlRigComponent;
class UCurveFloat;

// ============================================================================
// CONTROL RIG BINDING TYPES
// ============================================================================

/** Type of Control Rig property to bind */
UENUM(BlueprintType)
enum class ERshipControlRigPropertyType : uint8
{
    Float           UMETA(DisplayName = "Float"),
    Vector          UMETA(DisplayName = "Vector"),
    Rotator         UMETA(DisplayName = "Rotator"),
    Transform       UMETA(DisplayName = "Transform"),
    Bool            UMETA(DisplayName = "Bool"),
    Integer         UMETA(DisplayName = "Integer"),
    Color           UMETA(DisplayName = "Color")
};

/** Interpolation mode for value changes */
UENUM(BlueprintType)
enum class ERshipControlRigInterpMode : uint8
{
    None            UMETA(DisplayName = "None (Immediate)"),
    Linear          UMETA(DisplayName = "Linear"),
    EaseIn          UMETA(DisplayName = "Ease In"),
    EaseOut         UMETA(DisplayName = "Ease Out"),
    EaseInOut       UMETA(DisplayName = "Ease In/Out"),
    Spring          UMETA(DisplayName = "Spring")
};

/** Mapping function for value transformation */
UENUM(BlueprintType)
enum class ERshipControlRigMappingFunc : uint8
{
    Direct          UMETA(DisplayName = "Direct"),
    Remap           UMETA(DisplayName = "Remap Range"),
    Curve           UMETA(DisplayName = "Curve"),
    Expression      UMETA(DisplayName = "Expression")
};

/** Single binding from emitter field to Control Rig property */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipControlRigPropertyBinding
{
    GENERATED_BODY()

    /** Binding enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    bool bEnabled = true;

    /** Source emitter ID (can use wildcards) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    FString EmitterId;

    /** Field path in pulse data (e.g., "intensity", "color.r", "position.x") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    FString SourceField = TEXT("intensity");

    /** Target Control Rig control name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    FName ControlName;

    /** Property type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    ERshipControlRigPropertyType PropertyType = ERshipControlRigPropertyType::Float;

    /** Component to affect for vector types (X, Y, Z, or All) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (EditCondition = "PropertyType == ERshipControlRigPropertyType::Vector || PropertyType == ERshipControlRigPropertyType::Rotator"))
    FName VectorComponent = NAME_None;

    /** Mapping function */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    ERshipControlRigMappingFunc MappingFunc = ERshipControlRigMappingFunc::Remap;

    /** Input range minimum (for remapping) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (EditCondition = "MappingFunc == ERshipControlRigMappingFunc::Remap"))
    float InputMin = 0.0f;

    /** Input range maximum (for remapping) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (EditCondition = "MappingFunc == ERshipControlRigMappingFunc::Remap"))
    float InputMax = 1.0f;

    /** Output range minimum (for remapping) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (EditCondition = "MappingFunc == ERshipControlRigMappingFunc::Remap"))
    float OutputMin = 0.0f;

    /** Output range maximum (for remapping) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (EditCondition = "MappingFunc == ERshipControlRigMappingFunc::Remap"))
    float OutputMax = 100.0f;

    /** Clamp output to range */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    bool bClampOutput = true;

    /** Response curve for curve-based mapping (input 0-1 on X, output on Y) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (EditCondition = "MappingFunc == ERshipControlRigMappingFunc::Curve"))
    UCurveFloat* ResponseCurve = nullptr;

    /** Expression for expression-based mapping. Variables: x (input), t (time) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (EditCondition = "MappingFunc == ERshipControlRigMappingFunc::Expression"))
    FString Expression = TEXT("x");

    /** Interpolation mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    ERshipControlRigInterpMode InterpMode = ERshipControlRigInterpMode::Linear;

    /** Interpolation speed (higher = faster response) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (ClampMin = "0.1", ClampMax = "100.0", EditCondition = "InterpMode != ERshipControlRigInterpMode::None"))
    float InterpSpeed = 10.0f;

    /** Spring stiffness (for spring interpolation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (ClampMin = "1.0", ClampMax = "1000.0", EditCondition = "InterpMode == ERshipControlRigInterpMode::Spring"))
    float SpringStiffness = 100.0f;

    /** Spring damping (for spring interpolation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (ClampMin = "0.0", ClampMax = "10.0", EditCondition = "InterpMode == ERshipControlRigInterpMode::Spring"))
    float SpringDamping = 1.0f;

    /** Multiplier applied after mapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    float Multiplier = 1.0f;

    /** Offset added after mapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    float Offset = 0.0f;

    /** Additive blend (true) or replace (false) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    bool bAdditive = false;

    /** Weight for blending (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Weight = 1.0f;

    // Runtime state
    float CurrentValue = 0.0f;
    float TargetValue = 0.0f;
    float Velocity = 0.0f;  // For spring
};

/** Control Rig binding configuration */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipControlRigConfig
{
    GENERATED_BODY()

    /** Configuration name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    FString Name = TEXT("Default");

    /** Property bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    TArray<FRshipControlRigPropertyBinding> Bindings;

    /** Global weight multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GlobalWeight = 1.0f;

    /** Enable/disable all bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    bool bEnabled = true;
};

/** Runtime state for a binding */
struct FRshipControlRigBindingState
{
    float CurrentValue = 0.0f;
    FVector CurrentVector = FVector::ZeroVector;
    FRotator CurrentRotator = FRotator::ZeroRotator;
    FTransform CurrentTransform = FTransform::Identity;

    float TargetValue = 0.0f;
    FVector TargetVector = FVector::ZeroVector;
    FRotator TargetRotator = FRotator::ZeroRotator;
    FTransform TargetTransform = FTransform::Identity;

    // Spring dynamics
    float Velocity = 0.0f;
    FVector VelocityVector = FVector::ZeroVector;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnControlRigBindingUpdated, FName, ControlName, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlRigConfigChanged, const FRshipControlRigConfig&, Config);

// ============================================================================
// CONTROL RIG BINDING COMPONENT
// ============================================================================

/**
 * Comprehensive binding for Control Rig procedural animation to rship.
 * Exposes all Control Rig controls for direct manipulation and state publishing:
 * - Float, Vector, Rotator, Transform controls
 * - Animation blending and weights
 * - Configuration management
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Control Rig Binding"))
class RSHIPEXEC_API URshipControlRigBinding : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipControlRigBinding();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** Active binding configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    FRshipControlRigConfig BindingConfig;

    /** Saved configurations for quick switching */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    TArray<FRshipControlRigConfig> SavedConfigs;

    /** Auto-discover Control Rig component on same actor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    bool bAutoDiscoverControlRig = true;

    /** Manual Control Rig component reference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (EditCondition = "!bAutoDiscoverControlRig"))
    UControlRigComponent* ControlRigComponent;

    /** Publish rate in Hz (how often to publish rig state as emitters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig", meta = (ClampMin = "1", ClampMax = "60"))
    int32 PublishRateHz = 10;

    /** Only publish when values change (reduces network traffic) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|ControlRig")
    bool bOnlyPublishOnChange = true;

    // ========================================================================
    // RS_ ACTIONS - Direct Control Access
    // ========================================================================

    /** Set a float control by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Controls")
    void RS_SetFloatControl(FName ControlName, float Value);

    /** Set a vector control by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Controls")
    void RS_SetVectorControl(FName ControlName, float X, float Y, float Z);

    /** Set a rotator control by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Controls")
    void RS_SetRotatorControl(FName ControlName, float Pitch, float Yaw, float Roll);

    /** Set a transform control by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Controls")
    void RS_SetTransformControl(FName ControlName, float PosX, float PosY, float PosZ, float RotPitch, float RotYaw, float RotRoll, float Scale);

    /** Set a bool control by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Controls")
    void RS_SetBoolControl(FName ControlName, bool Value);

    /** Set an int control by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Controls")
    void RS_SetIntControl(FName ControlName, int32 Value);

    // ========================================================================
    // RS_ ACTIONS - Animation Control
    // ========================================================================

    /** Set global weight for all Control Rig animation (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Animation")
    void RS_SetGlobalWeight(float Weight);

    /** Set weight for a specific control (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Animation")
    void RS_SetControlWeight(FName ControlName, float Weight);

    /** Enable/disable all Control Rig bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Animation")
    void RS_SetEnabled(bool bEnabled);

    /** Reset all controls to default values */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Animation")
    void RS_ResetAllControls();

    /** Reset a specific control to default */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Animation")
    void RS_ResetControl(FName ControlName);

    // ========================================================================
    // RS_ ACTIONS - Configuration
    // ========================================================================

    /** Load a saved configuration by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Config")
    void RS_LoadConfiguration(const FString& ConfigName);

    /** Switch to next saved configuration */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Config")
    void RS_NextConfiguration();

    /** Switch to previous saved configuration */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig|Config")
    void RS_PreviousConfiguration();

    // ========================================================================
    // RS_ EMITTERS - State Publishing
    // ========================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRS_FloatControlEmitter, FName, ControlName, float, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FRS_VectorControlEmitter, FName, ControlName, float, X, float, Y, float, Z);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FRS_RotatorControlEmitter, FName, ControlName, float, Pitch, float, Yaw, float, Roll);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_FloatEmitter, float, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_BoolEmitter, bool, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_StringEmitter, const FString&, Value);

    /** Fired when any float control changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|ControlRig|Emitters")
    FRS_FloatControlEmitter RS_OnFloatControlChanged;

    /** Fired when any vector control changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|ControlRig|Emitters")
    FRS_VectorControlEmitter RS_OnVectorControlChanged;

    /** Fired when any rotator control changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|ControlRig|Emitters")
    FRS_RotatorControlEmitter RS_OnRotatorControlChanged;

    /** Fired when global weight changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|ControlRig|Emitters")
    FRS_FloatEmitter RS_OnGlobalWeightChanged;

    /** Fired when enabled state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|ControlRig|Emitters")
    FRS_BoolEmitter RS_OnEnabledChanged;

    /** Fired when configuration changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|ControlRig|Emitters")
    FRS_StringEmitter RS_OnConfigurationChanged;

    // ========================================================================
    // PUBLIC METHODS
    // ========================================================================

    /** Force publish all current values */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void ForcePublish();

    /** Get current rig state as JSON */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    FString GetControlRigStateJson() const;

    // ========================================================================
    // BINDING MANAGEMENT
    // ========================================================================

    /** Add a new property binding */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void AddBinding(const FRshipControlRigPropertyBinding& Binding);

    /** Remove binding by index */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void RemoveBinding(int32 Index);

    /** Clear all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void ClearBindings();

    /** Enable/disable specific binding */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void SetBindingEnabled(int32 Index, bool bEnabled);

    /** Set global weight */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void SetGlobalWeight(float Weight);

    // ========================================================================
    // CONFIGURATION MANAGEMENT
    // ========================================================================

    /** Save current config to saved configs list */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void SaveCurrentConfig(const FString& Name);

    /** Load a saved config by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    bool LoadConfig(const FString& Name);

    /** Delete a saved config */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    bool DeleteConfig(const FString& Name);

    /** Get list of saved config names */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|ControlRig")
    TArray<FString> GetSavedConfigNames() const;

    // ========================================================================
    // QUICK BINDING HELPERS
    // ========================================================================

    /** Quick bind: intensity to float control */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void BindIntensityToFloat(const FString& EmitterId, FName ControlName, float OutputMin = 0.0f, float OutputMax = 100.0f);

    /** Quick bind: color to vector control */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void BindColorToVector(const FString& EmitterId, FName ControlName);

    /** Quick bind: position to transform control */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void BindPositionToTransform(const FString& EmitterId, FName ControlName);

    /** Quick bind: rotation to rotator control */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void BindRotationToRotator(const FString& EmitterId, FName ControlName);

    // ========================================================================
    // DISCOVERY
    // ========================================================================

    /** Get available control names from the Control Rig */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|ControlRig")
    TArray<FName> GetAvailableControls() const;

    /** Get control type */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|ControlRig")
    ERshipControlRigPropertyType GetControlType(FName ControlName) const;

    /** Auto-generate bindings from emitter pattern */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void AutoGenerateBindings(const FString& EmitterPattern, const FString& ControlPattern);

    // ========================================================================
    // RUNTIME
    // ========================================================================

    /** Get current value of a binding by control name */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|ControlRig")
    float GetBindingValue(FName ControlName) const;

    /** Override a binding value manually (will be blended with pulse data) */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void SetBindingOverride(FName ControlName, float Value, float BlendTime = 0.0f);

    /** Clear manual override */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void ClearBindingOverride(FName ControlName);

    /** Reset all bindings to default values */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void ResetAllBindings();

    // ========================================================================
    // EVENTS
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Rship|ControlRig")
    FOnControlRigBindingUpdated OnBindingUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Rship|ControlRig")
    FOnControlRigConfigChanged OnConfigChanged;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Cached Control Rig reference
    UPROPERTY()
    UControlRig* ControlRig;

    // Binding states (parallel to BindingConfig.Bindings)
    TArray<FRshipControlRigBindingState> BindingStates;

    // Overrides
    TMap<FName, float> ManualOverrides;
    TMap<FName, float> OverrideBlendTimers;

    // Pulse receiver binding
    FDelegateHandle PulseReceiverHandle;

    // State tracking for publishing
    double LastPublishTime = 0.0;
    double PublishInterval = 0.1;
    float LastGlobalWeight = 1.0f;
    bool bLastEnabled = true;
    int32 CurrentConfigIndex = 0;
    FString CurrentConfigName;
    TMap<FName, float> LastFloatValues;
    TMap<FName, FVector> LastVectorValues;
    TMap<FName, FRotator> LastRotatorValues;

    void DiscoverControlRig();
    void BindToPulseReceiver();
    void UnbindFromPulseReceiver();

    void OnPulseReceived(const FString& EmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data);
    void UpdateBinding(int32 Index, float DeltaTime);
    void ApplyBindingToControlRig(int32 Index);

    float ExtractFieldValue(TSharedPtr<FJsonObject> Data, const FString& FieldPath) const;
    float MapValue(float Input, const FRshipControlRigPropertyBinding& Binding) const;
    float InterpolateValue(float Current, float Target, const FRshipControlRigPropertyBinding& Binding, float DeltaTime, float& Velocity) const;

    bool MatchesEmitterPattern(const FString& EmitterId, const FString& Pattern) const;

    void ReadAndPublishState();
    bool HasValueChanged(float OldValue, float NewValue, float Threshold = 0.001f) const;

    // Expression parser helpers
    float EvaluateExpression(const FString& ExpressionStr, float X) const;
    float ParseAddSub(const FString& Expr, int32& Pos) const;
    float ParseMulDiv(const FString& Expr, int32& Pos) const;
    float ParsePower(const FString& Expr, int32& Pos) const;
    float ParseUnary(const FString& Expr, int32& Pos) const;
    float ParsePrimary(const FString& Expr, int32& Pos) const;
    float ParseNumber(const FString& Expr, int32& Pos) const;
    void SkipWhitespace(const FString& Expr, int32& Pos) const;
};

// ============================================================================
// CONTROL RIG BINDING MANAGER
// ============================================================================

/**
 * Subsystem service for managing Control Rig bindings globally.
 * Provides templates and bulk operations.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipControlRigManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();

    // ========================================================================
    // REGISTRATION
    // ========================================================================

    /** Register a Control Rig binding component */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void RegisterBinding(URshipControlRigBinding* Binding);

    /** Unregister a Control Rig binding component */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void UnregisterBinding(URshipControlRigBinding* Binding);

    /** Get all registered bindings */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|ControlRig")
    TArray<URshipControlRigBinding*> GetAllBindings() const;

    // ========================================================================
    // BULK OPERATIONS
    // ========================================================================

    /** Set global weight on all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void SetGlobalWeightAll(float Weight);

    /** Enable/disable all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void SetEnabledAll(bool bEnabled);

    /** Reset all bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void ResetAll();

    // ========================================================================
    // TEMPLATES
    // ========================================================================

    /** Save a binding config as a template */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    void SaveTemplate(const FString& TemplateName, const FRshipControlRigConfig& Config);

    /** Load a template */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    bool LoadTemplate(const FString& TemplateName, FRshipControlRigConfig& OutConfig);

    /** Get available template names */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|ControlRig")
    TArray<FString> GetTemplateNames() const;

    /** Delete a template */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    bool DeleteTemplate(const FString& TemplateName);

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    TArray<URshipControlRigBinding*> RegisteredBindings;

    TMap<FString, FRshipControlRigConfig> Templates;

    FString GetTemplatesFilePath() const;
    void LoadTemplatesFromFile();
    void SaveTemplatesToFile();
};
