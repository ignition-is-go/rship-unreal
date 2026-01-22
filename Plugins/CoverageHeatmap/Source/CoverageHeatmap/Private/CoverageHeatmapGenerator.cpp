#include "CoverageHeatmapGenerator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "CineCameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Exporters/GLTFExporter.h"
#include "Options/GLTFExportOptions.h"
#include "UserData/GLTFMaterialUserData.h"
#endif

ACoverageHeatmapGenerator::ACoverageHeatmapGenerator()
{
    PrimaryActorTick.bCanEverTick = false;

    // Default camera tags
    CameraTags.Add(TEXT("TrackingRig1"));
    CameraTags.Add(TEXT("TrackingRig2"));
    CameraTags.Add(TEXT("TrackingRig3"));

    // Default scene export folder
    SceneExportFolders.Add(TEXT("Scene"));

    HeatmapPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeatmapPlane"));
    RootComponent = HeatmapPlane;
    HeatmapPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeatmapPlane->bDisallowNanite = true; // Nanite doesn't support translucent materials

    // Set a clean default material
    static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if (DefaultMat.Succeeded())
    {
        HeatmapPlane->SetMaterial(0, DefaultMat.Object);
    }
}

bool ACoverageHeatmapGenerator::CalculateFloorBounds(FVector& OutMin, FVector& OutMax, float& OutFloorZ)
{
    TArray<AActor*> FloorActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FloorTag, FloorActors);

    if (FloorActors.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Heatmap: No actors with tag '%s' found"), *FloorTag.ToString());
        return false;
    }

    OutMin = FVector(MAX_FLT, MAX_FLT, MAX_FLT);
    OutMax = FVector(-MAX_FLT, -MAX_FLT, -MAX_FLT);
    OutFloorZ = 0.0f;

    for (AActor* Actor : FloorActors)
    {
        FVector Origin, Extent;
        Actor->GetActorBounds(false, Origin, Extent);

        OutMin.X = FMath::Min(OutMin.X, Origin.X - Extent.X);
        OutMin.Y = FMath::Min(OutMin.Y, Origin.Y - Extent.Y);
        OutMax.X = FMath::Max(OutMax.X, Origin.X + Extent.X);
        OutMax.Y = FMath::Max(OutMax.Y, Origin.Y + Extent.Y);
        OutFloorZ = FMath::Max(OutFloorZ, Origin.Z + Extent.Z);
    }

    return true;
}

bool ACoverageHeatmapGenerator::GatherCameras(TArray<FCameraInfo>& OutCameras, int32& OutExcludedCount)
{
    OutExcludedCount = 0;
    TArray<AActor*> CameraActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), HeatmapCameraTag, CameraActors);

    for (AActor* Actor : CameraActors)
    {
        // Skip cameras with exclude tag
        if (CameraExcludeTag != NAME_None && Actor->ActorHasTag(CameraExcludeTag))
        {
            OutExcludedCount++;
            continue;
        }

        ACineCameraActor* CamActor = Cast<ACineCameraActor>(Actor);
        if (!CamActor) continue;

        UCineCameraComponent* CineComp = CamActor->GetCineCameraComponent();
        if (!CineComp) continue;

        FCameraInfo Info;
        Info.Location = CamActor->GetActorLocation();
        Info.Forward = CamActor->GetActorForwardVector();
        Info.Right = CamActor->GetActorRightVector();
        Info.Up = CamActor->GetActorUpVector();

        // Use CineCameraComponent's built-in FOV methods
        Info.HalfFOVH = FMath::DegreesToRadians(CineComp->GetHorizontalFieldOfView() * 0.5f);
        Info.HalfFOVV = FMath::DegreesToRadians(CineComp->GetVerticalFieldOfView() * 0.5f);

        OutCameras.Add(Info);
    }

    if (OutCameras.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Heatmap: No cameras found!"));
        return false;
    }

    return true;
}

