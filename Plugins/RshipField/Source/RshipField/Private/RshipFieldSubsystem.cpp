#include "RshipFieldSubsystem.h"

#include "RshipFieldComponent.h"
#include "RshipFieldSamplerComponent.h"
#include "RshipFieldShaders.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "RenderGraphBuilder.h"
#include "RenderingThread.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogRshipField, Log, All);

void URshipFieldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void URshipFieldSubsystem::Deinitialize()
{
    RegisteredFields.Reset();
    RegisteredSamplers.Reset();
    Super::Deinitialize();
}

TStatId URshipFieldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URshipFieldSubsystem, STATGROUP_Tickables);
}

bool URshipFieldSubsystem::IsTickable() const
{
    return !IsTemplate();
}

void URshipFieldSubsystem::RegisterField(URshipFieldComponent* Field)
{
    if (Field)
    {
        RegisteredFields.AddUnique(Field);
    }
}

void URshipFieldSubsystem::UnregisterField(URshipFieldComponent* Field)
{
    RegisteredFields.Remove(Field);
}

void URshipFieldSubsystem::RegisterSampler(URshipFieldSamplerComponent* Sampler)
{
    if (Sampler)
    {
        RegisteredSamplers.AddUnique(Sampler);
    }
}

void URshipFieldSubsystem::UnregisterSampler(URshipFieldSamplerComponent* Sampler)
{
    RegisteredSamplers.Remove(Sampler);
}

URshipFieldComponent* URshipFieldSubsystem::FindFieldById(const FString& InFieldId) const
{
    for (const TObjectPtr<URshipFieldComponent>& Field : RegisteredFields)
    {
        if (Field && Field->FieldId == InFieldId)
        {
            return Field;
        }
    }
    return nullptr;
}

void URshipFieldSubsystem::SetDebugEnabled(bool bEnabled)
{
    bDebugEnabled = bEnabled;
}

void URshipFieldSubsystem::SetDebugMode(ERshipFieldDebugMode InMode)
{
    DebugMode = InMode;
}

void URshipFieldSubsystem::Tick(float DeltaTime)
{
    bool bAnyFieldStepped = false;

    for (int32 Index = RegisteredFields.Num() - 1; Index >= 0; --Index)
    {
        URshipFieldComponent* Field = RegisteredFields[Index];
        if (!Field)
        {
            RegisteredFields.RemoveAtSwap(Index);
            continue;
        }

        TickField(Field, DeltaTime);
        bAnyFieldStepped = true;
    }

    if (bAnyFieldStepped && RegisteredSamplers.Num() > 0)
    {
        ReadbackAndDistributeSamplers();
    }
}

void URshipFieldSubsystem::TickField(URshipFieldComponent* Field, float DeltaTime)
{
    const float Step = 1.0f / FMath::Max(Field->UpdateHz, 1.0f);
    Field->TickAccumulator += DeltaTime;

    bool bDidStep = false;
    while (Field->TickAccumulator + KINDA_SMALL_NUMBER >= Step)
    {
        Field->TickAccumulator -= Step;
        ++Field->SimulationFrame;
        Field->SimulationTimeSeconds += Step;
        if (Field->bPlaying)
        {
            Field->BeatPhase = FMath::Fmod(
                Field->BeatPhase + (Step * Field->Bpm / 60.0f), 1.0f);
            if (Field->BeatPhase < 0.0f)
            {
                Field->BeatPhase += 1.0f;
            }
        }
        bDidStep = true;
    }

    if (bDidStep)
    {
        DispatchFieldPasses(Field);
    }
}

int32 URshipFieldSubsystem::NormalizeResolution(int32 RequestedResolution) const
{
    const int32 Allowed[] = { 64, 128, 192, 256, 320 };
    int32 Best = Allowed[0];
    int32 BestDist = FMath::Abs(RequestedResolution - Best);
    for (const int32 Candidate : Allowed)
    {
        const int32 Dist = FMath::Abs(RequestedResolution - Candidate);
        if (Dist < BestDist)
        {
            Best = Candidate;
            BestDist = Dist;
        }
    }
    return Best;
}

