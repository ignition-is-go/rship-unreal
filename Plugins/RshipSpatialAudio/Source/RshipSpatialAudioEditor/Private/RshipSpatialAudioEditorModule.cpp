// Copyright Rocketship. All Rights Reserved.

#include "RshipSpatialAudioEditorModule.h"
#include "RshipSpatialAudioRuntimeModule.h"
#include "SpatialAudioComponentVisualizer.h"
#include "SpatialAudioVisualizerComponent.h"
#include "Components/SpatialSpeakerComponent.h"
#include "Components/SpatialAudioSourceComponent.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FRshipSpatialAudioEditorModule"

void FRshipSpatialAudioEditorModule::StartupModule()
{
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("RshipSpatialAudioEditor module starting up"));

	// Register menus after ToolMenus is ready
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRshipSpatialAudioEditorModule::RegisterMenus));

	// Register component visualizers
	if (GUnrealEd)
	{
		// Spatial Audio Visualizer Component
		TSharedPtr<FSpatialAudioComponentVisualizer> SpatialAudioVisualizer = MakeShareable(new FSpatialAudioComponentVisualizer());
		GUnrealEd->RegisterComponentVisualizer(USpatialAudioVisualizerComponent::StaticClass()->GetFName(), SpatialAudioVisualizer);
		RegisteredVisualizers.Add(SpatialAudioVisualizer);

		// Speaker Component Visualizer
		TSharedPtr<FSpatialSpeakerComponentVisualizer> SpeakerVisualizer = MakeShareable(new FSpatialSpeakerComponentVisualizer());
		GUnrealEd->RegisterComponentVisualizer(USpatialSpeakerComponent::StaticClass()->GetFName(), SpeakerVisualizer);
		RegisteredVisualizers.Add(SpeakerVisualizer);

		// Audio Source Component Visualizer
		TSharedPtr<FSpatialAudioSourceComponentVisualizer> SourceVisualizer = MakeShareable(new FSpatialAudioSourceComponentVisualizer());
		GUnrealEd->RegisterComponentVisualizer(USpatialAudioSourceComponent::StaticClass()->GetFName(), SourceVisualizer);
		RegisteredVisualizers.Add(SourceVisualizer);

		UE_LOG(LogRshipSpatialAudio, Log, TEXT("Registered spatial audio component visualizers"));
	}
}

void FRshipSpatialAudioEditorModule::ShutdownModule()
{
	UE_LOG(LogRshipSpatialAudio, Log, TEXT("RshipSpatialAudioEditor module shutting down"));

	// Unregister component visualizers
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(USpatialAudioVisualizerComponent::StaticClass()->GetFName());
		GUnrealEd->UnregisterComponentVisualizer(USpatialSpeakerComponent::StaticClass()->GetFName());
		GUnrealEd->UnregisterComponentVisualizer(USpatialAudioSourceComponent::StaticClass()->GetFName());
	}
	RegisteredVisualizers.Empty();

	UnregisterMenus();

	// Unregister ToolMenus callback
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FRshipSpatialAudioEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add menu entry under Window menu
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& Section = Menu->FindOrAddSection("Rship");
	Section.Label = LOCTEXT("RshipSectionLabel", "Rocketship");

	Section.AddMenuEntry(
		"SpatialAudioManager",
		LOCTEXT("SpatialAudioManagerLabel", "Spatial Audio Manager"),
		LOCTEXT("SpatialAudioManagerTooltip", "Open the Spatial Audio loudspeaker management panel"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]()
			{
				// TODO(M7): Spawn the speaker layout editor panel
				UE_LOG(LogRshipSpatialAudio, Log, TEXT("Spatial Audio Manager panel requested"));
			})
		)
	);
}

void FRshipSpatialAudioEditorModule::UnregisterMenus()
{
	// Cleanup will be handled by ToolMenus
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipSpatialAudioEditorModule, RshipSpatialAudioEditor)