int32 ACoverageHeatmapGenerator::CalculateCoverageAtPoint(const FVector& WorldPos, const TArray<FCameraInfo>& Cameras, const TArray<AActor*>& OccluderActors)
{
    int32 VisibleCount = 0;

    for (const FCameraInfo& Cam : Cameras)
    {
        FVector ToPoint = WorldPos - Cam.Location;
        float Dist = ToPoint.Size();

        // Distance check
        if (Dist > MaxDistance)
        {
            continue;
        }

        // Project onto camera axes
        float ForwardDist = FVector::DotProduct(ToPoint, Cam.Forward);

        // Must be in front of camera
        if (ForwardDist <= 0.0f)
            continue;

        float RightDist = FVector::DotProduct(ToPoint, Cam.Right);
        float UpDist = FVector::DotProduct(ToPoint, Cam.Up);

        // FOV check
        float TanHalfH = FMath::Tan(Cam.HalfFOVH);
        float TanHalfV = FMath::Tan(Cam.HalfFOVV);

        if (FMath::Abs(RightDist) > ForwardDist * TanHalfH)
            continue;

        if (FMath::Abs(UpDist) > ForwardDist * TanHalfV)
            continue;

        // Pixel size check - is the person large enough in frame to be tracked?
        if (MinPixelHeight > 0 && SensorResolutionY > 0)
        {
            float ViewHeightAtDist = 2.0f * ForwardDist * TanHalfV;
            float PersonPixelHeight = (AssumedPersonHeight / ViewHeightAtDist) * SensorResolutionY;
            if (PersonPixelHeight < MinPixelHeight)
                continue;
        }

        // Occlusion check using pre-gathered occluder actors
        if (bCheckOcclusion && OccluderActors.Num() > 0)
        {
            FCollisionQueryParams OccluderTraceParams;
            OccluderTraceParams.bTraceComplex = bTraceComplex;
            bool bBlocked = false;
            for (AActor* Occluder : OccluderActors)
            {
                FHitResult Hit;
                if (Occluder->ActorLineTraceSingle(Hit, Cam.Location, WorldPos, ECC_Visibility, OccluderTraceParams))
                {
                    bBlocked = true;
                    break;
                }
            }
            if (!bBlocked)
                VisibleCount++;
        }
        else
        {
            VisibleCount++;
        }
    }

    return VisibleCount;
}

FColor CoverageToColorDiscrete(int32 CameraCount)
{
    switch (CameraCount)
    {
        case 0:  return FColor(80, 0, 0, 255);       // Dark red (no coverage)
        case 1:  return FColor(255, 0, 0, 255);      // Red
        case 2:  return FColor(255, 128, 0, 255);    // Orange
        case 3:  return FColor(255, 255, 0, 255);    // Yellow
        case 4:  return FColor(0, 200, 0, 255);      // Green
        case 5:  return FColor(0, 200, 255, 255);    // Cyan
        case 6:  return FColor(0, 100, 255, 255);    // Blue
        case 7:  return FColor(180, 0, 255, 255);    // Purple
        case 8:  return FColor(255, 0, 200, 255);    // Magenta
        case 9:  return FColor(255, 150, 200, 255);  // Pink
        default: return FColor(255, 255, 255, 255); // White (10+)
    }
}

void ACoverageHeatmapGenerator::WriteToTexture(const TArray<FColor>& PixelData)
{
    check(IsInGameThread());

    // Recreate texture if needed
    if (!ResultTexture || ResultTexture->GetSizeX() != Resolution)
    {
        ResultTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
        if (!ResultTexture)
        {
            UE_LOG(LogTemp, Error, TEXT("Heatmap: Failed to create texture"));
            return;
        }
        ResultTexture->Filter = TF_Bilinear;
        ResultTexture->SRGB = true;
        ResultTexture->AddToRoot(); // Prevent GC
        ResultTexture->UpdateResource();
    }

    FTexturePlatformData* PlatformData = ResultTexture->GetPlatformData();
    if (!PlatformData || PlatformData->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Heatmap: Invalid texture platform data"));
        return;
    }

    FTexture2DMipMap& Mip = PlatformData->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    if (Data)
    {
        FMemory::Memcpy(Data, PixelData.GetData(), PixelData.Num() * sizeof(FColor));
        Mip.BulkData.Unlock();
        ResultTexture->UpdateResource();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Heatmap: Failed to lock texture data"));
        Mip.BulkData.Unlock();
    }
}

