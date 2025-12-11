// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialRenderer.h"

// ============================================================================
// HOA/AMBISONICS TYPES
// ============================================================================

/**
 * Ambisonics order (determines spatial resolution).
 */
enum class EAmbisonicsOrder : uint8
{
	First = 1,   // 4 channels (W, X, Y, Z)
	Second = 2,  // 9 channels
	Third = 3,   // 16 channels
	Fourth = 4,  // 25 channels
	Fifth = 5    // 36 channels
};

/**
 * Ambisonics normalization scheme.
 */
enum class EAmbisonicsNormalization : uint8
{
	SN3D,        // Schmidt semi-normalized (AmbiX standard)
	N3D,         // Full 3D normalization
	FuMa,        // Furse-Malham (legacy B-format)
	MaxN         // Max-normalized
};

/**
 * Ambisonics channel ordering.
 */
enum class EAmbisonicsChannelOrder : uint8
{
	ACN,         // Ambisonics Channel Number (AmbiX standard)
	FuMa,        // Furse-Malham ordering (legacy)
	SID          // Single Index Designation
};

/**
 * Decoder type for Ambisonics to speaker conversion.
 */
enum class EAmbisonicsDecoderType : uint8
{
	Basic,            // Simple projection (sampling)
	MaxRE,            // Max energy (improved high-frequency)
	InPhase,          // In-phase decode (reduced side lobes)
	AllRAD,           // All-Round Ambisonic Decoding (periphonic)
	EPAD              // Energy-Preserving Ambisonic Decoding
};

/**
 * Single spherical harmonic coefficient.
 */
struct FAmbisonicsCoefficient
{
	int32 Order;      // l (0, 1, 2, ...)
	int32 Degree;     // m (-l to +l)
	float Value;

	FAmbisonicsCoefficient() : Order(0), Degree(0), Value(0.0f) {}
	FAmbisonicsCoefficient(int32 l, int32 m, float v) : Order(l), Degree(m), Value(v) {}
};

/**
 * Get number of Ambisonics channels for a given order.
 * Formula: (Order + 1)^2
 */
inline int32 GetAmbisonicsChannelCount(EAmbisonicsOrder Order)
{
	int32 O = static_cast<int32>(Order);
	return (O + 1) * (O + 1);
}

/**
 * Get ACN (Ambisonics Channel Number) for a given order and degree.
 * ACN = l^2 + l + m
 */
inline int32 GetACN(int32 Order, int32 Degree)
{
	return Order * Order + Order + Degree;
}

// ============================================================================
// HOA ENCODER
// ============================================================================

/**
 * Ambisonics encoder.
 * Encodes a 3D position to spherical harmonic coefficients.
 */
class RSHIPSPATIALAUDIORUNTIME_API FAmbisonicsEncoder
{
public:
	FAmbisonicsEncoder();

	/**
	 * Set the Ambisonics order (determines spatial resolution).
	 */
	void SetOrder(EAmbisonicsOrder InOrder);
	EAmbisonicsOrder GetOrder() const { return Order; }

	/**
	 * Set normalization scheme.
	 */
	void SetNormalization(EAmbisonicsNormalization InNorm);
	EAmbisonicsNormalization GetNormalization() const { return Normalization; }

	/**
	 * Encode a direction to Ambisonics coefficients.
	 * Direction should be normalized.
	 *
	 * @param Direction Normalized direction vector.
	 * @param OutCoefficients Output array of coefficients (resized to channel count).
	 */
	void Encode(const FVector& Direction, TArray<float>& OutCoefficients) const;

	/**
	 * Encode a position with distance attenuation.
	 *
	 * @param Position World position (will be normalized for direction).
	 * @param ListenerPosition Listener/reference position.
	 * @param OutCoefficients Output array of coefficients.
	 * @param OutDistance Output distance (for attenuation).
	 */
	void EncodePosition(
		const FVector& Position,
		const FVector& ListenerPosition,
		TArray<float>& OutCoefficients,
		float& OutDistance) const;

	/**
	 * Get the number of output channels.
	 */
	int32 GetChannelCount() const { return GetAmbisonicsChannelCount(Order); }

private:
	EAmbisonicsOrder Order;
	EAmbisonicsNormalization Normalization;

	// Precomputed normalization factors
	TArray<float> NormalizationFactors;

	void ComputeNormalizationFactors();

	// Spherical harmonic computation
	static float ComputeSH(int32 l, int32 m, float Azimuth, float Elevation);
	static float AssociatedLegendre(int32 l, int32 m, float x);
	static float Factorial(int32 n);
};

// ============================================================================
// HOA DECODER
// ============================================================================

/**
 * Ambisonics decoder.
 * Decodes Ambisonics coefficients to speaker gains.
 */
class RSHIPSPATIALAUDIORUNTIME_API FAmbisonicsDecoder
{
public:
	FAmbisonicsDecoder();

	/**
	 * Configure the decoder for a speaker layout.
	 *
	 * @param Speakers Speaker positions (will compute optimal decode matrix).
	 * @param InOrder Ambisonics order to decode.
	 * @param DecoderType Decoding algorithm.
	 */
	void Configure(
		const TArray<FSpatialSpeaker>& Speakers,
		EAmbisonicsOrder InOrder,
		EAmbisonicsDecoderType DecoderType = EAmbisonicsDecoderType::AllRAD);

