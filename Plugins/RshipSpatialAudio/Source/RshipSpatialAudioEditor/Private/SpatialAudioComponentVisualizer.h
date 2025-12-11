// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class USpatialAudioVisualizerComponent;
class USpatialSpeakerComponent;
class USpatialAudioSourceComponent;
class URshipSpatialAudioManager;

/**
 * Component visualizer for the spatial audio system.
 * Draws speakers, zones, and audio objects in the editor viewport.
 */
class FSpatialAudioComponentVisualizer : public FComponentVisualizer
{
public:
	FSpatialAudioComponentVisualizer();
	virtual ~FSpatialAudioComponentVisualizer();

	// FComponentVisualizer interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

private:
	/** Draw all speakers from the manager */
	void DrawSpeakers(const USpatialAudioVisualizerComponent* Visualizer, const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/** Draw a single speaker */
	void DrawSpeaker(
		const FVector& Position,
		const FRotator& Orientation,
		float Size,
		float DispersionH,
		float DispersionV,
		const FLinearColor& Color,
		bool bShowCoverage,
		float CoverageOpacity,
		const FString& Label,
		bool bShowLabel,
		float MeterLevel,
		bool bShowMeter,
		FPrimitiveDrawInterface* PDI);

	/** Draw all zones from the manager */
	void DrawZones(const USpatialAudioVisualizerComponent* Visualizer, const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/** Draw all audio objects from the manager */
	void DrawAudioObjects(const USpatialAudioVisualizerComponent* Visualizer, const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/** Draw an audio object with spread indicator */
	void DrawAudioObject(
		const FVector& Position,
		float Spread,
		float Size,
		const FLinearColor& Color,
		const FString& Name,
		FPrimitiveDrawInterface* PDI);

	/** Draw routing lines from object to speakers */
	void DrawRoutingLines(const USpatialAudioVisualizerComponent* Visualizer, const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/** Draw a coverage cone */
	void DrawCoverageCone(
		const FVector& Position,
		const FVector& Direction,
		float HorizontalAngle,
		float VerticalAngle,
		float Length,
		const FLinearColor& Color,
		float Opacity,
		FPrimitiveDrawInterface* PDI);

	/** Draw a meter bar */
	void DrawMeterBar(
		const FVector& Position,
		float Level,
		float MaxHeight,
		float Width,
		const FVector& UpVector,
		const FVector& RightVector,
		FPrimitiveDrawInterface* PDI);
};

/**
 * Component visualizer for individual speaker components.
 */
class FSpatialSpeakerComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};

/**
 * Component visualizer for audio source components.
 */
class FSpatialAudioSourceComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
