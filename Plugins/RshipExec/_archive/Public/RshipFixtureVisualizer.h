// Rship Fixture Visualizer Component
// Real-time visualization of fixture state including beam cones, colors, and editor gizmos

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureLightProfile.h"
#include "RshipPulseReceiver.h"
#include "RshipFixtureVisualizer.generated.h"

class URshipSubsystem;
class URshipPulseApplicator;
class UMaterialInstanceDynamic;
class USplineMeshComponent;

// ============================================================================
// VISUALIZATION MODES
// ============================================================================

UENUM(BlueprintType)
enum class ERshipVisualizationMode : uint8
{
    None            UMETA(DisplayName = "None"),
    BeamCone        UMETA(DisplayName = "Beam Cone Only"),
    BeamVolume      UMETA(DisplayName = "Volumetric Beam"),
    Symbol          UMETA(DisplayName = "Symbol/Icon Only"),
    Full            UMETA(DisplayName = "Full (Beam + Symbol)")
};

UENUM(BlueprintType)
enum class ERshipBeamQuality : uint8
{
    Low             UMETA(DisplayName = "Low (16 segments)"),
    Medium          UMETA(DisplayName = "Medium (32 segments)"),
    High            UMETA(DisplayName = "High (64 segments)"),
    Ultra           UMETA(DisplayName = "Ultra (128 segments)")
};

UENUM(BlueprintType)
enum class ERshipLODMode : uint8
{
    Off             UMETA(DisplayName = "Off (Always Full Quality)"),
    Auto            UMETA(DisplayName = "Auto (Distance-Based)"),
    Forced          UMETA(DisplayName = "Forced LOD Level")
};

// ============================================================================
// COLOR TEMPERATURE UTILITIES
// ============================================================================

/**
 * Utility functions for color temperature (Kelvin) conversion.
 * Based on CIE 1931 2-degree standard observer.
 */
struct RSHIPEXEC_API FRshipColorTemperature
{
    /**
     * Convert Kelvin temperature to linear RGB color.
     * @param Kelvin Color temperature in Kelvin (1000-40000)
     * @return Linear RGB color
     */
    static FLinearColor KelvinToRGB(float Kelvin);

    /**
     * Estimate Kelvin from RGB color (approximate).
     * @param Color Linear RGB color
     * @return Estimated Kelvin temperature
     */
    static float RGBToKelvin(const FLinearColor& Color);

    /**
     * Common color temperature presets (in Kelvin)
     */
    static constexpr float Candle = 1850.0f;
    static constexpr float Tungsten40W = 2600.0f;
    static constexpr float Tungsten100W = 2850.0f;
    static constexpr float Halogen = 3200.0f;
    static constexpr float CarbonArc = 5200.0f;
    static constexpr float Daylight = 5600.0f;
    static constexpr float Overcast = 6500.0f;
    static constexpr float BlueSky = 10000.0f;
};

// ============================================================================
// LOD SETTINGS
// ============================================================================

USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipVisualizationLOD
{
    GENERATED_BODY()

    /** LOD mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|LOD")
    ERshipLODMode Mode = ERshipLODMode::Auto;

    /** Distance for LOD0 (full quality) in units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|LOD", meta = (EditCondition = "Mode == ERshipLODMode::Auto"))
    float LOD0Distance = 1000.0f;

    /** Distance for LOD1 (medium quality) in units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|LOD", meta = (EditCondition = "Mode == ERshipLODMode::Auto"))
    float LOD1Distance = 3000.0f;

    /** Distance for LOD2 (low quality) in units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|LOD", meta = (EditCondition = "Mode == ERshipLODMode::Auto"))
    float LOD2Distance = 6000.0f;

    /** Distance beyond which visualization is hidden */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|LOD", meta = (EditCondition = "Mode == ERshipLODMode::Auto"))
    float CullDistance = 15000.0f;

    /** Forced LOD level (0=best, 3=worst) when Mode is Forced */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|LOD", meta = (EditCondition = "Mode == ERshipLODMode::Forced", ClampMin = "0", ClampMax = "3"))
    int32 ForcedLODLevel = 0;
};

// ============================================================================
// IES PROFILE SETTINGS
// ============================================================================

USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipIESSettings
{
    GENERATED_BODY()

    /** Enable IES profile visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|IES")
    bool bEnableIES = true;

    /** IES profile texture (auto-loaded from fixture library if available) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|IES")
    UTextureLightProfile* IESTexture = nullptr;

    /** Intensity multiplier for IES visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|IES", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float IESIntensityMultiplier = 1.0f;

    /** Show IES distribution preview in editor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|IES")
    bool bShowIESPreview = false;
};

// ============================================================================
// GOBO SETTINGS
// ============================================================================

USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipGoboSettings
{
    GENERATED_BODY()

    /** Enable gobo projection visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|Gobo")
    bool bEnableGobo = true;

    /** Array of gobo textures (index 0 = open, 1+ = gobos) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|Gobo")
    TArray<UTexture2D*> GoboTextures;

    /** Gobo rotation speed multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|Gobo", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float RotationSpeedMultiplier = 1.0f;

    /** Gobo projection sharpness */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|Gobo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ProjectionSharpness = 0.8f;
};

