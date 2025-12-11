// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialBiquadFilter.h"
#include "Core/SpatialAudioTypes.h"

/**
 * Configuration for a single EQ band (internal DSP representation).
 */
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialDSPEQBand
{
	/** Filter type */
	ESpatialBiquadType Type = ESpatialBiquadType::PeakingEQ;

	/** Center/corner frequency in Hz */
	float Frequency = 1000.0f;

	/** Gain in dB (for peaking/shelf types) */
	float GainDb = 0.0f;

	/** Q factor (bandwidth) */
	float Q = 1.0f;

	/** Is this band enabled */
	bool bEnabled = true;

	bool operator==(const FSpatialDSPEQBand& Other) const
	{
		return Type == Other.Type &&
			FMath::IsNearlyEqual(Frequency, Other.Frequency) &&
			FMath::IsNearlyEqual(GainDb, Other.GainDb) &&
			FMath::IsNearlyEqual(Q, Other.Q) &&
			bEnabled == Other.bEnabled;
	}

	bool operator!=(const FSpatialDSPEQBand& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * Configuration for crossover filters.
 */
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialCrossoverConfig
{
	/** High-pass frequency (0 = disabled) */
	float HighPassFrequency = 0.0f;

	/** High-pass filter order (2 = 12dB/oct, 4 = 24dB/oct) */
	int32 HighPassOrder = 4;

	/** Low-pass frequency (0 = disabled, uses Nyquist) */
	float LowPassFrequency = 0.0f;

	/** Low-pass filter order */
	int32 LowPassOrder = 4;

	/** Use Linkwitz-Riley (true) or Butterworth (false) */
	bool bLinkwitzRiley = true;
};

/**
 * Configuration for the limiter.
 */
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialLimiterConfig
{
	/** Threshold in dB */
	float ThresholdDb = 0.0f;

	/** Attack time in milliseconds */
	float AttackMs = 0.1f;

	/** Release time in milliseconds */
	float ReleaseMs = 100.0f;

	/** Knee width in dB (0 = hard knee) */
	float KneeDb = 0.0f;

	/** Enable limiter */
	bool bEnabled = true;
};

/**
 * Full DSP configuration for a single speaker.
 */
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialSpeakerDSPConfig
{
	/** Speaker ID this config applies to */
	FGuid SpeakerId;

	/** Input gain in dB */
	float InputGainDb = 0.0f;

	/** Output gain in dB */
	float OutputGainDb = 0.0f;

	/** Delay in milliseconds */
	float DelayMs = 0.0f;

	/** Polarity inversion */
	bool bInvertPolarity = false;

	/** Mute state */
	bool bMuted = false;

	/** Solo state (managed externally) */
	bool bSoloed = false;

	/** Crossover configuration */
	FSpatialCrossoverConfig Crossover;

	/** EQ bands (typically 4-8 bands) */
	TArray<FSpatialDSPEQBand> EQBands;

	/** Limiter configuration */
	FSpatialLimiterConfig Limiter;

	/** Bypass all processing */
	bool bBypass = false;
};

/**
 * Peak limiter with soft knee and lookahead capability.
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialLimiter
{
public:
	FSpatialLimiter();

	/**
	 * Configure the limiter.
	 */
	void Configure(float SampleRate, const FSpatialLimiterConfig& Config);

	/**
	 * Process a single sample.
	 */
	FORCEINLINE float Process(float Input)
	{
		if (!bEnabled)
		{
			return Input;
		}

		float InputLevel = FMath::Abs(Input);

		// Compute gain reduction
		float GainReduction = ComputeGainReduction(InputLevel);

		// Smooth the gain reduction
		if (GainReduction < CurrentGain)
		{
			// Attack
			CurrentGain = AttackCoeff * CurrentGain + (1.0f - AttackCoeff) * GainReduction;
		}
		else
		{
			// Release
			CurrentGain = ReleaseCoeff * CurrentGain + (1.0f - ReleaseCoeff) * GainReduction;
		}

		return Input * CurrentGain;
	}

	/**
	 * Process a buffer of samples.
	 */
	void ProcessBuffer(float* Buffer, int32 NumSamples);

	/**
	 * Reset limiter state.
	 */
	void Reset();

	/**
	 * Get current gain reduction in dB.
	 */
	float GetGainReductionDb() const;

private:
	float ComputeGainReduction(float InputLevel) const;

	float Threshold;        // Linear threshold
	float ThresholdDb;      // dB threshold for knee calculation
	float KneeDb;           // Knee width
	float KneeStart;        // Start of knee region (linear)
	float KneeEnd;          // End of knee region (linear)
	float AttackCoeff;      // Attack smoothing coefficient
	float ReleaseCoeff;     // Release smoothing coefficient
	float CurrentGain;      // Current gain value
	bool bEnabled;
};

/**
 * Delay line for speaker alignment.
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialDelayLine
{
public:
	FSpatialDelayLine();
	~FSpatialDelayLine();

	/**
	 * Initialize the delay line.
	 * @param SampleRate Audio sample rate.
	 * @param MaxDelayMs Maximum delay in milliseconds.
	 */
	void Initialize(float SampleRate, float MaxDelayMs = 500.0f);

	/**
	 * Set delay time.
	 * @param DelayMs Delay in milliseconds.
	 */
	void SetDelay(float DelayMs);

	/**
	 * Get current delay in milliseconds.
	 */
	float GetDelayMs() const { return CurrentDelayMs; }

	/**
	 * Process a single sample.
	 */
	FORCEINLINE float Process(float Input)
	{
		// Write to buffer
		Buffer[WriteIndex] = Input;

		// Read from buffer with interpolation
		float ReadPos = static_cast<float>(WriteIndex) - DelaySamples;
		if (ReadPos < 0.0f)
		{
			ReadPos += static_cast<float>(BufferSize);
		}

		int32 ReadIndex0 = static_cast<int32>(ReadPos);
		int32 ReadIndex1 = (ReadIndex0 + 1) % BufferSize;
		float Frac = ReadPos - static_cast<float>(ReadIndex0);

		float Output = Buffer[ReadIndex0] * (1.0f - Frac) + Buffer[ReadIndex1] * Frac;

		// Advance write position
		WriteIndex = (WriteIndex + 1) % BufferSize;

		return Output;
	}

	/**
	 * Process a buffer of samples.
	 */
	void ProcessBuffer(float* InBuffer, int32 NumSamples);

	/**
	 * Clear the delay buffer.
	 */
	void Clear();

private:
	float* Buffer;
	int32 BufferSize;
	int32 WriteIndex;
	float DelaySamples;
	float CurrentDelayMs;
	float SampleRate;
	float MaxDelayMs;
};

