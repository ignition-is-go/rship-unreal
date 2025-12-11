// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SpatialDSPTypes.h"
#include "SpatialCalibrationTypes.generated.h"

// ============================================================================
// SMAART MEASUREMENT DATA
// ============================================================================

/**
 * Single frequency bin from a SMAART measurement.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIOEDITOR_API FSMAARTFrequencyBin
{
	GENERATED_BODY()

	/** Frequency in Hz */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float FrequencyHz = 0.0f;

	/** Magnitude in dB SPL or dBFS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float MagnitudeDb = 0.0f;

	/** Phase in degrees (-180 to +180) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float PhaseDegrees = 0.0f;

	/** Coherence (0.0 to 1.0), if available */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float Coherence = 1.0f;

	FSMAARTFrequencyBin() = default;

	FSMAARTFrequencyBin(float InFreq, float InMag, float InPhase, float InCoherence = 1.0f)
		: FrequencyHz(InFreq)
		, MagnitudeDb(InMag)
		, PhaseDegrees(InPhase)
		, Coherence(InCoherence)
	{}
};

/**
 * Type of SMAART measurement data.
 */
UENUM(BlueprintType)
enum class ESMAARTMeasurementType : uint8
{
	/** Transfer function (FFT magnitude and phase) */
	TransferFunction,

	/** Impulse response measurement */
	ImpulseResponse,

	/** RTA (Real-Time Analyzer) spectrum */
	RTA,

	/** SPL measurement over time */
	SPL,

	/** Delay finder result */
	DelayFinder
};

/**
 * Complete SMAART measurement containing frequency response data.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIOEDITOR_API FSMAARTMeasurement
{
	GENERATED_BODY()

	/** User-friendly name for this measurement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString Name;

	/** Original filename */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString SourceFile;

	/** Type of measurement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	ESMAARTMeasurementType MeasurementType = ESMAARTMeasurementType::TransferFunction;

	/** Timestamp when measurement was taken (if available) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FDateTime Timestamp;

	/** Speaker/output this measurement corresponds to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FGuid SpeakerId;

	/** Frequency bins (sorted by frequency) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	TArray<FSMAARTFrequencyBin> FrequencyBins;

	/** Detected propagation delay in milliseconds (from delay finder or IR) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float DetectedDelayMs = 0.0f;

	/** Average coherence across measured range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float AverageCoherence = 0.0f;

	/** Peak SPL observed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float PeakSPL = 0.0f;

	/** Reference level used during measurement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float ReferenceLevelDb = 0.0f;

	// ========================================================================
	// METHODS
	// ========================================================================

	/** Get magnitude at specific frequency (interpolated) */
	float GetMagnitudeAtFrequency(float FreqHz) const;

	/** Get phase at specific frequency (interpolated) */
	float GetPhaseAtFrequency(float FreqHz) const;

	/** Get coherence at specific frequency (interpolated) */
	float GetCoherenceAtFrequency(float FreqHz) const;

	/** Get frequency range of measurement */
	void GetFrequencyRange(float& OutMinHz, float& OutMaxHz) const;

	/** Calculate average magnitude in a frequency band */
	float GetAverageMagnitudeInBand(float LowHz, float HighHz) const;

	/** Find frequencies with magnitude deviation exceeding threshold */
	TArray<float> FindProblematicFrequencies(float DeviationThresholdDb, float CoherenceThreshold = 0.5f) const;
};

// ============================================================================
// CALIBRATION TARGET
// ============================================================================

/**
 * Target response curve for calibration.
 */
UENUM(BlueprintType)
enum class ECalibrationTargetCurve : uint8
{
	/** Flat response (0 dB across spectrum) */
	Flat,

	/** X-Curve (cinema standard, slight HF rolloff) */
	XCurve,

	/** Custom user-defined curve */
	Custom
};

/**
 * Target curve definition for auto-EQ generation.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIOEDITOR_API FCalibrationTarget
{
	GENERATED_BODY()

	/** Name of this target curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString Name = TEXT("Flat");

	/** Curve type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	ECalibrationTargetCurve CurveType = ECalibrationTargetCurve::Flat;

	/** Custom target points (frequency, dB) for Custom curve type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	TArray<FVector2D> CustomCurvePoints;

	/** Low frequency limit for EQ correction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration", meta = (ClampMin = "20.0", ClampMax = "500.0"))
	float LowFrequencyLimitHz = 60.0f;

	/** High frequency limit for EQ correction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration", meta = (ClampMin = "2000.0", ClampMax = "20000.0"))
	float HighFrequencyLimitHz = 16000.0f;

	/** Get target magnitude at frequency */
	float GetTargetMagnitudeAtFrequency(float FreqHz) const;
};

