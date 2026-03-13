// Rship Fixture Manager Implementation

#include "RshipFixtureManager.h"
#include "RshipSubsystem.h"
#include "Logs.h"

void URshipFixtureManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogRshipExec, Log, TEXT("FixtureManager initialized"));
}

void URshipFixtureManager::Shutdown()
{
    Fixtures.Empty();
    FixtureTypes.Empty();
    Calibrations.Empty();
    CalibrationsByFixtureType.Empty();
    Subsystem = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("FixtureManager shutdown"));
}

// ============================================================================
// FIXTURE QUERIES
// ============================================================================

TArray<FRshipFixtureInfo> URshipFixtureManager::GetAllFixtures() const
{
    TArray<FRshipFixtureInfo> Result;
    Fixtures.GenerateValueArray(Result);
    return Result;
}

bool URshipFixtureManager::GetFixtureById(const FString& FixtureId, FRshipFixtureInfo& OutFixture) const
{
    const FRshipFixtureInfo* Found = Fixtures.Find(FixtureId);
    if (Found)
    {
        OutFixture = *Found;
        return true;
    }
    return false;
}

TArray<FRshipFixtureInfo> URshipFixtureManager::GetFixturesByType(const FString& FixtureTypeId) const
{
    TArray<FRshipFixtureInfo> Result;
    for (const auto& Pair : Fixtures)
    {
        if (Pair.Value.FixtureTypeId == FixtureTypeId)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

// ============================================================================
// FIXTURE TYPE QUERIES
// ============================================================================

TArray<FRshipFixtureTypeInfo> URshipFixtureManager::GetAllFixtureTypes() const
{
    TArray<FRshipFixtureTypeInfo> Result;
    FixtureTypes.GenerateValueArray(Result);
    return Result;
}

bool URshipFixtureManager::GetFixtureTypeById(const FString& FixtureTypeId, FRshipFixtureTypeInfo& OutFixtureType) const
{
    const FRshipFixtureTypeInfo* Found = FixtureTypes.Find(FixtureTypeId);
    if (Found)
    {
        OutFixtureType = *Found;
        return true;
    }
    return false;
}

bool URshipFixtureManager::GetFixtureTypeForFixture(const FString& FixtureId, FRshipFixtureTypeInfo& OutFixtureType) const
{
    FRshipFixtureInfo Fixture;
    if (!GetFixtureById(FixtureId, Fixture))
    {
        return false;
    }
    return GetFixtureTypeById(Fixture.FixtureTypeId, OutFixtureType);
}

// ============================================================================
// CALIBRATION QUERIES
// ============================================================================

TArray<FRshipFixtureCalibration> URshipFixtureManager::GetAllCalibrations() const
{
    TArray<FRshipFixtureCalibration> Result;
    Calibrations.GenerateValueArray(Result);
    return Result;
}

bool URshipFixtureManager::GetCalibrationById(const FString& CalibrationId, FRshipFixtureCalibration& OutCalibration) const
{
    const FRshipFixtureCalibration* Found = Calibrations.Find(CalibrationId);
    if (Found)
    {
        OutCalibration = *Found;
        return true;
    }
    return false;
}

bool URshipFixtureManager::GetCalibrationForFixtureType(const FString& FixtureTypeId, FRshipFixtureCalibration& OutCalibration) const
{
    // Get first calibration for this fixture type
    TArray<FString> CalibrationIds;
    CalibrationsByFixtureType.MultiFind(FixtureTypeId, CalibrationIds);

    if (CalibrationIds.Num() > 0)
    {
        return GetCalibrationById(CalibrationIds[0], OutCalibration);
    }
    return false;
}

bool URshipFixtureManager::GetCalibrationForFixture(const FString& FixtureId, FRshipFixtureCalibration& OutCalibration) const
{
    FRshipFixtureInfo Fixture;
    if (!GetFixtureById(FixtureId, Fixture))
    {
        return false;
    }

    // Check for per-fixture override first
    if (!Fixture.CalibrationId.IsEmpty())
    {
        if (GetCalibrationById(Fixture.CalibrationId, OutCalibration))
        {
            return true;
        }
    }

    // Fall back to fixture type calibration
    return GetCalibrationForFixtureType(Fixture.FixtureTypeId, OutCalibration);
}

TArray<FRshipFixtureCalibration> URshipFixtureManager::GetCalibrationsForFixtureType(const FString& FixtureTypeId) const
{
    TArray<FRshipFixtureCalibration> Result;
    TArray<FString> CalibrationIds;
    CalibrationsByFixtureType.MultiFind(FixtureTypeId, CalibrationIds);

    for (const FString& CalibrationId : CalibrationIds)
    {
        FRshipFixtureCalibration Cal;
        if (GetCalibrationById(CalibrationId, Cal))
        {
            Result.Add(Cal);
        }
    }
    return Result;
}

// ============================================================================
// CALIBRATION HELPERS
// ============================================================================

float URshipFixtureManager::DmxToOutputForFixture(const FString& FixtureId, int32 DmxValue) const
{
    FRshipFixtureCalibration Calibration;
    if (GetCalibrationForFixture(FixtureId, Calibration))
    {
        return Calibration.DmxToOutput(DmxValue);
    }
    // Linear fallback
    return FMath::Clamp(DmxValue, 0, 255) / 255.0f;
}

FLinearColor URshipFixtureManager::GetColorCorrectionForFixture(const FString& FixtureId, float TargetKelvin) const
{
    FRshipFixtureCalibration Calibration;
    if (GetCalibrationForFixture(FixtureId, Calibration))
    {
        return Calibration.GetColorCorrection(TargetKelvin);
    }
    return FLinearColor::White;
}

float URshipFixtureManager::GetCalibratedBeamAngleForFixture(const FString& FixtureId) const
{
    FRshipFixtureInfo Fixture;
    FRshipFixtureTypeInfo FixtureType;
    FRshipFixtureCalibration Calibration;

    if (!GetFixtureById(FixtureId, Fixture))
    {
        return 25.0f; // Default
    }

    float SpecBeamAngle = 25.0f;
    if (GetFixtureTypeById(Fixture.FixtureTypeId, FixtureType))
    {
        SpecBeamAngle = FixtureType.BeamAngle;
    }

    if (GetCalibrationForFixture(FixtureId, Calibration))
    {
        return Calibration.GetCalibratedBeamAngle(SpecBeamAngle);
    }

    return SpecBeamAngle;
}

float URshipFixtureManager::GetCalibratedFieldAngleForFixture(const FString& FixtureId) const
{
    FRshipFixtureInfo Fixture;
    FRshipFixtureTypeInfo FixtureType;
    FRshipFixtureCalibration Calibration;

    if (!GetFixtureById(FixtureId, Fixture))
    {
        return 35.0f; // Default
    }

    float SpecFieldAngle = 35.0f;
    if (GetFixtureTypeById(Fixture.FixtureTypeId, FixtureType))
    {
        SpecFieldAngle = FixtureType.FieldAngle;
    }

    if (GetCalibrationForFixture(FixtureId, Calibration))
    {
        return Calibration.GetCalibratedFieldAngle(SpecFieldAngle);
    }

    return SpecFieldAngle;
}

float URshipFixtureManager::GetFalloffExponentForFixture(const FString& FixtureId) const
{
    FRshipFixtureCalibration Calibration;
    if (GetCalibrationForFixture(FixtureId, Calibration))
    {
        return Calibration.FalloffExponent;
    }
    return 2.0f; // Default squared falloff
}

// ============================================================================
// LOCAL REGISTRATION
// ============================================================================

bool URshipFixtureManager::RegisterLocalFixture(const FRshipFixtureInfo& FixtureInfo)
{
    if (!Subsystem || FixtureInfo.Id.IsEmpty())
    {
        return false;
    }

    // Build JSON payload for the fixture
    TSharedPtr<FJsonObject> FixtureJson = MakeShareable(new FJsonObject);

    FixtureJson->SetStringField(TEXT("id"), FixtureInfo.Id);
    FixtureJson->SetStringField(TEXT("name"), FixtureInfo.Name);
    FixtureJson->SetStringField(TEXT("fixtureTypeId"), FixtureInfo.FixtureTypeId);

    // Position
    FixtureJson->SetNumberField(TEXT("x"), FixtureInfo.Position.X);
    FixtureJson->SetNumberField(TEXT("y"), FixtureInfo.Position.Y);
    FixtureJson->SetNumberField(TEXT("z"), FixtureInfo.Position.Z);

    // Rotation
    FixtureJson->SetNumberField(TEXT("rotX"), FixtureInfo.Rotation.Pitch);
    FixtureJson->SetNumberField(TEXT("rotY"), FixtureInfo.Rotation.Yaw);
    FixtureJson->SetNumberField(TEXT("rotZ"), FixtureInfo.Rotation.Roll);

    // DMX
    FixtureJson->SetNumberField(TEXT("universe"), FixtureInfo.Universe);
    FixtureJson->SetNumberField(TEXT("address"), FixtureInfo.Address);
    FixtureJson->SetStringField(TEXT("mode"), FixtureInfo.Mode);

    // Optional fields
    if (!FixtureInfo.EmitterId.IsEmpty())
    {
        FixtureJson->SetStringField(TEXT("emitterId"), FixtureInfo.EmitterId);
    }
    if (!FixtureInfo.CalibrationId.IsEmpty())
    {
        FixtureJson->SetStringField(TEXT("calibrationId"), FixtureInfo.CalibrationId);
    }

    // Send as SET command via subsystem
    // Use SetItem which wraps in proper myko message format
    Subsystem->SetItem(TEXT("Fixture"), FixtureJson, ERshipMessagePriority::High, FixtureInfo.Id);

    // Also cache locally
    Fixtures.Add(FixtureInfo.Id, FixtureInfo);

    UE_LOG(LogRshipExec, Log, TEXT("FixtureManager: Registered local fixture '%s' (%s)"),
        *FixtureInfo.Name, *FixtureInfo.Id);

    OnFixtureAdded.Broadcast(FixtureInfo);
    OnFixturesUpdated.Broadcast();

    return true;
}

bool URshipFixtureManager::UpdateFixturePosition(const FString& FixtureId, const FVector& Position, const FRotator& Rotation)
{
    if (!Subsystem || FixtureId.IsEmpty())
    {
        return false;
    }

    // Get existing fixture
    FRshipFixtureInfo* Existing = Fixtures.Find(FixtureId);
    if (!Existing)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("FixtureManager: Cannot update position for unknown fixture: %s"), *FixtureId);
        return false;
    }

    // Update local cache
    Existing->Position = Position;
    Existing->Rotation = Rotation;

    // Send update to server
    TSharedPtr<FJsonObject> UpdateJson = MakeShareable(new FJsonObject);
    UpdateJson->SetStringField(TEXT("id"), FixtureId);
    UpdateJson->SetStringField(TEXT("name"), Existing->Name);
    UpdateJson->SetStringField(TEXT("fixtureTypeId"), Existing->FixtureTypeId);

    UpdateJson->SetNumberField(TEXT("x"), Position.X);
    UpdateJson->SetNumberField(TEXT("y"), Position.Y);
    UpdateJson->SetNumberField(TEXT("z"), Position.Z);
    UpdateJson->SetNumberField(TEXT("rotX"), Rotation.Pitch);
    UpdateJson->SetNumberField(TEXT("rotY"), Rotation.Yaw);
    UpdateJson->SetNumberField(TEXT("rotZ"), Rotation.Roll);

    UpdateJson->SetNumberField(TEXT("universe"), Existing->Universe);
    UpdateJson->SetNumberField(TEXT("address"), Existing->Address);

    Subsystem->SetItem(TEXT("Fixture"), UpdateJson, ERshipMessagePriority::Normal, FixtureId);

    UE_LOG(LogRshipExec, Verbose, TEXT("FixtureManager: Updated position for fixture %s"), *FixtureId);

    OnFixturesUpdated.Broadcast();

    return true;
}

