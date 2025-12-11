// Copyright Rocketship. All Rights Reserved.

#include "Rendering/SpatialRendererDBAP.h"

FSpatialRendererDBAP::FSpatialRendererDBAP()
	: bIsConfigured(false)
	, RolloffExponent(2.0f)
	, ReferenceDistance(100.0f)  // 1 meter in cm
	, ReferencePoint(FVector::ZeroVector)
	, bPhaseCoherent(true)
	, MinGainThreshold(0.001f)  // -60dB
	, MaxActiveSpeakers(0)  // Use all
	, SpatialBlur(0.0f)
{
}

void FSpatialRendererDBAP::Configure(const TArray<FSpatialSpeaker>& Speakers)
{
	bIsConfigured = false;
	CachedSpeakers = Speakers;
	SpeakerPositions.Reset();

	if (Speakers.Num() < 2)
	{
		return;
	}

	// Cache speaker positions for fast access
	SpeakerPositions.SetNum(Speakers.Num());
	for (int32 i = 0; i < Speakers.Num(); ++i)
	{
		SpeakerPositions[i] = Speakers[i].Position;
	}

	bIsConfigured = true;
}

bool FSpatialRendererDBAP::IsConfigured() const
{
	return bIsConfigured;
}

int32 FSpatialRendererDBAP::GetSpeakerCount() const
{
	return CachedSpeakers.Num();
}

void FSpatialRendererDBAP::ComputeGains(
	const FVector& ObjectPosition,
	float Spread,
	TArray<FSpatialSpeakerGain>& OutGains) const
{
	OutGains.Reset();

	if (!bIsConfigured || CachedSpeakers.Num() == 0)
	{
		return;
	}

	// Compute raw gains based on distance
	TArray<float> Gains;
	TArray<float> Distances;
	ComputeRawGains(ObjectPosition, Gains, Distances);

	// Apply spread (increases contribution of distant speakers)
	if (Spread > KINDA_SMALL_NUMBER)
	{
		ApplySpread(Gains, Spread);
	}

	// Apply spatial blur
	if (SpatialBlur > KINDA_SMALL_NUMBER)
	{
		ApplyBlur(Gains);
	}

	// Normalize gains
	NormalizeGains(Gains);

	// If max active speakers is set, zero out speakers beyond the limit
	if (MaxActiveSpeakers > 0 && MaxActiveSpeakers < CachedSpeakers.Num())
	{
		// Find N largest gains and zero the rest
		TArray<TPair<float, int32>> GainIndexPairs;
		for (int32 i = 0; i < Gains.Num(); ++i)
		{
			GainIndexPairs.Add(TPair<float, int32>(Gains[i], i));
		}

		// Sort by gain descending
		GainIndexPairs.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) {
			return A.Key > B.Key;
		});

		// Zero gains beyond max
		for (int32 i = MaxActiveSpeakers; i < GainIndexPairs.Num(); ++i)
		{
			Gains[GainIndexPairs[i].Value] = 0.0f;
		}

		// Re-normalize after limiting
		NormalizeGains(Gains);
	}

	// Build output array
	OutGains.Reserve(Gains.Num());
	for (int32 i = 0; i < Gains.Num(); ++i)
	{
		if (Gains[i] > MinGainThreshold)
		{
			FSpatialSpeakerGain SpeakerGain;
			SpeakerGain.SpeakerId = CachedSpeakers[i].Id;
			SpeakerGain.SpeakerIndex = i;
			SpeakerGain.Gain = Gains[i];

			if (bPhaseCoherent)
			{
				SpeakerGain.DelayMs = ComputeSpeakerDelay(i, ObjectPosition);
			}
			else
			{
				SpeakerGain.DelayMs = 0.0f;
			}

			OutGains.Add(SpeakerGain);
		}
	}
}

void FSpatialRendererDBAP::ComputeGainsBatch(
	const TArray<FVector>& ObjectPositions,
	const TArray<float>& Spreads,
	TArray<TArray<FSpatialSpeakerGain>>& OutGainsPerObject) const
{
	OutGainsPerObject.SetNum(ObjectPositions.Num());
	for (int32 i = 0; i < ObjectPositions.Num(); ++i)
	{
		ComputeGains(ObjectPositions[i], Spreads[i], OutGainsPerObject[i]);
	}
}

FString FSpatialRendererDBAP::GetDescription() const
{
	return FString::Printf(
		TEXT("Distance-Based Amplitude Panning (rolloff=%.1f, %s)"),
		RolloffExponent,
		bPhaseCoherent ? TEXT("phase-coherent") : TEXT("amplitude-only"));
}

