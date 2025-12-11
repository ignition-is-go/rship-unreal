// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialAudioTypes.h"
#include "SpatialDSPTypes.generated.h"

// ============================================================================
// EQ TYPES
// ============================================================================

/**
 * Single parametric EQ band configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialEQBand
{
	GENERATED_BODY()

	/** Whether this band is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	bool bEnabled = true;

	/** Filter type for this band */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	ESpatialEQBandType Type = ESpatialEQBandType::Peak;

	/** Center/corner frequency in Hz */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "20.0", ClampMax = "20000.0"))
	float FrequencyHz = 1000.0f;

	/** Gain in dB (for peak/shelf types) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "-24.0", ClampMax = "24.0"))
	float GainDb = 0.0f;

	/** Q factor / bandwidth */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "0.1", ClampMax = "30.0"))
	float Q = 1.0f;

	/** Optional label for this band */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	FString Label;

	FSpatialEQBand() = default;

	FSpatialEQBand(ESpatialEQBandType InType, float InFreq, float InGain, float InQ)
		: bEnabled(true)
		, Type(InType)
		, FrequencyHz(InFreq)
		, GainDb(InGain)
		, Q(InQ)
	{}
};

// ============================================================================
// FILTER TYPES
// ============================================================================

/**
 * High-pass filter configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialHighPassFilter
{
	GENERATED_BODY()

	/** Whether the filter is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	bool bEnabled = false;

	/** Cutoff frequency in Hz */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "20.0", ClampMax = "2000.0"))
	float FrequencyHz = 80.0f;

	/** Filter slope */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	ESpatialFilterSlope Slope = ESpatialFilterSlope::Slope24dB;

	/** Filter type (Butterworth, Linkwitz-Riley, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	ESpatialFilterType FilterType = ESpatialFilterType::LinkwitzRiley;
};

/**
 * Low-pass filter configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialLowPassFilter
{
	GENERATED_BODY()

	/** Whether the filter is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	bool bEnabled = false;

	/** Cutoff frequency in Hz */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "80.0", ClampMax = "20000.0"))
	float FrequencyHz = 120.0f;

	/** Filter slope */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	ESpatialFilterSlope Slope = ESpatialFilterSlope::Slope24dB;

	/** Filter type (Butterworth, Linkwitz-Riley, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	ESpatialFilterType FilterType = ESpatialFilterType::LinkwitzRiley;
};

// ============================================================================
// DYNAMICS TYPES
// ============================================================================

/**
 * Limiter configuration.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialLimiterSettings
{
	GENERATED_BODY()

	/** Whether the limiter is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	bool bEnabled = true;

	/** Threshold level in dBFS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "-40.0", ClampMax = "0.0"))
	float ThresholdDb = -3.0f;

	/** Attack time in milliseconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "0.01", ClampMax = "100.0"))
	float AttackMs = 0.1f;

	/** Release time in milliseconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "1.0", ClampMax = "2000.0"))
	float ReleaseMs = 100.0f;

	/** Soft knee width in dB (0 = hard knee) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "0.0", ClampMax = "12.0"))
	float KneeDb = 3.0f;

	/** Lookahead time in milliseconds (adds latency) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float LookaheadMs = 1.0f;

	/** Output ceiling in dBFS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "-20.0", ClampMax = "0.0"))
	float CeilingDb = -0.3f;
};

/**
 * Compressor configuration (for bus processing).
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialCompressorSettings
{
	GENERATED_BODY()

	/** Whether the compressor is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	bool bEnabled = false;

	/** Threshold level in dBFS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "-60.0", ClampMax = "0.0"))
	float ThresholdDb = -20.0f;

	/** Compression ratio (e.g., 4.0 = 4:1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float Ratio = 4.0f;

	/** Attack time in milliseconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "0.1", ClampMax = "500.0"))
	float AttackMs = 10.0f;

	/** Release time in milliseconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "10.0", ClampMax = "5000.0"))
	float ReleaseMs = 200.0f;

	/** Soft knee width in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float KneeDb = 6.0f;

	/** Makeup gain in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float MakeupGainDb = 0.0f;
};

// ============================================================================
// COMPLETE DSP STATE
// ============================================================================

/**
 * Complete DSP processing state for a single speaker output channel.
 * This represents all processing applied after the mixing/panning stage.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialSpeakerDSPState
{
	GENERATED_BODY()

	// ========================================================================
	// GAIN STAGING
	// ========================================================================

	/** Pre-processing input trim in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Gain", meta = (ClampMin = "-40.0", ClampMax = "20.0"))
	float InputGainDb = 0.0f;

	/** Post-processing output trim in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Gain", meta = (ClampMin = "-40.0", ClampMax = "20.0"))
	float OutputGainDb = 0.0f;

	/** Polarity invert (phase flip) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Gain")
	bool bPolarityInvert = false;

	/** Channel muted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Gain")
	bool bMuted = false;

	/** Channel soloed (handled at mixer level) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Gain")
	bool bSoloed = false;

	// ========================================================================
	// DELAY
	// ========================================================================

	/** Alignment delay in milliseconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Delay", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
	float DelayMs = 0.0f;

	// ========================================================================
	// FILTERING
	// ========================================================================

	/** High-pass filter settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Filter")
	FSpatialHighPassFilter HighPass;

	/** Low-pass filter settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Filter")
	FSpatialLowPassFilter LowPass;

	// ========================================================================
	// EQ
	// ========================================================================

	/** Parametric EQ bands (up to 16) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|EQ")
	TArray<FSpatialEQBand> EQBands;

	// ========================================================================
	// DYNAMICS
	// ========================================================================

	/** Limiter settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP|Dynamics")
	FSpatialLimiterSettings Limiter;

	// ========================================================================
	// METHODS
	// ========================================================================

	/** Get input gain as linear multiplier */
	float GetInputGainLinear() const
	{
		return FMath::Pow(10.0f, InputGainDb / 20.0f);
	}

	/** Get output gain as linear multiplier */
	float GetOutputGainLinear() const
	{
		return FMath::Pow(10.0f, OutputGainDb / 20.0f);
	}

	/** Check if any DSP processing is enabled */
	bool HasActiveProcessing() const
	{
		if (FMath::Abs(InputGainDb) > 0.01f) return true;
		if (FMath::Abs(OutputGainDb) > 0.01f) return true;
		if (bPolarityInvert) return true;
		if (DelayMs > 0.01f) return true;
		if (HighPass.bEnabled) return true;
		if (LowPass.bEnabled) return true;
		if (Limiter.bEnabled) return true;
		for (const FSpatialEQBand& Band : EQBands)
		{
			if (Band.bEnabled && FMath::Abs(Band.GainDb) > 0.01f) return true;
		}
		return false;
	}
};

/**
 * DSP processing state for a bus.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialBusDSPState
{
	GENERATED_BODY()

	/** Bus gain in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP", meta = (ClampMin = "-80.0", ClampMax = "20.0"))
	float GainDb = 0.0f;

	/** Bus muted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	bool bMuted = false;

	/** Bus soloed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	bool bSoloed = false;

	/** Optional compressor for this bus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	FSpatialCompressorSettings Compressor;

	/** Optional EQ for this bus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio|DSP")
	TArray<FSpatialEQBand> EQBands;

	/** Get gain as linear multiplier */
	float GetGainLinear() const
	{
		return bMuted ? 0.0f : FMath::Pow(10.0f, GainDb / 20.0f);
	}
};
