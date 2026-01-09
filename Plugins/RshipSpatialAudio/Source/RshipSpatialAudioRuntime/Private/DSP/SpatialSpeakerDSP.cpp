// Copyright Rocketship. All Rights Reserved.

#include "DSP/SpatialSpeakerDSP.h"

// ============================================================================
// FSpatialLimiter
// ============================================================================

FSpatialLimiter::FSpatialLimiter()
	: Threshold(1.0f)
	, ThresholdDb(0.0f)
	, KneeDb(0.0f)
	, KneeStart(1.0f)
	, KneeEnd(1.0f)
	, AttackCoeff(0.0f)
	, ReleaseCoeff(0.0f)
	, CurrentGain(1.0f)
	, bEnabled(true)
{
}

void FSpatialLimiter::Configure(float SampleRate, const FSpatialLimiterConfig& Config)
{
	bEnabled = Config.bEnabled;

	if (!bEnabled)
	{
		CurrentGain = 1.0f;
		return;
	}

	ThresholdDb = Config.ThresholdDb;
	Threshold = FMath::Pow(10.0f, ThresholdDb / 20.0f);
	KneeDb = Config.KneeDb;

	// Knee region in linear domain
	if (KneeDb > 0.0f)
	{
		KneeStart = FMath::Pow(10.0f, (ThresholdDb - KneeDb * 0.5f) / 20.0f);
		KneeEnd = FMath::Pow(10.0f, (ThresholdDb + KneeDb * 0.5f) / 20.0f);
	}
	else
	{
		KneeStart = Threshold;
		KneeEnd = Threshold;
	}

	// Time constants
	// Attack: fast response to peaks
	float AttackSamples = (Config.AttackMs / 1000.0f) * SampleRate;
	AttackCoeff = FMath::Exp(-1.0f / FMath::Max(AttackSamples, 1.0f));

	// Release: slower return to unity
	float ReleaseSamples = (Config.ReleaseMs / 1000.0f) * SampleRate;
	ReleaseCoeff = FMath::Exp(-1.0f / FMath::Max(ReleaseSamples, 1.0f));
}

float FSpatialLimiter::ComputeGainReduction(float InputLevel) const
{
	if (InputLevel <= KneeStart)
	{
		// Below knee - no reduction
		return 1.0f;
	}
	else if (InputLevel >= KneeEnd)
	{
		// Above knee - full limiting
		return Threshold / InputLevel;
	}
	else
	{
		// In knee region - soft transition
		// Quadratic interpolation through knee
		float KneeRange = KneeEnd - KneeStart;
		float KneePos = (InputLevel - KneeStart) / KneeRange;

		// Smoothstep for natural curve
		float t = KneePos * KneePos * (3.0f - 2.0f * KneePos);

		float NoReduction = 1.0f;
		float FullReduction = Threshold / InputLevel;

		return FMath::Lerp(NoReduction, FullReduction, t);
	}
}

void FSpatialLimiter::ProcessBuffer(float* Buffer, int32 NumSamples)
{
	if (!bEnabled)
	{
		return;
	}

	for (int32 i = 0; i < NumSamples; ++i)
	{
		Buffer[i] = Process(Buffer[i]);
	}
}

void FSpatialLimiter::Reset()
{
	CurrentGain = 1.0f;
}

float FSpatialLimiter::GetGainReductionDb() const
{
	if (CurrentGain >= 1.0f)
	{
		return 0.0f;
	}
	return 20.0f * FMath::LogX(10.0f, CurrentGain);
}

// ============================================================================
// FSpatialDelayLine
// ============================================================================

FSpatialDelayLine::FSpatialDelayLine()
	: Buffer(nullptr)
	, BufferSize(0)
	, WriteIndex(0)
	, DelaySamples(0.0f)
	, CurrentDelayMs(0.0f)
	, SampleRate(48000.0f)
	, MaxDelayMs(500.0f)
{
}