FString FSpatialRendererDBAP::GetDiagnosticInfo() const
{
	FString Info;
	Info += FString::Printf(TEXT("DBAP Renderer\n"));
	Info += FString::Printf(TEXT("  Configured: %s\n"), bIsConfigured ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Speakers: %d\n"), CachedSpeakers.Num());
	Info += FString::Printf(TEXT("  Rolloff Exponent: %.2f\n"), RolloffExponent);
	Info += FString::Printf(TEXT("  Reference Distance: %.1f cm\n"), ReferenceDistance);
	Info += FString::Printf(TEXT("  Phase Coherent: %s\n"), bPhaseCoherent ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Reference Point: (%.1f, %.1f, %.1f)\n"),
		ReferencePoint.X, ReferencePoint.Y, ReferencePoint.Z);
	Info += FString::Printf(TEXT("  Min Gain Threshold: %.4f (%.1f dB)\n"),
		MinGainThreshold, 20.0f * FMath::LogX(10.0f, MinGainThreshold));
	Info += FString::Printf(TEXT("  Max Active Speakers: %d\n"), MaxActiveSpeakers);
	Info += FString::Printf(TEXT("  Spatial Blur: %.2f\n"), SpatialBlur);
	return Info;
}

TArray<FString> FSpatialRendererDBAP::Validate() const
{
	TArray<FString> Errors;

	if (CachedSpeakers.Num() < 2)
	{
		Errors.Add(TEXT("DBAP requires at least 2 speakers"));
	}

	// Check for coincident speakers
	for (int32 i = 0; i < CachedSpeakers.Num(); ++i)
	{
		for (int32 j = i + 1; j < CachedSpeakers.Num(); ++j)
		{
			float Dist = FVector::Dist(CachedSpeakers[i].Position, CachedSpeakers[j].Position);
			if (Dist < 1.0f)
			{
				Errors.Add(FString::Printf(
					TEXT("Speakers '%s' and '%s' are nearly coincident (%.2f cm apart)"),
					*CachedSpeakers[i].Name, *CachedSpeakers[j].Name, Dist));
			}
		}
	}

	// Warn about extreme rolloff values
	if (RolloffExponent < 0.5f)
	{
		Errors.Add(TEXT("Very low rolloff exponent may cause excessive diffusion"));
	}
	else if (RolloffExponent > 4.0f)
	{
		Errors.Add(TEXT("Very high rolloff exponent may cause unnatural focus"));
	}

	return Errors;
}

void FSpatialRendererDBAP::ComputeRawGains(
	const FVector& SourcePosition,
	TArray<float>& OutGains,
	TArray<float>& OutDistances) const
{
	OutGains.SetNum(CachedSpeakers.Num());
	OutDistances.SetNum(CachedSpeakers.Num());

	for (int32 i = 0; i < CachedSpeakers.Num(); ++i)
	{
		// Distance from source to speaker
		float Distance = FVector::Dist(SourcePosition, SpeakerPositions[i]);
		OutDistances[i] = Distance;

		// Clamp distance to reference distance minimum
		Distance = FMath::Max(Distance, ReferenceDistance);

		// Inverse distance weighting: 1 / d^a
		// Normalized by reference distance for consistent scaling
		float NormalizedDist = Distance / ReferenceDistance;
		OutGains[i] = 1.0f / FMath::Pow(NormalizedDist, RolloffExponent);
	}
}

void FSpatialRendererDBAP::ApplySpread(TArray<float>& Gains, float Spread) const
{
	// Spread increases the contribution of distant speakers
	// by reducing the dynamic range of gains
	//
	// At spread=0: original gains
	// At spread=180: all speakers equal

	float SpreadFactor = FMath::Clamp(Spread / 180.0f, 0.0f, 1.0f);

	if (SpreadFactor < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Find min and max gains
	float MinGain = FLT_MAX;
	float MaxGain = 0.0f;
	for (float Gain : Gains)
	{
		MinGain = FMath::Min(MinGain, Gain);
		MaxGain = FMath::Max(MaxGain, Gain);
	}

	if (MaxGain - MinGain < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Compress dynamic range based on spread
	// At full spread, all gains become equal
	float TargetGain = (MinGain + MaxGain) * 0.5f;

	for (float& Gain : Gains)
	{
		Gain = FMath::Lerp(Gain, TargetGain, SpreadFactor);
	}
}

void FSpatialRendererDBAP::ApplyBlur(TArray<float>& Gains) const
{
	if (SpatialBlur < KINDA_SMALL_NUMBER || Gains.Num() < 2)
	{
		return;
	}

	// Simple blur: blend each gain with its neighbors based on speaker proximity
	// This creates smoother transitions

	TArray<float> BlurredGains;
	BlurredGains.SetNum(Gains.Num());

	for (int32 i = 0; i < Gains.Num(); ++i)
	{
		float WeightedSum = Gains[i];
		float TotalWeight = 1.0f;

		for (int32 j = 0; j < Gains.Num(); ++j)
		{
			if (i == j) continue;

			float Dist = FVector::Dist(SpeakerPositions[i], SpeakerPositions[j]);
			float Weight = SpatialBlur / (1.0f + Dist / ReferenceDistance);

			WeightedSum += Gains[j] * Weight;
			TotalWeight += Weight;
		}

		BlurredGains[i] = WeightedSum / TotalWeight;
	}

	Gains = MoveTemp(BlurredGains);
}

float FSpatialRendererDBAP::ComputeSpeakerDelay(int32 SpeakerIndex, const FVector& SourcePosition) const
{
	if (SpeakerIndex < 0 || SpeakerIndex >= CachedSpeakers.Num())
	{
		return 0.0f;
	}

	// Same delay computation as VBAP for phase coherence
	float SourceToSpeaker = FVector::Dist(SourcePosition, SpeakerPositions[SpeakerIndex]);
	float SourceToRef = FVector::Dist(SourcePosition, ReferencePoint);

	// Convert from Unreal units (cm) to meters
	float SourceToSpeakerM = SourceToSpeaker / 100.0f;
	float SourceToRefM = SourceToRef / 100.0f;

	// Delay in milliseconds
	float DelayMs = (SourceToSpeakerM - SourceToRefM) * SpatialAudioConstants::MsPerMeter;

	// Clamp to non-negative
	return FMath::Max(0.0f, DelayMs);
}

void FSpatialRendererDBAP::NormalizeGains(TArray<float>& Gains) const
{
	if (Gains.Num() == 0)
	{
		return;
	}

	// Constant-power normalization
	float SumSquares = 0.0f;
	for (float Gain : Gains)
	{
		SumSquares += Gain * Gain;
	}

	if (SumSquares > KINDA_SMALL_NUMBER)
	{
		float Scale = 1.0f / FMath::Sqrt(SumSquares);
		for (float& Gain : Gains)
		{
			Gain *= Scale;
		}
	}
}
