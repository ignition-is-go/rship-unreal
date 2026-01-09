// Rship Scene Converter Implementation

#include "RshipSceneConverter.h"
#include "RshipSubsystem.h"
#include "RshipFixtureManager.h"
#include "RshipCameraManager.h"
#include "RshipEditorTransformSync.h"
#include "RshipSceneValidator.h"
#include "RshipFixtureActor.h"
#include "RshipCameraActor.h"
#include "Logs.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"

void URshipSceneConverter::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

    if (Subsystem)
    {
        FixtureManager = Subsystem->GetFixtureManager();
        CameraManager = Subsystem->GetCameraManager();
    }

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter initialized"));
}

void URshipSceneConverter::Shutdown()
{
    ClearDiscoveryResults();
    ConvertedActors.Empty();
    GenericFixtureTypes.Empty();

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter shutdown"));
}

// ============================================================================
// DISCOVERY
// ============================================================================

int32 URshipSceneConverter::DiscoverScene(const FRshipDiscoveryOptions& Options)
{
    ClearDiscoveryResults();

    UWorld* World = nullptr;
    if (Subsystem)
    {
        World = Subsystem->GetWorld();
    }

    if (!World)
    {
        // Try to get any world
        World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
        if (!World && GEngine && GEngine->GetWorldContexts().Num() > 0)
        {
            World = GEngine->GetWorldContexts()[0].World();
        }
    }

    if (!World)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipSceneConverter: No world available for discovery"));
        return 0;
    }

    // Discover lights
    if (Options.bIncludeSpotLights || Options.bIncludePointLights ||
        Options.bIncludeDirectionalLights || Options.bIncludeRectLights)
    {
        DiscoverLightsInWorld(World, Options);
    }

    // Discover cameras
    if (Options.bIncludeCameras)
    {
        DiscoverCamerasInWorld(World, Options);
    }

    int32 TotalFound = DiscoveredLights.Num() + DiscoveredCameras.Num();

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter: Discovered %d lights and %d cameras"),
        DiscoveredLights.Num(), DiscoveredCameras.Num());

    // Fire event
    OnDiscoveryComplete.Broadcast(DiscoveredLights, DiscoveredCameras);

    return TotalFound;
}

void URshipSceneConverter::DiscoverLightsInWorld(UWorld* World, const FRshipDiscoveryOptions& Options)
{
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || Actor->IsPendingKillPending())
        {
            continue;
        }

        // Check tag filter
        if (!Options.RequiredTag.IsNone() && !Actor->ActorHasTag(Options.RequiredTag))
        {
            continue;
        }

        // Get all light components on this actor
        TArray<ULightComponent*> LightComponents;
        Actor->GetComponents<ULightComponent>(LightComponents);

        for (ULightComponent* Light : LightComponents)
        {
            if (!Light)
            {
                continue;
            }

            // Determine light type and filter
            FString LightType = DetermineLightType(Light);

            if (LightType == TEXT("Spot") && !Options.bIncludeSpotLights) continue;
            if (LightType == TEXT("Point") && !Options.bIncludePointLights) continue;
            if (LightType == TEXT("Directional") && !Options.bIncludeDirectionalLights) continue;
            if (LightType == TEXT("Rect") && !Options.bIncludeRectLights) continue;
            if (LightType == TEXT("Unknown")) continue;

            // Check intensity filter
            if (Light->Intensity < Options.MinIntensity)
            {
                continue;
            }

            // Check if already converted
            FString ExistingId;
            bool bAlreadyConverted = IsLightAlreadyConverted(Light, ExistingId);

            if (bAlreadyConverted && Options.bSkipAlreadyConverted)
            {
                continue;
            }

            // Build discovery info
            FRshipDiscoveredLight Discovered;
            Discovered.LightComponent = Light;
            Discovered.OwnerActor = Actor;
            Discovered.SuggestedName = GenerateFixtureName(Actor, TEXT(""));
            Discovered.LightType = LightType;
            Discovered.Position = Light->GetComponentLocation();
            Discovered.Rotation = Light->GetComponentRotation();
            Discovered.Intensity = Light->Intensity;
            Discovered.Color = Light->GetLightColor();
            Discovered.bAlreadyConverted = bAlreadyConverted;
            Discovered.ExistingFixtureId = ExistingId;

            // Get cone angles for spot lights
            if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Light))
            {
                Discovered.InnerConeAngle = SpotLight->InnerConeAngle;
                Discovered.OuterConeAngle = SpotLight->OuterConeAngle;
            }

            DiscoveredLights.Add(Discovered);
        }
    }
}