FSpatialDelayLine::~FSpatialDelayLine()
{
	if (Buffer)
	{
		FMemory::Free(Buffer);
		Buffer = nullptr;
	}
}

void FSpatialDelayLine::Initialize(float InSampleRate, float InMaxDelayMs)
{
	SampleRate = InSampleRate;
	MaxDelayMs = InMaxDelayMs;

	// Calculate buffer size (add margin for interpolation)
	BufferSize = static_cast<int32>((MaxDelayMs / 1000.0f) * SampleRate) + 4;

	// Allocate aligned buffer
	if (Buffer)
	{
		FMemory::Free(Buffer);
	}
	Buffer = static_cast<float*>(FMemory::Malloc(BufferSize * sizeof(float), 64));

	Clear();
}

void FSpatialDelayLine::SetDelay(float DelayMs)
{
	CurrentDelayMs = FMath::Clamp(DelayMs, 0.0f, MaxDelayMs);
	DelaySamples = (CurrentDelayMs / 1000.0f) * SampleRate;
}

void FSpatialDelayLine::ProcessBuffer(float* InBuffer, int32 NumSamples)
{
	for (int32 i = 0; i < NumSamples; ++i)
	{
		InBuffer[i] = Process(InBuffer[i]);
	}
}

void FSpatialDelayLine::Clear()
{
	if (Buffer)
	{
		FMemory::Memzero(Buffer, BufferSize * sizeof(float));
	}
	WriteIndex = 0;
}

// ============================================================================
// FSpatialSpeakerDSP
// ============================================================================

FSpatialSpeakerDSP::FSpatialSpeakerDSP()
	: bInitialized(false)
	, SampleRate(48000.0f)
	, TargetInputGain(1.0f)
	, CurrentInputGain(1.0f)
	, TargetOutputGain(1.0f)
	, CurrentOutputGain(1.0f)
	, GainSmoothCoeff(0.0f)
	, bMuted(false)
	, bBypass(false)
	, bInvertPolarity(false)
	, bHighPassEnabled(false)
	, bLowPassEnabled(false)
	, NumActiveEQBands(0)
{
}

FSpatialSpeakerDSP::~FSpatialSpeakerDSP()
{
}

void FSpatialSpeakerDSP::Initialize(float InSampleRate, float MaxDelayMs)
{
	SampleRate = InSampleRate;

	// Initialize delay line
	DelayLine.Initialize(SampleRate, MaxDelayMs);

	// Set up gain smoothing (~5ms time constant)
	float SmoothTimeMs = 5.0f;
	float SmoothSamples = (SmoothTimeMs / 1000.0f) * SampleRate;
	GainSmoothCoeff = FMath::Exp(-1.0f / FMath::Max(SmoothSamples, 1.0f));

	// Initialize limiter with default config
	FSpatialLimiterConfig DefaultLimiter;
	Limiter.Configure(SampleRate, DefaultLimiter);

	// Reset all filters
	for (int32 i = 0; i < MaxEQBands; ++i)
	{
		EQFilters[i].Reset();
	}

	HighPassFilter.Reset();
	LowPassFilter.Reset();

	bInitialized = true;
}

void FSpatialSpeakerDSP::ApplyConfig(const FSpatialSpeakerDSPConfig& Config)
{
	if (!bInitialized)
	{
		return;
	}

	CurrentConfig = Config;

	// Apply gains
	SetInputGain(Config.InputGainDb);
	SetOutputGain(Config.OutputGainDb);

	// Apply delay
	SetDelay(Config.DelayMs);

	// Apply flags
	SetInvertPolarity(Config.bInvertPolarity);
	SetMuted(Config.bMuted);
	SetBypass(Config.bBypass);

	// Apply crossover
	SetCrossover(Config.Crossover);

	// Apply EQ
	NumActiveEQBands = FMath::Min(Config.EQBands.Num(), MaxEQBands);
	for (int32 i = 0; i < NumActiveEQBands; ++i)
	{
		SetEQBand(i, Config.EQBands[i]);
	}

	// Apply limiter
	SetLimiter(Config.Limiter);
}

