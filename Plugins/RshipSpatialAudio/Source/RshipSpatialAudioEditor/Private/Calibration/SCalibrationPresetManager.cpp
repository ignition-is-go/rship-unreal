// Copyright Rocketship. All Rights Reserved.

#include "Calibration/SCalibrationPresetManager.h"
#include "RshipSpatialAudioManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

FCalibrationPresetManager::FCalibrationPresetManager()
{
}

FCalibrationPresetManager::~FCalibrationPresetManager()
{
}

// ============================================================================
// IMPORT
// ============================================================================

FSMAARTImportResult FCalibrationPresetManager::ImportMeasurements(const TArray<FString>& FilePaths)
{
	FSMAARTImportResult Result = Importer.ImportFromFiles(FilePaths);

	if (Result.bSuccess)
	{
		ImportedMeasurements.Append(Result.Measurements);
	}

	return Result;
}

FSMAARTImportResult FCalibrationPresetManager::ImportFromClipboard()
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	if (ClipboardContent.IsEmpty())
	{
		return FSMAARTImportResult::Failure(TEXT("Clipboard is empty"));
	}

	FSMAARTImportResult Result = Importer.ImportFromString(ClipboardContent, TEXT("Clipboard"));

	if (Result.bSuccess)
	{
		ImportedMeasurements.Append(Result.Measurements);
	}

	return Result;
}

// ============================================================================
// AUTO-EQ GENERATION
// ============================================================================

FAutoEQResult FCalibrationPresetManager::GenerateAutoEQ(const FSMAARTMeasurement& Measurement, const FAutoEQSettings& Settings)
{
	FAutoEQResult Result;

	if (Measurement.FrequencyBins.Num() < 10)
	{
		Result.bSuccess = false;
		Result.Message = TEXT("Insufficient measurement data (need at least 10 frequency points)");
		return Result;
	}

	// Smooth the measurement if requested
	FSMAARTMeasurement WorkingMeasurement = Settings.bSmoothMeasurement
		? SmoothMeasurement(Measurement, Settings.SmoothingOctaves)
		: Measurement;

	// Find deviations from target
	TArray<FFrequencyDeviation> Deviations = FindDeviations(
		WorkingMeasurement,
		Settings.Target,
		1.5f  // 1.5 dB threshold for detection
	);

	// Filter by coherence
	Deviations.RemoveAll([&Settings](const FFrequencyDeviation& Dev)
	{
		return Dev.Coherence < Settings.CoherenceThreshold;
	});

	// Sort by magnitude of deviation (most problematic first)
	Deviations.Sort([](const FFrequencyDeviation& A, const FFrequencyDeviation& B)
	{
		return FMath::Abs(A.DeviationDb) > FMath::Abs(B.DeviationDb);
	});

	// If preferring cuts, prioritize peaks over dips
	if (Settings.bPreferCuts)
	{
		Deviations.Sort([](const FFrequencyDeviation& A, const FFrequencyDeviation& B)
		{
			// Peaks (positive deviation) get higher priority when preferring cuts
			if (A.bIsPeak != B.bIsPeak)
			{
				return A.bIsPeak;
			}
			return FMath::Abs(A.DeviationDb) > FMath::Abs(B.DeviationDb);
		});
	}

	// Generate correction bands for top deviations
	TArray<FSpatialEQBand> GeneratedBands;

	for (int32 i = 0; i < FMath::Min(Deviations.Num(), Settings.MaxBands); ++i)
	{
		const FFrequencyDeviation& Deviation = Deviations[i];

		// Skip if deviation is below threshold after sorting
		if (FMath::Abs(Deviation.DeviationDb) < 2.0f)
		{
			continue;
		}

		// Skip if outside frequency limits
		if (Deviation.FrequencyHz < Settings.Target.LowFrequencyLimitHz ||
			Deviation.FrequencyHz > Settings.Target.HighFrequencyLimitHz)
		{
			continue;
		}

		FSpatialEQBand Band = CreateCorrectionBand(Deviation, Settings);
		GeneratedBands.Add(Band);
	}

	// Merge overlapping bands
	GeneratedBands = MergeOverlappingBands(GeneratedBands);

	// Optimize Q values
	OptimizeQValues(GeneratedBands, WorkingMeasurement, Settings.Target);

	// Suggest high-pass if requested
	if (Settings.bSuggestHighPass)
	{
		Result.SuggestedHighPass = SuggestHighPass(WorkingMeasurement, Settings);
	}

	Result.EQBands = GeneratedBands;

	// Calculate correction quality
	FSMAARTMeasurement SimulatedResult = SimulateEQApplication(WorkingMeasurement, GeneratedBands);
	FDeviationStats Stats = AnalyzeDeviation(SimulatedResult, Settings.Target);

	Result.PredictedDeviation = Stats.RMSDeviation;
	Result.CorrectionQuality = FMath::Max(0.0f, 1.0f - (Stats.RMSDeviation / 12.0f));  // 12dB = 0 quality

	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Generated %d EQ bands. Predicted RMS deviation: %.1f dB"),
		GeneratedBands.Num(), Stats.RMSDeviation);

	return Result;
}

