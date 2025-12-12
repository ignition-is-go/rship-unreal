// Copyright Rocketship. All Rights Reserved.

#include "Rendering/SpatialRendererVBAP.h"

FSpatialRendererVBAP::FSpatialRendererVBAP()
	: bIsConfigured(false)
	, bUse2DMode(false)
	, bPhaseCoherent(true)
	, ReferencePoint(FVector::ZeroVector)
	, SpeakerCentroid(FVector::ZeroVector)
	, MinGainThreshold(0.001f)  // -60dB
	, SpreadFactor(1.0f)
{
}

void FSpatialRendererVBAP::Configure(const TArray<FSpatialSpeaker>& Speakers)
{
	bIsConfigured = false;
	CachedSpeakers = Speakers;
	SpeakerDirections.Reset();
	SpeakerDistances.Reset();

	if (Speakers.Num() < 3)
	{
		// Need at least 3 speakers for triangulation
		return;
	}

	// Compute centroid
	SpeakerCentroid = FVector::ZeroVector;
	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		SpeakerCentroid += Speaker.WorldPosition;
	}
	SpeakerCentroid /= Speakers.Num();

	// Convert speaker positions to directions and distances
	SpeakerDirections.SetNum(Speakers.Num());
	SpeakerDistances.SetNum(Speakers.Num());

	TArray<FVector2D> Positions2D;
	TArray<FVector> Positions3D;

	if (bUse2DMode)
	{
		Positions2D.SetNum(Speakers.Num());
	}
	else
	{
		Positions3D.SetNum(Speakers.Num());
	}

	for (int32 i = 0; i < Speakers.Num(); ++i)
	{
		FVector RelativePos = Speakers[i].Position - ReferencePoint;
		float Distance = RelativePos.Size();

		SpeakerDistances[i] = Distance;

		if (Distance > KINDA_SMALL_NUMBER)
		{
			SpeakerDirections[i] = RelativePos / Distance;
		}
		else
		{
			// Speaker at reference point - use forward direction
			SpeakerDirections[i] = FVector::ForwardVector;
		}

		if (bUse2DMode)
		{
			// For 2D, we use azimuth angles projected to XY plane
			// Store as 2D position on unit circle
			FVector2D Dir2D(SpeakerDirections[i].X, SpeakerDirections[i].Y);
			Dir2D.Normalize();
			Positions2D[i] = Dir2D;
		}
		else
		{
			// For 3D, use full unit sphere direction
			Positions3D[i] = SpeakerDirections[i];
		}
	}

	// Build triangulation
	if (bUse2DMode)
	{
		Triangulation2D.Triangulate(Positions2D);
	}
	else
	{
		Triangulation3D.Triangulate(Positions3D);
	}

	bIsConfigured = true;
}

bool FSpatialRendererVBAP::IsConfigured() const
{
	return bIsConfigured;
}

int32 FSpatialRendererVBAP::GetSpeakerCount() const
{
	return CachedSpeakers.Num();
}

void FSpatialRendererVBAP::ComputeGains(
	const FVector& ObjectPosition,
	float Spread,
	TArray<FSpatialSpeakerGain>& OutGains) const
{
	OutGains.Reset();

	if (!bIsConfigured || CachedSpeakers.Num() == 0)
	{
		return;
	}

	// Convert object position to direction and distance from reference
	FVector RelativePos = ObjectPosition - ReferencePoint;
	float Distance = RelativePos.Size();
	FVector Direction;

	if (Distance > KINDA_SMALL_NUMBER)
	{
		Direction = RelativePos / Distance;
	}
	else
	{
		// Object at reference point - use forward direction
		Direction = FVector::ForwardVector;
		Distance = 1.0f;
	}

	// Compute gains based on spread
	if (Spread <= KINDA_SMALL_NUMBER)
	{
		// Point source - use standard VBAP
		if (bUse2DMode)
		{
			ComputePointGains2D(Direction, Distance, OutGains);
		}
		else
		{
			ComputePointGains3D(Direction, Distance, OutGains);
		}
	}
	else
	{
		// Spread source - distribute energy
		ComputeSpreadGains(Direction, Distance, Spread, OutGains);
	}

	// Apply threshold and normalize
	ApplyThreshold(OutGains);
	NormalizeGains(OutGains);
}

