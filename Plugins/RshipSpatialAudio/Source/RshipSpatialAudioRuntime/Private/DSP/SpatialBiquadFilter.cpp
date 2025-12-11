// Copyright Rocketship. All Rights Reserved.

#include "DSP/SpatialBiquadFilter.h"

// ============================================================================
// FSpatialBiquadFilter
// ============================================================================

FSpatialBiquadFilter::FSpatialBiquadFilter()
	: B0(1.0f), B1(0.0f), B2(0.0f)
	, A1(0.0f), A2(0.0f)
	, TargetB0(1.0f), TargetB1(0.0f), TargetB2(0.0f)
	, TargetA1(0.0f), TargetA2(0.0f)
	, Z1(0.0f), Z2(0.0f)
	, FilterType(ESpatialBiquadType::LowPass)
	, bSmoothingEnabled(false)
	, SmoothingCoeff(0.001f)
{
}

void FSpatialBiquadFilter::Reset()
{
	Z1 = 0.0f;
	Z2 = 0.0f;
}

void FSpatialBiquadFilter::SetCoefficients(float InB0, float InB1, float InB2, float InA1, float InA2)
{
	B0 = TargetB0 = InB0;
	B1 = TargetB1 = InB1;
	B2 = TargetB2 = InB2;
	A1 = TargetA1 = InA1;
	A2 = TargetA2 = InA2;
}

void FSpatialBiquadFilter::SetTargetCoefficients(float InB0, float InB1, float InB2, float InA1, float InA2)
{
	TargetB0 = InB0;
	TargetB1 = InB1;
	TargetB2 = InB2;
	TargetA1 = InA1;
	TargetA2 = InA2;
}

