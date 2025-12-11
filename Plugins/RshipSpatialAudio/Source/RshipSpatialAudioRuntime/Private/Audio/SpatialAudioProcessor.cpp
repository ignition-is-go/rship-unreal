// Copyright Rocketship. All Rights Reserved.

#include "Audio/SpatialAudioProcessor.h"
#include "RshipSpatialAudioRuntimeModule.h"

FSpatialAudioProcessor::FSpatialAudioProcessor()
	: bIsInitialized(false)
	, CachedSampleRate(48000.0f)
	, CachedBufferSize(512)
	, NumOutputs(0)
	, MaxDelaySamples(0)
	, MasterGain(1.0f)
	, TargetMasterGain(1.0f)
	, SmoothingCoeff(0.001f)  // ~10ms smoothing at 48kHz
	, MeterUpdateCounter(0)
	, SamplesPerMeterUpdate(0)
	, bDSPChainEnabled(false)
	, bDSPChainBypass(false)
{
}

FSpatialAudioProcessor::~FSpatialAudioProcessor()
{
	Shutdown();
}

void FSpatialAudioProcessor::Initialize(float SampleRate, int32 BufferSize, int32 NumOutputChannels)
{
	if (bIsInitialized)
	{
		Shutdown();
	}

	CachedSampleRate = SampleRate;
	CachedBufferSize = BufferSize;
	NumOutputs = NumOutputChannels;

	// Calculate max delay (100ms should cover most room sizes)
	const float MaxDelayMs = 100.0f;
	MaxDelaySamples = MsToSamples(MaxDelayMs);

	// Smoothing coefficient for ~10ms time constant
	const float SmoothingTimeMs = 10.0f;
	SmoothingCoeff = 1.0f - FMath::Exp(-1.0f / (SmoothingTimeMs * SampleRate / 1000.0f));

	// Initialize speaker states
	SpeakerStates.SetNum(NumOutputChannels);
	for (FSpatialSpeakerAudioState& State : SpeakerStates)
	{
		State.InitDelayBuffer(MaxDelaySamples);
		State.Gain = 1.0f;
		State.TargetGain = 1.0f;
		State.DelaySamples = 0;
		State.TargetDelaySamples = 0;
		State.bMuted = false;
	}

	// Meter updates at ~60Hz
	SamplesPerMeterUpdate = FMath::RoundToInt(SampleRate / 60.0f);
	MeterUpdateCounter = 0;

	bIsInitialized = true;

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioProcessor initialized: %d Hz, %d samples, %d outputs"),
		FMath::RoundToInt(SampleRate), BufferSize, NumOutputChannels);
}

void FSpatialAudioProcessor::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	// Shutdown DSP manager
	if (DSPManager.IsValid())
	{
		DSPManager->Shutdown();
		DSPManager.Reset();
	}

	// Clear all state
	SpeakerStates.Empty();
	ObjectStates.Empty();

	bDSPChainEnabled = false;
	bDSPChainBypass = false;

	bIsInitialized = false;

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioProcessor shut down"));
}

void FSpatialAudioProcessor::QueuePositionUpdate(const FGuid& ObjectId, const FVector& Position, float Spread)
{
	FSpatialAudioCommandData Cmd = FSpatialAudioCommandData::MakePositionUpdate(ObjectId, Position, Spread);
	CommandQueue.PushOverwrite(Cmd);  // Position updates can overwrite old data
}

void FSpatialAudioProcessor::QueueGainsUpdate(const FGuid& ObjectId, const TArray<FSpatialSpeakerGain>& Gains)
{
	FSpatialAudioCommandData Cmd = FSpatialAudioCommandData::MakeGainsUpdate(ObjectId, Gains);
	if (!CommandQueue.Push(Cmd))
	{
		UE_LOG(LogRshipSpatialAudio, Warning, TEXT("Command queue full, dropping gains update for object %s"),
			*ObjectId.ToString());
	}
}