void URshipSceneConverter::DiscoverCamerasInWorld(UWorld* World, const FRshipDiscoveryOptions& Options)
{
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        ACameraActor* Camera = *It;
        if (!Camera || Camera->IsPendingKillPending())
        {
            continue;
        }

        // Check tag filter
        if (!Options.RequiredTag.IsNone() && !Camera->ActorHasTag(Options.RequiredTag))
        {
            continue;
        }

        // Check if already converted
        FString ExistingId;
        bool bAlreadyConverted = IsCameraAlreadyConverted(Camera, ExistingId);

        if (bAlreadyConverted && Options.bSkipAlreadyConverted)
        {
            continue;
        }

        // Get camera component
        UCameraComponent* CameraComp = Camera->GetCameraComponent();

        // Build discovery info
        FRshipDiscoveredCamera Discovered;
        Discovered.CameraActor = Camera;
        FString CameraLabel = FString(Camera->GetActorNameOrLabel());
        Discovered.SuggestedName = CameraLabel.IsEmpty() ? Camera->GetName() : CameraLabel;
        Discovered.Position = Camera->GetActorLocation();
        Discovered.Rotation = Camera->GetActorRotation();
        Discovered.bAlreadyConverted = bAlreadyConverted;
        Discovered.ExistingCameraId = ExistingId;

        if (CameraComp)
        {
            Discovered.FOV = CameraComp->FieldOfView;
            Discovered.AspectRatio = CameraComp->AspectRatio;
        }

        DiscoveredCameras.Add(Discovered);
    }
}

void URshipSceneConverter::ClearDiscoveryResults()
{
    DiscoveredLights.Empty();
    DiscoveredCameras.Empty();
}

FString URshipSceneConverter::DetermineLightType(ULightComponent* Light) const
{
    if (Cast<USpotLightComponent>(Light))
    {
        return TEXT("Spot");
    }
    if (Cast<UPointLightComponent>(Light))
    {
        return TEXT("Point");
    }
    if (Cast<UDirectionalLightComponent>(Light))
    {
        return TEXT("Directional");
    }
    if (Cast<URectLightComponent>(Light))
    {
        return TEXT("Rect");
    }
    return TEXT("Unknown");
}

bool URshipSceneConverter::IsLightAlreadyConverted(ULightComponent* Light, FString& OutFixtureId) const
{
    if (!Light || !Light->GetOwner())
    {
        return false;
    }

    AActor* Owner = Light->GetOwner();

    // Check our tracking map
    if (const FString* FoundId = ConvertedActors.Find(Owner))
    {
        OutFixtureId = *FoundId;
        return true;
    }

    // Check if there's an ARshipFixtureActor nearby controlling this light
    // (This handles cases where conversion was done in a previous session)
    // For now, we rely on the tracking map which is session-local

    return false;
}

bool URshipSceneConverter::IsCameraAlreadyConverted(ACameraActor* Camera, FString& OutCameraId) const
{
    if (!Camera)
    {
        return false;
    }

    // Check our tracking map
    if (const FString* FoundId = ConvertedActors.Find(Camera))
    {
        OutCameraId = *FoundId;
        return true;
    }

    return false;
}

// ============================================================================
// CONVERSION
// ============================================================================

