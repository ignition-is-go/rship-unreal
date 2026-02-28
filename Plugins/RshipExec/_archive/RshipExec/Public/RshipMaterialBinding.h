// Rship Material Parameter Binding
// Bind pulse data to material instance parameters for reactive surfaces

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RshipMaterialBinding.generated.h"

class URshipSubsystem;
class URshipPulseReceiver;

// ============================================================================
// BINDING MODES
// ============================================================================

/** How to map pulse values to material parameters */
UENUM(BlueprintType)
enum class ERshipMaterialBindingMode : uint8
{
    Direct          UMETA(DisplayName = "Direct"),          // 1:1 mapping
    Normalized      UMETA(DisplayName = "Normalized"),      // Map to 0-1 range
    Scaled          UMETA(DisplayName = "Scaled"),          // Apply scale factor
    Mapped          UMETA(DisplayName = "Range Mapped"),    // Map input range to output range
    Curve           UMETA(DisplayName = "Curve"),           // Apply response curve
    Trigger         UMETA(DisplayName = "Trigger"),         // Binary on/off based on threshold
    Blend           UMETA(DisplayName = "Blend")            // Blend between two values
};

// ============================================================================
// PARAMETER BINDINGS
// ============================================================================

/** Binding for a scalar material parameter */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipMaterialScalarBinding
{
    GENERATED_BODY()

    /** Name of the material parameter to control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FName ParameterName;

    /** Field path in pulse data (e.g., "intensity", "values.dimmer") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FString PulseField = TEXT("intensity");

    /** Binding mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    ERshipMaterialBindingMode Mode = ERshipMaterialBindingMode::Direct;

    /** Scale factor (for Scaled mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Scaled"))
    float Scale = 1.0f;

    /** Offset added after scaling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    float Offset = 0.0f;

    /** Input range minimum (for Mapped mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Mapped"))
    float InputMin = 0.0f;

    /** Input range maximum (for Mapped mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Mapped"))
    float InputMax = 1.0f;

    /** Output range minimum (for Mapped mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Mapped"))
    float OutputMin = 0.0f;

    /** Output range maximum (for Mapped mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Mapped"))
    float OutputMax = 1.0f;

    /** Response curve (for Curve mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Curve"))
    UCurveFloat* ResponseCurve = nullptr;

    /** Threshold for trigger mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Trigger"))
    float TriggerThreshold = 0.5f;

    /** Value when triggered (on) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Trigger || Mode == ERshipMaterialBindingMode::Blend"))
    float OnValue = 1.0f;

    /** Value when not triggered (off) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (EditCondition = "Mode == ERshipMaterialBindingMode::Trigger || Mode == ERshipMaterialBindingMode::Blend"))
    float OffValue = 0.0f;

    /** Smoothing factor (0 = instant, 1 = very slow) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float Smoothing = 0.0f;

    /** Whether this binding is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    bool bEnabled = true;

    // Runtime state
    float CurrentValue = 0.0f;
    float TargetValue = 0.0f;
};

/** Binding for a vector/color material parameter */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipMaterialVectorBinding
{
    GENERATED_BODY()

    /** Name of the material parameter to control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FName ParameterName;

    /** Field path for color data (e.g., "color", "values.rgb") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FString ColorField = TEXT("color");

    /** Field path for alpha/intensity (optional, e.g., "intensity") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FString AlphaField;

    /** Color multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    float ColorMultiplier = 1.0f;

    /** Apply color in HDR (values can exceed 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    bool bHDR = true;

    /** Smoothing factor for color transitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float Smoothing = 0.0f;

    /** Whether this binding is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    bool bEnabled = true;

    // Runtime state
    FLinearColor CurrentColor = FLinearColor::Black;
    FLinearColor TargetColor = FLinearColor::Black;
};

/** Binding for texture parameter (switches between textures based on value) */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipMaterialTextureBinding
{
    GENERATED_BODY()

    /** Name of the texture parameter to control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FName ParameterName;

    /** Field path in pulse data for index/selection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FString IndexField = TEXT("textureIndex");

    /** Array of textures to switch between */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    TArray<UTexture*> Textures;

    /** Whether this binding is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    bool bEnabled = true;

    // Runtime state
    int32 CurrentIndex = 0;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMaterialParameterUpdated, FName, ParameterName, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMaterialColorUpdated, FName, ParameterName, FLinearColor, Color);

// ============================================================================
// MATERIAL BINDING COMPONENT
// ============================================================================

/**
 * Comprehensive binding for material instance parameters to rship.
 * Exposes all common PBR material controls plus custom parameter access:
 * - Base Color, Emissive Color with intensity
 * - Roughness, Metallic, Specular
 * - Opacity for translucent materials
 * - UV Tiling and Offset for texture animation
 * - Custom scalar/vector/texture parameters by name
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Material Binding"))
class RSHIPEXEC_API URshipMaterialBinding : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipMaterialBinding();

    // UActorComponent interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** Emitter ID to bind to (e.g., "fixture:light01:intensity") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FString EmitterId;

    /** Scalar parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    TArray<FRshipMaterialScalarBinding> ScalarBindings;

    /** Vector/Color parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    TArray<FRshipMaterialVectorBinding> VectorBindings;

    /** Texture parameter bindings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    TArray<FRshipMaterialTextureBinding> TextureBindings;

    /** Material slots to affect (empty = all slots) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    TArray<int32> MaterialSlots;

    /** Mesh components to affect (empty = all mesh components) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    TArray<FName> MeshComponentNames;

    /** Auto-create dynamic material instances if needed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    bool bAutoCreateDynamicMaterials = true;

    /** Enable tick for smoothed parameters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    bool bEnableTick = true;

    /** Publish rate in Hz (how often to publish material state as emitters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material", meta = (ClampMin = "1", ClampMax = "60"))
    int32 PublishRateHz = 10;

    /** Only publish when values change (reduces network traffic) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    bool bOnlyPublishOnChange = true;

    // ========================================================================
    // RS_ ACTIONS - Generic Parameter Control
    // ========================================================================

    /** Set any scalar parameter by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Parameters")
    void RS_SetScalarParameter(FName ParameterName, float Value);

    /** Set any vector/color parameter by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Parameters")
    void RS_SetVectorParameter(FName ParameterName, float R, float G, float B, float A);

    /** Set texture parameter to a specific index in TextureBindings array */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Parameters")
    void RS_SetTextureIndex(FName ParameterName, int32 Index);

    // ========================================================================
    // RS_ ACTIONS - Common PBR Parameters
    // ========================================================================

    /** Set base color (albedo) - the main diffuse color of the material */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetBaseColor(float R, float G, float B);

    /** Set base color with alpha */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetBaseColorWithAlpha(float R, float G, float B, float A);

    /** Set emissive color - self-illumination color */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetEmissiveColor(float R, float G, float B);

    /** Set emissive intensity multiplier (for HDR glow effects) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetEmissiveIntensity(float Intensity);

    /** Set combined emissive color and intensity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetEmissive(float R, float G, float B, float Intensity);

    /** Set roughness (0 = mirror/glossy, 1 = matte/rough) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetRoughness(float Roughness);

    /** Set metallic (0 = dielectric/non-metal, 1 = metal) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetMetallic(float Metallic);

    /** Set specular (0 = no specular, 0.5 = default, 1 = max specular) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetSpecular(float Specular);

    /** Set opacity (for translucent materials, 0 = transparent, 1 = opaque) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetOpacity(float Opacity);

    /** Set opacity mask threshold (for masked materials) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetOpacityMask(float Threshold);

    /** Set ambient occlusion multiplier */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetAmbientOcclusion(float AO);

    /** Set normal intensity/strength */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|PBR")
    void RS_SetNormalIntensity(float Intensity);

    // ========================================================================
    // RS_ ACTIONS - UV/Texture Animation
    // ========================================================================

    /** Set UV tiling (scale) for texture coordinates */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|UV")
    void RS_SetUVTiling(float TileU, float TileV);

    /** Set UV offset for texture scrolling */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|UV")
    void RS_SetUVOffset(float OffsetU, float OffsetV);

    /** Set UV rotation in degrees */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|UV")
    void RS_SetUVRotation(float Degrees);

    /** Set UV pivot point for rotation */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|UV")
    void RS_SetUVPivot(float PivotU, float PivotV);

    // ========================================================================
    // RS_ ACTIONS - Subsurface/Cloth/Special
    // ========================================================================

    /** Set subsurface color (for skin, wax, etc.) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Special")
    void RS_SetSubsurfaceColor(float R, float G, float B);

    /** Set subsurface intensity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Special")
    void RS_SetSubsurfaceIntensity(float Intensity);

    /** Set cloth sheen color (for fabric materials) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Special")
    void RS_SetSheenColor(float R, float G, float B);

    /** Set clear coat intensity (for car paint, etc.) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Special")
    void RS_SetClearCoat(float Intensity);

    /** Set clear coat roughness */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Special")
    void RS_SetClearCoatRoughness(float Roughness);

    // ========================================================================
    // RS_ ACTIONS - Utility
    // ========================================================================

    /** Reset all parameters to material defaults */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Utility")
    void RS_ResetToDefaults();

    /** Set global intensity multiplier for all color parameters */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Utility")
    void RS_SetGlobalIntensity(float Intensity);

    /** Set global color tint applied to all color parameters */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Utility")
    void RS_SetGlobalTint(float R, float G, float B);

    /** Lerp all parameters toward another material's values */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material|Utility")
    void RS_BlendToDefaults(float Alpha);

    // ========================================================================
    // RS_ EMITTERS - State Publishing
    // ========================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRS_ScalarParameterEmitter, FName, ParameterName, float, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FRS_VectorParameterEmitter, FName, ParameterName, float, R, float, G, float, B, float, A);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_FloatEmitter, float, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRS_ColorEmitter, float, R, float, G, float, B);

    // PBR state emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_ColorEmitter RS_OnBaseColorChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_ColorEmitter RS_OnEmissiveColorChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_FloatEmitter RS_OnEmissiveIntensityChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_FloatEmitter RS_OnRoughnessChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_FloatEmitter RS_OnMetallicChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_FloatEmitter RS_OnSpecularChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_FloatEmitter RS_OnOpacityChanged;

    // Generic parameter change emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_ScalarParameterEmitter RS_OnScalarParameterChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FRS_VectorParameterEmitter RS_OnVectorParameterChanged;

    // ========================================================================
    // RUNTIME CONTROL (existing methods)
    // ========================================================================

    /** Set the emitter ID at runtime */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    void SetEmitterId(const FString& NewEmitterId);

    /** Manually set a scalar parameter value (bypasses binding) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    void SetScalarValue(FName ParameterName, float Value);

    /** Manually set a vector parameter value (bypasses binding) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    void SetVectorValue(FName ParameterName, FLinearColor Value);

    /** Force refresh all dynamic materials */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    void RefreshMaterials();

    /** Get all dynamic material instances being controlled */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    TArray<UMaterialInstanceDynamic*> GetDynamicMaterials() const { return DynamicMaterials; }

    /** Force publish all current values */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    void ForcePublish();

    /** Get current material state as JSON */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    FString GetMaterialStateJson() const;

    // ========================================================================
    // EVENTS (existing)
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material")
    FOnMaterialParameterUpdated OnScalarUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material")
    FOnMaterialColorUpdated OnColorUpdated;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> DynamicMaterials;

    FDelegateHandle PulseHandle;

    // State tracking for change detection
    double LastPublishTime = 0.0;
    double PublishInterval = 0.1;
    FLinearColor LastBaseColor = FLinearColor::Black;
    FLinearColor LastEmissiveColor = FLinearColor::Black;
    float LastEmissiveIntensity = 0.0f;
    float LastRoughness = 0.0f;
    float LastMetallic = 0.0f;
    float LastSpecular = 0.5f;
    float LastOpacity = 1.0f;
    float GlobalIntensityMultiplier = 1.0f;
    FLinearColor GlobalTint = FLinearColor::White;

    // Cached default values for reset
    TMap<FName, float> DefaultScalarValues;
    TMap<FName, FLinearColor> DefaultVectorValues;

    void SetupMaterials();
    void BindToPulseReceiver();
    void UnbindFromPulseReceiver();
    void OnPulseReceived(const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data);

    float ProcessScalarBinding(const FRshipMaterialScalarBinding& Binding, float InputValue);
    FLinearColor ProcessVectorBinding(const FRshipMaterialVectorBinding& Binding, const FLinearColor& InputColor, float Alpha);

    float ExtractFloatValue(TSharedPtr<FJsonObject> Data, const FString& FieldPath, float Default = 0.0f);
    FLinearColor ExtractColorValue(TSharedPtr<FJsonObject> Data, const FString& FieldPath);

    void ReadAndPublishState();
    bool HasColorChanged(const FLinearColor& OldColor, const FLinearColor& NewColor, float Threshold = 0.001f) const;
    bool HasValueChanged(float OldValue, float NewValue, float Threshold = 0.001f) const;
    void CacheDefaultValues();
};

// ============================================================================
// MATERIAL BINDING MANAGER
// ============================================================================

/**
 * Manager for bulk material binding operations.
 * Provides scene-wide control over reactive materials.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipMaterialManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    /** Register a material binding component */
    void RegisterBinding(URshipMaterialBinding* Binding);

    /** Unregister a material binding component */
    void UnregisterBinding(URshipMaterialBinding* Binding);

    /** Get all registered bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    TArray<URshipMaterialBinding*> GetAllBindings() const { return RegisteredBindings; }

    /** Set global intensity multiplier for all material bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    void SetGlobalIntensityMultiplier(float Multiplier);

    /** Get global intensity multiplier */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Material")
    float GetGlobalIntensityMultiplier() const { return GlobalIntensityMultiplier; }

    /** Set global color tint for all material bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    void SetGlobalColorTint(FLinearColor Tint);

    /** Get global color tint */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Material")
    FLinearColor GetGlobalColorTint() const { return GlobalColorTint; }

    /** Pause all material updates */
    UFUNCTION(BlueprintCallable, Category = "Rship|Material")
    void SetPaused(bool bPaused) { bIsPaused = bPaused; }

    /** Check if updates are paused */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Material")
    bool IsPaused() const { return bIsPaused; }

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    TArray<URshipMaterialBinding*> RegisteredBindings;

    float GlobalIntensityMultiplier = 1.0f;
    FLinearColor GlobalColorTint = FLinearColor::White;
    bool bIsPaused = false;
};
