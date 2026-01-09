// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SpatialDSPTypes.h"

/**
 * Biquad filter types for audio processing.
 */
enum class ESpatialBiquadType : uint8
{
	LowPass,
	HighPass,
	BandPass,
	Notch,
	AllPass,
	PeakingEQ,
	LowShelf,
	HighShelf
};

/**
 * Biquad IIR filter implementation.
 *
 * Standard second-order IIR filter (biquad) using the transposed direct form II.
 * This is the building block for EQ bands, crossover filters, etc.
 *
 * Transfer function:
 *   H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
 *
 * The a0 coefficient is normalized to 1.0.
 *
 * Thread Safety:
 * - SetCoefficients() should be called from game thread
 * - Process() is audio-thread safe
 * - Use SmoothCoefficients for glitch-free parameter changes
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialBiquadFilter
{
public:
	FSpatialBiquadFilter();

	/**
	 * Reset filter state (clear delay line).
	 */
	void Reset();

	/**
	 * Set filter coefficients directly.
	 */
	void SetCoefficients(float InB0, float InB1, float InB2, float InA1, float InA2);

	/**
	 * Set target coefficients for smoothed parameter changes.
	 */
	void SetTargetCoefficients(float InB0, float InB1, float InB2, float InA1, float InA2);

	/**
	 * Configure as low-pass filter.
	 * @param SampleRate Audio sample rate.
	 * @param Frequency Cutoff frequency in Hz.
	 * @param Q Filter Q factor (0.707 = Butterworth).
	 */
	void SetLowPass(float SampleRate, float Frequency, float Q = 0.707f);

	/**
	 * Configure as high-pass filter.
	 */
	void SetHighPass(float SampleRate, float Frequency, float Q = 0.707f);

	/**
	 * Configure as band-pass filter.
	 */
	void SetBandPass(float SampleRate, float Frequency, float Q = 1.0f);

	/**
	 * Configure as notch (band-reject) filter.
	 */
	void SetNotch(float SampleRate, float Frequency, float Q = 1.0f);

	/**
	 * Configure as peaking EQ filter.
	 * @param SampleRate Audio sample rate.
	 * @param Frequency Center frequency in Hz.
	 * @param GainDb Gain at center frequency in dB.
	 * @param Q Filter Q factor (bandwidth).
	 */
	void SetPeakingEQ(float SampleRate, float Frequency, float GainDb, float Q = 1.0f);

	/**
	 * Configure as low-shelf filter.
	 * @param SampleRate Audio sample rate.
	 * @param Frequency Shelf frequency in Hz.
	 * @param GainDb Shelf gain in dB.
	 * @param S Shelf slope (1.0 = steepest).
	 */
	void SetLowShelf(float SampleRate, float Frequency, float GainDb, float S = 1.0f);

	/**
	 * Configure as high-shelf filter.
	 */
	void SetHighShelf(float SampleRate, float Frequency, float GainDb, float S = 1.0f);

	/**
	 * Configure as all-pass filter.
	 */
	void SetAllPass(float SampleRate, float Frequency, float Q = 0.707f);

	/**
	 * Configure as bypass (unity gain, no filtering).
	 */
	void SetBypass(float SampleRate);

	/**
	 * Process a single sample.
	 * @param Input Input sample.
	 * @return Filtered output sample.
	 */
	FORCEINLINE float Process(float Input)
	{
		// Transposed Direct Form II
		float Output = B0 * Input + Z1;
		Z1 = B1 * Input - A1 * Output + Z2;
		Z2 = B2 * Input - A2 * Output;
		return Output;
	}

	/**
	 * Process a single sample with coefficient smoothing.
	 */
	float ProcessSmoothed(float Input);

	/**
	 * Process a buffer of samples.
	 * @param Buffer Input/output buffer (in-place).
	 * @param NumSamples Number of samples to process.
	 */
	void ProcessBuffer(float* Buffer, int32 NumSamples);

	/**
	 * Process a buffer with coefficient smoothing.
	 */
	void ProcessBufferSmoothed(float* Buffer, int32 NumSamples);

	/**
	 * Get current filter type.
	 */
	ESpatialBiquadType GetType() const { return FilterType; }

	/**
	 * Get frequency response magnitude at a given frequency.
	 * @param Frequency Query frequency in Hz.
	 * @param SampleRate Sample rate for calculation.
	 * @return Magnitude response (linear).
	 */
	float GetMagnitudeResponse(float Frequency, float SampleRate) const;

	/**
	 * Get frequency response in dB.
	 */
	float GetMagnitudeResponseDb(float Frequency, float SampleRate) const;

	/**
	 * Enable/disable coefficient smoothing.
	 */
	void SetSmoothingEnabled(bool bEnabled) { bSmoothingEnabled = bEnabled; }

	/**
	 * Set smoothing time constant in milliseconds.
	 */
	void SetSmoothingTime(float TimeMs, float SampleRate);