/**
 * Complete DSP processor for a single speaker output.
 *
 * Signal flow:
 * Input → Input Gain → HP Crossover → EQ → LP Crossover → Limiter → Delay → Polarity → Output Gain → Output
 *
 * All processing is designed for real-time audio thread usage.
 * Parameter changes are smoothed to avoid clicks/pops.
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialSpeakerDSP
{
public:
	/** Maximum number of EQ bands */
	static constexpr int32 MaxEQBands = 8;

	FSpatialSpeakerDSP();
	~FSpatialSpeakerDSP();

	/**
	 * Initialize the DSP processor.
	 * @param SampleRate Audio sample rate.
	 * @param MaxDelayMs Maximum delay in milliseconds.
	 */
	void Initialize(float SampleRate, float MaxDelayMs = 500.0f);

	/**
	 * Apply a full configuration update.
	 * This should be called from the game thread.
	 * Changes are applied with smoothing on the audio thread.
	 */
	void ApplyConfig(const FSpatialSpeakerDSPConfig& Config);

	/**
	 * Set input gain.
	 */
	void SetInputGain(float GainDb);

	/**
	 * Set output gain.
	 */
	void SetOutputGain(float GainDb);

	/**
	 * Set delay time.
	 */
	void SetDelay(float DelayMs);

	/**
	 * Set polarity inversion.
	 */
	void SetInvertPolarity(bool bInvert);

	/**
	 * Set mute state.
	 */
	void SetMuted(bool bMute);

	/**
	 * Set bypass state.
	 */
	void SetBypass(bool bBypassAll);

	/**
	 * Configure an EQ band.
	 */
	void SetEQBand(int32 BandIndex, const FSpatialDSPEQBand& Band);

	/**
	 * Configure crossover filters.
	 */
	void SetCrossover(const FSpatialCrossoverConfig& Config);

	/**
	 * Configure limiter.
	 */
	void SetLimiter(const FSpatialLimiterConfig& Config);

	/**
	 * Process a single sample.
	 * Call from audio thread only.
	 */
	FORCEINLINE float Process(float Input)
	{
		// Check bypass/mute first
		if (bBypass)
		{
			return Input;
		}

		if (bMuted)
		{
			// Still update smoothing
			UpdateSmoothing();
			return 0.0f;
		}

		// Update gain smoothing
		UpdateSmoothing();

		// Apply input gain
		float Sample = Input * CurrentInputGain;

		// High-pass crossover
		if (bHighPassEnabled)
		{
			Sample = HighPassFilter.Process(Sample);
		}

		// EQ bands
		for (int32 i = 0; i < NumActiveEQBands; ++i)
		{
			Sample = EQFilters[i].Process(Sample);
		}

		// Low-pass crossover
		if (bLowPassEnabled)
		{
			Sample = LowPassFilter.Process(Sample);
		}

		// Limiter
		Sample = Limiter.Process(Sample);

		// Delay
		Sample = DelayLine.Process(Sample);

		// Polarity
		if (bInvertPolarity)
		{
			Sample = -Sample;
		}

		// Output gain
		Sample *= CurrentOutputGain;

		return Sample;
	}

	/**
	 * Process a buffer of samples.
	 * Call from audio thread only.
	 */
	void ProcessBuffer(float* Buffer, int32 NumSamples);

	/**
	 * Reset all DSP state (clear delays, reset filters).
	 */
	void Reset();

	/**
	 * Get current configuration (for UI display).
	 */
	const FSpatialSpeakerDSPConfig& GetConfig() const { return CurrentConfig; }

	/**
	 * Get current limiter gain reduction in dB.
	 */
	float GetLimiterGainReductionDb() const { return Limiter.GetGainReductionDb(); }

	/**
	 * Check if processor is initialized.
	 */
	bool IsInitialized() const { return bInitialized; }