bool URshipFixtureManager::UnregisterFixture(const FString& FixtureId)
{
    if (!Subsystem || FixtureId.IsEmpty())
    {
        return false;
    }

    // Send DELETE command
    TSharedPtr<FJsonObject> DeleteJson = MakeShareable(new FJsonObject);
    DeleteJson->SetStringField(TEXT("id"), FixtureId);

    // Build delete event
    TSharedPtr<FJsonObject> EventData = MakeShareable(new FJsonObject);
    EventData->SetStringField(TEXT("itemType"), TEXT("Fixture"));
    EventData->SetObjectField(TEXT("item"), DeleteJson);

    TSharedPtr<FJsonObject> Event = MakeShareable(new FJsonObject);
    Event->SetStringField(TEXT("event"), TEXT("ws:m:del"));
    Event->SetObjectField(TEXT("data"), EventData);

    Subsystem->SendJson(Event);

    // Remove from local cache
    if (Fixtures.Remove(FixtureId) > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("FixtureManager: Unregistered fixture %s"), *FixtureId);
        OnFixtureRemoved.Broadcast(FixtureId);
        OnFixturesUpdated.Broadcast();
        return true;
    }

    return false;
}

// ============================================================================
// ENTITY PROCESSING
// ============================================================================

void URshipFixtureManager::ProcessFixtureEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
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
        if (Fixtures.Remove(Id) > 0)
        {
            UE_LOG(LogRshipExec, Log, TEXT("Fixture removed: %s"), *Id);
            OnFixtureRemoved.Broadcast(Id);
            OnFixturesUpdated.Broadcast();
        }
    }
    else
    {
        FRshipFixtureInfo Fixture = ParseFixture(Data);
        bool bIsNew = !Fixtures.Contains(Id);
        Fixtures.Add(Id, Fixture);

        UE_LOG(LogRshipExec, Log, TEXT("Fixture %s: %s (%s)"),
            bIsNew ? TEXT("added") : TEXT("updated"),
            *Fixture.Name, *Id);

        if (bIsNew)
        {
            OnFixtureAdded.Broadcast(Fixture);
        }
        OnFixturesUpdated.Broadcast();
    }
}