void ACoverageHeatmapGenerator::PositionPlane(const FVector& FloorMin, const FVector& FloorMax, float FloorZ)
{
    UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneMesh)
    {
        HeatmapPlane->SetStaticMesh(PlaneMesh);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Heatmap: Failed to load plane mesh"));
        return;
    }

    FVector Center = (FloorMin + FloorMax) * 0.5f;
    Center.Z = FloorZ + PlaneZOffset;

    FVector Size = FloorMax - FloorMin;
    if (FMath::Abs(Size.X) < 1.0f) Size.X = 100.0f;
    if (FMath::Abs(Size.Y) < 1.0f) Size.Y = 100.0f;

    FVector Scale(Size.X / 100.0f, Size.Y / 100.0f, 1.0f);

    SetActorLocation(Center);
    SetActorRotation(FRotator::ZeroRotator);
    SetActorScale3D(Scale);

    // Determine base material
    UMaterialInterface* BaseMat = HeatmapMaterial;

    // Create material programmatically if none assigned
    if (!BaseMat)
    {
#if WITH_EDITOR
        GeneratedBaseMaterial = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
        GeneratedBaseMaterial->MaterialDomain = EMaterialDomain::MD_Surface;
        GeneratedBaseMaterial->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
        GeneratedBaseMaterial->BlendMode = EBlendMode::BLEND_Translucent;

        // Texture sampler for heatmap
        UMaterialExpressionTextureSampleParameter2D* TexSampler = NewObject<UMaterialExpressionTextureSampleParameter2D>(GeneratedBaseMaterial);
        TexSampler->ParameterName = TEXT("CoverageTex");
        TexSampler->SamplerType = SAMPLERTYPE_Color;
        TexSampler->Texture = ResultTexture;
        GeneratedBaseMaterial->GetExpressionCollection().AddExpression(TexSampler);

        // Connect texture RGB to emissive (visible in Unlit viewport mode)
        GeneratedBaseMaterial->GetEditorOnlyData()->EmissiveColor.Connect(0, TexSampler);

        // Constant opacity
        UMaterialExpressionConstant* OpacityConst = NewObject<UMaterialExpressionConstant>(GeneratedBaseMaterial);
        OpacityConst->R = 0.8f;
        GeneratedBaseMaterial->GetExpressionCollection().AddExpression(OpacityConst);
        GeneratedBaseMaterial->GetEditorOnlyData()->Opacity.Connect(0, OpacityConst);

        // Compile the material
        GeneratedBaseMaterial->PreEditChange(nullptr);
        GeneratedBaseMaterial->PostEditChange();

        BaseMat = GeneratedBaseMaterial;
#else
        UE_LOG(LogTemp, Error, TEXT("Heatmap: Cannot create material at runtime. Assign HeatmapMaterial in Details panel."));
#endif
    }

    if (BaseMat)
    {
        DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
        HeatmapPlane->SetMaterial(0, DynamicMaterial);

        if (ResultTexture)
        {
            DynamicMaterial->SetTextureParameterValue(TEXT("CoverageTex"), ResultTexture);
        }
    }
}

void ACoverageHeatmapGenerator::Generate()
{
    // Must run on game thread (line traces and texture ops require it)
    check(IsInGameThread());

    if (Resolution <= 0 || Resolution > 2048)
    {
        UE_LOG(LogTemp, Error, TEXT("Heatmap: Invalid resolution %d (must be 1-2048)"), Resolution);
        return;
    }

    // Gather scene data
    FVector FloorMin, FloorMax;
    float FloorZ;
    if (!CalculateFloorBounds(FloorMin, FloorMax, FloorZ))
        return;

    TArray<FCameraInfo> Cameras;
    int32 ExcludedCameras = 0;
    if (!GatherCameras(Cameras, ExcludedCameras))
        return;

    // Setup trace params - ignore floor and camera actors so they don't block visibility
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(HeatmapTrace), true);
    TraceParams.bTraceComplex = false;

    // Ignore floor actors
    TArray<AActor*> FloorActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FloorTag, FloorActors);
    for (AActor* Floor : FloorActors)
    {
        TraceParams.AddIgnoredActor(Floor);
    }

    // Ignore camera actors
    TArray<AActor*> CameraActorsToIgnore;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), HeatmapCameraTag, CameraActorsToIgnore);
    for (AActor* Cam : CameraActorsToIgnore)
    {
        TraceParams.AddIgnoredActor(Cam);
    }

    // Also ignore the heatmap generator itself
    TraceParams.AddIgnoredActor(this);

    // Gather occluder actors based on OcclusionIncludeTags
    TArray<AActor*> OccluderActors;
    for (const FName& Tag : OcclusionIncludeTags)
    {
        TArray<AActor*> TaggedActors;
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, TaggedActors);
        OccluderActors.Append(TaggedActors);
    }

    // Apply padding to extend/shrink bounds
    FloorMin.X -= BoundsPadX;
    FloorMin.Y -= BoundsPadY;
    FloorMax.X += BoundsPadX;
    FloorMax.Y += BoundsPadY;

    // Apply margin to shrink test area away from walls
    FloorMin.X += BoundsMargin;
    FloorMin.Y += BoundsMargin;
    FloorMax.X -= BoundsMargin;
    FloorMax.Y -= BoundsMargin;

    FVector BoundsSize = FloorMax - FloorMin;

    // Allocate pixel buffer
    TArray<FColor> PixelData;
    PixelData.SetNum(Resolution * Resolution);

    double StartTime = FPlatformTime::Seconds();
    int32 MaxCoverage = 0;
    int32 PointsWithCoverage = 0;

    // First pass: calculate coverage values and find max
    TArray<int32> CoverageData;
    CoverageData.SetNum(Resolution * Resolution);

    for (int32 Y = 0; Y < Resolution; Y++)
    {
        for (int32 X = 0; X < Resolution; X++)
        {
            float U = (X + 0.5f) / Resolution;
            float V = (Y + 0.5f) / Resolution;

            FVector WorldPos(
                FloorMin.X + U * BoundsSize.X,
                FloorMin.Y + V * BoundsSize.Y,
                FloorZ + 10.0f
            );

            int32 Coverage = CalculateCoverageAtPoint(WorldPos, Cameras, OccluderActors);
            CoverageData[Y * Resolution + X] = Coverage;
            MaxCoverage = FMath::Max(MaxCoverage, Coverage);
            if (Coverage > 0) PointsWithCoverage++;
        }
    }

    // Second pass: colorize with discrete bands based on camera count
    for (int32 i = 0; i < CoverageData.Num(); i++)
    {
        PixelData[i] = CoverageToColorDiscrete(CoverageData[i]);
    }

    double ElapsedTime = FPlatformTime::Seconds() - StartTime;
    int32 TotalCameras = Cameras.Num() + ExcludedCameras;
    UE_LOG(LogTemp, Log, TEXT("Heatmap [%s]: %dx%d in %.1fs | Cameras: %d total, %d included, %d excluded | %d occluders | Max coverage: %d, Points covered: %d/%d"),
        *GetName(), Resolution, Resolution, ElapsedTime, TotalCameras, Cameras.Num(), ExcludedCameras, OccluderActors.Num(),
        MaxCoverage, PointsWithCoverage, Resolution * Resolution);

    // Write results
    WriteToTexture(PixelData);
    PositionPlane(FloorMin, FloorMax, FloorZ);

    // Ensure plane is visible (in case Clear() was called previously)
    HeatmapPlane->SetVisibility(true);

    // Display color legend on screen
    DisplayLegend();
}

