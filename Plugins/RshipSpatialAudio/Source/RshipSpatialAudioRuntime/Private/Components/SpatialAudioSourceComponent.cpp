// Copyright Rocketship. All Rights Reserved.

#include "Components/SpatialAudioSourceComponent.h"
#include "RshipSpatialAudioManager.h"
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
#include "RshipSubsystem.h"
#endif
#include "Engine/Engine.h"

USpatialAudioSourceComponent::USpatialAudioSourceComponent()
	: AudioManager(nullptr)
	, LastUpdateTime(0.0f)
	, LastPosition(FVector::ZeroVector)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void USpatialAudioSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRegister)
	{
		RegisterAudioObject();
	}
}

void USpatialAudioSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterAudioObject();
	Super::EndPlay(EndPlayReason);
}

void USpatialAudioSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!AudioObjectId.IsValid())
	{
		return;
	}

	// Rate limiting
	if (UpdateRateHz > 0)
	{
		const float UpdateInterval = 1.0f / static_cast<float>(UpdateRateHz);
		LastUpdateTime += DeltaTime;
		if (LastUpdateTime < UpdateInterval)
		{
			return;
		}
		LastUpdateTime = 0.0f;
	}

	// Update position if changed
	AActor* Owner = GetOwner();
	if (Owner)
	{
		FVector CurrentPosition = Owner->GetActorLocation() + PositionOffset;
		if (!CurrentPosition.Equals(LastPosition, 1.0f))  // 1cm threshold
		{
			LastPosition = CurrentPosition;
			UpdatePosition();
		}
	}
}

URshipSpatialAudioManager* USpatialAudioSourceComponent::GetAudioManager()
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

void USpatialAudioSourceComponent::RegisterAudioObject()
{
	if (AudioObjectId.IsValid())
	{
		// Already registered
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpatialAudioSourceComponent: Cannot register - SpatialAudioManager not available"));
		return;
	}

	// Determine name
	FString ObjectName = AudioObjectName;
	if (ObjectName.IsEmpty())
	{
		AActor* Owner = GetOwner();
		ObjectName = Owner ? Owner->GetName() : TEXT("UnnamedSource");
	}

	// Create the audio object
	AudioObjectId = Manager->CreateAudioObject(ObjectName);

	if (!AudioObjectId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SpatialAudioSourceComponent: Failed to create audio object"));
		return;
	}

	// Set initial parameters
	Manager->SetObjectSpread(AudioObjectId, InitialSpread);
	Manager->SetObjectGain(AudioObjectId, InitialGain);

	// Set initial mute state if needed
	if (bStartMuted)
	{
		// Access the object directly to set mute
		FSpatialAudioObject* Object = nullptr;
		// Note: We'll need to add a mute API to the manager
	}

	// Set zone routing if specified
	if (ZoneRouting.Num() > 0)
	{
		TArray<FGuid> ZoneIds;
		for (const FString& ZoneIdStr : ZoneRouting)
		{
			FGuid ZoneId;
			if (FGuid::Parse(ZoneIdStr, ZoneId))
			{
				ZoneIds.Add(ZoneId);
			}
		}
		if (ZoneIds.Num() > 0)
		{
			Manager->SetObjectZoneRouting(AudioObjectId, ZoneIds);
		}
	}

	// Set initial position
	AActor* Owner = GetOwner();
	if (Owner)
	{
		LastPosition = Owner->GetActorLocation() + PositionOffset;
		Manager->SetObjectPosition(AudioObjectId, LastPosition);
	}

	UE_LOG(LogTemp, Log, TEXT("SpatialAudioSourceComponent: Registered audio object '%s' (ID: %s)"),
		*ObjectName, *AudioObjectId.ToString());
}

void USpatialAudioSourceComponent::UnregisterAudioObject()
{
	if (!AudioObjectId.IsValid())
	{
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (Manager)
	{
		Manager->RemoveAudioObject(AudioObjectId);
	}

	UE_LOG(LogTemp, Log, TEXT("SpatialAudioSourceComponent: Unregistered audio object (ID: %s)"),
		*AudioObjectId.ToString());

	AudioObjectId.Invalidate();
}

void USpatialAudioSourceComponent::SetSpread(float Spread)
{
	if (!AudioObjectId.IsValid())
	{
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (Manager)
	{
		Manager->SetObjectSpread(AudioObjectId, Spread);
	}
}

void USpatialAudioSourceComponent::SetGain(float GainDb)
{
	if (!AudioObjectId.IsValid())
	{
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (Manager)
	{
		Manager->SetObjectGain(AudioObjectId, GainDb);
	}
}

void USpatialAudioSourceComponent::SetMuted(bool bMuted)
{
	if (!AudioObjectId.IsValid())
	{
		return;
	}

	// This would require adding a SetObjectMuted API to the manager
	// For now, we can use gain to simulate mute
	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (Manager)
	{
		Manager->SetObjectGain(AudioObjectId, bMuted ? -80.0f : InitialGain);
	}
}

void USpatialAudioSourceComponent::SetZoneRouting(const TArray<FGuid>& ZoneIds)
{
	if (!AudioObjectId.IsValid())
	{
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (Manager)
	{
		Manager->SetObjectZoneRouting(AudioObjectId, ZoneIds);
	}
}

void USpatialAudioSourceComponent::UpdatePosition()
{
	if (!AudioObjectId.IsValid())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	URshipSpatialAudioManager* Manager = GetAudioManager();
	if (Manager)
	{
		FVector Position = Owner->GetActorLocation() + PositionOffset;
		Manager->SetObjectPosition(AudioObjectId, Position);
	}
}

FSpatialMeterReading USpatialAudioSourceComponent::GetMeterReading() const
{
	if (!AudioObjectId.IsValid())
	{
		return FSpatialMeterReading();
	}

	if (AudioManager)
	{
		return AudioManager->GetObjectMeter(AudioObjectId);
	}

	return FSpatialMeterReading();
}
