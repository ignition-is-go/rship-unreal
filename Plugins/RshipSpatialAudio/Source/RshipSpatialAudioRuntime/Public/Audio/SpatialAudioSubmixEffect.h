// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundEffectSubmix.h"
#include "SpatialAudioProcessor.h"
#include "SpatialAudioSubmixEffect.generated.h"

class USpatialAudioSubmixEffectPreset;

/**
 * Settings for the spatial audio submix effect.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialAudioSubmixEffectSettings
{
	GENERATED_BODY()

	/** Master gain in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio", meta = (ClampMin = "-60.0", ClampMax = "12.0"))
	float MasterGainDb = 0.0f;

	/** Number of output channels (should match hardware) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio", meta = (ClampMin = "2", ClampMax = "256"))
	int32 OutputChannelCount = 64;

	/** Enable metering feedback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio")
	bool bEnableMetering = true;
};

/**
 * Spatial Audio Submix Effect.
 *
 * This effect processes audio through the spatial rendering pipeline,
 * applying per-speaker gains and phase-coherent delays.
 *
 * Usage:
 * 1. Create a SpatialAudioSubmixEffectPreset asset
 * 2. Apply it to a submix that receives spatialized audio
 * 3. Configure speaker layout via SpatialAudioManager
 * 4. Audio objects route through this submix for spatial processing
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialAudioSubmixEffect : public FSoundEffectSubmix
{
public:
	FSpatialAudioSubmixEffect();
	virtual ~FSpatialAudioSubmixEffect();

	// FSoundEffectSubmix interface
	virtual void Init(const FSoundEffectSubmixInitData& InitData) override;
	virtual void OnPresetChanged() override;
	virtual uint32 GetDesiredInputChannelCountOverride() const override;

	/**
	 * Process audio buffer.
	 * Called on audio render thread.
	 */
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;

	/**
	 * Get the audio processor for external configuration.
	 */
	FSpatialAudioProcessor* GetProcessor() { return Processor.Get(); }

protected:
	/** The audio processor instance */
	TUniquePtr<FSpatialAudioProcessor> Processor;

	/** Current settings */
	FSpatialAudioSubmixEffectSettings CurrentSettings;

	/** Cached sample rate */
	float SampleRate;

	/** Cached frame count */
	int32 NumFramesPerBuffer;

	/** Number of input channels */
	int32 NumInputChannels;

	/** Number of output channels */
	int32 NumOutputChannels;

	/** Temporary output buffers */
	TArray<TArray<float>> OutputBuffers;

	/** Pointers to output buffer data */
	TArray<float*> OutputBufferPtrs;

	/** Whether the processor has been fully initialized (deferred to first OnProcessAudio) */
	bool bProcessorInitialized;

	/** Apply settings from preset */
	void ApplySettings(const FSpatialAudioSubmixEffectSettings& Settings);

	/** Initialize processor on first process call (UE 5.6+ deferred init pattern) */
	void InitializeProcessor(int32 InNumInputChannels, int32 InNumFrames);
};

/**
 * Preset asset for Spatial Audio Submix Effect.
 */
UCLASS(BlueprintType)
class RSHIPSPATIALAUDIORUNTIME_API USpatialAudioSubmixEffectPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SpatialAudioSubmixEffect)

	virtual FColor GetPresetColor() const override { return FColor(100, 200, 100); }

	/** Effect settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialAudio")
	FSpatialAudioSubmixEffectSettings Settings;

	/** Set master gain (in dB) at runtime */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio")
	void SetMasterGain(float GainDb);

	/** Set output channel count */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio")
	void SetOutputChannelCount(int32 ChannelCount);
};

/**
 * Global accessor for the active spatial audio submix effect.
 * Used by SpatialAudioManager to send gains to the audio thread.
 */
RSHIPSPATIALAUDIORUNTIME_API FSpatialAudioSubmixEffect* GetActiveSpatialAudioSubmixEffect();

/**
 * Register a spatial audio submix effect as the active one.
 * Called internally when effect is initialized.
 */
RSHIPSPATIALAUDIORUNTIME_API void RegisterActiveSpatialAudioSubmixEffect(FSpatialAudioSubmixEffect* Effect);

/**
 * Unregister the active spatial audio submix effect.
 * Called internally when effect is destroyed.
 */
RSHIPSPATIALAUDIORUNTIME_API void UnregisterActiveSpatialAudioSubmixEffect(FSpatialAudioSubmixEffect* Effect);
