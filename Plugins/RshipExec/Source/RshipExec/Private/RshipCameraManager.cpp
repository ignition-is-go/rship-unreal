// Rship Camera Manager Implementation

#include "RshipCameraManager.h"
#include "RshipSubsystem.h"
#include "Myko.h"
#include "Logs.h"

void URshipCameraManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogRshipExec, Log, TEXT("CameraManager initialized"));
}

void URshipCameraManager::Shutdown()
{
    Cameras.Empty();
    ColorProfiles.Empty();
    ColorProfilesByCameraId.Empty();
    ActiveColorProfileId.Empty();
    Subsystem = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("CameraManager shutdown"));
}

// ============================================================================
// CAMERA QUERIES
// ============================================================================

TArray<FRshipCameraInfo> URshipCameraManager::GetAllCameras() const
{
    TArray<FRshipCameraInfo> Result;
    Cameras.GenerateValueArray(Result);
    return Result;
}

bool URshipCameraManager::GetCameraById(const FString& CameraId, FRshipCameraInfo& OutCamera) const
{
    const FRshipCameraInfo* Found = Cameras.Find(CameraId);
    if (Found)
    {
        OutCamera = *Found;
        return true;
    }
    return false;
}

// ============================================================================
// COLOR PROFILE QUERIES
// ============================================================================

TArray<FRshipColorProfile> URshipCameraManager::GetAllColorProfiles() const
{
    TArray<FRshipColorProfile> Result;
    ColorProfiles.GenerateValueArray(Result);
    return Result;
}

bool URshipCameraManager::GetColorProfileById(const FString& ProfileId, FRshipColorProfile& OutProfile) const
{
    const FRshipColorProfile* Found = ColorProfiles.Find(ProfileId);
    if (Found)
    {
        OutProfile = *Found;
        return true;
    }
    return false;
}

bool URshipCameraManager::GetColorProfileForCamera(const FString& CameraId, FRshipColorProfile& OutProfile) const
{
    // First check if camera has a direct profile association
    FRshipCameraInfo Camera;
    if (GetCameraById(CameraId, Camera) && !Camera.ColorProfileId.IsEmpty())
    {
        return GetColorProfileById(Camera.ColorProfileId, OutProfile);
    }

    // Fall back to finding a profile associated with this camera
    TArray<FString> ProfileIds;
    ColorProfilesByCameraId.MultiFind(CameraId, ProfileIds);

    if (ProfileIds.Num() > 0)
    {
        return GetColorProfileById(ProfileIds[0], OutProfile);
    }

    return false;
}

TArray<FRshipColorProfile> URshipCameraManager::GetColorProfilesByCameraId(const FString& CameraId) const
{
    TArray<FRshipColorProfile> Result;
    TArray<FString> ProfileIds;
    ColorProfilesByCameraId.MultiFind(CameraId, ProfileIds);

    for (const FString& ProfileId : ProfileIds)
    {
        FRshipColorProfile Profile;
        if (GetColorProfileById(ProfileId, Profile))
        {
            Result.Add(Profile);
        }
    }
    return Result;
}

// ============================================================================
// COLOR CORRECTION HELPERS
// ============================================================================

FLinearColor URshipCameraManager::ApplyColorCorrectionForCamera(const FString& CameraId, const FLinearColor& InputColor) const
{
    FRshipColorProfile Profile;
    if (GetColorProfileForCamera(CameraId, Profile))
    {
        return Profile.ApplyColorCorrection(InputColor);
    }
    return InputColor;
}

FString URshipCameraManager::GetCalibrationQualityForCamera(const FString& CameraId) const
{
    FRshipColorProfile Profile;
    if (GetColorProfileForCamera(CameraId, Profile))
    {
        return Profile.GetCalibrationQuality();
    }
    return TEXT("uncalibrated");
}

// ============================================================================
// ACTIVE PROFILE MANAGEMENT
// ============================================================================

void URshipCameraManager::SetActiveColorProfile(const FString& ProfileId)
{
    if (ProfileId != ActiveColorProfileId)
    {
        ActiveColorProfileId = ProfileId;
        UE_LOG(LogRshipExec, Log, TEXT("Active color profile set to: %s"),
            ProfileId.IsEmpty() ? TEXT("(none)") : *ProfileId);
    }
}

