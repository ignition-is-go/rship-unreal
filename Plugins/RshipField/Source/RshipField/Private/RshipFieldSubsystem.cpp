#include "RshipFieldSubsystem.h"

#include "RshipFieldShaders.h"
#include "RshipFieldTargetComponent.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "RenderGraphBuilder.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogRshipField, Log, All);

namespace
{
constexpr int32 MaxQueuedPackets = 4096;
constexpr int32 MaxPacketChars = 2 * 1024 * 1024;

float QuantizeFloat(const float Value, const float Step)
{
    if (Step <= KINDA_SMALL_NUMBER)
    {
        return Value;
    }
    return FMath::RoundToFloat(Value / Step) * Step;
}

void NormalizeAxisWeights(float& InOutNormalWeight, float& InOutWorldWeight, float& InOutObjectWeight)
{
    InOutNormalWeight = FMath::Clamp(InOutNormalWeight, 0.0f, 1.0f);
    InOutWorldWeight = FMath::Clamp(InOutWorldWeight, 0.0f, 1.0f);
    InOutObjectWeight = FMath::Clamp(InOutObjectWeight, 0.0f, 1.0f);

    const float Sum = InOutNormalWeight + InOutWorldWeight + InOutObjectWeight;
    if (Sum <= KINDA_SMALL_NUMBER)
    {
        InOutNormalWeight = 1.0f;
        InOutWorldWeight = 0.0f;
        InOutObjectWeight = 0.0f;
        return;
    }

    const float InvSum = 1.0f / Sum;
    InOutNormalWeight *= InvSum;
    InOutWorldWeight *= InvSum;
    InOutObjectWeight *= InvSum;
}

bool TryGetVector3(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, FVector& OutVector)
{
    if (!Obj.IsValid() || !Obj->HasTypedField<EJson::Object>(FieldName))
    {
        return false;
    }

    const TSharedPtr<FJsonObject> VecObj = Obj->GetObjectField(FieldName);
    if (!VecObj.IsValid())
    {
        return false;
    }

    double X = OutVector.X;
    double Y = OutVector.Y;
    double Z = OutVector.Z;
    VecObj->TryGetNumberField(TEXT("x"), X);
    VecObj->TryGetNumberField(TEXT("y"), Y);
    VecObj->TryGetNumberField(TEXT("z"), Z);
    OutVector = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
    return true;
}

template <typename T>
bool TryGetNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, T& OutValue)
{
    if (!Obj.IsValid())
    {
        return false;
    }

    double Number = static_cast<double>(OutValue);
    if (!Obj->TryGetNumberField(FieldName, Number))
    {
        return false;
    }

    OutValue = static_cast<T>(Number);
    return true;
}

ERshipFieldBlendOp ParseBlendOp(const FString& InValue)
{
    const FString Token = InValue.TrimStartAndEnd().ToLower();
    if (Token == TEXT("subtract") || Token == TEXT("sub"))
    {
        return ERshipFieldBlendOp::Subtract;
    }
    if (Token == TEXT("min"))
    {
        return ERshipFieldBlendOp::Min;
    }
    if (Token == TEXT("max"))
    {
        return ERshipFieldBlendOp::Max;
    }
    if (Token == TEXT("multiply") || Token == TEXT("mul"))
    {
        return ERshipFieldBlendOp::Multiply;
    }
    return ERshipFieldBlendOp::Add;
}

ERshipFieldWaveform ParseWaveform(const FString& InValue)
{
    const FString Token = InValue.TrimStartAndEnd().ToLower();
    if (Token == TEXT("triangle") || Token == TEXT("tri"))
    {
        return ERshipFieldWaveform::Triangle;
    }
    if (Token == TEXT("saw") || Token == TEXT("sawtooth"))
    {
        return ERshipFieldWaveform::Saw;
    }
    if (Token == TEXT("square"))
    {
        return ERshipFieldWaveform::Square;
    }
    if (Token == TEXT("curvelut") || Token == TEXT("curve") || Token == TEXT("lut"))
    {
        return ERshipFieldWaveform::CurveLUT;
    }
    return ERshipFieldWaveform::Sine;
}

ERshipFieldNoiseType ParseNoiseType(const FString& InValue)
{
    const FString Token = InValue.TrimStartAndEnd().ToLower();
    if (Token == TEXT("value"))
    {
        return ERshipFieldNoiseType::Value;
    }
    if (Token == TEXT("simplex"))
    {
        return ERshipFieldNoiseType::Simplex;
    }
    if (Token == TEXT("curl"))
    {
        return ERshipFieldNoiseType::Curl;
    }
    return ERshipFieldNoiseType::None;
}

void ParseLayerArray(const TSharedPtr<FJsonObject>& Root, TArray<FRshipFieldLayerDesc>& OutLayers)
{
    OutLayers.Reset();
    if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("layers")))
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>& Layers = Root->GetArrayField(TEXT("layers"));
    OutLayers.Reserve(Layers.Num());

    for (int32 Index = 0; Index < Layers.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Obj = Layers[Index].IsValid() ? Layers[Index]->AsObject() : nullptr;
        if (!Obj.IsValid())
        {
            continue;
        }

        FRshipFieldLayerDesc Layer;
        Obj->TryGetStringField(TEXT("id"), Layer.Id);
        if (Layer.Id.IsEmpty())
        {
            Layer.Id = FString::Printf(TEXT("layer_%d"), Index);
        }

        FString BlendToken;
        if (Obj->TryGetStringField(TEXT("blendOp"), BlendToken))
        {
            Layer.BlendOp = ParseBlendOp(BlendToken);
        }

        Obj->TryGetBoolField(TEXT("enabled"), Layer.bEnabled);
        TryGetNumber(Obj, TEXT("weight"), Layer.Weight);
        TryGetNumber(Obj, TEXT("clampMin"), Layer.ClampMin);
        TryGetNumber(Obj, TEXT("clampMax"), Layer.ClampMax);
        Obj->TryGetStringField(TEXT("phaseGroupId"), Layer.PhaseGroupId);

        OutLayers.Add(Layer);
    }
}

void ParsePhaseGroupArray(const TSharedPtr<FJsonObject>& Root, TArray<FRshipFieldPhaseGroupDesc>& OutGroups)
{
    OutGroups.Reset();
    if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("phaseGroups")))
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>& Groups = Root->GetArrayField(TEXT("phaseGroups"));
    OutGroups.Reserve(Groups.Num());

    for (int32 Index = 0; Index < Groups.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Obj = Groups[Index].IsValid() ? Groups[Index]->AsObject() : nullptr;
        if (!Obj.IsValid())
        {
            continue;
        }

        FRshipFieldPhaseGroupDesc Group;
        Obj->TryGetStringField(TEXT("id"), Group.Id);
        if (Group.Id.IsEmpty())
        {
            Group.Id = FString::Printf(TEXT("phase_%d"), Index);
        }

        Obj->TryGetBoolField(TEXT("syncToTempo"), Group.bSyncToTempo);
        TryGetNumber(Obj, TEXT("tempoMultiplier"), Group.TempoMultiplier);
        TryGetNumber(Obj, TEXT("phaseOffset"), Group.PhaseOffset);

        OutGroups.Add(Group);
    }
}

