// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpatialAudioVisualizerComponent.generated.h"

class URshipSpatialAudioManager;

/**
 * Editor-only component that visualizes the spatial audio system in the viewport.
 * Shows speakers, zones, audio objects, and their coverage patterns.
 *
 * This component is automatically added to the world when the spatial audio manager
 * is active in the editor.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent, DisplayName = "Spatial Audio Visualizer"))
class RSHIPSPATIALAUDIOEDITOR_API USpatialAudioVisualizerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialAudioVisualizerComponent();

	// ========================================================================
	// VISUALIZATION OPTIONS
	// ========================================================================

	/** Show speaker positions and coverage cones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Visualization")
	bool bShowSpeakers = true;

	/** Show zone boundaries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Visualization")
	bool bShowZones = true;

	/** Show audio object positions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Visualization")
	bool bShowAudioObjects = true;

	/** Show speaker labels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Visualization")
	bool bShowSpeakerLabels = true;

	/** Show metering on speakers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Visualization")
	bool bShowMetering = true;

	/** Show object-to-speaker routing lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Visualization")
	bool bShowRoutingLines = false;

	/** Show speaker coverage patterns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Visualization")
	bool bShowCoveragePatterns = true;

	// ========================================================================
	// APPEARANCE
	// ========================================================================

	/** Speaker visualization size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Appearance", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float SpeakerSize = 50.0f;

	/** Audio object visualization size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Appearance", meta = (ClampMin = "10.0", ClampMax = "200.0"))
	float ObjectSize = 30.0f;

	/** Coverage pattern opacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CoverageOpacity = 0.2f;

	/** Default speaker color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Appearance")
	FLinearColor SpeakerColor = FLinearColor(0.2f, 0.8f, 0.2f);

	/** Muted speaker color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Appearance")
	FLinearColor MutedSpeakerColor = FLinearColor(0.5f, 0.5f, 0.5f);

	/** Subwoofer color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Appearance")
	FLinearColor SubwooferColor = FLinearColor(0.8f, 0.4f, 0.1f);

	/** Audio object color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Appearance")
	FLinearColor ObjectColor = FLinearColor(0.3f, 0.6f, 1.0f);

	/** Zone boundary color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Appearance")
	FLinearColor ZoneColor = FLinearColor(1.0f, 1.0f, 0.0f, 0.5f);

	// ========================================================================
	// RUNTIME API
	// ========================================================================

	/** Get the audio manager being visualized */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Visualization")
	URshipSpatialAudioManager* GetAudioManager() const;

	/** Set the audio manager to visualize */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Visualization")
	void SetAudioManager(URshipSpatialAudioManager* Manager);

	/** Force a visualization refresh */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|Visualization")
	void RefreshVisualization();

private:
	/** Cached audio manager reference */
	UPROPERTY()
	TWeakObjectPtr<URshipSpatialAudioManager> AudioManager;
};
