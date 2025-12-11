// Copyright Rocketship. All Rights Reserved.

#include "Calibration/SSMAARTImporter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ============================================================================
// PUBLIC IMPORT METHODS
// ============================================================================

FSMAARTImportResult FSMAARTImporter::ImportFromFile(const FString& FilePath)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath))
	{
		return FSMAARTImportResult::Failure(FString::Printf(TEXT("Failed to read file: %s"), *FilePath));
	}

	EFileFormat Format = DetectFormatFromContent(Content);
	FString FormatName = GetFormatName(Format);

	FSMAARTImportResult Result;

	switch (Format)
	{
	case EFileFormat::SMAARTTransferFunction:
	case EFileFormat::SMAARTCSV:
		Result = ParseSMAARTTransferFunction(Content, FilePath);
		break;

	case EFileFormat::REW:
		Result = ParseREWFormat(Content, FilePath);
		break;

	case EFileFormat::GenericCSV:
		Result = ParseGenericCSV(Content, FilePath);
		break;

	default:
		return FSMAARTImportResult::Failure(TEXT("Unrecognized file format"));
	}

	Result.SourceFilePath = FilePath;
	Result.DetectedFormat = FormatName;

	return Result;
}

FSMAARTImportResult FSMAARTImporter::ImportFromFiles(const TArray<FString>& FilePaths)
{
	FSMAARTImportResult CombinedResult;
	CombinedResult.bSuccess = true;

	for (const FString& FilePath : FilePaths)
	{
		FSMAARTImportResult FileResult = ImportFromFile(FilePath);
		if (FileResult.bSuccess)
		{
			CombinedResult.Measurements.Append(FileResult.Measurements);
		}
		else
		{
			// Collect errors but continue with other files
			if (!CombinedResult.ErrorMessage.IsEmpty())
			{
				CombinedResult.ErrorMessage += TEXT("\n");
			}
			CombinedResult.ErrorMessage += FString::Printf(TEXT("%s: %s"),
				*FPaths::GetCleanFilename(FilePath), *FileResult.ErrorMessage);
		}
	}

	if (CombinedResult.Measurements.Num() == 0 && !CombinedResult.ErrorMessage.IsEmpty())
	{
		CombinedResult.bSuccess = false;
	}

	return CombinedResult;
}

FSMAARTImportResult FSMAARTImporter::ImportFromString(const FString& Content, const FString& SourceName)
{
	EFileFormat Format = DetectFormatFromContent(Content);

	switch (Format)
	{
	case EFileFormat::SMAARTTransferFunction:
	case EFileFormat::SMAARTCSV:
		return ParseSMAARTTransferFunction(Content, SourceName);

	case EFileFormat::REW:
		return ParseREWFormat(Content, SourceName);

	case EFileFormat::GenericCSV:
		return ParseGenericCSV(Content, SourceName);

	default:
		return FSMAARTImportResult::Failure(TEXT("Unrecognized data format"));
	}
}

// ============================================================================
// FORMAT DETECTION
// ============================================================================

FSMAARTImporter::EFileFormat FSMAARTImporter::DetectFormat(const FString& FilePath)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath))
	{
		return EFileFormat::Unknown;
	}
	return DetectFormatFromContent(Content);
}

FSMAARTImporter::EFileFormat FSMAARTImporter::DetectFormatFromContent(const FString& Content)
{
	// Check for SMAART header markers
	if (Content.Contains(TEXT("SMAART")) || Content.Contains(TEXT("Rational Acoustics")))
	{
		return EFileFormat::SMAARTTransferFunction;
	}

	// Check for REW header
	if (Content.Contains(TEXT("Room EQ Wizard")) || Content.Contains(TEXT("REW")))
	{
		return EFileFormat::REW;
	}

	// Check for SYSID markers
	if (Content.Contains(TEXT("SYSID")))
	{
		return EFileFormat::SYSID;
	}

	// Check if it looks like frequency data
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		// Skip empty lines and comments
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("*")))
		{
			continue;
		}

		// Try to parse first data line
		TCHAR Delimiter = DetectDelimiter(Line);
		TArray<FString> Columns = ParseLine(Line, Delimiter);

		// Check if first column looks like a frequency
		float FirstValue;
		if (Columns.Num() >= 2 && TryParseFloat(Columns[0], FirstValue))
		{
			if (FirstValue >= 20.0f && FirstValue <= 20000.0f)
			{
				return EFileFormat::GenericCSV;
			}
		}
		break;
	}

	return EFileFormat::Unknown;
}