void URshipFixtureManager::ProcessFixtureTypeEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
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
        if (FixtureTypes.Remove(Id) > 0)
        {
            UE_LOG(LogRshipExec, Log, TEXT("FixtureType removed: %s"), *Id);
        }
    }
    else
    {
        FRshipFixtureTypeInfo FixtureType = ParseFixtureType(Data);
        bool bIsNew = !FixtureTypes.Contains(Id);
        FixtureTypes.Add(Id, FixtureType);

        UE_LOG(LogRshipExec, Log, TEXT("FixtureType %s: %s %s"),
            bIsNew ? TEXT("added") : TEXT("updated"),
            *FixtureType.Manufacturer, *FixtureType.Name);

        if (bIsNew)
        {
            OnFixtureTypeAdded.Broadcast(FixtureType);
        }
    }
}

void URshipFixtureManager::ProcessCalibrationEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
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
        // Remove from index
        const FRshipFixtureCalibration* Existing = Calibrations.Find(Id);
        if (Existing)
        {
            CalibrationsByFixtureType.Remove(Existing->FixtureTypeId, Id);
        }

        if (Calibrations.Remove(Id) > 0)
        {
            UE_LOG(LogRshipExec, Log, TEXT("FixtureCalibration removed: %s"), *Id);
        }
    }
    else
    {
        FRshipFixtureCalibration Calibration = ParseCalibration(Data);

        // Update index
        const FRshipFixtureCalibration* Existing = Calibrations.Find(Id);
        if (Existing && Existing->FixtureTypeId != Calibration.FixtureTypeId)
        {
            // Fixture type changed, remove old index entry
            CalibrationsByFixtureType.Remove(Existing->FixtureTypeId, Id);
        }

        Calibrations.Add(Id, Calibration);
        CalibrationsByFixtureType.AddUnique(Calibration.FixtureTypeId, Id);

        UE_LOG(LogRshipExec, Log, TEXT("FixtureCalibration updated: %s for type %s"),
            *Calibration.Name, *Calibration.FixtureTypeId);

        OnCalibrationUpdated.Broadcast(Calibration);
    }
}