private:
	FORCEINLINE void UpdateSmoothing()
	{
		// Smooth input gain
		if (!FMath::IsNearlyEqual(CurrentInputGain, TargetInputGain, 0.0001f))
		{
			CurrentInputGain = CurrentInputGain * GainSmoothCoeff + TargetInputGain * (1.0f - GainSmoothCoeff);
		}
		else
		{
			CurrentInputGain = TargetInputGain;
		}

		// Smooth output gain
		if (!FMath::IsNearlyEqual(CurrentOutputGain, TargetOutputGain, 0.0001f))
		{
			CurrentOutputGain = CurrentOutputGain * GainSmoothCoeff + TargetOutputGain * (1.0f - GainSmoothCoeff);
		}
		else
		{
			CurrentOutputGain = TargetOutputGain;
		}
	}

	void ReconfigureHighPass();
	void ReconfigureLowPass();
	void ReconfigureEQ();

	// State
	bool bInitialized;
	float SampleRate;
	FSpatialSpeakerDSPConfig CurrentConfig;

	// Gain
	float TargetInputGain;
	float CurrentInputGain;
	float TargetOutputGain;
	float CurrentOutputGain;
	float GainSmoothCoeff;

	// Flags
	bool bMuted;
	bool bBypass;
	bool bInvertPolarity;
	bool bHighPassEnabled;
	bool bLowPassEnabled;

	// Crossover filters (cascaded for Linkwitz-Riley)
	FSpatialCascadedBiquad HighPassFilter;
	FSpatialCascadedBiquad LowPassFilter;

	// EQ bands
	FSpatialBiquadFilter EQFilters[MaxEQBands];
	int32 NumActiveEQBands;

	// Limiter
	FSpatialLimiter Limiter;

	// Delay
	FSpatialDelayLine DelayLine;
};

/**
 * Manager for multiple speaker DSP processors.
 *
 * Handles:
 * - Creating/destroying DSP instances per speaker
 * - Batch processing for efficiency
 * - Solo logic (when any speaker is soloed, mute all others)
 * - Global bypass
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialSpeakerDSPManager
{
public:
	FSpatialSpeakerDSPManager();
	~FSpatialSpeakerDSPManager();

	/**
	 * Initialize the manager.
	 */
	void Initialize(float SampleRate, int32 MaxSpeakers = 256);

	/**
	 * Shutdown and release resources.
	 */
	void Shutdown();

	/**
	 * Add a speaker DSP processor.
	 * @return Index of the created processor.
	 */
	int32 AddSpeaker(const FGuid& SpeakerId);

	/**
	 * Remove a speaker DSP processor.
	 */
	void RemoveSpeaker(const FGuid& SpeakerId);

	/**
	 * Get DSP processor for a speaker.
	 */
	FSpatialSpeakerDSP* GetSpeakerDSP(const FGuid& SpeakerId);

	/**
	 * Get DSP processor by index.
	 */
	FSpatialSpeakerDSP* GetSpeakerDSPByIndex(int32 Index);

	/**
	 * Apply configuration to a speaker.
	 */
	void ApplySpeakerConfig(const FGuid& SpeakerId, const FSpatialSpeakerDSPConfig& Config);

	/**
	 * Process output for a single speaker.
	 * @param SpeakerId Speaker to process.
	 * @param Buffer Audio buffer to process in-place.
	 * @param NumSamples Number of samples in buffer.
	 */
	void ProcessSpeaker(const FGuid& SpeakerId, float* Buffer, int32 NumSamples);

	/**
	 * Process output for a speaker by index.
	 */
	void ProcessSpeakerByIndex(int32 Index, float* Buffer, int32 NumSamples);

	/**
	 * Set global bypass (disable all processing).
	 */
	void SetGlobalBypass(bool bBypass);

	/**
	 * Update solo states.
	 * When any speaker is soloed, all non-soloed speakers are muted.
	 */
	void UpdateSoloStates();

	/**
	 * Get number of active speakers.
	 */
	int32 GetNumSpeakers() const { return SpeakerIdToIndex.Num(); }

	/**
	 * Reset all DSP processors.
	 */
	void ResetAll();

private:
	bool bInitialized;
	float SampleRate;
	int32 MaxSpeakers;
	bool bGlobalBypass;

	TArray<TUniquePtr<FSpatialSpeakerDSP>> DSPProcessors;
	TMap<FGuid, int32> SpeakerIdToIndex;
	TSet<int32> SoloedSpeakers;
};
