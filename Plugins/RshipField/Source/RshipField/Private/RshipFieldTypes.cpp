#include "RshipFieldTypes.h"

FRshipFieldEffectorDesc FRshipFieldEffectorDesc::FromWave(const FRshipFieldWaveEffector& Wave)
{
    FRshipFieldEffectorDesc Desc;
    Desc.Type = ERshipFieldEffectorType::Wave;
    Desc.bEnabled = Wave.bEnabled;
    Desc.bInfiniteRange = Wave.bInfiniteRange;
    Desc.PositionCm = Wave.PositionCm;
    Desc.Polarization = Wave.Polarization;
    Desc.RadiusCm = Wave.RadiusCm;
    Desc.FalloffExponent = Wave.FalloffExponent;
    Desc.Amplitude = Wave.Amplitude;
    Desc.WavelengthCm = Wave.WavelengthCm;
    Desc.FrequencyHz = Wave.FrequencyHz;
    Desc.PhaseOffset = Wave.PhaseOffset;
    Desc.Waveform = Wave.Waveform;
    Desc.WaveMode = Wave.WaveMode;
    Desc.EnvelopeWidthCm = Wave.EnvelopeWidthCm;

    // Resolve dispersion relation v = f · λ for traveling waves.
    if (Wave.WaveMode == ERshipFieldWaveMode::Traveling)
    {
        switch (Wave.Derive)
        {
        case ERshipFieldDerive::Speed:
            Desc.WavelengthCm = FMath::Max(Wave.WavelengthCm, 0.001f);
            Desc.FrequencyHz = FMath::Max(Wave.FrequencyHz, 0.001f);
            Desc.WaveSpeedCmPerSec = Desc.FrequencyHz * Desc.WavelengthCm;
            break;
        case ERshipFieldDerive::Wavelength:
            Desc.WaveSpeedCmPerSec = FMath::Max(Wave.WaveSpeedCmPerSec, 0.1f);
            Desc.FrequencyHz = FMath::Max(Wave.FrequencyHz, 0.001f);
            Desc.WavelengthCm = Desc.WaveSpeedCmPerSec / Desc.FrequencyHz;
            break;
        default: // LockFrequency
            Desc.WaveSpeedCmPerSec = FMath::Max(Wave.WaveSpeedCmPerSec, 0.1f);
            Desc.WavelengthCm = FMath::Max(Wave.WavelengthCm, 0.001f);
            Desc.FrequencyHz = Desc.WaveSpeedCmPerSec / Desc.WavelengthCm;
            break;
        }
    }
    else
    {
        Desc.WaveSpeedCmPerSec = Wave.WaveSpeedCmPerSec;
    }

    Desc.SyncGroup = Wave.SyncGroup;
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
    Desc.SyncGroup = Noise.SyncGroup;
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
    Desc.SyncGroup = Attractor.SyncGroup;
    return Desc;
}