void FSpatialSpeakerDSP::SetInputGain(float GainDb)
{
	CurrentConfig.InputGainDb = GainDb;
	TargetInputGain = FMath::Pow(10.0f, GainDb / 20.0f);
}

void FSpatialSpeakerDSP::SetOutputGain(float GainDb)
{
	CurrentConfig.OutputGainDb = GainDb;
	TargetOutputGain = FMath::Pow(10.0f, GainDb / 20.0f);
}

void FSpatialSpeakerDSP::SetDelay(float DelayMs)
{
	CurrentConfig.DelayMs = DelayMs;
	DelayLine.SetDelay(DelayMs);
}

void FSpatialSpeakerDSP::SetInvertPolarity(bool bInvert)
{
	CurrentConfig.bInvertPolarity = bInvert;
	bInvertPolarity = bInvert;
}

void FSpatialSpeakerDSP::SetMuted(bool bMute)
{
	CurrentConfig.bMuted = bMute;
	bMuted = bMute;
}

void FSpatialSpeakerDSP::SetBypass(bool bBypassAll)
{
	CurrentConfig.bBypass = bBypassAll;
	bBypass = bBypassAll;
}

void FSpatialSpeakerDSP::SetEQBand(int32 BandIndex, const FSpatialDSPEQBand& Band)
{
	if (BandIndex < 0 || BandIndex >= MaxEQBands)
	{
		return;
	}

	// Update config
	if (BandIndex < CurrentConfig.EQBands.Num())
	{
		CurrentConfig.EQBands[BandIndex] = Band;
	}

	if (!Band.bEnabled)
	{
		// Set to bypass (unity gain, no filtering)
		EQFilters[BandIndex].SetBypass(SampleRate);
		return;
	}

	// Configure filter based on type
	switch (Band.Type)
	{
	case ESpatialBiquadType::LowPass:
		EQFilters[BandIndex].SetLowPass(SampleRate, Band.Frequency, Band.Q);
		break;

	case ESpatialBiquadType::HighPass:
		EQFilters[BandIndex].SetHighPass(SampleRate, Band.Frequency, Band.Q);
		break;

	case ESpatialBiquadType::BandPass:
		EQFilters[BandIndex].SetBandPass(SampleRate, Band.Frequency, Band.Q);
		break;

	case ESpatialBiquadType::Notch:
		EQFilters[BandIndex].SetNotch(SampleRate, Band.Frequency, Band.Q);
		break;

	case ESpatialBiquadType::PeakingEQ:
		EQFilters[BandIndex].SetPeakingEQ(SampleRate, Band.Frequency, Band.GainDb, Band.Q);
		break;

	case ESpatialBiquadType::LowShelf:
		EQFilters[BandIndex].SetLowShelf(SampleRate, Band.Frequency, Band.GainDb, Band.Q);
		break;

	case ESpatialBiquadType::HighShelf:
		EQFilters[BandIndex].SetHighShelf(SampleRate, Band.Frequency, Band.GainDb, Band.Q);
		break;

	case ESpatialBiquadType::AllPass:
		EQFilters[BandIndex].SetAllPass(SampleRate, Band.Frequency, Band.Q);
		break;

	default:
		EQFilters[BandIndex].SetBypass(SampleRate);
		break;
	}
}

