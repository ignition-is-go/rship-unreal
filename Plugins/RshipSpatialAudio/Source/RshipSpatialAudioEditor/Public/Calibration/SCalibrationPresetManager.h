// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Calibration/SpatialCalibrationTypes.h"
#include "Calibration/SSMAARTImporter.h"

class URshipSpatialAudioManager;

/**
 * Result of auto-EQ generation.
 */
struct RSHIPSPATIALAUDIOEDITOR_API FAutoEQResult
{
	/** Whether generation was successful */
	bool bSuccess = false;

	/** Error or warning messages */
	FString Message;

	/** Generated EQ bands */
	TArray<FSpatialEQBand> EQBands;

	/** Suggested high-pass filter */
	FSpatialHighPassFilter SuggestedHighPass;

	/** Suggested low-pass filter */
	FSpatialLowPassFilter SuggestedLowPass;

	/** Estimated correction quality (0-1) */
	float CorrectionQuality = 0.0f;

	/** Predicted deviation from target after correction (dB) */
	float PredictedDeviation = 0.0f;
};

/**
 * Calibration Preset Manager.
 *
 * Manages SMAART measurement import, auto-EQ generation, and
 * calibration preset storage/recall. This is the main entry point
 * for the calibration workflow.
 *
 * Workflow:
 * 1. Import SMAART measurements via ImportMeasurements()
 * 2. Generate auto-EQ corrections via GenerateAutoEQ()
 * 3. Create calibration presets via CreatePreset()
 * 4. Apply presets to speakers via ApplyPresetToSpeaker()
 * 5. Save/load venue calibration sets via SaveCalibrationSet()/LoadCalibrationSet()
 */
class RSHIPSPATIALAUDIOEDITOR_API FCalibrationPresetManager
{
public:
	FCalibrationPresetManager();
	~FCalibrationPresetManager();

	// ========================================================================
	// IMPORT
	// ========================================================================

	/**
	 * Import SMAART measurements from files.
	 */
	FSMAARTImportResult ImportMeasurements(const TArray<FString>& FilePaths);

	/**
	 * Import SMAART measurement from clipboard.
	 */
	FSMAARTImportResult ImportFromClipboard();

	/**
	 * Get the last imported measurements.
	 */
	const TArray<FSMAARTMeasurement>& GetImportedMeasurements() const { return ImportedMeasurements; }

	/**
	 * Clear imported measurements.
	 */
	void ClearImportedMeasurements() { ImportedMeasurements.Empty(); }

	// ========================================================================
	// AUTO-EQ GENERATION
	// ========================================================================

	/**
	 * Generate auto-EQ from a measurement.
	 */
	FAutoEQResult GenerateAutoEQ(const FSMAARTMeasurement& Measurement, const FAutoEQSettings& Settings);

	/**
	 * Generate auto-EQ using default settings.
	 */
	FAutoEQResult GenerateAutoEQ(const FSMAARTMeasurement& Measurement);

	// ========================================================================
	// PRESET MANAGEMENT
	// ========================================================================

	/**
	 * Create a calibration preset from a measurement.
	 */
	FSpeakerCalibrationPreset CreatePreset(
		const FSMAARTMeasurement& Measurement,
		const FGuid& SpeakerId,
		const FString& SpeakerName,
		const FAutoEQSettings& Settings
	);

	/**
	 * Get all stored presets for a venue.
	 */
	const FVenueCalibrationSet* GetVenueCalibrationSet(const FString& VenueName) const;
	FVenueCalibrationSet* GetVenueCalibrationSet(const FString& VenueName);

	/**
	 * Create or get a venue calibration set.
	 */
	FVenueCalibrationSet& GetOrCreateVenueCalibrationSet(const FString& VenueName);

	/**
	 * Get all venue names with calibration data.
	 */
	TArray<FString> GetVenueNames() const;

	// ========================================================================
	// PERSISTENCE
	// ========================================================================

	/**
	 * Save a venue calibration set to file.
	 */
	bool SaveCalibrationSet(const FString& VenueName, const FString& FilePath);