// ============================================================================
// BEAM SETTINGS
// ============================================================================

USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipBeamSettings
{
    GENERATED_BODY()

    /** Length of the beam visualization in units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    float BeamLength = 1000.0f;

    /** Opacity of the beam cone (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BeamOpacity = 0.15f;

    /** Whether beam opacity scales with intensity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bScaleOpacityWithIntensity = true;

    /** Inner cone opacity multiplier (brighter core) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float InnerConeMultiplier = 1.5f;

    /** Quality/resolution of the beam cone mesh */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    ERshipBeamQuality Quality = ERshipBeamQuality::Medium;

    /** Show gobo pattern projection (if available) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bShowGoboProjection = true;

    /** Show beam edge highlight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bShowBeamEdge = true;

    /** Falloff curve for beam intensity along length */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization", meta = (ClampMin = "0.1", ClampMax = "4.0"))
    float FalloffExponent = 2.0f;
};

// ============================================================================
// SYMBOL SETTINGS
// ============================================================================

USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipSymbolSettings
{
    GENERATED_BODY()

    /** Size of the fixture symbol in units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    float SymbolSize = 50.0f;

    /** Always face the camera (billboard mode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bBillboard = false;

    /** Show fixture name label */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bShowLabel = true;

    /** Show DMX address (if assigned) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bShowAddress = false;

    /** Show intensity value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bShowIntensity = true;

    /** Color when fixture is at 0% */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    FLinearColor OffColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);

    /** Symbol visibility distance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    float MaxDrawDistance = 10000.0f;
};

// ============================================================================
// FIXTURE VISUALIZER COMPONENT
// ============================================================================

/**
 * Component that visualizes fixture state in the editor and at runtime.
 * Renders beam cones, gobo projections, and fixture symbols with real-time updates.
 */
UCLASS(ClassGroup=(Rship), meta=(BlueprintSpawnableComponent))
class RSHIPEXEC_API URshipFixtureVisualizer : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipFixtureVisualizer();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** The fixture ID this visualizer represents */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    FString FixtureId;

    /** Visualization mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    ERshipVisualizationMode Mode = ERshipVisualizationMode::Full;

    /** Beam visualization settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    FRshipBeamSettings BeamSettings;

    /** Symbol visualization settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    FRshipSymbolSettings SymbolSettings;

    /** Whether to show visualization in editor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bShowInEditor = true;

    /** Whether to show visualization at runtime */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    bool bShowAtRuntime = false;

    /** Link to pulse applicator for automatic state sync */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization")
    URshipPulseApplicator* LinkedApplicator;

    /** LOD settings for distance-based quality reduction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|LOD")
    FRshipVisualizationLOD LODSettings;

    /** IES profile visualization settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|IES")
    FRshipIESSettings IESSettings;

    /** Gobo projection settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|Gobo")
    FRshipGoboSettings GoboSettings;

    /** Use color temperature (Kelvin) instead of RGB */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|Color")
    bool bUseColorTemperature = false;

    /** Default color temperature in Kelvin (used when bUseColorTemperature is true) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Visualization|Color", meta = (EditCondition = "bUseColorTemperature", ClampMin = "1000", ClampMax = "40000"))
    float DefaultColorTemperature = 5600.0f;

    // ========================================================================
    // MANUAL STATE CONTROL
    // ========================================================================

    /** Manually set intensity (0-1) - overrides pulse data if set */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetIntensity(float Intensity);

    /** Manually set color - overrides pulse data if set */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetColor(FLinearColor Color);

    /** Manually set beam angle (degrees) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetBeamAngle(float OuterAngle, float InnerAngle = -1.0f);

    /** Manually set pan/tilt (degrees) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetPanTilt(float Pan, float Tilt);

    /** Manually set gobo (0 = open, 1+ = gobo index) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetGobo(int32 GoboIndex, float Rotation = 0.0f);

    /** Reset to automatic mode (follow pulse data) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void ResetToAutomatic();

    // ========================================================================
    // STATE QUERIES
    // ========================================================================

    /** Get current visualized intensity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    float GetCurrentIntensity() const { return CurrentIntensity; }

    /** Get current visualized color */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    FLinearColor GetCurrentColor() const { return CurrentColor; }

    /** Get current beam angles */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void GetBeamAngles(float& OuterAngle, float& InnerAngle) const;

    /** Manually set color temperature in Kelvin */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetColorTemperature(float Kelvin);

    /** Get current color temperature in Kelvin */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    float GetColorTemperature() const { return CurrentColorTemperature; }

    /** Get current LOD level (0=best, 3=worst/culled) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    int32 GetCurrentLODLevel() const { return CurrentLODLevel; }

    /** Get distance to active camera (for LOD calculation) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    float GetDistanceToCamera() const { return CachedCameraDistance; }

    // ========================================================================
    // VISIBILITY
    // ========================================================================

    /** Force rebuild of visualization geometry */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void RebuildVisualization();

    /** Show/hide the visualization */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetVisualizationVisible(bool bVisible);

    /** Check if visualization is currently visible */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    bool IsVisualizationVisible() const { return bIsVisible; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Visualization components
    UPROPERTY()
    UProceduralMeshComponent* BeamMesh;

    UPROPERTY()
    UProceduralMeshComponent* InnerBeamMesh;

    UPROPERTY()
    UStaticMeshComponent* SymbolMesh;

    // Materials
    UPROPERTY()
    UMaterialInstanceDynamic* BeamMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* InnerBeamMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* SymbolMaterial;

    // Current state
    float CurrentIntensity = 0.0f;
    FLinearColor CurrentColor = FLinearColor::White;
    float CurrentColorTemperature = 5600.0f;
    float CurrentOuterAngle = 30.0f;
    float CurrentInnerAngle = 20.0f;
    float CurrentPan = 0.0f;
    float CurrentTilt = 0.0f;
    int32 CurrentGobo = 0;
    float CurrentGoboRotation = 0.0f;

    // LOD state
    int32 CurrentLODLevel = 0;
    float CachedCameraDistance = 0.0f;

    // Manual override flags
    bool bManualIntensity = false;
    bool bManualColor = false;
    bool bManualColorTemperature = false;
    bool bManualAngle = false;
    bool bManualPanTilt = false;
    bool bManualGobo = false;

    // Visibility state
    bool bIsVisible = true;
    bool bNeedsRebuild = true;

    // Pulse delegate handle
    FDelegateHandle PulseReceivedHandle;

    // Initialization
    void InitializeVisualization();
    void CreateBeamMesh();
    void CreateSymbolMesh();
    void CreateMaterials();

    // Updates
    void UpdateFromPulse(const FRshipFixturePulse& Pulse);
    void UpdateBeamGeometry();
    void UpdateMaterialParameters();
    void UpdateSymbol();

    // Pulse event handler
    UFUNCTION()
    void OnPulseReceived(const FString& InFixtureId, const FRshipFixturePulse& Pulse);

    // Geometry generation
    void GenerateConeMesh(TArray<FVector>& Vertices, TArray<int32>& Triangles,
                          TArray<FVector>& Normals, TArray<FVector2D>& UVs,
                          float Angle, float Length, int32 Segments, bool bInnerCone);

    // Get segment count from quality setting
    int32 GetSegmentCount() const;

    // Get segment count adjusted for LOD level
    int32 GetSegmentCountForLOD(int32 LODLevel) const;

    // Find linked applicator automatically
    void FindLinkedApplicator();

    // Calculate LOD level based on camera distance
    void UpdateLOD();

    // Calculate distance to active camera
    float CalculateCameraDistance() const;

    // Apply gobo texture to beam material
    void UpdateGoboTexture();

    // Apply IES profile to visualization
    void UpdateIESVisualization();
};

