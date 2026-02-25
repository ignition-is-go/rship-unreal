// Rship Fixture Visualizer Implementation

#include "RshipFixtureVisualizer.h"
#include "RshipSubsystem.h"
#include "RshipPulseApplicator.h"
#include "RshipFixtureManager.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif

// ============================================================================
// FIXTURE VISUALIZER COMPONENT
// ============================================================================

URshipFixtureVisualizer::URshipFixtureVisualizer()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.033f;  // ~30Hz update
}

void URshipFixtureVisualizer::BeginPlay()
{
    Super::BeginPlay();

    // Get subsystem
    if (GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    }

    // Find linked applicator if not set
    if (!LinkedApplicator)
    {
        FindLinkedApplicator();
    }

    // Subscribe to pulse events
    if (Subsystem && !FixtureId.IsEmpty())
    {
        URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
        if (PulseReceiver)
        {
            PulseReceiver->OnFixturePulseReceived.AddDynamic(this, &URshipFixtureVisualizer::OnPulseReceived);
        }
    }

    // Initialize visualization
    InitializeVisualization();

    UE_LOG(LogRshipExec, Log, TEXT("FixtureVisualizer: Initialized for fixture %s"), *FixtureId);
}

void URshipFixtureVisualizer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unsubscribe from pulses
    if (Subsystem)
    {
        URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
        if (PulseReceiver)
        {
            PulseReceiver->OnFixturePulseReceived.RemoveDynamic(this, &URshipFixtureVisualizer::OnPulseReceived);
        }
    }

    // Cleanup visualization components
    if (BeamMesh)
    {
        BeamMesh->DestroyComponent();
        BeamMesh = nullptr;
    }
    if (InnerBeamMesh)
    {
        InnerBeamMesh->DestroyComponent();
        InnerBeamMesh = nullptr;
    }
    if (SymbolMesh)
    {
        SymbolMesh->DestroyComponent();
        SymbolMesh = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void URshipFixtureVisualizer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Check visibility conditions
    bool bShouldShow = false;
    if (GWorld && GWorld->IsPlayInEditor())
    {
        bShouldShow = bShowAtRuntime;
    }
    else
    {
        bShouldShow = bShowInEditor;
    }

    if (!bShouldShow || Mode == ERshipVisualizationMode::None)
    {
        SetVisualizationVisible(false);
        return;
    }

    // Update LOD based on camera distance
    UpdateLOD();

    // Check if we should be culled based on LOD level
    if (CurrentLODLevel >= 3)
    {
        SetVisualizationVisible(false);
        return;
    }

    SetVisualizationVisible(true);

    // Update from linked applicator if available
    if (LinkedApplicator && !bManualIntensity)
    {
        CurrentIntensity = LinkedApplicator->GetCurrentIntensity() / LinkedApplicator->MaxIntensity;
        CurrentColor = LinkedApplicator->GetCurrentColor();
    }

    // Apply color temperature if enabled
    if (bUseColorTemperature && !bManualColorTemperature)
    {
        CurrentColor = FRshipColorTemperature::KelvinToRGB(CurrentColorTemperature);
    }

    // Rebuild geometry if needed
    if (bNeedsRebuild)
    {
        UpdateBeamGeometry();
        bNeedsRebuild = false;
    }

    // Update material parameters every tick
    UpdateMaterialParameters();
    UpdateGoboTexture();
    UpdateIESVisualization();
    UpdateSymbol();
}

#if WITH_EDITOR
void URshipFixtureVisualizer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    // Trigger rebuild for relevant property changes
    if (PropertyName == GET_MEMBER_NAME_CHECKED(URshipFixtureVisualizer, Mode) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(FRshipBeamSettings, Quality) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(FRshipBeamSettings, BeamLength))
    {
        bNeedsRebuild = true;
    }

    // Update materials immediately for color/opacity changes
    if (PropertyName == GET_MEMBER_NAME_CHECKED(FRshipBeamSettings, BeamOpacity) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(FRshipSymbolSettings, OffColor))
    {
        UpdateMaterialParameters();
    }
}
#endif

void URshipFixtureVisualizer::InitializeVisualization()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    CreateMaterials();
    CreateBeamMesh();
    CreateSymbolMesh();

    bNeedsRebuild = true;
}