FAutoEQResult FCalibrationPresetManager::GenerateAutoEQ(const FSMAARTMeasurement& Measurement)
{
	FAutoEQSettings DefaultSettings;
	return GenerateAutoEQ(Measurement, DefaultSettings);
}

// ============================================================================
// PRESET MANAGEMENT
// ============================================================================

FSpeakerCalibrationPreset FCalibrationPresetManager::CreatePreset(
	const FSMAARTMeasurement& Measurement,
	const FGuid& SpeakerId,
	const FString& SpeakerName,
	const FAutoEQSettings& Settings)
{
	FSpeakerCalibrationPreset Preset;

	Preset.Name = FString::Printf(TEXT("%s Calibration"), *SpeakerName);
	Preset.SpeakerId = SpeakerId;
	Preset.SpeakerName = SpeakerName;
	Preset.Created = FDateTime::UtcNow();
	Preset.Modified = Preset.Created;

	Preset.Measurement = Measurement;
	Preset.AutoEQSettings = Settings;

	// Generate corrections
	FAutoEQResult EQResult = GenerateAutoEQ(Measurement, Settings);
	if (EQResult.bSuccess)
	{
		Preset.GeneratedEQBands = EQResult.EQBands;
		Preset.SuggestedHighPass = EQResult.SuggestedHighPass;
		Preset.SuggestedLowPass = EQResult.SuggestedLowPass;
	}

	// Set delay from measurement
	Preset.SuggestedDelayMs = Measurement.DetectedDelayMs;

	// Calculate gain offset
	float AvgMag = Measurement.GetAverageMagnitudeInBand(500.0f, 2000.0f);
	Preset.SuggestedGainDb = -AvgMag;

	return Preset;
}

const FVenueCalibrationSet* FCalibrationPresetManager::GetVenueCalibrationSet(const FString& VenueName) const
{
	return VenueCalibrations.Find(VenueName);
}

FVenueCalibrationSet* FCalibrationPresetManager::GetVenueCalibrationSet(const FString& VenueName)
{
	return VenueCalibrations.Find(VenueName);
}

FVenueCalibrationSet& FCalibrationPresetManager::GetOrCreateVenueCalibrationSet(const FString& VenueName)
{
	FVenueCalibrationSet* Existing = VenueCalibrations.Find(VenueName);
	if (Existing)
	{
		return *Existing;
	}

	FVenueCalibrationSet NewSet;
	NewSet.Name = VenueName;
	NewSet.VenueName = VenueName;
	NewSet.Created = FDateTime::UtcNow();
	NewSet.Modified = NewSet.Created;

	return VenueCalibrations.Add(VenueName, NewSet);
}

