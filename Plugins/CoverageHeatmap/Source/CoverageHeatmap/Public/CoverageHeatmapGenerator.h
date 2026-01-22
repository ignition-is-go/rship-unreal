#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CineCameraActor.h"
#include "CoverageHeatmapGenerator.generated.h"

UENUM(BlueprintType)
enum class EGLTFExportFormat : uint8
{
    GLTF    UMETA(DisplayName = "glTF (JSON + binary)"),
    GLB     UMETA(DisplayName = "GLB (single binary)")
};

UCLASS(Blueprintable)
class COVERAGEHEATMAP_API ACoverageHeatmapGenerator : public AActor
{
    GENERATED_BODY()

public:
    ACoverageHeatmapGenerator();

    UFUNCTION(CallInEditor, Category = "Heatmap", meta = (DisplayPriority = -1))
    void Generate();

    UFUNCTION(CallInEditor, Category = "Heatmap", meta = (DisplayPriority = 0))
    void Clear();

    UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayPriority = 1))
    void ExportCamerasToCSV();

    UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayPriority = 2))
    void ExportCamerasToFBX();

    UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayPriority = 3))
    void ExportLocationsToCSV();

    UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayPriority = 4))
    void ExportCamerasToGLTF();

    UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayPriority = 5))
    void ExportSceneToGLTFOrGLB();

    // Show color legend on screen after generation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    bool bShowLegend = true;

    // Settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Resolution (px)"))
    int32 Resolution = 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Max Distance (cm)"))
    float MaxDistance = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Sensor Resolution Y (px)"))
    int32 SensorResolutionY = 1080;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Min Pixel Height (px)"))
    int32 MinPixelHeight = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Assumed Person Height (cm)"))
    float AssumedPersonHeight = 170.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Bounds Margin (cm)"))
    float BoundsMargin = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Bounds Pad X (cm)"))
    float BoundsPadX = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Bounds Pad Y (cm)"))
    float BoundsPadY = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    bool bCheckOcclusion = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    bool bTraceComplex = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap", meta = (DisplayName = "Plane Z Offset (cm)"))
    float PlaneZOffset = 5.0f;

    // ONLY actors with these tags will block traces (e.g., walls, pillars)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    TArray<FName> OcclusionIncludeTags;

    // Camera tag for heatmap calculation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    FName HeatmapCameraTag = TEXT("TrackingRig1");

    // Cameras with this tag will be excluded from calculation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    FName CameraExcludeTag = TEXT("HeatmapExclude");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    FName FloorTag = TEXT("Floor");

    // Material - assign a material with a "Texture" parameter, or leave blank for auto
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    UMaterialInterface* HeatmapMaterial;

    // Export Settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
    TArray<FName> CameraTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export", meta = (DisplayName = "Export Directory"))
    FDirectoryPath ExportDirectory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export", meta = (DisplayName = "Export Filename"))
    FString ExportFilename = TEXT("cameras");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export", meta = (DisplayName = "Location Tolerance (cm)"))
    float LocationTolerance = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export", meta = (DisplayName = "glTF Format"))
    EGLTFExportFormat GLTFExportFormat = EGLTFExportFormat::GLB;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export", meta = (DisplayName = "Scene Export Folders"))
    TArray<FName> SceneExportFolders;

    // Output
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heatmap")
    UTexture2D* ResultTexture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heatmap")
    UStaticMeshComponent* HeatmapPlane;

private:
    struct FCameraInfo
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        float HalfFOVH;
        float HalfFOVV;
    };

    struct FExportCameraData
    {
        FString CameraID;
        FString LocationID;
        int32 SequenceIndex;
        FString Target;
        FVector Position;
        FRotator Rotation;
        float FOVH;
        float FOVV;
        ACineCameraActor* CameraActor;
    };

    bool GatherCamerasForExport(TArray<FExportCameraData>& OutCameras);
    void ParseCameraLabel(const FString& InLabel, FString& OutLocationID, int32& OutSequenceIndex, FString& OutTarget) const;

    bool CalculateFloorBounds(FVector& OutMin, FVector& OutMax, float& OutFloorZ);
    bool GatherCameras(TArray<FCameraInfo>& OutCameras, int32& OutExcludedCount);
    int32 CalculateCoverageAtPoint(const FVector& WorldPos, const TArray<FCameraInfo>& Cameras, const TArray<AActor*>& OccluderActors);
    void WriteToTexture(const TArray<FColor>& PixelData);
    void PositionPlane(const FVector& FloorMin, const FVector& FloorMax, float FloorZ);
    void DisplayLegend();

    UPROPERTY()
    UMaterialInstanceDynamic* DynamicMaterial;

    UPROPERTY()
    UMaterial* GeneratedBaseMaterial;
};