void URshipFixtureVisualizer::CreateBeamMesh()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Create outer beam cone
    if (!BeamMesh)
    {
        BeamMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("BeamMesh"));
        BeamMesh->SetupAttachment(Owner->GetRootComponent());
        BeamMesh->RegisterComponent();
        BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BeamMesh->SetCastShadow(false);
        BeamMesh->bUseAsyncCooking = true;
    }

    // Create inner beam cone (brighter core)
    if (!InnerBeamMesh)
    {
        InnerBeamMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("InnerBeamMesh"));
        InnerBeamMesh->SetupAttachment(Owner->GetRootComponent());
        InnerBeamMesh->RegisterComponent();
        InnerBeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        InnerBeamMesh->SetCastShadow(false);
        InnerBeamMesh->bUseAsyncCooking = true;
    }
}

void URshipFixtureVisualizer::CreateSymbolMesh()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    if (!SymbolMesh)
    {
        SymbolMesh = NewObject<UStaticMeshComponent>(Owner, TEXT("SymbolMesh"));
        SymbolMesh->SetupAttachment(Owner->GetRootComponent());
        SymbolMesh->RegisterComponent();
        SymbolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SymbolMesh->SetCastShadow(false);

        // Use a simple sphere as the default symbol
        static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
        if (SphereMesh.Succeeded())
        {
            SymbolMesh->SetStaticMesh(SphereMesh.Object);
        }

        // Scale to symbol size
        float Scale = SymbolSettings.SymbolSize / 100.0f;  // Default sphere is 100 units
        SymbolMesh->SetWorldScale3D(FVector(Scale));
    }
}

void URshipFixtureVisualizer::CreateMaterials()
{
    // Create beam material - translucent, additive, unlit
    UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial"));
    if (BaseMaterial)
    {
        BeamMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        InnerBeamMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        SymbolMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
    }
    else
    {
        // Fallback - create simple translucent material
        UMaterial* FallbackMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
        if (FallbackMaterial)
        {
            BeamMaterial = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
            InnerBeamMaterial = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
            SymbolMaterial = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
        }
    }

    // Apply materials
    if (BeamMesh && BeamMaterial)
    {
        BeamMesh->SetMaterial(0, BeamMaterial);
    }
    if (InnerBeamMesh && InnerBeamMaterial)
    {
        InnerBeamMesh->SetMaterial(0, InnerBeamMaterial);
    }
    if (SymbolMesh && SymbolMaterial)
    {
        SymbolMesh->SetMaterial(0, SymbolMaterial);
    }
}

void URshipFixtureVisualizer::OnPulseReceived(const FString& InFixtureId, const FRshipFixturePulse& Pulse)
{
    if (InFixtureId != FixtureId)
    {
        return;
    }

    UpdateFromPulse(Pulse);
}

void URshipFixtureVisualizer::UpdateFromPulse(const FRshipFixturePulse& Pulse)
{
    // Update intensity
    if (Pulse.bHasIntensity && !bManualIntensity)
    {
        CurrentIntensity = Pulse.Intensity;
    }

    // Update color
    if (Pulse.bHasColor && !bManualColor)
    {
        CurrentColor = Pulse.Color;
    }

    // Update beam angle (zoom)
    if (Pulse.bHasZoom && !bManualAngle)
    {
        // Map zoom 0-1 to typical beam range (10-60 degrees)
        CurrentOuterAngle = FMath::Lerp(10.0f, 60.0f, Pulse.Zoom);
        CurrentInnerAngle = CurrentOuterAngle * 0.7f;
        bNeedsRebuild = true;
    }

    // Update pan/tilt
    if ((Pulse.bHasPan || Pulse.bHasTilt) && !bManualPanTilt)
    {
        if (Pulse.bHasPan)
        {
            CurrentPan = Pulse.Pan;
        }
        if (Pulse.bHasTilt)
        {
            CurrentTilt = Pulse.Tilt;
        }
    }

    // Update gobo
    if (Pulse.bHasGobo && !bManualGobo)
    {
        CurrentGobo = FMath::RoundToInt(Pulse.Gobo);
        if (Pulse.bHasGoboRotation)
        {
            CurrentGoboRotation = Pulse.GoboRotation;
        }
    }
}

