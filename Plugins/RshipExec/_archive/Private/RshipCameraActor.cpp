// Rship Camera Actor Implementation

#include "RshipCameraActor.h"
#include "RshipSubsystem.h"
#include "RshipCameraManager.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

ARshipCameraActor::ARshipCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Create root component
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(RootSceneComponent);

    // Create camera mesh (placeholder)
    CameraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraMesh"));
    CameraMesh->SetupAttachment(RootSceneComponent);
    CameraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create scene capture component
    SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
    SceneCapture->SetupAttachment(RootSceneComponent);
    SceneCapture->bCaptureEveryFrame = false; // Manual capture only
    SceneCapture->bCaptureOnMovement = false;
}

void ARshipCameraActor::BeginPlay()
{
    Super::BeginPlay();

    // Get subsystem
    if (GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    }

    if (Subsystem)
    {
        CameraManager = Subsystem->GetCameraManager();
        BindToManager();
        RefreshCameraData();

        if (bEnableSceneCapture)
        {
            SetupSceneCapture();
        }
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("ARshipCameraActor: Could not get URshipSubsystem"));
    }
}

void ARshipCameraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindFromManager();
    Super::EndPlay(EndPlayReason);
}

void ARshipCameraActor::BindToManager()
{
    if (!CameraManager)
    {
        return;
    }

    // Bind to camera updates
    CameraUpdateHandle = CameraManager->OnCamerasUpdated.AddLambda([this]()
    {
        OnCamerasUpdatedInternal();
    });

    // Bind to color profile updates
    ColorProfileUpdateHandle = CameraManager->OnColorProfileUpdated.AddLambda([this](const FRshipColorProfile& Profile)
    {
        OnColorProfileUpdatedInternal(Profile);
    });
}

void ARshipCameraActor::UnbindFromManager()
{
    if (CameraManager)
    {
        if (CameraUpdateHandle.IsValid())
        {
            CameraManager->OnCamerasUpdated.Remove(CameraUpdateHandle);
        }
        if (ColorProfileUpdateHandle.IsValid())
        {
            CameraManager->OnColorProfileUpdated.Remove(ColorProfileUpdateHandle);
        }
    }
    CameraUpdateHandle.Reset();
    ColorProfileUpdateHandle.Reset();
}

void ARshipCameraActor::RefreshCameraData()
{
    if (!CameraManager || CameraId.IsEmpty())
    {
        return;
    }

    // Get camera info
    if (CameraManager->GetCameraById(CameraId, CachedCameraInfo))
    {
        // Get associated color profile
        CameraManager->GetColorProfileForCamera(CameraId, CachedColorProfile);

        // Apply calibration transform if enabled
        if (bSyncTransformFromCalibration && CachedCameraInfo.HasCalibration())
        {
            ApplyCalibrationTransform();
        }

        // Update scene capture FOV
        if (SceneCapture && CachedCameraInfo.HasCalibration())
        {
            SceneCapture->FOVAngle = GetCalibratedFOV();
        }

        OnCameraDataUpdated();

        UE_LOG(LogRshipExec, Log, TEXT("ARshipCameraActor: Loaded camera %s (%s), FOV=%.1f"),
            *CachedCameraInfo.Name, *CameraId, GetCalibratedFOV());
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("ARshipCameraActor: Camera not found: %s"), *CameraId);
    }
}

void ARshipCameraActor::OnCamerasUpdatedInternal()
{
    if (!CameraId.IsEmpty() && CameraManager)
    {
        FRshipCameraInfo NewInfo;
        if (CameraManager->GetCameraById(CameraId, NewInfo))
        {
            CachedCameraInfo = NewInfo;

            if (bSyncTransformFromCalibration && CachedCameraInfo.HasCalibration())
            {
                ApplyCalibrationTransform();
            }

            if (SceneCapture && CachedCameraInfo.HasCalibration())
            {
                SceneCapture->FOVAngle = GetCalibratedFOV();
            }

            OnCameraDataUpdated();
        }
    }
}

void ARshipCameraActor::OnColorProfileUpdatedInternal(const FRshipColorProfile& Profile)
{
    // Check if this profile is associated with our camera
    if (Profile.CameraId == CameraId || Profile.Id == CachedColorProfile.Id)
    {
        CachedColorProfile = Profile;
        OnColorProfileUpdated();

        UE_LOG(LogRshipExec, Log, TEXT("ARshipCameraActor: Color profile updated for %s (quality=%s)"),
            *CameraId, *GetCalibrationQuality());
    }
}

void ARshipCameraActor::OnCameraDataUpdated_Implementation()
{
    // Blueprint can override
    UpdateVisualization();
}

void ARshipCameraActor::OnColorProfileUpdated_Implementation()
{
    // Blueprint can override
}

void ARshipCameraActor::ApplyCalibrationTransform()
{
    const FRshipCameraCalibration& Cal = CachedCameraInfo.Calibration;

    // Convert rship coordinates to UE coordinates
    FVector NewLocation(
        Cal.Position.X * PositionScale,
        Cal.Position.Y * PositionScale,
        Cal.Position.Z * PositionScale
    );

    FRotator NewRotation = Cal.Rotation;

    SetActorLocationAndRotation(NewLocation, NewRotation);
}

void ARshipCameraActor::UpdateVisualization()
{
    if (bShowFrustumVisualization)
    {
        DrawFrustumVisualization();
    }
}

