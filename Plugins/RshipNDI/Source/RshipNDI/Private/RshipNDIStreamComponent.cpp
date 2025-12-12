// Copyright Lucid. All Rights Reserved.

#include "RshipNDIStreamComponent.h"
#include "RshipNDI.h"
#include "NDIStreamRenderer.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "CineCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"

URshipNDIStreamComponent::URshipNDIStreamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Default configuration
	Config.StreamName = TEXT("Unreal CineCamera");
	Config.Width = 7680;  // 8K
	Config.Height = 4320;
	Config.FrameRate = 60;
	Config.bEnableAlpha = true;
	Config.BufferCount = 3;
	Config.bUseAsyncReadback = true;
	Config.bAutoStartOnBeginPlay = false;
}

void URshipNDIStreamComponent::BeginPlay()
{
	Super::BeginPlay();

	// Try to find owning CineCamera
	if (!FindOwningCineCamera())
	{
		UE_LOG(LogRshipNDI, Warning, TEXT("URshipNDIStreamComponent::BeginPlay - Not attached to a CineCameraActor"));
	}

	// Auto-start if configured
	if (Config.bAutoStartOnBeginPlay)
	{
		StartStreaming();
	}
}

void URshipNDIStreamComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStreaming();
	Super::EndPlay(EndPlayReason);
}

void URshipNDIStreamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (StreamState != ERshipNDIStreamState::Streaming)
	{
		return;
	}

	// Capture frame
	CaptureFrame();

	// Process completed GPU readbacks
	ProcessReadbacks();

	// Update statistics
	UpdateStats(DeltaTime);
}

bool URshipNDIStreamComponent::FindOwningCineCamera()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// Check if owner is a CineCameraActor
	ACineCameraActor* CineActor = Cast<ACineCameraActor>(Owner);
	if (!CineActor)
	{
		UE_LOG(LogRshipNDI, Error, TEXT("URshipNDIStreamComponent - Owner is not a CineCameraActor"));
		return false;
	}

	OwningCameraActor = CineActor;

	// Get the CineCameraComponent
	UCineCameraComponent* CineComp = CineActor->GetCineCameraComponent();
	if (!CineComp)
	{
		UE_LOG(LogRshipNDI, Error, TEXT("URshipNDIStreamComponent - CineCameraActor has no CineCameraComponent"));
		return false;
	}

	CineCameraComponent = CineComp;

	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent - Found owning CineCamera: %s"),
		*CineActor->GetName());

	return true;
}

bool URshipNDIStreamComponent::StartStreaming()
{
	if (StreamState == ERshipNDIStreamState::Streaming)
	{
		UE_LOG(LogRshipNDI, Warning, TEXT("URshipNDIStreamComponent::StartStreaming - Already streaming"));
		return true;
	}

	if (!IsNDISenderAvailable())
	{
		SetError(TEXT("NDI sender library not available. Build the Rust library first."));
		return false;
	}

	SetStreamState(ERshipNDIStreamState::Starting);

	// Find camera if not already found
	if (!OwningCameraActor.IsValid() || !CineCameraComponent.IsValid())
	{
		if (!FindOwningCineCamera())
		{
			SetError(TEXT("Not attached to a CineCameraActor"));
			return false;
		}
	}

	// Log VRAM and bandwidth estimates
	int64 VRAMBytes = Config.GetVRAMUsageBytes();
	float BandwidthGBps = Config.GetBandwidthGBps();
	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent::StartStreaming - Config: %dx%d @ %d fps, VRAM: %.1f MB, Bandwidth: %.2f GB/s"),
		Config.Width, Config.Height, Config.FrameRate,
		VRAMBytes / (1024.0f * 1024.0f),
		BandwidthGBps);

	// Initialize CineCapture component
	if (!InitializeCineCapture())
	{
		SetError(TEXT("Failed to initialize CineCapture component"));
		return false;
	}

	// Initialize render targets
	if (!InitializeRenderTargets())
	{
		SetError(TEXT("Failed to initialize render targets"));
		CleanupResources();
		return false;
	}

	// Initialize NDI sender
	if (!InitializeNDISender())
	{
		SetError(TEXT("Failed to initialize NDI sender"));
		CleanupResources();
		return false;
	}

	// Enable ticking
	SetComponentTickEnabled(true);

	// Reset counters
	FrameCounter = 0;
	LastFrameTime = FPlatformTime::Seconds();
	LastReceiverCount = 0;
	Stats.Reset();

	SetStreamState(ERshipNDIStreamState::Streaming);

	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent::StartStreaming - Started streaming: %s"),
		*Config.StreamName);

	return true;
}