FString FSMAARTImporter::GetFormatName(EFileFormat Format)
{
	switch (Format)
	{
	case EFileFormat::SMAARTTransferFunction:
		return TEXT("SMAART Transfer Function");
	case EFileFormat::SMAARTCSV:
		return TEXT("SMAART CSV");
	case EFileFormat::SYSID:
		return TEXT("SYSID");
	case EFileFormat::REW:
		return TEXT("Room EQ Wizard");
	case EFileFormat::GenericCSV:
		return TEXT("Generic CSV");
	default:
		return TEXT("Unknown");
	}
}

TArray<FString> FSMAARTImporter::GetSupportedExtensions()
{
	return {
		TEXT("txt"),
		TEXT("csv"),
		TEXT("tsv"),
		TEXT("asc"),
		TEXT("frd"),
		TEXT("mdat")
	};
}

FString FSMAARTImporter::GetFileTypeFilter()
{
	return TEXT("Measurement Files (*.txt;*.csv;*.tsv;*.asc;*.frd)|*.txt;*.csv;*.tsv;*.asc;*.frd|All Files (*.*)|*.*");
}

// ============================================================================
// FORMAT-SPECIFIC PARSERS
// ============================================================================

FSMAARTImportResult FSMAARTImporter::ParseSMAARTTransferFunction(const FString& Content, const FString& SourceName)
{
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);

	if (Lines.Num() < 2)
	{
		return FSMAARTImportResult::Failure(TEXT("File contains no data"));
	}

	FSMAARTMeasurement Measurement;
	Measurement.SourceFile = SourceName;
	Measurement.Name = ExtractMeasurementName(SourceName, Content);
	Measurement.MeasurementType = ESMAARTMeasurementType::TransferFunction;

	// Parse metadata from header comments
	ParseSMAARTMetadata(Lines, Measurement);

	// Find first data line and detect format
	int32 DataStartLine = 0;
	TCHAR Delimiter = TEXT(',');
	FColumnMapping ColumnMap;

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FString& Line = Lines[i];

		// Skip empty lines and comments
		if (Line.IsEmpty() || Line.StartsWith(TEXT("*")) || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		// Check if this is a header line
		Delimiter = DetectDelimiter(Line);
		TArray<FString> Columns = ParseLine(Line, Delimiter);

		// Look for column headers
		bool bIsHeader = false;
		for (const FString& Col : Columns)
		{
			FString CleanCol = CleanValue(Col).ToLower();
			if (CleanCol.Contains(TEXT("freq")) || CleanCol.Contains(TEXT("hz")) ||
				CleanCol.Contains(TEXT("mag")) || CleanCol.Contains(TEXT("phase")))
			{
				bIsHeader = true;
				break;
			}
		}

		if (bIsHeader)
		{
			ColumnMap = IdentifyColumns(Columns);
			DataStartLine = i + 1;
			continue;
		}

		// Try parsing as data
		float TestValue;
		if (Columns.Num() >= 2 && TryParseFloat(Columns[0], TestValue))
		{
			// If we haven't found headers, assume standard format:
			// Column 0: Frequency, Column 1: Magnitude, Column 2: Phase (optional)
			if (!ColumnMap.IsValid())
			{
				ColumnMap.FrequencyColumn = 0;
				ColumnMap.MagnitudeColumn = 1;
				if (Columns.Num() > 2)
				{
					ColumnMap.PhaseColumn = 2;
				}
				if (Columns.Num() > 3)
				{
					ColumnMap.CoherenceColumn = 3;
				}
			}
			DataStartLine = i;
			break;
		}
	}

	if (!ColumnMap.IsValid())
	{
		return FSMAARTImportResult::Failure(TEXT("Could not identify frequency and magnitude columns"));
	}

	// Parse data lines
	float TotalCoherence = 0.0f;
	int32 CoherenceCount = 0;

	for (int32 i = DataStartLine; i < Lines.Num(); ++i)
	{
		const FString& Line = Lines[i];
		if (Line.IsEmpty() || Line.StartsWith(TEXT("*")) || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		TArray<FString> Columns = ParseLine(Line, Delimiter);

		FSMAARTFrequencyBin Bin;

		// Parse frequency
		if (!TryParseFloat(CleanValue(Columns[ColumnMap.FrequencyColumn]), Bin.FrequencyHz))
		{
			continue;
		}

		// Parse magnitude
		if (!TryParseFloat(CleanValue(Columns[ColumnMap.MagnitudeColumn]), Bin.MagnitudeDb))
		{
			continue;
		}

		// Parse phase (optional)
		if (ColumnMap.PhaseColumn >= 0 && ColumnMap.PhaseColumn < Columns.Num())
		{
			TryParseFloat(CleanValue(Columns[ColumnMap.PhaseColumn]), Bin.PhaseDegrees);
		}

		// Parse coherence (optional)
		if (ColumnMap.CoherenceColumn >= 0 && ColumnMap.CoherenceColumn < Columns.Num())
		{
			if (TryParseFloat(CleanValue(Columns[ColumnMap.CoherenceColumn]), Bin.Coherence))
			{
				TotalCoherence += Bin.Coherence;
				++CoherenceCount;
			}
		}

		// Validate frequency range
		if (Bin.FrequencyHz >= 1.0f && Bin.FrequencyHz <= 48000.0f)
		{
			Measurement.FrequencyBins.Add(Bin);
		}
	}

	if (Measurement.FrequencyBins.Num() == 0)
	{
		return FSMAARTImportResult::Failure(TEXT("No valid frequency data found"));
	}

	// Sort by frequency
	Measurement.FrequencyBins.Sort([](const FSMAARTFrequencyBin& A, const FSMAARTFrequencyBin& B)
	{
		return A.FrequencyHz < B.FrequencyHz;
	});

	// Calculate average coherence
	if (CoherenceCount > 0)
	{
		Measurement.AverageCoherence = TotalCoherence / CoherenceCount;
	}

	TArray<FSMAARTMeasurement> Results;
	Results.Add(Measurement);

	return FSMAARTImportResult::Success(Results);
}