bool URshipCameraManager::GetActiveColorProfile(FRshipColorProfile& OutProfile) const
{
    if (ActiveColorProfileId.IsEmpty())
    {
        return false;
    }
    return GetColorProfileById(ActiveColorProfileId, OutProfile);
}

// ============================================================================
// LOCAL REGISTRATION
// ============================================================================

bool URshipCameraManager::RegisterLocalCamera(const FRshipCameraInfo& CameraInfo)
{
    if (!Subsystem || CameraInfo.Id.IsEmpty())
    {
        return false;
    }

    // Build JSON payload for the camera
    TSharedPtr<FJsonObject> CameraJson = MakeShareable(new FJsonObject);

    CameraJson->SetStringField(TEXT("id"), CameraInfo.Id);
    CameraJson->SetStringField(TEXT("name"), CameraInfo.Name);

    // Position
    CameraJson->SetNumberField(TEXT("x"), CameraInfo.Position.X);
    CameraJson->SetNumberField(TEXT("y"), CameraInfo.Position.Y);
    CameraJson->SetNumberField(TEXT("z"), CameraInfo.Position.Z);

    // Rotation
    CameraJson->SetNumberField(TEXT("rotX"), CameraInfo.Rotation.Pitch);
    CameraJson->SetNumberField(TEXT("rotY"), CameraInfo.Rotation.Yaw);
    CameraJson->SetNumberField(TEXT("rotZ"), CameraInfo.Rotation.Roll);

    // Resolution
    CameraJson->SetNumberField(TEXT("width"), CameraInfo.Resolution.X);
    CameraJson->SetNumberField(TEXT("height"), CameraInfo.Resolution.Y);

    // Optional fields
    if (!CameraInfo.ColorProfileId.IsEmpty())
    {
        CameraJson->SetStringField(TEXT("colorProfileId"), CameraInfo.ColorProfileId);
    }

    // Send as SET command via subsystem
    Subsystem->SetItem(TEXT("Camera"), CameraJson, ERshipMessagePriority::High, CameraInfo.Id);

    // Also cache locally
    Cameras.Add(CameraInfo.Id, CameraInfo);

    UE_LOG(LogRshipExec, Log, TEXT("CameraManager: Registered local camera '%s' (%s)"),
        *CameraInfo.Name, *CameraInfo.Id);

    OnCameraAdded.Broadcast(CameraInfo);
    OnCamerasUpdated.Broadcast();

    return true;
}

bool URshipCameraManager::UpdateCameraPosition(const FString& CameraId, const FVector& Position, const FRotator& Rotation)
{
    if (!Subsystem || CameraId.IsEmpty())
    {
        return false;
    }

    // Get existing camera
    FRshipCameraInfo* Existing = Cameras.Find(CameraId);
    if (!Existing)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("CameraManager: Cannot update position for unknown camera: %s"), *CameraId);
        return false;
    }

    // Update local cache
    Existing->Position = Position;
    Existing->Rotation = Rotation;

    // Send update to server
    TSharedPtr<FJsonObject> UpdateJson = MakeShareable(new FJsonObject);
    UpdateJson->SetStringField(TEXT("id"), CameraId);
    UpdateJson->SetStringField(TEXT("name"), Existing->Name);

    UpdateJson->SetNumberField(TEXT("x"), Position.X);
    UpdateJson->SetNumberField(TEXT("y"), Position.Y);
    UpdateJson->SetNumberField(TEXT("z"), Position.Z);
    UpdateJson->SetNumberField(TEXT("rotX"), Rotation.Pitch);
    UpdateJson->SetNumberField(TEXT("rotY"), Rotation.Yaw);
    UpdateJson->SetNumberField(TEXT("rotZ"), Rotation.Roll);

    UpdateJson->SetNumberField(TEXT("width"), Existing->Resolution.X);
    UpdateJson->SetNumberField(TEXT("height"), Existing->Resolution.Y);

    Subsystem->SetItem(TEXT("Camera"), UpdateJson, ERshipMessagePriority::Normal, CameraId);

    UE_LOG(LogRshipExec, Verbose, TEXT("CameraManager: Updated position for camera %s"), *CameraId);

    OnCamerasUpdated.Broadcast();

    return true;
}

