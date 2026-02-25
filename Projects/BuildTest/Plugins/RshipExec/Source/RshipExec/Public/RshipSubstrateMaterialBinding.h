// Rship Substrate Material Binding
// Advanced material control for Substrate-enabled materials (UE 5.5+)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RshipSubstrateMaterialBinding.generated.h"

class URshipSubsystem;
class URshipPulseReceiver;
class FJsonObject;

// ============================================================================
// SUBSTRATE MATERIAL STATE
// ============================================================================

/**
 * Complete state snapshot of a Substrate material.
 * All parameters that can be controlled via rship.
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipSubstrateMaterialState
{
	GENERATED_BODY()

	// ========================================================================
	// BASE LAYER
	// ========================================================================

	/** Base color RGB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Base")
	FLinearColor BaseColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

	/** Surface roughness (0 = mirror, 1 = diffuse) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Base", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Roughness = 0.5f;

	/** Metallic (0 = dielectric, 1 = full metal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Base", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Metallic = 0.0f;

	/** Specular intensity override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Base", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Specular = 0.5f;

	// ========================================================================
	// EMISSIVE
	// ========================================================================

	/** Emissive color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Emissive")
	FLinearColor EmissiveColor = FLinearColor::Black;

	/** Emissive intensity multiplier (HDR) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Emissive", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float EmissiveIntensity = 0.0f;

	// ========================================================================
	// SUBSURFACE
	// ========================================================================

	/** Subsurface scattering color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Subsurface")
	FLinearColor SubsurfaceColor = FLinearColor::White;

	/** Subsurface scattering strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Subsurface", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SubsurfaceStrength = 0.0f;

	// ========================================================================
	// CLEAR COAT
	// ========================================================================

	/** Clear coat intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|ClearCoat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClearCoat = 0.0f;

	/** Clear coat roughness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|ClearCoat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClearCoatRoughness = 0.1f;

	// ========================================================================
	// ANISOTROPY
	// ========================================================================

	/** Anisotropic reflection strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Anisotropy", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Anisotropy = 0.0f;

	/** Anisotropic reflection rotation (0-1 maps to 0-180 degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Anisotropy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AnisotropyRotation = 0.0f;

	// ========================================================================
	// OPACITY
	// ========================================================================

	/** Overall opacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Opacity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 1.0f;

	/** Opacity mask threshold (for masked materials) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Opacity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpacityMask = 0.5f;

	// ========================================================================
	// FUZZ (CLOTH/VELVET)
	// ========================================================================

	/** Fuzz/cloth amount */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Fuzz", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FuzzAmount = 0.0f;

	/** Fuzz color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Fuzz")
	FLinearColor FuzzColor = FLinearColor::White;

	// ========================================================================
	// NORMAL/DISPLACEMENT
	// ========================================================================

	/** Normal map strength multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Detail", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float NormalStrength = 1.0f;

	/** Displacement/height scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|Detail", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float DisplacementScale = 1.0f;

	// ========================================================================
	// UTILITY
	// ========================================================================

	/** Lerp this state toward another state */
	FRshipSubstrateMaterialState LerpTo(const FRshipSubstrateMaterialState& Target, float Alpha) const;

	/** Create state from JSON pulse data */
	static FRshipSubstrateMaterialState FromJson(const TSharedPtr<FJsonObject>& JsonData);

	/** Convert state to JSON for emitter publishing */
	TSharedPtr<FJsonObject> ToJson() const;
};

// ============================================================================
// SUBSTRATE PRESET
// ============================================================================

/**
 * Named preset containing a complete material state.
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipSubstratePreset
{
	GENERATED_BODY()

	/** Unique preset name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	FString PresetName;

	/** Material state snapshot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	FRshipSubstrateMaterialState State;

	/** Optional description */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	FString Description;
};

// ============================================================================
// TRANSITION CONFIG
// ============================================================================