void URshipFixtureVisualizer::UpdateBeamGeometry()
{
    if (Mode == ERshipVisualizationMode::None || Mode == ERshipVisualizationMode::Symbol)
    {
        // Hide beam meshes
        if (BeamMesh) BeamMesh->SetVisibility(false);
        if (InnerBeamMesh) InnerBeamMesh->SetVisibility(false);
        return;
    }

    if (!BeamMesh || !InnerBeamMesh)
    {
        return;
    }

    // Use LOD-adjusted segment count for performance
    int32 Segments = GetSegmentCountForLOD(CurrentLODLevel);

    // Generate outer cone
    TArray<FVector> OuterVerts, OuterNormals;
    TArray<int32> OuterTris;
    TArray<FVector2D> OuterUVs;
    GenerateConeMesh(OuterVerts, OuterTris, OuterNormals, OuterUVs,
                     CurrentOuterAngle, BeamSettings.BeamLength, Segments, false);

    BeamMesh->CreateMeshSection(0, OuterVerts, OuterTris, OuterNormals, OuterUVs,
                                TArray<FColor>(), TArray<FProcMeshTangent>(), false);
    BeamMesh->SetVisibility(true);

    // Generate inner cone
    TArray<FVector> InnerVerts, InnerNormals;
    TArray<int32> InnerTris;
    TArray<FVector2D> InnerUVs;
    GenerateConeMesh(InnerVerts, InnerTris, InnerNormals, InnerUVs,
                     CurrentInnerAngle, BeamSettings.BeamLength * 0.95f, Segments, true);

    InnerBeamMesh->CreateMeshSection(0, InnerVerts, InnerTris, InnerNormals, InnerUVs,
                                     TArray<FColor>(), TArray<FProcMeshTangent>(), false);
    InnerBeamMesh->SetVisibility(true);
}

void URshipFixtureVisualizer::GenerateConeMesh(TArray<FVector>& Vertices, TArray<int32>& Triangles,
                                                TArray<FVector>& Normals, TArray<FVector2D>& UVs,
                                                float Angle, float Length, int32 Segments, bool bInnerCone)
{
    Vertices.Empty();
    Triangles.Empty();
    Normals.Empty();
    UVs.Empty();

    float RadiusAtEnd = Length * FMath::Tan(FMath::DegreesToRadians(Angle / 2.0f));

    // Apex vertex (at origin)
    Vertices.Add(FVector::ZeroVector);
    Normals.Add(FVector(0, 0, -1));
    UVs.Add(FVector2D(0.5f, 0.0f));

    // Generate ring vertices at the end of the cone
    for (int32 i = 0; i <= Segments; i++)
    {
        float AngleRad = (float)i / (float)Segments * 2.0f * PI;
        float X = RadiusAtEnd * FMath::Cos(AngleRad);
        float Y = RadiusAtEnd * FMath::Sin(AngleRad);

        // Point down the -Z axis (typical light direction)
        Vertices.Add(FVector(X, Y, -Length));

        FVector Normal = FVector(X, Y, RadiusAtEnd).GetSafeNormal();
        Normals.Add(Normal);

        UVs.Add(FVector2D((float)i / (float)Segments, 1.0f));
    }

    // Generate triangles from apex to ring
    for (int32 i = 0; i < Segments; i++)
    {
        // Triangle from apex (0) to two adjacent ring vertices
        Triangles.Add(0);
        Triangles.Add(i + 1);
        Triangles.Add(i + 2);
    }

    // Generate end cap (optional, for solid look)
    if (!bInnerCone)
    {
        int32 CenterIndex = Vertices.Num();
        Vertices.Add(FVector(0, 0, -Length));
        Normals.Add(FVector(0, 0, -1));
        UVs.Add(FVector2D(0.5f, 1.0f));

        for (int32 i = 0; i < Segments; i++)
        {
            Triangles.Add(CenterIndex);
            Triangles.Add(i + 2);
            Triangles.Add(i + 1);
        }
    }
}

