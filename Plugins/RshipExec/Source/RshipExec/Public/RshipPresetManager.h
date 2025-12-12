// Rocketship Preset Manager
// Save and recall emitter states with optional interpolation

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "RshipPresetManager.generated.h"

// Forward declarations
class URshipSubsystem;
class URshipTargetComponent;

/**
 * Snapshot of a single emitter's values at a point in time
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipEmitterSnapshot
{
	GENERATED_BODY()

	/** Full emitter ID (ServiceId:TargetId:EmitterId) */
	UPROPERTY(BlueprintReadWrite, Category = "Preset")
	FString EmitterId;

	/** Target this emitter belongs to */
	UPROPERTY(BlueprintReadWrite, Category = "Preset")
	FString TargetId;

	/** Emitter name (without target prefix) */
	UPROPERTY(BlueprintReadWrite, Category = "Preset")
	FString EmitterName;

	/** JSON string of the captured values */
	UPROPERTY(BlueprintReadWrite, Category = "Preset")
	FString ValuesJson;

	/** When this snapshot was captured */
	UPROPERTY(BlueprintReadOnly, Category = "Preset")
	FDateTime CapturedAt;

	FRshipEmitterSnapshot()
		: CapturedAt(FDateTime::Now())
	{
	}

	bool IsValid() const { return !EmitterId.IsEmpty() && !ValuesJson.IsEmpty(); }
};

/**
 * A complete preset containing multiple emitter snapshots
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipPreset
{
	GENERATED_BODY()

	/** Unique identifier for this preset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FString PresetId;

	/** User-facing display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FString DisplayName;

	/** Optional description */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FString Description;

	/** All emitter snapshots in this preset */
	UPROPERTY(BlueprintReadWrite, Category = "Preset")
	TArray<FRshipEmitterSnapshot> Snapshots;

	/** Tags for organization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	TArray<FString> Tags;

	/** When this preset was created */
	UPROPERTY(BlueprintReadOnly, Category = "Preset")
	FDateTime CreatedAt;

	/** When this preset was last modified */
	UPROPERTY(BlueprintReadOnly, Category = "Preset")
	FDateTime ModifiedAt;

	FRshipPreset()
		: CreatedAt(FDateTime::Now())
		, ModifiedAt(FDateTime::Now())
	{
	}

	bool IsValid() const { return !PresetId.IsEmpty() && !DisplayName.IsEmpty(); }
};

/**
 * Delegates for preset events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRshipPresetRecalled, const FString&, PresetId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRshipPresetInterpolating, float, Progress, const FString&, PresetId);

/**
 * Manages presets for saving and recalling emitter states.
 * Access via URshipSubsystem::GetPresetManager()
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipPresetManager : public UObject
{
	GENERATED_BODY()

public:
	URshipPresetManager();

	/** Initialize with reference to subsystem */
	void Initialize(URshipSubsystem* InSubsystem);

	/** Shutdown */
	void Shutdown();

	// ========================================================================
	// CAPTURE
	// ========================================================================

	/** Capture a preset from specific targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	FRshipPreset CapturePreset(const FString& Name, const TArray<URshipTargetComponent*>& Targets);

	/** Capture a preset from targets with a specific tag */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	FRshipPreset CapturePresetByTag(const FString& Name, const FString& Tag);

	/** Capture a preset from targets in a specific group */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	FRshipPreset CapturePresetByGroup(const FString& Name, const FString& GroupId);

	/** Capture a preset from all targets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	FRshipPreset CapturePresetAll(const FString& Name);

	// ========================================================================
	// RECALL
	// ========================================================================

	/** Recall a preset instantly */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	void RecallPreset(const FRshipPreset& Preset);

	/** Recall a preset by ID */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	void RecallPresetById(const FString& PresetId);

	/** Recall a preset with interpolation over time */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	void RecallPresetWithFade(const FRshipPreset& Preset, float FadeTimeSeconds);

	/** Crossfade between two presets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	void CrossfadePresets(const FRshipPreset& FromPreset, const FRshipPreset& ToPreset, float DurationSeconds);

	/** Stop any active interpolation */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	void StopInterpolation();

	/** Check if interpolation is in progress */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	bool IsInterpolating() const { return bIsInterpolating; }

	/** Get current interpolation progress (0.0-1.0) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	float GetInterpolationProgress() const { return InterpolationProgress; }

	// ========================================================================
	// PRESET MANAGEMENT
	// ========================================================================

	/** Save a preset to storage */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	void SavePreset(const FRshipPreset& Preset);

	/** Delete a preset by ID */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	bool DeletePreset(const FString& PresetId);

	/** Get a preset by ID */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	bool GetPreset(const FString& PresetId, FRshipPreset& OutPreset) const;

	/** Get all saved presets */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	TArray<FRshipPreset> GetAllPresets() const;

	/** Get presets with a specific tag */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	TArray<FRshipPreset> GetPresetsByTag(const FString& Tag) const;

	/** Update an existing preset's metadata (name, description, tags) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	bool UpdatePresetMetadata(const FString& PresetId, const FString& NewName, const FString& NewDescription, const TArray<FString>& NewTags);

	// ========================================================================
	// PERSISTENCE
	// ========================================================================

	/** Save all presets to file */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	bool SavePresetsToFile();

	/** Load presets from file */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	bool LoadPresetsFromFile();

	/** Get the path where presets are saved */
	UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
	static FString GetPresetsSaveFilePath();

	// ========================================================================
	// EMITTER VALUE CACHE (called internally)
	// ========================================================================

	/** Cache the last value sent for an emitter (called by subsystem during pulse) */
	void CacheEmitterValue(const FString& TargetId, const FString& EmitterId, TSharedPtr<FJsonObject> Values);

	/** Get cached value for an emitter */
	TSharedPtr<FJsonObject> GetCachedEmitterValue(const FString& TargetId, const FString& EmitterId) const;

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when a preset is recalled */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Presets|Events")
	FOnRshipPresetRecalled OnPresetRecalled;

	/** Fired during interpolation with progress */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Presets|Events")
	FOnRshipPresetInterpolating OnPresetInterpolating;

private:
	/** Generate a unique preset ID */
	FString GeneratePresetId() const;

	/** Apply a single snapshot to its target */
	void ApplySnapshot(const FRshipEmitterSnapshot& Snapshot);

	/** Apply interpolated values between two snapshots */
	void ApplyInterpolatedSnapshot(const FRshipEmitterSnapshot& From, const FRshipEmitterSnapshot& To, float Alpha);

	/** Interpolate between two JSON objects */
	TSharedPtr<FJsonObject> LerpJsonObjects(TSharedPtr<FJsonObject> A, TSharedPtr<FJsonObject> B, float Alpha);

	/** Update interpolation (called on timer) */
	void TickInterpolation();

	/** Reference to subsystem */
	UPROPERTY()
	URshipSubsystem* Subsystem;

	/** All saved presets */
	UPROPERTY()
	TMap<FString, FRshipPreset> Presets;

	/** Cached emitter values (TargetId:EmitterId -> JSON) */
	TMap<FString, TSharedPtr<FJsonObject>> EmitterValueCache;

	/** Counter for generating unique IDs */
	int32 PresetIdCounter = 0;

	// Interpolation state
	bool bIsInterpolating = false;
	float InterpolationProgress = 0.0f;
	float InterpolationDuration = 0.0f;
	float InterpolationElapsed = 0.0f;
	FRshipPreset InterpolationFromPreset;
	FRshipPreset InterpolationToPreset;
	FTimerHandle InterpolationTimerHandle;
};