/**
 * Configuration for smooth state transitions.
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipSubstrateTransitionConfig
{
	GENERATED_BODY()

	/** Transition duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float Duration = 1.0f;

	/** Easing curve (nullptr = linear) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	UCurveFloat* EasingCurve = nullptr;

	/** Whether to use tick-based interpolation (true) or timeline (false) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	bool bUseTickInterpolation = true;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubstrateStateChanged, const FRshipSubstrateMaterialState&, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubstrateTransitionProgress, float, Progress, const FRshipSubstrateMaterialState&, CurrentState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSubstrateTransitionComplete);

// ============================================================================
// SUBSTRATE MATERIAL BINDING COMPONENT
// ============================================================================

/**
 * Component that binds rship pulse data to Substrate material parameters.
 * Provides full control over all Substrate shading properties with smooth transitions.
 */
UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Substrate Binding"))
class RSHIPEXEC_API URshipSubstrateMaterialBinding : public UActorComponent
{
	GENERATED_BODY()

public:
	URshipSubstrateMaterialBinding();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** Target ID for receiving pulse data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	FString TargetId;

	/** Emitter ID to bind to (e.g., "material_state") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	FString EmitterId = TEXT("material");

	/** Material slots to affect (empty = all slots) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	TArray<int32> MaterialSlots;

	/** Mesh components to affect (empty = all mesh components) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	TArray<FName> MeshComponentNames;

	/** Default state when no pulses received */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	FRshipSubstrateMaterialState DefaultState;

	/** Default transition configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	FRshipSubstrateTransitionConfig TransitionConfig;

	/** Saved presets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	TArray<FRshipSubstratePreset> Presets;

	// ========================================================================
	// PARAMETER MAPPING (Optional field overrides)
	// ========================================================================

	/** Custom parameter name for base color (empty = use default "BaseColor") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|ParameterNames")
	FName BaseColorParam = NAME_None;

	/** Custom parameter name for roughness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|ParameterNames")
	FName RoughnessParam = NAME_None;

	/** Custom parameter name for metallic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|ParameterNames")
	FName MetallicParam = NAME_None;

	/** Custom parameter name for emissive color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|ParameterNames")
	FName EmissiveColorParam = NAME_None;

	/** Custom parameter name for emissive intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate|ParameterNames")
	FName EmissiveIntensityParam = NAME_None;

	/** Publish rate in Hz (how often to publish material state as emitters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate", meta = (ClampMin = "1", ClampMax = "60"))
	int32 PublishRateHz = 10;

	/** Only publish when values change (reduces network traffic) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Substrate")
	bool bOnlyPublishOnChange = true;

	// ========================================================================
	// RS_ ACTIONS - Base Layer
	// ========================================================================

	/** Set base color RGB */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Base")
	void RS_SetBaseColor(float R, float G, float B);

	/** Set base color with alpha */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Base")
	void RS_SetBaseColorWithAlpha(float R, float G, float B, float A);

	/** Set surface roughness (0 = mirror, 1 = diffuse) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Base")
	void RS_SetRoughness(float Roughness);

	/** Set metallic (0 = dielectric, 1 = full metal) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Base")
	void RS_SetMetallic(float Metallic);

	/** Set specular intensity */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Base")
	void RS_SetSpecular(float Specular);

	// ========================================================================
	// RS_ ACTIONS - Emissive
	// ========================================================================

	/** Set emissive color */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Emissive")
	void RS_SetEmissiveColor(float R, float G, float B);

	/** Set emissive intensity (HDR multiplier) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Emissive")
	void RS_SetEmissiveIntensity(float Intensity);

	/** Set combined emissive color and intensity */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Emissive")
	void RS_SetEmissive(float R, float G, float B, float Intensity);

	// ========================================================================
	// RS_ ACTIONS - Subsurface
	// ========================================================================

	/** Set subsurface scattering color */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Subsurface")
	void RS_SetSubsurfaceColor(float R, float G, float B);

	/** Set subsurface scattering strength */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Subsurface")
	void RS_SetSubsurfaceStrength(float Strength);

	// ========================================================================
	// RS_ ACTIONS - Clear Coat
	// ========================================================================

	/** Set clear coat intensity */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|ClearCoat")
	void RS_SetClearCoat(float Intensity);

	/** Set clear coat roughness */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|ClearCoat")
	void RS_SetClearCoatRoughness(float Roughness);

