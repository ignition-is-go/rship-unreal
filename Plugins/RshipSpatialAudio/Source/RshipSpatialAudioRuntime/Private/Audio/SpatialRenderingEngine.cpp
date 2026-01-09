// Copyright Rocketship. All Rights Reserved.

#include "Audio/SpatialRenderingEngine.h"
#include "Audio/SpatialAudioSubmixEffect.h"
#include "Rendering/SpatialRendererVBAP.h"
#include "RshipSpatialAudioRuntimeModule.h"

// Global rendering engine instance
static TUniquePtr<FSpatialRenderingEngine> GRenderingEngine;

FSpatialRenderingEngine* GetGlobalSpatialRenderingEngine()
{
	if (!GRenderingEngine.IsValid())
	{
		GRenderingEngine = MakeUnique<FSpatialRenderingEngine>();
	}
	return GRenderingEngine.Get();
}

FSpatialRenderingEngine::FSpatialRenderingEngine()
	: bIsInitialized(false)
	, CachedSampleRate(48000.0f)
	, CurrentRendererType(ESpatialRendererType::VBAP)
	, CurrentRenderer(nullptr)
	, ReferencePoint(FVector::ZeroVector)
	, bUse2DMode(false)
{
}

FSpatialRenderingEngine::~FSpatialRenderingEngine()
{
	Shutdown();
}

void FSpatialRenderingEngine::Initialize(float SampleRate, int32 BufferSize, int32 OutputChannelCount)
{
	if (bIsInitialized)
	{
		Shutdown();
	}

	CachedSampleRate = SampleRate;

	// Initialize processor
	Processor = MakeUnique<FSpatialAudioProcessor>();
	Processor->Initialize(SampleRate, BufferSize, OutputChannelCount);

	// Configure renderer registry defaults
	RendererRegistry.SetVBAPConfig(bUse2DMode, ReferencePoint, true);

	bIsInitialized = true;

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialRenderingEngine initialized: %.0f Hz, %d samples, %d channels"),
		SampleRate, BufferSize, OutputChannelCount);
}

void FSpatialRenderingEngine::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	if (Processor)
	{
		Processor->Shutdown();
		Processor.Reset();
	}

	CurrentRenderer = nullptr;
	RendererRegistry.InvalidateCache();
	CachedSpeakers.Empty();
	SpeakerIdToIndex.Empty();

	bIsInitialized = false;

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialRenderingEngine shut down"));
}

void FSpatialRenderingEngine::ConfigureSpeakers(
	const TArray<FSpatialSpeaker>& Speakers,
	ESpatialRendererType RendererType)
{
	CachedSpeakers = Speakers;
	CurrentRendererType = RendererType;

	// Build speaker ID to index map
	SpeakerIdToIndex.Empty();
	for (int32 i = 0; i < Speakers.Num(); ++i)
	{
		SpeakerIdToIndex.Add(Speakers[i].Id, i);
	}

	// Auto-configure output routing
	OutputRouter.AutoConfigureFromSpeakers(Speakers);

	// Reconfigure renderer
	ReconfigureRenderer();

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("Configured %d speakers with %s renderer"),
		Speakers.Num(), *FSpatialRendererRegistry::GetRendererTypeName(RendererType));
}

void FSpatialRenderingEngine::SetReferencePoint(const FVector& Point)
{
	ReferencePoint = Point;

	// Update renderer if VBAP
	if (CurrentRenderer && CurrentRendererType == ESpatialRendererType::VBAP)
	{
		RendererRegistry.SetVBAPConfig(bUse2DMode, ReferencePoint, true);
		ReconfigureRenderer();
	}
}

void FSpatialRenderingEngine::SetUse2DMode(bool bIn2D)
{
	if (bUse2DMode != bIn2D)
	{
		bUse2DMode = bIn2D;
		RendererRegistry.SetVBAPConfig(bUse2DMode, ReferencePoint, true);
		ReconfigureRenderer();
	}
}

