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
            // Accumulate beats continuously — no wrapping.
            // The shader uses this * TempoMultiplier * TWO_PI, so wrapping
            // happens naturally via sin() without phase discontinuities.
            Field->BeatPhase += Step * Field->Bpm / 60.0f;
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
        UE_LOG(LogRshipField, Warning, TEXT("DispatchFieldPasses: EnsureAtlasTextures failed for field '%s'"), *Field->FieldId);
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
    GlobalInputs.DebugMode = 0;
    GlobalInputs.DebugSelectionIndex = INDEX_NONE;
    GlobalInputs.OutScalarFieldAtlasTexture = ScalarResource->GetRenderTargetTexture();
    GlobalInputs.OutVectorFieldAtlasTexture = VectorResource->GetRenderTargetTexture();

    if (GEngine && Field->bShowDebugText)
    {
        GEngine->AddOnScreenDebugMessage(
            static_cast<uint64>(Field->GetUniqueID()) + 999,
            0.0f,
            FColor::Green,
            FString::Printf(TEXT("[Field '%s'] t=%.2f beat=%.3f bpm=%.0f playing=%s"),
                *Field->FieldId, Field->SimulationTimeSeconds, Field->BeatPhase, Field->Bpm, Field->bPlaying ? TEXT("Y") : TEXT("N")));
    }

    // Build phase groups
    TMap<FString, int32> PhaseGroupIndexById;
    // Default phase group at index 0 (free-running, no tempo sync)
    GlobalInputs.PhaseGroupData.Add(FVector4f(0.0f, 1.0f, 0.0f, 0.0f));
    PhaseGroupIndexById.Add(TEXT(""), 0);

    for (int32 GroupIndex = 0; GroupIndex < Field->PhaseGroups.Num(); ++GroupIndex)
    {
        const FRshipFieldPhaseGroup& Group = Field->PhaseGroups[GroupIndex];
        if (Group.Id.IsEmpty())
        {
            continue;
        }
        PhaseGroupIndexById.Add(Group.Id, GlobalInputs.PhaseGroupData.Num());
        GlobalInputs.PhaseGroupData.Add(FVector4f(
            1.0f, // bSyncToTempo = true for named groups
            Group.TempoMultiplier,
            Group.PhaseOffset,
            0.0f));
    }

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

    int32 EffectorDebugIndex = 0;
    for (const FRshipFieldEffectorDesc& Eff : AllEffectors)
    {
        const int32* PhaseGroupIndexPtr = PhaseGroupIndexById.Find(Eff.PhaseGroupId);
        const int32 PhaseGroupIndex = PhaseGroupIndexPtr ? *PhaseGroupIndexPtr : 0;

        if (GEngine && Field->bShowDebugText)
        {
            GEngine->AddOnScreenDebugMessage(
                static_cast<uint64>(Field->GetUniqueID()) + 1000 + EffectorDebugIndex,
                0.0f,
                FColor::Yellow,
                FString::Printf(TEXT("[Eff %d] pos=(%.0f,%.0f,%.0f) r=%.0f freq=%.2f wl=%.0f phase=%.2f group=%d(%s)"),
                    EffectorDebugIndex,
                    Eff.PositionCm.X, Eff.PositionCm.Y, Eff.PositionCm.Z,
                    Eff.RadiusCm, Eff.FrequencyHz, Eff.WavelengthCm,
                    Eff.PhaseOffset, PhaseGroupIndex, *Eff.PhaseGroupId));
        }
        ++EffectorDebugIndex;

        GlobalInputs.EffectorData0.Add(FVector4f(FVector3f(Eff.PositionCm), Eff.RadiusCm));
        GlobalInputs.EffectorData1.Add(FVector4f(FVector3f(Eff.Direction.GetSafeNormal()), Eff.Amplitude));
        GlobalInputs.EffectorData2.Add(FVector4f(Eff.WavelengthCm, Eff.FrequencyHz, 1.0f, Eff.PhaseOffset));
        // E3: (FadeWeight, FalloffExponent, EffectorType, 0)
        GlobalInputs.EffectorData3.Add(FVector4f(Eff.FadeWeight, Eff.FalloffExponent, static_cast<float>(static_cast<uint8>(Eff.Type)), 0.0f));
        GlobalInputs.EffectorData4.Add(FVector4f(Eff.ClampMin, Eff.ClampMax, static_cast<float>(static_cast<uint8>(Eff.BlendOp)), static_cast<float>(static_cast<uint8>(Eff.Waveform))));
        GlobalInputs.EffectorData5.Add(FVector4f(static_cast<float>(PhaseGroupIndex), static_cast<float>(static_cast<uint8>(Eff.NoiseMode)), Eff.NoiseScale, Eff.NoiseAmplitude));
        GlobalInputs.EffectorData6.Add(FVector4f(Eff.bEnabled ? 1.0f : 0.0f, Eff.bInfiniteRange ? 1.0f : 0.0f, Eff.bAffectsScalar ? 1.0f : 0.0f, Eff.bAffectsVector ? 1.0f : 0.0f));
    }

    GlobalInputs.LayerCount = 1;
    GlobalInputs.PhaseGroupCount = GlobalInputs.PhaseGroupData.Num();
    GlobalInputs.EffectorCount = AllEffectors.Num();

    if (!GlobalInputs.IsValid())
    {
        return;
    }

    UE_LOG(LogRshipField, Verbose, TEXT("Dispatching field '%s': %d effectors, %d layers, %d phasegroups, res=%d, t=%.2f"), *Field->FieldId, GlobalInputs.EffectorCount, GlobalInputs.LayerCount, GlobalInputs.PhaseGroupCount, GlobalInputs.FieldResolution, GlobalInputs.TimeSeconds);

    TArray<RshipFieldRDG::FTargetDispatchInputs> EmptyTargets;

    ENQUEUE_RENDER_COMMAND(RshipFieldDispatch)(
        [GlobalInputs, EmptyTargets](FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList);
            RshipFieldRDG::AddFieldPasses(GraphBuilder, GlobalInputs, EmptyTargets);
            GraphBuilder.Execute();
        });
}

