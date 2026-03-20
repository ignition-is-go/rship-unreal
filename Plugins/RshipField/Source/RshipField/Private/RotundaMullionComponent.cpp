#include "RotundaMullionComponent.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RotundaMullionComponent)

DEFINE_LOG_CATEGORY_STATIC(LogRotundaMullion, Log, All);

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

URotundaMullionComponent::URotundaMullionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bTickInEditor = true;

    Layers.SetNum(MULLION_MAX_LAYERS);
    ControlLayers.SetNum(MULLION_MAX_LAYERS);
    for (FMullionControlLayer& L : ControlLayers)
    {
        InitMullionControlLayer(L);
    }
}

void URotundaMullionComponent::OnRegister()
{
    Super::OnRegister();
}

void URotundaMullionComponent::OnUnregister()
{
    Super::OnUnregister();
}

void URotundaMullionComponent::BeginPlay()
{
    Super::BeginPlay();
    LoadCSVData();
}

void URotundaMullionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    PackLayersToGPU();
}

void URotundaMullionComponent::PackLayersToGPU()
{
    // Ensure arrays are sized
    if (Layers.Num() != MULLION_MAX_LAYERS)
    {
        Layers.SetNum(MULLION_MAX_LAYERS);
    }
    if (ControlLayers.Num() != MULLION_MAX_LAYERS)
    {
        ControlLayers.SetNum(MULLION_MAX_LAYERS);
    }

    for (int32 i = 0; i < MULLION_MAX_LAYERS; ++i)
    {
        const FMullionLayerDesc& Desc = Layers[i];
        FMullionControlLayer& GPU = ControlLayers[i];
        InitMullionControlLayer(GPU);

        // Pack selection masks
        const FMullionLayerSelection& Sel = Desc.Selection;
        if (Sel.Mullions.Num() > 0)
        {
            PackMullionMask(Sel.Mullions, GPU.MullionMask);
        }
        else
        {
            // No mullion selection = all mullions
            GPU.MullionMask[0] = 0xFFFFFFFF;
            GPU.MullionMask[1] = 0xFFFFFFFF;
            GPU.MullionMask[2] = 0x0001FFFF; // bits 0–16 for mullions 64–80
        }

        if (Sel.Atoms.Num() > 0 || Sel.Rows.Num() > 0 || Sel.Columns.Num() > 0)
        {
            PackAtomMask(Sel.Atoms, Sel.Rows, Sel.Columns, GPU.AtomMask);
        }
        else
        {
            // No atom selection = all atoms
            for (int32 j = 0; j < 5; ++j)
            {
                GPU.AtomMask[j] = 0xFFFFFFFF;
            }
            // Mask off bits beyond 144
            GPU.AtomMask[4] &= 0x0000FFFF; // bits 0–15 for atoms 128–143
        }

        // Blend
        GPU.Weight = Desc.Weight;
        GPU.BlendMode = static_cast<int32>(Desc.BlendMode);

        // Transform
        GPU.Rotation[0] = Desc.Rotation.X;
        GPU.Rotation[1] = Desc.Rotation.Y;
        GPU.Rotation[2] = Desc.Rotation.Z;
        GPU.Rotation[3] = Desc.Rotation.W;
        GPU.Translation[0] = Desc.Translation.X;
        GPU.Translation[1] = Desc.Translation.Y;
        GPU.Translation[2] = Desc.Translation.Z;
        GPU.Scale = Desc.Scale;
    }
}

// ---------------------------------------------------------------------------
// Rship target registration
// ---------------------------------------------------------------------------

void URotundaMullionComponent::RegisterOrRefreshTarget()
{
    FRshipTargetProxy Target = ResolveChildTarget(ChildTargetSuffix, TEXT("mullion"));
    if (!Target.IsValid())
    {
        return;
    }

    Target
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URotundaMullionComponent, SetLayerAction), TEXT("SetLayer"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URotundaMullionComponent, SetLayerSelectionAction), TEXT("SetLayerSelection"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URotundaMullionComponent, SetLayerTransformAction), TEXT("SetLayerTransform"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URotundaMullionComponent, SetLayerWeightAction), TEXT("SetLayerWeight"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URotundaMullionComponent, SetLayerBlendModeAction), TEXT("SetLayerBlendMode"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URotundaMullionComponent, ClearLayerAction), TEXT("ClearLayer"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URotundaMullionComponent, SetLayersBatchAction), TEXT("SetLayersBatch"));
}

