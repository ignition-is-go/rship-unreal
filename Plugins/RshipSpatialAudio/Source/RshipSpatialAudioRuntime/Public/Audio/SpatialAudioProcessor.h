// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialAudioQueue.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialDSPTypes.h"
#include "DSP/SpatialSpeakerDSP.h"

/**
 * Per-speaker state on audio thread.
 * Contains gain, delay line, and meter accumulators.
 */
struct FSpatialSpeakerAudioState
{
	/** Current linear gain (0.0 to 1.0+) */
	float Gain = 1.0f;

	/** Target gain for smoothing */
	float TargetGain = 1.0f;

	/** Delay in samples */
	int32 DelaySamples = 0;

	/** Target delay for smoothing */
	int32 TargetDelaySamples = 0;

	/** Muted state */
	bool bMuted = false;

	/** Delay line buffer */
	TArray<float> DelayBuffer;

	/** Delay line write position */
	int32 DelayWritePos = 0;

	/** Peak meter accumulator */
	float PeakAccum = 0.0f;

	/** RMS accumulator */
	float RMSAccum = 0.0f;

	/** Sample count for RMS */
	int32 MeterSampleCount = 0;

	/** Initialize delay buffer for given max delay */
	void InitDelayBuffer(int32 MaxDelaySamples)
	{
		DelayBuffer.SetNumZeroed(MaxDelaySamples);
		DelayWritePos = 0;
	}

	/** Write sample to delay line and read delayed sample */
	float ProcessDelay(float InSample)
	{
		if (DelayBuffer.Num() == 0 || DelaySamples == 0)
		{
			return InSample;
		}

		// Write to delay buffer
		DelayBuffer[DelayWritePos] = InSample;

		// Calculate read position
		int32 ReadPos = DelayWritePos - DelaySamples;
		if (ReadPos < 0)
		{
			ReadPos += DelayBuffer.Num();
		}

		// Read delayed sample
		float OutSample = DelayBuffer[ReadPos];

		// Advance write position
		DelayWritePos = (DelayWritePos + 1) % DelayBuffer.Num();

		return OutSample;
	}

	/** Accumulate metering */
	void AccumulateMeter(float Sample)
	{
		float AbsSample = FMath::Abs(Sample);
		PeakAccum = FMath::Max(PeakAccum, AbsSample);
		RMSAccum += Sample * Sample;
		MeterSampleCount++;
	}

	/** Get and reset meter values */
	void GetAndResetMeter(float& OutPeak, float& OutRMS)
	{
		OutPeak = PeakAccum;
		OutRMS = (MeterSampleCount > 0) ? FMath::Sqrt(RMSAccum / MeterSampleCount) : 0.0f;
		PeakAccum = 0.0f;
		RMSAccum = 0.0f;
		MeterSampleCount = 0;
	}
};

/**
 * Per-object audio state on audio thread.
 */
struct FSpatialObjectAudioState
{
	/** Object identifier */
	FGuid ObjectId;

	/** Current gains per speaker */
	TStaticArray<float, SPATIAL_AUDIO_MAX_SPEAKERS> Gains;

	/** Target gains for smoothing */
	TStaticArray<float, SPATIAL_AUDIO_MAX_SPEAKERS> TargetGains;

	/** Current delays per speaker (samples) */
	TStaticArray<int32, SPATIAL_AUDIO_MAX_SPEAKERS> Delays;

	/** Target delays for smoothing */
	TStaticArray<int32, SPATIAL_AUDIO_MAX_SPEAKERS> TargetDelays;

	/** Number of active speakers for this object */
	int32 ActiveSpeakerCount = 0;

	/** Active speaker indices */
	TStaticArray<int32, SPATIAL_AUDIO_MAX_SPEAKERS_PER_OBJECT> ActiveSpeakers;

