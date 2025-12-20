// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"
#include "Core/SpatialZone.h"
#include "Core/SpatialVenue.h"
#include "Core/SpatialAudioObject.h"
#include "Core/SpatialDSPTypes.h"
#include "ExternalProcessor/ExternalProcessorTypes.h"
#include "RshipSpatialAudioManager.generated.h"

// Forward declarations
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
class URshipSubsystem;
#endif
class FSpatialAudioProcessor;
class FSpatialRenderingEngine;
class IExternalSpatialProcessor;
class UExternalProcessorRegistry;
struct FSpatialSpeakerDSPConfig;

DECLARE_LOG_CATEGORY_EXTERN(LogRshipSpatialAudioManager, Log, All);

// Delegates for event notifications
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeakerAdded, const FGuid&, SpeakerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeakerRemoved, const FGuid&, SpeakerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeakerUpdated, const FGuid&, SpeakerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZoneAdded, const FGuid&, ZoneId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZoneRemoved, const FGuid&, ZoneId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectAdded, const FGuid&, ObjectId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectRemoved, const FGuid&, ObjectId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectPositionChanged, const FGuid&, ObjectId, const FVector&, NewPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVenueChanged);

/**
 * Main manager for the Rship Spatial Audio system.
 * Handles venue configuration, audio objects, rendering, and rShip/Myko integration.
 *
 * This manager follows the same lazy-initialization pattern as other Rship managers.
 */
UCLASS(BlueprintType)
class RSHIPSPATIALAUDIORUNTIME_API URshipSpatialAudioManager : public UObject
{
	GENERATED_BODY()

public:
	URshipSpatialAudioManager();

#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
	/**
	 * Initialize the manager with the parent subsystem.
	 * This is exposed as a UFUNCTION to enable reflection-based initialization
	 * from RshipExec (which has an optional dependency on this plugin).
	 * @param InSubsystem The RshipSubsystem that owns this manager.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Internal")
	void Initialize(URshipSubsystem* InSubsystem);
#else
	/**
	 * Initialize the manager standalone (without rShip/Myko integration).
	 */
	void Initialize();
#endif

	/**
	 * Shutdown and cleanup the manager.
	 */
	void Shutdown();

	/**
	 * Tick the manager (called from subsystem tick).
	 * @param DeltaTime Time since last tick.
	 */
	void Tick(float DeltaTime);

	// ========================================================================
	// VENUE MANAGEMENT
	// ========================================================================

	/**
	 * Create or reset the venue with a new configuration.
	 * @param VenueName Name for the venue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Venue")
	void CreateVenue(const FString& VenueName);

	/**
	 * Get the current venue configuration (mutable).
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Venue")
	FSpatialVenue& GetVenue() { return Venue; }

	/**
	 * Get the current venue configuration (const).
	 */
	const FSpatialVenue& GetVenueConst() const { return Venue; }

