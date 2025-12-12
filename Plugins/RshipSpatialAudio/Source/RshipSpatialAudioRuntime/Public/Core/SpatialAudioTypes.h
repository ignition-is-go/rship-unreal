// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialAudioTypes.generated.h"

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * Type of speaker in the system.
 */
UENUM(BlueprintType)
enum class ESpatialSpeakerType : uint8
{
	PointSource     UMETA(DisplayName = "Point Source"),
	LineArrayElement UMETA(DisplayName = "Line Array Element"),
	Subwoofer       UMETA(DisplayName = "Subwoofer"),
	Fill            UMETA(DisplayName = "Fill/Delay"),
	Surround        UMETA(DisplayName = "Surround"),
	Overhead        UMETA(DisplayName = "Overhead/Height"),
	Monitor         UMETA(DisplayName = "Monitor/Nearfield"),
	Custom          UMETA(DisplayName = "Custom")
};

/**
 * Type of speaker array grouping.
 */
UENUM(BlueprintType)
enum class ESpatialArrayType : uint8
{
	LineArray       UMETA(DisplayName = "Line Array (Vertical)"),
	ColumnArray     UMETA(DisplayName = "Column Array (Horizontal)"),
	Cluster         UMETA(DisplayName = "Cluster"),
	PointSource     UMETA(DisplayName = "Point Source Group"),
	SubArray        UMETA(DisplayName = "Subwoofer Array"),
	Distributed     UMETA(DisplayName = "Distributed System")
};

/**
 * Type of spatial renderer algorithm.
 */
UENUM(BlueprintType)
enum class ESpatialRendererType : uint8
{
	VBAP            UMETA(DisplayName = "VBAP (Vector Base Amplitude Panning)"),
	DBAP            UMETA(DisplayName = "DBAP (Distance-Based Amplitude Panning)"),
	HOA             UMETA(DisplayName = "HOA (Higher-Order Ambisonics)"),
	Stereo          UMETA(DisplayName = "Stereo Panning"),
	Direct          UMETA(DisplayName = "Direct Routing (No Panning)")
};

/**
 * Type of EQ band filter.
 */
UENUM(BlueprintType)
enum class ESpatialEQBandType : uint8
{
	Peak            UMETA(DisplayName = "Peak/Bell"),
	LowShelf        UMETA(DisplayName = "Low Shelf"),
	HighShelf       UMETA(DisplayName = "High Shelf"),
	Notch           UMETA(DisplayName = "Notch"),
	AllPass         UMETA(DisplayName = "All-Pass"),
	BandPass        UMETA(DisplayName = "Band-Pass")
};

/**
 * Type of filter slope.
 */
UENUM(BlueprintType)
enum class ESpatialFilterSlope : uint8
{
	Slope6dB        UMETA(DisplayName = "6 dB/oct (1st Order)"),
	Slope12dB       UMETA(DisplayName = "12 dB/oct (2nd Order)"),
	Slope18dB       UMETA(DisplayName = "18 dB/oct (3rd Order)"),
	Slope24dB       UMETA(DisplayName = "24 dB/oct (4th Order)"),
	Slope48dB       UMETA(DisplayName = "48 dB/oct (8th Order)")
};

/**
 * Type of filter (Butterworth vs Linkwitz-Riley).
 */
UENUM(BlueprintType)
enum class ESpatialFilterType : uint8
{
	Butterworth     UMETA(DisplayName = "Butterworth"),
	LinkwitzRiley   UMETA(DisplayName = "Linkwitz-Riley"),
	Bessel          UMETA(DisplayName = "Bessel")
};

/**
 * Type of bus in the routing hierarchy.
 */
UENUM(BlueprintType)
enum class ESpatialBusType : uint8
{
	Object          UMETA(DisplayName = "Object Bus"),
	Zone            UMETA(DisplayName = "Zone Bus"),
	Master          UMETA(DisplayName = "Master Bus"),
	Aux             UMETA(DisplayName = "Aux Send/Return"),
	Matrix          UMETA(DisplayName = "Matrix Output")
};

/**
 * Audio object source type.
 */
