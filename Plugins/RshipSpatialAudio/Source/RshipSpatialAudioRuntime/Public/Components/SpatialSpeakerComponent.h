// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/SpatialSpeaker.h"
#include "Core/SpatialDSPTypes.h"
#include "SpatialSpeakerComponent.generated.h"

class URshipSpatialAudioManager;

/**
 * Component that represents a spatial audio speaker in the world.
 * When attached to an actor, it registers a speaker with the spatial audio system
 * and syncs its position with the actor.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent, DisplayName = "Spatial Audio Speaker"))
class RSHIPSPATIALAUDIORUNTIME_API USpatialSpeakerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialSpeakerComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ========================================================================
	// SPEAKER CONFIGURATION
	// ========================================================================

	/** Speaker name (defaults to actor name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Speaker")
	FString SpeakerName;

	/** Speaker type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Speaker")
	ESpatialSpeakerType SpeakerType = ESpatialSpeakerType::PointSource;

	/** Output channel number (0-based) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Speaker", meta = (ClampMin = "0", ClampMax = "255"))
	int32 OutputChannel = 0;

	/** Speaker aiming direction (relative to actor forward) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Speaker")
	FRotator AimOffset = FRotator::ZeroRotator;

	/** Horizontal coverage angle in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Speaker", meta = (ClampMin = "10.0", ClampMax = "360.0"))
	float HorizontalCoverage = 90.0f;

	/** Vertical coverage angle in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Speaker", meta = (ClampMin = "10.0", ClampMax = "180.0"))
	float VerticalCoverage = 60.0f;

	/** Auto-register on BeginPlay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Speaker")
	bool bAutoRegister = true;

	/** Sync position with actor transform changes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|Speaker")
	bool bSyncPosition = true;

	// ========================================================================
	// DSP CONFIGURATION
	// ========================================================================

	/** Initial output gain in dB */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|DSP", meta = (ClampMin = "-80.0", ClampMax = "20.0"))
	float OutputGain = 0.0f;

	/** Alignment delay in milliseconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|DSP", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float DelayMs = 0.0f;

	/** Start muted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|DSP")
	bool bStartMuted = false;

	/** Invert polarity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio|DSP")
	bool bInvertPolarity = false;

	// ========================================================================
	// RUNTIME API
	// ========================================================================

	/** Register this speaker with the spatial audio system */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void RegisterSpeaker();

	/** Unregister from the spatial audio system */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio")
	void UnregisterSpeaker();

	/** Check if currently registered */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Audio")
	bool IsRegistered() const { return SpeakerId.IsValid(); }

	/** Get the speaker ID */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Audio")
	FGuid GetSpeakerId() const { return SpeakerId; }

	// ========================================================================
	// DSP CONTROL
	// ========================================================================

	/** Set output gain in dB */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|DSP")
	void SetGain(float GainDb);

	/** Set alignment delay in ms */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|DSP")
	void SetDelay(float DelayMilliseconds);

	/** Set mute state */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|DSP")
	void SetMuted(bool bMuted);

	/** Set polarity inversion */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|DSP")
	void SetPolarity(bool bInverted);

	/** Apply full DSP state */
	UFUNCTION(BlueprintCallable, Category = "Spatial Audio|DSP")
	void SetDSPState(const FSpatialSpeakerDSPState& DSPState);

	// ========================================================================
	// METERING
	// ========================================================================

	/** Get the last meter reading for this speaker */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Audio|Metering")
	FSpatialMeterReading GetMeterReading() const;

	/** Get the current peak level in dB */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Spatial Audio|Metering")
	float GetPeakLevel() const;

private:
	/** The speaker ID assigned by the manager */
	UPROPERTY()
	FGuid SpeakerId;

	/** Cached reference to the audio manager */
	UPROPERTY()
	URshipSpatialAudioManager* AudioManager;

	/** Cached last position for change detection */
	FVector LastPosition;

	/** Cached last rotation for change detection */
	FRotator LastRotation;

	/** Get or create the audio manager reference */
	URshipSpatialAudioManager* GetAudioManager();

	/** Build speaker configuration from component properties */
	FSpatialSpeaker BuildSpeakerConfig() const;

	/** Update speaker position from actor transform */
	void UpdateSpeakerTransform();
};