void URshipFixtureVisualizer::UpdateMaterialParameters()
{
    // Calculate effective opacity
    float EffectiveOpacity = BeamSettings.BeamOpacity;
    if (BeamSettings.bScaleOpacityWithIntensity)
    {
        EffectiveOpacity *= CurrentIntensity;
    }

    // Calculate emissive color
    FLinearColor EmissiveColor = CurrentColor * CurrentIntensity;

    // Update outer beam material
    if (BeamMaterial)
    {
        BeamMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor * EffectiveOpacity);
    }

    // Update inner beam material (brighter)
    if (InnerBeamMaterial)
    {
        float InnerOpacity = EffectiveOpacity * BeamSettings.InnerConeMultiplier;
        InnerBeamMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor * InnerOpacity);
    }

    // Update symbol material
    if (SymbolMaterial)
    {
        FLinearColor SymbolColor = CurrentIntensity > 0.01f ? CurrentColor : SymbolSettings.OffColor;
        SymbolMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), SymbolColor);
    }
}

void URshipFixtureVisualizer::UpdateSymbol()
{
    if (Mode == ERshipVisualizationMode::None || Mode == ERshipVisualizationMode::BeamCone ||
        Mode == ERshipVisualizationMode::BeamVolume)
    {
        if (SymbolMesh) SymbolMesh->SetVisibility(false);
        return;
    }

    if (!SymbolMesh)
    {
        return;
    }

    SymbolMesh->SetVisibility(true);

    // Update scale based on settings
    float Scale = SymbolSettings.SymbolSize / 100.0f;
    SymbolMesh->SetWorldScale3D(FVector(Scale));

    // Billboard mode
    if (SymbolSettings.bBillboard && GEngine && GEngine->GetFirstLocalPlayerController(GetWorld()))
    {
        APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
        if (PC && PC->PlayerCameraManager)
        {
            FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
            FVector ToCamera = CameraLocation - SymbolMesh->GetComponentLocation();
            FRotator LookAt = ToCamera.Rotation();
            SymbolMesh->SetWorldRotation(LookAt);
        }
    }
}

int32 URshipFixtureVisualizer::GetSegmentCount() const
{
    switch (BeamSettings.Quality)
    {
        case ERshipBeamQuality::Low:    return 16;
        case ERshipBeamQuality::Medium: return 32;
        case ERshipBeamQuality::High:   return 64;
        case ERshipBeamQuality::Ultra:  return 128;
        default:                        return 32;
    }
}

void URshipFixtureVisualizer::FindLinkedApplicator()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Look for a pulse applicator on the same actor
    LinkedApplicator = Owner->FindComponentByClass<URshipPulseApplicator>();

    if (LinkedApplicator)
    {
        // Sync fixture ID if not set
        if (FixtureId.IsEmpty() && !LinkedApplicator->FixtureId.IsEmpty())
        {
            FixtureId = LinkedApplicator->FixtureId;
        }

        UE_LOG(LogRshipExec, Log, TEXT("FixtureVisualizer: Linked to applicator for fixture %s"), *FixtureId);
    }
}

// ========================================================================
// MANUAL STATE CONTROL
// ========================================================================

void URshipFixtureVisualizer::SetIntensity(float Intensity)
{
    CurrentIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    bManualIntensity = true;
}

void URshipFixtureVisualizer::SetColor(FLinearColor Color)
{
    CurrentColor = Color;
    bManualColor = true;
}

void URshipFixtureVisualizer::SetBeamAngle(float OuterAngle, float InnerAngle)
{
    CurrentOuterAngle = FMath::Clamp(OuterAngle, 1.0f, 180.0f);
    CurrentInnerAngle = InnerAngle > 0 ? FMath::Clamp(InnerAngle, 1.0f, OuterAngle) : OuterAngle * 0.7f;
    bManualAngle = true;
    bNeedsRebuild = true;
}

void URshipFixtureVisualizer::SetPanTilt(float Pan, float Tilt)
{
    CurrentPan = Pan;
    CurrentTilt = Tilt;
    bManualPanTilt = true;
}

void URshipFixtureVisualizer::SetGobo(int32 GoboIndex, float Rotation)
{
    CurrentGobo = GoboIndex;
    CurrentGoboRotation = Rotation;
    bManualGobo = true;
}

void URshipFixtureVisualizer::ResetToAutomatic()
{
    bManualIntensity = false;
    bManualColor = false;
    bManualColorTemperature = false;
    bManualAngle = false;
    bManualPanTilt = false;
    bManualGobo = false;
}