TArray<FString> FCalibrationPresetManager::GetVenueNames() const
{
	TArray<FString> Names;
	VenueCalibrations.GetKeys(Names);
	return Names;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

FString FCalibrationPresetManager::GetCalibrationFileFilter()
{
	return TEXT("Calibration Files (*.rcal)|*.rcal|JSON Files (*.json)|*.json|All Files (*.*)|*.*");
}

bool FCalibrationPresetManager::SaveCalibrationSet(const FString& VenueName, const FString& FilePath)
{
	const FVenueCalibrationSet* CalSet = GetVenueCalibrationSet(VenueName);
	if (!CalSet)
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("version"), TEXT("1.0"));
	RootObject->SetStringField(TEXT("type"), TEXT("venue_calibration"));
	RootObject->SetStringField(TEXT("name"), CalSet->Name);
	RootObject->SetStringField(TEXT("venue"), CalSet->VenueName);
	RootObject->SetStringField(TEXT("created"), CalSet->Created.ToIso8601());
	RootObject->SetStringField(TEXT("modified"), CalSet->Modified.ToIso8601());
	RootObject->SetStringField(TEXT("notes"), CalSet->Notes);
	RootObject->SetNumberField(TEXT("reference_spl"), CalSet->ReferenceLevelSPL);
	RootObject->SetStringField(TEXT("reference_delay_speaker"), CalSet->ReferenceDelaySpeakerId.ToString());

	// Serialize speaker presets
	TArray<TSharedPtr<FJsonValue>> PresetArray;
	for (const auto& Pair : CalSet->SpeakerPresets)
	{
		TSharedPtr<FJsonObject> PresetObj = MakeShared<FJsonObject>();

		PresetObj->SetStringField(TEXT("speaker_id"), Pair.Key.ToString());
		PresetObj->SetStringField(TEXT("speaker_name"), Pair.Value.SpeakerName);
		PresetObj->SetStringField(TEXT("name"), Pair.Value.Name);
		PresetObj->SetNumberField(TEXT("delay_ms"), Pair.Value.SuggestedDelayMs);
		PresetObj->SetNumberField(TEXT("gain_db"), Pair.Value.SuggestedGainDb);

		// EQ bands
		TArray<TSharedPtr<FJsonValue>> EQArray;
		for (const FSpatialEQBand& Band : Pair.Value.GeneratedEQBands)
		{
			TSharedPtr<FJsonObject> BandObj = MakeShared<FJsonObject>();
			BandObj->SetBoolField(TEXT("enabled"), Band.bEnabled);
			BandObj->SetNumberField(TEXT("type"), static_cast<int32>(Band.Type));
			BandObj->SetNumberField(TEXT("frequency"), Band.FrequencyHz);
			BandObj->SetNumberField(TEXT("gain"), Band.GainDb);
			BandObj->SetNumberField(TEXT("q"), Band.Q);
			EQArray.Add(MakeShared<FJsonValueObject>(BandObj));
		}
		PresetObj->SetArrayField(TEXT("eq_bands"), EQArray);

		// Measurement data (frequency bins)
		TArray<TSharedPtr<FJsonValue>> MeasurementArray;
		for (const FSMAARTFrequencyBin& Bin : Pair.Value.Measurement.FrequencyBins)
		{
			TSharedPtr<FJsonObject> BinObj = MakeShared<FJsonObject>();
			BinObj->SetNumberField(TEXT("f"), Bin.FrequencyHz);
			BinObj->SetNumberField(TEXT("m"), Bin.MagnitudeDb);
			BinObj->SetNumberField(TEXT("p"), Bin.PhaseDegrees);
			BinObj->SetNumberField(TEXT("c"), Bin.Coherence);
			MeasurementArray.Add(MakeShared<FJsonValueObject>(BinObj));
		}
		PresetObj->SetArrayField(TEXT("measurement"), MeasurementArray);

		PresetArray.Add(MakeShared<FJsonValueObject>(PresetObj));
	}
	RootObject->SetArrayField(TEXT("presets"), PresetArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString, 0);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

bool FCalibrationPresetManager::LoadCalibrationSet(const FString& FilePath, FString& OutVenueName)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	FVenueCalibrationSet CalSet;
	CalSet.Name = RootObject->GetStringField(TEXT("name"));
	CalSet.VenueName = RootObject->GetStringField(TEXT("venue"));
	CalSet.Notes = RootObject->GetStringField(TEXT("notes"));
	CalSet.ReferenceLevelSPL = RootObject->GetNumberField(TEXT("reference_spl"));

	FString CreatedStr = RootObject->GetStringField(TEXT("created"));
	FString ModifiedStr = RootObject->GetStringField(TEXT("modified"));
	FDateTime::ParseIso8601(*CreatedStr, CalSet.Created);
	FDateTime::ParseIso8601(*ModifiedStr, CalSet.Modified);

	FString RefSpeakerStr = RootObject->GetStringField(TEXT("reference_delay_speaker"));
	FGuid::Parse(RefSpeakerStr, CalSet.ReferenceDelaySpeakerId);

	// Parse presets
	const TArray<TSharedPtr<FJsonValue>>* PresetArray;
	if (RootObject->TryGetArrayField(TEXT("presets"), PresetArray))
	{
		for (const TSharedPtr<FJsonValue>& PresetValue : *PresetArray)
		{
			TSharedPtr<FJsonObject> PresetObj = PresetValue->AsObject();
			if (!PresetObj.IsValid())
			{
				continue;
			}

			FSpeakerCalibrationPreset Preset;

			FString SpeakerIdStr = PresetObj->GetStringField(TEXT("speaker_id"));
			FGuid::Parse(SpeakerIdStr, Preset.SpeakerId);

			Preset.SpeakerName = PresetObj->GetStringField(TEXT("speaker_name"));
			Preset.Name = PresetObj->GetStringField(TEXT("name"));
			Preset.SuggestedDelayMs = PresetObj->GetNumberField(TEXT("delay_ms"));
			Preset.SuggestedGainDb = PresetObj->GetNumberField(TEXT("gain_db"));

			// Parse EQ bands
			const TArray<TSharedPtr<FJsonValue>>* EQArray;
			if (PresetObj->TryGetArrayField(TEXT("eq_bands"), EQArray))
			{
				for (const TSharedPtr<FJsonValue>& BandValue : *EQArray)
				{
					TSharedPtr<FJsonObject> BandObj = BandValue->AsObject();
					if (!BandObj.IsValid())
					{
						continue;
					}

					FSpatialEQBand Band;
					Band.bEnabled = BandObj->GetBoolField(TEXT("enabled"));
					Band.Type = static_cast<ESpatialEQBandType>(BandObj->GetIntegerField(TEXT("type")));
					Band.FrequencyHz = BandObj->GetNumberField(TEXT("frequency"));
					Band.GainDb = BandObj->GetNumberField(TEXT("gain"));
					Band.Q = BandObj->GetNumberField(TEXT("q"));

					Preset.GeneratedEQBands.Add(Band);
				}
			}

			// Parse measurement data
			const TArray<TSharedPtr<FJsonValue>>* MeasurementArray;
			if (PresetObj->TryGetArrayField(TEXT("measurement"), MeasurementArray))
			{
				for (const TSharedPtr<FJsonValue>& BinValue : *MeasurementArray)
				{
					TSharedPtr<FJsonObject> BinObj = BinValue->AsObject();
					if (!BinObj.IsValid())
					{
						continue;
					}

					FSMAARTFrequencyBin Bin;
					Bin.FrequencyHz = BinObj->GetNumberField(TEXT("f"));
					Bin.MagnitudeDb = BinObj->GetNumberField(TEXT("m"));
					Bin.PhaseDegrees = BinObj->GetNumberField(TEXT("p"));
					Bin.Coherence = BinObj->GetNumberField(TEXT("c"));

					Preset.Measurement.FrequencyBins.Add(Bin);
				}
			}

			CalSet.SpeakerPresets.Add(Preset.SpeakerId, Preset);
		}
	}

	OutVenueName = CalSet.VenueName;
	VenueCalibrations.Add(CalSet.VenueName, CalSet);

	return true;
}