bool URshipCameraManager::UnregisterCamera(const FString& CameraId)
{
    if (!Subsystem || CameraId.IsEmpty())
    {
        return false;
    }

    // Send DELETE command using ws:m:event + changeType=DEL
    TSharedPtr<FJsonObject> DeleteJson = MakeShareable(new FJsonObject);
    DeleteJson->SetStringField(TEXT("id"), CameraId);
    Subsystem->SendJson(MakeDel(TEXT("Camera"), DeleteJson));

    // Remove from local cache
    if (Cameras.Remove(CameraId) > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("CameraManager: Unregistered camera %s"), *CameraId);
        OnCameraRemoved.Broadcast(CameraId);
        OnCamerasUpdated.Broadcast();
        return true;
    }

    return false;
}

// ============================================================================
// ENTITY PROCESSING
// ============================================================================

void URshipCameraManager::ProcessCameraEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
{
    if (!Data.IsValid())
    {
        return;
    }

    FString Id = Data->GetStringField(TEXT("id"));
    if (Id.IsEmpty())
    {
        return;
    }

    if (bIsDelete)
    {
        if (Cameras.Remove(Id) > 0)
        {
            UE_LOG(LogRshipExec, Log, TEXT("Camera removed: %s"), *Id);
            OnCameraRemoved.Broadcast(Id);
            OnCamerasUpdated.Broadcast();
        }
    }
    else
    {
        FRshipCameraInfo Camera = ParseCamera(Data);
        bool bIsNew = !Cameras.Contains(Id);
        Cameras.Add(Id, Camera);

        UE_LOG(LogRshipExec, Log, TEXT("Camera %s: %s (%s)"),
            bIsNew ? TEXT("added") : TEXT("updated"),
            *Camera.Name, *Id);

        if (bIsNew)
        {
            OnCameraAdded.Broadcast(Camera);
        }
        OnCamerasUpdated.Broadcast();
    }
}

void URshipCameraManager::ProcessCalibrationEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
{
    if (!Data.IsValid())
    {
        return;
    }

    // This is for OpenCV camera calibration results (Calibration entity)
    // It updates the calibration data on the associated camera

    FString Id = Data->GetStringField(TEXT("id"));
    FString CameraId = Data->GetStringField(TEXT("cameraId"));

    if (CameraId.IsEmpty())
    {
        return;
    }

    if (bIsDelete)
    {
        // Clear calibration from camera
        FRshipCameraInfo* Camera = Cameras.Find(CameraId);
        if (Camera)
        {
            Camera->Calibration = FRshipCameraCalibration();
            UE_LOG(LogRshipExec, Log, TEXT("Camera calibration cleared for: %s"), *CameraId);
            OnCamerasUpdated.Broadcast();
        }
    }
    else
    {
        // Get or create camera entry
        FRshipCameraInfo* Camera = Cameras.Find(CameraId);
        if (Camera)
        {
            // Parse savedResult if present (this is where OpenCV results are stored)
            const TSharedPtr<FJsonObject>* SavedResult;
            if (Data->TryGetObjectField(TEXT("savedResult"), SavedResult))
            {
                Camera->Calibration = ParseCameraCalibration(*SavedResult);
                UE_LOG(LogRshipExec, Log, TEXT("Camera calibration updated for: %s (FOV=%.1f°, error=%.2f)"),
                    *CameraId, Camera->Calibration.FOV, Camera->Calibration.ReprojectionError);
                OnCamerasUpdated.Broadcast();
            }
        }
    }
}

void URshipCameraManager::ProcessColorProfileEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
{
    if (!Data.IsValid())
    {
        return;
    }

    FString Id = Data->GetStringField(TEXT("id"));
    if (Id.IsEmpty())
    {
        return;
    }

    if (bIsDelete)
    {
        // Remove from camera index
        const FRshipColorProfile* Existing = ColorProfiles.Find(Id);
        if (Existing && !Existing->CameraId.IsEmpty())
        {
            ColorProfilesByCameraId.Remove(Existing->CameraId, Id);
        }

        if (ColorProfiles.Remove(Id) > 0)
        {
            UE_LOG(LogRshipExec, Log, TEXT("ColorProfile removed: %s"), *Id);

            // Clear active if this was the active profile
            if (ActiveColorProfileId == Id)
            {
                ActiveColorProfileId.Empty();
            }
        }
    }
    else
    {
        FRshipColorProfile Profile = ParseColorProfile(Data);
        bool bIsNew = !ColorProfiles.Contains(Id);

        // Update camera index
        const FRshipColorProfile* Existing = ColorProfiles.Find(Id);
        if (Existing && !Existing->CameraId.IsEmpty() && Existing->CameraId != Profile.CameraId)
        {
            // Camera association changed, remove old index
            ColorProfilesByCameraId.Remove(Existing->CameraId, Id);
        }

        ColorProfiles.Add(Id, Profile);

        if (!Profile.CameraId.IsEmpty())
        {
            ColorProfilesByCameraId.AddUnique(Profile.CameraId, Id);
        }

        UE_LOG(LogRshipExec, Log, TEXT("ColorProfile %s: %s (quality=%s)"),
            bIsNew ? TEXT("added") : TEXT("updated"),
            *Profile.Name, *Profile.GetCalibrationQuality());

        if (bIsNew)
        {
            OnColorProfileAdded.Broadcast(Profile);
        }
        else
        {
            OnColorProfileUpdated.Broadcast(Profile);
        }
    }
}