void ParseEmitterArray(const TSharedPtr<FJsonObject>& Root, TArray<FRshipFieldEmitterDesc>& OutEmitters)
{
    OutEmitters.Reset();
    if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("emitters")))
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>& Emitters = Root->GetArrayField(TEXT("emitters"));
    OutEmitters.Reserve(Emitters.Num());

    for (int32 Index = 0; Index < Emitters.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Obj = Emitters[Index].IsValid() ? Emitters[Index]->AsObject() : nullptr;
        if (!Obj.IsValid())
        {
            continue;
        }

        FRshipFieldEmitterDesc E;
        Obj->TryGetStringField(TEXT("id"), E.Id);
        if (E.Id.IsEmpty())
        {
            E.Id = FString::Printf(TEXT("emitter_%d"), Index);
        }

        Obj->TryGetStringField(TEXT("layerId"), E.LayerId);
        Obj->TryGetBoolField(TEXT("enabled"), E.bEnabled);
        Obj->TryGetBoolField(TEXT("infiniteRange"), E.bInfiniteRange);

        TryGetVector3(Obj, TEXT("position"), E.PositionCm);
        TryGetVector3(Obj, TEXT("direction"), E.Direction);

        TryGetNumber(Obj, TEXT("radius"), E.RadiusCm);
        TryGetNumber(Obj, TEXT("amplitude"), E.Amplitude);
        TryGetNumber(Obj, TEXT("wavelength"), E.WavelengthCm);
        TryGetNumber(Obj, TEXT("frequency"), E.FrequencyHz);
        TryGetNumber(Obj, TEXT("speed"), E.Speed);
        TryGetNumber(Obj, TEXT("phaseOffset"), E.PhaseOffset);
        TryGetNumber(Obj, TEXT("fadeWeight"), E.FadeWeight);
        TryGetNumber(Obj, TEXT("attack"), E.EnvelopeAttackSeconds);
        TryGetNumber(Obj, TEXT("decay"), E.EnvelopeDecaySeconds);
        TryGetNumber(Obj, TEXT("noiseScale"), E.NoiseScale);
        TryGetNumber(Obj, TEXT("noiseAmplitude"), E.NoiseAmplitude);
        TryGetNumber(Obj, TEXT("clampMin"), E.ClampMin);
        TryGetNumber(Obj, TEXT("clampMax"), E.ClampMax);
        Obj->TryGetBoolField(TEXT("affectsScalar"), E.bAffectsScalar);
        Obj->TryGetBoolField(TEXT("affectsVector"), E.bAffectsVector);
        Obj->TryGetStringField(TEXT("phaseGroupId"), E.PhaseGroupId);

        FString BlendToken;
        if (Obj->TryGetStringField(TEXT("blendOp"), BlendToken))
        {
            E.BlendOp = ParseBlendOp(BlendToken);
        }

        FString WaveToken;
        if (Obj->TryGetStringField(TEXT("waveform"), WaveToken))
        {
            E.Waveform = ParseWaveform(WaveToken);
        }

        FString NoiseToken;
        if (Obj->TryGetStringField(TEXT("noiseType"), NoiseToken))
        {
            E.NoiseType = ParseNoiseType(NoiseToken);
        }

        OutEmitters.Add(E);
    }
}

void ParseSplineArray(const TSharedPtr<FJsonObject>& Root, TArray<FRshipFieldSplineEmitterDesc>& OutSplines)
{
    OutSplines.Reset();
    if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("splines")))
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>& Splines = Root->GetArrayField(TEXT("splines"));
    OutSplines.Reserve(Splines.Num());

    for (int32 Index = 0; Index < Splines.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Obj = Splines[Index].IsValid() ? Splines[Index]->AsObject() : nullptr;
        if (!Obj.IsValid())
        {
            continue;
        }

        FRshipFieldSplineEmitterDesc S;
        Obj->TryGetStringField(TEXT("id"), S.Id);
        if (S.Id.IsEmpty())
        {
            S.Id = FString::Printf(TEXT("spline_%d"), Index);
        }

        Obj->TryGetStringField(TEXT("layerId"), S.LayerId);
        Obj->TryGetBoolField(TEXT("enabled"), S.bEnabled);
        TryGetNumber(Obj, TEXT("curvatureToleranceCm"), S.CurvatureToleranceCm);
        TryGetNumber(Obj, TEXT("amplitude"), S.Amplitude);
        TryGetNumber(Obj, TEXT("radius"), S.RadiusCm);
        TryGetNumber(Obj, TEXT("clampMin"), S.ClampMin);
        TryGetNumber(Obj, TEXT("clampMax"), S.ClampMax);

        FString BlendToken;
        if (Obj->TryGetStringField(TEXT("blendOp"), BlendToken))
        {
            S.BlendOp = ParseBlendOp(BlendToken);
        }

        FString WaveToken;
        if (Obj->TryGetStringField(TEXT("waveform"), WaveToken))
        {
            S.Waveform = ParseWaveform(WaveToken);
        }

        if (Obj->HasTypedField<EJson::Array>(TEXT("controlPoints")))
        {
            const TArray<TSharedPtr<FJsonValue>>& Pts = Obj->GetArrayField(TEXT("controlPoints"));
            S.ControlPointsCm.Reserve(Pts.Num());
            for (const TSharedPtr<FJsonValue>& Pt : Pts)
            {
                const TSharedPtr<FJsonObject> PtObj = Pt.IsValid() ? Pt->AsObject() : nullptr;
                if (!PtObj.IsValid())
                {
                    continue;
                }

                FVector P = FVector::ZeroVector;
                double X = 0.0;
                double Y = 0.0;
                double Z = 0.0;
                PtObj->TryGetNumberField(TEXT("x"), X);
                PtObj->TryGetNumberField(TEXT("y"), Y);
                PtObj->TryGetNumberField(TEXT("z"), Z);
                P.X = static_cast<float>(X);
                P.Y = static_cast<float>(Y);
                P.Z = static_cast<float>(Z);
                S.ControlPointsCm.Add(P);
            }
        }

        OutSplines.Add(S);
    }
}

