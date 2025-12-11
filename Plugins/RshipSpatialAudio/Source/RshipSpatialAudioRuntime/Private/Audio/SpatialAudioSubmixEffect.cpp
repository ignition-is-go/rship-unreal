// Copyright Rocketship. All Rights Reserved.

#include "Audio/SpatialAudioSubmixEffect.h"
#include "RshipSpatialAudioRuntimeModule.h"

// Global active effect pointer (thread-safe access via atomic)
static std::atomic<FSpatialAudioSubmixEffect*> GActiveSpatialAudioSubmixEffect(nullptr);

FSpatialAudioSubmixEffect* GetActiveSpatialAudioSubmixEffect()
{
	return GActiveSpatialAudioSubmixEffect.load(std::memory_order_acquire);
}

void RegisterActiveSpatialAudioSubmixEffect(FSpatialAudioSubmixEffect* Effect)
{
	GActiveSpatialAudioSubmixEffect.store(Effect, std::memory_order_release);
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("Registered active spatial audio submix effect"));
}

void UnregisterActiveSpatialAudioSubmixEffect(FSpatialAudioSubmixEffect* Effect)
{
	// Only unregister if we're the active one
	FSpatialAudioSubmixEffect* Expected = Effect;
	if (GActiveSpatialAudioSubmixEffect.compare_exchange_strong(Expected, nullptr, std::memory_order_release))
	{
		UE_LOG(LogRshipSpatialAudio, Log, TEXT("Unregistered active spatial audio submix effect"));
	}
}

// ============================================================================
// FSpatialAudioSubmixEffect
// ============================================================================

FSpatialAudioSubmixEffect::FSpatialAudioSubmixEffect()
	: SampleRate(48000.0f)
	, NumFramesPerBuffer(512)
	, NumInputChannels(2)
	, NumOutputChannels(64)
{
}

FSpatialAudioSubmixEffect::~FSpatialAudioSubmixEffect()
{
	UnregisterActiveSpatialAudioSubmixEffect(this);

	if (Processor)
	{
		Processor->Shutdown();
	}
}

void FSpatialAudioSubmixEffect::Init(const FSoundEffectSubmixInitData& InitData)
{
	SampleRate = InitData.SampleRate;
	NumInputChannels = InitData.NumInputChannels;
	NumFramesPerBuffer = InitData.NumFramesPerBuffer > 0 ? InitData.NumFramesPerBuffer : 512;

	// Create processor
	Processor = MakeUnique<FSpatialAudioProcessor>();

	// Apply initial settings from preset
	if (USpatialAudioSubmixEffectPreset* EffectPreset = Cast<USpatialAudioSubmixEffectPreset>(Preset.Get()))
	{
		CurrentSettings = EffectPreset->Settings;
	}

	NumOutputChannels = CurrentSettings.OutputChannelCount;

	// Initialize processor
	Processor->Initialize(SampleRate, NumFramesPerBuffer, NumOutputChannels);

	// Apply master gain
	float LinearGain = FMath::Pow(10.0f, CurrentSettings.MasterGainDb / 20.0f);
	Processor->QueueMasterGain(LinearGain);

	// Allocate output buffers
	OutputBuffers.SetNum(NumOutputChannels);
	OutputBufferPtrs.SetNum(NumOutputChannels);
	for (int32 i = 0; i < NumOutputChannels; ++i)
	{
		OutputBuffers[i].SetNumZeroed(NumFramesPerBuffer);
		OutputBufferPtrs[i] = OutputBuffers[i].GetData();
	}

	// Register as active effect
	RegisterActiveSpatialAudioSubmixEffect(this);

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioSubmixEffect initialized: %.0f Hz, %d frames, %d inputs, %d outputs"),
		SampleRate, NumFramesPerBuffer, NumInputChannels, NumOutputChannels);
}

void FSpatialAudioSubmixEffect::OnPresetChanged()
{
	if (USpatialAudioSubmixEffectPreset* EffectPreset = Cast<USpatialAudioSubmixEffectPreset>(Preset.Get()))
	{
		ApplySettings(EffectPreset->Settings);
	}
}