void FSpatialRenderingEngine::UpdateObject(const FSpatialAudioObject& Object)
{
	if (!bIsInitialized || !CurrentRenderer || !Processor)
	{
		return;
	}

	// Compute gains using renderer
	TArray<FSpatialSpeakerGain> Gains;
	CurrentRenderer->ComputeGains(Object.Position, Object.Spread, Gains);

	// Apply output routing trims
	for (FSpatialSpeakerGain& Gain : Gains)
	{
		// Map speaker index to output channel
		if (Gain.SpeakerIndex >= 0 && Gain.SpeakerIndex < CachedSpeakers.Num())
		{
			const FSpatialSpeaker& Speaker = CachedSpeakers[Gain.SpeakerIndex];

			// Get route trim
			float RouteTrim = OutputRouter.GetRouteTrim(Speaker.Id);
			float DelayTrim = OutputRouter.GetDelayTrim(Speaker.Id);

			Gain.Gain *= RouteTrim;
			Gain.DelayMs += DelayTrim;

			// Map to output channel
			Gain.SpeakerIndex = OutputRouter.GetOutputChannelFromIndex(Speaker.OutputChannel);
		}
	}

	// Apply object gain
	float ObjectGainLinear = DbToLinear(Object.GainDb);
	for (FSpatialSpeakerGain& Gain : Gains)
	{
		Gain.Gain *= ObjectGainLinear;
	}

	// Send to audio processor
	Processor->QueueGainsUpdate(Object.Id, Gains);
}

void FSpatialRenderingEngine::UpdateObjectsBatch(const TArray<FSpatialAudioObject>& Objects)
{
	if (!bIsInitialized || !CurrentRenderer || !Processor)
	{
		return;
	}

	// Extract positions and spreads
	TArray<FVector> Positions;
	TArray<float> Spreads;
	Positions.Reserve(Objects.Num());
	Spreads.Reserve(Objects.Num());

	for (const FSpatialAudioObject& Obj : Objects)
	{
		Positions.Add(Obj.Position);
		Spreads.Add(Obj.Spread);
	}

	// Batch compute gains
	TArray<TArray<FSpatialSpeakerGain>> GainsPerObject;
	CurrentRenderer->ComputeGainsBatch(Positions, Spreads, GainsPerObject);

	// Send to processor
	for (int32 i = 0; i < Objects.Num() && i < GainsPerObject.Num(); ++i)
	{
		TArray<FSpatialSpeakerGain>& Gains = GainsPerObject[i];
		const FSpatialAudioObject& Object = Objects[i];

		// Apply routing and object gain
		float ObjectGainLinear = DbToLinear(Object.GainDb);
		for (FSpatialSpeakerGain& Gain : Gains)
		{
			if (Gain.SpeakerIndex >= 0 && Gain.SpeakerIndex < CachedSpeakers.Num())
			{
				const FSpatialSpeaker& Speaker = CachedSpeakers[Gain.SpeakerIndex];
				Gain.Gain *= OutputRouter.GetRouteTrim(Speaker.Id) * ObjectGainLinear;
				Gain.DelayMs += OutputRouter.GetDelayTrim(Speaker.Id);
				Gain.SpeakerIndex = OutputRouter.GetOutputChannelFromIndex(Speaker.OutputChannel);
			}
		}

		Processor->QueueGainsUpdate(Object.Id, Gains);
	}
}

void FSpatialRenderingEngine::RemoveObject(const FGuid& ObjectId)
{
	if (Processor)
	{
		// Send zero gains to fade out
		TArray<FSpatialSpeakerGain> EmptyGains;
		Processor->QueueGainsUpdate(ObjectId, EmptyGains);
	}
}

void FSpatialRenderingEngine::ComputeGains(const FVector& Position, float Spread, TArray<FSpatialSpeakerGain>& OutGains)
{
	OutGains.Reset();

	if (!CurrentRenderer)
	{
		return;
	}

	CurrentRenderer->ComputeGains(Position, Spread, OutGains);
}

void FSpatialRenderingEngine::SetSpeakerDSP(int32 SpeakerIndex, float GainDb, float DelayMs, bool bMuted)
{
	if (Processor)
	{
		float LinearGain = DbToLinear(GainDb);
		Processor->QueueSpeakerDSP(SpeakerIndex, LinearGain, DelayMs, bMuted);
	}
}