void ParseTargetOverrideArray(const TSharedPtr<FJsonObject>& Root, TArray<FRshipFieldTargetDesc>& OutTargets)
{
    OutTargets.Reset();
    if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("targetOverrides")))
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>& Targets = Root->GetArrayField(TEXT("targetOverrides"));
    OutTargets.Reserve(Targets.Num());

    for (int32 Index = 0; Index < Targets.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Obj = Targets[Index].IsValid() ? Targets[Index]->AsObject() : nullptr;
        if (!Obj.IsValid())
        {
            continue;
        }

        FRshipFieldTargetDesc T;
        FString GuidString;
        if (Obj->TryGetStringField(TEXT("stableGuid"), GuidString))
        {
            FGuid::Parse(GuidString, T.StableGuid);
        }
        Obj->TryGetStringField(TEXT("visibleTargetPath"), T.VisibleTargetPath);
        TryGetNumber(Obj, TEXT("axisWeightNormal"), T.AxisWeightNormal);
        TryGetNumber(Obj, TEXT("axisWeightWorld"), T.AxisWeightWorld);
        TryGetNumber(Obj, TEXT("axisWeightObject"), T.AxisWeightObject);
        TryGetVector3(Obj, TEXT("worldAxis"), T.WorldAxis);
        TryGetVector3(Obj, TEXT("objectAxis"), T.ObjectAxis);
        TryGetNumber(Obj, TEXT("scalarGain"), T.ScalarGain);
        TryGetNumber(Obj, TEXT("vectorGain"), T.VectorGain);
        TryGetNumber(Obj, TEXT("maxDisplacementCm"), T.MaxDisplacementCm);

        OutTargets.Add(T);
    }
}
} // namespace

void URshipFieldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    GlobalParams.UpdateHz = 60.0f;
    GlobalParams.MasterScalarGain = 1.0f;
    GlobalParams.MasterVectorGain = 1.0f;
    GlobalParams.FieldResolution = 256;
    GlobalParams.DomainCenterCm = FVector::ZeroVector;
    GlobalParams.DomainSizeCm = 10000.0f;

    TransportState.Bpm = 120.0f;
    TransportState.BeatPhase = 0.0f;
    TransportState.bPlaying = true;

    PendingPackets.Reset();
    LastAppliedSequence = 0;
    SimulationFrame = 0;
    SimulationTimeSeconds = 0.0f;
    TickAccumulator = 0.0f;
    LastDispatchedTargetCount = 0;
    LastActiveEmitterCount = 0;
    LastQueuedPacketCount = 0;
}

void URshipFieldSubsystem::Deinitialize()
{
    RegisteredTargets.Reset();
    PendingPackets.Reset();
    ActiveSplineCompiledEmitters.Reset();
    GlobalScalarFieldAtlas = nullptr;
    GlobalVectorFieldAtlas = nullptr;
    LastDispatchedTargetCount = 0;
    LastActiveEmitterCount = 0;
    LastQueuedPacketCount = 0;
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

void URshipFieldSubsystem::Tick(float DeltaTime)
{
    const float Step = 1.0f / FMath::Max(GlobalParams.UpdateHz, 1.0f);
    TickAccumulator += DeltaTime;

    bool bDidStep = false;
    while (TickAccumulator + KINDA_SMALL_NUMBER >= Step)
    {
        TickAccumulator -= Step;
        ++SimulationFrame;
        SimulationTimeSeconds += Step;
        ApplyQueuedPackets();
        if (TransportState.bPlaying)
        {
            TransportState.BeatPhase = FMath::Fmod(TransportState.BeatPhase + (Step * TransportState.Bpm / 60.0f), 1.0f);
            if (TransportState.BeatPhase < 0.0f)
            {
                TransportState.BeatPhase += 1.0f;
            }
        }
        bDidStep = true;
    }

    if (bDidStep)
    {
        DispatchFieldPasses();
    }
}

bool URshipFieldSubsystem::ParsePacket(const FString& PacketJson, FRshipFieldPacket& OutPacket, FString& OutError) const
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PacketJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Invalid packet JSON.");
        return false;
    }

    if (!Root->HasTypedField<EJson::Number>(TEXT("schemaVersion")))
    {
        OutError = TEXT("Missing required field: schemaVersion.");
        return false;
    }

    if (!Root->HasTypedField<EJson::Number>(TEXT("sequence")))
    {
        OutError = TEXT("Missing required field: sequence.");
        return false;
    }

    const int32 SchemaVersion = Root->GetIntegerField(TEXT("schemaVersion"));
    if (SchemaVersion != 1)
    {
        OutError = FString::Printf(TEXT("Unsupported schemaVersion: %d"), SchemaVersion);
        return false;
    }

    OutPacket.SchemaVersion = SchemaVersion;

    const double SequenceDouble = Root->GetNumberField(TEXT("sequence"));
    if (SequenceDouble < 0.0)
    {
        OutError = TEXT("sequence must be non-negative.");
        return false;
    }
    if (SequenceDouble > static_cast<double>(MAX_int64))
    {
        OutError = TEXT("sequence exceeds int64 range.");
        return false;
    }
    OutPacket.Sequence = static_cast<int64>(FMath::FloorToDouble(SequenceDouble));

    if (Root->HasTypedField<EJson::Number>(TEXT("applyFrame")))
    {
        OutPacket.bHasApplyFrame = true;
        OutPacket.ApplyFrame = static_cast<int64>(FMath::FloorToDouble(Root->GetNumberField(TEXT("applyFrame"))));
    }

    if (Root->HasTypedField<EJson::Object>(TEXT("transport")))
    {
        const TSharedPtr<FJsonObject> TransportObj = Root->GetObjectField(TEXT("transport"));
        TryGetNumber(TransportObj, TEXT("bpm"), OutPacket.Transport.Bpm);
        TryGetNumber(TransportObj, TEXT("phase"), OutPacket.Transport.BeatPhase);
        TransportObj->TryGetBoolField(TEXT("playing"), OutPacket.Transport.bPlaying);
    }

    if (Root->HasTypedField<EJson::Object>(TEXT("globals")))
    {
        const TSharedPtr<FJsonObject> GlobalsObj = Root->GetObjectField(TEXT("globals"));
        TryGetNumber(GlobalsObj, TEXT("updateHz"), OutPacket.Globals.UpdateHz);
        TryGetNumber(GlobalsObj, TEXT("masterScalarGain"), OutPacket.Globals.MasterScalarGain);
        TryGetNumber(GlobalsObj, TEXT("masterVectorGain"), OutPacket.Globals.MasterVectorGain);
        TryGetNumber(GlobalsObj, TEXT("domainSizeCm"), OutPacket.Globals.DomainSizeCm);

        int32 Resolution = OutPacket.Globals.FieldResolution;
        TryGetNumber(GlobalsObj, TEXT("fieldResolution"), Resolution);
        OutPacket.Globals.FieldResolution = Resolution;

        FVector DomainCenter = OutPacket.Globals.DomainCenterCm;
        if (TryGetVector3(GlobalsObj, TEXT("domainCenter"), DomainCenter))
        {
            OutPacket.Globals.DomainCenterCm = DomainCenter;
        }
    }

    ParseLayerArray(Root, OutPacket.Layers);
    ParsePhaseGroupArray(Root, OutPacket.PhaseGroups);
    ParseEmitterArray(Root, OutPacket.Emitters);
    ParseSplineArray(Root, OutPacket.Splines);
    ParseTargetOverrideArray(Root, OutPacket.TargetOverrides);

    return true;
}