void URshipFieldSubsystem::DispatchFieldPasses(URshipFieldComponent* Field)
{
    if (!Field->EnsureAtlasTextures())
    {
        return;
    }

    const int32 Resolution = NormalizeResolution(Field->FieldResolution);
    const int32 TilesPerRow = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Resolution))));

    FTextureRenderTargetResource* ScalarResource = Field->GetScalarAtlas() ? Field->GetScalarAtlas()->GameThread_GetRenderTargetResource() : nullptr;
    FTextureRenderTargetResource* VectorResource = Field->GetVectorAtlas() ? Field->GetVectorAtlas()->GameThread_GetRenderTargetResource() : nullptr;

    if (!ScalarResource || !VectorResource)
    {
        return;
    }

    const FVector DomainHalfExtent = FVector(Field->DomainSizeCm * 0.5f);
    const FVector DomainMin = Field->DomainCenterCm - DomainHalfExtent;
    const FVector DomainMax = Field->DomainCenterCm + DomainHalfExtent;

    RshipFieldRDG::FGlobalDispatchInputs GlobalInputs;
    GlobalInputs.FieldResolution = Resolution;
    GlobalInputs.TilesPerRow = TilesPerRow;
    GlobalInputs.TimeSeconds = Field->SimulationTimeSeconds;
    GlobalInputs.BPM = Field->Bpm;
    GlobalInputs.TransportPhase = Field->BeatPhase;
    GlobalInputs.MasterScalarGain = Field->MasterScalarGain;
    GlobalInputs.MasterVectorGain = Field->MasterVectorGain;
    GlobalInputs.DomainMinCm = FVector3f(DomainMin);
    GlobalInputs.DomainMaxCm = FVector3f(DomainMax);
    GlobalInputs.bDebugEnabled = bDebugEnabled && Field->bDebugEnabled;
    GlobalInputs.DebugMode = static_cast<int32>(DebugMode);
    GlobalInputs.OutScalarFieldAtlasTexture = ScalarResource->GetRenderTargetTexture();
    GlobalInputs.OutVectorFieldAtlasTexture = VectorResource->GetRenderTargetTexture();

    // Single default phase group (layers/phase groups disabled for MVP)
    GlobalInputs.PhaseGroupData.Add(FVector4f(0.0f, 1.0f, 0.0f, 0.0f));

    // Single default layer
    GlobalInputs.LayerDataA.Add(FVector4f(1.0f, -100.0f, 100.0f, 0.0f));
    GlobalInputs.LayerDataB.Add(FVector4f(1.0f, -1.0f, 0.0f, 0.0f));

    // Flatten typed effectors into internal format
    TArray<FRshipFieldEffectorDesc> AllEffectors;
    AllEffectors.Reserve(Field->WaveEffectors.Num() + Field->NoiseEffectors.Num() + Field->AttractorEffectors.Num());

    for (const FRshipFieldWaveEffector& Wave : Field->WaveEffectors)
    {
        AllEffectors.Add(FRshipFieldEffectorDesc::FromWave(Wave));
    }
    for (const FRshipFieldNoiseEffector& Noise : Field->NoiseEffectors)
    {
        AllEffectors.Add(FRshipFieldEffectorDesc::FromNoise(Noise));
    }
    for (const FRshipFieldAttractorEffector& Attractor : Field->AttractorEffectors)
    {
        AllEffectors.Add(FRshipFieldEffectorDesc::FromAttractor(Attractor));
    }

    GlobalInputs.EffectorData0.Reserve(AllEffectors.Num());
    GlobalInputs.EffectorData1.Reserve(AllEffectors.Num());
    GlobalInputs.EffectorData2.Reserve(AllEffectors.Num());
    GlobalInputs.EffectorData3.Reserve(AllEffectors.Num());
    GlobalInputs.EffectorData4.Reserve(AllEffectors.Num());
    GlobalInputs.EffectorData5.Reserve(AllEffectors.Num());
    GlobalInputs.EffectorData6.Reserve(AllEffectors.Num());

    for (const FRshipFieldEffectorDesc& Eff : AllEffectors)
    {
        GlobalInputs.EffectorData0.Add(FVector4f(FVector3f(Eff.PositionCm), Eff.RadiusCm));
        GlobalInputs.EffectorData1.Add(FVector4f(FVector3f(Eff.Direction.GetSafeNormal()), Eff.Amplitude));
        GlobalInputs.EffectorData2.Add(FVector4f(Eff.WavelengthCm, Eff.FrequencyHz, Eff.Speed, Eff.PhaseOffset));
        GlobalInputs.EffectorData3.Add(FVector4f(Eff.FadeWeight, 0.0f, 0.0f, 0.0f));
        GlobalInputs.EffectorData4.Add(FVector4f(Eff.ClampMin, Eff.ClampMax, static_cast<float>(static_cast<uint8>(Eff.BlendOp)), static_cast<float>(static_cast<uint8>(Eff.Waveform))));
        GlobalInputs.EffectorData5.Add(FVector4f(-1.0f, static_cast<float>(static_cast<uint8>(Eff.NoiseMode)), Eff.NoiseScale, Eff.NoiseAmplitude));
        GlobalInputs.EffectorData6.Add(FVector4f(
            Eff.bEnabled ? 1.0f : 0.0f,
            Eff.bInfiniteRange ? 1.0f : 0.0f,
            Eff.bAffectsScalar ? 1.0f : 0.0f,
            Eff.bAffectsVector ? 1.0f : 0.0f));
    }

    GlobalInputs.LayerCount = 1;
    GlobalInputs.PhaseGroupCount = 1;
    GlobalInputs.EffectorCount = AllEffectors.Num();
    GlobalInputs.DebugSelectionIndex = INDEX_NONE;

    if (!GlobalInputs.IsValid())
    {
        return;
    }

    TArray<RshipFieldRDG::FTargetDispatchInputs> EmptyTargets;

    ENQUEUE_RENDER_COMMAND(RshipFieldDispatch)(
        [GlobalInputs, EmptyTargets](FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList);
            RshipFieldRDG::AddFieldPasses(GraphBuilder, GlobalInputs, EmptyTargets);
            GraphBuilder.Execute();
        });
}