FSMAARTImportResult FSMAARTImporter::ParseGenericCSV(const FString& Content, const FString& SourceName)
{
	// For generic CSV, we reuse the SMAART parser which handles most cases
	return ParseSMAARTTransferFunction(Content, SourceName);
}

FSMAARTImportResult FSMAARTImporter::ParseREWFormat(const FString& Content, const FString& SourceName)
{
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);

	if (Lines.Num() < 2)
	{
		return FSMAARTImportResult::Failure(TEXT("File contains no data"));
	}

	FSMAARTMeasurement Measurement;
	Measurement.SourceFile = SourceName;
	Measurement.Name = ExtractMeasurementName(SourceName, Content);
	Measurement.MeasurementType = ESMAARTMeasurementType::TransferFunction;

	// REW format typically uses space or tab delimited:
	// Freq(Hz)  SPL(dB)  Phase(degrees)
	// The header line often starts with *

	int32 DataStartLine = 0;
	TCHAR Delimiter = TEXT(' ');

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FString& Line = Lines[i];
		if (Line.IsEmpty())
		{
			continue;
		}

		// REW uses * for header comments
		if (Line.StartsWith(TEXT("*")))
		{
			// Parse REW metadata
			if (Line.Contains(TEXT("Measurement:")))
			{
				Measurement.Name = Line.RightChop(Line.Find(TEXT(":")) + 1).TrimStartAndEnd();
			}
			continue;
		}

		// Try parsing as data
		Delimiter = DetectDelimiter(Line);
		TArray<FString> Columns = ParseLine(Line, Delimiter);

		float TestFreq;
		if (Columns.Num() >= 2 && TryParseFloat(CleanValue(Columns[0]), TestFreq))
		{
			DataStartLine = i;
			break;
		}
	}

	// Parse data
	for (int32 i = DataStartLine; i < Lines.Num(); ++i)
	{
		const FString& Line = Lines[i];
		if (Line.IsEmpty() || Line.StartsWith(TEXT("*")))
		{
			continue;
		}

		TArray<FString> Columns = ParseLine(Line, Delimiter);
		if (Columns.Num() < 2)
		{
			continue;
		}

		FSMAARTFrequencyBin Bin;

		if (!TryParseFloat(CleanValue(Columns[0]), Bin.FrequencyHz))
		{
			continue;
		}
		if (!TryParseFloat(CleanValue(Columns[1]), Bin.MagnitudeDb))
		{
			continue;
		}

		if (Columns.Num() > 2)
		{
			TryParseFloat(CleanValue(Columns[2]), Bin.PhaseDegrees);
		}

		if (Bin.FrequencyHz >= 1.0f && Bin.FrequencyHz <= 48000.0f)
		{
			Measurement.FrequencyBins.Add(Bin);
		}
	}

	if (Measurement.FrequencyBins.Num() == 0)
	{
		return FSMAARTImportResult::Failure(TEXT("No valid frequency data found"));
	}

	// Sort by frequency
	Measurement.FrequencyBins.Sort([](const FSMAARTFrequencyBin& A, const FSMAARTFrequencyBin& B)
	{
		return A.FrequencyHz < B.FrequencyHz;
	});

	TArray<FSMAARTMeasurement> Results;
	Results.Add(Measurement);

	return FSMAARTImportResult::Success(Results);
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