bool FCalibrationPresetManager::ExportPreset(const FSpeakerCalibrationPreset& Preset, const FString& FilePath)
{
	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("version"), TEXT("1.0"));
	RootObject->SetStringField(TEXT("type"), TEXT("speaker_calibration"));
	RootObject->SetStringField(TEXT("name"), Preset.Name);
	RootObject->SetStringField(TEXT("speaker_id"), Preset.SpeakerId.ToString());
	RootObject->SetStringField(TEXT("speaker_name"), Preset.SpeakerName);
	RootObject->SetNumberField(TEXT("delay_ms"), Preset.SuggestedDelayMs);
	RootObject->SetNumberField(TEXT("gain_db"), Preset.SuggestedGainDb);

	// EQ bands
	TArray<TSharedPtr<FJsonValue>> EQArray;
	for (const FSpatialEQBand& Band : Preset.GeneratedEQBands)
	{
		TSharedPtr<FJsonObject> BandObj = MakeShared<FJsonObject>();
		BandObj->SetBoolField(TEXT("enabled"), Band.bEnabled);
		BandObj->SetNumberField(TEXT("type"), static_cast<int32>(Band.Type));
		BandObj->SetNumberField(TEXT("frequency"), Band.FrequencyHz);
		BandObj->SetNumberField(TEXT("gain"), Band.GainDb);
		BandObj->SetNumberField(TEXT("q"), Band.Q);
		EQArray.Add(MakeShared<FJsonValueObject>(BandObj));
	}
	RootObject->SetArrayField(TEXT("eq_bands"), EQArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString, 0);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

