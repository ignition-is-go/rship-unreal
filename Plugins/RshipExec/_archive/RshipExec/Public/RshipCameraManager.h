// Rship Camera Manager
// Manages camera entities and color profiles from rship server

#pragma once

#include "CoreMinimal.h"
#include "RshipCalibrationTypes.h"
#include "RshipCameraManager.generated.h"

class URshipSubsystem;

// ============================================================================
// DELEGATES (non-dynamic to support AddLambda in C++)
// ============================================================================

DECLARE_MULTICAST_DELEGATE(FOnCamerasUpdated);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCameraAdded, const FRshipCameraInfo&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCameraRemoved, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnColorProfileAdded, const FRshipColorProfile&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnColorProfileUpdated, const FRshipColorProfile&);

/**
 * Manages camera entities and color profiles for camera calibration.
 * Subscribes to server-side entities and provides O(1) lookups.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipCameraManager : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initialize the manager with reference to the subsystem
     */
    void Initialize(URshipSubsystem* InSubsystem);

    /**
     * Cleanup on shutdown
     */
    void Shutdown();

    // ========================================================================
    // CAMERA QUERIES
    // ========================================================================

    /** Get all cameras in the current project */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    TArray<FRshipCameraInfo> GetAllCameras() const;

    /** Get camera by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    bool GetCameraById(const FString& CameraId, FRshipCameraInfo& OutCamera) const;

    /** Get camera count */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    int32 GetCameraCount() const { return Cameras.Num(); }

    // ========================================================================
    // COLOR PROFILE QUERIES
    // ========================================================================

    /** Get all color profiles */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    TArray<FRshipColorProfile> GetAllColorProfiles() const;

    /** Get color profile by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    bool GetColorProfileById(const FString& ProfileId, FRshipColorProfile& OutProfile) const;

    /** Get color profile for a camera */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    bool GetColorProfileForCamera(const FString& CameraId, FRshipColorProfile& OutProfile) const;

    /** Get profiles associated with a specific camera */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    TArray<FRshipColorProfile> GetColorProfilesByCameraId(const FString& CameraId) const;

    // ========================================================================
    // COLOR CORRECTION HELPERS
    // ========================================================================

    /**
     * Apply color correction to an RGB value using a camera's profile
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    FLinearColor ApplyColorCorrectionForCamera(const FString& CameraId, const FLinearColor& InputColor) const;

    /**
     * Get calibration quality rating for a camera's profile
     * @return "excellent", "good", "acceptable", "poor", or "uncalibrated"
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    FString GetCalibrationQualityForCamera(const FString& CameraId) const;

    // ========================================================================
    // ACTIVE PROFILE MANAGEMENT
    // ========================================================================

    /**
     * Set the currently active color profile (local preference)
     * This can be used for preview/comparison in the editor
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    void SetActiveColorProfile(const FString& ProfileId);

    /**
     * Get the currently active color profile ID
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    FString GetActiveColorProfileId() const { return ActiveColorProfileId; }

    /**
     * Get the currently active color profile
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    bool GetActiveColorProfile(FRshipColorProfile& OutProfile) const;

    // ========================================================================
    // LOCAL REGISTRATION (for scene conversion)
    // ========================================================================

    /**
     * Register a locally-created camera with the server
     * Used by scene converter to push UE cameras as rship cameras
     * @param CameraInfo The camera info to register
     * @return Whether the registration was sent successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    bool RegisterLocalCamera(const FRshipCameraInfo& CameraInfo);

    /**
     * Update a camera's position on the server
     * Used to sync UE editor changes back to rship
     * @param CameraId The camera to update
     * @param Position New position (in rship units - meters)
     * @param Rotation New rotation
     * @return Whether the update was sent successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    bool UpdateCameraPosition(const FString& CameraId, const FVector& Position, const FRotator& Rotation);

    /**
     * Remove a locally-registered camera from the server
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    bool UnregisterCamera(const FString& CameraId);

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when camera list changes (C++ only, use AddLambda) */
    FOnCamerasUpdated OnCamerasUpdated;

    /** Fired when a camera is added (C++ only, use AddLambda) */
    FOnCameraAdded OnCameraAdded;

    /** Fired when a camera is removed (C++ only, use AddLambda) */
    FOnCameraRemoved OnCameraRemoved;

    /** Fired when a color profile is added (C++ only, use AddLambda) */
    FOnColorProfileAdded OnColorProfileAdded;

    /** Fired when a color profile is updated (C++ only, use AddLambda) */
    FOnColorProfileUpdated OnColorProfileUpdated;

    // ========================================================================
    // ENTITY PROCESSING (called by subsystem)
    // ========================================================================

    /**
     * Process incoming Camera entity event
     */
    void ProcessCameraEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);

    /**
     * Process incoming Calibration entity event (OpenCV camera calibration)
     */
    void ProcessCalibrationEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);

    /**
     * Process incoming ColorProfile entity event
     */
    void ProcessColorProfileEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Cameras by ID
    TMap<FString, FRshipCameraInfo> Cameras;

    // Color profiles by ID
    TMap<FString, FRshipColorProfile> ColorProfiles;

    // Color profiles indexed by camera ID
    TMultiMap<FString, FString> ColorProfilesByCameraId;

    // Currently active color profile (local preference)
    FString ActiveColorProfileId;

    // Parse camera info from JSON
    FRshipCameraInfo ParseCamera(const TSharedPtr<FJsonObject>& Data) const;

    // Parse camera calibration result from JSON
    FRshipCameraCalibration ParseCameraCalibration(const TSharedPtr<FJsonObject>& Data) const;

    // Parse color profile from JSON
    FRshipColorProfile ParseColorProfile(const TSharedPtr<FJsonObject>& Data) const;
};
