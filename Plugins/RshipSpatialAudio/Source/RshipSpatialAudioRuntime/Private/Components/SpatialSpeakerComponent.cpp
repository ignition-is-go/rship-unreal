// Copyright Rocketship. All Rights Reserved.

#include "Components/SpatialSpeakerComponent.h"
#include "RshipSpatialAudioManager.h"
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
#include "RshipSubsystem.h"
#endif
#include "Engine/Engine.h"

USpatialSpeakerComponent::USpatialSpeakerComponent()
	: AudioManager(nullptr)
	, LastPosition(FVector::ZeroVector)
	, LastRotation(FRotator::ZeroRotator)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;  // Only tick if syncing position
}

void USpatialSpeakerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRegister)
	{
		RegisterSpeaker();
	}

	// Enable ticking only if position sync is needed
	PrimaryComponentTick.SetTickFunctionEnable(bSyncPosition);
}

void USpatialSpeakerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterSpeaker();
	Super::EndPlay(EndPlayReason);
}

void USpatialSpeakerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!SpeakerId.IsValid() || !bSyncPosition)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Check for transform changes
	FVector CurrentPosition = Owner->GetActorLocation();
	FRotator CurrentRotation = Owner->GetActorRotation();

	if (!CurrentPosition.Equals(LastPosition, 1.0f) || !CurrentRotation.Equals(LastRotation, 0.5f))
	{
		LastPosition = CurrentPosition;
		LastRotation = CurrentRotation;
		UpdateSpeakerTransform();
	}
}

#if WITH_EDITOR
void USpatialSpeakerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// If registered and a relevant property changed, update the speaker
	if (SpeakerId.IsValid())
	{
		URshipSpatialAudioManager* Manager = GetAudioManager();
		if (Manager)
		{
			FSpatialSpeaker Config = BuildSpeakerConfig();
			Manager->UpdateSpeaker(SpeakerId, Config);
		}
	}
}
#endif

URshipSpatialAudioManager* USpatialSpeakerComponent::GetAudioManager()
{
	if (!AudioManager)
	{
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
		if (GEngine)
		{
			if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
			{
				AudioManager = Subsystem->GetSpatialAudioManager();
			}
		}
#endif
	}
	return AudioManager;
}

FSpatialSpeaker USpatialSpeakerComponent::BuildSpeakerConfig() const
{
	FSpatialSpeaker Config;

	// Name
	Config.Name = SpeakerName;
	if (Config.Name.IsEmpty())
	{
		AActor* Owner = GetOwner();
		Config.Name = Owner ? Owner->GetName() : TEXT("UnnamedSpeaker");
	}

	// Type
	Config.Type = SpeakerType;

	// Channel
	Config.OutputChannel = OutputChannel;

	// Position from actor
	AActor* Owner = GetOwner();
	if (Owner)
	{
		Config.WorldPosition = Owner->GetActorLocation();

		// Calculate orientation
		FRotator WorldAim = Owner->GetActorRotation() + AimOffset;
		Config.Orientation = WorldAim;
	}

	// Coverage
	Config.NominalDispersionH = HorizontalCoverage;
	Config.NominalDispersionV = VerticalCoverage;

	// DSP State
	Config.DSP.OutputGainDb = OutputGain;
	Config.DSP.DelayMs = DelayMs;
	Config.DSP.bMuted = bStartMuted;
	Config.DSP.bPolarityInvert = bInvertPolarity;

	return Config;
}

void USpatialSpeakerComponent::RegisterSpeaker()
{
	if (SpeakerId.IsValid())
	{
		// Already registered
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpatialSpeakerComponent: Cannot register - SpatialAudioManager not available"));
		return;
	}

	FSpatialSpeaker Config = BuildSpeakerConfig();
	SpeakerId = Manager->AddSpeaker(Config);

	if (!SpeakerId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SpatialSpeakerComponent: Failed to register speaker"));
		return;
	}

	// Cache initial transform
	AActor* Owner = GetOwner();
	if (Owner)
	{
		LastPosition = Owner->GetActorLocation();
		LastRotation = Owner->GetActorRotation();
	}

	UE_LOG(LogTemp, Log, TEXT("SpatialSpeakerComponent: Registered speaker '%s' (ID: %s, Channel: %d)"),
		*Config.Name, *SpeakerId.ToString(), Config.OutputChannel);
}