	FSpatialObjectAudioState()
	{
		for (int32 i = 0; i < SPATIAL_AUDIO_MAX_SPEAKERS; ++i)
		{
			Gains[i] = 0.0f;
			TargetGains[i] = 0.0f;
			Delays[i] = 0;
			TargetDelays[i] = 0;
		}
		ActiveSpeakerCount = 0;
	}
};

/**
 * Audio processor that runs on the audio thread.
 *
 * Responsibilities:
 * - Process commands from game thread (lock-free queue)
 * - Apply per-speaker gains and delays
 * - Mix objects to output channels
 * - Send meter feedback to game thread
 *
 * Thread Safety:
 * - All public methods are audio-thread safe
 * - Uses lock-free queues for inter-thread communication
 * - No blocking operations
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialAudioProcessor
{
public:
	FSpatialAudioProcessor();
	~FSpatialAudioProcessor();

	// Non-copyable
	FSpatialAudioProcessor(const FSpatialAudioProcessor&) = delete;
	FSpatialAudioProcessor& operator=(const FSpatialAudioProcessor&) = delete;

	// ========================================================================
	// INITIALIZATION (Game thread)
	// ========================================================================

	/**
	 * Initialize the processor with audio settings.
	 * Must be called before any processing.
	 *
	 * @param SampleRate Audio sample rate.
	 * @param BufferSize Audio buffer size in samples.
	 * @param NumOutputChannels Number of output channels.
	 */
	void Initialize(float SampleRate, int32 BufferSize, int32 NumOutputChannels);

	/**
	 * Shutdown the processor.
	 */
	void Shutdown();

	/**
	 * Check if processor is initialized.
	 */
	bool IsInitialized() const { return bIsInitialized; }

	// ========================================================================
	// COMMAND INTERFACE (Game thread)
	// ========================================================================

	/**
	 * Get the command queue for sending commands to audio thread.
	 */
	FSpatialCommandQueue& GetCommandQueue() { return CommandQueue; }

	/**
	 * Get the feedback queue for receiving feedback from audio thread.
	 */
	FSpatialFeedbackQueue& GetFeedbackQueue() { return FeedbackQueue; }

	/**
	 * Queue a position update for an object.
	 */
	void QueuePositionUpdate(const FGuid& ObjectId, const FVector& Position, float Spread);

	/**
	 * Queue computed gains for an object.
	 */
	void QueueGainsUpdate(const FGuid& ObjectId, const TArray<FSpatialSpeakerGain>& Gains);

	/**
	 * Queue a speaker DSP update.
	 */
	void QueueSpeakerDSP(int32 SpeakerIndex, float Gain, float DelayMs, bool bMuted);

	/**
	 * Queue master gain change.
	 */
	void QueueMasterGain(float Gain);

	/**
	 * Queue DSP chain enable/disable.
	 */
	void QueueEnableDSPChain(bool bEnable);

	/**
	 * Queue DSP chain bypass.
	 */
	void QueueSetDSPBypass(bool bBypass);

	// ========================================================================
	// DSP CHAIN CONFIGURATION (Game thread)
	// ========================================================================

	/**
	 * Get the DSP manager for configuring speaker DSP.
	 * @return The DSP manager, or nullptr if DSP chain is not enabled.
	 */
	FSpatialSpeakerDSPManager* GetDSPManager() { return DSPManager.Get(); }

	/**
	 * Apply full DSP configuration to a speaker.
	 * Thread-safe: can be called from game thread.
	 */
	void ApplySpeakerDSPConfig(const FGuid& SpeakerId, const FSpatialSpeakerDSPConfig& Config);

	/**
	 * Check if DSP chain is enabled.
	 */
	bool IsDSPChainEnabled() const { return bDSPChainEnabled; }

	// ========================================================================
	// PROCESSING (Audio thread)
	// ========================================================================

	/**
	 * Process pending commands from game thread.
	 * Call at start of audio callback.
	 */
	void ProcessCommands();

	/**
	 * Process a mono audio buffer for an object, outputting to all speakers.
	 *
	 * @param ObjectId The object identifier.
	 * @param InputBuffer Mono input samples.
	 * @param NumSamples Number of samples.
	 * @param OutputBuffers Array of output buffers (one per output channel).
	 */
	void ProcessObject(
		const FGuid& ObjectId,
		const float* InputBuffer,
		int32 NumSamples,
		TArray<float*>& OutputBuffers);

	/**
	 * Process accumulated output buffers through speaker DSP.
	 * Call after all objects have been processed.
	 *
	 * @param OutputBuffers Array of output buffers (one per output channel).
	 * @param NumSamples Number of samples.
	 */
	void ProcessSpeakerDSP(TArray<float*>& OutputBuffers, int32 NumSamples);

	/**
	 * Send meter feedback to game thread.
	 * Call periodically (e.g., every buffer).
	 */
	void SendMeterFeedback();

	// ========================================================================
	// ACCESSORS
	// ========================================================================

	/** Get current sample rate */
	float GetSampleRate() const { return CachedSampleRate; }

	/** Get buffer size */
	int32 GetBufferSize() const { return CachedBufferSize; }

	/** Get number of output channels */
	int32 GetNumOutputChannels() const { return NumOutputs; }

	/** Convert milliseconds to samples */
	int32 MsToSamples(float Ms) const
	{
		return FMath::RoundToInt(Ms * CachedSampleRate / 1000.0f);
	}

	/** Convert samples to milliseconds */
	float SamplesToMs(int32 Samples) const
	{
		return (Samples * 1000.0f) / CachedSampleRate;
	}

