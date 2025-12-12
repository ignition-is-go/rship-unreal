// Rship Fixture Actor Implementation

#include "RshipFixtureActor.h"
#include "RshipSubsystem.h"
#include "RshipFixtureManager.h"
#include "RshipIESProfileService.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

ARshipFixtureActor::ARshipFixtureActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Create root component
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(RootSceneComponent);

    // Create body mesh component (placeholder, can be replaced in Blueprint)
    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(RootSceneComponent);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create spot light component
    BeamLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("BeamLight"));
    BeamLight->SetupAttachment(RootSceneComponent);
    BeamLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // Point down by default
    BeamLight->SetIntensity(0.0f); // Start off
    BeamLight->SetInnerConeAngle(12.5f);
    BeamLight->SetOuterConeAngle(17.5f);
    BeamLight->SetAttenuationRadius(1000.0f);
    BeamLight->SetCastShadows(true);
}

void ARshipFixtureActor::BeginPlay()
{
    Super::BeginPlay();

    // Get subsystem
    if (GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    }

    if (Subsystem)
    {
        FixtureManager = Subsystem->GetFixtureManager();
        BindToManager();
        RefreshFixtureData();
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("ARshipFixtureActor: Could not get URshipSubsystem"));
    }
}

void ARshipFixtureActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindFromManager();
    Super::EndPlay(EndPlayReason);
}

void ARshipFixtureActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update light visualization each frame (for smooth DMX response)
    UpdateLightVisualization();
}

void ARshipFixtureActor::BindToManager()
{
    if (!FixtureManager)
    {
        return;
    }

    // Bind to fixture updates
    FixtureUpdateHandle = FixtureManager->OnFixturesUpdated.AddLambda([this]()
    {
        OnFixturesUpdatedInternal();
    });

    // Bind to calibration updates
    CalibrationUpdateHandle = FixtureManager->OnCalibrationUpdated.AddLambda([this](const FRshipFixtureCalibration& Calibration)
    {
        OnCalibrationUpdatedInternal(Calibration);
    });
}

void ARshipFixtureActor::UnbindFromManager()
{
    if (FixtureManager)
    {
        if (FixtureUpdateHandle.IsValid())
        {
            FixtureManager->OnFixturesUpdated.Remove(FixtureUpdateHandle);
        }
        if (CalibrationUpdateHandle.IsValid())
        {
            FixtureManager->OnCalibrationUpdated.Remove(CalibrationUpdateHandle);
        }
    }
    FixtureUpdateHandle.Reset();
    CalibrationUpdateHandle.Reset();
}

void ARshipFixtureActor::RefreshFixtureData()
{
    if (!FixtureManager || FixtureId.IsEmpty())
    {
        return;
    }

    // Get fixture info
    if (FixtureManager->GetFixtureById(FixtureId, CachedFixtureInfo))
    {
        // Get fixture type
        FixtureManager->GetFixtureTypeById(CachedFixtureInfo.FixtureTypeId, CachedFixtureType);

        // Get calibration
        FixtureManager->GetCalibrationForFixture(FixtureId, CachedCalibration);

        // Apply transform if enabled
        if (bSyncTransformFromServer)
        {
            ApplyServerTransform();
        }

        // Load IES profile if available and not already loaded
        LoadIESProfile();

        // Update light properties from fixture type (IES will override if loaded)
        if (BeamLight)
        {
            BeamLight->SetInnerConeAngle(GetCalibratedBeamAngle() * 0.5f);
            BeamLight->SetOuterConeAngle(GetCalibratedFieldAngle() * 0.5f);
        }

        OnFixtureDataUpdated();

        UE_LOG(LogRshipExec, Log, TEXT("ARshipFixtureActor: Loaded fixture %s (%s)"),
            *CachedFixtureInfo.Name, *FixtureId);
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("ARshipFixtureActor: Fixture not found: %s"), *FixtureId);
    }
}

void ARshipFixtureActor::OnFixturesUpdatedInternal()
{
    // Re-fetch our fixture data if it was updated
    if (!FixtureId.IsEmpty() && FixtureManager)
    {
        FRshipFixtureInfo NewInfo;
        if (FixtureManager->GetFixtureById(FixtureId, NewInfo))
        {
            CachedFixtureInfo = NewInfo;

            if (bSyncTransformFromServer)
            {
                ApplyServerTransform();
            }

            OnFixtureDataUpdated();
        }
    }
}

