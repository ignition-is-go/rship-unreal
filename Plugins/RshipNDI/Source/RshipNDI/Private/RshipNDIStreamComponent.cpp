// Copyright Lucid. All Rights Reserved.

#include "RshipNDIStreamComponent.h"
#include "RshipNDI.h"
#include "NDIStreamRenderer.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"

// CineCameraSceneCapture module (optional - provides exact CineCamera matching)
#if RSHIP_HAS_CINE_CAPTURE
#include "CineCaptureComponent2D.h"
#endif

URshipNDIStreamComponent::URshipNDIStreamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Default configuration
	Config.StreamName = TEXT("Unreal CineCamera");
	Config.Width = 1920;  // 1080p (start with reasonable default, can scale up)
	Config.Height = 1080;
	Config.FrameRate = 60;
	Config.bEnableAlpha = false;  // Use RGBX - SCS_FinalColorLDR outputs alpha=0
	Config.BufferCount = 3;
	Config.bUseAsyncReadback = true;
	Config.bAutoStartOnBeginPlay = false;
}

URshipNDIStreamComponent::~URshipNDIStreamComponent()
{
	// Ensure resources are cleaned up
	// TUniquePtr<FNDIStreamRenderer> will be deleted here with full type info
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
	bLoggedCameraSync = false;  // Reset so we log exposure settings for this stream session
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

#if RSHIP_HAS_CINE_CAPTURE
	// Create CineCaptureComponent2D - this automatically syncs with the parent CineCamera's
	// filmback, DOF, and lens settings for exact visual match
	UCineCaptureComponent2D* CineCapture = NewObject<UCineCaptureComponent2D>(OwningCameraActor.Get(), TEXT("NDICineCapture"));
	if (!CineCapture)
	{
		UE_LOG(LogRshipNDI, Error, TEXT("URshipNDIStreamComponent::InitializeCineCapture - Failed to create UCineCaptureComponent2D"));
		return false;
	}
	SceneCapture = CineCapture;
	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent::InitializeCineCapture - Using CineCaptureComponent2D (exact CineCamera match)"));
#else
	// Fallback to standard USceneCaptureComponent2D
	SceneCapture = NewObject<USceneCaptureComponent2D>(OwningCameraActor.Get(), TEXT("NDISceneCapture"));
	if (!SceneCapture)
	{
		UE_LOG(LogRshipNDI, Error, TEXT("URshipNDIStreamComponent::InitializeCineCapture - Failed to create USceneCaptureComponent2D"));
		return false;
	}
	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent::InitializeCineCapture - Using standard SceneCaptureComponent2D (CineCameraSceneCapture plugin not available)"));
#endif

	// Attach to the CineCameraComponent for correct position/rotation
	if (CineCameraComponent.IsValid())
	{
		SceneCapture->SetupAttachment(CineCameraComponent.Get());
	}
	else
	{
		SceneCapture->SetupAttachment(OwningCameraActor->GetRootComponent());
	}
	SceneCapture->RegisterComponent();

	// Configure capture settings
	SceneCapture->bCaptureEveryFrame = false;  // We capture manually
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->bAlwaysPersistRenderingState = true;

	// Use FinalColorLDR - this is exactly what the viewport shows
	// (after all post-processing, tone mapping, and in final display color space)
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	// Sync all camera settings for visual match (FOV, post-process, etc.)
	SyncCameraSettingsToCapture();

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

		// Initialize with sRGB format for proper gamma/color matching
		// RTF_RGBA8_SRGB ensures correct gamma curve matching the viewport
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->ClearColor = FLinearColor::Black;
		RT->bGPUSharedFlag = true;  // Enable GPU sharing for efficient readback
		RT->bAutoGenerateMips = false;
		RT->InitCustomFormat(Config.Width, Config.Height, PF_R8G8B8A8, true);  // true = sRGB gamma
		RT->UpdateResourceImmediate();

		RenderTargets[i] = RT;

		UE_LOG(LogRshipNDI, Verbose, TEXT("URshipNDIStreamComponent::InitializeRenderTargets - Created RT[%d]: %dx%d"),
			i, Config.Width, Config.Height);
	}

	// Assign first render target to capture component
	if (SceneCapture && RenderTargets.Num() > 0)
	{
		SceneCapture->TextureTarget = RenderTargets[0];
	}

	// UE 5.7+: Flush rendering commands to ensure render targets are fully initialized
	// on the GPU before we start using them. Without this, the first frames may fail
	// to capture properly due to uninitialized resources.
	FlushRenderingCommands();

	UE_LOG(LogRshipNDI, Log, TEXT("URshipNDIStreamComponent::InitializeRenderTargets - Created %d render targets"),
		Config.BufferCount);

	return true;
}