void FSpatialRenderingEngine::SetMasterGain(float GainDb)
{
	if (Processor)
	{
		float LinearGain = DbToLinear(GainDb);
		Processor->QueueMasterGain(LinearGain);
	}
}

void FSpatialRenderingEngine::ProcessMeterFeedback(TMap<int32, FSpatialMeterReading>& OutMeterReadings)
{
	if (!Processor)
	{
		return;
	}

	FSpatialFeedbackQueue& FeedbackQueue = Processor->GetFeedbackQueue();
	FSpatialAudioFeedbackData Feedback;

	while (FeedbackQueue.Pop(Feedback))
	{
		if (Feedback.Type == ESpatialAudioFeedback::MeterUpdate)
		{
			FSpatialMeterReading& Reading = OutMeterReadings.FindOrAdd(Feedback.Meter.SpeakerIndex);
			Reading.Peak = Feedback.Meter.PeakLevel;
			Reading.RMS = Feedback.Meter.RMSLevel;
			Reading.Timestamp = FPlatformTime::Seconds();
		}
	}
}

FString FSpatialRenderingEngine::GetDiagnosticInfo() const
{
	FString Info;
	Info += FString::Printf(TEXT("Spatial Rendering Engine\n"));
	Info += FString::Printf(TEXT("  Initialized: %s\n"), bIsInitialized ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Sample Rate: %.0f Hz\n"), CachedSampleRate);
	Info += FString::Printf(TEXT("  Speakers: %d\n"), CachedSpeakers.Num());
	Info += FString::Printf(TEXT("  Renderer: %s\n"), *FSpatialRendererRegistry::GetRendererTypeName(CurrentRendererType));
	Info += FString::Printf(TEXT("  Mode: %s\n"), bUse2DMode ? TEXT("2D") : TEXT("3D"));
	Info += FString::Printf(TEXT("  Reference Point: (%.1f, %.1f, %.1f)\n"),
		ReferencePoint.X, ReferencePoint.Y, ReferencePoint.Z);
	Info += FString::Printf(TEXT("  Output Channels: %d\n"), OutputRouter.GetTotalOutputChannels());

	if (CurrentRenderer)
	{
		Info += TEXT("\nRenderer Info:\n");
		Info += CurrentRenderer->GetDiagnosticInfo();
	}

	if (Processor && Processor->IsInitialized())
	{
		Info += TEXT("\nProcessor Info:\n");
		Info += FString::Printf(TEXT("  Buffer Size: %d samples\n"), Processor->GetBufferSize());
		Info += FString::Printf(TEXT("  Output Channels: %d\n"), Processor->GetNumOutputChannels());
	}

	return Info;
}

void FSpatialRenderingEngine::ReconfigureRenderer()
{
	if (CachedSpeakers.Num() < 3)
	{
		CurrentRenderer = nullptr;
		UE_LOG(LogRshipSpatialAudio, Warning, TEXT("Cannot configure renderer: need at least 3 speakers"));
		return;
	}

	// Update registry configuration
	RendererRegistry.SetVBAPConfig(bUse2DMode, ReferencePoint, true);

	// Get or create renderer
	FSpatialRendererConfig Config;
	Config.RendererType = CurrentRendererType;
	Config.bPhaseCoherent = true;

	CurrentRenderer = RendererRegistry.GetOrCreateRenderer(CurrentRendererType, CachedSpeakers, Config);

	if (CurrentRenderer)
	{
		UE_LOG(LogRshipSpatialAudio, Log, TEXT("Renderer configured: %s"), *CurrentRenderer->GetDescription());

		// Validate configuration
		TArray<FString> Errors = CurrentRenderer->Validate();
		for (const FString& Error : Errors)
		{
			UE_LOG(LogRshipSpatialAudio, Warning, TEXT("Renderer validation: %s"), *Error);
		}
	}
	else
	{
		UE_LOG(LogRshipSpatialAudio, Error, TEXT("Failed to create renderer of type %s"),
			*FSpatialRendererRegistry::GetRendererTypeName(CurrentRendererType));
	}
}