void USpatialSpeakerComponent::UnregisterSpeaker()
{
	if (!SpeakerId.IsValid())
	{
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (Manager)
	{
		Manager->RemoveSpeaker(SpeakerId);
	}

	UE_LOG(LogTemp, Log, TEXT("SpatialSpeakerComponent: Unregistered speaker (ID: %s)"),
		*SpeakerId.ToString());

	SpeakerId.Invalidate();
}

void USpatialSpeakerComponent::UpdateSpeakerTransform()
{
	if (!SpeakerId.IsValid())
	{
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (!Manager)
	{
		return;
	}

	// Get current speaker config and update position/aim
	FSpatialSpeaker CurrentConfig;
	if (Manager->GetSpeaker(SpeakerId, CurrentConfig))
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			CurrentConfig.WorldPosition = Owner->GetActorLocation();

			FRotator WorldAim = Owner->GetActorRotation() + AimOffset;
			CurrentConfig.Orientation = WorldAim;

			Manager->UpdateSpeaker(SpeakerId, CurrentConfig);
		}
	}
}

void USpatialSpeakerComponent::SetGain(float GainDb)
{
	OutputGain = GainDb;

	if (SpeakerId.IsValid())
	{
		URshipSpatialAudioManager* Manager = GetAudioManager();
		if (Manager)
		{
			Manager->SetSpeakerGain(SpeakerId, GainDb);
		}
	}
}

void USpatialSpeakerComponent::SetDelay(float DelayMilliseconds)
{
	DelayMs = DelayMilliseconds;

	if (SpeakerId.IsValid())
	{
		URshipSpatialAudioManager* Manager = GetAudioManager();
		if (Manager)
		{
			Manager->SetSpeakerDelay(SpeakerId, DelayMilliseconds);
		}
	}
}

void USpatialSpeakerComponent::SetMuted(bool bMuted)
{
	bStartMuted = bMuted;

	if (SpeakerId.IsValid())
	{
		URshipSpatialAudioManager* Manager = GetAudioManager();
		if (Manager)
		{
			Manager->SetSpeakerMute(SpeakerId, bMuted);
		}
	}
}

void USpatialSpeakerComponent::SetPolarity(bool bInverted)
{
	bInvertPolarity = bInverted;

	if (SpeakerId.IsValid())
	{
		URshipSpatialAudioManager* Manager = GetAudioManager();
		if (Manager)
		{
			Manager->SetSpeakerPolarity(SpeakerId, bInverted);
		}
	}
}

void USpatialSpeakerComponent::SetDSPState(const FSpatialSpeakerDSPState& DSPState)
{
	// Update local properties
	OutputGain = DSPState.OutputGainDb;
	DelayMs = DSPState.DelayMs;
	bStartMuted = DSPState.bMuted;
	bInvertPolarity = DSPState.bPolarityInvert;

	if (SpeakerId.IsValid())
	{
		URshipSpatialAudioManager* Manager = GetAudioManager();
		if (Manager)
		{
			// Get current config and apply new DSP state
			FSpatialSpeaker CurrentConfig;
			if (Manager->GetSpeaker(SpeakerId, CurrentConfig))
			{
				CurrentConfig.DSP = DSPState;
				Manager->UpdateSpeaker(SpeakerId, CurrentConfig);
			}
		}
	}
}

FSpatialMeterReading USpatialSpeakerComponent::GetMeterReading() const
{
	if (!SpeakerId.IsValid() || !AudioManager)
	{
		return FSpatialMeterReading();
	}

	return AudioManager->GetSpeakerMeter(SpeakerId);
}

float USpatialSpeakerComponent::GetPeakLevel() const
{
	// Convert linear Peak to dB
	const FSpatialMeterReading& Meter = GetMeterReading();
	return Meter.Peak > SpatialAudioConstants::MinGainThreshold ? 20.0f * FMath::LogX(10.0f, Meter.Peak) : -80.0f;
}