void URshipFieldSubsystem::NormalizePacket(FRshipFieldPacket& Packet) const
{
    Packet.Transport.Bpm = FMath::Clamp(Packet.Transport.Bpm, 1.0f, 400.0f);
    Packet.Transport.BeatPhase = FMath::Fmod(Packet.Transport.BeatPhase, 1.0f);
    if (Packet.Transport.BeatPhase < 0.0f)
    {
        Packet.Transport.BeatPhase += 1.0f;
    }

    Packet.Globals.UpdateHz = FMath::Clamp(Packet.Globals.UpdateHz, 1.0f, 240.0f);
    Packet.Globals.MasterScalarGain = QuantizeFloat(Packet.Globals.MasterScalarGain, 0.0001f);
    Packet.Globals.MasterVectorGain = QuantizeFloat(Packet.Globals.MasterVectorGain, 0.0001f);
    Packet.Globals.DomainSizeCm = FMath::Max(Packet.Globals.DomainSizeCm, 1.0f);
    Packet.Globals.FieldResolution = NormalizeResolution(Packet.Globals.FieldResolution);

    auto NormalizeClamp = [](float& MinValue, float& MaxValue)
    {
        if (MinValue > MaxValue)
        {
            Swap(MinValue, MaxValue);
        }
    };

    for (FRshipFieldLayerDesc& Layer : Packet.Layers)
    {
        Layer.Weight = QuantizeFloat(Layer.Weight, 0.0001f);
        NormalizeClamp(Layer.ClampMin, Layer.ClampMax);
    }

    for (FRshipFieldPhaseGroupDesc& Group : Packet.PhaseGroups)
    {
        Group.TempoMultiplier = FMath::Clamp(Group.TempoMultiplier, 0.0f, 32.0f);
        Group.PhaseOffset = QuantizeFloat(Group.PhaseOffset, 0.0001f);
    }

    for (FRshipFieldEmitterDesc& E : Packet.Emitters)
    {
        E.Amplitude = QuantizeFloat(E.Amplitude, 0.0001f);
        E.WavelengthCm = FMath::Max(E.WavelengthCm, 0.001f);
        E.FrequencyHz = FMath::Max(E.FrequencyHz, 0.0f);
        E.Speed = QuantizeFloat(E.Speed, 0.0001f);
        E.FadeWeight = FMath::Clamp(E.FadeWeight, 0.0f, 1.0f);
        E.RadiusCm = FMath::Max(E.RadiusCm, 0.0f);
        E.NoiseScale = FMath::Max(E.NoiseScale, 0.0f);
        E.EnvelopeAttackSeconds = FMath::Max(E.EnvelopeAttackSeconds, 0.0f);
        E.EnvelopeDecaySeconds = FMath::Max(E.EnvelopeDecaySeconds, 0.0f);
        NormalizeClamp(E.ClampMin, E.ClampMax);
    }

    for (FRshipFieldSplineEmitterDesc& S : Packet.Splines)
    {
        S.CurvatureToleranceCm = FMath::Max(S.CurvatureToleranceCm, 0.01f);
        S.RadiusCm = FMath::Max(S.RadiusCm, 0.0f);
        NormalizeClamp(S.ClampMin, S.ClampMax);
    }

    for (FRshipFieldTargetDesc& T : Packet.TargetOverrides)
    {
        T.AxisWeightNormal = FMath::Clamp(T.AxisWeightNormal, 0.0f, 1.0f);
        T.AxisWeightWorld = FMath::Clamp(T.AxisWeightWorld, 0.0f, 1.0f);
        T.AxisWeightObject = FMath::Clamp(T.AxisWeightObject, 0.0f, 1.0f);
        T.MaxDisplacementCm = FMath::Max(T.MaxDisplacementCm, 0.0f);
    }

    Packet.Layers.Sort([](const FRshipFieldLayerDesc& A, const FRshipFieldLayerDesc& B) { return A.Id < B.Id; });
    Packet.PhaseGroups.Sort([](const FRshipFieldPhaseGroupDesc& A, const FRshipFieldPhaseGroupDesc& B) { return A.Id < B.Id; });
    Packet.Emitters.Sort([](const FRshipFieldEmitterDesc& A, const FRshipFieldEmitterDesc& B) { return A.Id < B.Id; });
    Packet.Splines.Sort([](const FRshipFieldSplineEmitterDesc& A, const FRshipFieldSplineEmitterDesc& B) { return A.Id < B.Id; });
    Packet.TargetOverrides.Sort([](const FRshipFieldTargetDesc& A, const FRshipFieldTargetDesc& B)
    {
        if (A.StableGuid != B.StableGuid)
        {
            return A.StableGuid.ToString() < B.StableGuid.ToString();
        }
        return A.VisibleTargetPath < B.VisibleTargetPath;
    });
}

bool URshipFieldSubsystem::EnqueuePacketJson(const FString& PacketJson, FString* OutError)
{
    if (PacketJson.Len() > MaxPacketChars)
    {
        const FString SizeError = FString::Printf(TEXT("Packet too large (%d chars, max %d)."), PacketJson.Len(), MaxPacketChars);
        if (OutError)
        {
            *OutError = SizeError;
        }
        UE_LOG(LogRshipField, Warning, TEXT("Packet rejected: %s"), *SizeError);
        return false;
    }

    FRshipFieldPacket Parsed;
    FString ParseError;
    if (!ParsePacket(PacketJson, Parsed, ParseError))
    {
        if (OutError)
        {
            *OutError = ParseError;
        }
        UE_LOG(LogRshipField, Warning, TEXT("Packet rejected: %s"), *ParseError);
        return false;
    }

    NormalizePacket(Parsed);

    if (Parsed.Sequence <= LastAppliedSequence)
    {
        const FString Error = FString::Printf(TEXT("Stale sequence %lld <= last applied %lld."), Parsed.Sequence, LastAppliedSequence);
        if (OutError)
        {
            *OutError = Error;
        }
        return false;
    }

    FQueuedPacket Queued;
    Queued.Sequence = Parsed.Sequence;
    Queued.Packet = MoveTemp(Parsed);
    Queued.ApplyFrame = Queued.Packet.bHasApplyFrame
        ? FMath::Max<int64>(Queued.Packet.ApplyFrame, SimulationFrame + 1)
        : (SimulationFrame + 1);

    PendingPackets.Add(MoveTemp(Queued));
    PendingPackets.Sort([](const FQueuedPacket& A, const FQueuedPacket& B)
    {
        if (A.ApplyFrame != B.ApplyFrame)
        {
            return A.ApplyFrame < B.ApplyFrame;
        }
        return A.Sequence < B.Sequence;
    });

    if (PendingPackets.Num() > MaxQueuedPackets)
    {
        const int32 DropCount = PendingPackets.Num() - MaxQueuedPackets;
        PendingPackets.RemoveAt(0, DropCount, EAllowShrinking::No);
        UE_LOG(LogRshipField, Warning, TEXT("Packet queue exceeded max (%d), dropped %d oldest packets."), MaxQueuedPackets, DropCount);
    }
    LastQueuedPacketCount = PendingPackets.Num();

    if (OutError)
    {
        OutError->Reset();
    }

    return true;
}