void URshipFieldSubsystem::EnsureFieldReadbackCache(URshipFieldComponent* Field)
{
    if (!Field || !Field->GetScalarAtlas() || !Field->GetVectorAtlas())
    {
        return;
    }

    FFieldReadbackCache& Cache = ReadbackCaches.FindOrAdd(Field->FieldId);

    const uint64 CurrentFrame = GFrameCounter;
    if (Cache.FrameNumber == CurrentFrame)
    {
        return;
    }

    FRenderTarget* ScalarRT = Field->GetScalarAtlas()->GameThread_GetRenderTargetResource();
    FRenderTarget* VectorRT = Field->GetVectorAtlas()->GameThread_GetRenderTargetResource();
    if (!ScalarRT || !VectorRT)
    {
        return;
    }

    ScalarRT->ReadLinearColorPixels(Cache.ScalarPixels);
    VectorRT->ReadLinearColorPixels(Cache.VectorPixels);

    Cache.Resolution = NormalizeResolution(Field->FieldResolution);
    Cache.TilesPerRow = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Cache.Resolution))));
    Cache.AtlasDim = Cache.TilesPerRow * Cache.Resolution;

    const FVector DomainHalfExtent = FVector(Field->DomainSizeCm * 0.5f);
    Cache.DomainMin = Field->DomainCenterCm - DomainHalfExtent;
    const FVector DomainSize = DomainHalfExtent * 2.0f;
    Cache.InvDomainSize = FVector(
        DomainSize.X > 1.0f ? 1.0f / DomainSize.X : 0.0f,
        DomainSize.Y > 1.0f ? 1.0f / DomainSize.Y : 0.0f,
        DomainSize.Z > 1.0f ? 1.0f / DomainSize.Z : 0.0f);
    Cache.FrameNumber = CurrentFrame;
}