void URshipFixtureVisualizer::GetBeamAngles(float& OuterAngle, float& InnerAngle) const
{
    OuterAngle = CurrentOuterAngle;
    InnerAngle = CurrentInnerAngle;
}

void URshipFixtureVisualizer::RebuildVisualization()
{
    bNeedsRebuild = true;
}

void URshipFixtureVisualizer::SetVisualizationVisible(bool bVisible)
{
    if (bIsVisible == bVisible)
    {
        return;
    }

    bIsVisible = bVisible;

    if (BeamMesh) BeamMesh->SetVisibility(bVisible);
    if (InnerBeamMesh) InnerBeamMesh->SetVisibility(bVisible);
    if (SymbolMesh) SymbolMesh->SetVisibility(bVisible);
}

void URshipFixtureVisualizer::SetColorTemperature(float Kelvin)
{
    CurrentColorTemperature = FMath::Clamp(Kelvin, 1000.0f, 40000.0f);
    CurrentColor = FRshipColorTemperature::KelvinToRGB(CurrentColorTemperature);
    bManualColorTemperature = true;
    bManualColor = true;
}

int32 URshipFixtureVisualizer::GetSegmentCountForLOD(int32 LODLevel) const
{
    int32 BaseSegments = GetSegmentCount();

    switch (LODLevel)
    {
        case 0: return BaseSegments;           // Full quality
        case 1: return FMath::Max(8, BaseSegments / 2);   // Half segments
        case 2: return FMath::Max(6, BaseSegments / 4);   // Quarter segments
        default: return 6;                     // Minimum
    }
}

void URshipFixtureVisualizer::UpdateLOD()
{
    if (LODSettings.Mode == ERshipLODMode::Off)
    {
        CurrentLODLevel = 0;
        return;
    }

    if (LODSettings.Mode == ERshipLODMode::Forced)
    {
        CurrentLODLevel = LODSettings.ForcedLODLevel;
        return;
    }

    // Auto LOD based on distance
    CachedCameraDistance = CalculateCameraDistance();

    int32 PreviousLOD = CurrentLODLevel;

    if (CachedCameraDistance > LODSettings.CullDistance)
    {
        CurrentLODLevel = 3;  // Culled
    }
    else if (CachedCameraDistance > LODSettings.LOD2Distance)
    {
        CurrentLODLevel = 2;
    }
    else if (CachedCameraDistance > LODSettings.LOD1Distance)
    {
        CurrentLODLevel = 1;
    }
    else
    {
        CurrentLODLevel = 0;
    }

    // Trigger rebuild if LOD changed
    if (PreviousLOD != CurrentLODLevel)
    {
        bNeedsRebuild = true;
    }
}

float URshipFixtureVisualizer::CalculateCameraDistance() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return 0.0f;
    }

    FVector VisualizerLocation = Owner->GetActorLocation();

#if WITH_EDITOR
    // In editor, use the editor viewport camera
    if (!GWorld || !GWorld->IsPlayInEditor())
    {
        if (GEditor && GEditor->GetActiveViewport())
        {
            FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
            if (ViewportClient)
            {
                FVector CameraLocation = ViewportClient->GetViewLocation();
                return FVector::Dist(VisualizerLocation, CameraLocation);
            }
        }
    }
#endif

    // At runtime, use player camera
    if (GEngine && GetWorld())
    {
        APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
        if (PC && PC->PlayerCameraManager)
        {
            FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
            return FVector::Dist(VisualizerLocation, CameraLocation);
        }
    }

    return 0.0f;
}