// ============================================================================
// JSON PARSING
// ============================================================================

FRshipFixtureInfo URshipFixtureManager::ParseFixture(const TSharedPtr<FJsonObject>& Data) const
{
    FRshipFixtureInfo Info;

    Info.Id = Data->GetStringField(TEXT("id"));
    Info.Name = Data->GetStringField(TEXT("name"));
    Info.FixtureTypeId = Data->GetStringField(TEXT("fixtureTypeId"));

    // Position
    Info.Position.X = Data->GetNumberField(TEXT("x"));
    Info.Position.Y = Data->GetNumberField(TEXT("y"));
    Info.Position.Z = Data->GetNumberField(TEXT("z"));

    // Rotation
    Info.Rotation.Pitch = Data->GetNumberField(TEXT("rotX"));
    Info.Rotation.Yaw = Data->GetNumberField(TEXT("rotY"));
    Info.Rotation.Roll = Data->GetNumberField(TEXT("rotZ"));

    // DMX
    Info.Universe = Data->GetIntegerField(TEXT("universe"));
    Info.Address = Data->GetIntegerField(TEXT("address"));
    Info.Mode = Data->GetStringField(TEXT("mode"));

    // Optional fields
    Data->TryGetStringField(TEXT("emitterId"), Info.EmitterId);
    Data->TryGetStringField(TEXT("calibrationId"), Info.CalibrationId);

    return Info;
}