void FSpatialAudioProcessor::QueueSpeakerDSP(int32 SpeakerIndex, float Gain, float DelayMs, bool bMuted)
{
	FSpatialAudioCommandData Cmd = FSpatialAudioCommandData::MakeSpeakerDSP(SpeakerIndex, Gain, DelayMs, bMuted);
	CommandQueue.Push(Cmd);
}

void FSpatialAudioProcessor::QueueMasterGain(float Gain)
{
	FSpatialAudioCommandData Cmd = FSpatialAudioCommandData::MakeMasterGain(Gain);
	CommandQueue.Push(Cmd);
}

void FSpatialAudioProcessor::QueueEnableDSPChain(bool bEnable)
{
	FSpatialAudioCommandData Cmd = FSpatialAudioCommandData::MakeEnableDSPChain(bEnable);
	CommandQueue.Push(Cmd);
}

void FSpatialAudioProcessor::QueueSetDSPBypass(bool bBypass)
{
	FSpatialAudioCommandData Cmd = FSpatialAudioCommandData::MakeSetDSPBypass(bBypass);
	CommandQueue.Push(Cmd);
}

void FSpatialAudioProcessor::ApplySpeakerDSPConfig(const FGuid& SpeakerId, const FSpatialSpeakerDSPConfig& Config)
{
	if (DSPManager.IsValid())
	{
		DSPManager->ApplySpeakerConfig(SpeakerId, Config);
	}
}

void FSpatialAudioProcessor::ProcessCommands()
{
	FSpatialAudioCommandData Cmd;
	int32 CommandsProcessed = 0;
	const int32 MaxCommandsPerFrame = 256;  // Prevent starvation

	while (CommandsProcessed < MaxCommandsPerFrame && CommandQueue.Pop(Cmd))
	{
		HandleCommand(Cmd);
		CommandsProcessed++;
	}

	if (CommandsProcessed > 0)
	{
		UE_LOG(LogRshipSpatialAudio, VeryVerbose, TEXT("Processed %d audio commands"), CommandsProcessed);
	}
}

