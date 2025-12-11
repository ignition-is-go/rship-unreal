// Copyright Rocketship. All Rights Reserved.

#include "Rendering/SpatialRendererHOA.h"

// Speed of sound in cm/s (for delay calculations)
static constexpr float SPEED_OF_SOUND_CM = 34300.0f;

// ============================================================================
// FAmbisonicsEncoder
// ============================================================================

FAmbisonicsEncoder::FAmbisonicsEncoder()
	: Order(EAmbisonicsOrder::First)
	, Normalization(EAmbisonicsNormalization::SN3D)
{
	ComputeNormalizationFactors();
}

void FAmbisonicsEncoder::SetOrder(EAmbisonicsOrder InOrder)
{
	Order = InOrder;
	ComputeNormalizationFactors();
}

void FAmbisonicsEncoder::SetNormalization(EAmbisonicsNormalization InNorm)
{
	Normalization = InNorm;
	ComputeNormalizationFactors();
}

void FAmbisonicsEncoder::ComputeNormalizationFactors()
{
	int32 NumChannels = GetChannelCount();
	NormalizationFactors.SetNum(NumChannels);

	for (int32 l = 0; l <= static_cast<int32>(Order); ++l)
	{
		for (int32 m = -l; m <= l; ++m)
		{
			int32 ACN = GetACN(l, m);
			float Factor = 1.0f;

			switch (Normalization)
			{
			case EAmbisonicsNormalization::SN3D:
				// Schmidt semi-normalized (AmbiX standard)
				if (m == 0)
				{
					Factor = 1.0f;
				}
				else
				{
					Factor = FMath::Sqrt(2.0f * Factorial(l - FMath::Abs(m)) / Factorial(l + FMath::Abs(m)));
				}
				break;

			case EAmbisonicsNormalization::N3D:
				// Full 3D normalization
				Factor = FMath::Sqrt((2.0f * l + 1.0f) * Factorial(l - FMath::Abs(m)) / (4.0f * PI * Factorial(l + FMath::Abs(m))));
				break;

			case EAmbisonicsNormalization::FuMa:
				// Legacy B-format (only valid for 1st order)
				if (ACN == 0) Factor = 1.0f / FMath::Sqrt(2.0f);  // W
				else Factor = 1.0f;  // X, Y, Z
				break;

			case EAmbisonicsNormalization::MaxN:
				// Max-normalized (peak = 1)
				Factor = 1.0f;  // Simplified
				break;
			}

			NormalizationFactors[ACN] = Factor;
		}
	}
}

float FAmbisonicsEncoder::Factorial(int32 n)
{
	if (n <= 1) return 1.0f;
	float Result = 1.0f;
	for (int32 i = 2; i <= n; ++i)
	{
		Result *= i;
	}
	return Result;
}

float FAmbisonicsEncoder::AssociatedLegendre(int32 l, int32 m, float x)
{
	// Compute P_l^m(x) using recurrence relations
	// Handles negative m by symmetry

	int32 AbsM = FMath::Abs(m);

	if (AbsM > l) return 0.0f;

	// P_m^m(x) = (-1)^m * (2m-1)!! * (1-x^2)^(m/2)
	float Pmm = 1.0f;
	if (AbsM > 0)
	{
		float SqrtOneMinusX2 = FMath::Sqrt(FMath::Max(0.0f, 1.0f - x * x));
		float Fact = 1.0f;
		for (int32 i = 1; i <= AbsM; ++i)
		{
			Pmm *= -Fact * SqrtOneMinusX2;
			Fact += 2.0f;
		}
	}

	if (l == AbsM) return Pmm;

	// P_{m+1}^m(x) = x * (2m+1) * P_m^m(x)
	float Pmm1 = x * (2.0f * AbsM + 1.0f) * Pmm;

	if (l == AbsM + 1) return Pmm1;

	// Recurrence: (l-m) * P_l^m = x * (2l-1) * P_{l-1}^m - (l+m-1) * P_{l-2}^m
	float Pll = 0.0f;
	for (int32 ll = AbsM + 2; ll <= l; ++ll)
	{
		Pll = (x * (2.0f * ll - 1.0f) * Pmm1 - (ll + AbsM - 1.0f) * Pmm) / (ll - AbsM);
		Pmm = Pmm1;
		Pmm1 = Pll;
	}

	return Pll;
}