// ============================================================================
// JSON PARSING
// ============================================================================

FRshipCameraInfo URshipCameraManager::ParseCamera(const TSharedPtr<FJsonObject>& Data) const
{
    FRshipCameraInfo Info;

    Info.Id = Data->GetStringField(TEXT("id"));
    Info.Name = Data->GetStringField(TEXT("name"));

    // Position
    Info.Position.X = Data->GetNumberField(TEXT("x"));
    Info.Position.Y = Data->GetNumberField(TEXT("y"));
    Info.Position.Z = Data->GetNumberField(TEXT("z"));

    // Rotation
    Info.Rotation.Pitch = Data->GetNumberField(TEXT("rotX"));
    Info.Rotation.Yaw = Data->GetNumberField(TEXT("rotY"));
    Info.Rotation.Roll = Data->GetNumberField(TEXT("rotZ"));

    // Resolution
    Info.Resolution.X = Data->GetIntegerField(TEXT("resolutionX"));
    Info.Resolution.Y = Data->GetIntegerField(TEXT("resolutionY"));
    if (Info.Resolution.X == 0) Info.Resolution.X = 1920;
    if (Info.Resolution.Y == 0) Info.Resolution.Y = 1080;

    // Optional color profile association
    Data->TryGetStringField(TEXT("colorProfileId"), Info.ColorProfileId);

    return Info;
}

FRshipCameraCalibration URshipCameraManager::ParseCameraCalibration(const TSharedPtr<FJsonObject>& Data) const
{
    FRshipCameraCalibration Cal;

    // Position and rotation from calibration result
    Cal.Position.X = Data->GetNumberField(TEXT("posX"));
    Cal.Position.Y = Data->GetNumberField(TEXT("posY"));
    Cal.Position.Z = Data->GetNumberField(TEXT("posZ"));

    Cal.Rotation.Pitch = Data->GetNumberField(TEXT("rotX"));
    Cal.Rotation.Yaw = Data->GetNumberField(TEXT("rotY"));
    Cal.Rotation.Roll = Data->GetNumberField(TEXT("rotZ"));

    // Intrinsics
    Cal.FocalLength.X = Data->GetNumberField(TEXT("fx"));
    Cal.FocalLength.Y = Data->GetNumberField(TEXT("fy"));
    Cal.PrincipalPoint.X = Data->GetNumberField(TEXT("cx"));
    Cal.PrincipalPoint.Y = Data->GetNumberField(TEXT("cy"));

    // FOV (may be computed from focal length)
    Cal.FOV = Data->GetNumberField(TEXT("fov"));
    if (Cal.FOV <= 0.0f && Cal.FocalLength.X > 0.0f)
    {
        // Approximate horizontal FOV from focal length assuming common sensor
        float SensorWidth = 36.0f; // mm (full frame equivalent)
        Cal.FOV = 2.0f * FMath::RadiansToDegrees(FMath::Atan(SensorWidth / (2.0f * Cal.FocalLength.X)));
    }

    // Distortion coefficients
    Cal.RadialDistortion.X = Data->GetNumberField(TEXT("k1"));
    Cal.RadialDistortion.Y = Data->GetNumberField(TEXT("k2"));
    Cal.RadialDistortion.Z = Data->GetNumberField(TEXT("k3"));
    Cal.TangentialDistortion.X = Data->GetNumberField(TEXT("p1"));
    Cal.TangentialDistortion.Y = Data->GetNumberField(TEXT("p2"));

    // Quality metric
    Cal.ReprojectionError = Data->GetNumberField(TEXT("reprojectionError"));

    return Cal;
}