void URshipFixtureVisualizer::UpdateGoboTexture()
{
    if (!GoboSettings.bEnableGobo || !BeamMaterial)
    {
        return;
    }

    // Select gobo texture based on current index
    UTexture2D* GoboTexture = nullptr;
    if (CurrentGobo > 0 && CurrentGobo <= GoboSettings.GoboTextures.Num())
    {
        GoboTexture = GoboSettings.GoboTextures[CurrentGobo - 1];
    }

    if (GoboTexture)
    {
        BeamMaterial->SetTextureParameterValue(TEXT("GoboTexture"), GoboTexture);
        BeamMaterial->SetScalarParameterValue(TEXT("GoboRotation"), CurrentGoboRotation * GoboSettings.RotationSpeedMultiplier);
        BeamMaterial->SetScalarParameterValue(TEXT("GoboSharpness"), GoboSettings.ProjectionSharpness);
        BeamMaterial->SetScalarParameterValue(TEXT("GoboEnabled"), 1.0f);
    }
    else
    {
        BeamMaterial->SetScalarParameterValue(TEXT("GoboEnabled"), 0.0f);
    }
}

void URshipFixtureVisualizer::UpdateIESVisualization()
{
    if (!IESSettings.bEnableIES || !BeamMaterial)
    {
        return;
    }

    if (IESSettings.IESTexture)
    {
        BeamMaterial->SetTextureParameterValue(TEXT("IESProfile"), IESSettings.IESTexture);
        BeamMaterial->SetScalarParameterValue(TEXT("IESIntensity"), IESSettings.IESIntensityMultiplier);
        BeamMaterial->SetScalarParameterValue(TEXT("IESEnabled"), 1.0f);
    }
    else
    {
        BeamMaterial->SetScalarParameterValue(TEXT("IESEnabled"), 0.0f);
    }
}

// ============================================================================
// COLOR TEMPERATURE UTILITIES
// ============================================================================

FLinearColor FRshipColorTemperature::KelvinToRGB(float Kelvin)
{
    // Tanner Helland's algorithm for accurate color temperature
    // Based on black body radiation approximation
    Kelvin = FMath::Clamp(Kelvin, 1000.0f, 40000.0f);
    float Temp = Kelvin / 100.0f;

    float Red, Green, Blue;

    // Calculate Red
    if (Temp <= 66.0f)
    {
        Red = 255.0f;
    }
    else
    {
        Red = Temp - 60.0f;
        Red = 329.698727446f * FMath::Pow(Red, -0.1332047592f);
        Red = FMath::Clamp(Red, 0.0f, 255.0f);
    }

    // Calculate Green
    if (Temp <= 66.0f)
    {
        Green = Temp;
        Green = 99.4708025861f * FMath::Loge(Green) - 161.1195681661f;
        Green = FMath::Clamp(Green, 0.0f, 255.0f);
    }
    else
    {
        Green = Temp - 60.0f;
        Green = 288.1221695283f * FMath::Pow(Green, -0.0755148492f);
        Green = FMath::Clamp(Green, 0.0f, 255.0f);
    }

    // Calculate Blue
    if (Temp >= 66.0f)
    {
        Blue = 255.0f;
    }
    else if (Temp <= 19.0f)
    {
        Blue = 0.0f;
    }
    else
    {
        Blue = Temp - 10.0f;
        Blue = 138.5177312231f * FMath::Loge(Blue) - 305.0447927307f;
        Blue = FMath::Clamp(Blue, 0.0f, 255.0f);
    }

    // Convert from 0-255 to 0-1 linear
    return FLinearColor(Red / 255.0f, Green / 255.0f, Blue / 255.0f, 1.0f);
}

float FRshipColorTemperature::RGBToKelvin(const FLinearColor& Color)
{
    // Approximate inverse - not perfectly accurate but useful for estimation
    // Based on McCamy's formula for correlated color temperature
    float X = Color.R;
    float Y = Color.G;
    float Z = Color.B;

    // Simplified chromaticity calculation
    float Sum = X + Y + Z;
    if (Sum < 0.001f)
    {
        return 6500.0f; // Default to daylight
    }

    float x = X / Sum;
    float y = Y / Sum;

    // McCamy's CCT formula (approximation)
    float n = (x - 0.3320f) / (0.1858f - y);
    float CCT = 449.0f * FMath::Pow(n, 3.0f) + 3525.0f * FMath::Pow(n, 2.0f) + 6823.3f * n + 5520.33f;

    return FMath::Clamp(CCT, 1000.0f, 40000.0f);
}

// ============================================================================
// VISUALIZATION MANAGER
// ============================================================================

void URshipVisualizationManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogRshipExec, Log, TEXT("VisualizationManager initialized"));
}

