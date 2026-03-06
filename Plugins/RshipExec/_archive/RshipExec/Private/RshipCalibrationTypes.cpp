// Rship Calibration Types Implementation

#include "RshipCalibrationTypes.h"

// ============================================================================
// FRshipFixtureCalibration Implementation
// ============================================================================

float FRshipFixtureCalibration::DmxToOutput(int32 InDmxValue) const
{
    // Clamp input
    InDmxValue = FMath::Clamp(InDmxValue, 0, 255);

    // No dimmer curve - return linear mapping
    if (DimmerCurve.Num() == 0)
    {
        return InDmxValue / 255.0f;
    }

    // Single point - return that value
    if (DimmerCurve.Num() == 1)
    {
        return DimmerCurve[0].OutputPercent;
    }

    // Find surrounding points and interpolate
    int32 LowerIdx = 0;
    int32 UpperIdx = DimmerCurve.Num() - 1;

    // Binary search for the correct segment
    for (int32 i = 0; i < DimmerCurve.Num() - 1; i++)
    {
        if (DimmerCurve[i].DmxValue <= InDmxValue && DimmerCurve[i + 1].DmxValue >= InDmxValue)
        {
            LowerIdx = i;
            UpperIdx = i + 1;
            break;
        }
    }

    // Handle edge cases
    if (InDmxValue <= DimmerCurve[0].DmxValue)
    {
        return DimmerCurve[0].OutputPercent;
    }
    if (InDmxValue >= DimmerCurve.Last().DmxValue)
    {
        return DimmerCurve.Last().OutputPercent;
    }

    // Linear interpolation between points
    const FRshipDimmerCurvePoint& Lower = DimmerCurve[LowerIdx];
    const FRshipDimmerCurvePoint& Upper = DimmerCurve[UpperIdx];

    float DmxRange = Upper.DmxValue - Lower.DmxValue;
    if (DmxRange <= 0.0f)
    {
        return Lower.OutputPercent;
    }

    float T = (InDmxValue - Lower.DmxValue) / DmxRange;
    return FMath::Lerp(Lower.OutputPercent, Upper.OutputPercent, T);
}

FLinearColor FRshipFixtureCalibration::GetColorCorrection(float TargetKelvin) const
{
    // No color calibrations - return white (no correction)
    if (ColorCalibrations.Num() == 0)
    {
        return FLinearColor::White;
    }

    // Single calibration point - return that correction
    if (ColorCalibrations.Num() == 1)
    {
        return ColorCalibrations[0].RgbCorrection;
    }

    // Find surrounding calibration points and interpolate
    int32 LowerIdx = 0;
    int32 UpperIdx = ColorCalibrations.Num() - 1;

    // Sort assumption: calibrations are ordered by TargetKelvin
    for (int32 i = 0; i < ColorCalibrations.Num() - 1; i++)
    {
        if (ColorCalibrations[i].TargetKelvin <= TargetKelvin &&
            ColorCalibrations[i + 1].TargetKelvin >= TargetKelvin)
        {
            LowerIdx = i;
            UpperIdx = i + 1;
            break;
        }
    }

    // Edge cases
    if (TargetKelvin <= ColorCalibrations[0].TargetKelvin)
    {
        return ColorCalibrations[0].RgbCorrection;
    }
    if (TargetKelvin >= ColorCalibrations.Last().TargetKelvin)
    {
        return ColorCalibrations.Last().RgbCorrection;
    }

    // Interpolate
    const FRshipColorCalibration& Lower = ColorCalibrations[LowerIdx];
    const FRshipColorCalibration& Upper = ColorCalibrations[UpperIdx];

    float KelvinRange = Upper.TargetKelvin - Lower.TargetKelvin;
    if (KelvinRange <= 0.0f)
    {
        return Lower.RgbCorrection;
    }

    float T = (TargetKelvin - Lower.TargetKelvin) / KelvinRange;
    return FLinearColor(
        FMath::Lerp(Lower.RgbCorrection.R, Upper.RgbCorrection.R, T),
        FMath::Lerp(Lower.RgbCorrection.G, Upper.RgbCorrection.G, T),
        FMath::Lerp(Lower.RgbCorrection.B, Upper.RgbCorrection.B, T),
        1.0f
    );
}

float FRshipFixtureCalibration::GetCalibratedBeamAngle(float SpecBeamAngle) const
{
    return SpecBeamAngle * BeamAngleMultiplier;
}

float FRshipFixtureCalibration::GetCalibratedFieldAngle(float SpecFieldAngle) const
{
    return SpecFieldAngle * FieldAngleMultiplier;
}

// ============================================================================
// FRshipColorCheckerData Implementation
// ============================================================================

FLinearColor FRshipColorCheckerData::ApplyMatrix(const FLinearColor& Input) const
{
    if (ColorMatrix.Num() != 9)
    {
        return Input;
    }

    // Apply 3x3 matrix multiplication
    // Matrix is stored row-major: [0,1,2] = first row, etc.
    float OutR = Input.R * ColorMatrix[0] + Input.G * ColorMatrix[1] + Input.B * ColorMatrix[2];
    float OutG = Input.R * ColorMatrix[3] + Input.G * ColorMatrix[4] + Input.B * ColorMatrix[5];
    float OutB = Input.R * ColorMatrix[6] + Input.G * ColorMatrix[7] + Input.B * ColorMatrix[8];

    return FLinearColor(
        FMath::Clamp(OutR, 0.0f, 1.0f),
        FMath::Clamp(OutG, 0.0f, 1.0f),
        FMath::Clamp(OutB, 0.0f, 1.0f),
        Input.A
    );
}

// ============================================================================
// FRshipColorProfile Implementation
// ============================================================================

FLinearColor FRshipColorProfile::ApplyColorCorrection(const FLinearColor& Input) const
{
    FLinearColor Corrected = Input;

    // Step 1: Apply white balance correction
    if (WhiteBalance.IsValid())
    {
        Corrected.R *= WhiteBalance.Multipliers.R;
        Corrected.G *= WhiteBalance.Multipliers.G;
        Corrected.B *= WhiteBalance.Multipliers.B;

        // Clamp
        Corrected.R = FMath::Min(Corrected.R, 1.0f);
        Corrected.G = FMath::Min(Corrected.G, 1.0f);
        Corrected.B = FMath::Min(Corrected.B, 1.0f);
    }

    // Step 2: Apply color checker matrix
    if (ColorChecker.IsValid())
    {
        Corrected = ColorChecker.ApplyMatrix(Corrected);
    }

    return Corrected;
}

FString FRshipColorProfile::GetCalibrationQuality() const
{
    if (!ColorChecker.IsValid())
    {
        return TEXT("uncalibrated");
    }

    float DeltaE = ColorChecker.DeltaE;

    if (DeltaE <= RshipCalibrationQuality::ExcellentMaxDeltaE)
    {
        return TEXT("excellent");
    }
    if (DeltaE <= RshipCalibrationQuality::GoodMaxDeltaE)
    {
        return TEXT("good");
    }
    if (DeltaE <= RshipCalibrationQuality::AcceptableMaxDeltaE)
    {
        return TEXT("acceptable");
    }
    return TEXT("poor");
}
