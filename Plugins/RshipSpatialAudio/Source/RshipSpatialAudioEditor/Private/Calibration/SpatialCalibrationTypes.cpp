// Copyright Rocketship. All Rights Reserved.

#include "Calibration/SpatialCalibrationTypes.h"

// ============================================================================
// FSMAARTMeasurement
// ============================================================================

float FSMAARTMeasurement::GetMagnitudeAtFrequency(float FreqHz) const
{
	if (FrequencyBins.Num() == 0)
	{
		return 0.0f;
	}

	// Find surrounding bins for interpolation
	int32 LowerIndex = -1;
	int32 UpperIndex = -1;

	for (int32 i = 0; i < FrequencyBins.Num(); ++i)
	{
		if (FrequencyBins[i].FrequencyHz <= FreqHz)
		{
			LowerIndex = i;
		}
		if (FrequencyBins[i].FrequencyHz >= FreqHz && UpperIndex < 0)
		{
			UpperIndex = i;
			break;
		}
	}

	// Exact match or edge cases
	if (LowerIndex < 0)
	{
		return FrequencyBins[0].MagnitudeDb;
	}
	if (UpperIndex < 0 || LowerIndex == UpperIndex)
	{
		return FrequencyBins[LowerIndex].MagnitudeDb;
	}

	// Logarithmic interpolation for frequency domain
	const float LowFreq = FrequencyBins[LowerIndex].FrequencyHz;
	const float HighFreq = FrequencyBins[UpperIndex].FrequencyHz;
	const float LowMag = FrequencyBins[LowerIndex].MagnitudeDb;
	const float HighMag = FrequencyBins[UpperIndex].MagnitudeDb;

	// Interpolate in log frequency space
	const float LogRatio = FMath::Loge(FreqHz / LowFreq) / FMath::Loge(HighFreq / LowFreq);
	return FMath::Lerp(LowMag, HighMag, LogRatio);
}

float FSMAARTMeasurement::GetPhaseAtFrequency(float FreqHz) const
{
	if (FrequencyBins.Num() == 0)
	{
		return 0.0f;
	}

	// Find surrounding bins for interpolation
	int32 LowerIndex = -1;
	int32 UpperIndex = -1;

	for (int32 i = 0; i < FrequencyBins.Num(); ++i)
	{
		if (FrequencyBins[i].FrequencyHz <= FreqHz)
		{
			LowerIndex = i;
		}
		if (FrequencyBins[i].FrequencyHz >= FreqHz && UpperIndex < 0)
		{
			UpperIndex = i;
			break;
		}
	}

	if (LowerIndex < 0)
	{
		return FrequencyBins[0].PhaseDegrees;
	}
	if (UpperIndex < 0 || LowerIndex == UpperIndex)
	{
		return FrequencyBins[LowerIndex].PhaseDegrees;
	}

	// Linear interpolation for phase (unwrap if needed)
	const float LowFreq = FrequencyBins[LowerIndex].FrequencyHz;
	const float HighFreq = FrequencyBins[UpperIndex].FrequencyHz;
	float LowPhase = FrequencyBins[LowerIndex].PhaseDegrees;
	float HighPhase = FrequencyBins[UpperIndex].PhaseDegrees;

	// Handle phase wrapping
	if (HighPhase - LowPhase > 180.0f)
	{
		HighPhase -= 360.0f;
	}
	else if (LowPhase - HighPhase > 180.0f)
	{
		LowPhase -= 360.0f;
	}

	const float LogRatio = FMath::Loge(FreqHz / LowFreq) / FMath::Loge(HighFreq / LowFreq);
	float Result = FMath::Lerp(LowPhase, HighPhase, LogRatio);

	// Wrap to -180 to +180
	while (Result > 180.0f) Result -= 360.0f;
	while (Result < -180.0f) Result += 360.0f;

	return Result;
}

float FSMAARTMeasurement::GetCoherenceAtFrequency(float FreqHz) const
{
	if (FrequencyBins.Num() == 0)
	{
		return 0.0f;
	}

	int32 LowerIndex = -1;
	int32 UpperIndex = -1;

	for (int32 i = 0; i < FrequencyBins.Num(); ++i)
	{
		if (FrequencyBins[i].FrequencyHz <= FreqHz)
		{
			LowerIndex = i;
		}
		if (FrequencyBins[i].FrequencyHz >= FreqHz && UpperIndex < 0)
		{
			UpperIndex = i;
			break;
		}
	}

	if (LowerIndex < 0)
	{
		return FrequencyBins[0].Coherence;
	}
	if (UpperIndex < 0 || LowerIndex == UpperIndex)
	{
		return FrequencyBins[LowerIndex].Coherence;
	}

	const float LowFreq = FrequencyBins[LowerIndex].FrequencyHz;
	const float HighFreq = FrequencyBins[UpperIndex].FrequencyHz;
	const float LogRatio = FMath::Loge(FreqHz / LowFreq) / FMath::Loge(HighFreq / LowFreq);

	return FMath::Lerp(FrequencyBins[LowerIndex].Coherence, FrequencyBins[UpperIndex].Coherence, LogRatio);
}

