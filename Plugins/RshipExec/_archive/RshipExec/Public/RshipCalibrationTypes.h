// Rship Calibration Data Types
// Matches entity schemas from @rship/entities-core

#pragma once

#include "CoreMinimal.h"
#include "RshipCalibrationTypes.generated.h"

// ============================================================================
// FIXTURE CALIBRATION TYPES
// ============================================================================

/**
 * A single point on the dimmer curve mapping DMX value to output percent
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDimmerCurvePoint
{
    GENERATED_BODY()

    /** DMX input value (0-255) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    int32 DmxValue = 0;

    /** Output intensity (0.0 - 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    float OutputPercent = 0.0f;
};

/**
 * Color calibration data for a specific color temperature
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipColorCalibration
{
    GENERATED_BODY()

    /** Target color temperature in Kelvin */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    float TargetKelvin = 6500.0f;

    /** Actual measured color temperature in Kelvin */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    float MeasuredKelvin = 6500.0f;

    /** CIE xy chromaticity offset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    FVector2D ChromaticityOffset = FVector2D::ZeroVector;

    /** RGB correction multipliers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    FLinearColor RgbCorrection = FLinearColor::White;
};

/**
 * Full fixture calibration profile matching FixtureCalibration entity
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureCalibration
{
    GENERATED_BODY()

    /** Entity ID */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Id;

    /** Display name */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Name;

    /** Associated fixture type ID */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString FixtureTypeId;

    /** Project ID (scope) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString ProjectId;

    /** Dimmer curve points for intensity correction */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    TArray<FRshipDimmerCurvePoint> DimmerCurve;

    /** Minimum DMX value that produces visible output */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    int32 MinVisibleDmx = 0;

    /** Color calibrations at various temperatures */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    TArray<FRshipColorCalibration> ColorCalibrations;

    /** Actual measured white point in Kelvin (0 = not measured) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float ActualWhitePoint = 0.0f;

    /** Multiplier to adjust spec beam angle (1.0 = no adjustment) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float BeamAngleMultiplier = 1.0f;

    /** Multiplier to adjust spec field angle (1.0 = no adjustment) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float FieldAngleMultiplier = 1.0f;

    /** Beam falloff exponent (1.0 = linear, 2.0 = squared, etc.) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float FalloffExponent = 2.0f;

    /** URL to reference photo in asset store */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString ReferencePhotoUrl;

    /** Notes about this calibration */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Notes;

    /** Entity hash for optimistic locking */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Hash;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * Convert DMX value to output intensity using dimmer curve
     * @param DmxValue Input DMX value (0-255)
     * @return Output intensity (0.0 - 1.0)
     */
    float DmxToOutput(int32 DmxValue) const;

    /**
     * Get color correction for a target color temperature
     * @param TargetKelvin Desired color temperature
     * @return RGB correction multipliers
     */
    FLinearColor GetColorCorrection(float TargetKelvin) const;

    /**
     * Get calibrated beam angle from spec beam angle
     */
    float GetCalibratedBeamAngle(float SpecBeamAngle) const;

    /**
     * Get calibrated field angle from spec field angle
     */
    float GetCalibratedFieldAngle(float SpecFieldAngle) const;

    /**
     * Check if this calibration has valid dimmer curve data
     */
    bool HasDimmerCurve() const { return DimmerCurve.Num() > 0; }

    /**
     * Check if this calibration has color calibration data
     */
    bool HasColorCalibration() const { return ColorCalibrations.Num() > 0; }
};

// ============================================================================
// COLOR PROFILE TYPES (Camera Calibration)
// ============================================================================

/**
 * RGB color value
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipRGBColor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    float R = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    float G = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Calibration")
    float B = 0.0f;

    FLinearColor ToLinearColor() const
    {
        return FLinearColor(R / 255.0f, G / 255.0f, B / 255.0f);
    }
};

/**
 * White balance calibration data
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipWhiteBalanceData
{
    GENERATED_BODY()

    /** Estimated color temperature in Kelvin */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float Kelvin = 6500.0f;

    /** Green-magenta tint correction */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float Tint = 0.0f;

    /** Measured gray card values */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FRshipRGBColor MeasuredGray;

    /** Correction multipliers (R, G, B) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FRshipRGBColor Multipliers;

    /** ISO timestamp of calibration */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString CalibratedAt;

    bool IsValid() const { return !CalibratedAt.IsEmpty(); }
};

/**
 * Color checker calibration data
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipColorCheckerData
{
    GENERATED_BODY()

    /** 3x3 color correction matrix (row-major) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    TArray<float> ColorMatrix;

    /** Average Delta E (calibration quality) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float DeltaE = 100.0f;

    /** Maximum Delta E (worst-case patch) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float MaxDeltaE = 100.0f;

    /** ISO timestamp of calibration */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString CalibratedAt;

    bool IsValid() const { return ColorMatrix.Num() == 9 && !CalibratedAt.IsEmpty(); }

    /**
     * Apply color matrix to an RGB value
     */
    FLinearColor ApplyMatrix(const FLinearColor& Input) const;
};

