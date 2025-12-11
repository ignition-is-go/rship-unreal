// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/SpatialAudioTypes.h"
#include "Core/SpatialSpeaker.h"
#include "ExternalProcessor/ExternalProcessorTypes.h"
#include "SpatialAudioBlueprintLibrary.generated.h"

// Forward declarations
class URshipSpatialAudioManager;

/**
 * Blueprint function library for Rship Spatial Audio.
 *
 * Provides convenient Blueprint-callable functions for common
 * spatial audio operations.
 */
UCLASS()
class RSHIPSPATIALAUDIORUNTIME_API USpatialAudioBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========================================================================
	// MANAGER ACCESS
	// ========================================================================

	/**
	 * Get the global spatial audio manager.
	 * @return The manager, or nullptr if not available.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio", meta = (WorldContext = "WorldContextObject"))
	static URshipSpatialAudioManager* GetSpatialAudioManager(UObject* WorldContextObject);

	// ========================================================================
	// QUICK SETUP
	// ========================================================================

	/**
	 * Quick setup: Create a standard stereo speaker pair.
	 * @param WorldContextObject World context.
	 * @param Distance Distance from center in cm.
	 * @param Height Height in cm.
	 * @return Array of created speaker IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|QuickSetup", meta = (WorldContext = "WorldContextObject"))
	static TArray<FGuid> CreateStereoPair(
		UObject* WorldContextObject,
		float Distance = 300.0f,
		float Height = 150.0f);

	/**
	 * Quick setup: Create a 5.1 surround speaker layout.
	 * @param WorldContextObject World context.
	 * @param Radius Radius of the speaker ring in cm.
	 * @param Height Height of speakers in cm.
	 * @return Array of created speaker IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|QuickSetup", meta = (WorldContext = "WorldContextObject"))
	static TArray<FGuid> Create51SurroundLayout(
		UObject* WorldContextObject,
		float Radius = 400.0f,
		float Height = 150.0f);

	/**
	 * Quick setup: Create a speaker ring (for immersive/dome installations).
	 * @param WorldContextObject World context.
	 * @param NumSpeakers Number of speakers in the ring.
	 * @param Radius Radius in cm.
	 * @param Height Height in cm.
	 * @return Array of created speaker IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|QuickSetup", meta = (WorldContext = "WorldContextObject"))
	static TArray<FGuid> CreateSpeakerRing(
		UObject* WorldContextObject,
		int32 NumSpeakers = 8,
		float Radius = 500.0f,
		float Height = 150.0f);

	/**
	 * Quick setup: Create a speaker dome (hemisphere).
	 * @param WorldContextObject World context.
	 * @param NumRings Number of elevation rings.
	 * @param SpeakersPerRing Number of speakers per ring.
	 * @param Radius Dome radius in cm.
	 * @return Array of created speaker IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|QuickSetup", meta = (WorldContext = "WorldContextObject"))
	static TArray<FGuid> CreateSpeakerDome(
		UObject* WorldContextObject,
		int32 NumRings = 3,
		int32 SpeakersPerRing = 8,
		float Radius = 600.0f);

	// ========================================================================
	// AUDIO OBJECT HELPERS
	// ========================================================================

	/**
	 * Create an audio object and attach it to an actor.
	 * @param WorldContextObject World context.
	 * @param ActorToFollow Actor to follow.
	 * @param Name Object name.
	 * @return Created object ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (WorldContext = "WorldContextObject"))
	static FGuid CreateAudioObjectForActor(
		UObject* WorldContextObject,
		AActor* ActorToFollow,
		const FString& Name);

	/**
	 * Move an audio object along a path over time.
	 * Note: This sets up the movement; use TickAudioObjectPath to update.
	 * @param ObjectId Object to move.
	 * @param PathPoints Array of positions in the path.
	 * @param Duration Total duration in seconds.
	 * @param bLoop Whether to loop the path.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (WorldContext = "WorldContextObject"))
	static void SetAudioObjectPath(
		UObject* WorldContextObject,
		const FGuid& ObjectId,
		const TArray<FVector>& PathPoints,
		float Duration,
		bool bLoop = false);

	/**
	 * Get all speaker positions as an array.
	 * Useful for visualization or debugging.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers", meta = (WorldContext = "WorldContextObject"))
	static TArray<FVector> GetAllSpeakerPositions(UObject* WorldContextObject);

	/**
	 * Get all audio object positions as an array.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (WorldContext = "WorldContextObject"))
	static TArray<FVector> GetAllAudioObjectPositions(UObject* WorldContextObject);

	// ========================================================================
	// EXTERNAL PROCESSOR HELPERS
	// ========================================================================

	/**
	 * Quick setup: Configure and connect to a DS100.
	 * @param WorldContextObject World context.
	 * @param IPAddress DS100 IP address.
	 * @param SendPort OSC send port (default 50010).
	 * @param ReceivePort OSC receive port (default 50011).
	 * @return True if connection was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor", meta = (WorldContext = "WorldContextObject"))
	static bool QuickConnectDS100(
		UObject* WorldContextObject,
		const FString& IPAddress,
		int32 SendPort = 50010,
		int32 ReceivePort = 50011);

	/**
	 * Auto-map all audio objects to DS100 sources sequentially.
	 * Object 1 -> Source 1, Object 2 -> Source 2, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor", meta = (WorldContext = "WorldContextObject"))
	static int32 AutoMapObjectsToDS100(UObject* WorldContextObject);

	// ========================================================================
	// RENDERER HELPERS
	// ========================================================================

	/**
	 * Set the renderer type by name.
	 * @param WorldContextObject World context.
	 * @param RendererName "VBAP", "DBAP", "HOA", or "Direct".
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Rendering", meta = (WorldContext = "WorldContextObject"))
	static void SetRendererByName(UObject* WorldContextObject, const FString& RendererName);

	/**
	 * Get the current renderer type as a string.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Rendering", meta = (WorldContext = "WorldContextObject"))
	static FString GetCurrentRendererName(UObject* WorldContextObject);

	// ========================================================================
	// DSP HELPERS
	// ========================================================================

	/**
	 * Set gain for all speakers at once.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP", meta = (WorldContext = "WorldContextObject"))
	static void SetAllSpeakersGain(UObject* WorldContextObject, float GainDb);

	/**
	 * Mute all speakers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP", meta = (WorldContext = "WorldContextObject"))
	static void MuteAllSpeakers(UObject* WorldContextObject, bool bMute);

	/**
	 * Apply delay alignment based on speaker distances from reference point.
	 * @param WorldContextObject World context.
	 * @param ReferencePoint The point to measure distances from.
	 * @param SpeedOfSound Speed of sound in cm/s (default ~34300 cm/s).
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|DSP", meta = (WorldContext = "WorldContextObject"))
	static void AutoAlignSpeakerDelays(
		UObject* WorldContextObject,
		FVector ReferencePoint,
		float SpeedOfSound = 34300.0f);

	// ========================================================================
	// SCENE HELPERS
	// ========================================================================

	/**
	 * Store current state as named scene.
	 * @return Scene ID for later recall.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Scenes", meta = (WorldContext = "WorldContextObject"))
	static FString StoreCurrentScene(UObject* WorldContextObject, const FString& SceneName);

	/**
	 * Recall a scene with optional crossfade.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Scenes", meta = (WorldContext = "WorldContextObject"))
	static bool RecallSceneWithFade(
		UObject* WorldContextObject,
		const FString& SceneId,
		float FadeTimeSeconds = 0.5f);

	// ========================================================================
	// CONVERSION UTILITIES
	// ========================================================================

	/**
	 * Convert decibels to linear gain.
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|SpatialAudio|Utilities")
	static float DbToLinear(float Db);

	/**
	 * Convert linear gain to decibels.
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|SpatialAudio|Utilities")
	static float LinearToDb(float Linear);

	/**
	 * Convert milliseconds to samples at a given sample rate.
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|SpatialAudio|Utilities")
	static int32 MsToSamples(float Ms, float SampleRate = 48000.0f);

	/**
	 * Convert samples to milliseconds.
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|SpatialAudio|Utilities")
	static float SamplesToMs(int32 Samples, float SampleRate = 48000.0f);

	/**
	 * Calculate delay in ms for a distance at speed of sound.
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|SpatialAudio|Utilities", meta = (ToolTip = "Calculate acoustic delay for a given distance"))
	static float DistanceToDelayMs(float DistanceCm, float SpeedOfSoundCmPerSec = 34300.0f);

	// ========================================================================
	// ADVANCED SETUP HELPERS
	// ========================================================================

	/**
	 * Create a line array of speakers.
	 * @param WorldContextObject World context.
	 * @param StartPosition Start position of the array.
	 * @param EndPosition End position of the array.
	 * @param NumSpeakers Number of speakers in the line.
	 * @param ArrayName Name for the array.
	 * @return Array of created speaker IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|QuickSetup", meta = (WorldContext = "WorldContextObject", ToolTip = "Create a vertical or horizontal line array"))
	static TArray<FGuid> CreateLineArray(
		UObject* WorldContextObject,
		FVector StartPosition,
		FVector EndPosition,
		int32 NumSpeakers = 8,
		const FString& ArrayName = TEXT("LineArray"));

	/**
	 * Create a zone and assign speakers to it in one call.
	 * @param WorldContextObject World context.
	 * @param ZoneName Name for the zone.
	 * @param SpeakerIds Speakers to assign to the zone.
	 * @param RendererType Renderer type for the zone.
	 * @return The created zone ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|QuickSetup", meta = (WorldContext = "WorldContextObject", ToolTip = "Create a zone with speakers assigned"))
	static FGuid CreateZoneWithSpeakers(
		UObject* WorldContextObject,
		const FString& ZoneName,
		const TArray<FGuid>& SpeakerIds,
		ESpatialRendererType RendererType = ESpatialRendererType::VBAP);

	/**
	 * Create multiple speakers at specific positions.
	 * @param WorldContextObject World context.
	 * @param Positions Array of speaker positions.
	 * @param NamePrefix Prefix for speaker names (Speaker_1, Speaker_2, etc.).
	 * @return Array of created speaker IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|QuickSetup", meta = (WorldContext = "WorldContextObject", ToolTip = "Create speakers at specific positions"))
	static TArray<FGuid> CreateSpeakersAtPositions(
		UObject* WorldContextObject,
		const TArray<FVector>& Positions,
		const FString& NamePrefix = TEXT("Speaker"));

	/**
	 * Automatically assign output channels to all speakers sequentially.
	 * @param WorldContextObject World context.
	 * @param StartChannel First output channel (default 1).
	 * @return Number of speakers assigned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|QuickSetup", meta = (WorldContext = "WorldContextObject", ToolTip = "Auto-assign output channels 1, 2, 3..."))
	static int32 AutoAssignOutputChannels(UObject* WorldContextObject, int32 StartChannel = 1);

	// ========================================================================
	// STATUS & DIAGNOSTICS
	// ========================================================================

	/**
	 * Check if the spatial audio system is ready to use.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Status", meta = (WorldContext = "WorldContextObject", ToolTip = "Check if system is initialized and ready"))
	static bool IsSystemReady(UObject* WorldContextObject);

	/**
	 * Get comprehensive system status.
	 * Useful for debugging and status displays.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Status", meta = (WorldContext = "WorldContextObject", ToolTip = "Get comprehensive status information"))
	static FSpatialAudioSystemStatus GetSystemStatus(UObject* WorldContextObject);

	/**
	 * Get the closest speaker to a world position.
	 * @param WorldContextObject World context.
	 * @param Position World position to search from.
	 * @param OutSpeakerId Output speaker ID.
	 * @param OutDistance Output distance to speaker.
	 * @return True if a speaker was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Speakers", meta = (WorldContext = "WorldContextObject", ToolTip = "Find the nearest speaker to a position"))
	static bool GetClosestSpeaker(
		UObject* WorldContextObject,
		FVector Position,
		FGuid& OutSpeakerId,
		float& OutDistance);

	/**
	 * Get count of speakers, zones, and objects as a quick status check.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|SpatialAudio|Status", meta = (WorldContext = "WorldContextObject", ToolTip = "Get entity counts for status display"))
	static void GetEntityCounts(
		UObject* WorldContextObject,
		int32& OutSpeakerCount,
		int32& OutZoneCount,
		int32& OutObjectCount);

	// ========================================================================
	// OBJECT MANAGEMENT HELPERS
	// ========================================================================

	/**
	 * Create audio objects for all tagged actors in the level.
	 * @param WorldContextObject World context.
	 * @param ActorTag Tag to search for.
	 * @return Number of objects created.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (WorldContext = "WorldContextObject", ToolTip = "Create audio objects for all actors with a specific tag"))
	static int32 CreateObjectsForTaggedActors(
		UObject* WorldContextObject,
		FName ActorTag);

	/**
	 * Route all audio objects to a zone.
	 * @param WorldContextObject World context.
	 * @param ZoneId Zone to route to.
	 * @return Number of objects routed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (WorldContext = "WorldContextObject", ToolTip = "Route all objects to a specific zone"))
	static int32 RouteAllObjectsToZone(UObject* WorldContextObject, const FGuid& ZoneId);

	/**
	 * Clear all routing for all audio objects.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Objects", meta = (WorldContext = "WorldContextObject", ToolTip = "Clear zone routing for all objects"))
	static void ClearAllObjectRouting(UObject* WorldContextObject);

	// ========================================================================
	// DEBUGGING HELPERS
	// ========================================================================

	/**
	 * Print spatial audio system status to the log.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Debug", meta = (WorldContext = "WorldContextObject", ToolTip = "Print system status to log"))
	static void PrintSystemStatus(UObject* WorldContextObject);

	/**
	 * Visualize speaker coverage by playing test tones through each speaker.
	 * Note: Requires audio processor to be connected.
	 * @param WorldContextObject World context.
	 * @param DurationPerSpeaker How long to play each speaker in seconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|Debug", meta = (WorldContext = "WorldContextObject", ToolTip = "Play test tone through each speaker sequentially"))
	static void TestAllSpeakers(UObject* WorldContextObject, float DurationPerSpeaker = 0.5f);
};