	/**
	 * Check if a venue is loaded.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Venue")
	bool HasVenue() const { return Venue.Id.IsValid(); }

	// ========================================================================
	// SPEAKER MANAGEMENT
	// ========================================================================

	/**
	 * Add a speaker to the venue.
	 * @param Speaker Speaker configuration.
	 * @return The ID of the added speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers")
	FGuid AddSpeaker(const FSpatialSpeaker& Speaker);

	/**
	 * Update an existing speaker.
	 * @param SpeakerId ID of the speaker to update.
	 * @param Speaker New speaker configuration.
	 * @return True if the speaker was found and updated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers")
	bool UpdateSpeaker(const FGuid& SpeakerId, const FSpatialSpeaker& Speaker);

	/**
	 * Remove a speaker from the venue.
	 * @param SpeakerId ID of the speaker to remove.
	 * @return True if the speaker was found and removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers")
	bool RemoveSpeaker(const FGuid& SpeakerId);

	/**
	 * Get a speaker by ID.
	 * @param SpeakerId ID of the speaker.
	 * @param OutSpeaker Output speaker configuration.
	 * @return True if the speaker was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers")
	bool GetSpeaker(const FGuid& SpeakerId, FSpatialSpeaker& OutSpeaker) const;

	/**
	 * Get all speakers in the venue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers")
	TArray<FSpatialSpeaker> GetAllSpeakers() const;

	/**
	 * Get speaker count.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Speakers")
	int32 GetSpeakerCount() const { return Venue.GetSpeakerCount(); }

	// ========================================================================
	// SPEAKER DSP CONTROL
	// ========================================================================

	/**
	 * Set gain for a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerGain(const FGuid& SpeakerId, float GainDb);

	/**
	 * Set delay for a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerDelay(const FGuid& SpeakerId, float DelayMs);

	/**
	 * Set mute state for a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerMute(const FGuid& SpeakerId, bool bMuted);

	/**
	 * Set polarity for a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerPolarity(const FGuid& SpeakerId, bool bInverted);

	/**
	 * Set EQ for a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerEQ(const FGuid& SpeakerId, const TArray<FSpatialEQBand>& Bands);

	/**
	 * Set limiter settings for a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerLimiter(const FGuid& SpeakerId, const FSpatialLimiterSettings& Settings);

	/**
	 * Set complete DSP state for a speaker from a calibration preset.
	 * This is the primary entry point for applying calibration data.
	 * @param SpeakerId ID of the speaker.
	 * @param DSPState Complete DSP state from calibration preset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerDSP(const FGuid& SpeakerId, const FSpatialSpeakerDSPState& DSPState);

	/**
	 * Apply high-pass filter to a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerHighPass(const FGuid& SpeakerId, const FSpatialHighPassFilter& HighPass);

	/**
	 * Apply low-pass filter to a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP")
	void SetSpeakerLowPass(const FGuid& SpeakerId, const FSpatialLowPassFilter& LowPass);

	// ========================================================================
	// ZONE MANAGEMENT
	// ========================================================================

	/**
	 * Add a zone to the venue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones")
	FGuid AddZone(const FSpatialZone& Zone);

	/**
	 * Update an existing zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones")
	bool UpdateZone(const FGuid& ZoneId, const FSpatialZone& Zone);

	/**
	 * Remove a zone from the venue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones")
	bool RemoveZone(const FGuid& ZoneId);

	/**
	 * Set the renderer type for a zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones")
	void SetZoneRenderer(const FGuid& ZoneId, ESpatialRendererType RendererType);

	/**
	 * Get zone count.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Zones")
	int32 GetZoneCount() const { return Venue.GetZoneCount(); }

	// ========================================================================
	// AUDIO OBJECT MANAGEMENT
	// ========================================================================

	/**
	 * Create a new audio object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects")
	FGuid CreateAudioObject(const FString& Name);

	/**
	 * Remove an audio object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects")
	bool RemoveAudioObject(const FGuid& ObjectId);

	/**
	 * Set the position of an audio object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects")
	void SetObjectPosition(const FGuid& ObjectId, FVector Position);

	/**
	 * Set the spread of an audio object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects")
	void SetObjectSpread(const FGuid& ObjectId, float Spread);

	/**
	 * Set the gain of an audio object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects")
	void SetObjectGain(const FGuid& ObjectId, float GainDb);

	/**
	 * Set zone routing for an audio object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects")
	void SetObjectZoneRouting(const FGuid& ObjectId, const TArray<FGuid>& ZoneIds);

	/**
	 * Get audio object count.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Objects")
	int32 GetAudioObjectCount() const { return AudioObjects.Num(); }

	/**
	 * Get all audio objects.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects")
	TArray<FSpatialAudioObject> GetAllAudioObjects() const;

	/**
	 * Get an audio object by ID.
	 * @param ObjectId ID of the audio object.
	 * @param OutObject Output object data.
	 * @return True if the object was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (ToolTip = "Get an audio object by its unique ID"))
	bool GetAudioObject(const FGuid& ObjectId, FSpatialAudioObject& OutObject) const;

	/**
	 * Get an audio object by name.
	 * @param Name Name of the audio object.
	 * @param OutObject Output object data.
	 * @return True if an object with that name was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (ToolTip = "Find an audio object by its display name"))
	bool GetAudioObjectByName(const FString& Name, FSpatialAudioObject& OutObject) const;

	/**
	 * Get the current position of an audio object.
	 * Convenience function to avoid retrieving entire object.
	 * @param ObjectId ID of the audio object.
	 * @param OutPosition Output position.
	 * @return True if the object was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (ToolTip = "Get just the position of an audio object"))
	bool GetObjectPosition(const FGuid& ObjectId, FVector& OutPosition) const;

	/**
	 * Check if an audio object is currently active (has non-zero gain and routing).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Objects", meta = (ToolTip = "Check if object is audible"))
	bool IsObjectActive(const FGuid& ObjectId) const;

	/**
	 * Add an existing audio object to the manager (for component registration).
	 * @param Object The audio object to add.
	 * @return The ID of the added object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects")
	FGuid AddObject(const FSpatialAudioObject& Object);

	// ========================================================================
	// ZONE QUERY & CONVENIENCE
	// ========================================================================

	/**
	 * Get a zone by ID.
	 * @param ZoneId ID of the zone.
	 * @param OutZone Output zone data.
	 * @return True if the zone was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones", meta = (ToolTip = "Get a zone by its unique ID"))
	bool GetZone(const FGuid& ZoneId, FSpatialZone& OutZone) const;

	/**
	 * Get all zones in the venue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones")
	TArray<FSpatialZone> GetAllZones() const;

	/**
	 * Get all speakers belonging to a zone.
	 * @param ZoneId ID of the zone.
	 * @return Array of speakers in the zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones", meta = (ToolTip = "Get all speakers assigned to a zone"))
	TArray<FSpatialSpeaker> GetSpeakersByZone(const FGuid& ZoneId) const;

	/**
	 * Get all audio objects routed to a zone.
	 * @param ZoneId ID of the zone.
	 * @return Array of objects routed to the zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones", meta = (ToolTip = "Get all audio objects routed to a zone"))
	TArray<FSpatialAudioObject> GetObjectsByZone(const FGuid& ZoneId) const;

	/**
	 * Get the renderer type for a zone.
	 * @param ZoneId ID of the zone.
	 * @return The renderer type, or VBAP if zone not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Zones")
	ESpatialRendererType GetZoneRenderer(const FGuid& ZoneId) const;

	// ========================================================================
	// ARRAY QUERY
	// ========================================================================

	/**
	 * Get a speaker array by ID.
	 * @param ArrayId ID of the array.
	 * @param OutArray Output array data.
	 * @return True if the array was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Arrays", meta = (ToolTip = "Get a speaker array by its unique ID"))
	bool GetArray(const FGuid& ArrayId, FSpatialSpeakerArray& OutArray) const;

	/**
	 * Get all speaker arrays in the venue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Arrays")
	TArray<FSpatialSpeakerArray> GetAllArrays() const;

	/**
	 * Get array count.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Arrays")
	int32 GetArrayCount() const { return Venue.GetArrayCount(); }

	// ========================================================================
	// SPATIAL QUERIES
	// ========================================================================

	/**
	 * Find speakers near a world position.
	 * @param Position World position to search around.
	 * @param Radius Search radius in cm.
	 * @return Array of speakers within the radius, sorted by distance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers", meta = (ToolTip = "Find speakers within a radius of a position"))
	TArray<FSpatialSpeaker> FindSpeakersNearPosition(FVector Position, float Radius) const;

	/**
	 * Find the closest speaker to a position.
	 * @param Position World position.
	 * @param OutSpeaker Output speaker data.
	 * @return True if a speaker was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers", meta = (ToolTip = "Find the single closest speaker to a position"))
	bool FindClosestSpeaker(FVector Position, FSpatialSpeaker& OutSpeaker) const;

	// ========================================================================
	// CONVENIENCE HELPERS
	// ========================================================================

	/**
	 * Add a speaker to a zone.
	 * @param SpeakerId ID of the speaker.
	 * @param ZoneId ID of the zone.
	 * @return True if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Convenience", meta = (ToolTip = "Assign a speaker to a zone"))
	bool AddSpeakerToZone(const FGuid& SpeakerId, const FGuid& ZoneId);

	/**
	 * Remove a speaker from a zone.
	 * @param SpeakerId ID of the speaker.
	 * @param ZoneId ID of the zone.
	 * @return True if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Convenience", meta = (ToolTip = "Remove a speaker from a zone"))
	bool RemoveSpeakerFromZone(const FGuid& SpeakerId, const FGuid& ZoneId);

	/**
	 * Add an audio object to a zone's routing.
	 * @param ObjectId ID of the audio object.
	 * @param ZoneId ID of the zone.
	 * @return True if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Convenience", meta = (ToolTip = "Route an audio object to a zone"))
	bool AddObjectToZone(const FGuid& ObjectId, const FGuid& ZoneId);

	/**
	 * Remove an audio object from a zone's routing.
	 * @param ObjectId ID of the audio object.
	 * @param ZoneId ID of the zone.
	 * @return True if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Convenience", meta = (ToolTip = "Remove an audio object from a zone's routing"))
	bool RemoveObjectFromZone(const FGuid& ObjectId, const FGuid& ZoneId);

	/**
	 * Remove all audio objects.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Convenience", meta = (ToolTip = "Clear all audio objects"))
	void ClearAllObjects();

	/**
	 * Remove all speakers (and associated zones/arrays).
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Convenience", meta = (ToolTip = "Clear all speakers, zones, and arrays"))
	void ClearAllSpeakers();

	// ========================================================================
	// BATCH OPERATIONS
	// ========================================================================

	/**
	 * Set gain for multiple speakers at once.
	 * @param SpeakerIds Array of speaker IDs.
	 * @param GainDb Gain in decibels.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Batch", meta = (ToolTip = "Set gain for multiple speakers"))
	void SetMultipleSpeakerGains(const TArray<FGuid>& SpeakerIds, float GainDb);

	/**
	 * Set delay for multiple speakers at once.
	 * @param SpeakerIds Array of speaker IDs.
	 * @param DelayMs Delay in milliseconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Batch", meta = (ToolTip = "Set delay for multiple speakers"))
	void SetMultipleSpeakerDelays(const TArray<FGuid>& SpeakerIds, float DelayMs);

	/**
	 * Set mute state for multiple speakers at once.
	 * @param SpeakerIds Array of speaker IDs.
	 * @param bMuted Mute state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Batch", meta = (ToolTip = "Mute or unmute multiple speakers"))
	void SetMultipleSpeakerMute(const TArray<FGuid>& SpeakerIds, bool bMuted);

	/**
	 * Solo multiple speakers (mute all others).
	 * @param SpeakerIds Array of speaker IDs to solo.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Batch", meta = (ToolTip = "Solo speakers - mute all except these"))
	void SoloSpeakers(const TArray<FGuid>& SpeakerIds);

	/**
	 * Clear solo state (unmute all speakers).
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Batch", meta = (ToolTip = "Clear solo - unmute all speakers"))
	void ClearSolo();

	// ========================================================================
	// SYSTEM STATUS
	// ========================================================================

	/**
	 * Get the currently active scene ID (if any).
	 * @return Scene ID, or empty string if no scene active.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Scenes", meta = (ToolTip = "Get the ID of the currently active scene"))
	FString GetActiveSceneId() const { return ActiveSceneId; }

	/**
	 * Check if scene interpolation is in progress.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Scenes", meta = (ToolTip = "Check if scene transition is in progress"))
	bool IsSceneInterpolating() const { return bSceneInterpolationActive; }

	/**
	 * Get scene interpolation progress (0-1).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Scenes", meta = (ToolTip = "Get scene transition progress (0-1)"))
	float GetSceneInterpolationProgress() const;

	/**
	 * Check if the system is ready (audio processor and rendering engine connected).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Status", meta = (ToolTip = "Check if spatial audio is fully initialized"))
	bool IsSystemReady() const { return HasVenue() && (HasAudioProcessor() || HasRenderingEngine()); }

	/**
	 * Get comprehensive system status.
	 * @return Status structure with all relevant info.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Status", meta = (ToolTip = "Get comprehensive system status"))
	FSpatialAudioSystemStatus GetSystemStatus() const;

	// ========================================================================
	// METERING
	// ========================================================================

	/**
	 * Get the current meter reading for a speaker.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Metering")
	FSpatialMeterReading GetSpeakerMeter(const FGuid& SpeakerId) const;

	/**
	 * Get the current meter reading for an audio object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Metering")
	FSpatialMeterReading GetObjectMeter(const FGuid& ObjectId) const;

	// ========================================================================
	// SCENE/PRESET MANAGEMENT
	// ========================================================================

	/**
	 * Store the current state as a scene preset.
	 * Captures all speaker DSP states and audio object positions.
	 * @param SceneName Display name for the scene.
	 * @return Scene ID for recall.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Scenes")
	FString StoreScene(const FString& SceneName);

	/**
	 * Recall a stored scene preset.
	 * @param SceneId The ID returned from StoreScene.
	 * @param bInterpolate Whether to interpolate to new values (vs snap).
	 * @param InterpolateTimeMs Interpolation time in milliseconds.
	 * @return True if scene was found and applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Scenes")
	bool RecallScene(const FString& SceneId, bool bInterpolate = false, float InterpolateTimeMs = 500.0f);

	/**
	 * Delete a stored scene.
	 * @param SceneId The scene to delete.
	 * @return True if scene was found and deleted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Scenes")
	bool DeleteScene(const FString& SceneId);

	/**
	 * Get list of stored scene IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Scenes")
	TArray<FString> GetSceneList() const;

	/**
	 * Get the display name for a stored scene.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Scenes")
	FString GetSceneName(const FString& SceneId) const;

	// ========================================================================
	// VENUE IMPORT/EXPORT
	// ========================================================================

	/**
	 * Export the complete venue configuration to JSON.
	 * Includes speakers, zones, arrays, and audio objects.
	 * @return JSON string of the venue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Venue")
	FString ExportVenueToJson() const;

	/**
	 * Import a venue configuration from JSON.
	 * Replaces the current venue entirely.
	 * @param JsonString The venue JSON.
	 * @return True if import succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Venue")
	bool ImportVenueFromJson(const FString& JsonString);

	/**
	 * Export venue to a file.
	 * @param FilePath Full path to save the file.
	 * @return True if file was written successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Venue")
	bool ExportVenueToFile(const FString& FilePath) const;

	/**
	 * Import venue from a file.
	 * @param FilePath Full path to the venue file.
	 * @return True if import succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Venue")
	bool ImportVenueFromFile(const FString& FilePath);

	// ========================================================================
	// DELEGATES
	// ========================================================================

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnSpeakerAdded OnSpeakerAdded;

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnSpeakerRemoved OnSpeakerRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnSpeakerUpdated OnSpeakerUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnZoneAdded OnZoneAdded;

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnZoneRemoved OnZoneRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnObjectAdded OnObjectAdded;

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnObjectRemoved OnObjectRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnObjectPositionChanged OnObjectPositionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|Events")
	FOnVenueChanged OnVenueChanged;

	// ========================================================================
	// AUDIO PROCESSOR INTEGRATION
	// ========================================================================

	/**
	 * Set the audio processor for DSP processing.
	 * Called by external components (Submix, Audio Component) that own the processor.
	 * @param Processor The audio processor instance.
	 */
	void SetAudioProcessor(FSpatialAudioProcessor* Processor);