	/**
	 * Load a venue calibration set from file.
	 */
	bool LoadCalibrationSet(const FString& FilePath, FString& OutVenueName);

	/**
	 * Export a single preset to file.
	 */
	bool ExportPreset(const FSpeakerCalibrationPreset& Preset, const FString& FilePath);

	/**
	 * Import a single preset from file.
	 */
	bool ImportPreset(const FString& FilePath, FSpeakerCalibrationPreset& OutPreset);

	/**
	 * Get file type filter for calibration files.
	 */
	static FString GetCalibrationFileFilter();

	// ========================================================================
	// APPLICATION
	// ========================================================================

	/**
	 * Apply a calibration preset to a speaker via the audio manager.
	 */
	bool ApplyPresetToSpeaker(
		URshipSpatialAudioManager* Manager,
		const FGuid& SpeakerId,
		const FSpeakerCalibrationPreset& Preset
	);

	/**
	 * Apply all presets from a venue calibration set.
	 */
	int32 ApplyVenueCalibration(
		URshipSpatialAudioManager* Manager,
		const FVenueCalibrationSet& CalibrationSet
	);

	// ========================================================================
	// ANALYSIS
	// ========================================================================

	/**
	 * Compare a measurement against a target curve.
	 * Returns deviation statistics.
	 */
	struct FDeviationStats
	{
		float AverageDeviation = 0.0f;
		float MaxDeviation = 0.0f;
		float MinDeviation = 0.0f;
		float RMSDeviation = 0.0f;
		TArray<TTuple<float, float>> DeviationPerFrequency;  // Freq, Deviation
	};

	FDeviationStats AnalyzeDeviation(
		const FSMAARTMeasurement& Measurement,
		const FCalibrationTarget& Target
	);

	/**
	 * Simulate applying EQ to a measurement and predict result.
	 */
	FSMAARTMeasurement SimulateEQApplication(
		const FSMAARTMeasurement& Measurement,
		const TArray<FSpatialEQBand>& EQBands
	);

private:
	// ========================================================================
	// AUTO-EQ ALGORITHMS
	// ========================================================================

	/**
	 * Smooth measurement data using moving average in log frequency space.
	 */
	FSMAARTMeasurement SmoothMeasurement(const FSMAARTMeasurement& Measurement, float OctaveFraction);

	/**
	 * Find peaks and dips in the frequency response.
	 */
	struct FFrequencyDeviation
	{
		float FrequencyHz;
		float DeviationDb;  // Positive = peak, Negative = dip
		float Bandwidth;    // In octaves
		float Coherence;
		bool bIsPeak;
	};

	TArray<FFrequencyDeviation> FindDeviations(
		const FSMAARTMeasurement& Measurement,
		const FCalibrationTarget& Target,
		float ThresholdDb
	);

	/**
	 * Generate optimal EQ band for a deviation.
	 */
	FSpatialEQBand CreateCorrectionBand(
		const FFrequencyDeviation& Deviation,
		const FAutoEQSettings& Settings
	);

	/**
	 * Merge overlapping EQ bands.
	 */
	TArray<FSpatialEQBand> MergeOverlappingBands(
		const TArray<FSpatialEQBand>& Bands,
		float OverlapThresholdOctaves = 0.5f
	);

	/**
	 * Optimize Q values for minimum deviation.
	 */
	void OptimizeQValues(
		TArray<FSpatialEQBand>& Bands,
		const FSMAARTMeasurement& Measurement,
		const FCalibrationTarget& Target
	);

	/**
	 * Suggest high-pass filter based on measurement.
	 */
	FSpatialHighPassFilter SuggestHighPass(
		const FSMAARTMeasurement& Measurement,
		const FAutoEQSettings& Settings
	);

	// ========================================================================
	// DATA
	// ========================================================================

	/** Last imported measurements */
	TArray<FSMAARTMeasurement> ImportedMeasurements;

	/** Venue calibration sets (keyed by venue name) */
	TMap<FString, FVenueCalibrationSet> VenueCalibrations;

	/** SMAART file importer */
	FSMAARTImporter Importer;
};