TCHAR FSMAARTImporter::DetectDelimiter(const FString& Line)
{
	// Count occurrences of common delimiters
	int32 TabCount = 0;
	int32 CommaCount = 0;
	int32 SemicolonCount = 0;
	int32 SpaceCount = 0;
	bool bInQuotes = false;

	for (TCHAR Char : Line)
	{
		if (Char == TEXT('"'))
		{
			bInQuotes = !bInQuotes;
		}
		else if (!bInQuotes)
		{
			if (Char == TEXT('\t')) ++TabCount;
			else if (Char == TEXT(',')) ++CommaCount;
			else if (Char == TEXT(';')) ++SemicolonCount;
			else if (Char == TEXT(' ')) ++SpaceCount;
		}
	}

	// Tab takes precedence if present
	if (TabCount > 0) return TEXT('\t');
	if (CommaCount > 0) return TEXT(',');
	if (SemicolonCount > 0) return TEXT(';');
	if (SpaceCount > 0) return TEXT(' ');

	return TEXT(',');  // Default
}

TArray<FString> FSMAARTImporter::ParseLine(const FString& Line, TCHAR Delimiter)
{
	TArray<FString> Result;

	if (Delimiter == TEXT(' '))
	{
		// For space delimiter, collapse multiple spaces
		Line.ParseIntoArray(Result, TEXT(" "), true);
	}
	else
	{
		// For other delimiters, handle quoted strings
		FString Current;
		bool bInQuotes = false;

		for (TCHAR Char : Line)
		{
			if (Char == TEXT('"'))
			{
				bInQuotes = !bInQuotes;
			}
			else if (Char == Delimiter && !bInQuotes)
			{
				Result.Add(Current);
				Current.Empty();
			}
			else
			{
				Current.AppendChar(Char);
			}
		}

		if (!Current.IsEmpty() || Result.Num() > 0)
		{
			Result.Add(Current);
		}
	}

	return Result;
}

FString FSMAARTImporter::CleanValue(const FString& Value)
{
	FString Clean = Value.TrimStartAndEnd();

	// Remove surrounding quotes
	if (Clean.StartsWith(TEXT("\"")) && Clean.EndsWith(TEXT("\"")))
	{
		Clean = Clean.Mid(1, Clean.Len() - 2);
	}

	return Clean;
}

