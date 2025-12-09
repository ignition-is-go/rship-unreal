// Rship Blueprint Function Library
// Static functions for easy Blueprint access to rship features

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RshipCalibrationTypes.h"
#include "RshipSceneValidator.h"
#include "RshipTimecodeSync.h"
#include "RshipMultiCameraManager.h"
#include "RshipSequencerSync.h"
#include "RshipDMXOutput.h"
#include "RshipLiveLinkSource.h"
#include "RshipBlueprintLibrary.generated.h"

class URshipSubsystem;
class URshipTargetComponent;
class ULevelSequence;

/**
 * Blueprint function library for common rship operations.
 * Provides static functions accessible from any Blueprint.
 */
UCLASS()
class RSHIPEXEC_API URshipBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========================================================================
    // CONNECTION
    // ========================================================================

    /** Check if connected to rship server */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Connection")
    static bool IsConnectedToRship();

    /** Reconnect to rship server */
    UFUNCTION(BlueprintCallable, Category = "Rship|Connection")
    static void ReconnectToRship();

    /** Get the rship service ID */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Connection")
    static FString GetRshipServiceId();

    // ========================================================================
    // TARGETS
    // ========================================================================

    /** Get all registered target components in the world */
    UFUNCTION(BlueprintCallable, Category = "Rship|Targets")
    static TArray<URshipTargetComponent*> GetAllTargetComponents();

    /** Find a target component by its target ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Targets")
    static URshipTargetComponent* FindTargetById(const FString& TargetId);

    /** Pulse an emitter with JSON data (use MakeRshipData helpers) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Targets")
    static void PulseEmitter(const FString& TargetId, const FString& EmitterId, const TMap<FString, FString>& Data);

    // ========================================================================
    // FIXTURES
    // ========================================================================

    /** Get all registered fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    static TArray<FRshipFixtureInfo> GetAllFixtures();

    /** Get a specific fixture by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    static bool GetFixtureById(const FString& FixtureId, FRshipFixtureInfo& OutFixture);

    /** Set fixture intensity (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    static void SetFixtureIntensity(const FString& FixtureId, float Intensity);

    /** Set fixture color */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    static void SetFixtureColor(const FString& FixtureId, FLinearColor Color);

    /** Set fixture intensity and color together */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    static void SetFixtureState(const FString& FixtureId, float Intensity, FLinearColor Color);

    // ========================================================================
    // CAMERAS
    // ========================================================================

    /** Get all camera views */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    static TArray<FRshipCameraView> GetAllCameraViews();

    /** Switch to a camera view with optional transition */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    static void SwitchToCameraView(const FString& ViewId, ERshipTransitionType TransitionType = ERshipTransitionType::Cut, float Duration = 0.0f);

    /** Cut to program (instant switch from preview) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    static void CutToProgram();

    /** Auto transition (use default transition settings) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    static void AutoTransition();

    /** Get the current program view */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Cameras")
    static FRshipCameraView GetProgramView();

    /** Get the current preview view */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Cameras")
    static FRshipCameraView GetPreviewView();

    // ========================================================================
    // TIMECODE
    // ========================================================================

    /** Get current timecode as formatted string (HH:MM:SS:FF) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Timecode")
    static FString GetCurrentTimecodeString();

    /** Get current timecode status */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Timecode")
    static FRshipTimecodeStatus GetTimecodeStatus();

    /** Get elapsed time in seconds */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Timecode")
    static float GetElapsedSeconds();

    /** Get current frame number */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Timecode")
    static int64 GetCurrentFrame();

    /** Play timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    static void PlayTimecode();

    /** Pause timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    static void PauseTimecode();

    /** Stop and reset timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    static void StopTimecode();

    /** Seek to a specific time in seconds */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    static void SeekToTime(float Seconds);

    /** Set playback speed (1.0 = normal, 0.5 = half, 2.0 = double) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    static void SetPlaybackSpeed(float Speed);

    // ========================================================================
    // SEQUENCER
    // ========================================================================

    /** Is sequencer sync enabled */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Sequencer")
    static bool IsSequencerSyncEnabled();

    /** Enable/disable sequencer sync */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    static void SetSequencerSyncEnabled(bool bEnabled);

    /** Play synced sequences */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    static void PlaySequencer();

    /** Stop synced sequences */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    static void StopSequencer();

    /** Force sync sequencer to current timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    static void ForceSyncSequencer();

    /** Quick sync a level sequence starting at current timecode */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    static FString QuickSyncLevelSequence(ULevelSequence* Sequence);

    // ========================================================================
    // SCENE CONVERSION
    // ========================================================================

    /** Discover all convertible items in the current scene */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConversion")
    static int32 DiscoverScene();

    /** Validate the scene for conversion */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConversion")
    static FRshipValidationResult ValidateScene();

    /** Convert all discovered lights to rship fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConversion")
    static int32 ConvertAllLights();

    // ========================================================================
    // VISUALIZATION
    // ========================================================================

    /** Show/hide all fixture beam visualizations */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    static void SetBeamVisualizationVisible(bool bVisible);

    /** Set beam visualization mode for all fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    static void SetBeamVisualizationMode(ERshipVisualizationMode Mode);

    /** Apply programming preset (full visibility) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    static void ApplyProgrammingVisualization();

    /** Apply show preset (minimal visibility) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    static void ApplyShowVisualization();

    // ========================================================================
    // DMX OUTPUT
    // ========================================================================

    /** Check if DMX output is enabled */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    static bool IsDMXOutputEnabled();

    /** Enable or disable DMX output */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    static void SetDMXOutputEnabled(bool bEnabled);

    /** Get the current DMX protocol (Art-Net or sACN) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    static ERshipDMXProtocol GetDMXProtocol();

    /** Set the DMX protocol */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    static void SetDMXProtocol(ERshipDMXProtocol Protocol);

    /** Set DMX destination address */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    static void SetDMXDestination(const FString& IPAddress);

    /** Trigger DMX blackout (all channels to 0) */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    static void DMXBlackout();

    /** Release DMX blackout */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    static void DMXReleaseBlackout();

    /** Check if DMX is in blackout mode */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    static bool IsDMXBlackout();

    /** Set DMX master dimmer (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    static void SetDMXMasterDimmer(float Dimmer);

    /** Get DMX master dimmer (0-1) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    static float GetDMXMasterDimmer();

    /** Set a specific DMX channel value */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    static void SetDMXChannel(int32 Universe, int32 Channel, uint8 Value);

    /** Auto-map all fixtures to DMX */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    static int32 DMXAutoMapFixtures(int32 StartUniverse = 1, int32 StartAddress = 1);

    /** Get number of mapped fixtures */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    static int32 GetDMXFixtureCount();

    // ========================================================================
    // OSC BRIDGE
    // ========================================================================

    /** Check if OSC server is running */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|OSC")
    static bool IsOSCServerRunning();

    /** Start OSC server on specified port */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    static bool StartOSCServer(int32 Port = 8000);

    /** Stop OSC server */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    static void StopOSCServer();

    /** Send OSC float message */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    static void SendOSCFloat(const FString& Address, float Value);

    /** Send OSC color message */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    static void SendOSCColor(const FString& Address, FLinearColor Color);

    /** Add OSC destination (IP and port to send to) */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    static void AddOSCDestination(const FString& Name, const FString& IPAddress, int32 Port);

    /** Remove OSC destination by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    static void RemoveOSCDestination(const FString& Name);

    /** Create TouchOSC preset mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    static void CreateTouchOSCMappings();

    /** Create QLab preset mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    static void CreateQLabMappings();

    // ========================================================================
    // LIVE LINK
    // ========================================================================

    /** Check if Live Link source is active */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|LiveLink")
    static bool IsLiveLinkSourceActive();

    /** Start Live Link source */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    static bool StartLiveLinkSource();

    /** Stop Live Link source */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    static void StopLiveLinkSource();

    /** Create Live Link subjects from all fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    static int32 CreateLiveLinkSubjectsFromFixtures();

    /** Create a camera tracking Live Link subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    static void CreateLiveLinkCameraSubject(const FString& EmitterId, FName SubjectName);

    /** Create a light tracking Live Link subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    static void CreateLiveLinkLightSubject(const FString& EmitterId, FName SubjectName);

    /** Update Live Link transform manually */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    static void UpdateLiveLinkTransform(FName SubjectName, FTransform Transform);

    /** Get all Live Link subject names */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    static TArray<FName> GetLiveLinkSubjectNames();

    // ========================================================================
    // UTILITY
    // ========================================================================

    /** Get the rship subsystem (for advanced usage) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Utility", meta = (DisplayName = "Get Rship Subsystem"))
    static URshipSubsystem* GetRshipSubsystem();

    /** Format a timecode to string */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Utility")
    static FString FormatTimecode(int32 Hours, int32 Minutes, int32 Seconds, int32 Frames);

    /** Parse a timecode string to components */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Utility")
    static bool ParseTimecode(const FString& TimecodeString, int32& Hours, int32& Minutes, int32& Seconds, int32& Frames);

    /** Convert linear color to hex string */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Utility")
    static FString ColorToHex(FLinearColor Color);

    /** Convert hex string to linear color */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|Utility")
    static FLinearColor HexToColor(const FString& HexString);

private:
    static URshipSubsystem* GetSubsystem();
};