void FSpatialRendererVBAP::ComputeGainsBatch(
	const TArray<FVector>& ObjectPositions,
	const TArray<float>& Spreads,
	TArray<TArray<FSpatialSpeakerGain>>& OutGainsPerObject) const
{
	// Default implementation - can be optimized with SIMD later
	OutGainsPerObject.SetNum(ObjectPositions.Num());
	for (int32 i = 0; i < ObjectPositions.Num(); ++i)
	{
		ComputeGains(ObjectPositions[i], Spreads[i], OutGainsPerObject[i]);
	}
}

FString FSpatialRendererVBAP::GetDescription() const
{
	return FString::Printf(
		TEXT("Vector Base Amplitude Panning (%s mode, %s)"),
		bUse2DMode ? TEXT("2D") : TEXT("3D"),
		bPhaseCoherent ? TEXT("phase-coherent") : TEXT("amplitude-only"));
}

FString FSpatialRendererVBAP::GetDiagnosticInfo() const
{
	FString Info;
	Info += FString::Printf(TEXT("VBAP Renderer\n"));
	Info += FString::Printf(TEXT("  Configured: %s\n"), bIsConfigured ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Mode: %s\n"), bUse2DMode ? TEXT("2D") : TEXT("3D"));
	Info += FString::Printf(TEXT("  Phase Coherent: %s\n"), bPhaseCoherent ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Speakers: %d\n"), CachedSpeakers.Num());
	Info += FString::Printf(TEXT("  Mesh Elements: %d\n"), GetMeshElementCount());
	Info += FString::Printf(TEXT("  Reference Point: (%.1f, %.1f, %.1f)\n"),
		ReferencePoint.X, ReferencePoint.Y, ReferencePoint.Z);
	Info += FString::Printf(TEXT("  Speaker Centroid: (%.1f, %.1f, %.1f)\n"),
		SpeakerCentroid.X, SpeakerCentroid.Y, SpeakerCentroid.Z);
	Info += FString::Printf(TEXT("  Min Gain Threshold: %.4f (%.1f dB)\n"),
		MinGainThreshold, 20.0f * FMath::LogX(10.0f, MinGainThreshold));
	return Info;
}

TArray<FString> FSpatialRendererVBAP::Validate() const
{
	TArray<FString> Errors;

	if (CachedSpeakers.Num() < 3)
	{
		Errors.Add(TEXT("VBAP requires at least 3 speakers"));
	}

	if (bUse2DMode)
	{
		if (Triangulation2D.Triangles.Num() == 0 && bIsConfigured)
		{
			Errors.Add(TEXT("2D triangulation produced no triangles - speakers may be collinear"));
		}
	}
	else
	{
		if (Triangulation3D.Tetrahedra.Num() == 0 && bIsConfigured)
		{
			Errors.Add(TEXT("3D triangulation produced no tetrahedra - speakers may be coplanar"));
		}
	}

	// Check for coincident speakers
	for (int32 i = 0; i < CachedSpeakers.Num(); ++i)
	{
		for (int32 j = i + 1; j < CachedSpeakers.Num(); ++j)
		{
			float Dist = FVector::Dist(CachedSpeakers[i].Position, CachedSpeakers[j].Position);
			if (Dist < 1.0f) // Less than 1cm
			{
				Errors.Add(FString::Printf(
					TEXT("Speakers '%s' and '%s' are nearly coincident (%.2f cm apart)"),
					*CachedSpeakers[i].Name, *CachedSpeakers[j].Name, Dist));
			}
		}
	}

	// Check for speakers at reference point
	for (const FSpatialSpeaker& Speaker : CachedSpeakers)
	{
		float Dist = FVector::Dist(Speaker.WorldPosition, ReferencePoint);
		if (Dist < 10.0f) // Less than 10cm
		{
			Errors.Add(FString::Printf(
				TEXT("Speaker '%s' is very close to reference point (%.2f cm) - may cause instability"),
				*Speaker.Name, Dist));
		}
	}

	return Errors;
}

int32 FSpatialRendererVBAP::GetMeshElementCount() const
{
	if (bUse2DMode)
	{
		return Triangulation2D.Triangles.Num();
	}
	else
	{
		return Triangulation3D.Tetrahedra.Num();
	}
}

void FSpatialRendererVBAP::ComputePointGains2D(
	const FVector& Direction,
	float Distance,
	TArray<FSpatialSpeakerGain>& OutGains) const
{
	// Project direction to 2D (XY plane)
	FVector2D Dir2D(Direction.X, Direction.Y);
	Dir2D.Normalize();

	// Find containing triangle
	FVector Bary;
	int32 TriIndex = Triangulation2D.FindContainingTriangle(Dir2D, Bary);

	if (TriIndex >= 0)
	{
		const FSpatialTriangle2D& Tri = Triangulation2D.Triangles[TriIndex];

		// Barycentric coordinates are the VBAP gains
		const int32 Indices[3] = { Tri.Indices[0], Tri.Indices[1], Tri.Indices[2] };
		const float Coords[3] = { Bary.X, Bary.Y, Bary.Z };

		for (int32 i = 0; i < 3; ++i)
		{
			if (Coords[i] > MinGainThreshold && Indices[i] >= 0 && Indices[i] < CachedSpeakers.Num())
			{
				FSpatialSpeakerGain Gain;
				Gain.SpeakerId = CachedSpeakers[Indices[i]].Id;
				Gain.SpeakerIndex = Indices[i];
				Gain.Gain = Coords[i];

				if (bPhaseCoherent)
				{
					// Compute delay for phase coherence
					FVector SourcePos = ReferencePoint + Direction * Distance;
					Gain.DelayMs = ComputeSpeakerDelay(Indices[i], SourcePos);
				}
				else
				{
					Gain.DelayMs = 0.0f;
				}

				OutGains.Add(Gain);
			}
		}
	}
	else
	{
		// Source outside speaker array - find nearest speaker pair
		// Use fallback: two nearest speakers with linear interpolation
		float BestDot1 = -2.0f;
		float BestDot2 = -2.0f;
		int32 BestIdx1 = -1;
		int32 BestIdx2 = -1;

		for (int32 i = 0; i < SpeakerDirections.Num(); ++i)
		{
			FVector2D SpkDir2D(SpeakerDirections[i].X, SpeakerDirections[i].Y);
			SpkDir2D.Normalize();
			float Dot = FVector2D::DotProduct(Dir2D, SpkDir2D);

			if (Dot > BestDot1)
			{
				BestDot2 = BestDot1;
				BestIdx2 = BestIdx1;
				BestDot1 = Dot;
				BestIdx1 = i;
			}
			else if (Dot > BestDot2)
			{
				BestDot2 = Dot;
				BestIdx2 = i;
			}
		}

		if (BestIdx1 >= 0)
		{
			// Simple fallback - primary speaker with full gain
			// More sophisticated: interpolate between two nearest
			FSpatialSpeakerGain Gain;
			Gain.SpeakerId = CachedSpeakers[BestIdx1].Id;
			Gain.SpeakerIndex = BestIdx1;
			Gain.Gain = 1.0f;

			if (bPhaseCoherent)
			{
				FVector SourcePos = ReferencePoint + Direction * Distance;
				Gain.DelayMs = ComputeSpeakerDelay(BestIdx1, SourcePos);
			}
			else
			{
				Gain.DelayMs = 0.0f;
			}

			OutGains.Add(Gain);
		}
	}
}

void FSpatialRendererVBAP::ComputePointGains3D(
	const FVector& Direction,
	float Distance,
	TArray<FSpatialSpeakerGain>& OutGains) const
{
	// Find containing tetrahedron
	FVector4 Bary;
	int32 TetIndex = Triangulation3D.FindContainingTetrahedron(Direction, Bary);

	if (TetIndex >= 0)
	{
		const FSpatialTetrahedron& Tet = Triangulation3D.Tetrahedra[TetIndex];

		// Barycentric coordinates are the VBAP gains
		const int32 Indices[4] = { Tet.Indices[0], Tet.Indices[1], Tet.Indices[2], Tet.Indices[3] };
		const float Coords[4] = { Bary.X, Bary.Y, Bary.Z, Bary.W };

		for (int32 i = 0; i < 4; ++i)
		{
			if (Coords[i] > MinGainThreshold && Indices[i] >= 0 && Indices[i] < CachedSpeakers.Num())
			{
				FSpatialSpeakerGain Gain;
				Gain.SpeakerId = CachedSpeakers[Indices[i]].Id;
				Gain.SpeakerIndex = Indices[i];
				Gain.Gain = Coords[i];

				if (bPhaseCoherent)
				{
					FVector SourcePos = ReferencePoint + Direction * Distance;
					Gain.DelayMs = ComputeSpeakerDelay(Indices[i], SourcePos);
				}
				else
				{
					Gain.DelayMs = 0.0f;
				}

				OutGains.Add(Gain);
			}
		}
	}
	else
	{
		// Source outside speaker hull - find nearest speakers
		float BestDot = -2.0f;
		int32 BestIdx = -1;

		for (int32 i = 0; i < SpeakerDirections.Num(); ++i)
		{
			float Dot = FVector::DotProduct(Direction, SpeakerDirections[i]);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				BestIdx = i;
			}
		}

		if (BestIdx >= 0)
		{
			FSpatialSpeakerGain Gain;
			Gain.SpeakerId = CachedSpeakers[BestIdx].Id;
			Gain.SpeakerIndex = BestIdx;
			Gain.Gain = 1.0f;

			if (bPhaseCoherent)
			{
				FVector SourcePos = ReferencePoint + Direction * Distance;
				Gain.DelayMs = ComputeSpeakerDelay(BestIdx, SourcePos);
			}
			else
			{
				Gain.DelayMs = 0.0f;
			}

			OutGains.Add(Gain);
		}
	}
}

void FSpatialRendererVBAP::ComputeSpreadGains(
	const FVector& Direction,
	float Distance,
	float Spread,
	TArray<FSpatialSpeakerGain>& OutGains) const
{
	// Spread angle in radians (input is degrees)
	float SpreadRad = FMath::DegreesToRadians(Spread * SpreadFactor);
	float CosSpread = FMath::Cos(SpreadRad);

	// For spread sources, we compute gains for all speakers within the spread cone
	// and blend them based on their angle from the source direction

	TMap<int32, float> GainMap;
	float TotalGain = 0.0f;

	for (int32 i = 0; i < SpeakerDirections.Num(); ++i)
	{
		float Dot = FVector::DotProduct(Direction, SpeakerDirections[i]);

		if (Dot >= CosSpread)
		{
			// Speaker is within spread cone
			// Gain falls off from center of cone
			float Angle = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f));
			float NormalizedAngle = Angle / SpreadRad;

			// Cosine rolloff within spread
			float Gain = FMath::Cos(NormalizedAngle * PI * 0.5f);
			Gain = FMath::Max(0.0f, Gain);

			GainMap.Add(i, Gain);
			TotalGain += Gain;
		}
	}

	// If no speakers in cone, fall back to point source
	if (GainMap.Num() == 0)
	{
		if (bUse2DMode)
		{
			ComputePointGains2D(Direction, Distance, OutGains);
		}
		else
		{
			ComputePointGains3D(Direction, Distance, OutGains);
		}
		return;
	}

	// Convert to output format
	FVector SourcePos = ReferencePoint + Direction * Distance;

	for (auto& Pair : GainMap)
	{
		int32 SpeakerIdx = Pair.Key;
		float Gain = Pair.Value;

		if (Gain > MinGainThreshold)
		{
			FSpatialSpeakerGain SpeakerGain;
			SpeakerGain.SpeakerId = CachedSpeakers[SpeakerIdx].Id;
			SpeakerGain.SpeakerIndex = SpeakerIdx;
			SpeakerGain.Gain = Gain;

			if (bPhaseCoherent)
			{
				SpeakerGain.DelayMs = ComputeSpeakerDelay(SpeakerIdx, SourcePos);
			}
			else
			{
				SpeakerGain.DelayMs = 0.0f;
			}

			OutGains.Add(SpeakerGain);
		}
	}
}