bool URshipNDIStreamComponent::InitializeNDISender()
{
	Renderer = MakeShared<FNDIStreamRenderer>();

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

	// Destroy SceneCapture
	if (SceneCapture)
	{
		SceneCapture->DestroyComponent();
		SceneCapture = nullptr;
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

void URshipNDIStreamComponent::SyncCameraSettingsToCapture()
{
	if (!SceneCapture || !CineCameraComponent.IsValid())
	{
		return;
	}

	UCineCameraComponent* CineCamera = CineCameraComponent.Get();

	// Sync FOV - computed from focal length and filmback
	SceneCapture->FOVAngle = CineCamera->FieldOfView;

	// Copy post-process settings from CineCamera for visual match
	// This includes bloom, exposure, color grading, vignette, etc.
	SceneCapture->PostProcessSettings = CineCamera->PostProcessSettings;
	SceneCapture->PostProcessBlendWeight = 1.0f;

	// Exposure handling depends on Config.bMatchViewportExposure:
	// - true: Enable eye adaptation so capture drifts WITH viewport (they match)
	// - false: Disable eye adaptation for fixed, predictable broadcast exposure

	// Ensure consistent gamma/color handling
	SceneCapture->bEnableClipPlane = false;

	// Use camera's view state (matches what the viewport sees)
	SceneCapture->bUseCustomProjectionMatrix = false;

	// Ensure we render the same primitives
	SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;

	// Constrain aspect ratio to match render target (prevents stretching/cropping)
	float TargetAspect = static_cast<float>(Config.Width) / static_cast<float>(Config.Height);
	SceneCapture->bOverride_CustomNearClippingPlane = false;

	// Match LOD rendering to viewport (prevents LOD pop differences)
	SceneCapture->LODDistanceFactor = 1.0f;

	// Show flags - match viewport rendering exactly
	// Enable all visual features for full fidelity capture
	SceneCapture->ShowFlags.SetAntiAliasing(true);
	SceneCapture->ShowFlags.SetMotionBlur(true);
	SceneCapture->ShowFlags.SetBloom(true);
	SceneCapture->ShowFlags.SetEyeAdaptation(Config.bMatchViewportExposure);  // Match viewport or use fixed exposure
	SceneCapture->ShowFlags.SetToneCurve(true);
	SceneCapture->ShowFlags.SetColorGrading(true);
	SceneCapture->ShowFlags.SetTonemapper(true);
	SceneCapture->ShowFlags.SetAtmosphere(true);
	SceneCapture->ShowFlags.SetFog(true);
	SceneCapture->ShowFlags.SetVolumetricFog(true);
	SceneCapture->ShowFlags.SetAmbientOcclusion(true);
	SceneCapture->ShowFlags.SetDynamicShadows(true);
	SceneCapture->ShowFlags.SetPostProcessing(true);
	SceneCapture->ShowFlags.SetDepthOfField(true);
	SceneCapture->ShowFlags.SetLensFlares(true);
	SceneCapture->ShowFlags.SetScreenSpaceReflections(true);
	SceneCapture->ShowFlags.SetGlobalIllumination(true);
	SceneCapture->ShowFlags.SetReflectionEnvironment(true);
	SceneCapture->ShowFlags.SetInstancedStaticMeshes(true);
	SceneCapture->ShowFlags.SetInstancedFoliage(true);
	SceneCapture->ShowFlags.SetLighting(true);
	SceneCapture->ShowFlags.SetGame(true);
	SceneCapture->ShowFlags.SetVignette(true);
	SceneCapture->ShowFlags.SetGrain(true);
	SceneCapture->ShowFlags.SetSeparateTranslucency(true);
	SceneCapture->ShowFlags.SetScreenPercentage(true);
	SceneCapture->ShowFlags.SetTemporalAA(true);
	SceneCapture->ShowFlags.SetDistanceFieldAO(true);
	SceneCapture->ShowFlags.SetVolumetricLightmap(true);
	SceneCapture->ShowFlags.SetContactShadows(true);
	SceneCapture->ShowFlags.SetCapsuleShadows(true);
	SceneCapture->ShowFlags.SetSubsurfaceScattering(true);

	// Log initial sync (use member variable to allow logging after stream restart)
	if (!bLoggedCameraSync)
	{
		UE_LOG(LogRshipNDI, Log, TEXT("SyncCameraSettingsToCapture - FOV: %.1f, PostProcess weight: %.1f, AspectRatio: %.3f, EyeAdaptation: %s"),
			SceneCapture->FOVAngle, SceneCapture->PostProcessBlendWeight, TargetAspect,
			Config.bMatchViewportExposure ? TEXT("ON (matching viewport)") : TEXT("OFF (fixed exposure)"));
		bLoggedCameraSync = true;
	}
}

void URshipNDIStreamComponent::CaptureFrame()
{
	if (!SceneCapture || RenderTargets.Num() == 0 || !Renderer)
	{
		return;
	}

	// Sync camera settings each frame (handles dynamic FOV/post-process changes)
	SyncCameraSettingsToCapture();

	// Get the current render target
	UTextureRenderTarget2D* CurrentRT = RenderTargets[CurrentBufferIndex];
	if (!CurrentRT)
	{
		return;
	}

	// Assign render target to capture component
	SceneCapture->TextureTarget = CurrentRT;

	// Trigger capture
	SceneCapture->CaptureScene();

	// Warm-up period: Skip first few frames to let the GPU pipeline fully initialize.
	// This ensures render targets are properly allocated and scene capture is stable.
	// Without this, the first frames may be black or corrupted on UE 5.7+.
	constexpr int32 WarmUpFrames = 3;
	if (FrameCounter < WarmUpFrames)
	{
		++FrameCounter;
		CurrentBufferIndex = (CurrentBufferIndex + 1) % RenderTargets.Num();
		return;
	}

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