	// ========================================================================
	// RS_ ACTIONS - Anisotropy
	// ========================================================================

	/** Set anisotropic reflection strength (-1 to 1) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Anisotropy")
	void RS_SetAnisotropy(float Anisotropy);

	/** Set anisotropic reflection rotation (0-1 maps to 0-180 degrees) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Anisotropy")
	void RS_SetAnisotropyRotation(float Rotation);

	// ========================================================================
	// RS_ ACTIONS - Opacity
	// ========================================================================

	/** Set overall opacity */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Opacity")
	void RS_SetOpacity(float Opacity);

	/** Set opacity mask threshold */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Opacity")
	void RS_SetOpacityMask(float Threshold);

	// ========================================================================
	// RS_ ACTIONS - Fuzz (Cloth/Velvet)
	// ========================================================================

	/** Set fuzz/cloth amount */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Fuzz")
	void RS_SetFuzzAmount(float Amount);

	/** Set fuzz color */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Fuzz")
	void RS_SetFuzzColor(float R, float G, float B);

	// ========================================================================
	// RS_ ACTIONS - Detail
	// ========================================================================

	/** Set normal map strength multiplier */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Detail")
	void RS_SetNormalStrength(float Strength);

	/** Set displacement/height scale */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Detail")
	void RS_SetDisplacementScale(float Scale);

	// ========================================================================
	// RS_ ACTIONS - Transitions & Presets
	// ========================================================================

	/** Transition to a named preset */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Transition")
	void RS_TransitionToPreset(const FString& PresetName, float Duration);

	/** Set the default transition duration */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Transition")
	void RS_SetTransitionDuration(float Duration);

	/** Next preset in list */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Transition")
	void RS_NextPreset();

	/** Previous preset in list */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Transition")
	void RS_PreviousPreset();

	// ========================================================================
	// RS_ ACTIONS - Utility
	// ========================================================================

	/** Reset to default state */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Utility")
	void RS_ResetToDefault();

	/** Set global intensity multiplier for all parameters */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate|Utility")
	void RS_SetGlobalIntensity(float Intensity);