FRshipConversionResult URshipSceneConverter::ConvertLight(const FRshipDiscoveredLight& Light, const FRshipConversionOptions& Options)
{
    FRshipConversionResult Result;

    if (!Light.LightComponent || !Light.OwnerActor)
    {
        Result.ErrorMessage = TEXT("Invalid light or owner actor");
        return Result;
    }

    if (!FixtureManager)
    {
        Result.ErrorMessage = TEXT("Fixture manager not available");
        return Result;
    }

    // Create fixture info
    FRshipFixtureInfo FixtureInfo = CreateFixtureInfoFromLight(Light, Options, 0);

    // Determine fixture type
    FString FixtureTypeId = Options.FixtureTypeId;
    if (FixtureTypeId.IsEmpty())
    {
        FixtureTypeId = GetOrCreateGenericFixtureType(Light.LightType);
    }
    FixtureInfo.FixtureTypeId = FixtureTypeId;

    // Register with fixture manager (sends to server)
    bool bRegistered = FixtureManager->RegisterLocalFixture(FixtureInfo);

    if (!bRegistered)
    {
        Result.ErrorMessage = TEXT("Failed to register fixture with server");
        return Result;
    }

    Result.bSuccess = true;
    Result.EntityId = FixtureInfo.Id;

    // Track the conversion
    ConvertedActors.Add(Light.OwnerActor, FixtureInfo.Id);

    // Track for automatic transform sync if enabled
    if (Subsystem && Options.bEnableTransformSync)
    {
        URshipEditorTransformSync* TransformSync = Subsystem->GetEditorTransformSync();
        if (TransformSync)
        {
            TransformSync->TrackActor(Light.OwnerActor, FixtureInfo.Id, true);  // true = is fixture
        }
    }

    // Optionally spawn visualization actor
    if (Options.bSpawnVisualizationActor)
    {
        UWorld* World = Light.OwnerActor->GetWorld();
        if (World)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            ARshipFixtureActor* FixtureActor = World->SpawnActor<ARshipFixtureActor>(
                ARshipFixtureActor::StaticClass(),
                Light.Position,
                Light.Rotation,
                SpawnParams
            );

            if (FixtureActor)
            {
                FixtureActor->FixtureId = FixtureInfo.Id;
                FixtureActor->bSyncTransformFromServer = false;  // We're the source of truth
                Result.VisualizationActor = FixtureActor;
            }
        }
    }

    // Optionally hide original light
    if (Options.bHideOriginalLight)
    {
        Light.LightComponent->SetVisibility(false);
    }

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter: Converted light '%s' to fixture '%s'"),
        *Light.SuggestedName, *FixtureInfo.Id);

    return Result;
}

FRshipConversionResult URshipSceneConverter::ConvertCamera(const FRshipDiscoveredCamera& Camera, const FRshipConversionOptions& Options)
{
    FRshipConversionResult Result;

    if (!Camera.CameraActor)
    {
        Result.ErrorMessage = TEXT("Invalid camera actor");
        return Result;
    }

    if (!CameraManager)
    {
        Result.ErrorMessage = TEXT("Camera manager not available");
        return Result;
    }

    // Create camera info
    FRshipCameraInfo CameraInfo = CreateCameraInfoFromDiscovered(Camera, Options);

    // Register with camera manager (sends to server)
    bool bRegistered = CameraManager->RegisterLocalCamera(CameraInfo);

    if (!bRegistered)
    {
        Result.ErrorMessage = TEXT("Failed to register camera with server");
        return Result;
    }

    Result.bSuccess = true;
    Result.EntityId = CameraInfo.Id;

    // Track the conversion
    ConvertedActors.Add(Camera.CameraActor, CameraInfo.Id);

    // Track for automatic transform sync if enabled
    if (Subsystem && Options.bEnableTransformSync)
    {
        URshipEditorTransformSync* TransformSync = Subsystem->GetEditorTransformSync();
        if (TransformSync)
        {
            TransformSync->TrackActor(Camera.CameraActor, CameraInfo.Id, false);  // false = is camera
        }
    }

    // Optionally spawn visualization actor
    if (Options.bSpawnVisualizationActor)
    {
        UWorld* World = Camera.CameraActor->GetWorld();
        if (World)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            ARshipCameraActor* CameraVisActor = World->SpawnActor<ARshipCameraActor>(
                ARshipCameraActor::StaticClass(),
                Camera.Position,
                Camera.Rotation,
                SpawnParams
            );

            if (CameraVisActor)
            {
                CameraVisActor->CameraId = CameraInfo.Id;
                CameraVisActor->bSyncTransformFromCalibration = false;  // We're the source
                Result.VisualizationActor = CameraVisActor;
            }
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter: Converted camera '%s' to rship camera '%s'"),
        *Camera.SuggestedName, *CameraInfo.Id);

    return Result;
}

int32 URshipSceneConverter::ConvertAllLights(const FRshipConversionOptions& Options, TArray<FRshipConversionResult>& OutResults)
{
    TArray<int32> AllIndices;
    for (int32 i = 0; i < DiscoveredLights.Num(); i++)
    {
        AllIndices.Add(i);
    }
    return ConvertLightsByIndex(AllIndices, Options, OutResults);
}