float FAmbisonicsEncoder::ComputeSH(int32 l, int32 m, float Azimuth, float Elevation)
{
	// Spherical harmonic Y_l^m(theta, phi)
	// theta = elevation (0 at equator), phi = azimuth

	float CosElevation = FMath::Cos(Elevation);
	float SinElevation = FMath::Sin(Elevation);

	float Plm = AssociatedLegendre(l, FMath::Abs(m), SinElevation);

	if (m > 0)
	{
		return Plm * FMath::Cos(m * Azimuth);
	}
	else if (m < 0)
	{
		return Plm * FMath::Sin(-m * Azimuth);
	}
	else
	{
		return Plm;
	}
}

void FAmbisonicsEncoder::Encode(const FVector& Direction, TArray<float>& OutCoefficients) const
{
	int32 NumChannels = GetChannelCount();
	OutCoefficients.SetNum(NumChannels);

	// Convert direction to spherical coordinates
	// Azimuth: angle in XY plane from +X axis
	// Elevation: angle from XY plane

	FVector NormDir = Direction.GetSafeNormal();
	if (NormDir.IsNearlyZero())
	{
		// Default to forward
		NormDir = FVector::ForwardVector;
	}

	float Azimuth = FMath::Atan2(NormDir.Y, NormDir.X);
	float Elevation = FMath::Asin(FMath::Clamp(NormDir.Z, -1.0f, 1.0f));

	// Compute spherical harmonics for each channel
	for (int32 l = 0; l <= static_cast<int32>(Order); ++l)
	{
		for (int32 m = -l; m <= l; ++m)
		{
			int32 ACN = GetACN(l, m);
			float SH = ComputeSH(l, m, Azimuth, Elevation);
			OutCoefficients[ACN] = SH * NormalizationFactors[ACN];
		}
	}
}

void FAmbisonicsEncoder::EncodePosition(
	const FVector& Position,
	const FVector& ListenerPosition,
	TArray<float>& OutCoefficients,
	float& OutDistance) const
{
	FVector RelativePos = Position - ListenerPosition;
	OutDistance = RelativePos.Size();

	if (OutDistance > KINDA_SMALL_NUMBER)
	{
		Encode(RelativePos / OutDistance, OutCoefficients);
	}
	else
	{
		// Object at listener position - omnidirectional
		int32 NumChannels = GetChannelCount();
		OutCoefficients.SetNum(NumChannels);
		FMemory::Memzero(OutCoefficients.GetData(), NumChannels * sizeof(float));
		OutCoefficients[0] = 1.0f;  // W channel only
	}
}

// ============================================================================
// FAmbisonicsDecoder
// ============================================================================

FAmbisonicsDecoder::FAmbisonicsDecoder()
	: bConfigured(false)
	, Order(EAmbisonicsOrder::First)
	, Type(EAmbisonicsDecoderType::AllRAD)
	, NumSpeakers(0)
	, NumChannels(0)
{
}

void FAmbisonicsDecoder::Configure(
	const TArray<FSpatialSpeaker>& Speakers,
	EAmbisonicsOrder InOrder,
	EAmbisonicsDecoderType DecoderType)
{
	Order = InOrder;
	Type = DecoderType;
	NumSpeakers = Speakers.Num();
	NumChannels = GetAmbisonicsChannelCount(Order);

	if (NumSpeakers == 0)
	{
		bConfigured = false;
		return;
	}

	// Store speaker directions (normalized)
	SpeakerDirections.SetNum(NumSpeakers);
	for (int32 i = 0; i < NumSpeakers; ++i)
	{
		SpeakerDirections[i] = Speakers[i].Position.GetSafeNormal();
		if (SpeakerDirections[i].IsNearlyZero())
		{
			SpeakerDirections[i] = FVector::ForwardVector;
		}
	}

	// Initialize decode matrix
	DecodeMatrix.SetNum(NumSpeakers);
	for (int32 s = 0; s < NumSpeakers; ++s)
	{
		DecodeMatrix[s].SetNum(NumChannels);
	}

	// Compute decode matrix based on type
	switch (Type)
	{
	case EAmbisonicsDecoderType::Basic:
		ComputeBasicDecodeMatrix();
		break;

	case EAmbisonicsDecoderType::MaxRE:
		ComputeMaxREDecodeMatrix();
		break;

	case EAmbisonicsDecoderType::InPhase:
		ComputeInPhaseDecodeMatrix();
		break;

	case EAmbisonicsDecoderType::AllRAD:
	case EAmbisonicsDecoderType::EPAD:
	default:
		ComputeAllRADDecodeMatrix();
		break;
	}

	bConfigured = true;
}