void ARshipFixtureActor::OnCalibrationUpdatedInternal(const FRshipFixtureCalibration& Calibration)
{
    // Check if this calibration applies to our fixture type
    if (Calibration.FixtureTypeId == CachedFixtureInfo.FixtureTypeId ||
        Calibration.Id == CachedFixtureInfo.CalibrationId)
    {
        CachedCalibration = Calibration;

        // Update beam angles
        if (BeamLight)
        {
            BeamLight->SetInnerConeAngle(GetCalibratedBeamAngle() * 0.5f);
            BeamLight->SetOuterConeAngle(GetCalibratedFieldAngle() * 0.5f);
        }

        OnCalibrationUpdated();

        UE_LOG(LogRshipExec, Log, TEXT("ARshipFixtureActor: Calibration updated for %s"), *FixtureId);
    }
}

void ARshipFixtureActor::OnFixtureDataUpdated_Implementation()
{
    // Blueprint can override this
}

void ARshipFixtureActor::OnCalibrationUpdated_Implementation()
{
    // Blueprint can override this
}

void ARshipFixtureActor::ApplyServerTransform()
{
    // Convert rship coordinates to UE coordinates
    // rship uses meters, UE uses centimeters by default
    FVector NewLocation(
        CachedFixtureInfo.Position.X * PositionScale,
        CachedFixtureInfo.Position.Y * PositionScale,
        CachedFixtureInfo.Position.Z * PositionScale
    );

    // Apply rotation (may need coordinate system conversion)
    FRotator NewRotation = CachedFixtureInfo.Rotation;

    SetActorLocationAndRotation(NewLocation, NewRotation);
}

void ARshipFixtureActor::UpdateLightVisualization()
{
    if (!BeamLight)
    {
        return;
    }

    // Get calibrated output
    float Intensity = GetCalibratedDimmerOutput();
    FLinearColor Color = GetCalibratedColor();

    // Apply to light
    // Scale intensity to reasonable UE light intensity (lumens or unitless depending on settings)
    float LightIntensity = Intensity * CachedFixtureType.Lumens;
    BeamLight->SetIntensity(LightIntensity);
    BeamLight->SetLightColor(Color);

    // Debug visualization
    if (bShowDebugVisualization && Intensity > 0.0f)
    {
        FVector Start = GetActorLocation();
        FVector End = Start + GetActorForwardVector() * 500.0f; // 5m beam length
        DrawDebugLine(GetWorld(), Start, End, Color.ToFColor(true), false, -1.0f, 0, 2.0f);
    }
}

float ARshipFixtureActor::GetCalibratedDimmerOutput() const
{
    if (CachedCalibration.HasDimmerCurve())
    {
        return CachedCalibration.DmxToOutput(RawDMXIntensity);
    }
    // Linear fallback
    return RawDMXIntensity / 255.0f;
}

FLinearColor ARshipFixtureActor::GetCalibratedColor() const
{
    // Start with base color from color temperature
    FLinearColor BaseColor = FLinearColor::White;

    // TODO: Convert color temperature to RGB
    // For now, use white

    // Apply color calibration correction
    if (CachedCalibration.HasColorCalibration())
    {
        FLinearColor Correction = CachedCalibration.GetColorCorrection(CurrentColorTemp);
        BaseColor.R *= Correction.R;
        BaseColor.G *= Correction.G;
        BaseColor.B *= Correction.B;
    }

    return BaseColor;
}

float ARshipFixtureActor::GetCalibratedBeamAngle() const
{
    float SpecAngle = CachedFixtureType.BeamAngle;
    if (SpecAngle <= 0.0f)
    {
        SpecAngle = 25.0f; // Default
    }

    return CachedCalibration.GetCalibratedBeamAngle(SpecAngle);
}

float ARshipFixtureActor::GetCalibratedFieldAngle() const
{
    float SpecAngle = CachedFixtureType.FieldAngle;
    if (SpecAngle <= 0.0f)
    {
        SpecAngle = 35.0f; // Default
    }

    return CachedCalibration.GetCalibratedFieldAngle(SpecAngle);
}

void ARshipFixtureActor::SetDMXChannel(const FString& ChannelName, float Value)
{
    Value = FMath::Clamp(Value, 0.0f, 1.0f);
    CurrentDMXValues.Add(ChannelName, Value);

    // Handle common channel names
    if (ChannelName.Equals(TEXT("intensity"), ESearchCase::IgnoreCase) ||
        ChannelName.Equals(TEXT("dimmer"), ESearchCase::IgnoreCase))
    {
        RawDMXIntensity = FMath::RoundToInt(Value * 255.0f);
    }
    else if (ChannelName.Equals(TEXT("colortemp"), ESearchCase::IgnoreCase) ||
             ChannelName.Equals(TEXT("cto"), ESearchCase::IgnoreCase))
    {
        // Map 0-1 to typical color temp range (2700K - 6500K)
        CurrentColorTemp = FMath::Lerp(2700.0f, 6500.0f, Value);
    }

    OnDMXUpdated.Broadcast(CurrentDMXValues);
}