int32 URshipSceneConverter::ConvertAllCameras(const FRshipConversionOptions& Options, TArray<FRshipConversionResult>& OutResults)
{
    int32 SuccessCount = 0;

    for (const FRshipDiscoveredCamera& Camera : DiscoveredCameras)
    {
        FRshipConversionResult Result = ConvertCamera(Camera, Options);
        OutResults.Add(Result);

        if (Result.bSuccess)
        {
            SuccessCount++;
        }
    }

    OnConversionComplete.Broadcast(SuccessCount, OutResults.Num() - SuccessCount);

    return SuccessCount;
}

int32 URshipSceneConverter::ConvertLightsByIndex(const TArray<int32>& Indices, const FRshipConversionOptions& Options, TArray<FRshipConversionResult>& OutResults)
{
    int32 SuccessCount = 0;

    // Make a mutable copy of options for address incrementing
    FRshipConversionOptions MutableOptions = Options;
    int32 CurrentAddress = Options.StartAddress;

    for (int32 Index : Indices)
    {
        if (!DiscoveredLights.IsValidIndex(Index))
        {
            FRshipConversionResult FailResult;
            FailResult.ErrorMessage = FString::Printf(TEXT("Invalid index: %d"), Index);
            OutResults.Add(FailResult);
            continue;
        }

        // Update address for this fixture
        MutableOptions.StartAddress = CurrentAddress;

        FRshipConversionResult Result = ConvertLight(DiscoveredLights[Index], MutableOptions);
        OutResults.Add(Result);

        if (Result.bSuccess)
        {
            SuccessCount++;
            CurrentAddress += Options.ChannelsPerFixture;
        }
    }

    OnConversionComplete.Broadcast(SuccessCount, OutResults.Num() - SuccessCount);

    return SuccessCount;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool URshipSceneConverter::ValidateBeforeConversion(bool bStopOnError)
{
    if (!Subsystem)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipSceneConverter: No subsystem for validation"));
        return false;
    }

    URshipSceneValidator* Validator = Subsystem->GetSceneValidator();
    if (!Validator)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipSceneConverter: No validator available"));
        return true;  // No validator = assume valid
    }

    // Collect all actors from discovered items
    TArray<AActor*> ActorsToValidate;
    for (const FRshipDiscoveredLight& Light : DiscoveredLights)
    {
        if (Light.OwnerActor)
        {
            ActorsToValidate.AddUnique(Light.OwnerActor);
        }
    }
    for (const FRshipDiscoveredCamera& Camera : DiscoveredCameras)
    {
        if (Camera.CameraActor)
        {
            ActorsToValidate.AddUnique(Camera.CameraActor);
        }
    }

    // Validate
    FRshipValidationResult Result = Validator->ValidateActors(ActorsToValidate);

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter: Validation complete - %d errors, %d warnings"),
        Result.ErrorCount, Result.WarningCount);

    return Result.ErrorCount == 0;
}