// ---------------------------------------------------------------------------
// CSV loading
// ---------------------------------------------------------------------------

FString URotundaMullionComponent::ResolveCSVPath(const FFilePath& InPath) const
{
    FString Path = InPath.FilePath;
    if (Path.IsEmpty())
    {
        return FString();
    }
    if (FPaths::IsRelative(Path))
    {
        Path = FPaths::Combine(FPaths::ProjectContentDir(), Path);
    }
    FPaths::NormalizeFilename(Path);
    return Path;
}

bool URotundaMullionComponent::LoadCSVData()
{
    bool bOk = true;

    const FString MullionPath = ResolveCSVPath(MullionCSVPath);
    if (!MullionPath.IsEmpty())
    {
        if (!LoadMullionCSV(MullionPath))
        {
            bOk = false;
        }
    }

    const FString AtomPath = ResolveCSVPath(AtomCSVPath);
    if (!AtomPath.IsEmpty())
    {
        if (!LoadAtomCSV(AtomPath))
        {
            bOk = false;
        }
    }

    UE_LOG(LogRotundaMullion, Log, TEXT("CSV load: %d mullions, %d atoms (ok=%d)"),
        MullionDataArray.Num(), AtomDataArray.Num(), bOk);
    return bOk;
}

// Helper: find column index by name (case-insensitive)
static int32 FindColumn(const TArray<FString>& Headers, const FString& Name)
{
    for (int32 i = 0; i < Headers.Num(); ++i)
    {
        if (Headers[i].TrimStartAndEnd().Equals(Name, ESearchCase::IgnoreCase))
        {
            return i;
        }
    }
    return INDEX_NONE;
}

static float GetFloat(const TArray<FString>& Fields, int32 Col, float Default = 0.0f)
{
    if (Col == INDEX_NONE || Col >= Fields.Num()) return Default;
    return FCString::Atof(*Fields[Col].TrimStartAndEnd());
}

static int32 GetInt(const TArray<FString>& Fields, int32 Col, int32 Default = 0)
{
    if (Col == INDEX_NONE || Col >= Fields.Num()) return Default;
    return FCString::Atoi(*Fields[Col].TrimStartAndEnd());
}

bool URotundaMullionComponent::LoadMullionCSV(const FString& FilePath)
{
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
    {
        UE_LOG(LogRotundaMullion, Warning, TEXT("Failed to read mullion CSV: %s"), *FilePath);
        return false;
    }

    TArray<FString> Lines;
    FileContent.ParseIntoArrayLines(Lines, true);
    if (Lines.Num() < 2)
    {
        UE_LOG(LogRotundaMullion, Warning, TEXT("Mullion CSV has no data rows: %s"), *FilePath);
        return false;
    }

    TArray<FString> Headers;
    Lines[0].ParseIntoArray(Headers, TEXT(","));

    // Map columns — support both "CentroidX" and "Centroid.x" styles
    const int32 CX = FindColumn(Headers, TEXT("CentroidX"));
    const int32 CY = FindColumn(Headers, TEXT("CentroidY"));
    const int32 CZ = FindColumn(Headers, TEXT("CentroidZ"));
    const int32 FX = FindColumn(Headers, TEXT("ForwardX"));
    const int32 FY = FindColumn(Headers, TEXT("ForwardY"));
    const int32 FZ = FindColumn(Headers, TEXT("ForwardZ"));
    const int32 TX = FindColumn(Headers, TEXT("TangentX"));
    const int32 TY = FindColumn(Headers, TEXT("TangentY"));
    const int32 TZ = FindColumn(Headers, TEXT("TangentZ"));
    const int32 iZMin = FindColumn(Headers, TEXT("ZMin"));
    const int32 iZMax = FindColumn(Headers, TEXT("ZMax"));
    const int32 iHMin = FindColumn(Headers, TEXT("HMin"));
    const int32 iHMax = FindColumn(Headers, TEXT("HMax"));
    const int32 iWidth = FindColumn(Headers, TEXT("Width"));
    const int32 iHeight = FindColumn(Headers, TEXT("Height"));
    const int32 iAngle = FindColumn(Headers, TEXT("Angle"));
    const int32 iRowCount = FindColumn(Headers, TEXT("RowCount"));
    const int32 iColCount = FindColumn(Headers, TEXT("ColCount"));

    MullionDataArray.Reset();
    MullionDataArray.Reserve(Lines.Num() - 1);

    for (int32 i = 1; i < Lines.Num(); ++i)
    {
        TArray<FString> Fields;
        Lines[i].ParseIntoArray(Fields, TEXT(","));
        if (Fields.Num() == 0) continue;

        FMullionData M;
        M.Centroid = FVector3f(GetFloat(Fields, CX), GetFloat(Fields, CY), GetFloat(Fields, CZ));
        M.Forward  = FVector3f(GetFloat(Fields, FX), GetFloat(Fields, FY), GetFloat(Fields, FZ));
        M.Tangent  = FVector3f(GetFloat(Fields, TX), GetFloat(Fields, TY), GetFloat(Fields, TZ));
        M.ZMin     = GetFloat(Fields, iZMin);
        M.ZMax     = GetFloat(Fields, iZMax);
        M.HMin     = GetFloat(Fields, iHMin);
        M.HMax     = GetFloat(Fields, iHMax);
        M.Width    = GetFloat(Fields, iWidth);
        M.Height   = GetFloat(Fields, iHeight);
        M.Angle    = GetFloat(Fields, iAngle);
        M.RowCount = GetInt(Fields, iRowCount, ATOM_ROWS);
        M.ColCount = GetInt(Fields, iColCount, ATOM_COLS);
        MullionDataArray.Add(M);
    }

    return true;
}