void FAmbisonicsDecoder::ComputeBasicDecodeMatrix()
{
	// Basic/sampling decoder: D = (1/N) * Y^T
	// Each speaker samples the sound field at its direction

	FAmbisonicsEncoder Encoder;
	Encoder.SetOrder(Order);
	Encoder.SetNormalization(EAmbisonicsNormalization::SN3D);

	float NormFactor = 1.0f / NumSpeakers;

	for (int32 s = 0; s < NumSpeakers; ++s)
	{
		TArray<float> Coefficients;
		Encoder.Encode(SpeakerDirections[s], Coefficients);

		for (int32 c = 0; c < NumChannels; ++c)
		{
			DecodeMatrix[s][c] = Coefficients[c] * NormFactor;
		}
	}
}

void FAmbisonicsDecoder::ComputeMaxREDecodeMatrix()
{
	// Max rE decoder: Applies energy-preserving weights per order
	// Better high-frequency localization

	// First compute basic decode matrix
	ComputeBasicDecodeMatrix();

	// Apply max rE weights
	// These weights maximize the energy vector rE
	static const float MaxREWeights[] = {
		1.0f,                    // Order 0
		0.577350269f,            // Order 1: 1/sqrt(3)
		0.408248290f,            // Order 2: 1/sqrt(6)
		0.316227766f,            // Order 3: 1/sqrt(10)
		0.258198889f,            // Order 4: 1/sqrt(15)
		0.218217890f             // Order 5: 1/sqrt(21)
	};

	for (int32 s = 0; s < NumSpeakers; ++s)
	{
		for (int32 l = 0; l <= static_cast<int32>(Order); ++l)
		{
			float Weight = MaxREWeights[l];
			for (int32 m = -l; m <= l; ++m)
			{
				int32 ACN = GetACN(l, m);
				if (ACN < NumChannels)
				{
					DecodeMatrix[s][ACN] *= Weight;
				}
			}
		}
	}
}

void FAmbisonicsDecoder::ComputeInPhaseDecodeMatrix()
{
	// In-phase decoder: Reduces side lobes but also localization
	// Good for irregular arrays

	ComputeBasicDecodeMatrix();

	// In-phase weights reduce higher orders more aggressively
	for (int32 s = 0; s < NumSpeakers; ++s)
	{
		for (int32 l = 1; l <= static_cast<int32>(Order); ++l)
		{
			// Cosine-squared taper
			float OrderRatio = static_cast<float>(l) / static_cast<float>(Order);
			float Weight = FMath::Cos(OrderRatio * PI * 0.5f);
			Weight *= Weight;

			for (int32 m = -l; m <= l; ++m)
			{
				int32 ACN = GetACN(l, m);
				if (ACN < NumChannels)
				{
					DecodeMatrix[s][ACN] *= Weight;
				}
			}
		}
	}
}

void FAmbisonicsDecoder::ComputeAllRADDecodeMatrix()
{
	// AllRAD (All-Round Ambisonic Decoding)
	// Uses pseudoinverse for optimal decoding
	// D = Y_speakers^+ = (Y^T * Y)^-1 * Y^T

	FAmbisonicsEncoder Encoder;
	Encoder.SetOrder(Order);
	Encoder.SetNormalization(EAmbisonicsNormalization::SN3D);

	// Build Y matrix [NumSpeakers x NumChannels]
	TArray<TArray<float>> Y;
	Y.SetNum(NumSpeakers);
	for (int32 s = 0; s < NumSpeakers; ++s)
	{
		Encoder.Encode(SpeakerDirections[s], Y[s]);
	}

	// Compute pseudoinverse
	TArray<TArray<float>> YPinv;
	PseudoInverse(Y, YPinv);

	// YPinv is [NumChannels x NumSpeakers], we need [NumSpeakers x NumChannels]
	// So we transpose it to get the decode matrix
	for (int32 s = 0; s < NumSpeakers; ++s)
	{
		for (int32 c = 0; c < NumChannels; ++c)
		{
			DecodeMatrix[s][c] = YPinv[c][s];
		}
	}

	// Apply energy normalization
	float EnergySum = 0.0f;
	for (int32 s = 0; s < NumSpeakers; ++s)
	{
		float SpeakerEnergy = 0.0f;
		for (int32 c = 0; c < NumChannels; ++c)
		{
			SpeakerEnergy += DecodeMatrix[s][c] * DecodeMatrix[s][c];
		}
		EnergySum += SpeakerEnergy;
	}

	if (EnergySum > KINDA_SMALL_NUMBER)
	{
		float NormFactor = FMath::Sqrt(NumSpeakers / EnergySum);
		for (int32 s = 0; s < NumSpeakers; ++s)
		{
			for (int32 c = 0; c < NumChannels; ++c)
			{
				DecodeMatrix[s][c] *= NormFactor;
			}
		}
	}
}