int32 ARshipFixtureActor::GetDMXIntensity() const
{
    return RawDMXIntensity;
}

// ============================================================================
// IES PROFILE INTEGRATION
// ============================================================================

void ARshipFixtureActor::LoadIESProfile()
{
    // Check if we have an IES profile URL
    if (CachedFixtureType.IESProfileUrl.IsEmpty())
    {
        return;
    }

    // Check if already loaded or loading this URL
    if (LoadedIESProfileUrl == CachedFixtureType.IESProfileUrl)
    {
        return;
    }

    // Get IES profile service
    if (!Subsystem)
    {
        return;
    }

    URshipIESProfileService* IESService = Subsystem->GetIESProfileService();
    if (!IESService)
    {
        return;
    }

    LoadedIESProfileUrl = CachedFixtureType.IESProfileUrl;

    UE_LOG(LogRshipExec, Log, TEXT("ARshipFixtureActor: Loading IES profile from %s"), *LoadedIESProfileUrl);

    // Create delegate for callback (dynamic delegate requires BindDynamic)
    FOnIESProfileLoaded Callback;
    Callback.BindDynamic(this, &ARshipFixtureActor::OnIESProfileLoadedInternal);

    IESService->LoadProfile(LoadedIESProfileUrl, Callback);
}

void ARshipFixtureActor::OnIESProfileLoadedInternal(bool bSuccess, const FRshipIESProfile& Profile)
{
    if (!bSuccess)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("ARshipFixtureActor: Failed to load IES profile for fixture %s"), *FixtureId);
        bHasIESProfile = false;
        return;
    }

    CachedIESProfile = Profile;
    bHasIESProfile = true;

    UE_LOG(LogRshipExec, Log, TEXT("ARshipFixtureActor: IES profile loaded - peak candela: %.1f, beam angle: %.1f°, field angle: %.1f°"),
        Profile.PeakCandela, Profile.BeamAngle, Profile.FieldAngle);

    // Generate and apply light profile texture
    ApplyIESProfile();

    // Notify Blueprint
    OnIESProfileLoaded();
}

void ARshipFixtureActor::ApplyIESProfile()
{
    if (!bHasIESProfile || !Subsystem)
    {
        return;
    }

    URshipIESProfileService* IESService = Subsystem->GetIESProfileService();
    if (!IESService)
    {
        return;
    }

    // Generate light profile texture
    IESLightProfileTexture = IESService->GenerateLightProfileTexture(CachedIESProfile, 256);

    if (IESLightProfileTexture && BeamLight)
    {
        // Apply to spot light
        BeamLight->SetIESTexture(IESLightProfileTexture);

        // Update cone angles from IES profile
        float IESBeam = GetIESBeamAngle();
        float IESField = GetIESFieldAngle();

        BeamLight->SetInnerConeAngle(IESBeam * 0.5f);
        BeamLight->SetOuterConeAngle(IESField * 0.5f);

        UE_LOG(LogRshipExec, Log, TEXT("ARshipFixtureActor: Applied IES profile texture to light"));
    }
}

void ARshipFixtureActor::ReloadIESProfile()
{
    // Clear current state
    LoadedIESProfileUrl.Empty();
    bHasIESProfile = false;
    IESLightProfileTexture = nullptr;

    if (BeamLight)
    {
        BeamLight->SetIESTexture(nullptr);
    }

    // Trigger reload
    LoadIESProfile();
}

void ARshipFixtureActor::OnIESProfileLoaded_Implementation()
{
    // Blueprint can override this
}

float ARshipFixtureActor::GetIESBeamAngle() const
{
    if (bHasIESProfile && CachedIESProfile.BeamAngle > 0.0f)
    {
        return CachedIESProfile.BeamAngle;
    }
    return GetCalibratedBeamAngle();
}

float ARshipFixtureActor::GetIESFieldAngle() const
{
    if (bHasIESProfile && CachedIESProfile.FieldAngle > 0.0f)
    {
        return CachedIESProfile.FieldAngle;
    }
    return GetCalibratedFieldAngle();
}

float ARshipFixtureActor::GetIESIntensityAtAngle(float VerticalAngle, float HorizontalAngle) const
{
    if (bHasIESProfile)
    {
        return CachedIESProfile.GetIntensity(VerticalAngle, HorizontalAngle);
    }
    // Fallback: simple cosine distribution
    float AngleRad = FMath::DegreesToRadians(VerticalAngle);
    return FMath::Max(0.0f, FMath::Cos(AngleRad));
}