// ============================================================================
// VISUALIZATION MANAGER (GLOBAL)
// ============================================================================

/**
 * Manager for controlling all fixture visualizations globally.
 * Useful for switching modes during different workflow phases.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipVisualizationManager : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize with subsystem reference */
    void Initialize(URshipSubsystem* InSubsystem);

    /** Shutdown and cleanup */
    void Shutdown();

    /** Tick for animation updates */
    void Tick(float DeltaTime);

    // ========================================================================
    // GLOBAL CONTROL
    // ========================================================================

    /** Set visualization mode for all fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetGlobalMode(ERshipVisualizationMode Mode);

    /** Show/hide all fixture visualizations */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetGlobalVisibility(bool bVisible);

    /** Set beam opacity for all fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetGlobalBeamOpacity(float Opacity);

    /** Set beam length for all fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void SetGlobalBeamLength(float Length);

    // ========================================================================
    // REGISTRATION
    // ========================================================================

    /** Register a visualizer with the manager */
    void RegisterVisualizer(URshipFixtureVisualizer* Visualizer);

    /** Unregister a visualizer */
    void UnregisterVisualizer(URshipFixtureVisualizer* Visualizer);

    /** Get all registered visualizers */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    TArray<URshipFixtureVisualizer*> GetAllVisualizers() const;

    /** Get visualizer by fixture ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    URshipFixtureVisualizer* GetVisualizerForFixture(const FString& FixtureId) const;

    // ========================================================================
    // PRESETS
    // ========================================================================

    /** Apply "programming" preset - full visualization, high opacity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void ApplyProgrammingPreset();

    /** Apply "preview" preset - beam cones only, medium opacity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void ApplyPreviewPreset();

    /** Apply "show" preset - minimal visualization, low opacity */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void ApplyShowPreset();

    /** Apply "off" preset - no visualization */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    void ApplyOffPreset();

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    TArray<URshipFixtureVisualizer*> RegisteredVisualizers;

    // Current global settings
    ERshipVisualizationMode GlobalMode = ERshipVisualizationMode::Full;
    bool bGlobalVisibility = true;
    float GlobalBeamOpacity = 0.15f;
    float GlobalBeamLength = 1000.0f;
};