bool FSMAARTImporter::TryParseFloat(const FString& Value, float& OutResult)
{
	FString Clean = CleanValue(Value);

	// Handle European decimal format (comma as decimal separator)
	Clean.ReplaceInline(TEXT(","), TEXT("."));

	// Remove any non-numeric characters except minus and decimal point
	FString Numeric;
	bool bHasDecimal = false;
	bool bHasMinus = false;

	for (TCHAR Char : Clean)
	{
		if (Char >= TEXT('0') && Char <= TEXT('9'))
		{
			Numeric.AppendChar(Char);
		}
		else if (Char == TEXT('.') && !bHasDecimal)
		{
			Numeric.AppendChar(Char);
			bHasDecimal = true;
		}
		else if (Char == TEXT('-') && Numeric.IsEmpty() && !bHasMinus)
		{
			Numeric.AppendChar(Char);
			bHasMinus = true;
		}
		else if (Char == TEXT('e') || Char == TEXT('E'))
		{
			// Scientific notation
			Numeric.AppendChar(Char);
		}
		else if ((Char == TEXT('+') || Char == TEXT('-')) &&
				 (Numeric.EndsWith(TEXT("e")) || Numeric.EndsWith(TEXT("E"))))
		{
			// Sign after exponent
			Numeric.AppendChar(Char);
		}
	}

	if (Numeric.IsEmpty())
	{
		return false;
	}

	OutResult = FCString::Atof(*Numeric);
	return true;
}

FSMAARTImporter::FColumnMapping FSMAARTImporter::IdentifyColumns(const TArray<FString>& HeaderColumns)
{
	FColumnMapping Mapping;

	for (int32 i = 0; i < HeaderColumns.Num(); ++i)
	{
		FString Header = CleanValue(HeaderColumns[i]).ToLower();

		if (Header.Contains(TEXT("freq")) || Header.Contains(TEXT("hz")))
		{
			if (!Header.Contains(TEXT("phase")))  // Avoid matching "phase (Hz)" type headers
			{
				Mapping.FrequencyColumn = i;
			}
		}
		else if (Header.Contains(TEXT("mag")) || Header.Contains(TEXT("spl")) ||
				 Header.Contains(TEXT("level")) || Header.Contains(TEXT("db")))
		{
			if (!Header.Contains(TEXT("phase")))
			{
				Mapping.MagnitudeColumn = i;
			}
		}
		else if (Header.Contains(TEXT("phase")) || Header.Contains(TEXT("deg")))
		{
			Mapping.PhaseColumn = i;
		}
		else if (Header.Contains(TEXT("coher")) || Header.Contains(TEXT("coh")))
		{
			Mapping.CoherenceColumn = i;
		}
	}

	return Mapping;
}

FString FSMAARTImporter::ExtractMeasurementName(const FString& FilePath, const FString& Content)
{
	// Try to get name from filename
	FString Name = FPaths::GetBaseFilename(FilePath);

	// Clean up common suffixes
	Name.ReplaceInline(TEXT("_TF"), TEXT(""));
	Name.ReplaceInline(TEXT("_transfer_function"), TEXT(""));
	Name.ReplaceInline(TEXT("_measurement"), TEXT(""));

	return Name;
}

void FSMAARTImporter::ParseSMAARTMetadata(const TArray<FString>& Lines, FSMAARTMeasurement& OutMeasurement)
{
	for (const FString& Line : Lines)
	{
		if (!Line.StartsWith(TEXT("*")) && !Line.StartsWith(TEXT("#")))
		{
			break;  // Stop at first data line
		}

		FString MetaLine = Line.RightChop(1).TrimStartAndEnd();

		// Parse common SMAART metadata
		if (MetaLine.StartsWith(TEXT("Name:")))
		{
			OutMeasurement.Name = MetaLine.RightChop(5).TrimStartAndEnd();
		}
		else if (MetaLine.StartsWith(TEXT("Date:")))
		{
			// Try to parse date
			FString DateStr = MetaLine.RightChop(5).TrimStartAndEnd();
			FDateTime::Parse(DateStr, OutMeasurement.Timestamp);
		}
		else if (MetaLine.StartsWith(TEXT("Reference:")))
		{
			TryParseFloat(MetaLine.RightChop(10), OutMeasurement.ReferenceLevelDb);
		}
		else if (MetaLine.StartsWith(TEXT("Delay:")))
		{
			TryParseFloat(MetaLine.RightChop(6), OutMeasurement.DetectedDelayMs);
		}
	}
}