void ARshipCameraActor::DrawFrustumVisualization()
{
    if (!GetWorld())
    {
        return;
    }

    float FOV = GetCalibratedFOV();
    float HalfFOVRad = FMath::DegreesToRadians(FOV * 0.5f);

    // Calculate frustum corners at visualization distance
    float HalfWidth = FMath::Tan(HalfFOVRad) * FrustumVisualizationDistance;

    // Aspect ratio from resolution
    float AspectRatio = static_cast<float>(CachedCameraInfo.Resolution.X) /
                        static_cast<float>(CachedCameraInfo.Resolution.Y);
    float HalfHeight = HalfWidth / AspectRatio;

    FVector CameraPos = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    FVector Right = GetActorRightVector();
    FVector Up = GetActorUpVector();

    // Calculate corner points
    FVector FarCenter = CameraPos + Forward * FrustumVisualizationDistance;
    FVector TopLeft = FarCenter - Right * HalfWidth + Up * HalfHeight;
    FVector TopRight = FarCenter + Right * HalfWidth + Up * HalfHeight;
    FVector BottomLeft = FarCenter - Right * HalfWidth - Up * HalfHeight;
    FVector BottomRight = FarCenter + Right * HalfWidth - Up * HalfHeight;

    FColor LineColor = FrustumColor.ToFColor(true);
    float LineThickness = 2.0f;

    // Draw frustum lines from camera to corners
    DrawDebugLine(GetWorld(), CameraPos, TopLeft, LineColor, false, -1.0f, 0, LineThickness);
    DrawDebugLine(GetWorld(), CameraPos, TopRight, LineColor, false, -1.0f, 0, LineThickness);
    DrawDebugLine(GetWorld(), CameraPos, BottomLeft, LineColor, false, -1.0f, 0, LineThickness);
    DrawDebugLine(GetWorld(), CameraPos, BottomRight, LineColor, false, -1.0f, 0, LineThickness);

    // Draw far plane rectangle
    DrawDebugLine(GetWorld(), TopLeft, TopRight, LineColor, false, -1.0f, 0, LineThickness);
    DrawDebugLine(GetWorld(), TopRight, BottomRight, LineColor, false, -1.0f, 0, LineThickness);
    DrawDebugLine(GetWorld(), BottomRight, BottomLeft, LineColor, false, -1.0f, 0, LineThickness);
    DrawDebugLine(GetWorld(), BottomLeft, TopLeft, LineColor, false, -1.0f, 0, LineThickness);

    // Draw cross at center
    DrawDebugLine(GetWorld(), FarCenter - Right * 20.0f, FarCenter + Right * 20.0f, LineColor, false, -1.0f, 0, LineThickness);
    DrawDebugLine(GetWorld(), FarCenter - Up * 20.0f, FarCenter + Up * 20.0f, LineColor, false, -1.0f, 0, LineThickness);
}

void ARshipCameraActor::SetupSceneCapture()
{
    if (!SceneCapture)
    {
        return;
    }

    // Create render target if needed
    if (!CaptureRenderTarget)
    {
        CaptureRenderTarget = NewObject<UTextureRenderTarget2D>(this);
        CaptureRenderTarget->InitAutoFormat(
            CachedCameraInfo.Resolution.X > 0 ? CachedCameraInfo.Resolution.X : 1920,
            CachedCameraInfo.Resolution.Y > 0 ? CachedCameraInfo.Resolution.Y : 1080
        );
        CaptureRenderTarget->UpdateResourceImmediate();
    }

    SceneCapture->TextureTarget = CaptureRenderTarget;
    SceneCapture->FOVAngle = GetCalibratedFOV();

    UE_LOG(LogRshipExec, Log, TEXT("ARshipCameraActor: Scene capture initialized at %dx%d"),
        CachedCameraInfo.Resolution.X, CachedCameraInfo.Resolution.Y);
}

float ARshipCameraActor::GetCalibratedFOV() const
{
    if (CachedCameraInfo.HasCalibration() && CachedCameraInfo.Calibration.FOV > 0.0f)
    {
        return CachedCameraInfo.Calibration.FOV;
    }
    return 60.0f; // Default FOV
}

FString ARshipCameraActor::GetCalibrationQuality() const
{
    if (CachedColorProfile.HasColorChecker())
    {
        return CachedColorProfile.GetCalibrationQuality();
    }
    return TEXT("uncalibrated");
}

bool ARshipCameraActor::HasCalibration() const
{
    return CachedCameraInfo.HasCalibration();
}

FLinearColor ARshipCameraActor::ApplyColorCorrection(const FLinearColor& InputColor) const
{
    return CachedColorProfile.ApplyColorCorrection(InputColor);
}

void ARshipCameraActor::SetColorProfile(const FString& ProfileId)
{
    if (CameraManager)
    {
        CameraManager->GetColorProfileById(ProfileId, CachedColorProfile);
        OnColorProfileUpdated();
    }
}

FVector2D ARshipCameraActor::GetFocalLength() const
{
    if (CachedCameraInfo.HasCalibration())
    {
        return CachedCameraInfo.Calibration.FocalLength;
    }
    return FVector2D(1000.0f, 1000.0f);
}

FVector2D ARshipCameraActor::GetPrincipalPoint() const
{
    if (CachedCameraInfo.HasCalibration())
    {
        return CachedCameraInfo.Calibration.PrincipalPoint;
    }
    return FVector2D(
        CachedCameraInfo.Resolution.X * 0.5f,
        CachedCameraInfo.Resolution.Y * 0.5f
    );
}

void ARshipCameraActor::GetDistortionCoefficients(FVector& OutRadial, FVector2D& OutTangential) const
{
    if (CachedCameraInfo.HasCalibration())
    {
        OutRadial = CachedCameraInfo.Calibration.RadialDistortion;
        OutTangential = CachedCameraInfo.Calibration.TangentialDistortion;
    }
    else
    {
        OutRadial = FVector::ZeroVector;
        OutTangential = FVector2D::ZeroVector;
    }
}