bool FCalibrationPresetManager::ImportPreset(const FString& FilePath, FSpeakerCalibrationPreset& OutPreset)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	OutPreset.Name = RootObject->GetStringField(TEXT("name"));

	FString SpeakerIdStr = RootObject->GetStringField(TEXT("speaker_id"));
	FGuid::Parse(SpeakerIdStr, OutPreset.SpeakerId);

	OutPreset.SpeakerName = RootObject->GetStringField(TEXT("speaker_name"));
	OutPreset.SuggestedDelayMs = RootObject->GetNumberField(TEXT("delay_ms"));
	OutPreset.SuggestedGainDb = RootObject->GetNumberField(TEXT("gain_db"));

	// Parse EQ bands
	const TArray<TSharedPtr<FJsonValue>>* EQArray;
	if (RootObject->TryGetArrayField(TEXT("eq_bands"), EQArray))
	{
		for (const TSharedPtr<FJsonValue>& BandValue : *EQArray)
		{
			TSharedPtr<FJsonObject> BandObj = BandValue->AsObject();
			if (!BandObj.IsValid())
			{
				continue;
			}

			FSpatialEQBand Band;
			Band.bEnabled = BandObj->GetBoolField(TEXT("enabled"));
			Band.Type = static_cast<ESpatialEQBandType>(BandObj->GetIntegerField(TEXT("type")));
			Band.FrequencyHz = BandObj->GetNumberField(TEXT("frequency"));
			Band.GainDb = BandObj->GetNumberField(TEXT("gain"));
			Band.Q = BandObj->GetNumberField(TEXT("q"));

			OutPreset.GeneratedEQBands.Add(Band);
		}
	}

	return true;
}

// ============================================================================
// APPLICATION
// ============================================================================

bool FCalibrationPresetManager::ApplyPresetToSpeaker(
	URshipSpatialAudioManager* Manager,
	const FGuid& SpeakerId,
	const FSpeakerCalibrationPreset& Preset)
{
	if (!Manager)
	{
		return false;
	}

	// Generate DSP state from preset
	FSpatialSpeakerDSPState DSPState = Preset.GenerateDSPState();

	// Apply via manager
	Manager->SetSpeakerDSP(SpeakerId, DSPState);

	return true;
}

int32 FCalibrationPresetManager::ApplyVenueCalibration(
	URshipSpatialAudioManager* Manager,
	const FVenueCalibrationSet& CalibrationSet)
{
	if (!Manager)
	{
		return 0;
	}

	int32 AppliedCount = 0;

	for (const auto& Pair : CalibrationSet.SpeakerPresets)
	{
		if (ApplyPresetToSpeaker(Manager, Pair.Key, Pair.Value))
		{
			++AppliedCount;
		}
	}

	return AppliedCount;
}

// ============================================================================
// ANALYSIS
// ============================================================================

FCalibrationPresetManager::FDeviationStats FCalibrationPresetManager::AnalyzeDeviation(
	const FSMAARTMeasurement& Measurement,
	const FCalibrationTarget& Target)
{
	FDeviationStats Stats;

	if (Measurement.FrequencyBins.Num() == 0)
	{
		return Stats;
	}

	float TotalDeviation = 0.0f;
	float TotalSquaredDeviation = 0.0f;
	Stats.MinDeviation = FLT_MAX;
	Stats.MaxDeviation = -FLT_MAX;

	for (const FSMAARTFrequencyBin& Bin : Measurement.FrequencyBins)
	{
		// Only analyze within target limits
		if (Bin.FrequencyHz < Target.LowFrequencyLimitHz ||
			Bin.FrequencyHz > Target.HighFrequencyLimitHz)
		{
			continue;
		}

		float TargetMag = Target.GetTargetMagnitudeAtFrequency(Bin.FrequencyHz);
		float Deviation = Bin.MagnitudeDb - TargetMag;

		Stats.DeviationPerFrequency.Add(MakeTuple(Bin.FrequencyHz, Deviation));

		TotalDeviation += FMath::Abs(Deviation);
		TotalSquaredDeviation += Deviation * Deviation;
		Stats.MinDeviation = FMath::Min(Stats.MinDeviation, Deviation);
		Stats.MaxDeviation = FMath::Max(Stats.MaxDeviation, Deviation);
	}

	int32 Count = Stats.DeviationPerFrequency.Num();
	if (Count > 0)
	{
		Stats.AverageDeviation = TotalDeviation / Count;
		Stats.RMSDeviation = FMath::Sqrt(TotalSquaredDeviation / Count);
	}

	return Stats;
}

