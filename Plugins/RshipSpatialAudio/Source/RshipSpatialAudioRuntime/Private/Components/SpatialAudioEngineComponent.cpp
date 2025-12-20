// Copyright Rocketship. All Rights Reserved.

#include "Components/SpatialAudioEngineComponent.h"
#include "Audio/SpatialRenderingEngine.h"
#include "Audio/SpatialAudioSubmixEffect.h"
#include "RshipSpatialAudioManager.h"
#include "RshipSpatialAudioRuntimeModule.h"
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
#include "RshipSubsystem.h"
#endif
#include "Engine/Engine.h"
#include "Engine/World.h"

USpatialAudioEngineComponent::USpatialAudioEngineComponent()
	: OutputChannelCount(64)
	, SampleRate(48000.0f)
	, BufferSize(512)
	, DefaultRendererType(ESpatialRendererType::VBAP)
	, bAutoConnectToManager(true)
	, bUse2DMode(false)
	, bIsInitialized(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

USpatialAudioEngineComponent::~USpatialAudioEngineComponent()
{
	if (bIsInitialized)
	{
		ShutdownEngine();
	}
}

void USpatialAudioEngineComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize the engine
	InitializeEngine();
}

void USpatialAudioEngineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownEngine();

	Super::EndPlay(EndPlayReason);
}

void USpatialAudioEngineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsInitialized || !RenderingEngine)
	{
		return;
	}

	// Process meter feedback from audio thread
	TMap<int32, FSpatialMeterReading> MeterReadings;
	RenderingEngine->ProcessMeterFeedback(MeterReadings);

	// Note: Meter data is also processed by the manager via the processor's feedback queue
	// This is an additional path for direct component access
}

void USpatialAudioEngineComponent::InitializeEngine()
{
	if (bIsInitialized)
	{
		UE_LOG(LogRshipSpatialAudio, Warning, TEXT("SpatialAudioEngineComponent: Already initialized"));
		return;
	}

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: Initializing (%.0f Hz, %d buffer, %d outputs)"),
		SampleRate, BufferSize, OutputChannelCount);

	// Create rendering engine
	RenderingEngine = MakeUnique<FSpatialRenderingEngine>();
	RenderingEngine->Initialize(SampleRate, BufferSize, OutputChannelCount);

	// Set 2D mode if configured
	RenderingEngine->SetUse2DMode(bUse2DMode);

	// Connect to manager if auto-connect enabled
	if (bAutoConnectToManager)
	{
		ConnectToManager();
	}

	// Connect to active submix effect
	ConnectToSubmixEffect();

	bIsInitialized = true;

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: Initialized successfully"));
}

void USpatialAudioEngineComponent::ShutdownEngine()
{
	if (!bIsInitialized)
	{
		return;
	}

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: Shutting down"));

	// Disconnect from manager
	DisconnectFromManager();

	// Disconnect from submix effect
	DisconnectFromSubmixEffect();

	// Shutdown rendering engine
	if (RenderingEngine)
	{
		RenderingEngine->Shutdown();
		RenderingEngine.Reset();
	}

	bIsInitialized = false;
}

void USpatialAudioEngineComponent::SetRendererType(ESpatialRendererType RendererType)
{
	DefaultRendererType = RendererType;

	// If connected to manager, set its renderer type
	if (ConnectedManager.IsValid())
	{
		ConnectedManager->SetGlobalRendererType(RendererType);
	}
}

void USpatialAudioEngineComponent::SetListenerPosition(FVector Position)
{
	if (RenderingEngine)
	{
		RenderingEngine->SetReferencePoint(Position);
	}

	// Also update manager
	if (ConnectedManager.IsValid())
	{
		ConnectedManager->SetListenerPosition(Position);
	}
}

void USpatialAudioEngineComponent::SetMasterGain(float GainDb)
{
	if (RenderingEngine)
	{
		RenderingEngine->SetMasterGain(GainDb);
	}
}

FString USpatialAudioEngineComponent::GetDiagnosticInfo() const
{
	FString Info = FString::Printf(TEXT("SpatialAudioEngineComponent:\n"));
	Info += FString::Printf(TEXT("  Initialized: %s\n"), bIsInitialized ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Sample Rate: %.0f\n"), SampleRate);
	Info += FString::Printf(TEXT("  Buffer Size: %d\n"), BufferSize);
	Info += FString::Printf(TEXT("  Output Channels: %d\n"), OutputChannelCount);
	Info += FString::Printf(TEXT("  Renderer: %d\n"), static_cast<int32>(DefaultRendererType));
	Info += FString::Printf(TEXT("  2D Mode: %s\n"), bUse2DMode ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Manager Connected: %s\n"), ConnectedManager.IsValid() ? TEXT("Yes") : TEXT("No"));

	if (RenderingEngine)
	{
		Info += TEXT("\nRendering Engine:\n");
		Info += RenderingEngine->GetDiagnosticInfo();
	}

	return Info;
}

void USpatialAudioEngineComponent::ConnectToSubmixEffect()
{
	// Get the active submix effect
	FSpatialAudioSubmixEffect* SubmixEffect = GetActiveSpatialAudioSubmixEffect();
	if (!SubmixEffect)
	{
		UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: No active submix effect to connect to"));
		return;
	}

	// The submix effect has its own processor
	// For now, we log the connection but the manager uses the rendering engine's processor
	// In a full implementation, we might share processors or use the submix effect's processor
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: Found active submix effect"));
}

void USpatialAudioEngineComponent::DisconnectFromSubmixEffect()
{
	// Nothing to disconnect for now
}

void USpatialAudioEngineComponent::ConnectToManager()
{
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
	if (!GEngine)
	{
		UE_LOG(LogRshipSpatialAudio, Warning, TEXT("SpatialAudioEngineComponent: GEngine not available"));
		return;
	}

	// Find the spatial audio manager via RshipSubsystem (engine subsystem)
	URshipSubsystem* RshipSubsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!RshipSubsystem)
	{
		UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: RshipSubsystem not available"));
		return;
	}

	// Get the spatial audio manager
	URshipSpatialAudioManager* Manager = RshipSubsystem->GetSpatialAudioManager();
	if (!Manager)
	{
		UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: SpatialAudioManager not available"));
		return;
	}

	// Connect the rendering engine to the manager
	Manager->SetRenderingEngine(RenderingEngine.Get());
	Manager->SetGlobalRendererType(DefaultRendererType);

	ConnectedManager = Manager;

	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: Connected to SpatialAudioManager"));
#else
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: RshipExec not available, running standalone"));
#endif
}

void USpatialAudioEngineComponent::DisconnectFromManager()
{
	if (ConnectedManager.IsValid())
	{
		// Clear the rendering engine reference in manager
		ConnectedManager->SetRenderingEngine(nullptr);
		ConnectedManager.Reset();

		UE_LOG(LogRshipSpatialAudio, Log, TEXT("SpatialAudioEngineComponent: Disconnected from SpatialAudioManager"));
	}
}