// ============================================================================
// AUTO-EQ SETTINGS
// ============================================================================

/**
 * Settings for auto-EQ generation from measurements.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIOEDITOR_API FAutoEQSettings
{
	GENERATED_BODY()

	/** Target response curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FCalibrationTarget Target;

	/** Maximum number of EQ bands to generate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxBands = 8;

	/** Maximum gain per band in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration", meta = (ClampMin = "1.0", ClampMax = "24.0"))
	float MaxGainDb = 12.0f;

	/** Minimum Q value for generated bands */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration", meta = (ClampMin = "0.3", ClampMax = "10.0"))
	float MinQ = 0.5f;

	/** Maximum Q value for generated bands */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float MaxQ = 10.0f;

	/** Minimum coherence to trust measurement data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CoherenceThreshold = 0.6f;

	/** Smooth measurement data before EQ calculation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bSmoothMeasurement = true;

	/** Smoothing factor (octave fraction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SmoothingOctaves = 0.125f;  // 1/8 octave

	/** Prefer cuts over boosts (safer, better headroom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bPreferCuts = true;

	/** Generate high-pass filter suggestion based on speaker capability */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bSuggestHighPass = true;
};

// ============================================================================
// CALIBRATION PRESET
// ============================================================================

/**
 * Complete calibration preset for a speaker.
 * Contains measurement data and generated corrections.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIOEDITOR_API FSpeakerCalibrationPreset
{
	GENERATED_BODY()

	/** Preset name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString Name;

	/** Speaker this calibration applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FGuid SpeakerId;

	/** Speaker name (for display) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString SpeakerName;

	/** Creation timestamp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FDateTime Created;

	/** Last modified timestamp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FDateTime Modified;

	/** Notes/comments */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString Notes;

	// ========================================================================
	// MEASUREMENT DATA
	// ========================================================================

	/** Original measurement used for this calibration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FSMAARTMeasurement Measurement;

	/** Settings used for auto-EQ generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FAutoEQSettings AutoEQSettings;

	// ========================================================================
	// GENERATED CORRECTIONS
	// ========================================================================

	/** Suggested delay alignment in milliseconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float SuggestedDelayMs = 0.0f;

	/** Suggested level trim in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float SuggestedGainDb = 0.0f;

	/** Generated EQ bands */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	TArray<FSpatialEQBand> GeneratedEQBands;

	/** Suggested high-pass filter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FSpatialHighPassFilter SuggestedHighPass;

	/** Suggested low-pass filter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FSpatialLowPassFilter SuggestedLowPass;

	// ========================================================================
	// APPLICATION STATE
	// ========================================================================

	/** Whether delay correction is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bApplyDelay = true;

	/** Whether gain correction is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bApplyGain = true;

	/** Whether EQ correction is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bApplyEQ = true;

	/** Whether filter suggestions are enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	bool bApplyFilters = false;

	// ========================================================================
	// METHODS
	// ========================================================================

	/** Generate DSP state from this preset */
	FSpatialSpeakerDSPState GenerateDSPState() const;

	/** Recalculate corrections from measurement with current settings */
	void RecalculateCorrections();
};

// ============================================================================
// VENUE CALIBRATION SET
// ============================================================================

/**
 * Complete calibration set for an entire venue.
 * Contains calibrations for all speakers plus global settings.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIOEDITOR_API FVenueCalibrationSet
{
	GENERATED_BODY()

	/** Set name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString Name;

	/** Associated venue name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString VenueName;

	/** Creation timestamp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FDateTime Created;

	/** Last modified timestamp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FDateTime Modified;

	/** Notes/comments about this calibration session */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FString Notes;

	/** Global reference level used during calibration (dB SPL @ 1kHz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float ReferenceLevelSPL = 85.0f;

	/** Reference delay speaker (all delays relative to this) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	FGuid ReferenceDelaySpeakerId;

	/** Per-speaker calibration presets (keyed by speaker ID) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	TMap<FGuid, FSpeakerCalibrationPreset> SpeakerPresets;

	// ========================================================================
	// METHODS
	// ========================================================================

	/** Get calibration for a specific speaker */
	FSpeakerCalibrationPreset* GetSpeakerPreset(const FGuid& SpeakerId);
	const FSpeakerCalibrationPreset* GetSpeakerPreset(const FGuid& SpeakerId) const;

	/** Add or update speaker calibration */
	void SetSpeakerPreset(const FGuid& SpeakerId, const FSpeakerCalibrationPreset& Preset);

	/** Remove speaker calibration */
	void RemoveSpeakerPreset(const FGuid& SpeakerId);

	/** Recalculate all delays relative to reference speaker */
	void NormalizeDelays();

	/** Recalculate all gains relative to reference level */
	void NormalizeGains();
};