FSMAARTMeasurement FCalibrationPresetManager::SimulateEQApplication(
	const FSMAARTMeasurement& Measurement,
	const TArray<FSpatialEQBand>& EQBands)
{
	FSMAARTMeasurement Result = Measurement;

	for (FSMAARTFrequencyBin& Bin : Result.FrequencyBins)
	{
		float TotalGain = 0.0f;

		for (const FSpatialEQBand& Band : EQBands)
		{
			if (!Band.bEnabled)
			{
				continue;
			}

			// Calculate gain contribution from this band
			// Using standard parametric EQ response
			float FreqRatio = Bin.FrequencyHz / Band.FrequencyHz;
			float LogRatio = FMath::Log2(FreqRatio);

			// Simplified bell curve response
			float BandwidthOctaves = 1.0f / Band.Q;  // Approximation
			float NormalizedOffset = LogRatio / (BandwidthOctaves * 0.5f);

			float Attenuation = 1.0f / (1.0f + NormalizedOffset * NormalizedOffset);
			TotalGain += Band.GainDb * Attenuation;
		}

		Bin.MagnitudeDb += TotalGain;
	}

	return Result;
}

// ============================================================================
// AUTO-EQ ALGORITHMS
// ============================================================================

FSMAARTMeasurement FCalibrationPresetManager::SmoothMeasurement(const FSMAARTMeasurement& Measurement, float OctaveFraction)
{
	FSMAARTMeasurement Smoothed = Measurement;

	if (Measurement.FrequencyBins.Num() < 3 || OctaveFraction <= 0.0f)
	{
		return Smoothed;
	}

	TArray<float> SmoothedMagnitudes;
	SmoothedMagnitudes.SetNum(Measurement.FrequencyBins.Num());

	for (int32 i = 0; i < Measurement.FrequencyBins.Num(); ++i)
	{
		float CenterFreq = Measurement.FrequencyBins[i].FrequencyHz;
		float LowFreq = CenterFreq * FMath::Pow(2.0f, -OctaveFraction / 2.0f);
		float HighFreq = CenterFreq * FMath::Pow(2.0f, OctaveFraction / 2.0f);

		float Sum = 0.0f;
		float WeightSum = 0.0f;

		for (int32 j = 0; j < Measurement.FrequencyBins.Num(); ++j)
		{
			float Freq = Measurement.FrequencyBins[j].FrequencyHz;
			if (Freq >= LowFreq && Freq <= HighFreq)
			{
				// Weight by coherence
				float Weight = Measurement.FrequencyBins[j].Coherence;
				Sum += Measurement.FrequencyBins[j].MagnitudeDb * Weight;
				WeightSum += Weight;
			}
		}

		SmoothedMagnitudes[i] = WeightSum > 0.0f ? Sum / WeightSum : Measurement.FrequencyBins[i].MagnitudeDb;
	}

	for (int32 i = 0; i < Smoothed.FrequencyBins.Num(); ++i)
	{
		Smoothed.FrequencyBins[i].MagnitudeDb = SmoothedMagnitudes[i];
	}

	return Smoothed;
}