void FSpatialAudioProcessor::HandleCommand(const FSpatialAudioCommandData& Cmd)
{
	switch (Cmd.Type)
	{
	case ESpatialAudioCommand::UpdateObjectPosition:
		// Position updates are typically followed by gains updates
		// We just ensure the object exists here
		GetOrCreateObjectState(Cmd.Position.ObjectId);
		break;

	case ESpatialAudioCommand::UpdateObjectGains:
		{
			FSpatialObjectAudioState& ObjState = GetOrCreateObjectState(Cmd.Gains.ObjectId);

			// Reset all gains to zero
			for (int32 i = 0; i < SPATIAL_AUDIO_MAX_SPEAKERS; ++i)
			{
				ObjState.TargetGains[i] = 0.0f;
				ObjState.TargetDelays[i] = 0;
			}

			// Set new target gains and delays
			ObjState.ActiveSpeakerCount = 0;
			for (int32 i = 0; i < Cmd.Gains.GainCount; ++i)
			{
				const FSpatialSpeakerGain& G = Cmd.Gains.Gains[i];
				if (G.SpeakerIndex >= 0 && G.SpeakerIndex < NumOutputs)
				{
					ObjState.TargetGains[G.SpeakerIndex] = G.Gain;
					ObjState.TargetDelays[G.SpeakerIndex] = MsToSamples(G.DelayMs);

					// Track active speakers for efficient iteration
					if (ObjState.ActiveSpeakerCount < SPATIAL_AUDIO_MAX_SPEAKERS_PER_OBJECT)
					{
						ObjState.ActiveSpeakers[ObjState.ActiveSpeakerCount++] = G.SpeakerIndex;
					}
				}
			}
		}
		break;

	case ESpatialAudioCommand::UpdateSpeakerDSP:
		if (Cmd.SpeakerDSP.SpeakerIndex >= 0 && Cmd.SpeakerDSP.SpeakerIndex < SpeakerStates.Num())
		{
			FSpatialSpeakerAudioState& State = SpeakerStates[Cmd.SpeakerDSP.SpeakerIndex];
			State.TargetGain = Cmd.SpeakerDSP.Gain;
			State.TargetDelaySamples = MsToSamples(Cmd.SpeakerDSP.DelayMs);
			State.bMuted = Cmd.SpeakerDSP.bMuted;
		}
		break;

	case ESpatialAudioCommand::SetSpeakerMute:
		if (Cmd.SpeakerDSP.SpeakerIndex >= 0 && Cmd.SpeakerDSP.SpeakerIndex < SpeakerStates.Num())
		{
			SpeakerStates[Cmd.SpeakerDSP.SpeakerIndex].bMuted = Cmd.SpeakerDSP.bMuted;
		}
		break;

	case ESpatialAudioCommand::SetMasterGain:
		TargetMasterGain = Cmd.MasterGain;
		break;

	case ESpatialAudioCommand::RemoveObject:
		// Remove from map by key would need object ID stored separately
		// For now, just zero the gains (object will be cleaned up on next full update)
		break;

	case ESpatialAudioCommand::Flush:
		// Process all pending commands immediately (already doing this)
		break;

	case ESpatialAudioCommand::EnableDSPChain:
		bDSPChainEnabled = Cmd.DSPControl.bEnable;
		if (bDSPChainEnabled && !DSPManager.IsValid())
		{
			// Create DSP manager on demand
			DSPManager = MakeUnique<FSpatialSpeakerDSPManager>();
			DSPManager->Initialize(CachedSampleRate, NumOutputs);
		}
		break;

	case ESpatialAudioCommand::SetDSPBypass:
		bDSPChainBypass = Cmd.DSPControl.bBypass;
		if (DSPManager.IsValid())
		{
			DSPManager->SetGlobalBypass(bDSPChainBypass);
		}
		break;

	default:
		break;
	}
}

FSpatialObjectAudioState& FSpatialAudioProcessor::GetOrCreateObjectState(const FGuid& ObjectId)
{
	FSpatialObjectAudioState* Existing = ObjectStates.Find(ObjectId);
	if (Existing)
	{
		return *Existing;
	}

	FSpatialObjectAudioState& NewState = ObjectStates.Add(ObjectId);
	NewState.ObjectId = ObjectId;
	return NewState;
}

void FSpatialAudioProcessor::ProcessObject(
	const FGuid& ObjectId,
	const float* InputBuffer,
	int32 NumSamples,
	TArray<float*>& OutputBuffers)
{
	if (!bIsInitialized || InputBuffer == nullptr || OutputBuffers.Num() == 0)
	{
		return;
	}

	FSpatialObjectAudioState* ObjState = ObjectStates.Find(ObjectId);
	if (!ObjState || ObjState->ActiveSpeakerCount == 0)
	{
		return;
	}

	// Process each active speaker
	for (int32 i = 0; i < ObjState->ActiveSpeakerCount; ++i)
	{
		int32 SpeakerIdx = ObjState->ActiveSpeakers[i];
		if (SpeakerIdx < 0 || SpeakerIdx >= OutputBuffers.Num() || OutputBuffers[SpeakerIdx] == nullptr)
		{
			continue;
		}

		float* OutBuffer = OutputBuffers[SpeakerIdx];
		float& CurrentGain = ObjState->Gains[SpeakerIdx];
		float TargetGain = ObjState->TargetGains[SpeakerIdx];

		// TODO: Per-object delay (phase coherence per object)
		// For now, phase coherent delays are applied in speaker DSP

		for (int32 s = 0; s < NumSamples; ++s)
		{
			// Smooth gain
			CurrentGain = SmoothGain(CurrentGain, TargetGain, SmoothingCoeff);

			// Apply gain and mix to output
			OutBuffer[s] += InputBuffer[s] * CurrentGain;
		}
	}
}