void URshipFieldSubsystem::ApplyPacketState(const FRshipFieldPacket& Packet)
{
    TransportState = Packet.Transport;
    GlobalParams.UpdateHz = Packet.Globals.UpdateHz;
    GlobalParams.MasterScalarGain = Packet.Globals.MasterScalarGain;
    GlobalParams.MasterVectorGain = Packet.Globals.MasterVectorGain;
    GlobalParams.FieldResolution = NormalizeResolution(Packet.Globals.FieldResolution);
    GlobalParams.DomainCenterCm = Packet.Globals.DomainCenterCm;
    GlobalParams.DomainSizeCm = Packet.Globals.DomainSizeCm;

    ActiveLayers = Packet.Layers;
    ActivePhaseGroups = Packet.PhaseGroups;
    ActiveEmitters = Packet.Emitters;
    ActiveSplines = Packet.Splines;
    ActiveTargetOverrides = Packet.TargetOverrides;
    RebuildCompiledSplineEmitters();
}

void URshipFieldSubsystem::RebuildCompiledSplineEmitters()
{
    ActiveSplineCompiledEmitters.Reset();

    for (const FRshipFieldSplineEmitterDesc& Spline : ActiveSplines)
    {
        if (!Spline.bEnabled || Spline.ControlPointsCm.Num() < 2)
        {
            continue;
        }

        const float ToleranceCm = FMath::Max(Spline.CurvatureToleranceCm, 0.01f);
        const int32 LastPointIndex = Spline.ControlPointsCm.Num() - 1;

        for (int32 SegmentIndex = 0; SegmentIndex < LastPointIndex; ++SegmentIndex)
        {
            const FVector P0 = Spline.ControlPointsCm[SegmentIndex];
            const FVector P1 = Spline.ControlPointsCm[SegmentIndex + 1];
            const FVector Segment = P1 - P0;
            const float SegmentLen = Segment.Size();
            if (SegmentLen <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            FVector PrevDir = FVector::ZeroVector;
            FVector NextDir = FVector::ZeroVector;
            if (SegmentIndex > 0)
            {
                PrevDir = (P0 - Spline.ControlPointsCm[SegmentIndex - 1]).GetSafeNormal();
            }
            if (SegmentIndex + 2 <= LastPointIndex)
            {
                NextDir = (Spline.ControlPointsCm[SegmentIndex + 2] - P1).GetSafeNormal();
            }

            float CurvatureFactor = 1.0f;
            if (!PrevDir.IsNearlyZero() && !NextDir.IsNearlyZero())
            {
                const float Dot = FVector::DotProduct(PrevDir, NextDir);
                CurvatureFactor = 1.0f + (1.0f - Dot) * 8.0f;
            }

            const float StepCm = FMath::Max(0.5f, ToleranceCm / CurvatureFactor);
            const int32 NumSamples = FMath::Max(1, FMath::CeilToInt(SegmentLen / StepCm));

            const int32 FirstSampleIndex = (SegmentIndex == 0) ? 0 : 1;
            for (int32 SampleIndex = FirstSampleIndex; SampleIndex <= NumSamples; ++SampleIndex)
            {
                const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(NumSamples);
                FRshipFieldEmitterDesc Em;
                Em.Id = FString::Printf(TEXT("%s_seg_%04d_s_%04d"), *Spline.Id, SegmentIndex, SampleIndex);
                Em.LayerId = Spline.LayerId;
                Em.bEnabled = true;
                Em.bInfiniteRange = false;
                Em.PositionCm = FMath::Lerp(P0, P1, Alpha);
                Em.Direction = Segment.GetSafeNormal();
                Em.RadiusCm = Spline.RadiusCm;
                Em.Amplitude = Spline.Amplitude;
                Em.WavelengthCm = FMath::Max(4.0f * ToleranceCm, 1.0f);
                Em.FrequencyHz = 1.0f;
                Em.Speed = 1.0f;
                Em.PhaseOffset = Alpha;
                Em.FadeWeight = 1.0f;
                Em.EnvelopeAttackSeconds = 0.1f;
                Em.EnvelopeDecaySeconds = 0.1f;
                Em.Waveform = Spline.Waveform;
                Em.BlendOp = Spline.BlendOp;
                Em.NoiseType = ERshipFieldNoiseType::None;
                Em.NoiseScale = 0.0f;
                Em.NoiseAmplitude = 0.0f;
                Em.bAffectsScalar = true;
                Em.bAffectsVector = true;
                Em.ClampMin = Spline.ClampMin;
                Em.ClampMax = Spline.ClampMax;
                ActiveSplineCompiledEmitters.Add(MoveTemp(Em));
            }
        }
    }

    ActiveSplineCompiledEmitters.Sort([](const FRshipFieldEmitterDesc& A, const FRshipFieldEmitterDesc& B)
    {
        return A.Id < B.Id;
    });
}

void URshipFieldSubsystem::ApplyQueuedPackets()
{
    while (PendingPackets.Num() > 0)
    {
        const FQueuedPacket& Head = PendingPackets[0];
        if (Head.ApplyFrame > SimulationFrame)
        {
            break;
        }

        if (Head.Sequence > LastAppliedSequence)
        {
            ApplyPacketState(Head.Packet);
            LastAppliedSequence = Head.Sequence;
        }

        PendingPackets.RemoveAt(0, 1, EAllowShrinking::No);
    }

    LastQueuedPacketCount = PendingPackets.Num();
}

int32 URshipFieldSubsystem::NormalizeResolution(const int32 RequestedResolution) const
{
    const int32 Allowed[] = { 128, 192, 256, 320 };
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

const FRshipFieldTargetDesc* URshipFieldSubsystem::FindTargetOverride(const URshipFieldTargetComponent* Target) const
{
    if (!Target)
    {
        return nullptr;
    }

    if (Target->StableGuid.IsValid())
    {
        for (const FRshipFieldTargetDesc& Candidate : ActiveTargetOverrides)
        {
            if (Candidate.StableGuid.IsValid() && Candidate.StableGuid == Target->StableGuid)
            {
                return &Candidate;
            }
        }
    }

    if (!Target->VisibleTargetPath.IsEmpty())
    {
        for (const FRshipFieldTargetDesc& Candidate : ActiveTargetOverrides)
        {
            if (!Candidate.VisibleTargetPath.IsEmpty() && Candidate.VisibleTargetPath == Target->VisibleTargetPath)
            {
                return &Candidate;
            }
        }
    }

    return nullptr;
}

FRshipFieldTargetDesc URshipFieldSubsystem::BuildResolvedTargetDesc(const URshipFieldTargetComponent* Target) const
{
    FRshipFieldTargetDesc Result;
    if (!Target)
    {
        return Result;
    }

    Result.StableGuid = Target->StableGuid;
    Result.VisibleTargetPath = Target->VisibleTargetPath;
    Result.AxisWeightNormal = Target->AxisWeightNormal;
    Result.AxisWeightWorld = Target->AxisWeightWorld;
    Result.AxisWeightObject = Target->AxisWeightObject;
    Result.WorldAxis = Target->WorldAxis;
    Result.ObjectAxis = Target->ObjectAxis;
    Result.ScalarGain = Target->ScalarGain;
    Result.VectorGain = Target->VectorGain;
    Result.MaxDisplacementCm = Target->MaxDisplacementCm;

    if (const FRshipFieldTargetDesc* Override = FindTargetOverride(Target))
    {
        Result.AxisWeightNormal = Override->AxisWeightNormal;
        Result.AxisWeightWorld = Override->AxisWeightWorld;
        Result.AxisWeightObject = Override->AxisWeightObject;
        Result.WorldAxis = Override->WorldAxis;
        Result.ObjectAxis = Override->ObjectAxis;
        Result.ScalarGain = Override->ScalarGain;
        Result.VectorGain = Override->VectorGain;
        Result.MaxDisplacementCm = Override->MaxDisplacementCm;
    }

    NormalizeAxisWeights(Result.AxisWeightNormal, Result.AxisWeightWorld, Result.AxisWeightObject);
    Result.WorldAxis = Result.WorldAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);
    Result.ObjectAxis = Result.ObjectAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);
    Result.MaxDisplacementCm = FMath::Max(Result.MaxDisplacementCm, 0.0f);
    return Result;
}