/**
 * Recommended exposure settings
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipExposureData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    int32 ISO = 100;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString ShutterSpeed;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float Aperture = 2.8f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    float WhiteBalanceKelvin = 6500.0f;
};

/**
 * Full color profile matching ColorProfile entity
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipColorProfile
{
    GENERATED_BODY()

    /** Entity ID */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Id;

    /** Display name */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Name;

    /** Project ID (scope) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString ProjectId;

    /** Camera manufacturer */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Manufacturer;

    /** Camera model */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Model;

    /** Associated camera entity ID (optional) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString CameraId;

    /** White balance calibration data */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FRshipWhiteBalanceData WhiteBalance;

    /** Color checker calibration data */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FRshipColorCheckerData ColorChecker;

    /** Recommended exposure settings */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FRshipExposureData RecommendedExposure;

    /** Entity hash for optimistic locking */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Calibration")
    FString Hash;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * Apply full color correction pipeline to an RGB value
     * First applies white balance, then color checker matrix
     */
    FLinearColor ApplyColorCorrection(const FLinearColor& Input) const;

    /**
     * Get calibration quality rating based on Delta E
     * @return "excellent", "good", "acceptable", or "poor"
     */
    FString GetCalibrationQuality() const;

    bool HasWhiteBalance() const { return WhiteBalance.IsValid(); }
    bool HasColorChecker() const { return ColorChecker.IsValid(); }
};

// ============================================================================
// FIXTURE TYPE INFO
// ============================================================================

/**
 * Fixture type information from FixtureType entity
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureTypeInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString Manufacturer;

    /** Beam angle in degrees */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    float BeamAngle = 25.0f;

    /** Field angle in degrees */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    float FieldAngle = 35.0f;

    /** Default color temperature in Kelvin */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    float ColorTemperature = 6500.0f;

    /** Luminous output in lumens */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    int32 Lumens = 1000;

    /** URL to IES profile in asset store */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString IESProfileUrl;

    /** URL to GDTF file in asset store */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString GDTFUrl;

    /** URL to 3D geometry in asset store */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString GeometryUrl;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    bool bHasPanTilt = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    bool bHasZoom = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    bool bHasGobo = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    float MaxPan = 540.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    float MaxTilt = 270.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FVector2D ZoomRange = FVector2D(15.0f, 45.0f);
};

// ============================================================================
// FIXTURE INFO
// ============================================================================

/**
 * Fixture instance information from Fixture entity
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString FixtureTypeId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    int32 Universe = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    int32 Address = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString Mode;

    /** Emitter ID for receiving DMX state via pulses */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString EmitterId;

    /** Optional override calibration ID (if different from fixture type default) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Fixtures")
    FString CalibrationId;
};

// ============================================================================
// CAMERA INFO
// ============================================================================

/**
 * Camera calibration result (position, intrinsics, distortion)
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipCameraCalibration
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FRotator Rotation = FRotator::ZeroRotator;

    /** Focal length (fx, fy) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FVector2D FocalLength = FVector2D(1000.0f, 1000.0f);

    /** Principal point (cx, cy) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FVector2D PrincipalPoint = FVector2D(960.0f, 540.0f);

    /** Field of view in degrees */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    float FOV = 60.0f;

    /** Radial distortion coefficients (k1, k2, k3) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FVector RadialDistortion = FVector::ZeroVector;

    /** Tangential distortion coefficients (p1, p2) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FVector2D TangentialDistortion = FVector2D::ZeroVector;

    /** Mean reprojection error from calibration */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    float ReprojectionError = 0.0f;

    bool IsValid() const { return FocalLength.X > 0.0f && FocalLength.Y > 0.0f; }
};

/**
 * Camera instance information from Camera entity
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipCameraInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FRotator Rotation = FRotator::ZeroRotator;

    /** Image resolution */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FIntPoint Resolution = FIntPoint(1920, 1080);

    /** Associated color profile ID */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FString ColorProfileId;

    /** Calibration result (if calibrated) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Cameras")
    FRshipCameraCalibration Calibration;

    bool HasCalibration() const { return Calibration.IsValid(); }
};

// ============================================================================
// CALIBRATION QUALITY THRESHOLDS
// ============================================================================

namespace RshipCalibrationQuality
{
    constexpr float ExcellentMaxDeltaE = 2.0f;
    constexpr float GoodMaxDeltaE = 5.0f;
    constexpr float AcceptableMaxDeltaE = 10.0f;
    // Anything above 10 is "poor"
}