void URshipNDIStreamComponent::StopStreaming()
{
	if (StreamState == ERshipNDIStreamState::Stopped)
	{
		return;
	}

	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent::StopStreaming - Stopping stream: %s"),
		*Config.StreamName);

	// Disable ticking
	SetComponentTickEnabled(false);

	// Cleanup
	CleanupResources();

	SetStreamState(ERshipNDIStreamState::Stopped);
}

bool URshipNDIStreamComponent::InitializeCineCapture()
{
	if (!OwningCameraActor.IsValid())
	{
		return false;
	}

	// Create CineCaptureComponent2D - this automatically syncs with the parent CineCamera
	CineCapture = NewObject<UCineCaptureComponent2D>(OwningCameraActor.Get(), TEXT("NDICineCapture"));
	if (!CineCapture)
	{
		UE_LOG(LogRshipNDI, Error, TEXT("URshipNDIStreamComponent::InitializeCineCapture - Failed to create UCineCaptureComponent2D"));
		return false;
	}

	// Attach to the camera actor's root
	CineCapture->SetupAttachment(OwningCameraActor->GetRootComponent());
	CineCapture->RegisterComponent();

	// Configure capture settings
	CineCapture->bCaptureEveryFrame = false;  // We capture manually
	CineCapture->bCaptureOnMovement = false;
	CineCapture->bAlwaysPersistRenderingState = true;

	// Set capture source to final color
	CineCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent::InitializeCineCapture - CineCapture created and attached"));

	return true;
}

bool URshipNDIStreamComponent::InitializeRenderTargets()
{
	// Create render targets
	RenderTargets.SetNum(Config.BufferCount);

	for (int32 i = 0; i < Config.BufferCount; ++i)
	{
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
		if (!RT)
		{
			UE_LOG(LogRshipNDI, Error, TEXT("URshipNDIStreamComponent::InitializeRenderTargets - Failed to create render target %d"), i);
			return false;
		}

		// Initialize with explicit format for RGBA
		RT->RenderTargetFormat = RTF_RGBA8;
		RT->ClearColor = FLinearColor::Black;
		RT->bGPUSharedFlag = true;  // Enable GPU sharing for efficient readback
		RT->InitCustomFormat(Config.Width, Config.Height, PF_R8G8B8A8, false);
		RT->UpdateResourceImmediate();

		RenderTargets[i] = RT;

		UE_LOG(LogRshipNDI, Verbose, TEXT("URshipNDIStreamComponent::InitializeRenderTargets - Created RT[%d]: %dx%d"),
			i, Config.Width, Config.Height);
	}

	// Assign first render target to capture component
	if (CineCapture && RenderTargets.Num() > 0)
	{
		CineCapture->TextureTarget = RenderTargets[0];
	}

	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent::InitializeRenderTargets - Created %d render targets"),
		Config.BufferCount);

	return true;
}

bool URshipNDIStreamComponent::InitializeNDISender()
{
	Renderer = MakeUnique<FNDIStreamRenderer>();

	FNDIStreamRenderer::FConfig RendererConfig;
	RendererConfig.Width = Config.Width;
	RendererConfig.Height = Config.Height;
	RendererConfig.BufferCount = Config.BufferCount;
	RendererConfig.bEnableAlpha = Config.bEnableAlpha;
	RendererConfig.StreamName = Config.StreamName;
	RendererConfig.FrameRate = Config.FrameRate;

	if (!Renderer->Initialize(RendererConfig))
	{
		UE_LOG(LogRshipNDI, Error, TEXT("URshipNDIStreamComponent::InitializeNDISender - Failed to initialize renderer"));
		Renderer.Reset();
		return false;
	}

	return true;
}

void URshipNDIStreamComponent::CleanupResources()
{
	// Destroy renderer first (this will wait for GPU operations)
	if (Renderer)
	{
		Renderer->Shutdown();
		Renderer.Reset();
	}

	// Destroy CineCapture
	if (CineCapture)
	{
		CineCapture->DestroyComponent();
		CineCapture = nullptr;
	}

	// Release render targets
	for (UTextureRenderTarget2D* RT : RenderTargets)
	{
		if (RT)
		{
			RT->ReleaseResource();
		}
	}
	RenderTargets.Empty();

	CurrentBufferIndex = 0;
}