bool URshipFieldSubsystem::SampleFromCache(const FFieldReadbackCache& Cache, const FVector& WorldPosition, float& OutScalar, FVector& OutVector) const
{
    if (Cache.ScalarPixels.Num() < Cache.AtlasDim * Cache.AtlasDim)
    {
        return false;
    }

    const FVector UVW = FVector(
        FMath::Clamp((WorldPosition.X - Cache.DomainMin.X) * Cache.InvDomainSize.X, 0.0, 1.0),
        FMath::Clamp((WorldPosition.Y - Cache.DomainMin.Y) * Cache.InvDomainSize.Y, 0.0, 1.0),
        FMath::Clamp((WorldPosition.Z - Cache.DomainMin.Z) * Cache.InvDomainSize.Z, 0.0, 1.0));

    const int32 VoxelX = FMath::Clamp(FMath::FloorToInt(UVW.X * Cache.Resolution), 0, Cache.Resolution - 1);
    const int32 VoxelY = FMath::Clamp(FMath::FloorToInt(UVW.Y * Cache.Resolution), 0, Cache.Resolution - 1);
    const int32 VoxelZ = FMath::Clamp(FMath::FloorToInt(UVW.Z * Cache.Resolution), 0, Cache.Resolution - 1);

    const int32 TileX = VoxelZ % Cache.TilesPerRow;
    const int32 TileY = VoxelZ / Cache.TilesPerRow;
    const int32 AtlasX = TileX * Cache.Resolution + VoxelX;
    const int32 AtlasY = TileY * Cache.Resolution + VoxelY;
    const int32 PixelIndex = AtlasY * Cache.AtlasDim + AtlasX;

    if (PixelIndex < 0 || PixelIndex >= Cache.ScalarPixels.Num())
    {
        return false;
    }

    OutScalar = Cache.ScalarPixels[PixelIndex].R;
    const FLinearColor& VecPixel = Cache.VectorPixels[PixelIndex];
    OutVector = FVector(VecPixel.R, VecPixel.G, VecPixel.B);
    return true;
}

bool URshipFieldSubsystem::SampleFieldAtPosition(const FString& InFieldId, const FVector& WorldPosition, float& OutScalar, FVector& OutVector)
{
    OutScalar = 0.0f;
    OutVector = FVector::ZeroVector;

    URshipFieldComponent* Field = FindFieldById(InFieldId);
    if (!Field)
    {
        return false;
    }

    EnsureFieldReadbackCache(Field);

    const FFieldReadbackCache* Cache = ReadbackCaches.Find(InFieldId);
    if (!Cache)
    {
        return false;
    }

    return SampleFromCache(*Cache, WorldPosition, OutScalar, OutVector);
}

void URshipFieldSubsystem::DistributeSamplersForField(URshipFieldComponent* Field)
{
    if (!Field)
    {
        return;
    }

    EnsureFieldReadbackCache(Field);

    const FFieldReadbackCache* Cache = ReadbackCaches.Find(Field->FieldId);
    if (!Cache || Cache->ScalarPixels.Num() < Cache->AtlasDim * Cache->AtlasDim)
    {
        return;
    }

    for (int32 Index = RegisteredSamplers.Num() - 1; Index >= 0; --Index)
    {
        URshipFieldSamplerComponent* Sampler = RegisteredSamplers[Index];
        if (!Sampler)
        {
            RegisteredSamplers.RemoveAtSwap(Index);
            continue;
        }

        const TArray<FString> RequiredIds = Sampler->GetRequiredFieldIds();
        if (!RequiredIds.Contains(Field->FieldId))
        {
            continue;
        }

        const AActor* Owner = Sampler->GetOwner();
        if (!Owner)
        {
            continue;
        }

        float Scalar = 0.0f;
        FVector Vec = FVector::ZeroVector;
        if (SampleFromCache(*Cache, Owner->GetActorLocation(), Scalar, Vec))
        {
            Sampler->ApplySampledValue(Field->FieldId, Scalar, Vec);
        }
    }
}