	/**
	 * Get the audio processor.
	 * @return The audio processor, or nullptr if not set.
	 */
	FSpatialAudioProcessor* GetAudioProcessor() const { return AudioProcessor; }

	/**
	 * Check if audio processor is connected.
	 */
	bool HasAudioProcessor() const { return AudioProcessor != nullptr; }

	// ========================================================================
	// RENDERING ENGINE INTEGRATION
	// ========================================================================

	/**
	 * Set the rendering engine for VBAP/DBAP gain computation.
	 * The rendering engine owns its own audio processor internally.
	 * @param Engine The rendering engine instance.
	 */
	void SetRenderingEngine(FSpatialRenderingEngine* Engine);

	/**
	 * Get the rendering engine.
	 * @return The rendering engine, or nullptr if not set.
	 */
	FSpatialRenderingEngine* GetRenderingEngine() const { return RenderingEngine; }

	/**
	 * Check if rendering engine is connected.
	 */
	bool HasRenderingEngine() const { return RenderingEngine != nullptr; }

	/**
	 * Set the renderer type (VBAP, DBAP, etc.) for the current zone.
	 * @param RendererType The renderer algorithm to use.
	 */
	void SetGlobalRendererType(ESpatialRendererType RendererType);

	/**
	 * Get the current global renderer type.
	 */
	ESpatialRendererType GetGlobalRendererType() const { return CurrentRendererType; }