FRshipFixtureTypeInfo URshipFixtureManager::ParseFixtureType(const TSharedPtr<FJsonObject>& Data) const
{
    FRshipFixtureTypeInfo Info;

    Info.Id = Data->GetStringField(TEXT("id"));
    Info.Name = Data->GetStringField(TEXT("name"));

    Data->TryGetStringField(TEXT("manufacturer"), Info.Manufacturer);

    // Beam angles
    Info.BeamAngle = Data->GetNumberField(TEXT("beamAngle"));
    Info.FieldAngle = Data->GetNumberField(TEXT("fieldAngle"));

    // Color and output
    Info.ColorTemperature = Data->GetNumberField(TEXT("colorTemperature"));
    Info.Lumens = Data->GetIntegerField(TEXT("lumens"));

    // Asset URLs
    Data->TryGetStringField(TEXT("iesProfileUrl"), Info.IESProfileUrl);
    Data->TryGetStringField(TEXT("gdtfUrl"), Info.GDTFUrl);
    Data->TryGetStringField(TEXT("geometryUrl"), Info.GeometryUrl);

    // Capabilities
    Info.bHasPanTilt = Data->GetBoolField(TEXT("hasPanTilt"));
    Info.bHasZoom = Data->GetBoolField(TEXT("hasZoom"));
    Info.bHasGobo = Data->GetBoolField(TEXT("hasGobo"));

    // Movement ranges
    Info.MaxPan = Data->GetNumberField(TEXT("maxPan"));
    Info.MaxTilt = Data->GetNumberField(TEXT("maxTilt"));

    // Zoom range
    const TArray<TSharedPtr<FJsonValue>>* ZoomArray;
    if (Data->TryGetArrayField(TEXT("zoomRange"), ZoomArray) && ZoomArray->Num() >= 2)
    {
        Info.ZoomRange.X = (*ZoomArray)[0]->AsNumber();
        Info.ZoomRange.Y = (*ZoomArray)[1]->AsNumber();
    }

    return Info;
}