TArray<FCalibrationPresetManager::FFrequencyDeviation> FCalibrationPresetManager::FindDeviations(
	const FSMAARTMeasurement& Measurement,
	const FCalibrationTarget& Target,
	float ThresholdDb)
{
	TArray<FFrequencyDeviation> Deviations;

	if (Measurement.FrequencyBins.Num() < 3)
	{
		return Deviations;
	}

	// Find local maxima and minima that deviate from target
	for (int32 i = 1; i < Measurement.FrequencyBins.Num() - 1; ++i)
	{
		const FSMAARTFrequencyBin& Prev = Measurement.FrequencyBins[i - 1];
		const FSMAARTFrequencyBin& Curr = Measurement.FrequencyBins[i];
		const FSMAARTFrequencyBin& Next = Measurement.FrequencyBins[i + 1];

		float TargetMag = Target.GetTargetMagnitudeAtFrequency(Curr.FrequencyHz);
		float Deviation = Curr.MagnitudeDb - TargetMag;

		// Check if this is a local peak
		bool bIsPeak = Curr.MagnitudeDb > Prev.MagnitudeDb && Curr.MagnitudeDb > Next.MagnitudeDb;
		// Check if this is a local dip
		bool bIsDip = Curr.MagnitudeDb < Prev.MagnitudeDb && Curr.MagnitudeDb < Next.MagnitudeDb;

		if ((bIsPeak || bIsDip) && FMath::Abs(Deviation) >= ThresholdDb)
		{
			FFrequencyDeviation Dev;
			Dev.FrequencyHz = Curr.FrequencyHz;
			Dev.DeviationDb = Deviation;
			Dev.Coherence = Curr.Coherence;
			Dev.bIsPeak = bIsPeak;

			// Estimate bandwidth
			float PeakHeight = FMath::Abs(Deviation);
			float HalfPowerLevel = Curr.MagnitudeDb - (bIsPeak ? 3.0f : -3.0f);

			// Find -3dB points
			int32 LowIndex = i, HighIndex = i;

			for (int32 j = i - 1; j >= 0; --j)
			{
				if ((bIsPeak && Measurement.FrequencyBins[j].MagnitudeDb <= HalfPowerLevel) ||
					(!bIsPeak && Measurement.FrequencyBins[j].MagnitudeDb >= HalfPowerLevel))
				{
					LowIndex = j;
					break;
				}
			}

			for (int32 j = i + 1; j < Measurement.FrequencyBins.Num(); ++j)
			{
				if ((bIsPeak && Measurement.FrequencyBins[j].MagnitudeDb <= HalfPowerLevel) ||
					(!bIsPeak && Measurement.FrequencyBins[j].MagnitudeDb >= HalfPowerLevel))
				{
					HighIndex = j;
					break;
				}
			}

			float LowFreq = Measurement.FrequencyBins[LowIndex].FrequencyHz;
			float HighFreq = Measurement.FrequencyBins[HighIndex].FrequencyHz;
			Dev.Bandwidth = FMath::Log2(HighFreq / LowFreq);

			Deviations.Add(Dev);
		}
	}

	return Deviations;
}

FSpatialEQBand FCalibrationPresetManager::CreateCorrectionBand(
	const FFrequencyDeviation& Deviation,
	const FAutoEQSettings& Settings)
{
	FSpatialEQBand Band;

	Band.bEnabled = true;
	Band.Type = ESpatialEQBandType::Peak;
	Band.FrequencyHz = Deviation.FrequencyHz;

	// Correction is opposite of deviation (cut peaks, boost dips)
	float CorrectionGain = -Deviation.DeviationDb;

	// Clamp to maximum allowed gain
	CorrectionGain = FMath::Clamp(CorrectionGain, -Settings.MaxGainDb, Settings.MaxGainDb);

	// If preferring cuts and this would be a boost, reduce it
	if (Settings.bPreferCuts && CorrectionGain > 0)
	{
		CorrectionGain *= 0.5f;  // Only boost half as much
	}

	Band.GainDb = CorrectionGain;

	// Calculate Q from bandwidth
	if (Deviation.Bandwidth > 0.001f)
	{
		// Q ≈ f0 / BW where BW is -3dB bandwidth
		// For octave bandwidth: Q ≈ sqrt(2^BW) / (2^BW - 1)
		float BW = FMath::Pow(2.0f, Deviation.Bandwidth);
		Band.Q = FMath::Sqrt(BW) / (BW - 1.0f);
	}
	else
	{
		Band.Q = 4.0f;  // Default narrow Q
	}

	// Clamp Q
	Band.Q = FMath::Clamp(Band.Q, Settings.MinQ, Settings.MaxQ);

	Band.Label = FString::Printf(TEXT("%.0f Hz"), Band.FrequencyHz);

	return Band;
}