bool URotundaMullionComponent::LoadAtomCSV(const FString& FilePath)
{
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
    {
        UE_LOG(LogRotundaMullion, Warning, TEXT("Failed to read atom CSV: %s"), *FilePath);
        return false;
    }

    TArray<FString> Lines;
    FileContent.ParseIntoArrayLines(Lines, true);
    if (Lines.Num() < 2)
    {
        UE_LOG(LogRotundaMullion, Warning, TEXT("Atom CSV has no data rows: %s"), *FilePath);
        return false;
    }

    TArray<FString> Headers;
    Lines[0].ParseIntoArray(Headers, TEXT(","));

    const int32 CX = FindColumn(Headers, TEXT("CentroidX"));
    const int32 CY = FindColumn(Headers, TEXT("CentroidY"));
    const int32 CZ = FindColumn(Headers, TEXT("CentroidZ"));
    const int32 FX = FindColumn(Headers, TEXT("ForwardX"));
    const int32 FY = FindColumn(Headers, TEXT("ForwardY"));
    const int32 FZ = FindColumn(Headers, TEXT("ForwardZ"));
    const int32 iMullionID = FindColumn(Headers, TEXT("MullionID"));
    const int32 iColumn = FindColumn(Headers, TEXT("Column"));
    const int32 iRow = FindColumn(Headers, TEXT("Row"));

    AtomDataArray.Reset();
    AtomDataArray.Reserve(Lines.Num() - 1);

    for (int32 i = 1; i < Lines.Num(); ++i)
    {
        TArray<FString> Fields;
        Lines[i].ParseIntoArray(Fields, TEXT(","));
        if (Fields.Num() == 0) continue;

        FAtomData A;
        A.Centroid  = FVector3f(GetFloat(Fields, CX), GetFloat(Fields, CY), GetFloat(Fields, CZ));
        A.Forward   = FVector3f(GetFloat(Fields, FX), GetFloat(Fields, FY), GetFloat(Fields, FZ));
        A.MullionID = GetInt(Fields, iMullionID);
        A.Column    = GetInt(Fields, iColumn);
        A.Row       = GetInt(Fields, iRow);
        AtomDataArray.Add(A);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Selection mask packing
// ---------------------------------------------------------------------------

void URotundaMullionComponent::PackMullionMask(const TArray<int32>& Indices, uint32 OutMask[3])
{
    OutMask[0] = OutMask[1] = OutMask[2] = 0;
    for (int32 Idx : Indices)
    {
        if (Idx >= 0 && Idx < MULLION_COUNT)
        {
            OutMask[Idx / 32] |= (1u << (Idx % 32));
        }
    }
}

void URotundaMullionComponent::PackAtomMask(const TArray<int32>& Atoms, const TArray<int32>& Rows, const TArray<int32>& Columns, uint32 OutMask[5])
{
    // Start with nothing selected
    FMemory::Memzero(OutMask, 5 * sizeof(uint32));

    bool bHasExplicit = Atoms.Num() > 0 || Rows.Num() > 0 || Columns.Num() > 0;
    if (!bHasExplicit)
    {
        // No atom-level selection specified — select all atoms
        for (int32 i = 0; i < ATOMS_PER_MULLION; ++i)
        {
            OutMask[i / 32] |= (1u << (i % 32));
        }
        return;
    }

    // Direct atom indices (row * ColCount + col)
    for (int32 Idx : Atoms)
    {
        if (Idx >= 0 && Idx < ATOMS_PER_MULLION)
        {
            OutMask[Idx / 32] |= (1u << (Idx % 32));
        }
    }

    // Rows: set all columns for those rows
    for (int32 Row : Rows)
    {
        if (Row >= 0 && Row < ATOM_ROWS)
        {
            for (int32 Col = 0; Col < ATOM_COLS; ++Col)
            {
                int32 Idx = Row * ATOM_COLS + Col;
                OutMask[Idx / 32] |= (1u << (Idx % 32));
            }
        }
    }

    // Columns: set all rows for those columns
    for (int32 Col : Columns)
    {
        if (Col >= 0 && Col < ATOM_COLS)
        {
            for (int32 Row = 0; Row < ATOM_ROWS; ++Row)
            {
                int32 Idx = Row * ATOM_COLS + Col;
                OutMask[Idx / 32] |= (1u << (Idx % 32));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// JSON parsing helpers
// ---------------------------------------------------------------------------

static bool ParseIntArray(const TSharedPtr<FJsonObject>& Obj, const FString& Key, TArray<int32>& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* Arr;
    if (!Obj->TryGetArrayField(Key, Arr)) return false;
    Out.Reserve(Arr->Num());
    for (const auto& Val : *Arr)
    {
        Out.Add(static_cast<int32>(Val->AsNumber()));
    }
    return true;
}

static void ParseSelectionIntoLayer(const TSharedPtr<FJsonObject>& Obj, FMullionControlLayer& Layer, URotundaMullionComponent* Comp)
{
    TArray<int32> Mullions, Atoms, Rows, Columns;
    ParseIntArray(Obj, TEXT("mullions"), Mullions);
    ParseIntArray(Obj, TEXT("atoms"), Atoms);
    ParseIntArray(Obj, TEXT("rows"), Rows);
    ParseIntArray(Obj, TEXT("columns"), Columns);

    if (Mullions.Num() > 0)
    {
        Comp->PackMullionMask(Mullions, Layer.MullionMask);
    }

    if (Atoms.Num() > 0 || Rows.Num() > 0 || Columns.Num() > 0)
    {
        Comp->PackAtomMask(Atoms, Rows, Columns, Layer.AtomMask);
    }
}

static void ParseTransformIntoLayer(const TSharedPtr<FJsonObject>& Obj, FMullionControlLayer& Layer)
{
    if (Obj->HasField(TEXT("rotX")))  Layer.Rotation[0] = Obj->GetNumberField(TEXT("rotX"));
    if (Obj->HasField(TEXT("rotY")))  Layer.Rotation[1] = Obj->GetNumberField(TEXT("rotY"));
    if (Obj->HasField(TEXT("rotZ")))  Layer.Rotation[2] = Obj->GetNumberField(TEXT("rotZ"));
    if (Obj->HasField(TEXT("rotW")))  Layer.Rotation[3] = Obj->GetNumberField(TEXT("rotW"));
    if (Obj->HasField(TEXT("transX"))) Layer.Translation[0] = Obj->GetNumberField(TEXT("transX"));
    if (Obj->HasField(TEXT("transY"))) Layer.Translation[1] = Obj->GetNumberField(TEXT("transY"));
    if (Obj->HasField(TEXT("transZ"))) Layer.Translation[2] = Obj->GetNumberField(TEXT("transZ"));
    if (Obj->HasField(TEXT("scale")))  Layer.Scale = Obj->GetNumberField(TEXT("scale"));
}

// ---------------------------------------------------------------------------
// Rship actions
// ---------------------------------------------------------------------------

void URotundaMullionComponent::SetLayerAction(const FString& LayerJson)
{
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LayerJson);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
    {
        UE_LOG(LogRotundaMullion, Warning, TEXT("SetLayer: invalid JSON"));
        return;
    }

    const int32 Idx = static_cast<int32>(Obj->GetNumberField(TEXT("layer")));
    if (Idx < 0 || Idx >= MULLION_MAX_LAYERS)
    {
        UE_LOG(LogRotundaMullion, Warning, TEXT("SetLayer: layer index %d out of range"), Idx);
        return;
    }

    FMullionControlLayer& Layer = ControlLayers[Idx];
    InitMullionControlLayer(Layer);

    ParseSelectionIntoLayer(Obj, Layer, this);
    ParseTransformIntoLayer(Obj, Layer);

    if (Obj->HasField(TEXT("weight")))    Layer.Weight = Obj->GetNumberField(TEXT("weight"));
    if (Obj->HasField(TEXT("blendMode"))) Layer.BlendMode = static_cast<int32>(Obj->GetNumberField(TEXT("blendMode")));
}

void URotundaMullionComponent::SetLayerSelectionAction(int32 LayerIndex, const FString& SelectionJson)
{
    if (LayerIndex < 0 || LayerIndex >= MULLION_MAX_LAYERS) return;

    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> SelReader = TJsonReaderFactory<>::Create(SelectionJson);
    if (!FJsonSerializer::Deserialize(SelReader, Obj) || !Obj.IsValid()) return;

    FMullionControlLayer& Layer = ControlLayers[LayerIndex];
    // Reset masks to zero before repacking
    FMemory::Memzero(Layer.MullionMask);
    FMemory::Memzero(Layer.AtomMask);
    ParseSelectionIntoLayer(Obj, Layer, this);
}

void URotundaMullionComponent::SetLayerTransformAction(int32 LayerIndex, float RotX, float RotY, float RotZ, float RotW, float TransX, float TransY, float TransZ, float InScale)
{
    if (LayerIndex < 0 || LayerIndex >= MULLION_MAX_LAYERS) return;

    FMullionControlLayer& Layer = ControlLayers[LayerIndex];
    Layer.Rotation[0] = RotX;
    Layer.Rotation[1] = RotY;
    Layer.Rotation[2] = RotZ;
    Layer.Rotation[3] = RotW;
    Layer.Translation[0] = TransX;
    Layer.Translation[1] = TransY;
    Layer.Translation[2] = TransZ;
    Layer.Scale = InScale;
}

void URotundaMullionComponent::SetLayerWeightAction(int32 LayerIndex, float Weight)
{
    if (LayerIndex < 0 || LayerIndex >= MULLION_MAX_LAYERS) return;
    ControlLayers[LayerIndex].Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
}

void URotundaMullionComponent::SetLayerBlendModeAction(int32 LayerIndex, EMullionBlendMode BlendMode)
{
    if (LayerIndex < 0 || LayerIndex >= MULLION_MAX_LAYERS) return;
    ControlLayers[LayerIndex].BlendMode = static_cast<int32>(BlendMode);
}

void URotundaMullionComponent::ClearLayerAction(int32 LayerIndex)
{
    if (LayerIndex < 0 || LayerIndex >= MULLION_MAX_LAYERS) return;
    InitMullionControlLayer(ControlLayers[LayerIndex]);
}

void URotundaMullionComponent::SetLayersBatchAction(const FString& BatchJson)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> BatchReader = TJsonReaderFactory<>::Create(BatchJson);
    if (!FJsonSerializer::Deserialize(BatchReader, Root) || !Root.IsValid())
    {
        UE_LOG(LogRotundaMullion, Warning, TEXT("SetLayersBatch: invalid JSON"));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* LayersArr;
    if (!Root->TryGetArrayField(TEXT("layers"), LayersArr)) return;

    for (const auto& Val : *LayersArr)
    {
        const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        const int32 Idx = static_cast<int32>(Obj->GetNumberField(TEXT("layer")));
        if (Idx < 0 || Idx >= MULLION_MAX_LAYERS) continue;

        FMullionControlLayer& Layer = ControlLayers[Idx];
        InitMullionControlLayer(Layer);
        ParseSelectionIntoLayer(Obj, Layer, this);
        ParseTransformIntoLayer(Obj, Layer);
        if (Obj->HasField(TEXT("weight")))    Layer.Weight = Obj->GetNumberField(TEXT("weight"));
        if (Obj->HasField(TEXT("blendMode"))) Layer.BlendMode = static_cast<int32>(Obj->GetNumberField(TEXT("blendMode")));
    }
}