void FSMAARTMeasurement::GetFrequencyRange(float& OutMinHz, float& OutMaxHz) const
{
	if (FrequencyBins.Num() == 0)
	{
		OutMinHz = 0.0f;
		OutMaxHz = 0.0f;
		return;
	}

	OutMinHz = FrequencyBins[0].FrequencyHz;
	OutMaxHz = FrequencyBins.Last().FrequencyHz;
}

float FSMAARTMeasurement::GetAverageMagnitudeInBand(float LowHz, float HighHz) const
{
	if (FrequencyBins.Num() == 0 || LowHz >= HighHz)
	{
		return 0.0f;
	}

	float Sum = 0.0f;
	int32 Count = 0;

	for (const FSMAARTFrequencyBin& Bin : FrequencyBins)
	{
		if (Bin.FrequencyHz >= LowHz && Bin.FrequencyHz <= HighHz)
		{
			Sum += Bin.MagnitudeDb;
			++Count;
		}
	}

	return Count > 0 ? Sum / Count : 0.0f;
}

TArray<float> FSMAARTMeasurement::FindProblematicFrequencies(float DeviationThresholdDb, float CoherenceThreshold) const
{
	TArray<float> ProblematicFreqs;

	if (FrequencyBins.Num() < 3)
	{
		return ProblematicFreqs;
	}

	// Calculate average magnitude
	float AverageMag = 0.0f;
	for (const FSMAARTFrequencyBin& Bin : FrequencyBins)
	{
		AverageMag += Bin.MagnitudeDb;
	}
	AverageMag /= FrequencyBins.Num();

	// Find deviations
	for (const FSMAARTFrequencyBin& Bin : FrequencyBins)
	{
		if (Bin.Coherence >= CoherenceThreshold)
		{
			float Deviation = FMath::Abs(Bin.MagnitudeDb - AverageMag);
			if (Deviation > DeviationThresholdDb)
			{
				ProblematicFreqs.Add(Bin.FrequencyHz);
			}
		}
	}

	return ProblematicFreqs;
}

// ============================================================================
// FCalibrationTarget
// ============================================================================

float FCalibrationTarget::GetTargetMagnitudeAtFrequency(float FreqHz) const
{
	switch (CurveType)
	{
	case ECalibrationTargetCurve::Flat:
		return 0.0f;

	case ECalibrationTargetCurve::XCurve:
		// X-Curve: Flat to 2kHz, then -3dB/octave rolloff
		if (FreqHz <= 2000.0f)
		{
			return 0.0f;
		}
		else
		{
			// Calculate octaves above 2kHz
			float Octaves = FMath::Log2(FreqHz / 2000.0f);
			return -3.0f * Octaves;
		}

	case ECalibrationTargetCurve::Custom:
		if (CustomCurvePoints.Num() == 0)
		{
			return 0.0f;
		}
		if (CustomCurvePoints.Num() == 1)
		{
			return CustomCurvePoints[0].Y;
		}

		// Interpolate between custom points
		for (int32 i = 0; i < CustomCurvePoints.Num() - 1; ++i)
		{
			if (FreqHz >= CustomCurvePoints[i].X && FreqHz <= CustomCurvePoints[i + 1].X)
			{
				float LowFreq = CustomCurvePoints[i].X;
				float HighFreq = CustomCurvePoints[i + 1].X;
				float LowMag = CustomCurvePoints[i].Y;
				float HighMag = CustomCurvePoints[i + 1].Y;

				float LogRatio = FMath::Loge(FreqHz / LowFreq) / FMath::Loge(HighFreq / LowFreq);
				return FMath::Lerp(LowMag, HighMag, LogRatio);
			}
		}

		// Outside range - return nearest
		if (FreqHz < CustomCurvePoints[0].X)
		{
			return CustomCurvePoints[0].Y;
		}
		return CustomCurvePoints.Last().Y;

	default:
		return 0.0f;
	}
}

// ============================================================================
// FSpeakerCalibrationPreset
// ============================================================================