bool URshipFieldSubsystem::EnsureGlobalAtlasTextures()
{
    const int32 Resolution = NormalizeResolution(GlobalParams.FieldResolution);
    const int32 TilesPerRow = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Resolution))));
    const int32 AtlasDim = TilesPerRow * Resolution;

    auto EnsureTarget = [AtlasDim](TObjectPtr<UTextureRenderTarget2D>& Texture, ETextureRenderTargetFormat Format, const TCHAR* Name) -> bool
    {
        if (!Texture)
        {
            Texture = NewObject<UTextureRenderTarget2D>(GetTransientPackageAsObject(), FName(Name), RF_Transient);
        }

        if (!Texture)
        {
            return false;
        }

        const bool bNeedsInit = Texture->SizeX != AtlasDim || Texture->SizeY != AtlasDim || Texture->RenderTargetFormat != Format || !Texture->bCanCreateUAV;
        if (bNeedsInit)
        {
            Texture->RenderTargetFormat = Format;
            Texture->bCanCreateUAV = true;
            Texture->bAutoGenerateMips = false;
            Texture->AddressX = TA_Clamp;
            Texture->AddressY = TA_Clamp;
            Texture->InitAutoFormat(AtlasDim, AtlasDim);
            Texture->ClearColor = FLinearColor::Black;
            Texture->UpdateResourceImmediate(true);
        }

        return true;
    };

    if (!EnsureTarget(GlobalScalarFieldAtlas, RTF_R16f, TEXT("RshipField_GlobalScalarAtlas")))
    {
        return false;
    }

    if (!EnsureTarget(GlobalVectorFieldAtlas, RTF_RGBA16f, TEXT("RshipField_GlobalVectorAtlas")))
    {
        return false;
    }

    return true;
}