int32 URshipSceneConverter::ConvertAllLightsValidated(const FRshipConversionOptions& Options, TArray<FRshipConversionResult>& OutResults)
{
    if (!Subsystem)
    {
        return 0;
    }

    URshipSceneValidator* Validator = Subsystem->GetSceneValidator();

    TArray<int32> ValidIndices;
    for (int32 i = 0; i < DiscoveredLights.Num(); i++)
    {
        const FRshipDiscoveredLight& Light = DiscoveredLights[i];
        if (!Light.OwnerActor)
        {
            continue;
        }

        // Check if this actor has validation errors
        bool bHasErrors = false;
        if (Validator)
        {
            TArray<FRshipValidationIssue> Issues = Validator->ValidateActor(Light.OwnerActor);
            for (const FRshipValidationIssue& Issue : Issues)
            {
                if (Issue.Severity == ERshipValidationSeverity::Error ||
                    Issue.Severity == ERshipValidationSeverity::Critical)
                {
                    bHasErrors = true;
                    UE_LOG(LogRshipExec, Warning, TEXT("Skipping light %s due to validation error: %s"),
                        *Light.SuggestedName, *Issue.Message);
                    break;
                }
            }
        }

        if (!bHasErrors)
        {
            ValidIndices.Add(i);
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter: Converting %d/%d lights (passed validation)"),
        ValidIndices.Num(), DiscoveredLights.Num());

    return ConvertLightsByIndex(ValidIndices, Options, OutResults);
}

// ============================================================================
// POSITION SYNC
// ============================================================================

int32 URshipSceneConverter::SyncAllPositionsToServer(float PositionScale)
{
    int32 SyncCount = 0;

    for (auto& Pair : ConvertedActors)
    {
        if (Pair.Key.IsValid())
        {
            if (SyncActorPositionToServer(Pair.Key.Get(), Pair.Value, PositionScale))
            {
                SyncCount++;
            }
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter: Synced %d positions to server"), SyncCount);

    return SyncCount;
}

bool URshipSceneConverter::SyncActorPositionToServer(AActor* Actor, const FString& EntityId, float PositionScale)
{
    if (!Actor || EntityId.IsEmpty())
    {
        return false;
    }

    FVector Position = Actor->GetActorLocation() * PositionScale;
    FRotator Rotation = Actor->GetActorRotation();

    // Determine if this is a fixture or camera based on manager lookups
    if (FixtureManager)
    {
        FRshipFixtureInfo FixtureInfo;
        if (FixtureManager->GetFixtureById(EntityId, FixtureInfo))
        {
            // Update fixture position
            FixtureInfo.Position = Position;
            FixtureInfo.Rotation = Rotation;
            return FixtureManager->UpdateFixturePosition(EntityId, Position, Rotation);
        }
    }

    if (CameraManager)
    {
        FRshipCameraInfo CameraInfo;
        if (CameraManager->GetCameraById(EntityId, CameraInfo))
        {
            // Update camera position
            CameraInfo.Position = Position;
            CameraInfo.Rotation = Rotation;
            return CameraManager->UpdateCameraPosition(EntityId, Position, Rotation);
        }
    }

    return false;
}

// ============================================================================
// UTILITY
// ============================================================================

FString URshipSceneConverter::GetOrCreateGenericFixtureType(const FString& LightType)
{
    // Check cache
    if (FString* CachedId = GenericFixtureTypes.Find(LightType))
    {
        return *CachedId;
    }

    // Create a generic fixture type for this UE light type
    FString TypeId = FString::Printf(TEXT("ue-generic-%s"), *LightType.ToLower());

    // TODO: Actually create the fixture type on the server
    // For now, we just use a well-known ID pattern
    // The server should have these pre-defined or auto-create them

    GenericFixtureTypes.Add(LightType, TypeId);

    UE_LOG(LogRshipExec, Log, TEXT("RshipSceneConverter: Using generic fixture type '%s' for UE %s lights"),
        *TypeId, *LightType);

    return TypeId;
}

FString URshipSceneConverter::GetConvertedEntityId(AActor* Actor) const
{
    if (!Actor)
    {
        return FString();
    }

    if (const FString* FoundId = ConvertedActors.Find(Actor))
    {
        return *FoundId;
    }

    return FString();
}

FString URshipSceneConverter::GenerateFixtureName(AActor* Actor, const FString& Prefix)
{
    if (!Actor)
    {
        return TEXT("Unknown");
    }

    // Try to use actor label first (user-friendly name in editor)
    FString Name = FString(Actor->GetActorNameOrLabel());

    if (Name.IsEmpty())
    {
        Name = Actor->GetName();
    }

    // Clean up common UE naming patterns
    Name = Name.Replace(TEXT("_C"), TEXT(""));
    Name = Name.Replace(TEXT("BP_"), TEXT(""));

    if (!Prefix.IsEmpty())
    {
        Name = Prefix + Name;
    }

    return Name;
}

FRshipFixtureInfo URshipSceneConverter::CreateFixtureInfoFromLight(const FRshipDiscoveredLight& Light, const FRshipConversionOptions& Options, int32 Index)
{
    FRshipFixtureInfo Info;

    // Generate unique ID
    Info.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

    // Name
    Info.Name = Options.NamePrefix + Light.SuggestedName;

    // Position (convert from UE units to meters)
    Info.Position = Light.Position * Options.PositionScale;
    Info.Rotation = Light.Rotation;

    // DMX addressing
    Info.Universe = Options.Universe;
    Info.Address = Options.StartAddress + (Index * Options.ChannelsPerFixture);

    return Info;
}

FRshipCameraInfo URshipSceneConverter::CreateCameraInfoFromDiscovered(const FRshipDiscoveredCamera& Camera, const FRshipConversionOptions& Options)
{
    FRshipCameraInfo Info;

    // Generate unique ID
    Info.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

    // Name
    Info.Name = Options.NamePrefix + Camera.SuggestedName;

    // Position (convert from UE units to meters)
    Info.Position = Camera.Position * Options.PositionScale;
    Info.Rotation = Camera.Rotation;

    // Camera properties - stored in calibration
    Info.Calibration.FOV = Camera.FOV;

    return Info;
}