private:
	// ========================================================================
	// STATE
	// ========================================================================

	/** Is processor initialized */
	bool bIsInitialized;

	/** Cached sample rate */
	float CachedSampleRate;

	/** Cached buffer size */
	int32 CachedBufferSize;

	/** Number of output channels */
	int32 NumOutputs;

	/** Maximum delay in samples */
	int32 MaxDelaySamples;

	/** Master gain */
	float MasterGain;

	/** Target master gain for smoothing */
	float TargetMasterGain;

	/** Smoothing coefficient (per sample) */
	float SmoothingCoeff;

	/** Speaker states */
	TArray<FSpatialSpeakerAudioState> SpeakerStates;

	/** Object states (keyed by object ID hash for fast lookup) */
	TMap<FGuid, FSpatialObjectAudioState> ObjectStates;

	/** Command queue (game -> audio) */
	FSpatialCommandQueue CommandQueue;

	/** Feedback queue (audio -> game) */
	FSpatialFeedbackQueue FeedbackQueue;

	/** Meter update counter */
	int32 MeterUpdateCounter;

	/** Samples between meter updates */
	int32 SamplesPerMeterUpdate;

	// DSP Chain
	/** Full DSP chain manager */
	TUniquePtr<FSpatialSpeakerDSPManager> DSPManager;

	/** Is DSP chain enabled */
	bool bDSPChainEnabled;

	/** Is DSP chain bypassed */
	bool bDSPChainBypass;

	// ========================================================================
	// INTERNAL METHODS
	// ========================================================================

	/** Handle a single command */
	void HandleCommand(const FSpatialAudioCommandData& Cmd);

	/** Get or create object state */
	FSpatialObjectAudioState& GetOrCreateObjectState(const FGuid& ObjectId);

	/** Smooth gain towards target */
	float SmoothGain(float Current, float Target, float Coeff) const
	{
		return Current + (Target - Current) * Coeff;
	}

	/** Smooth delay towards target (integer version) */
	int32 SmoothDelay(int32 Current, int32 Target, int32 MaxStep) const
	{
		int32 Diff = Target - Current;
		if (FMath::Abs(Diff) <= MaxStep)
		{
			return Target;
		}
		return Current + FMath::Sign(Diff) * MaxStep;
	}
};