void ACoverageHeatmapGenerator::DisplayLegend()
{
    if (!GEngine) return;

    // Clear existing legend first
    uint64 BaseKey = GetUniqueID();
    for (int32 i = 0; i <= 10; i++)
    {
        GEngine->RemoveOnScreenDebugMessage(BaseKey + i);
    }

    if (!bShowLegend) return;

    const float DisplayTime = 9999.0f; // Persist until cleared
    GEngine->AddOnScreenDebugMessage(BaseKey + 10, DisplayTime, FColor(255, 255, 255, 255), TEXT("10+: White"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 9,  DisplayTime, FColor(255, 150, 200, 255), TEXT(" 9 : Pink"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 8,  DisplayTime, FColor(255, 0, 200, 255),   TEXT(" 8 : Magenta"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 7,  DisplayTime, FColor(180, 0, 255, 255),   TEXT(" 7 : Purple"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 6,  DisplayTime, FColor(0, 100, 255, 255),   TEXT(" 6 : Blue"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 5,  DisplayTime, FColor(0, 200, 255, 255),   TEXT(" 5 : Cyan"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 4,  DisplayTime, FColor(0, 200, 0, 255),     TEXT(" 4 : Green"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 3,  DisplayTime, FColor(255, 255, 0, 255),   TEXT(" 3 : Yellow"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 2,  DisplayTime, FColor(255, 128, 0, 255),   TEXT(" 2 : Orange"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 1,  DisplayTime, FColor(255, 0, 0, 255),     TEXT(" 1 : Red"));
    GEngine->AddOnScreenDebugMessage(BaseKey + 0,  DisplayTime, FColor(80, 0, 0, 255),      TEXT(" 0 : Dark Red (no coverage)"));
}

void ACoverageHeatmapGenerator::Clear()
{
    // Clear legend
    if (GEngine)
    {
        uint64 BaseKey = GetUniqueID();
        for (int32 i = 0; i <= 10; i++)
        {
            GEngine->RemoveOnScreenDebugMessage(BaseKey + i);
        }
    }

    // Hide the heatmap plane
    if (HeatmapPlane)
    {
        HeatmapPlane->SetVisibility(false);
        HeatmapPlane->SetStaticMesh(nullptr);
    }

    // Clear materials
    DynamicMaterial = nullptr;
    GeneratedBaseMaterial = nullptr;

    // Clear the result texture
    if (ResultTexture)
    {
        ResultTexture->RemoveFromRoot();
        ResultTexture = nullptr;
    }
}

void ACoverageHeatmapGenerator::ParseCameraLabel(const FString& InLabel, FString& OutLocationID, int32& OutSequenceIndex, FString& OutTarget) const
{
    // Parse naming convention: EVT_CAM_PERIM2_M3_1_MAIN
    //                                 LocationID  Seq Target
    // Format: PREFIX_LOCATIONID_<SequenceIndex>_TARGET
    // LocationID includes zone and mount (e.g., PERIM2_M3)

    OutLocationID = TEXT("");
    OutSequenceIndex = 0;
    OutTarget = TEXT("");

    FString Label = InLabel;

    // Remove EVT_CAM_ prefix if present
    if (Label.StartsWith(TEXT("EVT_CAM_")))
    {
        Label = Label.RightChop(8);
    }
    else if (Label.StartsWith(TEXT("CAM_")))
    {
        Label = Label.RightChop(4);
    }

    // Split by underscore
    TArray<FString> Parts;
    Label.ParseIntoArray(Parts, TEXT("_"), true);

    if (Parts.Num() == 0)
    {
        return;
    }

    // Find the M<Index> part, then the sequence number after it
    int32 MountIdx = -1;
    for (int32 i = 0; i < Parts.Num(); i++)
    {
        if (Parts[i].StartsWith(TEXT("M")) && Parts[i].Len() > 1)
        {
            FString NumPart = Parts[i].RightChop(1);
            if (NumPart.IsNumeric())
            {
                MountIdx = i;
                break;
            }
        }
    }

    if (MountIdx >= 0)
    {
        // LocationID is everything up to and including M<Index>
        for (int32 i = 0; i <= MountIdx; i++)
        {
            if (i > 0) OutLocationID += TEXT("_");
            OutLocationID += Parts[i];
        }

        // SequenceIndex is the part after M<Index>
        if (MountIdx + 1 < Parts.Num() && Parts[MountIdx + 1].IsNumeric())
        {
            OutSequenceIndex = FCString::Atoi(*Parts[MountIdx + 1]);

            // Target is everything after SequenceIndex
            for (int32 i = MountIdx + 2; i < Parts.Num(); i++)
            {
                if (!OutTarget.IsEmpty()) OutTarget += TEXT("_");
                OutTarget += Parts[i];
            }
        }
    }
    else
    {
        // Fallback: first part is location, last part is target
        OutLocationID = Parts[0];
        if (Parts.Num() > 1)
        {
            OutTarget = Parts.Last();
        }
    }
}

bool ACoverageHeatmapGenerator::GatherCamerasForExport(TArray<FExportCameraData>& OutCameras)
{
    TArray<AActor*> CameraActors;

    // Gather cameras from all specified tags
    for (const FName& Tag : CameraTags)
    {
        TArray<AActor*> TaggedActors;
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, TaggedActors);
        CameraActors.Append(TaggedActors);
    }

    for (AActor* Actor : CameraActors)
    {
        // Skip cameras with exclude tag
        if (CameraExcludeTag != NAME_None && Actor->ActorHasTag(CameraExcludeTag))
        {
            continue;
        }

        ACineCameraActor* CamActor = Cast<ACineCameraActor>(Actor);
        if (!CamActor) continue;

        UCineCameraComponent* CineComp = CamActor->GetCineCameraComponent();
        if (!CineComp) continue;

        FExportCameraData Data;
        Data.CameraActor = CamActor;
        Data.CameraID = CamActor->GetActorLabel();
        ParseCameraLabel(CamActor->GetActorLabel(), Data.LocationID, Data.SequenceIndex, Data.Target);
        Data.Position = CamActor->GetActorLocation();
        Data.Rotation = CamActor->GetActorRotation();
        Data.FOVH = CineComp->GetHorizontalFieldOfView();
        Data.FOVV = CineComp->GetVerticalFieldOfView();

        OutCameras.Add(Data);
    }

    if (OutCameras.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Export: No cameras found with specified tags"));
        return false;
    }

    return true;
}

void ACoverageHeatmapGenerator::ExportCamerasToCSV()
{
#if WITH_EDITOR
    TArray<FExportCameraData> Cameras;
    if (!GatherCamerasForExport(Cameras))
    {
        return;
    }

    // Build file path from configured directory and filename
    FString Directory = ExportDirectory.Path;
    if (Directory.IsEmpty())
    {
        Directory = FPaths::ProjectDir();
    }

    FString FilePath = FPaths::Combine(Directory, ExportFilename + TEXT(".csv"));
    FilePath = FPaths::ConvertRelativePathToFull(FilePath);

    // Natural sort comparison lambda (handles numeric parts properly)
    auto NaturalCompare = [](const FString& A, const FString& B) -> bool
    {
        int32 i = 0, j = 0;
        while (i < A.Len() && j < B.Len())
        {
            TCHAR cA = A[i];
            TCHAR cB = B[j];

            if (FChar::IsDigit(cA) && FChar::IsDigit(cB))
            {
                // Extract numeric parts
                int32 numA = 0, numB = 0;
                while (i < A.Len() && FChar::IsDigit(A[i]))
                {
                    numA = numA * 10 + (A[i] - '0');
                    i++;
                }
                while (j < B.Len() && FChar::IsDigit(B[j]))
                {
                    numB = numB * 10 + (B[j] - '0');
                    j++;
                }
                if (numA != numB) return numA < numB;
            }
            else
            {
                if (cA != cB) return cA < cB;
                i++;
                j++;
            }
        }
        return A.Len() < B.Len();
    };

    // Sort cameras by LocationID (natural), then SequenceIndex
    Cameras.Sort([&NaturalCompare](const FExportCameraData& A, const FExportCameraData& B)
    {
        if (A.LocationID != B.LocationID) return NaturalCompare(A.LocationID, B.LocationID);
        return A.SequenceIndex < B.SequenceIndex;
    });

    // Build CSV content
    FString CSVContent;

    // Header row
    CSVContent += TEXT("Camera ID,Location ID,Sequence Index,Target,X,Y,Z,Pitch,Yaw,Roll,FOV-H,FOV-V\n");

    // Data rows
    for (const FExportCameraData& Cam : Cameras)
    {
        CSVContent += FString::Printf(
            TEXT("%s,%s,%d,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n"),
            *Cam.CameraID,
            *Cam.LocationID,
            Cam.SequenceIndex,
            *Cam.Target,
            Cam.Position.X,
            Cam.Position.Y,
            Cam.Position.Z,
            Cam.Rotation.Pitch,
            Cam.Rotation.Yaw,
            Cam.Rotation.Roll,
            Cam.FOVH,
            Cam.FOVV
        );
    }

    // Write to file
    if (FFileHelper::SaveStringToFile(CSVContent, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogTemp, Log, TEXT("Export: Successfully exported %d cameras to %s"), Cameras.Num(), *FilePath);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                FString::Printf(TEXT("Exported %d cameras to CSV"), Cameras.Num()));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Export: Failed to write CSV to %s"), *FilePath);
    }
#else
    UE_LOG(LogTemp, Error, TEXT("Export: CSV export only available in editor"));
#endif
}

void ACoverageHeatmapGenerator::ExportLocationsToCSV()
{
#if WITH_EDITOR
    TArray<FExportCameraData> Cameras;
    if (!GatherCamerasForExport(Cameras))
    {
        return;
    }

    // Build file path
    FString Directory = ExportDirectory.Path;
    if (Directory.IsEmpty())
    {
        Directory = FPaths::ProjectDir();
    }

    FString FilePath = FPaths::Combine(Directory, ExportFilename + TEXT(".csv"));
    FilePath = FPaths::ConvertRelativePathToFull(FilePath);

    // Group cameras by LocationID and average transforms
    TMap<FString, TArray<FExportCameraData*>> LocationGroups;
    for (FExportCameraData& Cam : Cameras)
    {
        LocationGroups.FindOrAdd(Cam.LocationID).Add(&Cam);
    }

    // Build CSV content
    FString CSVContent;
    CSVContent += TEXT("Location ID,Camera Count,X,Y,Z\n");

    // Natural sort comparison lambda
    auto NaturalCompare = [](const FString& A, const FString& B) -> bool
    {
        int32 i = 0, j = 0;
        while (i < A.Len() && j < B.Len())
        {
            TCHAR cA = A[i];
            TCHAR cB = B[j];

            if (FChar::IsDigit(cA) && FChar::IsDigit(cB))
            {
                int32 numA = 0, numB = 0;
                while (i < A.Len() && FChar::IsDigit(A[i]))
                {
                    numA = numA * 10 + (A[i] - '0');
                    i++;
                }
                while (j < B.Len() && FChar::IsDigit(B[j]))
                {
                    numB = numB * 10 + (B[j] - '0');
                    j++;
                }
                if (numA != numB) return numA < numB;
            }
            else
            {
                if (cA != cB) return cA < cB;
                i++;
                j++;
            }
        }
        return A.Len() < B.Len();
    };

    // Sort location IDs naturally
    TArray<FString> SortedLocationIDs;
    LocationGroups.GetKeys(SortedLocationIDs);
    SortedLocationIDs.Sort(NaturalCompare);

    for (const FString& LocationID : SortedLocationIDs)
    {
        const TArray<FExportCameraData*>& Group = LocationGroups[LocationID];

        // Average the positions
        FVector AvgPosition = FVector::ZeroVector;
        for (const FExportCameraData* Cam : Group)
        {
            AvgPosition += Cam->Position;
        }
        AvgPosition /= static_cast<float>(Group.Num());

        CSVContent += FString::Printf(
            TEXT("%s,%d,%.2f,%.2f,%.2f\n"),
            *LocationID,
            Group.Num(),
            AvgPosition.X,
            AvgPosition.Y,
            AvgPosition.Z
        );
    }

    // Write to file
    if (FFileHelper::SaveStringToFile(CSVContent, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogTemp, Log, TEXT("Export: Successfully exported %d locations to %s"), LocationGroups.Num(), *FilePath);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                FString::Printf(TEXT("Exported %d locations to CSV"), LocationGroups.Num()));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Export: Failed to write locations CSV to %s"), *FilePath);
    }
#else
    UE_LOG(LogTemp, Error, TEXT("Export: CSV export only available in editor"));
#endif
}

void ACoverageHeatmapGenerator::ExportCamerasToFBX()
{
#if WITH_EDITOR
    TArray<FExportCameraData> Cameras;
    if (!GatherCamerasForExport(Cameras))
    {
        return;
    }

    // Build file path from configured directory and filename
    FString Directory = ExportDirectory.Path;
    if (Directory.IsEmpty())
    {
        Directory = FPaths::ProjectDir();
    }

    FString FilePath = FPaths::Combine(Directory, ExportFilename + TEXT(".fbx"));
    FilePath = FPaths::ConvertRelativePathToFull(FilePath);

    if (!GEditor)
    {
        UE_LOG(LogTemp, Error, TEXT("Export: Editor not available"));
        return;
    }

    // Load a simple mesh for camera markers
    UStaticMesh* MarkerMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (!MarkerMesh)
    {
        MarkerMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    }

    // Spawn temporary marker actors at each camera location
    TArray<AActor*> TempMarkers;
    for (const FExportCameraData& Cam : Cameras)
    {
        if (!Cam.CameraActor) continue;

        // Offset rotation to align cone with camera forward direction
        FRotator MarkerRotation = Cam.CameraActor->GetActorRotation();
        MarkerRotation.Yaw += 180.0f;

        AStaticMeshActor* Marker = GetWorld()->SpawnActor<AStaticMeshActor>(
            Cam.CameraActor->GetActorLocation(),
            MarkerRotation
        );

        if (Marker)
        {
            UStaticMeshComponent* MeshComp = Marker->GetStaticMeshComponent();
            MeshComp->SetStaticMesh(MarkerMesh);
            MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            MeshComp->bUseDefaultCollision = false;
            Marker->SetActorScale3D(FVector(0.2f, 0.2f, 0.4f)); // Small elongated marker
            Marker->SetActorLabel(Cam.CameraID);
            TempMarkers.Add(Marker);
        }
    }

    // Select marker actors for export
    GEditor->SelectNone(false, true, false);
    for (AActor* Marker : TempMarkers)
    {
        GEditor->SelectActor(Marker, true, false, true);
    }

    // Use GEditor's export functionality
    GEditor->ExportMap(GetWorld(), *FilePath, true);

    // Deselect actors
    GEditor->SelectNone(false, true, false);

    // Clean up temporary markers
    for (AActor* Marker : TempMarkers)
    {
        if (Marker)
        {
            Marker->Destroy();
        }
    }
    TempMarkers.Empty();

    // Check if file was created
    if (FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("Export: Successfully exported %d cameras to %s"), Cameras.Num(), *FilePath);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                FString::Printf(TEXT("Exported %d cameras to FBX"), Cameras.Num()));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Export: Failed to export FBX to %s"), *FilePath);
    }
#else
    UE_LOG(LogTemp, Error, TEXT("Export: FBX export only available in editor"));
#endif
}

void ACoverageHeatmapGenerator::ExportCamerasToGLTF()
{
#if WITH_EDITOR
    TArray<FExportCameraData> Cameras;
    if (!GatherCamerasForExport(Cameras))
    {
        return;
    }

    // Build file path from configured directory and filename
    FString Directory = ExportDirectory.Path;
    if (Directory.IsEmpty())
    {
        Directory = FPaths::ProjectDir();
    }

    FString Extension = (GLTFExportFormat == EGLTFExportFormat::GLB) ? TEXT(".glb") : TEXT(".gltf");
    FString FilePath = FPaths::Combine(Directory, ExportFilename + Extension);
    FilePath = FPaths::ConvertRelativePathToFull(FilePath);

    // Collect camera actors to export
    TSet<AActor*> ActorsToExport;
    for (const FExportCameraData& Cam : Cameras)
    {
        if (Cam.CameraActor)
        {
            ActorsToExport.Add(Cam.CameraActor);
        }
    }

    if (ActorsToExport.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Export: No valid camera actors to export"));
        return;
    }

    // Configure export options
    UGLTFExportOptions* ExportOptions = NewObject<UGLTFExportOptions>();
    ExportOptions->bExportProxyMaterials = false;
    ExportOptions->bExportUnlitMaterials = false;
    ExportOptions->bExportCameras = true;
    ExportOptions->bExportLights = false;

    // Perform the export
    UWorld* World = GetWorld();
    bool bSuccess = UGLTFExporter::ExportToGLTF(World, FilePath, ExportOptions, ActorsToExport);

    // Report result
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("Export: Successfully exported %d cameras to %s"), Cameras.Num(), *FilePath);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                FString::Printf(TEXT("Exported %d cameras to glTF"), Cameras.Num()));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Export: Failed to export glTF to %s"), *FilePath);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
                TEXT("Failed to export glTF - check Output Log"));
        }
    }