	/**
	 * Set the reference/listener position for rendering.
	 * @param Position The listener position in world space.
	 */
	void SetListenerPosition(const FVector& Position);

	// ========================================================================
	// EXTERNAL PROCESSOR INTEGRATION
	// ========================================================================

	/**
	 * Configure an external spatial audio processor (e.g., d&b DS100, L-ISA).
	 * This creates and initializes a processor connection.
	 * @param Config Processor configuration.
	 * @return True if processor was configured successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool ConfigureExternalProcessor(const FExternalProcessorConfig& Config);

	/**
	 * Connect to the configured external processor.
	 * @return True if connection was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool ConnectExternalProcessor();

	/**
	 * Disconnect from the external processor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	void DisconnectExternalProcessor();

	/**
	 * Check if external processor is connected.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool IsExternalProcessorConnected() const;

	/**
	 * Get the current external processor connection status.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	EProcessorConnectionState GetExternalProcessorState() const;

	/**
	 * Get the external processor status.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	FExternalProcessorStatus GetExternalProcessorStatus() const;

	/**
	 * Map an internal audio object to an external processor object.
	 * @param ObjectId Internal audio object ID.
	 * @param ExternalObjectNumber External processor object number (e.g., 1-64 for DS100).
	 * @param MappingArea Optional mapping area (processor-specific).
	 * @return True if mapping was registered.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool MapObjectToExternalProcessor(const FGuid& ObjectId, int32 ExternalObjectNumber, int32 MappingArea = 1);

	/**
	 * Remove object mapping from external processor.
	 * @param ObjectId Internal audio object ID.
	 * @return True if mapping was removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool UnmapObjectFromExternalProcessor(const FGuid& ObjectId);

	/**
	 * Enable/disable forwarding object positions to external processor.
	 * When enabled, SetObjectPosition() will also send to the external processor.
	 * @param bEnable True to enable forwarding.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	void SetExternalProcessorForwarding(bool bEnable);

	/**
	 * Check if external processor forwarding is enabled.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool IsExternalProcessorForwardingEnabled() const { return bExternalProcessorForwardingEnabled; }

	/**
	 * Send a position update directly to the external processor.
	 * Use this for immediate updates bypassing the normal object system.
	 * @param ExternalObjectNumber External processor object number.
	 * @param Position World position.
	 * @return True if update was sent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool SendPositionToExternalProcessor(int32 ExternalObjectNumber, const FVector& Position);

	/**
	 * Get the external processor interface (for advanced usage).
	 * @return Processor interface, or nullptr if not configured.
	 */
	IExternalSpatialProcessor* GetExternalProcessor() const { return ExternalProcessor; }

