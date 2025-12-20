// Copyright Rocketship. All Rights Reserved.

#include "SpatialAudioVisualizerComponent.h"
#include "RshipSpatialAudioManager.h"
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
#include "RshipSubsystem.h"
#endif
#include "Engine/Engine.h"

USpatialAudioVisualizerComponent::USpatialAudioVisualizerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bIsEditorOnly = true;
}

URshipSpatialAudioManager* USpatialAudioVisualizerComponent::GetAudioManager() const
{
	if (AudioManager.IsValid())
	{
		return AudioManager.Get();
	}

	// Try to get from subsystem
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
	if (GEngine)
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			return Subsystem->GetSpatialAudioManager();
		}
	}
#endif

	return nullptr;
}

void USpatialAudioVisualizerComponent::SetAudioManager(URshipSpatialAudioManager* Manager)
{
	AudioManager = Manager;
}

void USpatialAudioVisualizerComponent::RefreshVisualization()
{
	MarkRenderStateDirty();
}