void URshipNDIStreamComponent::CaptureFrame()
{
	if (!CineCapture || RenderTargets.Num() == 0 || !Renderer)
	{
		return;
	}

	// Get the current render target
	UTextureRenderTarget2D* CurrentRT = RenderTargets[CurrentBufferIndex];
	if (!CurrentRT)
	{
		return;
	}

	// Assign render target to capture component
	CineCapture->TextureTarget = CurrentRT;

	// Trigger capture
	CineCapture->CaptureScene();

	// Submit for GPU readback
	if (Renderer->SubmitFrame(CurrentRT, FrameCounter))
	{
		++FrameCounter;
	}

	// Advance to next buffer (round-robin)
	CurrentBufferIndex = (CurrentBufferIndex + 1) % RenderTargets.Num();
}

void URshipNDIStreamComponent::ProcessReadbacks()
{
	if (Renderer)
	{
		Renderer->ProcessPendingFrames();
	}
}

void URshipNDIStreamComponent::UpdateStats(float DeltaTime)
{
	if (!Renderer)
	{
		return;
	}

	FNDIStreamRenderer::FStats RendererStats = Renderer->GetStats();

	// Update stats
	Stats.TotalFramesSent = RendererStats.FramesSent;
	Stats.DroppedFrames = RendererStats.FramesDropped;
	Stats.GPUReadbackTimeMs = RendererStats.AvgReadbackTimeMs;
	Stats.NDISendTimeMs = RendererStats.AvgSendTimeMs;
	Stats.ConnectedReceivers = RendererStats.ConnectedReceivers;
	Stats.QueueDepth = RendererStats.QueueDepth;

	// Calculate FPS
	double CurrentTime = FPlatformTime::Seconds();
	double TimeSinceLastFrame = CurrentTime - LastFrameTime;
	if (TimeSinceLastFrame > 0.0)
	{
		Stats.CurrentFPS = 1.0f / TimeSinceLastFrame;
	}
	LastFrameTime = CurrentTime;

	// Calculate bandwidth (MB/s)
	float FrameSizeMB = (Config.Width * Config.Height * 4) / (1024.0f * 1024.0f);
	Stats.BandwidthMbps = Stats.CurrentFPS * FrameSizeMB * 8.0f; // Convert to Mbps

	// Calculate average frame time
	Stats.AverageFrameTimeMs = Stats.GPUReadbackTimeMs + Stats.NDISendTimeMs;

	// Fire receiver count changed event
	if (Stats.ConnectedReceivers != LastReceiverCount)
	{
		OnReceiverCountChanged.Broadcast(Stats.ConnectedReceivers);
		LastReceiverCount = Stats.ConnectedReceivers;

		UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent - Receiver count changed: %d"),
			Stats.ConnectedReceivers);
	}
}

void URshipNDIStreamComponent::SetStreamState(ERshipNDIStreamState NewState)
{
	if (StreamState != NewState)
	{
		ERshipNDIStreamState OldState = StreamState;
		StreamState = NewState;

		UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent - State changed: %d -> %d"),
			static_cast<int32>(OldState), static_cast<int32>(NewState));

		OnStreamStateChanged.Broadcast(NewState);
	}
}

void URshipNDIStreamComponent::SetError(const FString& ErrorMessage)
{
	LastErrorMessage = ErrorMessage;
	UE_LOG(LogRshipNDI, Error, TEXT("URshipNDIStreamComponent - Error: %s"), *ErrorMessage);
	SetStreamState(ERshipNDIStreamState::Error);
}

void URshipNDIStreamComponent::SetStreamName(const FString& NewName)
{
	if (StreamState == ERshipNDIStreamState::Streaming)
	{
		UE_LOG(LogRshipNDI, Warning, TEXT("URshipNDIStreamComponent::SetStreamName - Cannot change while streaming. Stop first."));
		return;
	}
	Config.StreamName = NewName;
}

void URshipNDIStreamComponent::SetResolution(int32 NewWidth, int32 NewHeight)
{
	if (StreamState == ERshipNDIStreamState::Streaming)
	{
		UE_LOG(LogRshipNDI, Warning, TEXT("URshipNDIStreamComponent::SetResolution - Cannot change while streaming. Stop first."));
		return;
	}
	Config.Width = FMath::Clamp(NewWidth, 640, 15360);
	Config.Height = FMath::Clamp(NewHeight, 360, 8640);
}

bool URshipNDIStreamComponent::IsNDISenderAvailable()
{
#if RSHIP_HAS_NDI_SENDER
	return true;
#else
	return false;
#endif
}