UENUM(BlueprintType)
enum class ESpatialObjectSourceType : uint8
{
	UEAudioComponent UMETA(DisplayName = "Unreal Audio Component"),
	ExternalInput   UMETA(DisplayName = "External Input Channel"),
	Oscillator      UMETA(DisplayName = "Test Oscillator"),
	Noise           UMETA(DisplayName = "Test Noise")
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

struct FSpatialSpeaker;
struct FSpatialSpeakerArray;
struct FSpatialZone;
struct FSpatialVenue;
struct FSpatialAudioObject;
struct FSpatialBus;

// ============================================================================
// UTILITY TYPES
// ============================================================================

/**
 * Computed speaker gains from renderer.
 * Contains both amplitude and phase/delay information for phase-coherent panning.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialSpeakerGain
{
	GENERATED_BODY()

	/** Speaker ID this gain applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio")
	FGuid SpeakerId;

	/** Speaker index for audio thread processing (maps to speaker array index) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio")
	int32 SpeakerIndex = -1;

	/** Linear amplitude gain (0.0 to 1.0+, typically power-normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio")
	float Gain = 0.0f;

	/** Delay in milliseconds for phase alignment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio")
	float DelayMs = 0.0f;

	/** Additional phase shift in radians (for fine phase control) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio")
	float PhaseRadians = 0.0f;

	FSpatialSpeakerGain() = default;

	FSpatialSpeakerGain(const FGuid& InSpeakerId, float InGain, float InDelayMs = 0.0f, float InPhase = 0.0f)
		: SpeakerId(InSpeakerId)
		, SpeakerIndex(-1)
		, Gain(InGain)
		, DelayMs(InDelayMs)
		, PhaseRadians(InPhase)
	{}

	FSpatialSpeakerGain(int32 InSpeakerIndex, float InGain, float InDelayMs = 0.0f, float InPhase = 0.0f)
		: SpeakerId()
		, SpeakerIndex(InSpeakerIndex)
		, Gain(InGain)
		, DelayMs(InDelayMs)
		, PhaseRadians(InPhase)
	{}
};

/**
 * Real-time meter reading for a speaker or bus.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialMeterReading
{
	GENERATED_BODY()

	/** RMS level (0.0 to 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio")
	float RMS = 0.0f;

	/** Peak level (0.0 to 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio")
	float Peak = 0.0f;

	/** Peak hold level (0.0 to 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio")
	float PeakHold = 0.0f;

	/** Whether the limiter is currently active */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio")
	bool bLimiting = false;

	/** Current gain reduction from limiter in dB (negative value) */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio")
	float GainReductionDb = 0.0f;

	/** Timestamp of this reading */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio")
	double Timestamp = 0.0;
};

/**
 * Comprehensive system status for diagnostics and UI feedback.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialAudioSystemStatus
{
	GENERATED_BODY()

	/** Is the system fully initialized and ready */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	bool bIsReady = false;

	/** Is a venue loaded */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	bool bHasVenue = false;

	/** Is the audio processor connected */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	bool bHasAudioProcessor = false;

	/** Is the rendering engine connected */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	bool bHasRenderingEngine = false;

	/** Is an external processor configured */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	bool bHasExternalProcessor = false;

	/** Is the external processor connected */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	bool bExternalProcessorConnected = false;

	/** Is rShip/Myko registered */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	bool bMykoRegistered = false;

	/** Is scene interpolation active */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	bool bSceneInterpolating = false;

	/** Number of speakers in the venue */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	int32 SpeakerCount = 0;

	/** Number of zones in the venue */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	int32 ZoneCount = 0;

	/** Number of arrays in the venue */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	int32 ArrayCount = 0;

	/** Number of audio objects */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	int32 ObjectCount = 0;

	/** Number of stored scenes */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	int32 SceneCount = 0;

	/** Currently active scene ID (empty if none) */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	FString ActiveSceneId;

	/** Current global renderer type */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	ESpatialRendererType CurrentRendererType = ESpatialRendererType::VBAP;

	/** Venue name */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	FString VenueName;

	/** Any validation warnings */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialAudio|Status")
	TArray<FString> Warnings;
};

// ============================================================================
// CONSTANTS
// ============================================================================

namespace SpatialAudioConstants
{
	/** Speed of sound in meters per second (at 20C, sea level) */
	constexpr float SpeedOfSoundMps = 343.0f;

	/** Speed of sound in centimeters per millisecond (UE units) */
	constexpr float SpeedOfSoundCmPerMs = 34.3f;

	/** Milliseconds of delay per meter of distance */
	constexpr float MsPerMeter = 2.915f;  // 1000 / 343

	/** Minimum gain threshold (below this is considered silence) */
	constexpr float MinGainThreshold = 0.0001f;  // -80dB

	/** Maximum delay in milliseconds */
	constexpr float MaxDelayMs = 1000.0f;

	/** Default sample rate */
	constexpr int32 DefaultSampleRate = 48000;

	/** Default buffer size */
	constexpr int32 DefaultBufferSize = 512;
}