	/**
	 * Check if decoder is configured.
	 */
	bool IsConfigured() const { return bConfigured; }

	/**
	 * Decode Ambisonics coefficients to speaker gains.
	 *
	 * @param Coefficients Input Ambisonics coefficients.
	 * @param OutGains Output speaker gains (one per speaker).
	 */
	void Decode(const TArray<float>& Coefficients, TArray<float>& OutGains) const;

	/**
	 * Get the decode matrix (NumSpeakers x NumChannels).
	 */
	const TArray<TArray<float>>& GetDecodeMatrix() const { return DecodeMatrix; }

	/**
	 * Get speaker count.
	 */
	int32 GetSpeakerCount() const { return NumSpeakers; }

	/**
	 * Get Ambisonics channel count.
	 */
	int32 GetChannelCount() const { return NumChannels; }

private:
	bool bConfigured;
	EAmbisonicsOrder Order;
	EAmbisonicsDecoderType Type;
	int32 NumSpeakers;
	int32 NumChannels;

	// Decode matrix [speaker][channel]
	TArray<TArray<float>> DecodeMatrix;

	// Speaker directions (for decode computation)
	TArray<FVector> SpeakerDirections;

	void ComputeBasicDecodeMatrix();
	void ComputeMaxREDecodeMatrix();
	void ComputeInPhaseDecodeMatrix();
	void ComputeAllRADDecodeMatrix();

	// Matrix operations
	static void PseudoInverse(
		const TArray<TArray<float>>& A,
		TArray<TArray<float>>& OutPinv);
};

// ============================================================================
// HOA RENDERER
// ============================================================================

/**
 * Higher-Order Ambisonics spatial audio renderer.
 *
 * Implements ISpatialRenderer for HOA-based panning:
 * 1. Encodes source position to Ambisonics (spherical harmonics)
 * 2. Decodes Ambisonics to speaker gains
 *
 * Advantages:
 * - Rotation-invariant (scene can be rotated without recalculation)
 * - Scalable resolution (higher order = better localization)
 * - Works well with irregular speaker layouts
 *
 * Best for:
 * - Immersive dome/sphere installations
 * - VR/AR audio
 * - Systems where source count >> speaker count
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialRendererHOA : public ISpatialRenderer
{
public:
	FSpatialRendererHOA();
	virtual ~FSpatialRendererHOA() override;

	// ========================================================================
	// ISpatialRenderer INTERFACE
	// ========================================================================

	virtual void Configure(const TArray<FSpatialSpeaker>& Speakers) override;
	virtual bool IsConfigured() const override;
	virtual int32 GetSpeakerCount() const override;

	virtual void ComputeGains(
		const FVector& ObjectPosition,
		float Spread,
		TArray<FSpatialSpeakerGain>& OutGains) const override;

	virtual ESpatialRendererType GetType() const override { return ESpatialRendererType::HOA; }
	virtual FString GetName() const override { return TEXT("Higher-Order Ambisonics"); }
	virtual FString GetDescription() const override;
	virtual FString GetDiagnosticInfo() const override;
	virtual TArray<FString> Validate() const override;

	// ========================================================================
	// HOA-SPECIFIC CONFIGURATION
	// ========================================================================

	/**
	 * Set Ambisonics order.
	 */
	void SetOrder(EAmbisonicsOrder InOrder);
	EAmbisonicsOrder GetOrder() const { return Order; }

	/**
	 * Set decoder type.
	 */
	void SetDecoderType(EAmbisonicsDecoderType InType);
	EAmbisonicsDecoderType GetDecoderType() const { return DecoderType; }

	/**
	 * Set reference/listener position.
	 */
	void SetListenerPosition(const FVector& Position);
	FVector GetListenerPosition() const { return ListenerPosition; }

	/**
	 * Set scene rotation (useful for head tracking).
	 */
	void SetSceneRotation(const FRotator& Rotation);
	FRotator GetSceneRotation() const { return SceneRotation; }

	/**
	 * Enable/disable near-field compensation.
	 */
	void SetNearFieldCompensation(bool bEnable, float ProximityDistance = 100.0f);

	/**
	 * Configure spread handling (how source width affects encoding).
	 */
	void SetSpreadMode(bool bUseOrderReduction);

private:
	// Configuration
	EAmbisonicsOrder Order;
	EAmbisonicsDecoderType DecoderType;
	FVector ListenerPosition;
	FRotator SceneRotation;
	bool bNearFieldCompensation;
	float NearFieldDistance;
	bool bUseOrderReductionForSpread;

	// Processing components
	FAmbisonicsEncoder Encoder;
	FAmbisonicsDecoder Decoder;

	// Cached speaker info
	TArray<FSpatialSpeaker> ConfiguredSpeakers;
	TArray<FGuid> SpeakerIds;
	bool bConfigured;

	// Internal helpers
	void ReconfigureDecoder();
	float ComputeDistanceAttenuation(float Distance) const;
	void ApplySpread(TArray<float>& Coefficients, float Spread) const;
};
