// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Calibration/SpatialCalibrationTypes.h"

/**
 * Result of SMAART file parsing.
 */
struct RSHIPSPATIALAUDIOEDITOR_API FSMAARTImportResult
{
	/** Whether import was successful */
	bool bSuccess = false;

	/** Error message if import failed */
	FString ErrorMessage;

	/** Imported measurements */
	TArray<FSMAARTMeasurement> Measurements;

	/** Source file path */
	FString SourceFilePath;

	/** Detected file format */
	FString DetectedFormat;

	/** Create success result */
	static FSMAARTImportResult Success(const TArray<FSMAARTMeasurement>& InMeasurements)
	{
		FSMAARTImportResult Result;
		Result.bSuccess = true;
		Result.Measurements = InMeasurements;
		return Result;
	}

	/** Create failure result */
	static FSMAARTImportResult Failure(const FString& InError)
	{
		FSMAARTImportResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = InError;
		return Result;
	}
};

/**
 * SMAART measurement file importer.
 *
 * Supports various SMAART export formats:
 * - Transfer function CSV/TXT (frequency, magnitude, phase)
 * - ASCII export with header metadata
 * - Tab/comma/semicolon delimited
 *
 * Usage:
 * 1. Call ImportFromFile() with a file path
 * 2. Check bSuccess in the result
 * 3. Access Measurements array for imported data
 */
class RSHIPSPATIALAUDIOEDITOR_API FSMAARTImporter
{
public:
	FSMAARTImporter() = default;

	// ========================================================================
	// IMPORT METHODS
	// ========================================================================

	/**
	 * Import measurements from a single file.
	 * Auto-detects file format.
	 */
	FSMAARTImportResult ImportFromFile(const FString& FilePath);

	/**
	 * Import measurements from multiple files.
	 */
	FSMAARTImportResult ImportFromFiles(const TArray<FString>& FilePaths);

	/**
	 * Import from raw string content.
	 * Useful for clipboard paste or drag-drop.
	 */
	FSMAARTImportResult ImportFromString(const FString& Content, const FString& SourceName = TEXT("Clipboard"));

	// ========================================================================
	// FORMAT DETECTION
	// ========================================================================

	/**
	 * Detected file format types.
	 */
	enum class EFileFormat
	{
		Unknown,
		SMAARTTransferFunction,  // SMAART 7/8 transfer function export
		SMAARTCSV,               // Generic SMAART CSV export
		SYSID,                   // SYSID format
		REW,                     // Room EQ Wizard format
		GenericCSV               // Generic freq/mag/phase CSV
	};

	/**
	 * Detect the format of a file.
	 */
	EFileFormat DetectFormat(const FString& FilePath);
	EFileFormat DetectFormatFromContent(const FString& Content);

	/**
	 * Get human-readable format name.
	 */
	static FString GetFormatName(EFileFormat Format);

	// ========================================================================
	// SUPPORTED FILE TYPES
	// ========================================================================

	/**
	 * Get supported file extensions for file dialogs.
	 */
	static TArray<FString> GetSupportedExtensions();

	/**
	 * Get file type filter string for file dialogs.
	 */
	static FString GetFileTypeFilter();

private:
	// ========================================================================
	// FORMAT-SPECIFIC PARSERS
	// ========================================================================

	/** Parse SMAART transfer function format */
	FSMAARTImportResult ParseSMAARTTransferFunction(const FString& Content, const FString& SourceName);

	/** Parse generic CSV format */
	FSMAARTImportResult ParseGenericCSV(const FString& Content, const FString& SourceName);

	/** Parse REW format */
	FSMAARTImportResult ParseREWFormat(const FString& Content, const FString& SourceName);

	// ========================================================================
	// UTILITY METHODS
	// ========================================================================

	/** Detect column delimiter */
	TCHAR DetectDelimiter(const FString& Line);

	/** Parse a line into columns */
	TArray<FString> ParseLine(const FString& Line, TCHAR Delimiter);

	/** Clean a string value (trim whitespace, remove quotes) */
	FString CleanValue(const FString& Value);

	/** Try to parse a float value */
	bool TryParseFloat(const FString& Value, float& OutResult);

	/** Identify column indices from header row */
	struct FColumnMapping
	{
		int32 FrequencyColumn = -1;
		int32 MagnitudeColumn = -1;
		int32 PhaseColumn = -1;
		int32 CoherenceColumn = -1;

		bool IsValid() const { return FrequencyColumn >= 0 && MagnitudeColumn >= 0; }
	};

	FColumnMapping IdentifyColumns(const TArray<FString>& HeaderColumns);

	/** Extract measurement name from file path or content */
	FString ExtractMeasurementName(const FString& FilePath, const FString& Content);

	/** Parse metadata from SMAART header comments */
	void ParseSMAARTMetadata(const TArray<FString>& Lines, FSMAARTMeasurement& OutMeasurement);
};

/**
 * Async SMAART import task for importing large files without blocking.
 */
class RSHIPSPATIALAUDIOEDITOR_API FSMAARTImportTask : public FNonAbandonableTask
{
public:
	FSMAARTImportTask(const TArray<FString>& InFilePaths)
		: FilePaths(InFilePaths)
	{}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSMAARTImportTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork()
	{
		FSMAARTImporter Importer;
		Result = Importer.ImportFromFiles(FilePaths);
	}

	/** Get import result (only valid after task completion) */
	const FSMAARTImportResult& GetResult() const { return Result; }

private:
	TArray<FString> FilePaths;
	FSMAARTImportResult Result;
};

typedef FAsyncTask<FSMAARTImportTask> FAsyncSMAARTImportTask;