float FSpatialRendererVBAP::ComputeSpeakerDelay(
	int32 SpeakerIndex,
	const FVector& SourcePosition) const
{
	if (SpeakerIndex < 0 || SpeakerIndex >= CachedSpeakers.Num())
	{
		return 0.0f;
	}

	const FSpatialSpeaker& Speaker = CachedSpeakers[SpeakerIndex];

	// Distance from source to speaker
	float SourceToSpeaker = FVector::Dist(SourcePosition, Speaker.WorldPosition);

	// Distance from source to reference point
	float SourceToRef = FVector::Dist(SourcePosition, ReferencePoint);

	// Distance from speaker to reference point
	float SpeakerToRef = SpeakerDistances[SpeakerIndex];

	// Compute delay relative to a virtual point source at the source position
	// We want all speakers to receive signal as if from a coherent wavefront
	//
	// Time for sound to travel from source to speaker:
	//   t_speaker = SourceToSpeaker / SpeedOfSound
	//
	// Time for sound to travel from source to reference:
	//   t_ref = SourceToRef / SpeedOfSound
	//
	// To align at reference point, each speaker needs delay:
	//   delay = (SourceToSpeaker - SourceToRef) / SpeedOfSound
	//
	// In milliseconds (using MsPerMeter constant from SpatialAudioTypes.h):
	//   delay_ms = (SourceToSpeaker - SourceToRef) * MsPerMeter / 100 (convert from cm to m)

	// Convert from Unreal units (cm) to meters
	float SourceToSpeakerM = SourceToSpeaker / 100.0f;
	float SourceToRefM = SourceToRef / 100.0f;

	// Delay in milliseconds
	// Positive delay means speaker is farther, needs to play later
	// Negative delay means speaker is closer, would need to play earlier (we clamp to 0)
	float DelayMs = (SourceToSpeakerM - SourceToRefM) * SpatialAudioConstants::MsPerMeter;

	// Clamp to non-negative (we can only delay, not advance)
	return FMath::Max(0.0f, DelayMs);
}

void FSpatialRendererVBAP::NormalizeGains(TArray<FSpatialSpeakerGain>& Gains) const
{
	if (Gains.Num() == 0)
	{
		return;
	}

	// Constant-power normalization (preserve total power)
	// Sum of squares should equal 1.0
	float SumSquares = 0.0f;
	for (const FSpatialSpeakerGain& G : Gains)
	{
		SumSquares += G.Gain * G.Gain;
	}

	if (SumSquares > KINDA_SMALL_NUMBER)
	{
		float Scale = 1.0f / FMath::Sqrt(SumSquares);
		for (FSpatialSpeakerGain& G : Gains)
		{
			G.Gain *= Scale;
		}
	}
}

void FSpatialRendererVBAP::ApplyThreshold(TArray<FSpatialSpeakerGain>& Gains) const
{
	// Remove gains below threshold
	for (int32 i = Gains.Num() - 1; i >= 0; --i)
	{
		if (Gains[i].Gain < MinGainThreshold)
		{
			Gains.RemoveAt(i);
		}
	}
}