	// ========================================================================
	// DIAGNOSTICS
	// ========================================================================

	/**
	 * Get diagnostic info as string.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Diagnostics")
	FString GetDiagnosticInfo() const;

	/**
	 * Validate the current configuration.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Diagnostics")
	TArray<FString> ValidateConfiguration() const;

private:
#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
	// Parent subsystem reference
	UPROPERTY()
	URshipSubsystem* Subsystem;
#endif

	// Venue configuration
	FSpatialVenue Venue;

	// Audio objects (separate from venue as they're runtime entities)
	TMap<FGuid, FSpatialAudioObject> AudioObjects;

	// Stored scenes (keyed by scene ID)
	TMap<FString, FString> StoredScenes;  // ID -> JSON
	TMap<FString, FString> SceneNames;    // ID -> Display Name

	// Currently active scene ID
	FString ActiveSceneId;

	// Tick timer
	float MeterUpdateAccumulator;
	static constexpr float MeterUpdateInterval = 1.0f / 60.0f;  // 60Hz

	// ========================================================================
	// Scene Interpolation State
	// ========================================================================

	/** Interpolation target for a speaker */
	struct FSpeakerInterpolationTarget
	{
		float TargetInputGain;
		float TargetOutputGain;
		float TargetDelay;
		bool bTargetMuted;
		float StartInputGain;
		float StartOutputGain;
		float StartDelay;
		bool bStartMuted;
	};

