// Rship Material Parameter Controller
// Action-driven material parameter controller

#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RshipMaterialController.generated.h"

class UTexture;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMaterialParameterUpdated, FName, ParameterName, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMaterialColorUpdated, FName, ParameterName, FLinearColor, Color);

// ============================================================================
// MATERIAL CONTROLLER COMPONENT
// ============================================================================

/**
 * Comprehensive controller for material instance parameters to rship.
 * Exposes all common PBR material controls plus custom parameter access:
 * - Base Color, Emissive Color with intensity
 * - Roughness, Metallic, Specular
 * - Opacity for translucent materials
 * - UV Tiling and Offset for texture animation
 * - Custom scalar/vector/texture parameters by name
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Material Controller"))
class RSHIPEXEC_API URshipMaterialController : public URshipControllerComponent
{
    GENERATED_BODY()

public:
    URshipMaterialController();

    // UActorComponent interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** Child target suffix used for material controls (defaults to "material") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material")
    FString ChildTargetSuffix = TEXT("material");

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
    //  ACTIONS - Generic Parameter Control
    // ========================================================================

    /** Set any scalar parameter by name */
    UFUNCTION()
    void SetScalarParameter(FName ParameterName, float Value);

    /** Set any vector/color parameter by name */
    UFUNCTION()
    void SetVectorParameter(FName ParameterName, float R, float G, float B, float A);

    /** Optional texture choices keyed by parameter name for SetTextureIndex. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Material|Parameters")
    TMap<FName, TArray<UTexture*>> TextureParameterOptions;

    /** Set texture parameter to a specific index in TextureParameterOptions. */
    UFUNCTION()
    void SetTextureIndex(FName ParameterName, int32 Index);

    // ========================================================================
    //  ACTIONS - Common PBR Parameters
    // ========================================================================

    /** Set base color (albedo) - the main diffuse color of the material */
    UFUNCTION()
    void SetBaseColor(float R, float G, float B);

    /** Set base color with alpha */
    UFUNCTION()
    void SetBaseColorWithAlpha(float R, float G, float B, float A);

    /** Set emissive color - self-illumination color */
    UFUNCTION()
    void SetEmissiveColor(float R, float G, float B);

    /** Set emissive intensity multiplier (for HDR glow effects) */
    UFUNCTION()
    void SetEmissiveIntensity(float Intensity);

    /** Set combined emissive color and intensity */
    UFUNCTION()
    void SetEmissive(float R, float G, float B, float Intensity);

    /** Set roughness (0 = mirror/glossy, 1 = matte/rough) */
    UFUNCTION()
    void SetRoughness(float Roughness);

    /** Set metallic (0 = dielectric/non-metal, 1 = metal) */
    UFUNCTION()
    void SetMetallic(float Metallic);

    /** Set specular (0 = no specular, 0.5 = default, 1 = max specular) */
    UFUNCTION()
    void SetSpecular(float Specular);

    /** Set opacity (for translucent materials, 0 = transparent, 1 = opaque) */
    UFUNCTION()
    void SetOpacity(float Opacity);

    /** Set opacity mask threshold (for masked materials) */
    UFUNCTION()
    void SetOpacityMask(float Threshold);

    /** Set ambient occlusion multiplier */
    UFUNCTION()
    void SetAmbientOcclusion(float AO);

    /** Set normal intensity/strength */
    UFUNCTION()
    void SetNormalIntensity(float Intensity);

    // ========================================================================
    //  ACTIONS - UV/Texture Animation
    // ========================================================================

    /** Set UV tiling (scale) for texture coordinates */
    UFUNCTION()
    void SetUVTiling(float TileU, float TileV);

    /** Set UV offset for texture scrolling */
    UFUNCTION()
    void SetUVOffset(float OffsetU, float OffsetV);

    /** Set UV rotation in degrees */
    UFUNCTION()
    void SetUVRotation(float Degrees);

    /** Set UV pivot point for rotation */
    UFUNCTION()
    void SetUVPivot(float PivotU, float PivotV);

    // ========================================================================
    //  ACTIONS - Subsurface/Cloth/Special
    // ========================================================================

    /** Set subsurface color (for skin, wax, etc.) */
    UFUNCTION()
    void SetSubsurfaceColor(float R, float G, float B);

    /** Set subsurface intensity */
    UFUNCTION()
    void SetSubsurfaceIntensity(float Intensity);

    /** Set cloth sheen color (for fabric materials) */
    UFUNCTION()
    void SetSheenColor(float R, float G, float B);

    /** Set clear coat intensity (for car paint, etc.) */
    UFUNCTION()
    void SetClearCoat(float Intensity);

    /** Set clear coat roughness */
    UFUNCTION()
    void SetClearCoatRoughness(float Roughness);

    // ========================================================================
    //  ACTIONS - Utility
    // ========================================================================

    /** Reset all parameters to material defaults */
    UFUNCTION()
    void ResetToDefaults();

    /** Set global intensity multiplier for all color parameters */
    UFUNCTION()
    void SetGlobalIntensity(float Intensity);

    /** Set global color tint applied to all color parameters */
    UFUNCTION()
    void SetGlobalTint(float R, float G, float B);

    /** Lerp all parameters toward another material's values */
    UFUNCTION()
    void BlendToDefaults(float Alpha);

    // ========================================================================
    //  EMITTERS - State Publishing
    // ========================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FScalarParameterEmitter, FName, ParameterName, float, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FVectorParameterEmitter, FName, ParameterName, float, R, float, G, float, B, float, A);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFloatEmitter, float, Value);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FColorEmitter, float, R, float, G, float, B);

    // PBR state emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FColorEmitter OnBaseColorChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FColorEmitter OnEmissiveColorChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FFloatEmitter OnEmissiveIntensityChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FFloatEmitter OnRoughnessChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FFloatEmitter OnMetallicChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FFloatEmitter OnSpecularChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FFloatEmitter OnOpacityChanged;

    // Generic parameter change emitters
    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FScalarParameterEmitter OnScalarParameterChanged;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material|Emitters")
    FVectorParameterEmitter OnVectorParameterChanged;

    // ========================================================================
    // RUNTIME CONTROL (existing methods)
    // ========================================================================

    /** Manually set a scalar parameter value (direct write). */
    UFUNCTION()
    void SetScalarValue(FName ParameterName, float Value);

    /** Manually set a vector parameter value (direct write). */
    UFUNCTION()
    void SetVectorValue(FName ParameterName, FLinearColor Value);

    /** Force refresh all dynamic materials */
    UFUNCTION()
    void RefreshMaterials();

    /** Get all dynamic material instances being controlled */
    UFUNCTION()
    TArray<UMaterialInstanceDynamic*> GetDynamicMaterials() const { return DynamicMaterials; }

    /** Force publish all current values */
    UFUNCTION()
    void ForcePublish();

    /** Get current material state as JSON */
    UFUNCTION()
    FString GetMaterialStateJson() const;

    // ========================================================================
    // EVENTS (existing)
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material")
    FOnMaterialParameterUpdated OnScalarUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Rship|Material")
    FOnMaterialColorUpdated OnColorUpdated;

private:
    virtual void RegisterOrRefreshTarget() override;

    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> DynamicMaterials;

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

    void ReadAndPublishState();
    bool HasColorChanged(const FLinearColor& OldColor, const FLinearColor& NewColor, float Threshold = 0.001f) const;
    bool HasValueChanged(float OldValue, float NewValue, float Threshold = 0.001f) const;
    void CacheDefaultValues();
};
