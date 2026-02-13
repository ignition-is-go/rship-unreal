// Rship Camera Actor
// Visualizes a camera from rship with calibration data and FOV visualization

#pragma once

#include "CoreMinimal.h"
#include "CineCameraActor.h"
#include "RshipCalibrationTypes.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RshipCameraActor.generated.h"

class URshipSubsystem;
class URshipCameraManager;

/**
 * Actor that visualizes a camera from rship with calibration-accurate positioning and FOV.
 * Can optionally render a scene capture for preview purposes.
 */
UCLASS(BlueprintType, Blueprintable)
class RSHIPEXEC_API ARshipCameraActor : public ACineCameraActor
{
    GENERATED_BODY()

public:
    ARshipCameraActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** The rship camera ID to visualize */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
    FString CameraId;

    /** Auto-sync position/rotation from rship calibration data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
    bool bSyncTransformFromCalibration = true;

    /** Show FOV frustum visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
    bool bShowFrustumVisualization = true;

    /** Enable scene capture for preview */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
    bool bEnableSceneCapture = false;

    /** Scale factor for position (rship units to UE units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
    float PositionScale = 100.0f;  // cm per meter

    /** Frustum visualization distance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
    float FrustumVisualizationDistance = 500.0f;  // cm

    /** Frustum line color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
    FLinearColor FrustumColor = FLinearColor(0.0f, 1.0f, 0.5f, 1.0f);

    // ========================================================================
    // COMPONENTS
    // ========================================================================

    /** Root scene component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootSceneComponent;

    /** Camera body mesh for visualization */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* CameraMesh;

    /** Scene capture component for preview rendering */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneCaptureComponent2D* SceneCapture;

    // ========================================================================
    // RUNTIME STATE
    // ========================================================================

    /** Current camera info from server */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera")
    FRshipCameraInfo CachedCameraInfo;

    /** Current color profile */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera")
    FRshipColorProfile CachedColorProfile;

    /** Render target for scene capture */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Camera")
    UTextureRenderTarget2D* CaptureRenderTarget;

    // ========================================================================
    // BLUEPRINT CALLABLE
    // ========================================================================

    /** Manually refresh camera data from server */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    void RefreshCameraData();

    /** Get the camera's FOV after calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    float GetCalibratedFOV() const;

    /** Get calibration quality string */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    FString GetCalibrationQuality() const;

    /** Check if camera has valid calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    bool HasCalibration() const;

    /** Apply color correction to an input color using the camera's color profile */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    FLinearColor ApplyColorCorrection(const FLinearColor& InputColor) const;

    /** Set the active color profile for this camera */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    void SetColorProfile(const FString& ProfileId);

    /** Get focal length from calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    FVector2D GetFocalLength() const;

    /** Get principal point from calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    FVector2D GetPrincipalPoint() const;

    /** Get distortion coefficients */
    UFUNCTION(BlueprintCallable, Category = "Rship|Camera")
    void GetDistortionCoefficients(FVector& OutRadial, FVector2D& OutTangential) const;

protected:
    /** Called when camera data is updated from server */
    UFUNCTION(BlueprintNativeEvent, Category = "Rship|Camera")
    void OnCameraDataUpdated();

    /** Called when color profile is updated */
    UFUNCTION(BlueprintNativeEvent, Category = "Rship|Camera")
    void OnColorProfileUpdated();

    /** Update visualization based on current state */
    virtual void UpdateVisualization();

    /** Apply transform from calibration data */
    virtual void ApplyCalibrationTransform();

    /** Draw frustum lines for debugging */
    virtual void DrawFrustumVisualization();

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    URshipCameraManager* CameraManager;

    // Delegate handles
    FDelegateHandle CameraUpdateHandle;
    FDelegateHandle ColorProfileUpdateHandle;

    void BindToManager();
    void UnbindFromManager();

    void OnCamerasUpdatedInternal();
    void OnColorProfileUpdatedInternal(const FRshipColorProfile& Profile);

    void SetupSceneCapture();
};