private:
	// Filter coefficients (normalized, a0 = 1.0)
	float B0, B1, B2;  // Feedforward
	float A1, A2;       // Feedback

	// Target coefficients for smoothing
	float TargetB0, TargetB1, TargetB2;
	float TargetA1, TargetA2;

	// Filter state (delay elements)
	float Z1, Z2;

	// Filter type for reference
	ESpatialBiquadType FilterType;

	// Coefficient smoothing
	bool bSmoothingEnabled;
	float SmoothingCoeff;

	// Smooth coefficient towards target
	FORCEINLINE void SmoothCoefficients()
	{
		B0 += (TargetB0 - B0) * SmoothingCoeff;
		B1 += (TargetB1 - B1) * SmoothingCoeff;
		B2 += (TargetB2 - B2) * SmoothingCoeff;
		A1 += (TargetA1 - A1) * SmoothingCoeff;
		A2 += (TargetA2 - A2) * SmoothingCoeff;
	}
};

/**
 * Cascaded biquad filter (multiple stages).
 * Used for higher-order filters (e.g., 4th order Linkwitz-Riley crossover).
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialCascadedBiquad
{
public:
	FSpatialCascadedBiquad();

	/**
	 * Reset all filter stages.
	 */
	void Reset();

	/**
	 * Set number of cascaded stages.
	 */
	void SetStageCount(int32 Count);

	/**
	 * Get number of stages.
	 */
	int32 GetStageCount() const { return Stages.Num(); }

	/**
	 * Get a specific stage for configuration.
	 */
	FSpatialBiquadFilter& GetStage(int32 Index) { return Stages[Index]; }
	const FSpatialBiquadFilter& GetStage(int32 Index) const { return Stages[Index]; }

	/**
	 * Configure as Linkwitz-Riley low-pass crossover.
	 * @param SampleRate Audio sample rate.
	 * @param Frequency Crossover frequency.
	 * @param Order Filter order (2 or 4).
	 */
	void SetLinkwitzRileyLowPass(float SampleRate, float Frequency, int32 Order = 4);

	/**
	 * Configure as Linkwitz-Riley high-pass crossover.
	 */
	void SetLinkwitzRileyHighPass(float SampleRate, float Frequency, int32 Order = 4);

	/**
	 * Configure as Butterworth low-pass.
	 */
	void SetButterworthLowPass(float SampleRate, float Frequency, int32 Order = 2);

	/**
	 * Configure as Butterworth high-pass.
	 */
	void SetButterworthHighPass(float SampleRate, float Frequency, int32 Order = 2);

	/**
	 * Process a single sample through all stages.
	 */
	float Process(float Input);

	/**
	 * Process a buffer through all stages.
	 */
	void ProcessBuffer(float* Buffer, int32 NumSamples);

private:
	TArray<FSpatialBiquadFilter> Stages;
};
