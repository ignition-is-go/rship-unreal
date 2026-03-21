#pragma once

#include "CoreMinimal.h"
#include "RotundaMullionTypes.generated.h"

// ---------------------------------------------------------------------------
// GPU-compatible structs — must match HLSL in DataInterfaceRotundaMullion.ush
// ---------------------------------------------------------------------------

/** Per-mullion geometry data computed in Houdini. Static — loaded once from CSV. */
struct FMullionData
{
    FVector3f Centroid;
    FVector3f Forward;
    FVector3f Tangent;
    float ZMin;
    float ZMax;
    float HMin;
    float HMax;
    float Width;
    float Height;
    float Angle;
    int32 RowCount;
    int32 ColCount;
};
static_assert(sizeof(FMullionData) == 72, "FMullionData size must match HLSL StructuredBuffer layout");

/** Per-atom geometry data computed in Houdini. Static — loaded once from CSV. */
struct FAtomData
{
    FVector3f Centroid;
    FVector3f Forward;
    int32 MullionID;
    int32 Column;
    int32 Row;
};
static_assert(sizeof(FAtomData) == 36, "FAtomData size must match HLSL StructuredBuffer layout");

/** Blend mode for control layers. */
UENUM(BlueprintType)
enum class EMullionBlendMode : uint8
{
    Override = 0,   // result = lerp(below, mine, weight)
    Additive = 1    // result = below + mine * weight
};

/** Runtime control layer. Written by rship every frame. Default: no selection, identity transform. */
struct FMullionControlLayer
{
    // Selection — bitmasks, all combine via intersection. All 0s = nothing selected (default).
    uint32 MullionMask[3];  // 96 bits covers 81 mullions
    uint32 AtomMask[5];     // 160 bits covers 144 atoms per mullion (24 rows x 6 cols)

    // Blend
    float Weight;           // 0–1 layer strength (default 0)
    int32 BlendMode;        // EMullionBlendMode as int (default 0 = Override)

    // Transform
    float Rotation[4];      // quaternion xyzw (default 0,0,0,1 = identity)
    float Translation[3];   // offset (default 0,0,0)
    float Scale;            // uniform scale (default 1)

    // Padding to 80 bytes (16-byte aligned)
    float _pad0;
    float _pad1;
};
static_assert(sizeof(FMullionControlLayer) == 80, "FMullionControlLayer size must match HLSL StructuredBuffer layout");

inline void InitMullionControlLayer(FMullionControlLayer& Layer)
{
    FMemory::Memzero(Layer);
    Layer.Rotation[3] = 1.0f;  // identity quaternion w
    Layer.Scale = 1.0f;
}

constexpr int32 MULLION_MAX_LAYERS = 8;
constexpr int32 MULLION_COUNT = 81;
constexpr int32 ATOM_ROWS = 24;
constexpr int32 ATOM_COLS = 6;
constexpr int32 ATOMS_PER_MULLION = ATOM_ROWS * ATOM_COLS; // 144

// ---------------------------------------------------------------------------
// Editor-friendly layer representation — component packs into GPU struct
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FMullionLayerSelection
{
    GENERATED_BODY()

    /** Mullion indices to select. Empty = all mullions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    TArray<int32> Mullions;

    /** Row indices within mullion. Empty = all rows. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    TArray<int32> Rows;

    /** Column indices within mullion. Empty = all columns. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    TArray<int32> Columns;

    /** Specific atom indices (row * 6 + col) within mullion. Empty = use rows/columns. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    TArray<int32> Atoms;
};

USTRUCT(BlueprintType)
struct FMullionLayerDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    FMullionLayerSelection Selection;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Weight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
    EMullionBlendMode BlendMode = EMullionBlendMode::Override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
    FQuat4f Rotation = FQuat4f::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
    FVector3f Translation = FVector3f::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
    float Scale = 1.0f;
};
