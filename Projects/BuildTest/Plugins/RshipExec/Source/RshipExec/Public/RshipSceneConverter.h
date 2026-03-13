// Rship Scene Converter
// Converts existing UE scenes to rship-controlled setups

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RshipCalibrationTypes.h"
#include "Components/LightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/CameraActor.h"
#include "RshipSceneConverter.generated.h"

class URshipSubsystem;
class URshipFixtureManager;
class URshipCameraManager;
class ARshipFixtureActor;
class ARshipCameraActor;

// ============================================================================
// DISCOVERY RESULTS
// ============================================================================

/**
 * Information about a discovered light in the scene
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDiscoveredLight
{
    GENERATED_BODY()

    /** The light component found */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    ULightComponent* LightComponent = nullptr;

    /** The owning actor */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    AActor* OwnerActor = nullptr;

    /** Suggested name for the fixture */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FString SuggestedName;

    /** Light type (Spot, Point, Directional, Rect) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FString LightType;

    /** World position */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FVector Position = FVector::ZeroVector;

    /** World rotation */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FRotator Rotation = FRotator::ZeroRotator;

    /** Current intensity */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    float Intensity = 0.0f;

    /** Current color */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FLinearColor Color = FLinearColor::White;

    /** Cone angles for spot lights */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    float InnerConeAngle = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    float OuterConeAngle = 0.0f;

    /** Whether this light already has an rship fixture actor controlling it */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    bool bAlreadyConverted = false;

    /** If converted, the fixture ID */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FString ExistingFixtureId;
};

/**
 * Information about a discovered camera in the scene
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDiscoveredCamera
{
    GENERATED_BODY()

    /** The camera actor found */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    ACameraActor* CameraActor = nullptr;

    /** Suggested name for the camera */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FString SuggestedName;

    /** World position */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FVector Position = FVector::ZeroVector;

    /** World rotation */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FRotator Rotation = FRotator::ZeroRotator;

    /** Field of view */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    float FOV = 90.0f;

    /** Aspect ratio */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    float AspectRatio = 1.777f;

    /** Whether this camera already has an rship camera actor controlling it */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    bool bAlreadyConverted = false;

    /** If converted, the camera ID */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FString ExistingCameraId;
};