FRshipFixtureCalibration URshipFixtureManager::ParseCalibration(const TSharedPtr<FJsonObject>& Data) const
{
    FRshipFixtureCalibration Cal;

    Cal.Id = Data->GetStringField(TEXT("id"));
    Cal.Name = Data->GetStringField(TEXT("name"));
    Cal.FixtureTypeId = Data->GetStringField(TEXT("fixtureTypeId"));
    Cal.ProjectId = Data->GetStringField(TEXT("projectId"));

    Data->TryGetStringField(TEXT("hash"), Cal.Hash);

    // Dimmer curve
    const TArray<TSharedPtr<FJsonValue>>* DimmerCurveArray;
    if (Data->TryGetArrayField(TEXT("dimmerCurve"), DimmerCurveArray))
    {
        for (const TSharedPtr<FJsonValue>& PointValue : *DimmerCurveArray)
        {
            TSharedPtr<FJsonObject> PointObj = PointValue->AsObject();
            if (PointObj.IsValid())
            {
                FRshipDimmerCurvePoint Point;
                Point.DmxValue = PointObj->GetIntegerField(TEXT("dmxValue"));
                Point.OutputPercent = PointObj->GetNumberField(TEXT("outputPercent"));
                Cal.DimmerCurve.Add(Point);
            }
        }
    }

    Cal.MinVisibleDmx = Data->GetIntegerField(TEXT("minVisibleDmx"));

    // Color calibrations
    const TArray<TSharedPtr<FJsonValue>>* ColorCalsArray;
    if (Data->TryGetArrayField(TEXT("colorCalibrations"), ColorCalsArray))
    {
        for (const TSharedPtr<FJsonValue>& CalValue : *ColorCalsArray)
        {
            TSharedPtr<FJsonObject> CalObj = CalValue->AsObject();
            if (CalObj.IsValid())
            {
                FRshipColorCalibration ColorCal;
                ColorCal.TargetKelvin = CalObj->GetNumberField(TEXT("targetKelvin"));
                ColorCal.MeasuredKelvin = CalObj->GetNumberField(TEXT("measuredKelvin"));

                // Chromaticity offset
                TSharedPtr<FJsonObject> ChromOffset = CalObj->GetObjectField(TEXT("chromaticityOffset"));
                if (ChromOffset.IsValid())
                {
                    ColorCal.ChromaticityOffset.X = ChromOffset->GetNumberField(TEXT("x"));
                    ColorCal.ChromaticityOffset.Y = ChromOffset->GetNumberField(TEXT("y"));
                }

                // RGB correction
                TSharedPtr<FJsonObject> RgbCorr = CalObj->GetObjectField(TEXT("rgbCorrection"));
                if (RgbCorr.IsValid())
                {
                    ColorCal.RgbCorrection.R = RgbCorr->GetNumberField(TEXT("r"));
                    ColorCal.RgbCorrection.G = RgbCorr->GetNumberField(TEXT("g"));
                    ColorCal.RgbCorrection.B = RgbCorr->GetNumberField(TEXT("b"));
                    ColorCal.RgbCorrection.A = 1.0f;
                }

                Cal.ColorCalibrations.Add(ColorCal);
            }
        }
    }

    // White point
    Cal.ActualWhitePoint = Data->GetNumberField(TEXT("actualWhitePoint"));

    // Beam adjustments
    Cal.BeamAngleMultiplier = Data->GetNumberField(TEXT("beamAngleMultiplier"));
    if (Cal.BeamAngleMultiplier <= 0.0f) Cal.BeamAngleMultiplier = 1.0f;

    Cal.FieldAngleMultiplier = Data->GetNumberField(TEXT("fieldAngleMultiplier"));
    if (Cal.FieldAngleMultiplier <= 0.0f) Cal.FieldAngleMultiplier = 1.0f;

    Cal.FalloffExponent = Data->GetNumberField(TEXT("falloffExponent"));
    if (Cal.FalloffExponent <= 0.0f) Cal.FalloffExponent = 2.0f;

    // Reference data
    Data->TryGetStringField(TEXT("referencePhotoUrl"), Cal.ReferencePhotoUrl);
    Data->TryGetStringField(TEXT("notes"), Cal.Notes);

    return Cal;
}