uint32 FSpatialAudioSubmixEffect::GetDesiredInputChannelCountOverride() const
{
	// Accept any number of input channels
	return INDEX_NONE;
}

void FSpatialAudioSubmixEffect::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	if (!Processor || !Processor->IsInitialized())
	{
		// Pass through if not initialized
		if (InData.AudioBuffer && OutData.AudioBuffer)
		{
			int32 NumSamples = FMath::Min(InData.NumFrames * InData.NumChannels,
				OutData.NumChannels * InData.NumFrames);
			FMemory::Memcpy(OutData.AudioBuffer->GetData(), InData.AudioBuffer->GetData(),
				NumSamples * sizeof(float));
		}
		return;
	}

	// Process commands from game thread
	Processor->ProcessCommands();

	// Clear output buffers
	for (int32 i = 0; i < NumOutputChannels; ++i)
	{
		FMemory::Memzero(OutputBuffers[i].GetData(), InData.NumFrames * sizeof(float));
	}

	// For now, treat input as mono or extract mono from stereo
	// In a full implementation, each audio object would be tracked separately
	// and ProcessObject would be called per-object

	// Extract mono from input (simple average of all channels)
	TArray<float> MonoBuffer;
	MonoBuffer.SetNumUninitialized(InData.NumFrames);

	const float* InputData = InData.AudioBuffer ? InData.AudioBuffer->GetData() : nullptr;
	if (InputData && InData.NumChannels > 0)
	{
		for (int32 Frame = 0; Frame < InData.NumFrames; ++Frame)
		{
			float Sum = 0.0f;
			for (int32 Ch = 0; Ch < InData.NumChannels; ++Ch)
			{
				Sum += InputData[Frame * InData.NumChannels + Ch];
			}
			MonoBuffer[Frame] = Sum / InData.NumChannels;
		}
	}

	// TODO: In the full implementation, each audio object sends audio through
	// its own submix send, and we track objects by source ID.
	// For now, we process the mono mix through a default object.

	// Process through speaker DSP (applies delays and gains)
	Processor->ProcessSpeakerDSP(OutputBufferPtrs, InData.NumFrames);

	// Copy output buffers to interleaved output
	// Note: UE submix output is interleaved, we need to interleave our per-channel buffers
	if (OutData.AudioBuffer)
	{
		float* OutPtr = OutData.AudioBuffer->GetData();
		int32 OutChannels = OutData.NumChannels;

		for (int32 Frame = 0; Frame < InData.NumFrames; ++Frame)
		{
			for (int32 Ch = 0; Ch < OutChannels; ++Ch)
			{
				if (Ch < NumOutputChannels)
				{
					OutPtr[Frame * OutChannels + Ch] = OutputBuffers[Ch][Frame];
				}
				else
				{
					OutPtr[Frame * OutChannels + Ch] = 0.0f;
				}
			}
		}
	}
}

void FSpatialAudioSubmixEffect::ApplySettings(const FSpatialAudioSubmixEffectSettings& Settings)
{
	CurrentSettings = Settings;

	if (Processor && Processor->IsInitialized())
	{
		// Apply master gain
		float LinearGain = FMath::Pow(10.0f, Settings.MasterGainDb / 20.0f);
		Processor->QueueMasterGain(LinearGain);
	}

	// Note: Changing output channel count requires re-initialization
	// which should be done through a separate API
}

// ============================================================================
// USpatialAudioSubmixEffectPreset
// ============================================================================

void USpatialAudioSubmixEffectPreset::SetMasterGain(float GainDb)
{
	Settings.MasterGainDb = FMath::Clamp(GainDb, -60.0f, 12.0f);
	Update();
}

void USpatialAudioSubmixEffectPreset::SetOutputChannelCount(int32 ChannelCount)
{
	Settings.OutputChannelCount = FMath::Clamp(ChannelCount, 2, 256);
	// Note: This requires re-initialization of the effect
	Update();
}