void FAmbisonicsDecoder::PseudoInverse(
	const TArray<TArray<float>>& A,
	TArray<TArray<float>>& OutPinv)
{
	// Moore-Penrose pseudoinverse: A^+ = (A^T * A)^-1 * A^T
	// For overdetermined systems (more speakers than channels)

	int32 M = A.Num();           // Rows (speakers)
	int32 N = A[0].Num();        // Cols (channels)

	// Compute A^T * A (N x N)
	TArray<TArray<float>> AtA;
	AtA.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		AtA[i].SetNumZeroed(N);
		for (int32 j = 0; j < N; ++j)
		{
			float Sum = 0.0f;
			for (int32 k = 0; k < M; ++k)
			{
				Sum += A[k][i] * A[k][j];
			}
			AtA[i][j] = Sum;
		}
	}

	// Add small regularization for stability
	float Reg = 1e-6f;
	for (int32 i = 0; i < N; ++i)
	{
		AtA[i][i] += Reg;
	}

	// Invert (A^T * A) using Gauss-Jordan elimination
	TArray<TArray<float>> AtAInv;
	AtAInv.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		AtAInv[i].SetNumZeroed(N);
		AtAInv[i][i] = 1.0f;  // Start with identity
	}

	// Forward elimination
	for (int32 i = 0; i < N; ++i)
	{
		// Find pivot
		float MaxVal = FMath::Abs(AtA[i][i]);
		int32 MaxRow = i;
		for (int32 k = i + 1; k < N; ++k)
		{
			if (FMath::Abs(AtA[k][i]) > MaxVal)
			{
				MaxVal = FMath::Abs(AtA[k][i]);
				MaxRow = k;
			}
		}

		// Swap rows
		if (MaxRow != i)
		{
			Swap(AtA[i], AtA[MaxRow]);
			Swap(AtAInv[i], AtAInv[MaxRow]);
		}

		// Scale row
		float Pivot = AtA[i][i];
		if (FMath::Abs(Pivot) < 1e-10f)
		{
			continue;  // Skip singular rows
		}

		for (int32 j = 0; j < N; ++j)
		{
			AtA[i][j] /= Pivot;
			AtAInv[i][j] /= Pivot;
		}

		// Eliminate column
		for (int32 k = 0; k < N; ++k)
		{
			if (k != i)
			{
				float Factor = AtA[k][i];
				for (int32 j = 0; j < N; ++j)
				{
					AtA[k][j] -= Factor * AtA[i][j];
					AtAInv[k][j] -= Factor * AtAInv[i][j];
				}
			}
		}
	}

	// Compute (A^T * A)^-1 * A^T (N x M)
	OutPinv.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		OutPinv[i].SetNumZeroed(M);
		for (int32 j = 0; j < M; ++j)
		{
			float Sum = 0.0f;
			for (int32 k = 0; k < N; ++k)
			{
				Sum += AtAInv[i][k] * A[j][k];
			}
			OutPinv[i][j] = Sum;
		}
	}
}

void FAmbisonicsDecoder::Decode(const TArray<float>& Coefficients, TArray<float>& OutGains) const
{
	if (!bConfigured || Coefficients.Num() != NumChannels)
	{
		OutGains.SetNumZeroed(NumSpeakers);
		return;
	}

	OutGains.SetNum(NumSpeakers);

	// Matrix-vector multiplication: gains = DecodeMatrix * coefficients
	for (int32 s = 0; s < NumSpeakers; ++s)
	{
		float Sum = 0.0f;
		for (int32 c = 0; c < NumChannels; ++c)
		{
			Sum += DecodeMatrix[s][c] * Coefficients[c];
		}
		OutGains[s] = Sum;
	}
}

// ============================================================================
// FSpatialRendererHOA
// ============================================================================