void URshipFieldSubsystem::ReadbackAndDistributeSamplers()
{
    // Cache readback data per field to avoid reading the same atlas multiple times.
    struct FFieldReadback
    {
        TArray<FLinearColor> ScalarPixels;
        TArray<FLinearColor> VectorPixels;
        int32 Resolution = 0;
        int32 TilesPerRow = 0;
        int32 AtlasDim = 0;
        FVector DomainMin = FVector::ZeroVector;
        FVector InvDomainSize = FVector::ZeroVector;
        bool bValid = false;
    };
    TMap<FString, FFieldReadback> ReadbackCache;

    auto GetOrCreateReadback = [&](const FString& FieldId) -> const FFieldReadback*
    {
        if (FFieldReadback* Cached = ReadbackCache.Find(FieldId))
        {
            return Cached->bValid ? Cached : nullptr;
        }

        URshipFieldComponent* Field = !FieldId.IsEmpty() ? FindFieldById(FieldId) : nullptr;
        if (!Field && RegisteredFields.Num() > 0)
        {
            Field = RegisteredFields[0];
        }

        FFieldReadback& Rb = ReadbackCache.Add(FieldId);
        if (!Field || !Field->GetScalarAtlas() || !Field->GetVectorAtlas())
        {
            return nullptr;
        }

        FRenderTarget* ScalarRT = Field->GetScalarAtlas()->GameThread_GetRenderTargetResource();
        FRenderTarget* VectorRT = Field->GetVectorAtlas()->GameThread_GetRenderTargetResource();
        if (!ScalarRT || !VectorRT)
        {
            return nullptr;
        }

        ScalarRT->ReadLinearColorPixels(Rb.ScalarPixels);
        VectorRT->ReadLinearColorPixels(Rb.VectorPixels);

        Rb.Resolution = NormalizeResolution(Field->FieldResolution);
        Rb.TilesPerRow = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Rb.Resolution))));
        Rb.AtlasDim = Rb.TilesPerRow * Rb.Resolution;

        if (Rb.ScalarPixels.Num() < Rb.AtlasDim * Rb.AtlasDim || Rb.VectorPixels.Num() < Rb.AtlasDim * Rb.AtlasDim)
        {
            return nullptr;
        }

        const FVector DomainHalfExtent = FVector(Field->DomainSizeCm * 0.5f);
        Rb.DomainMin = Field->DomainCenterCm - DomainHalfExtent;
        const FVector DomainMax = Field->DomainCenterCm + DomainHalfExtent;
        const FVector DomainSize = DomainMax - Rb.DomainMin;
        Rb.InvDomainSize = FVector(
            DomainSize.X > 1.0f ? 1.0f / DomainSize.X : 0.0f,
            DomainSize.Y > 1.0f ? 1.0f / DomainSize.Y : 0.0f,
            DomainSize.Z > 1.0f ? 1.0f / DomainSize.Z : 0.0f);
        Rb.bValid = true;
        return &Rb;
    };

    auto SampleFieldAtPosition = [&](const FString& FieldId, const FVector& WorldPos, float& OutScalar, FVector& OutVector) -> bool
    {
        const FFieldReadback* Rb = GetOrCreateReadback(FieldId);
        if (!Rb)
        {
            return false;
        }

        const FVector UVW = FVector(
            FMath::Clamp((WorldPos.X - Rb->DomainMin.X) * Rb->InvDomainSize.X, 0.0, 1.0),
            FMath::Clamp((WorldPos.Y - Rb->DomainMin.Y) * Rb->InvDomainSize.Y, 0.0, 1.0),
            FMath::Clamp((WorldPos.Z - Rb->DomainMin.Z) * Rb->InvDomainSize.Z, 0.0, 1.0));

        const int32 VoxelX = FMath::Clamp(FMath::RoundToInt(UVW.X * (Rb->Resolution - 1)), 0, Rb->Resolution - 1);
        const int32 VoxelY = FMath::Clamp(FMath::RoundToInt(UVW.Y * (Rb->Resolution - 1)), 0, Rb->Resolution - 1);
        const int32 VoxelZ = FMath::Clamp(FMath::RoundToInt(UVW.Z * (Rb->Resolution - 1)), 0, Rb->Resolution - 1);

        const int32 TileX = VoxelZ % Rb->TilesPerRow;
        const int32 TileY = VoxelZ / Rb->TilesPerRow;
        const int32 AtlasX = TileX * Rb->Resolution + VoxelX;
        const int32 AtlasY = TileY * Rb->Resolution + VoxelY;
        const int32 PixelIndex = AtlasY * Rb->AtlasDim + AtlasX;

        if (PixelIndex < 0 || PixelIndex >= Rb->ScalarPixels.Num())
        {
            return false;
        }

        OutScalar = Rb->ScalarPixels[PixelIndex].R;
        const FLinearColor& VecPixel = Rb->VectorPixels[PixelIndex];
        OutVector = FVector(VecPixel.R, VecPixel.G, VecPixel.B);
        return true;
    };

    for (int32 Index = RegisteredSamplers.Num() - 1; Index >= 0; --Index)
    {
        URshipFieldSamplerComponent* Sampler = RegisteredSamplers[Index];
        if (!Sampler)
        {
            RegisteredSamplers.RemoveAtSwap(Index);
            continue;
        }

        const AActor* Owner = Sampler->GetOwner();
        if (!Owner)
        {
            continue;
        }

        const FVector WorldPos = Owner->GetActorLocation();

        for (const FString& FieldId : Sampler->GetRequiredFieldIds())
        {
            float Scalar = 0.0f;
            FVector Vector = FVector::ZeroVector;
            if (SampleFieldAtPosition(FieldId, WorldPos, Scalar, Vector))
            {
                Sampler->ApplySampledValue(FieldId, Scalar, Vector);
            }
        }
    }
}