	/** Interpolation target for an audio object */
	struct FObjectInterpolationTarget
	{
		FVector TargetPosition;
		float TargetSpread;
		float TargetGain;
		bool bTargetMuted;
		FVector StartPosition;
		float StartSpread;
		float StartGain;
		bool bStartMuted;
	};

	/** Is scene interpolation currently active */
	bool bSceneInterpolationActive;

	/** Total interpolation time in seconds */
	float SceneInterpolationDuration;

	/** Current interpolation time elapsed in seconds */
	float SceneInterpolationElapsed;

	/** Speaker interpolation targets */
	TMap<FGuid, FSpeakerInterpolationTarget> SpeakerInterpolationTargets;

	/** Object interpolation targets */
	TMap<FGuid, FObjectInterpolationTarget> ObjectInterpolationTargets;

	/** Update scene interpolation progress */
	void UpdateSceneInterpolation(float DeltaTime);

#if RSHIP_SPATIAL_AUDIO_HAS_EXEC
	// ========================================================================
	// rShip/Myko Integration
	// ========================================================================

	/** Is Myko registration complete */
	bool bMykoRegistered;

	/** Cached speaker IDs for meter pulses */
	TArray<FGuid> CachedSpeakerIds;

	/** Register all Myko targets for the current venue */
	void RegisterMykoTargets();