FSpatialRendererHOA::FSpatialRendererHOA()
	: Order(EAmbisonicsOrder::First)
	, DecoderType(EAmbisonicsDecoderType::AllRAD)
	, ListenerPosition(FVector::ZeroVector)
	, SceneRotation(FRotator::ZeroRotator)
	, bNearFieldCompensation(false)
	, NearFieldDistance(100.0f)
	, bUseOrderReductionForSpread(true)
	, bConfigured(false)
{
	Encoder.SetOrder(Order);
}

FSpatialRendererHOA::~FSpatialRendererHOA()
{
}

void FSpatialRendererHOA::Configure(const TArray<FSpatialSpeaker>& Speakers)
{
	ConfiguredSpeakers = Speakers;

	SpeakerIds.SetNum(Speakers.Num());
	for (int32 i = 0; i < Speakers.Num(); ++i)
	{
		SpeakerIds[i] = Speakers[i].Id;
	}

	ReconfigureDecoder();
	bConfigured = Decoder.IsConfigured();
}

bool FSpatialRendererHOA::IsConfigured() const
{
	return bConfigured;
}

int32 FSpatialRendererHOA::GetSpeakerCount() const
{
	return ConfiguredSpeakers.Num();
}

void FSpatialRendererHOA::ComputeGains(
	const FVector& ObjectPosition,
	float Spread,
	TArray<FSpatialSpeakerGain>& OutGains) const
{
	OutGains.Empty();

	if (!bConfigured)
	{
		return;
	}

	// Encode position to Ambisonics
	TArray<float> Coefficients;
	float Distance;
	Encoder.EncodePosition(ObjectPosition, ListenerPosition, Coefficients, Distance);

	// Apply scene rotation if set
	if (!SceneRotation.IsNearlyZero())
	{
		// For rotation, we'd need to rotate the coefficients
		// This is a simplification - proper implementation would use rotation matrices
		// For now, we rotate the position before encoding
		FVector RotatedPos = SceneRotation.RotateVector(ObjectPosition - ListenerPosition) + ListenerPosition;
		Encoder.EncodePosition(RotatedPos, ListenerPosition, Coefficients, Distance);
	}

	// Apply spread (order reduction)
	if (Spread > 0.0f)
	{
		ApplySpread(const_cast<TArray<float>&>(Coefficients), Spread);
	}

	// Decode to speaker gains
	TArray<float> SpeakerGains;
	Decoder.Decode(Coefficients, SpeakerGains);

	// Compute distance attenuation
	float DistanceGain = ComputeDistanceAttenuation(Distance);

	// Build output with gain and delay
	OutGains.SetNum(ConfiguredSpeakers.Num());
	for (int32 i = 0; i < ConfiguredSpeakers.Num(); ++i)
	{
		OutGains[i].SpeakerId = SpeakerIds[i];
		OutGains[i].SpeakerIndex = i;
		OutGains[i].Gain = FMath::Max(0.0f, SpeakerGains[i] * DistanceGain);

		// Compute delay for phase coherence
		// Distance from object to speaker (simplified - assumes speakers at same distance from listener)
		float SpeakerDistance = (ConfiguredSpeakers[i].Position - ListenerPosition).Size();
		float ObjectToSpeaker = (ConfiguredSpeakers[i].Position - ObjectPosition).Size();
		float DelayMs = (ObjectToSpeaker / SPEED_OF_SOUND_CM) * 1000.0f;
		OutGains[i].DelayMs = DelayMs;
	}
}

FString FSpatialRendererHOA::GetDescription() const
{
	return FString::Printf(
		TEXT("Higher-Order Ambisonics renderer (Order %d, %d channels). "
		     "Encodes to spherical harmonics and decodes to speaker array. "
		     "Best for immersive dome/sphere installations and VR/AR."),
		static_cast<int32>(Order),
		GetAmbisonicsChannelCount(Order));
}

FString FSpatialRendererHOA::GetDiagnosticInfo() const
{
	return FString::Printf(
		TEXT("HOA Renderer:\n"
		     "  Order: %d\n"
		     "  Channels: %d\n"
		     "  Speakers: %d\n"
		     "  Decoder: %s\n"
		     "  Listener: (%.1f, %.1f, %.1f)\n"
		     "  Near-field: %s"),
		static_cast<int32>(Order),
		GetAmbisonicsChannelCount(Order),
		ConfiguredSpeakers.Num(),
		DecoderType == EAmbisonicsDecoderType::AllRAD ? TEXT("AllRAD") :
			DecoderType == EAmbisonicsDecoderType::MaxRE ? TEXT("MaxRE") :
			DecoderType == EAmbisonicsDecoderType::InPhase ? TEXT("InPhase") :
			TEXT("Basic"),
		ListenerPosition.X, ListenerPosition.Y, ListenerPosition.Z,
		bNearFieldCompensation ? TEXT("Enabled") : TEXT("Disabled"));
}