void FSpatialAudioProcessor::ProcessSpeakerDSP(TArray<float*>& OutputBuffers, int32 NumSamples)
{
	if (!bIsInitialized)
	{
		return;
	}

	// Process each speaker
	for (int32 i = 0; i < SpeakerStates.Num() && i < OutputBuffers.Num(); ++i)
	{
		if (OutputBuffers[i] == nullptr)
		{
			continue;
		}

		FSpatialSpeakerAudioState& State = SpeakerStates[i];
		float* Buffer = OutputBuffers[i];

		// Use full DSP chain if enabled
		if (bDSPChainEnabled && DSPManager.IsValid() && !bDSPChainBypass)
		{
			// Process through full DSP chain
			DSPManager->ProcessSpeakerByIndex(i, Buffer, NumSamples);

			// Still need to apply master gain and accumulate metering
			for (int32 s = 0; s < NumSamples; ++s)
			{
				// Smooth master gain
				MasterGain = SmoothGain(MasterGain, TargetMasterGain, SmoothingCoeff);

				// Apply master gain
				Buffer[s] *= MasterGain;

				// Accumulate metering
				State.AccumulateMeter(Buffer[s]);
			}
		}
		else
		{
			// Use simple DSP (gain + delay only)
			for (int32 s = 0; s < NumSamples; ++s)
			{
				// Smooth master gain
				MasterGain = SmoothGain(MasterGain, TargetMasterGain, SmoothingCoeff);

				// Smooth speaker gain
				State.Gain = SmoothGain(State.Gain, State.TargetGain, SmoothingCoeff);

				// Smooth delay (1 sample per frame max change to avoid clicks)
				State.DelaySamples = SmoothDelay(State.DelaySamples, State.TargetDelaySamples, 1);

				// Get input sample
				float Sample = Buffer[s];

				// Apply speaker delay (phase coherence)
				Sample = State.ProcessDelay(Sample);

				// Apply speaker gain and master gain
				float FinalGain = State.bMuted ? 0.0f : (State.Gain * MasterGain);
				Sample *= FinalGain;

				// Write output
				Buffer[s] = Sample;

				// Accumulate metering
				State.AccumulateMeter(Sample);
			}
		}
	}

	// Update meter counter and send feedback if needed
	MeterUpdateCounter += NumSamples;
	if (MeterUpdateCounter >= SamplesPerMeterUpdate)
	{
		SendMeterFeedback();
		MeterUpdateCounter = 0;
	}
}

void FSpatialAudioProcessor::SendMeterFeedback()
{
	for (int32 i = 0; i < SpeakerStates.Num(); ++i)
	{
		FSpatialSpeakerAudioState& State = SpeakerStates[i];

		float Peak, RMS;
		State.GetAndResetMeter(Peak, RMS);

		// Only send if there's activity
		if (Peak > 0.0001f)
		{
			FSpatialAudioFeedbackData Feedback;
			Feedback.Type = ESpatialAudioFeedback::MeterUpdate;
			Feedback.Meter.SpeakerIndex = i;
			Feedback.Meter.PeakLevel = Peak;
			Feedback.Meter.RMSLevel = RMS;

			// Don't block if queue is full
			FeedbackQueue.Push(Feedback);
		}

		// Send limiter gain reduction if DSP chain is active
		if (bDSPChainEnabled && DSPManager.IsValid())
		{
			if (FSpatialSpeakerDSP* DSP = DSPManager->GetSpeakerDSPByIndex(i))
			{
				float GRDb = DSP->GetLimiterGainReductionDb();
				if (GRDb < -0.1f)  // Only send if limiting is active
				{
					FSpatialAudioFeedbackData GRFeedback;
					GRFeedback.Type = ESpatialAudioFeedback::LimiterGRUpdate;
					GRFeedback.LimiterGR.SpeakerIndex = i;
					GRFeedback.LimiterGR.GainReductionDb = GRDb;
					FeedbackQueue.Push(GRFeedback);
				}
			}
		}
	}
}