TArray<FSpatialEQBand> FCalibrationPresetManager::MergeOverlappingBands(
	const TArray<FSpatialEQBand>& Bands,
	float OverlapThresholdOctaves)
{
	if (Bands.Num() <= 1)
	{
		return Bands;
	}

	TArray<FSpatialEQBand> Merged;
	TArray<bool> Used;
	Used.SetNum(Bands.Num());

	for (int32 i = 0; i < Bands.Num(); ++i)
	{
		if (Used[i])
		{
			continue;
		}

		FSpatialEQBand MergedBand = Bands[i];
		Used[i] = true;

		// Look for overlapping bands
		for (int32 j = i + 1; j < Bands.Num(); ++j)
		{
			if (Used[j])
			{
				continue;
			}

			float OctaveDistance = FMath::Abs(FMath::Log2(Bands[j].FrequencyHz / MergedBand.FrequencyHz));
			if (OctaveDistance < OverlapThresholdOctaves)
			{
				// Merge: average frequency and gains
				float Weight1 = FMath::Abs(MergedBand.GainDb);
				float Weight2 = FMath::Abs(Bands[j].GainDb);
				float TotalWeight = Weight1 + Weight2;

				if (TotalWeight > 0.001f)
				{
					// Weighted geometric mean for frequency
					MergedBand.FrequencyHz = FMath::Pow(
						MergedBand.FrequencyHz * Bands[j].FrequencyHz,
						0.5f
					);
					// Sum gains (they might partially cancel)
					MergedBand.GainDb += Bands[j].GainDb;
					// Average Q
					MergedBand.Q = (MergedBand.Q * Weight1 + Bands[j].Q * Weight2) / TotalWeight;
				}

				Used[j] = true;
			}
		}

		// Only keep bands with significant correction
		if (FMath::Abs(MergedBand.GainDb) >= 0.5f)
		{
			Merged.Add(MergedBand);
		}
	}

	return Merged;
}

void FCalibrationPresetManager::OptimizeQValues(
	TArray<FSpatialEQBand>& Bands,
	const FSMAARTMeasurement& Measurement,
	const FCalibrationTarget& Target)
{
	// Simple optimization: try a few Q values and pick best
	const float QValues[] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };

	for (FSpatialEQBand& Band : Bands)
	{
		float BestQ = Band.Q;
		float BestDeviation = FLT_MAX;

		for (float TestQ : QValues)
		{
			Band.Q = TestQ;

			// Simulate EQ with just this band
			TArray<FSpatialEQBand> SingleBand = { Band };
			FSMAARTMeasurement Simulated = SimulateEQApplication(Measurement, SingleBand);

			// Calculate deviation around this frequency
			float LocalDeviation = 0.0f;
			int32 Count = 0;

			for (const FSMAARTFrequencyBin& Bin : Simulated.FrequencyBins)
			{
				float OctaveDistance = FMath::Abs(FMath::Log2(Bin.FrequencyHz / Band.FrequencyHz));
				if (OctaveDistance < 1.0f)  // Within 1 octave
				{
					float TargetMag = Target.GetTargetMagnitudeAtFrequency(Bin.FrequencyHz);
					LocalDeviation += FMath::Abs(Bin.MagnitudeDb - TargetMag);
					++Count;
				}
			}

			if (Count > 0)
			{
				LocalDeviation /= Count;
				if (LocalDeviation < BestDeviation)
				{
					BestDeviation = LocalDeviation;
					BestQ = TestQ;
				}
			}
		}

		Band.Q = BestQ;
	}
}

FSpatialHighPassFilter FCalibrationPresetManager::SuggestHighPass(
	const FSMAARTMeasurement& Measurement,
	const FAutoEQSettings& Settings)
{
	FSpatialHighPassFilter HPF;
	HPF.bEnabled = false;

	// Find where response drops significantly
	// This often indicates speaker's low-frequency limit

	float MinMag = FLT_MAX;
	float MinFreq = 0.0f;

	for (const FSMAARTFrequencyBin& Bin : Measurement.FrequencyBins)
	{
		if (Bin.FrequencyHz >= 20.0f && Bin.FrequencyHz <= 200.0f)
		{
			if (Bin.MagnitudeDb < MinMag)
			{
				MinMag = Bin.MagnitudeDb;
				MinFreq = Bin.FrequencyHz;
			}
		}
	}

	// Get average level in speech range
	float AvgMag = Measurement.GetAverageMagnitudeInBand(500.0f, 2000.0f);

	// If low frequency is significantly below average, suggest HPF
	if (AvgMag - MinMag > 12.0f && MinFreq > 20.0f)
	{
		HPF.bEnabled = true;
		HPF.FrequencyHz = MinFreq * 1.2f;  // Set slightly above problem area
		HPF.Slope = ESpatialFilterSlope::Slope24dB;
		HPF.FilterType = ESpatialFilterType::LinkwitzRiley;
	}

	return HPF;
}