TArray<FString> FSpatialRendererHOA::Validate() const
{
	TArray<FString> Errors;

	if (ConfiguredSpeakers.Num() == 0)
	{
		Errors.Add(TEXT("No speakers configured"));
	}

	int32 RequiredChannels = GetAmbisonicsChannelCount(Order);
	if (ConfiguredSpeakers.Num() < RequiredChannels)
	{
		Errors.Add(FString::Printf(
			TEXT("Not enough speakers for order %d. Need at least %d, have %d. "
			     "Consider lowering the Ambisonics order."),
			static_cast<int32>(Order), RequiredChannels, ConfiguredSpeakers.Num()));
	}

	// Check for degenerate speaker positions
	for (int32 i = 0; i < ConfiguredSpeakers.Num(); ++i)
	{
		if (ConfiguredSpeakers[i].Position.IsNearlyZero())
		{
			Errors.Add(FString::Printf(TEXT("Speaker %d has zero position"), i));
		}
	}

	return Errors;
}

void FSpatialRendererHOA::SetOrder(EAmbisonicsOrder InOrder)
{
	if (Order != InOrder)
	{
		Order = InOrder;
		Encoder.SetOrder(Order);
		if (bConfigured)
		{
			ReconfigureDecoder();
		}
	}
}

void FSpatialRendererHOA::SetDecoderType(EAmbisonicsDecoderType InType)
{
	if (DecoderType != InType)
	{
		DecoderType = InType;
		if (bConfigured)
		{
			ReconfigureDecoder();
		}
	}
}

void FSpatialRendererHOA::SetListenerPosition(const FVector& Position)
{
	ListenerPosition = Position;
}

void FSpatialRendererHOA::SetSceneRotation(const FRotator& Rotation)
{
	SceneRotation = Rotation;
}

void FSpatialRendererHOA::SetNearFieldCompensation(bool bEnable, float ProximityDistance)
{
	bNearFieldCompensation = bEnable;
	NearFieldDistance = ProximityDistance;
}

void FSpatialRendererHOA::SetSpreadMode(bool bUseOrderReduction)
{
	bUseOrderReductionForSpread = bUseOrderReduction;
}

void FSpatialRendererHOA::ReconfigureDecoder()
{
	Decoder.Configure(ConfiguredSpeakers, Order, DecoderType);
}

float FSpatialRendererHOA::ComputeDistanceAttenuation(float Distance) const
{
	// Simple inverse distance with near-field handling
	if (Distance < 1.0f)
	{
		return 1.0f;  // Clamp at very close distances
	}

	// Reference distance of 100cm for 0dB
	float RefDistance = 100.0f;
	float Attenuation = RefDistance / Distance;

	// Optional near-field boost
	if (bNearFieldCompensation && Distance < NearFieldDistance)
	{
		float NearFieldFactor = NearFieldDistance / FMath::Max(Distance, 1.0f);
		Attenuation *= FMath::Sqrt(NearFieldFactor);
	}

	return FMath::Clamp(Attenuation, 0.0f, 4.0f);  // Max +12dB
}

void FSpatialRendererHOA::ApplySpread(TArray<float>& Coefficients, float Spread) const
{
	if (!bUseOrderReductionForSpread || Spread <= 0.0f)
	{
		return;
	}

	// Spread implemented as order reduction
	// Higher spread = more weight on lower orders = more diffuse
	// Spread of 0 = point source, Spread of 180 = omnidirectional

	float SpreadNorm = FMath::Clamp(Spread / 180.0f, 0.0f, 1.0f);

	for (int32 l = 1; l <= static_cast<int32>(Order); ++l)
	{
		// Progressive attenuation of higher orders
		float OrderWeight = FMath::Pow(1.0f - SpreadNorm, static_cast<float>(l));

		for (int32 m = -l; m <= l; ++m)
		{
			int32 ACN = GetACN(l, m);
			if (ACN < Coefficients.Num())
			{
				Coefficients[ACN] *= OrderWeight;
			}
		}
	}
}