FSpatialSpeakerDSPState FSpeakerCalibrationPreset::GenerateDSPState() const
{
	FSpatialSpeakerDSPState State;

	// Apply delay
	if (bApplyDelay)
	{
		State.DelayMs = SuggestedDelayMs;
	}

	// Apply gain
	if (bApplyGain)
	{
		State.InputGainDb = SuggestedGainDb;
	}

	// Apply EQ
	if (bApplyEQ)
	{
		State.EQBands = GeneratedEQBands;
	}

	// Apply filters
	if (bApplyFilters)
	{
		State.HighPass = SuggestedHighPass;
		State.LowPass = SuggestedLowPass;
	}

	return State;
}

void FSpeakerCalibrationPreset::RecalculateCorrections()
{
	// Clear existing corrections
	GeneratedEQBands.Empty();
	SuggestedDelayMs = 0.0f;
	SuggestedGainDb = 0.0f;
	SuggestedHighPass = FSpatialHighPassFilter();
	SuggestedLowPass = FSpatialLowPassFilter();

	if (Measurement.FrequencyBins.Num() == 0)
	{
		return;
	}

	// Use detected delay from measurement
	SuggestedDelayMs = Measurement.DetectedDelayMs;

	// Calculate suggested gain (normalize to reference level)
	float AvgMag = Measurement.GetAverageMagnitudeInBand(500.0f, 2000.0f);
	SuggestedGainDb = -AvgMag;  // Invert to normalize

	// Auto-EQ generation is handled by the calibration manager
	// This method just does basic prep

	Modified = FDateTime::UtcNow();
}

// ============================================================================
// FVenueCalibrationSet
// ============================================================================

FSpeakerCalibrationPreset* FVenueCalibrationSet::GetSpeakerPreset(const FGuid& SpeakerId)
{
	return SpeakerPresets.Find(SpeakerId);
}

const FSpeakerCalibrationPreset* FVenueCalibrationSet::GetSpeakerPreset(const FGuid& SpeakerId) const
{
	return SpeakerPresets.Find(SpeakerId);
}

void FVenueCalibrationSet::SetSpeakerPreset(const FGuid& SpeakerId, const FSpeakerCalibrationPreset& Preset)
{
	SpeakerPresets.Add(SpeakerId, Preset);
	Modified = FDateTime::UtcNow();
}

void FVenueCalibrationSet::RemoveSpeakerPreset(const FGuid& SpeakerId)
{
	SpeakerPresets.Remove(SpeakerId);
	Modified = FDateTime::UtcNow();
}

void FVenueCalibrationSet::NormalizeDelays()
{
	if (!ReferenceDelaySpeakerId.IsValid())
	{
		return;
	}

	const FSpeakerCalibrationPreset* RefPreset = GetSpeakerPreset(ReferenceDelaySpeakerId);
	if (!RefPreset)
	{
		return;
	}

	float ReferenceDelay = RefPreset->Measurement.DetectedDelayMs;

	for (auto& Pair : SpeakerPresets)
	{
		// Calculate relative delay (positive = arrives later than reference)
		float AbsoluteDelay = Pair.Value.Measurement.DetectedDelayMs;
		float RelativeDelay = AbsoluteDelay - ReferenceDelay;

		// We want all speakers to arrive at same time as reference
		// If speaker arrives early (negative relative), add delay
		// If speaker arrives late (positive relative), it becomes the new reference
		if (RelativeDelay < 0)
		{
			Pair.Value.SuggestedDelayMs = FMath::Abs(RelativeDelay);
		}
		else
		{
			Pair.Value.SuggestedDelayMs = 0.0f;
		}
	}

	Modified = FDateTime::UtcNow();
}

void FVenueCalibrationSet::NormalizeGains()
{
	// Calculate average magnitude at 1kHz across all speakers
	float TotalMag = 0.0f;
	int32 Count = 0;

	for (const auto& Pair : SpeakerPresets)
	{
		float Mag = Pair.Value.Measurement.GetMagnitudeAtFrequency(1000.0f);
		TotalMag += Mag;
		++Count;
	}

	if (Count == 0)
	{
		return;
	}

	float AverageMag = TotalMag / Count;

	// Calculate target based on reference SPL
	// ReferenceLevelSPL is the desired SPL at 1kHz
	// We normalize all speakers to have same 1kHz level

	for (auto& Pair : SpeakerPresets)
	{
		float SpeakerMag = Pair.Value.Measurement.GetMagnitudeAtFrequency(1000.0f);
		// Trim to bring this speaker to average level
		Pair.Value.SuggestedGainDb = AverageMag - SpeakerMag;
	}

	Modified = FDateTime::UtcNow();
}