void FSpatialBiquadFilter::SetLowPass(float SampleRate, float Frequency, float Q)
{
	FilterType = ESpatialBiquadType::LowPass;

	float W0 = 2.0f * PI * Frequency / SampleRate;
	float CosW0 = FMath::Cos(W0);
	float SinW0 = FMath::Sin(W0);
	float Alpha = SinW0 / (2.0f * Q);

	float A0 = 1.0f + Alpha;
	float InvA0 = 1.0f / A0;

	float NewB0 = ((1.0f - CosW0) / 2.0f) * InvA0;
	float NewB1 = (1.0f - CosW0) * InvA0;
	float NewB2 = ((1.0f - CosW0) / 2.0f) * InvA0;
	float NewA1 = (-2.0f * CosW0) * InvA0;
	float NewA2 = (1.0f - Alpha) * InvA0;

	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
	else
	{
		SetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
}

void FSpatialBiquadFilter::SetHighPass(float SampleRate, float Frequency, float Q)
{
	FilterType = ESpatialBiquadType::HighPass;

	float W0 = 2.0f * PI * Frequency / SampleRate;
	float CosW0 = FMath::Cos(W0);
	float SinW0 = FMath::Sin(W0);
	float Alpha = SinW0 / (2.0f * Q);

	float A0 = 1.0f + Alpha;
	float InvA0 = 1.0f / A0;

	float NewB0 = ((1.0f + CosW0) / 2.0f) * InvA0;
	float NewB1 = (-(1.0f + CosW0)) * InvA0;
	float NewB2 = ((1.0f + CosW0) / 2.0f) * InvA0;
	float NewA1 = (-2.0f * CosW0) * InvA0;
	float NewA2 = (1.0f - Alpha) * InvA0;

	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
	else
	{
		SetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
}

void FSpatialBiquadFilter::SetBandPass(float SampleRate, float Frequency, float Q)
{
	FilterType = ESpatialBiquadType::BandPass;

	float W0 = 2.0f * PI * Frequency / SampleRate;
	float CosW0 = FMath::Cos(W0);
	float SinW0 = FMath::Sin(W0);
	float Alpha = SinW0 / (2.0f * Q);

	float A0 = 1.0f + Alpha;
	float InvA0 = 1.0f / A0;

	float NewB0 = (SinW0 / 2.0f) * InvA0;  // = Q * Alpha
	float NewB1 = 0.0f;
	float NewB2 = (-SinW0 / 2.0f) * InvA0;
	float NewA1 = (-2.0f * CosW0) * InvA0;
	float NewA2 = (1.0f - Alpha) * InvA0;

	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
	else
	{
		SetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
}

void FSpatialBiquadFilter::SetNotch(float SampleRate, float Frequency, float Q)
{
	FilterType = ESpatialBiquadType::Notch;

	float W0 = 2.0f * PI * Frequency / SampleRate;
	float CosW0 = FMath::Cos(W0);
	float SinW0 = FMath::Sin(W0);
	float Alpha = SinW0 / (2.0f * Q);

	float A0 = 1.0f + Alpha;
	float InvA0 = 1.0f / A0;

	float NewB0 = 1.0f * InvA0;
	float NewB1 = (-2.0f * CosW0) * InvA0;
	float NewB2 = 1.0f * InvA0;
	float NewA1 = (-2.0f * CosW0) * InvA0;
	float NewA2 = (1.0f - Alpha) * InvA0;

	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
	else
	{
		SetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
}

void FSpatialBiquadFilter::SetPeakingEQ(float SampleRate, float Frequency, float GainDb, float Q)
{
	FilterType = ESpatialBiquadType::PeakingEQ;

	float A = FMath::Pow(10.0f, GainDb / 40.0f);  // sqrt of dB gain
	float W0 = 2.0f * PI * Frequency / SampleRate;
	float CosW0 = FMath::Cos(W0);
	float SinW0 = FMath::Sin(W0);
	float Alpha = SinW0 / (2.0f * Q);

	float A0 = 1.0f + Alpha / A;
	float InvA0 = 1.0f / A0;

	float NewB0 = (1.0f + Alpha * A) * InvA0;
	float NewB1 = (-2.0f * CosW0) * InvA0;
	float NewB2 = (1.0f - Alpha * A) * InvA0;
	float NewA1 = (-2.0f * CosW0) * InvA0;
	float NewA2 = (1.0f - Alpha / A) * InvA0;

	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
	else
	{
		SetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
}

void FSpatialBiquadFilter::SetLowShelf(float SampleRate, float Frequency, float GainDb, float S)
{
	FilterType = ESpatialBiquadType::LowShelf;

	float A = FMath::Pow(10.0f, GainDb / 40.0f);
	float W0 = 2.0f * PI * Frequency / SampleRate;
	float CosW0 = FMath::Cos(W0);
	float SinW0 = FMath::Sin(W0);
	float Alpha = SinW0 / 2.0f * FMath::Sqrt((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);

	float Ap1 = A + 1.0f;
	float Am1 = A - 1.0f;
	float TwoSqrtAAlpha = 2.0f * FMath::Sqrt(A) * Alpha;

	float A0 = Ap1 + Am1 * CosW0 + TwoSqrtAAlpha;
	float InvA0 = 1.0f / A0;

	float NewB0 = A * (Ap1 - Am1 * CosW0 + TwoSqrtAAlpha) * InvA0;
	float NewB1 = 2.0f * A * (Am1 - Ap1 * CosW0) * InvA0;
	float NewB2 = A * (Ap1 - Am1 * CosW0 - TwoSqrtAAlpha) * InvA0;
	float NewA1 = -2.0f * (Am1 + Ap1 * CosW0) * InvA0;
	float NewA2 = (Ap1 + Am1 * CosW0 - TwoSqrtAAlpha) * InvA0;

	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
	else
	{
		SetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
}

void FSpatialBiquadFilter::SetHighShelf(float SampleRate, float Frequency, float GainDb, float S)
{
	FilterType = ESpatialBiquadType::HighShelf;

	float A = FMath::Pow(10.0f, GainDb / 40.0f);
	float W0 = 2.0f * PI * Frequency / SampleRate;
	float CosW0 = FMath::Cos(W0);
	float SinW0 = FMath::Sin(W0);
	float Alpha = SinW0 / 2.0f * FMath::Sqrt((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);

	float Ap1 = A + 1.0f;
	float Am1 = A - 1.0f;
	float TwoSqrtAAlpha = 2.0f * FMath::Sqrt(A) * Alpha;

	float A0 = Ap1 - Am1 * CosW0 + TwoSqrtAAlpha;
	float InvA0 = 1.0f / A0;

	float NewB0 = A * (Ap1 + Am1 * CosW0 + TwoSqrtAAlpha) * InvA0;
	float NewB1 = -2.0f * A * (Am1 + Ap1 * CosW0) * InvA0;
	float NewB2 = A * (Ap1 + Am1 * CosW0 - TwoSqrtAAlpha) * InvA0;
	float NewA1 = 2.0f * (Am1 - Ap1 * CosW0) * InvA0;
	float NewA2 = (Ap1 - Am1 * CosW0 - TwoSqrtAAlpha) * InvA0;

	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
	else
	{
		SetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
}

void FSpatialBiquadFilter::SetAllPass(float SampleRate, float Frequency, float Q)
{
	FilterType = ESpatialBiquadType::AllPass;

	float W0 = 2.0f * PI * Frequency / SampleRate;
	float CosW0 = FMath::Cos(W0);
	float SinW0 = FMath::Sin(W0);
	float Alpha = SinW0 / (2.0f * Q);

	float A0 = 1.0f + Alpha;
	float InvA0 = 1.0f / A0;

	float NewB0 = (1.0f - Alpha) * InvA0;
	float NewB1 = (-2.0f * CosW0) * InvA0;
	float NewB2 = (1.0f + Alpha) * InvA0;
	float NewA1 = (-2.0f * CosW0) * InvA0;
	float NewA2 = (1.0f - Alpha) * InvA0;

	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
	else
	{
		SetCoefficients(NewB0, NewB1, NewB2, NewA1, NewA2);
	}
}

void FSpatialBiquadFilter::SetBypass(float SampleRate)
{
	// Unity gain, no filtering: B0=1, B1=B2=A1=A2=0
	if (bSmoothingEnabled)
	{
		SetTargetCoefficients(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	}
	else
	{
		SetCoefficients(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	}
}

float FSpatialBiquadFilter::ProcessSmoothed(float Input)
{
	SmoothCoefficients();
	return Process(Input);
}

void FSpatialBiquadFilter::ProcessBuffer(float* Buffer, int32 NumSamples)
{
	for (int32 i = 0; i < NumSamples; ++i)
	{
		Buffer[i] = Process(Buffer[i]);
	}
}

void FSpatialBiquadFilter::ProcessBufferSmoothed(float* Buffer, int32 NumSamples)
{
	for (int32 i = 0; i < NumSamples; ++i)
	{
		SmoothCoefficients();
		Buffer[i] = Process(Buffer[i]);
	}
}

float FSpatialBiquadFilter::GetMagnitudeResponse(float Frequency, float SampleRate) const
{
	float W = 2.0f * PI * Frequency / SampleRate;
	float CosW = FMath::Cos(W);
	float Cos2W = FMath::Cos(2.0f * W);
	float SinW = FMath::Sin(W);
	float Sin2W = FMath::Sin(2.0f * W);

	// Numerator: B0 + B1*e^(-jw) + B2*e^(-2jw)
	float NumReal = B0 + B1 * CosW + B2 * Cos2W;
	float NumImag = -B1 * SinW - B2 * Sin2W;

	// Denominator: 1 + A1*e^(-jw) + A2*e^(-2jw)
	float DenReal = 1.0f + A1 * CosW + A2 * Cos2W;
	float DenImag = -A1 * SinW - A2 * Sin2W;

	float NumMagSq = NumReal * NumReal + NumImag * NumImag;
	float DenMagSq = DenReal * DenReal + DenImag * DenImag;

	return FMath::Sqrt(NumMagSq / FMath::Max(DenMagSq, 1e-10f));
}

float FSpatialBiquadFilter::GetMagnitudeResponseDb(float Frequency, float SampleRate) const
{
	return 20.0f * FMath::LogX(10.0f, FMath::Max(GetMagnitudeResponse(Frequency, SampleRate), 1e-10f));
}

void FSpatialBiquadFilter::SetSmoothingTime(float TimeMs, float SampleRate)
{
	// Convert time constant to per-sample smoothing coefficient
	float TimeSamples = (TimeMs / 1000.0f) * SampleRate;
	SmoothingCoeff = 1.0f - FMath::Exp(-1.0f / FMath::Max(TimeSamples, 1.0f));
}

// ============================================================================
// FSpatialCascadedBiquad
// ============================================================================

FSpatialCascadedBiquad::FSpatialCascadedBiquad()
{
}

void FSpatialCascadedBiquad::Reset()
{
	for (FSpatialBiquadFilter& Stage : Stages)
	{
		Stage.Reset();
	}
}

void FSpatialCascadedBiquad::SetStageCount(int32 Count)
{
	Stages.SetNum(Count);
}

void FSpatialCascadedBiquad::SetLinkwitzRileyLowPass(float SampleRate, float Frequency, int32 Order)
{
	// Linkwitz-Riley is cascaded Butterworth filters
	// LR2 = 1 stage (2nd order), LR4 = 2 stages (4th order)
	int32 NumStages = Order / 2;
	SetStageCount(NumStages);

	// Q for cascaded Butterworth = 0.707 (sqrt(2)/2)
	for (int32 i = 0; i < NumStages; ++i)
	{
		Stages[i].SetLowPass(SampleRate, Frequency, 0.707f);
	}
}

void FSpatialCascadedBiquad::SetLinkwitzRileyHighPass(float SampleRate, float Frequency, int32 Order)
{
	int32 NumStages = Order / 2;
	SetStageCount(NumStages);

	for (int32 i = 0; i < NumStages; ++i)
	{
		Stages[i].SetHighPass(SampleRate, Frequency, 0.707f);
	}
}

void FSpatialCascadedBiquad::SetButterworthLowPass(float SampleRate, float Frequency, int32 Order)
{
	int32 NumStages = (Order + 1) / 2;
	SetStageCount(NumStages);

	// Calculate Q values for higher-order Butterworth
	for (int32 i = 0; i < NumStages; ++i)
	{
		float Angle = PI * (2.0f * i + 1) / (2.0f * Order);
		float Q = 1.0f / (2.0f * FMath::Cos(Angle));
		Stages[i].SetLowPass(SampleRate, Frequency, Q);
	}
}

void FSpatialCascadedBiquad::SetButterworthHighPass(float SampleRate, float Frequency, int32 Order)
{
	int32 NumStages = (Order + 1) / 2;
	SetStageCount(NumStages);

	for (int32 i = 0; i < NumStages; ++i)
	{
		float Angle = PI * (2.0f * i + 1) / (2.0f * Order);
		float Q = 1.0f / (2.0f * FMath::Cos(Angle));
		Stages[i].SetHighPass(SampleRate, Frequency, Q);
	}
}

float FSpatialCascadedBiquad::Process(float Input)
{
	float Output = Input;
	for (FSpatialBiquadFilter& Stage : Stages)
	{
		Output = Stage.Process(Output);
	}
	return Output;
}

void FSpatialCascadedBiquad::ProcessBuffer(float* Buffer, int32 NumSamples)
{
	for (FSpatialBiquadFilter& Stage : Stages)
	{
		Stage.ProcessBuffer(Buffer, NumSamples);
	}
}