void URshipVisualizationManager::Shutdown()
{
    RegisteredVisualizers.Empty();
    Subsystem = nullptr;
    UE_LOG(LogRshipExec, Log, TEXT("VisualizationManager shutdown"));
}

void URshipVisualizationManager::Tick(float DeltaTime)
{
    // Tick all registered visualizers for smooth animation
    for (URshipFixtureVisualizer* Visualizer : RegisteredVisualizers)
    {
        if (Visualizer && Visualizer->IsValidLowLevel())
        {
            // Visualizers have their own component tick, but this allows
            // centralized animation control for synchronized effects
        }
    }
}

void URshipVisualizationManager::RegisterVisualizer(URshipFixtureVisualizer* Visualizer)
{
    if (Visualizer && !RegisteredVisualizers.Contains(Visualizer))
    {
        RegisteredVisualizers.Add(Visualizer);
    }
}

void URshipVisualizationManager::UnregisterVisualizer(URshipFixtureVisualizer* Visualizer)
{
    RegisteredVisualizers.Remove(Visualizer);
}

TArray<URshipFixtureVisualizer*> URshipVisualizationManager::GetAllVisualizers() const
{
    return RegisteredVisualizers;
}

URshipFixtureVisualizer* URshipVisualizationManager::GetVisualizerForFixture(const FString& FixtureId) const
{
    for (URshipFixtureVisualizer* Viz : RegisteredVisualizers)
    {
        if (Viz && Viz->FixtureId == FixtureId)
        {
            return Viz;
        }
    }
    return nullptr;
}

void URshipVisualizationManager::SetGlobalMode(ERshipVisualizationMode Mode)
{
    GlobalMode = Mode;
    for (URshipFixtureVisualizer* Viz : RegisteredVisualizers)
    {
        if (Viz)
        {
            Viz->Mode = Mode;
        }
    }
}

void URshipVisualizationManager::SetGlobalVisibility(bool bVisible)
{
    bGlobalVisibility = bVisible;
    for (URshipFixtureVisualizer* Viz : RegisteredVisualizers)
    {
        if (Viz)
        {
            Viz->SetVisualizationVisible(bVisible);
        }
    }
}

void URshipVisualizationManager::SetGlobalBeamOpacity(float Opacity)
{
    GlobalBeamOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
    for (URshipFixtureVisualizer* Viz : RegisteredVisualizers)
    {
        if (Viz)
        {
            Viz->BeamSettings.BeamOpacity = GlobalBeamOpacity;
        }
    }
}

void URshipVisualizationManager::SetGlobalBeamLength(float Length)
{
    GlobalBeamLength = FMath::Max(10.0f, Length);
    for (URshipFixtureVisualizer* Viz : RegisteredVisualizers)
    {
        if (Viz)
        {
            Viz->BeamSettings.BeamLength = GlobalBeamLength;
            Viz->RebuildVisualization();
        }
    }
}

void URshipVisualizationManager::ApplyProgrammingPreset()
{
    SetGlobalMode(ERshipVisualizationMode::Full);
    SetGlobalBeamOpacity(0.3f);
    SetGlobalBeamLength(1500.0f);
    SetGlobalVisibility(true);

    UE_LOG(LogRshipExec, Log, TEXT("VisualizationManager: Applied Programming preset"));
}

void URshipVisualizationManager::ApplyPreviewPreset()
{
    SetGlobalMode(ERshipVisualizationMode::BeamCone);
    SetGlobalBeamOpacity(0.15f);
    SetGlobalBeamLength(1000.0f);
    SetGlobalVisibility(true);

    UE_LOG(LogRshipExec, Log, TEXT("VisualizationManager: Applied Preview preset"));
}

void URshipVisualizationManager::ApplyShowPreset()
{
    SetGlobalMode(ERshipVisualizationMode::Symbol);
    SetGlobalBeamOpacity(0.05f);
    SetGlobalBeamLength(500.0f);
    SetGlobalVisibility(true);

    UE_LOG(LogRshipExec, Log, TEXT("VisualizationManager: Applied Show preset"));
}

void URshipVisualizationManager::ApplyOffPreset()
{
    SetGlobalVisibility(false);

    UE_LOG(LogRshipExec, Log, TEXT("VisualizationManager: Applied Off preset"));
}