void FSpatialSpeakerDSP::SetCrossover(const FSpatialCrossoverConfig& Config)
{
	CurrentConfig.Crossover = Config;

	// High-pass
	bHighPassEnabled = (Config.HighPassFrequency > 0.0f);
	if (bHighPassEnabled)
	{
		if (Config.bLinkwitzRiley)
		{
			HighPassFilter.SetLinkwitzRileyHighPass(SampleRate, Config.HighPassFrequency, Config.HighPassOrder);
		}
		else
		{
			HighPassFilter.SetButterworthHighPass(SampleRate, Config.HighPassFrequency, Config.HighPassOrder);
		}
	}

	// Low-pass
	bLowPassEnabled = (Config.LowPassFrequency > 0.0f && Config.LowPassFrequency < SampleRate * 0.5f);
	if (bLowPassEnabled)
	{
		if (Config.bLinkwitzRiley)
		{
			LowPassFilter.SetLinkwitzRileyLowPass(SampleRate, Config.LowPassFrequency, Config.LowPassOrder);
		}
		else
		{
			LowPassFilter.SetButterworthLowPass(SampleRate, Config.LowPassFrequency, Config.LowPassOrder);
		}
	}
}

void FSpatialSpeakerDSP::SetLimiter(const FSpatialLimiterConfig& Config)
{
	CurrentConfig.Limiter = Config;
	Limiter.Configure(SampleRate, Config);
}

void FSpatialSpeakerDSP::ProcessBuffer(float* Buffer, int32 NumSamples)
{
	if (!bInitialized)
	{
		return;
	}

	if (bBypass)
	{
		return;
	}

	if (bMuted)
	{
		FMemory::Memzero(Buffer, NumSamples * sizeof(float));
		return;
	}

	// Process sample by sample for full signal chain
	// (Could be optimized with block processing for each stage)
	for (int32 i = 0; i < NumSamples; ++i)
	{
		Buffer[i] = Process(Buffer[i]);
	}
}

void FSpatialSpeakerDSP::Reset()
{
	// Reset gains to target immediately
	CurrentInputGain = TargetInputGain;
	CurrentOutputGain = TargetOutputGain;

	// Reset filters
	for (int32 i = 0; i < MaxEQBands; ++i)
	{
		EQFilters[i].Reset();
	}

	HighPassFilter.Reset();
	LowPassFilter.Reset();

	// Reset limiter
	Limiter.Reset();

	// Clear delay
	DelayLine.Clear();
}

// ============================================================================
// FSpatialSpeakerDSPManager
// ============================================================================

FSpatialSpeakerDSPManager::FSpatialSpeakerDSPManager()
	: bInitialized(false)
	, SampleRate(48000.0f)
	, MaxSpeakers(256)
	, bGlobalBypass(false)
{
}

FSpatialSpeakerDSPManager::~FSpatialSpeakerDSPManager()
{
	Shutdown();
}

void FSpatialSpeakerDSPManager::Initialize(float InSampleRate, int32 InMaxSpeakers)
{
	SampleRate = InSampleRate;
	MaxSpeakers = InMaxSpeakers;

	DSPProcessors.Reserve(MaxSpeakers);

	bInitialized = true;
}

void FSpatialSpeakerDSPManager::Shutdown()
{
	DSPProcessors.Empty();
	SpeakerIdToIndex.Empty();
	SoloedSpeakers.Empty();
	bInitialized = false;
}

int32 FSpatialSpeakerDSPManager::AddSpeaker(const FGuid& SpeakerId)
{
	if (!bInitialized)
	{
		return INDEX_NONE;
	}

	// Check if already exists
	if (int32* ExistingIndex = SpeakerIdToIndex.Find(SpeakerId))
	{
		return *ExistingIndex;
	}

	// Check capacity
	if (DSPProcessors.Num() >= MaxSpeakers)
	{
		UE_LOG(LogTemp, Warning, TEXT("FSpatialSpeakerDSPManager: Maximum speaker limit reached (%d)"), MaxSpeakers);
		return INDEX_NONE;
	}

	// Create new processor
	int32 Index = DSPProcessors.Num();
	TUniquePtr<FSpatialSpeakerDSP> NewDSP = MakeUnique<FSpatialSpeakerDSP>();
	NewDSP->Initialize(SampleRate);

	DSPProcessors.Add(MoveTemp(NewDSP));
	SpeakerIdToIndex.Add(SpeakerId, Index);

	return Index;
}