void URshipFieldSubsystem::DispatchFieldPasses()
{
    if (!EnsureGlobalAtlasTextures())
    {
        LastDispatchedTargetCount = 0;
        UE_LOG(LogRshipField, Warning, TEXT("Global field textures could not be created."));
        return;
    }

    FTextureRenderTargetResource* ScalarResource = GlobalScalarFieldAtlas ? GlobalScalarFieldAtlas->GameThread_GetRenderTargetResource() : nullptr;
    FTextureRenderTargetResource* VectorResource = GlobalVectorFieldAtlas ? GlobalVectorFieldAtlas->GameThread_GetRenderTargetResource() : nullptr;

    const FVector DomainHalfExtent = FVector(GlobalParams.DomainSizeCm * 0.5f);
    const FVector DomainMin = GlobalParams.DomainCenterCm - DomainHalfExtent;
    const FVector DomainMax = GlobalParams.DomainCenterCm + DomainHalfExtent;

    RshipFieldRDG::FGlobalDispatchInputs GlobalInputs;
    GlobalInputs.FieldResolution = NormalizeResolution(GlobalParams.FieldResolution);
    GlobalInputs.TilesPerRow = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(GlobalInputs.FieldResolution))));
    GlobalInputs.TimeSeconds = SimulationTimeSeconds;
    GlobalInputs.BPM = TransportState.Bpm;
    GlobalInputs.TransportPhase = TransportState.BeatPhase;
    GlobalInputs.MasterScalarGain = GlobalParams.MasterScalarGain;
    GlobalInputs.MasterVectorGain = GlobalParams.MasterVectorGain;
    GlobalInputs.DomainMinCm = FVector3f(DomainMin);
    GlobalInputs.DomainMaxCm = FVector3f(DomainMax);
    GlobalInputs.bDebugEnabled = bDebugEnabled;
    GlobalInputs.DebugMode = static_cast<int32>(DebugMode);
    GlobalInputs.OutScalarFieldAtlasTexture = ScalarResource ? ScalarResource->GetRenderTargetTexture() : nullptr;
    GlobalInputs.OutVectorFieldAtlasTexture = VectorResource ? VectorResource->GetRenderTargetTexture() : nullptr;

    TArray<FRshipFieldPhaseGroupDesc> PhaseGroups = ActivePhaseGroups;
    if (PhaseGroups.Num() == 0)
    {
        FRshipFieldPhaseGroupDesc DefaultPhaseGroup;
        DefaultPhaseGroup.Id = TEXT("default");
        PhaseGroups.Add(DefaultPhaseGroup);
    }
    PhaseGroups.Sort([](const FRshipFieldPhaseGroupDesc& A, const FRshipFieldPhaseGroupDesc& B) { return A.Id < B.Id; });

    TMap<FString, int32> PhaseGroupIndexById;
    PhaseGroupIndexById.Reserve(PhaseGroups.Num());
    GlobalInputs.PhaseGroupData.Reserve(PhaseGroups.Num());
    for (int32 GroupIndex = 0; GroupIndex < PhaseGroups.Num(); ++GroupIndex)
    {
        const FRshipFieldPhaseGroupDesc& Group = PhaseGroups[GroupIndex];
        PhaseGroupIndexById.Add(Group.Id, GroupIndex);
        GlobalInputs.PhaseGroupData.Add(FVector4f(
            Group.bSyncToTempo ? 1.0f : 0.0f,
            Group.TempoMultiplier,
            Group.PhaseOffset,
            0.0f));
    }

    TArray<FRshipFieldLayerDesc> Layers = ActiveLayers;
    if (Layers.Num() == 0)
    {
        FRshipFieldLayerDesc DefaultLayer;
        DefaultLayer.Id = TEXT("default");
        Layers.Add(DefaultLayer);
    }
    Layers.Sort([](const FRshipFieldLayerDesc& A, const FRshipFieldLayerDesc& B) { return A.Id < B.Id; });

    TMap<FString, int32> LayerIndexById;
    LayerIndexById.Reserve(Layers.Num());
    TArray<int32> LayerPhaseGroupIndices;
    LayerPhaseGroupIndices.Reserve(Layers.Num());
    GlobalInputs.LayerDataA.Reserve(Layers.Num());
    GlobalInputs.LayerDataB.Reserve(Layers.Num());
    for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
    {
        const FRshipFieldLayerDesc& Layer = Layers[LayerIndex];
        LayerIndexById.Add(Layer.Id, LayerIndex);

        const int32* PhaseGroupIndexPtr = !Layer.PhaseGroupId.IsEmpty() ? PhaseGroupIndexById.Find(Layer.PhaseGroupId) : nullptr;
        const int32 PhaseGroupIndex = PhaseGroupIndexPtr ? *PhaseGroupIndexPtr : INDEX_NONE;
        LayerPhaseGroupIndices.Add(PhaseGroupIndex);

        GlobalInputs.LayerDataA.Add(FVector4f(
            Layer.Weight,
            Layer.ClampMin,
            Layer.ClampMax,
            static_cast<float>(static_cast<uint8>(Layer.BlendOp))));
        GlobalInputs.LayerDataB.Add(FVector4f(
            Layer.bEnabled ? 1.0f : 0.0f,
            static_cast<float>(PhaseGroupIndex),
            0.0f,
            0.0f));
    }

    TArray<FRshipFieldEmitterDesc> EffectiveEmitters = ActiveEmitters;
    EffectiveEmitters.Append(ActiveSplineCompiledEmitters);
    EffectiveEmitters.Sort([](const FRshipFieldEmitterDesc& A, const FRshipFieldEmitterDesc& B) { return A.Id < B.Id; });

    GlobalInputs.EmitterData0.Reserve(EffectiveEmitters.Num());
    GlobalInputs.EmitterData1.Reserve(EffectiveEmitters.Num());
    GlobalInputs.EmitterData2.Reserve(EffectiveEmitters.Num());
    GlobalInputs.EmitterData3.Reserve(EffectiveEmitters.Num());
    GlobalInputs.EmitterData4.Reserve(EffectiveEmitters.Num());
    GlobalInputs.EmitterData5.Reserve(EffectiveEmitters.Num());
    GlobalInputs.EmitterData6.Reserve(EffectiveEmitters.Num());

    LastActiveEmitterCount = 0;
    for (const FRshipFieldEmitterDesc& Emitter : EffectiveEmitters)
    {
        const int32* LayerIndexPtr = !Emitter.LayerId.IsEmpty() ? LayerIndexById.Find(Emitter.LayerId) : nullptr;
        const int32 LayerIndex = LayerIndexPtr ? *LayerIndexPtr : 0;

        int32 PhaseGroupIndex = INDEX_NONE;
        if (!Emitter.PhaseGroupId.IsEmpty())
        {
            if (const int32* PhaseGroupIndexPtr = PhaseGroupIndexById.Find(Emitter.PhaseGroupId))
            {
                PhaseGroupIndex = *PhaseGroupIndexPtr;
            }
        }
        if (PhaseGroupIndex == INDEX_NONE && LayerPhaseGroupIndices.IsValidIndex(LayerIndex))
        {
            PhaseGroupIndex = LayerPhaseGroupIndices[LayerIndex];
        }

        GlobalInputs.EmitterData0.Add(FVector4f(FVector3f(Emitter.PositionCm), Emitter.RadiusCm));
        GlobalInputs.EmitterData1.Add(FVector4f(FVector3f(Emitter.Direction.GetSafeNormal()), Emitter.Amplitude));
        GlobalInputs.EmitterData2.Add(FVector4f(Emitter.WavelengthCm, Emitter.FrequencyHz, Emitter.Speed, Emitter.PhaseOffset));
        GlobalInputs.EmitterData3.Add(FVector4f(Emitter.FadeWeight, Emitter.EnvelopeAttackSeconds, Emitter.EnvelopeDecaySeconds, static_cast<float>(LayerIndex)));
        GlobalInputs.EmitterData4.Add(FVector4f(Emitter.ClampMin, Emitter.ClampMax, static_cast<float>(static_cast<uint8>(Emitter.BlendOp)), static_cast<float>(static_cast<uint8>(Emitter.Waveform))));
        GlobalInputs.EmitterData5.Add(FVector4f(static_cast<float>(PhaseGroupIndex), static_cast<float>(static_cast<uint8>(Emitter.NoiseType)), Emitter.NoiseScale, Emitter.NoiseAmplitude));
        GlobalInputs.EmitterData6.Add(FVector4f(
            Emitter.bEnabled ? 1.0f : 0.0f,
            Emitter.bInfiniteRange ? 1.0f : 0.0f,
            Emitter.bAffectsScalar ? 1.0f : 0.0f,
            Emitter.bAffectsVector ? 1.0f : 0.0f));

        if (Emitter.bEnabled)
        {
            ++LastActiveEmitterCount;
        }
    }

    GlobalInputs.LayerCount = Layers.Num();
    GlobalInputs.PhaseGroupCount = PhaseGroups.Num();
    GlobalInputs.EmitterCount = EffectiveEmitters.Num();

    auto ResolveSelectionIndex = [](const FString& Selection, const auto& Items) -> int32
    {
        if (Selection.IsEmpty())
        {
            return INDEX_NONE;
        }

        if (Selection.IsNumeric())
        {
            return FCString::Atoi(*Selection);
        }

        for (int32 Index = 0; Index < Items.Num(); ++Index)
        {
            if (Items[Index].Id == Selection)
            {
                return Index;
            }
        }
        return INDEX_NONE;
    };

    if (DebugMode == ERshipFieldDebugMode::IsolateEmitter)
    {
        GlobalInputs.DebugSelectionIndex = ResolveSelectionIndex(DebugSelection, EffectiveEmitters);
    }
    else if (DebugMode == ERshipFieldDebugMode::IsolateLayer)
    {
        GlobalInputs.DebugSelectionIndex = ResolveSelectionIndex(DebugSelection, Layers);
    }
    else
    {
        GlobalInputs.DebugSelectionIndex = INDEX_NONE;
    }

    TArray<RshipFieldRDG::FTargetDispatchInputs> TargetInputs;
    TargetInputs.Reserve(RegisteredTargets.Num());

    for (int32 Index = RegisteredTargets.Num() - 1; Index >= 0; --Index)
    {
        URshipFieldTargetComponent* Target = RegisteredTargets[Index];
        if (!Target)
        {
            RegisteredTargets.RemoveAtSwap(Index);
            continue;
        }

        FString Error;
        if (!Target->ValidateConfig(&Error))
        {
            UE_LOG(LogRshipField, Verbose, TEXT("Skipping target dispatch: %s"), *Error);
            continue;
        }

        FTextureResource* RestPosResource = Target->RestPositionTexture ? Target->RestPositionTexture->GetResource() : nullptr;
        FTextureResource* RestNormalResource = Target->RestNormalTexture ? Target->RestNormalTexture->GetResource() : nullptr;
        FTextureResource* MaskResource = Target->MaskTexture ? Target->MaskTexture->GetResource() : nullptr;
        FTextureRenderTargetResource* OutPosResource = Target->DeformedPositionRT ? Target->DeformedPositionRT->GameThread_GetRenderTargetResource() : nullptr;
        FTextureRenderTargetResource* OutNormalResource = Target->DeformedNormalRT ? Target->DeformedNormalRT->GameThread_GetRenderTargetResource() : nullptr;
        const FRshipFieldTargetDesc ResolvedTarget = BuildResolvedTargetDesc(Target);

        RshipFieldRDG::FTargetDispatchInputs Inputs;
        Inputs.GridSize = FIntPoint(Target->DeformedPositionRT->SizeX, Target->DeformedPositionRT->SizeY);
        Inputs.TimeSeconds = SimulationTimeSeconds;
        Inputs.ScalarGain = ResolvedTarget.ScalarGain;
        Inputs.VectorGain = ResolvedTarget.VectorGain;
        Inputs.AxisWeightNormal = ResolvedTarget.AxisWeightNormal;
        Inputs.AxisWeightWorld = ResolvedTarget.AxisWeightWorld;
        Inputs.AxisWeightObject = ResolvedTarget.AxisWeightObject;
        Inputs.MaxDisplacementCm = ResolvedTarget.MaxDisplacementCm;
        Inputs.DomainMinCm = FVector3f(DomainMin);
        Inputs.DomainMaxCm = FVector3f(DomainMax);
        Inputs.WorldAxis = FVector3f(ResolvedTarget.WorldAxis);
        Inputs.ObjectAxis = FVector3f(ResolvedTarget.ObjectAxis);
        Inputs.bAsyncCompute = Target->bUseAsyncCompute;
        Inputs.RestPositionTexture = RestPosResource ? RestPosResource->TextureRHI : nullptr;
        Inputs.RestNormalTexture = RestNormalResource ? RestNormalResource->TextureRHI : nullptr;
        Inputs.MaskTexture = MaskResource ? MaskResource->TextureRHI : nullptr;
        Inputs.OutDeformedPositionTexture = OutPosResource ? OutPosResource->GetRenderTargetTexture() : nullptr;
        Inputs.OutDeformedNormalTexture = OutNormalResource ? OutNormalResource->GetRenderTargetTexture() : nullptr;

        if (!Inputs.IsValid())
        {
            continue;
        }

        Target->UpdateMaterialBindings(GlobalScalarFieldAtlas, GlobalVectorFieldAtlas);
        TargetInputs.Add(Inputs);
    }

    LastDispatchedTargetCount = TargetInputs.Num();

    if (!GlobalInputs.IsValid())
    {
        LastDispatchedTargetCount = 0;
        UE_LOG(LogRshipField, Verbose, TEXT("Global dispatch inputs are not ready."));
        return;
    }

    ENQUEUE_RENDER_COMMAND(RshipFieldDispatch)(
        [GlobalInputs, TargetInputs](FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList);
            RshipFieldRDG::AddFieldPasses(GraphBuilder, GlobalInputs, TargetInputs);
            GraphBuilder.Execute();
        });
}