	// ========================================================================
	// RS_ EMITTERS - State Publishing
	// ========================================================================

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRS_ColorEmitter, float, R, float, G, float, B);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_FloatEmitter, float, Value);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRS_StringEmitter, const FString&, Value);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRS_TransitionEmitter, const FString&, PresetName, float, Progress);

	// Base layer emitters
	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_ColorEmitter RS_OnBaseColorChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_FloatEmitter RS_OnRoughnessChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_FloatEmitter RS_OnMetallicChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_FloatEmitter RS_OnSpecularChanged;

	// Emissive emitters
	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_ColorEmitter RS_OnEmissiveColorChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_FloatEmitter RS_OnEmissiveIntensityChanged;

	// Opacity emitters
	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_FloatEmitter RS_OnOpacityChanged;

	// Transition emitters
	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_StringEmitter RS_OnPresetChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_TransitionEmitter RS_OnTransitionProgressEmitter;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate|Emitters")
	FRS_FloatEmitter RS_OnGlobalIntensityChanged;

	// ========================================================================
	// RUNTIME STATE
	// ========================================================================

	/** Get current material state */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Substrate")
	FRshipSubstrateMaterialState GetCurrentState() const { return CurrentState; }

	/** Get target state (during transition) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Substrate")
	FRshipSubstrateMaterialState GetTargetState() const { return TargetState; }

	/** Check if a transition is in progress */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Substrate")
	bool IsTransitioning() const { return bIsTransitioning; }

	/** Get transition progress (0-1) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Substrate")
	float GetTransitionProgress() const { return TransitionProgress; }

	/** Force publish all current values */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void ForcePublish();

	/** Get current material state as JSON */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	FString GetSubstrateStateJson() const;

	// ========================================================================
	// RUNTIME CONTROL
	// ========================================================================

	/** Set the complete material state immediately */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void SetState(const FRshipSubstrateMaterialState& NewState);

	/** Transition to a new state over time */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void TransitionToState(const FRshipSubstrateMaterialState& NewState, float Duration = -1.0f);

	/** Transition to a named preset */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	bool TransitionToPreset(const FString& PresetName, float Duration = -1.0f);

	/** Crossfade between two presets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	bool CrossfadePresets(const FString& PresetA, const FString& PresetB, float Alpha);

	/** Cancel any in-progress transition */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void CancelTransition();

	/** Save current state as a preset */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void SaveCurrentAsPreset(const FString& PresetName);

	/** Delete a preset by name */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	bool DeletePreset(const FString& PresetName);

	/** Get a preset by name */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	bool GetPreset(const FString& PresetName, FRshipSubstratePreset& OutPreset) const;

	/** Refresh dynamic material instances */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void RefreshMaterials();

	// ========================================================================
	// SUBSTRATE DETECTION
	// ========================================================================

	/** Check if a material is Substrate-enabled */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Substrate")
	static bool IsSubstrateMaterial(UMaterialInterface* Material);

	/** Get all Substrate materials from this actor */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	TArray<UMaterialInstanceDynamic*> GetSubstrateMaterials() const;

	/** Get all dynamic material instances being controlled */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	TArray<UMaterialInstanceDynamic*> GetDynamicMaterials() const { return DynamicMaterials; }

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when material state changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate")
	FOnSubstrateStateChanged OnStateChanged;

	/** Fired during state transition with progress */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate")
	FOnSubstrateTransitionProgress OnTransitionProgress;

	/** Fired when a transition completes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Substrate")
	FOnSubstrateTransitionComplete OnTransitionComplete;

private:
	UPROPERTY()
	URshipSubsystem* Subsystem;

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> DynamicMaterials;

	FDelegateHandle PulseHandle;

	// State management
	FRshipSubstrateMaterialState CurrentState;
	FRshipSubstrateMaterialState TargetState;
	FRshipSubstrateMaterialState TransitionStartState;
	bool bIsTransitioning = false;
	float TransitionProgress = 0.0f;
	float TransitionDuration = 1.0f;

	// Setup and binding
	void SetupMaterials();
	void BindToPulseReceiver();
	void UnbindFromPulseReceiver();
	void OnPulseReceived(const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data);

	// Apply state to materials
	void ApplyStateToMaterials(const FRshipSubstrateMaterialState& State);

	// Get parameter name with fallback
	FName GetParamName(FName CustomName, const TCHAR* DefaultName) const;
};

// ============================================================================
// SUBSTRATE MATERIAL MANAGER
// ============================================================================

/**
 * Manager for bulk Substrate material operations.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipSubstrateMaterialManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(URshipSubsystem* InSubsystem);
	void Shutdown();
	void Tick(float DeltaTime);

	/** Register a Substrate binding component */
	void RegisterBinding(URshipSubstrateMaterialBinding* Binding);

	/** Unregister a Substrate binding component */
	void UnregisterBinding(URshipSubstrateMaterialBinding* Binding);

	/** Get all registered Substrate bindings */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	TArray<URshipSubstrateMaterialBinding*> GetAllBindings() const { return RegisteredBindings; }

	/** Transition all bindings to a preset */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void TransitionAllToPreset(const FString& PresetName, float Duration = 1.0f);

	/** Add or update a global preset */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void AddPreset(const FRshipSubstratePreset& Preset);

	/** Get all global presets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	TArray<FRshipSubstratePreset> GetGlobalPresets() const { return GlobalPresets; }

	/** Get a global preset by name */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	bool GetGlobalPreset(const FString& PresetName, FRshipSubstratePreset& OutPreset) const;

	/** Set global master brightness for all Substrate materials */
	UFUNCTION(BlueprintCallable, Category = "Rship|Substrate")
	void SetGlobalMasterBrightness(float Brightness);

	/** Get global master brightness */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Substrate")
	float GetGlobalMasterBrightness() const { return GlobalMasterBrightness; }

private:
	UPROPERTY()
	URshipSubsystem* Subsystem;

	UPROPERTY()
	TArray<URshipSubstrateMaterialBinding*> RegisteredBindings;

	UPROPERTY()
	TArray<FRshipSubstratePreset> GlobalPresets;

	float GlobalMasterBrightness = 1.0f;
};