void FSpatialSpeakerDSPManager::RemoveSpeaker(const FGuid& SpeakerId)
{
	if (int32* Index = SpeakerIdToIndex.Find(SpeakerId))
	{
		// Mark as removed (don't actually remove to preserve indices)
		// In a real implementation, would use a free list
		DSPProcessors[*Index].Reset();
		SpeakerIdToIndex.Remove(SpeakerId);
		SoloedSpeakers.Remove(*Index);
	}
}

FSpatialSpeakerDSP* FSpatialSpeakerDSPManager::GetSpeakerDSP(const FGuid& SpeakerId)
{
	if (int32* Index = SpeakerIdToIndex.Find(SpeakerId))
	{
		return DSPProcessors[*Index].Get();
	}
	return nullptr;
}

FSpatialSpeakerDSP* FSpatialSpeakerDSPManager::GetSpeakerDSPByIndex(int32 Index)
{
	if (Index >= 0 && Index < DSPProcessors.Num())
	{
		return DSPProcessors[Index].Get();
	}
	return nullptr;
}

void FSpatialSpeakerDSPManager::ApplySpeakerConfig(const FGuid& SpeakerId, const FSpatialSpeakerDSPConfig& Config)
{
	if (FSpatialSpeakerDSP* DSP = GetSpeakerDSP(SpeakerId))
	{
		DSP->ApplyConfig(Config);

		// Update solo tracking
		if (int32* Index = SpeakerIdToIndex.Find(SpeakerId))
		{
			if (Config.bSoloed)
			{
				SoloedSpeakers.Add(*Index);
			}
			else
			{
				SoloedSpeakers.Remove(*Index);
			}
		}

		UpdateSoloStates();
	}
}

void FSpatialSpeakerDSPManager::ProcessSpeaker(const FGuid& SpeakerId, float* Buffer, int32 NumSamples)
{
	if (bGlobalBypass)
	{
		return;
	}

	if (FSpatialSpeakerDSP* DSP = GetSpeakerDSP(SpeakerId))
	{
		DSP->ProcessBuffer(Buffer, NumSamples);
	}
}

void FSpatialSpeakerDSPManager::ProcessSpeakerByIndex(int32 Index, float* Buffer, int32 NumSamples)
{
	if (bGlobalBypass)
	{
		return;
	}

	if (FSpatialSpeakerDSP* DSP = GetSpeakerDSPByIndex(Index))
	{
		DSP->ProcessBuffer(Buffer, NumSamples);
	}
}

void FSpatialSpeakerDSPManager::SetGlobalBypass(bool bBypass)
{
	bGlobalBypass = bBypass;
}

void FSpatialSpeakerDSPManager::UpdateSoloStates()
{
	if (SoloedSpeakers.Num() == 0)
	{
		// No speakers soloed - unmute all (restore their actual mute state from config)
		for (auto& Pair : SpeakerIdToIndex)
		{
			if (FSpatialSpeakerDSP* DSP = DSPProcessors[Pair.Value].Get())
			{
				DSP->SetMuted(DSP->GetConfig().bMuted);
			}
		}
	}
	else
	{
		// Some speakers soloed - mute all non-soloed
		for (auto& Pair : SpeakerIdToIndex)
		{
			if (FSpatialSpeakerDSP* DSP = DSPProcessors[Pair.Value].Get())
			{
				bool bShouldMute = !SoloedSpeakers.Contains(Pair.Value);
				DSP->SetMuted(bShouldMute || DSP->GetConfig().bMuted);
			}
		}
	}
}

void FSpatialSpeakerDSPManager::ResetAll()
{
	for (auto& DSP : DSPProcessors)
	{
		if (DSP.IsValid())
		{
			DSP->Reset();
		}
	}
}