void URshipFieldSubsystem::RegisterTarget(URshipFieldTargetComponent* Target)
{
    if (!Target)
    {
        return;
    }

    RegisteredTargets.AddUnique(Target);
    Target->UpdateMaterialBindings(GlobalScalarFieldAtlas, GlobalVectorFieldAtlas);
}

void URshipFieldSubsystem::UnregisterTarget(URshipFieldTargetComponent* Target)
{
    RegisteredTargets.Remove(Target);
}

void URshipFieldSubsystem::SetUpdateHz(const float InHz)
{
    GlobalParams.UpdateHz = FMath::Clamp(InHz, 1.0f, 240.0f);
}

void URshipFieldSubsystem::SetBpm(const float InBpm)
{
    TransportState.Bpm = FMath::Clamp(InBpm, 1.0f, 400.0f);
}

void URshipFieldSubsystem::SetTransport(const float InPhase, const bool bInPlaying)
{
    TransportState.BeatPhase = FMath::Fmod(InPhase, 1.0f);
    if (TransportState.BeatPhase < 0.0f)
    {
        TransportState.BeatPhase += 1.0f;
    }
    TransportState.bPlaying = bInPlaying;
}

void URshipFieldSubsystem::SetMasterScalarGain(const float InGain)
{
    GlobalParams.MasterScalarGain = InGain;
}

void URshipFieldSubsystem::SetMasterVectorGain(const float InGain)
{
    GlobalParams.MasterVectorGain = InGain;
}

void URshipFieldSubsystem::SetDebugEnabled(const bool bEnabled)
{
    bDebugEnabled = bEnabled;
}

void URshipFieldSubsystem::SetDebugMode(const ERshipFieldDebugMode InMode)
{
    DebugMode = InMode;
}

void URshipFieldSubsystem::SetDebugSlice(const FString& InAxis, const float InPosition01)
{
    DebugSliceAxis = InAxis.ToLower();
    DebugSlicePosition01 = FMath::Clamp(InPosition01, 0.0f, 1.0f);
}

void URshipFieldSubsystem::SetDebugSelection(const FString& InSelectionId)
{
    DebugSelection = InSelectionId;
}

FRshipFieldTargetIdentity URshipFieldSubsystem::BuildIdentityForTarget(const URshipFieldTargetComponent* Target) const
{
    if (!Target)
    {
        return FRshipFieldTargetIdentity();
    }

    return Target->BuildIdentity();
}