	/** Unregister all Myko targets */
	void UnregisterMykoTargets();

	/** Register individual entity targets */
	void RegisterSpeakerTarget(const FSpatialSpeaker& Speaker);
	void RegisterZoneTarget(const FSpatialZone& Zone);
	void RegisterObjectTarget(const FSpatialAudioObject& Object);

	/** Unregister individual entity targets */
	void UnregisterSpeakerTarget(const FGuid& SpeakerId);
	void UnregisterZoneTarget(const FGuid& ZoneId);
	void UnregisterObjectTarget(const FGuid& ObjectId);

	/** Send meter data as pulses */
	void SendMeterPulses();

	/** Send speaker state update to rShip */
	void SendSpeakerUpdate(const FGuid& SpeakerId);

	/** Send zone state update to rShip */
	void SendZoneUpdate(const FGuid& ZoneId);

	/** Send object state update to rShip */
	void SendObjectUpdate(const FGuid& ObjectId);

	/** Process an incoming action from rShip */
	void ProcessRshipAction(const FString& TargetId, const FString& ActionId, const TSharedPtr<FJsonObject>& Data);

	/** Process speaker-specific action */
	void ProcessSpeakerAction(const FGuid& SpeakerId, const FString& ActionId, const TSharedPtr<FJsonObject>& Data);

	/** Process zone-specific action */
	void ProcessZoneAction(const FGuid& ZoneId, const FString& ActionId, const TSharedPtr<FJsonObject>& Data);

	/** Process object-specific action */
	void ProcessObjectAction(const FGuid& ObjectId, const FString& ActionId, const TSharedPtr<FJsonObject>& Data);
#endif // RSHIP_SPATIAL_AUDIO_HAS_EXEC

	// ========================================================================
	// Audio Engine Integration
	// ========================================================================

	/** Audio processor (set by external component like Submix) */
	FSpatialAudioProcessor* AudioProcessor;

	/** Rendering engine for VBAP/DBAP gain computation */
	FSpatialRenderingEngine* RenderingEngine;

	/** Current global renderer type */
	ESpatialRendererType CurrentRendererType;

	/** Speaker ID to index mapping for audio processor */
	TMap<FGuid, int32> SpeakerIdToIndex;

	// ========================================================================
	// External Processor Integration
	// ========================================================================

	/** External spatial processor (e.g., DS100) */
	IExternalSpatialProcessor* ExternalProcessor;

	/** External processor configuration */
	FExternalProcessorConfig ExternalProcessorConfig;

	/** Whether to forward object positions to external processor */
	bool bExternalProcessorForwardingEnabled;

	/** Update external processor with object position */
	void UpdateExternalProcessorObjectPosition(const FGuid& ObjectId, const FVector& Position);

	/** Update external processor with object spread */
	void UpdateExternalProcessorObjectSpread(const FGuid& ObjectId, float Spread);

	/** Update external processor with object gain */
	void UpdateExternalProcessorObjectGain(const FGuid& ObjectId, float GainDb);

	/** Build speaker ID to index mapping */
	void RebuildSpeakerIndexMapping();

	/** Sync speaker configuration to rendering engine */
	void SyncSpeakersToRenderingEngine();

	/** Update the audio engine with current state */
	void UpdateAudioEngine();

	/** Notify audio engine of parameter change */
	void NotifyDSPChange(const FGuid& SpeakerId);

	/** Notify audio engine of object change */
	void NotifyObjectChange(const FGuid& ObjectId);

	/** Build DSP config from speaker */
	FSpatialSpeakerDSPConfig BuildDSPConfig(const FSpatialSpeaker& Speaker) const;
};