FRshipColorProfile URshipCameraManager::ParseColorProfile(const TSharedPtr<FJsonObject>& Data) const
{
    FRshipColorProfile Profile;

    Profile.Id = Data->GetStringField(TEXT("id"));
    Profile.Name = Data->GetStringField(TEXT("name"));
    Profile.ProjectId = Data->GetStringField(TEXT("projectId"));

    Data->TryGetStringField(TEXT("manufacturer"), Profile.Manufacturer);
    Data->TryGetStringField(TEXT("model"), Profile.Model);
    Data->TryGetStringField(TEXT("cameraId"), Profile.CameraId);
    Data->TryGetStringField(TEXT("hash"), Profile.Hash);

    // White balance
    const TSharedPtr<FJsonObject>* WBObj;
    if (Data->TryGetObjectField(TEXT("whiteBalance"), WBObj))
    {
        Profile.WhiteBalance.Kelvin = (*WBObj)->GetNumberField(TEXT("kelvin"));
        Profile.WhiteBalance.Tint = (*WBObj)->GetNumberField(TEXT("tint"));
        Profile.WhiteBalance.CalibratedAt = (*WBObj)->GetStringField(TEXT("calibratedAt"));

        // Measured gray
        const TSharedPtr<FJsonObject>* MeasuredGrayObj;
        if ((*WBObj)->TryGetObjectField(TEXT("measuredGray"), MeasuredGrayObj))
        {
            Profile.WhiteBalance.MeasuredGray.R = (*MeasuredGrayObj)->GetNumberField(TEXT("r"));
            Profile.WhiteBalance.MeasuredGray.G = (*MeasuredGrayObj)->GetNumberField(TEXT("g"));
            Profile.WhiteBalance.MeasuredGray.B = (*MeasuredGrayObj)->GetNumberField(TEXT("b"));
        }

        // Multipliers
        const TSharedPtr<FJsonObject>* MultipliersObj;
        if ((*WBObj)->TryGetObjectField(TEXT("multipliers"), MultipliersObj))
        {
            Profile.WhiteBalance.Multipliers.R = (*MultipliersObj)->GetNumberField(TEXT("r"));
            Profile.WhiteBalance.Multipliers.G = (*MultipliersObj)->GetNumberField(TEXT("g"));
            Profile.WhiteBalance.Multipliers.B = (*MultipliersObj)->GetNumberField(TEXT("b"));
        }
    }

    // Color checker
    const TSharedPtr<FJsonObject>* CCObj;
    if (Data->TryGetObjectField(TEXT("colorChecker"), CCObj))
    {
        Profile.ColorChecker.DeltaE = (*CCObj)->GetNumberField(TEXT("deltaE"));
        Profile.ColorChecker.MaxDeltaE = (*CCObj)->GetNumberField(TEXT("maxDeltaE"));
        Profile.ColorChecker.CalibratedAt = (*CCObj)->GetStringField(TEXT("calibratedAt"));

        // Color matrix (3x3 stored as flat array)
        const TArray<TSharedPtr<FJsonValue>>* MatrixArray;
        if ((*CCObj)->TryGetArrayField(TEXT("colorMatrix"), MatrixArray))
        {
            for (int32 Row = 0; Row < 3 && Row < MatrixArray->Num(); Row++)
            {
                const TArray<TSharedPtr<FJsonValue>>& RowArray = (*MatrixArray)[Row]->AsArray();
                for (int32 Col = 0; Col < 3 && Col < RowArray.Num(); Col++)
                {
                    Profile.ColorChecker.ColorMatrix.Add(RowArray[Col]->AsNumber());
                }
            }
        }
    }

    // Recommended exposure
    const TSharedPtr<FJsonObject>* ExposureObj;
    if (Data->TryGetObjectField(TEXT("recommendedExposure"), ExposureObj))
    {
        Profile.RecommendedExposure.ISO = (*ExposureObj)->GetIntegerField(TEXT("iso"));
        Profile.RecommendedExposure.ShutterSpeed = (*ExposureObj)->GetStringField(TEXT("shutterSpeed"));
        Profile.RecommendedExposure.Aperture = (*ExposureObj)->GetNumberField(TEXT("aperture"));
        Profile.RecommendedExposure.WhiteBalanceKelvin = (*ExposureObj)->GetNumberField(TEXT("whiteBalanceKelvin"));
    }

    return Profile;
}
