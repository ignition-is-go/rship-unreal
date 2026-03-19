#include "RshipFieldTypes.h"

FRshipFieldEffectorDesc FRshipFieldEffectorDesc::FromWave(const FRshipFieldWaveEffector& Wave)
{
    FRshipFieldEffectorDesc Desc;
    Desc.Type = ERshipFieldEffectorType::Wave;
    Desc.bEnabled = Wave.bEnabled;
    Desc.bInfiniteRange = Wave.bInfiniteRange;
    Desc.PositionCm = Wave.PositionCm;
    Desc.Direction = Wave.Direction;
    Desc.RadiusCm = Wave.RadiusCm;
    Desc.FalloffExponent = Wave.FalloffExponent;
    Desc.Amplitude = Wave.Amplitude;
    Desc.WavelengthCm = Wave.WavelengthCm;
    Desc.FrequencyHz = Wave.FrequencyHz;
    Desc.PhaseOffset = Wave.PhaseOffset;
    Desc.Waveform = Wave.Waveform;
    Desc.PhaseGroupId = Wave.PhaseGroupId;
    return Desc;
}

FRshipFieldEffectorDesc FRshipFieldEffectorDesc::FromNoise(const FRshipFieldNoiseEffector& Noise)
{
    FRshipFieldEffectorDesc Desc;
    Desc.Type = ERshipFieldEffectorType::Noise;
    Desc.bEnabled = Noise.bEnabled;
    Desc.bInfiniteRange = Noise.bInfiniteRange;
    Desc.PositionCm = Noise.PositionCm;
    Desc.RadiusCm = Noise.RadiusCm;
    Desc.Amplitude = 0.0f;
    Desc.NoiseMode = Noise.NoiseMode;
    Desc.NoiseScale = Noise.Scale;
    Desc.NoiseAmplitude = Noise.Amplitude;
    Desc.PhaseGroupId = Noise.PhaseGroupId;
    return Desc;
}

FRshipFieldEffectorDesc FRshipFieldEffectorDesc::FromAttractor(const FRshipFieldAttractorEffector& Attractor)
{
    FRshipFieldEffectorDesc Desc;
    Desc.Type = ERshipFieldEffectorType::Attractor;
    Desc.bEnabled = Attractor.bEnabled;
    Desc.bInfiniteRange = Attractor.bInfiniteRange;
    Desc.PositionCm = Attractor.PositionCm;
    Desc.RadiusCm = Attractor.RadiusCm;
    Desc.Amplitude = Attractor.Strength;
    Desc.FalloffExponent = Attractor.FalloffExponent;
    Desc.bAffectsVector = true;
    Desc.bAffectsScalar = true;
    Desc.PhaseGroupId = Attractor.PhaseGroupId;
    return Desc;
}
