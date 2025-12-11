// Copyright Rocketship. All Rights Reserved.

#include "RshipSpatialAudioRuntimeModule.h"

DEFINE_LOG_CATEGORY(LogRshipSpatialAudio);

#define LOCTEXT_NAMESPACE "FRshipSpatialAudioRuntimeModule"

void FRshipSpatialAudioRuntimeModule::StartupModule()
{
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("RshipSpatialAudioRuntime module starting up"));
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("  Max Speakers: %d"), SPATIAL_AUDIO_MAX_SPEAKERS);
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("  Max Objects: %d"), SPATIAL_AUDIO_MAX_OBJECTS);
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("  Max Outputs: %d"), SPATIAL_AUDIO_MAX_OUTPUTS);
}

void FRshipSpatialAudioRuntimeModule::ShutdownModule()
{
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("RshipSpatialAudioRuntime module shutting down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipSpatialAudioRuntimeModule, RshipSpatialAudioRuntime)