#else
    UE_LOG(LogTemp, Error, TEXT("Export: glTF export only available in editor"));
#endif
}

void ACoverageHeatmapGenerator::ExportSceneToGLTFOrGLB()
{
#if WITH_EDITOR
    if (SceneExportFolders.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Export: No scene export folders specified"));
        return;
    }

    // Build file path
    FString Directory = ExportDirectory.Path;
    if (Directory.IsEmpty())
    {
        Directory = FPaths::ProjectDir();
    }

    FString Extension = (GLTFExportFormat == EGLTFExportFormat::GLB) ? TEXT(".glb") : TEXT(".gltf");
    FString FilePath = FPaths::Combine(Directory, ExportFilename + Extension);
    FilePath = FPaths::ConvertRelativePathToFull(FilePath);

    // Gather actors from specified folders
    TSet<AActor*> ActorsToExport;
    UWorld* World = GetWorld();

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;

        FName ActorFolder = Actor->GetFolderPath();

        for (const FName& ExportFolder : SceneExportFolders)
        {
            // Check if actor's folder starts with the export folder path
            if (ActorFolder.ToString().StartsWith(ExportFolder.ToString()))
            {
                ActorsToExport.Add(Actor);
                break;
            }
        }
    }

    if (ActorsToExport.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Export: No actors found in specified folders"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
                TEXT("No actors found in specified folders"));
        }
        return;
    }

    // Configure export options - simple geometry only, no textures
    UGLTFExportOptions* ExportOptions = NewObject<UGLTFExportOptions>();
    ExportOptions->TextureImageFormat = EGLTFTextureImageFormat::None;
    ExportOptions->BakeMaterialInputs = EGLTFMaterialBakeMode::Disabled;
    ExportOptions->bExportProxyMaterials = false;
    ExportOptions->bExportUnlitMaterials = false;
    ExportOptions->bExportClearCoatMaterials = false;
    ExportOptions->bExportClothMaterials = false;
    ExportOptions->bExportThinTranslucentMaterials = false;
    ExportOptions->bExportLightmaps = false;
    ExportOptions->bExportTextureTransforms = false;
    ExportOptions->bExportVertexColors = false;
    ExportOptions->bExportVertexSkinWeights = false;
    ExportOptions->bExportLevelSequences = false;
    ExportOptions->bExportAnimationSequences = false;
    ExportOptions->bExportCameras = false;
    ExportOptions->bExportLights = false;

    // Perform the export
    bool bSuccess = UGLTFExporter::ExportToGLTF(World, FilePath, ExportOptions, ActorsToExport);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("Export: Successfully exported %d actors to %s"), ActorsToExport.Num(), *FilePath);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                FString::Printf(TEXT("Exported %d scene actors to glTF"), ActorsToExport.Num()));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Export: Failed to export scene to %s"), *FilePath);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
                TEXT("Failed to export scene - check Output Log"));
        }
    }
#else
    UE_LOG(LogTemp, Error, TEXT("Export: glTF export only available in editor"));
#endif
}