/**
 * Options for scene discovery
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDiscoveryOptions
{
    GENERATED_BODY()

    /** Include spot lights */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bIncludeSpotLights = true;

    /** Include point lights */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bIncludePointLights = true;

    /** Include directional lights */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bIncludeDirectionalLights = false;

    /** Include rect lights */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bIncludeRectLights = true;

    /** Include cameras */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bIncludeCameras = true;

    /** Skip lights that are already converted */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bSkipAlreadyConverted = true;

    /** Only include lights with specific tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    FName RequiredTag;

    /** Minimum intensity to include (filters out dim/off lights) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    float MinIntensity = 0.0f;
};

/**
 * Options for fixture conversion
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipConversionOptions
{
    GENERATED_BODY()

    /** Fixture type ID to assign (empty = auto-detect or create generic) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    FString FixtureTypeId;

    /** DMX universe to assign */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    int32 Universe = 1;

    /** Starting DMX address (auto-increments for multiple fixtures) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    int32 StartAddress = 1;

    /** Channels per fixture for address allocation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    int32 ChannelsPerFixture = 16;

    /** Scale factor for positions (UE units to rship meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    float PositionScale = 0.01f;  // 1 UE cm = 0.01 meters

    /** Spawn ARshipFixtureActor to visualize */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bSpawnVisualizationActor = false;

    /** Hide original light after conversion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bHideOriginalLight = false;

    /** Name prefix for created fixtures */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    FString NamePrefix = TEXT("UE_");

    /** Tags to apply to created fixtures */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    TArray<FString> Tags;

    /** Enable automatic transform sync (when actors are moved in editor, sync to server) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|SceneConverter")
    bool bEnableTransformSync = true;
};

/**
 * Result of a conversion operation
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipConversionResult
{
    GENERATED_BODY()

    /** Whether the conversion succeeded */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    bool bSuccess = false;

    /** The created fixture/camera ID */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FString EntityId;

    /** Error message if failed */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    FString ErrorMessage;

    /** The spawned visualization actor (if requested) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|SceneConverter")
    AActor* VisualizationActor = nullptr;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSceneDiscoveryComplete, const TArray<FRshipDiscoveredLight>&, Lights, const TArray<FRshipDiscoveredCamera>&, Cameras);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConversionComplete, int32, SuccessCount, int32, FailCount);

// ============================================================================
// SCENE CONVERTER SERVICE
// ============================================================================

/**
 * Service for converting existing UE scenes to rship-controlled setups.
 * Handles discovery of lights/cameras, registration with rship server,
 * and optional spawning of visualization actors.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipSceneConverter : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize the converter with the subsystem */
    void Initialize(URshipSubsystem* InSubsystem);

    /** Shutdown and cleanup */
    void Shutdown();

    // ========================================================================
    // DISCOVERY
    // ========================================================================

    /**
     * Discover all convertible lights and cameras in the current world
     * @param Options Discovery filter options
     * @return Number of items discovered
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    int32 DiscoverScene(const FRshipDiscoveryOptions& Options);

    /**
     * Get the last discovery results for lights
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    TArray<FRshipDiscoveredLight> GetDiscoveredLights() const { return DiscoveredLights; }

    /**
     * Get the last discovery results for cameras
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    TArray<FRshipDiscoveredCamera> GetDiscoveredCameras() const { return DiscoveredCameras; }

    /**
     * Clear discovery results
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    void ClearDiscoveryResults();

    // ========================================================================
    // CONVERSION
    // ========================================================================

    /**
     * Convert a single discovered light to an rship fixture
     * @param Light The discovered light to convert
     * @param Options Conversion options
     * @return Conversion result
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    FRshipConversionResult ConvertLight(const FRshipDiscoveredLight& Light, const FRshipConversionOptions& Options);

    /**
     * Convert a single discovered camera to an rship camera
     * @param Camera The discovered camera to convert
     * @param Options Conversion options (PositionScale used)
     * @return Conversion result
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    FRshipConversionResult ConvertCamera(const FRshipDiscoveredCamera& Camera, const FRshipConversionOptions& Options);

    /**
     * Convert all discovered lights to rship fixtures
     * @param Options Conversion options
     * @param OutResults Array to receive individual results
     * @return Number of successful conversions
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    int32 ConvertAllLights(const FRshipConversionOptions& Options, TArray<FRshipConversionResult>& OutResults);

    /**
     * Convert all discovered cameras to rship cameras
     * @param Options Conversion options
     * @param OutResults Array to receive individual results
     * @return Number of successful conversions
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    int32 ConvertAllCameras(const FRshipConversionOptions& Options, TArray<FRshipConversionResult>& OutResults);

    /**
     * Convert selected lights by index
     * @param Indices Indices into DiscoveredLights array
     * @param Options Conversion options
     * @param OutResults Array to receive individual results
     * @return Number of successful conversions
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    int32 ConvertLightsByIndex(const TArray<int32>& Indices, const FRshipConversionOptions& Options, TArray<FRshipConversionResult>& OutResults);

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /**
     * Validate discovered items before conversion
     * @param bStopOnError If true, returns false on first error
     * @return True if all items pass validation (no errors)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    bool ValidateBeforeConversion(bool bStopOnError = false);

    /**
     * Convert all lights with pre-validation
     * Skips items that have validation errors
     * @param Options Conversion options
     * @param OutResults Array to receive individual results
     * @return Number of successful conversions
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    int32 ConvertAllLightsValidated(const FRshipConversionOptions& Options, TArray<FRshipConversionResult>& OutResults);

    // ========================================================================
    // POSITION SYNC
    // ========================================================================

    /**
     * Push UE light positions to rship server for all converted fixtures
     * Useful after moving lights in the editor
     * @param PositionScale Scale factor (UE units to meters)
     * @return Number of fixtures updated
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    int32 SyncAllPositionsToServer(float PositionScale = 0.01f);

    /**
     * Push a single actor's position to its rship entity
     * @param Actor The actor to sync
     * @param EntityId The rship entity ID (fixture or camera)
     * @param PositionScale Scale factor
     * @return Whether the sync succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    bool SyncActorPositionToServer(AActor* Actor, const FString& EntityId, float PositionScale = 0.01f);

    // ========================================================================
    // UTILITY
    // ========================================================================

    /**
     * Get or create a generic fixture type for converted lights
     * @param LightType The UE light type (Spot, Point, etc.)
     * @return The fixture type ID
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    FString GetOrCreateGenericFixtureType(const FString& LightType);

    /**
     * Check if an actor has already been converted
     * @param Actor The actor to check
     * @return The entity ID if converted, empty string otherwise
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    FString GetConvertedEntityId(AActor* Actor) const;

    /**
     * Generate a unique fixture name from an actor
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConverter")
    static FString GenerateFixtureName(AActor* Actor, const FString& Prefix);

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when scene discovery completes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|SceneConverter")
    FOnSceneDiscoveryComplete OnDiscoveryComplete;

    /** Fired when batch conversion completes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|SceneConverter")
    FOnConversionComplete OnConversionComplete;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    URshipFixtureManager* FixtureManager;

    UPROPERTY()
    URshipCameraManager* CameraManager;

    // Discovery results
    TArray<FRshipDiscoveredLight> DiscoveredLights;
    TArray<FRshipDiscoveredCamera> DiscoveredCameras;

    // Tracking converted actors (Actor -> EntityId)
    TMap<TWeakObjectPtr<AActor>, FString> ConvertedActors;

    // Generic fixture type IDs created for UE light types
    TMap<FString, FString> GenericFixtureTypes;

    // Internal helpers
    void DiscoverLightsInWorld(UWorld* World, const FRshipDiscoveryOptions& Options);
    void DiscoverCamerasInWorld(UWorld* World, const FRshipDiscoveryOptions& Options);
    FString DetermineLightType(ULightComponent* Light) const;
    bool IsLightAlreadyConverted(ULightComponent* Light, FString& OutFixtureId) const;
    bool IsCameraAlreadyConverted(ACameraActor* Camera, FString& OutCameraId) const;

    FRshipFixtureInfo CreateFixtureInfoFromLight(const FRshipDiscoveredLight& Light, const FRshipConversionOptions& Options, int32 Index);
    FRshipCameraInfo CreateCameraInfoFromDiscovered(const FRshipDiscoveredCamera& Camera, const FRshipConversionOptions& Options);
};
