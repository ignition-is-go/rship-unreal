// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "IWebSocket.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RshipTargetComponent.h"
#include "RshipTargetGroup.h"
#include "RshipHealthMonitor.h"
#include "RshipPresetManager.h"
#include "RshipTemplateManager.h"
#include "RshipLevelManager.h"
#include "RshipEditorSelection.h"
#include "RshipDataLayerManager.h"
#include "RshipFixtureManager.h"
#include "RshipCameraManager.h"
#include "RshipIESProfileService.h"
#include "RshipSceneConverter.h"
#include "RshipEditorTransformSync.h"
#include "RshipPulseReceiver.h"
#include "RshipFeedbackReporter.h"
#include "RshipFixtureVisualizer.h"
#include "RshipTimecodeSync.h"
#include "RshipFixtureLibrary.h"
#include "RshipMultiCameraManager.h"
#include "RshipSceneValidator.h"
#include "RshipNiagaraBinding.h"
#include "RshipSequencerSync.h"
#include "RshipMaterialBinding.h"
#include "RshipDMXOutput.h"
#include "RshipOSCBridge.h"
#include "RshipLiveLinkSource.h"
#include "RshipAudioReactive.h"
#include "RshipRecorder.h"
#include "RshipControlRigBinding.h"
#if RSHIP_HAS_PCG
#include "PCG/RshipPCGBinding.h"
#else
// Forward declaration when PCG plugin is not available
class URshipPCGManager;
#endif
#include "GameFramework/Actor.h"

// Forward declaration for optional SpatialAudio plugin
class URshipSpatialAudioManager;
#include "Containers/List.h"
#include "Target.h"
#include "RshipRateLimiter.h"
#include "RshipWebSocket.h"
#include "RshipSubsystem.generated.h"


DECLARE_DYNAMIC_DELEGATE(FRshipMessageDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipSelectionChanged);

// Connection state for tracking
enum class ERshipConnectionState : uint8
{
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    BackingOff
};

/**
 * Main subsystem for managing Rocketship WebSocket connection and message routing.
 * Uses rate limiting and message queuing to prevent overwhelming the server.
 */
UCLASS()
class RSHIPEXEC_API URshipSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

    // Friend classes that need access to SetItem
    friend class URshipCameraManager;
    friend class URshipFixtureManager;

    AEmitterHandler *EmitterHandler;

    // WebSocket connections (use one or the other based on settings)
    TSharedPtr<IWebSocket> WebSocket;                    // Standard UE WebSocket
    TSharedPtr<FRshipWebSocket> HighPerfWebSocket;       // High-performance WebSocket
    bool bUsingHighPerfWebSocket;                        // Which one is active

    FString InstanceId;
    FString ServiceId;
    FString MachineId;

    FString ClientId = "UNSET";
    FString ClusterId;

    // Rate limiter for outbound messages
    TUniquePtr<FRshipRateLimiter> RateLimiter;

    // Group manager for target organization (lazy initialized)
    UPROPERTY()
    URshipTargetGroupManager* GroupManager;

    // Health monitor for dashboard (lazy initialized)
    UPROPERTY()
    URshipHealthMonitor* HealthMonitor;

    // Preset manager for emitter state capture/recall (lazy initialized)
    UPROPERTY()
    URshipPresetManager* PresetManager;

    // Template manager for target configuration templates (lazy initialized)
    UPROPERTY()
    URshipTemplateManager* TemplateManager;

    // Level manager for streaming level awareness (lazy initialized)
    UPROPERTY()
    URshipLevelManager* LevelManager;

    // Editor selection sync (lazy initialized, Editor only)
    UPROPERTY()
    URshipEditorSelection* EditorSelection;

    // Data Layer manager for World Partition workflows (lazy initialized)
    UPROPERTY()
    URshipDataLayerManager* DataLayerManager;

    // Fixture manager for lighting calibration (lazy initialized)
    UPROPERTY()
    URshipFixtureManager* FixtureManager;

    // Camera manager for camera calibration (lazy initialized)
    UPROPERTY()
    URshipCameraManager* CameraManager;

    // IES profile service for photometric data (lazy initialized)
    UPROPERTY()
    URshipIESProfileService* IESProfileService;

    // Scene converter for importing existing UE scenes (lazy initialized)
    UPROPERTY()
    URshipSceneConverter* SceneConverter;

    // Editor transform sync for automatic position synchronization (lazy initialized)
    UPROPERTY()
    URshipEditorTransformSync* EditorTransformSync;

    // Pulse receiver for incoming fixture control values (lazy initialized)
    UPROPERTY()
    URshipPulseReceiver* PulseReceiver;

    // Feedback reporter for bug reports and feature requests (lazy initialized)
    UPROPERTY()
    URshipFeedbackReporter* FeedbackReporter;

    // Visualization manager for fixture beam cones and gizmos (lazy initialized)
    UPROPERTY()
    URshipVisualizationManager* VisualizationManager;

    // Timecode sync for timeline integration (lazy initialized)
    UPROPERTY()
    URshipTimecodeSync* TimecodeSync;

    // Fixture library for GDTF profiles (lazy initialized)
    UPROPERTY()
    URshipFixtureLibrary* FixtureLibrary;

    // Multi-camera manager for switcher-style camera control (lazy initialized)
    UPROPERTY()
    URshipMultiCameraManager* MultiCameraManager;

    // Scene validator for pre-conversion checks (lazy initialized)
    UPROPERTY()
    URshipSceneValidator* SceneValidator;

    // Niagara manager for VFX pulse bindings (lazy initialized)
    UPROPERTY()
    URshipNiagaraManager* NiagaraManager;

    // Sequencer sync for timeline integration (lazy initialized)
    UPROPERTY()
    URshipSequencerSync* SequencerSync;

    // Material binding manager for reactive materials (lazy initialized)
    UPROPERTY()
    URshipMaterialManager* MaterialManager;

    // DMX output for real-world fixture control (lazy initialized)
    UPROPERTY()
    URshipDMXOutput* DMXOutput;

    // OSC bridge for external controller integration (lazy initialized)
    UPROPERTY()
    URshipOSCBridge* OSCBridge;

    // Live Link service for streaming data (lazy initialized)
    UPROPERTY()
    URshipLiveLinkService* LiveLinkService;

    // Audio manager for audio-reactive components (lazy initialized)
    UPROPERTY()
    URshipAudioManager* AudioManager;

    // Recorder for pulse recording/playback (lazy initialized)
    UPROPERTY()
    URshipRecorder* Recorder;

    // Control Rig manager for binding pulse data to Control Rigs (lazy initialized)
    UPROPERTY()
    URshipControlRigManager* ControlRigManager;

    // PCG manager for binding pulse data to PCG graphs (lazy initialized)
    // Note: Returns nullptr if PCG plugin is not enabled
    // Not a UPROPERTY because UHT requires full UCLASS definition which is only available when PCG plugin is enabled
    URshipPCGManager* PCGManager;

    // Spatial Audio manager for loudspeaker management and spatialization (lazy initialized)
    // Note: Returns nullptr if RshipSpatialAudio plugin is not enabled
    // Not a UPROPERTY because UHT requires full UCLASS definition which is only available when SpatialAudio plugin is enabled
    URshipSpatialAudioManager* SpatialAudioManager;

    // Connection state management
    ERshipConnectionState ConnectionState;
    int32 ReconnectAttempts;
    FTimerHandle QueueProcessTimerHandle;
    FTimerHandle ReconnectTimerHandle;
    FTimerHandle SubsystemTickTimerHandle;
    double LastTickTime;

    // Internal message handling
    void SetItem(FString itemType, TSharedPtr<FJsonObject> data, ERshipMessagePriority Priority = ERshipMessagePriority::Normal, const FString& CoalesceKey = TEXT(""));
    void SendTarget(Target* target);
    void SendAction(Action* action, FString targetId);
    void SendEmitter(EmitterContainer* emitter, FString targetId);
    void SendTargetStatus(Target* target, bool online);
    void ProcessMessage(const FString& message);

    // Queue a message through rate limiter (preferred method)
    void QueueMessage(TSharedPtr<FJsonObject> Payload, ERshipMessagePriority Priority = ERshipMessagePriority::Normal,
                      ERshipMessageType Type = ERshipMessageType::Generic, const FString& CoalesceKey = TEXT(""));

    // Direct send - only used by rate limiter callback
    void SendJsonDirect(const FString& JsonString);

    // Timer callbacks
    void ProcessMessageQueue();
    void AttemptReconnect();
    void TickSubsystems();

    // WebSocket event handlers
    void OnWebSocketConnected();
    void OnWebSocketConnectionError(const FString& Error);
    void OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void OnWebSocketMessage(const FString& Message);

    // Rate limiter event handlers
    void OnRateLimiterStatusChanged(bool bIsBackingOff, float BackoffSeconds);

    // Initialize rate limiter from settings
    void InitializeRateLimiter();

    // Schedule reconnection with backoff
    void ScheduleReconnect();

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;

    void Reconnect();
    void PulseEmitter(FString TargetId, FString EmitterId, TSharedPtr<FJsonObject> data);
    void SendAll();

    EmitterContainer* GetEmitterInfo(FString targetId, FString emitterId);

    TSet<URshipTargetComponent*>* TargetComponents;

    FString GetServiceId();

    // ========================================================================
    // TARGET GROUP MANAGEMENT
    // ========================================================================

    /** Get the group manager for organizing targets */
    UFUNCTION(BlueprintCallable, Category = "Rship|Groups")
    URshipTargetGroupManager* GetGroupManager();

    /** Get the health monitor for dashboard */
    UFUNCTION(BlueprintCallable, Category = "Rship|Health")
    URshipHealthMonitor* GetHealthMonitor();

    /** Get the preset manager for emitter state snapshots */
    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    URshipPresetManager* GetPresetManager();

    /** Get the template manager for target configuration templates */
    UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
    URshipTemplateManager* GetTemplateManager();

    /** Get the level manager for streaming level awareness */
    UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
    URshipLevelManager* GetLevelManager();

    /** Get the editor selection sync manager (Editor only) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Editor")
    URshipEditorSelection* GetEditorSelection();

    /** Get the Data Layer manager for World Partition workflows */
    UFUNCTION(BlueprintCallable, Category = "Rship|DataLayers")
    URshipDataLayerManager* GetDataLayerManager();

    /** Get the Fixture manager for lighting calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    URshipFixtureManager* GetFixtureManager();

    /** Get the Camera manager for camera calibration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    URshipCameraManager* GetCameraManager();

    /** Get the IES profile service for photometric data */
    UFUNCTION(BlueprintCallable, Category = "Rship|IES")
    URshipIESProfileService* GetIESProfileService();

    /** Get the scene converter for importing existing UE scenes */
    UFUNCTION(BlueprintCallable, Category = "Rship|SceneConversion")
    URshipSceneConverter* GetSceneConverter();

    /** Get the editor transform sync for automatic position synchronization */
    UFUNCTION(BlueprintCallable, Category = "Rship|Editor")
    URshipEditorTransformSync* GetEditorTransformSync();

    /** Get the pulse receiver for incoming fixture control values */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    URshipPulseReceiver* GetPulseReceiver();

    /** Get the feedback reporter for bug reports and feature requests */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    URshipFeedbackReporter* GetFeedbackReporter();

    /** Get the visualization manager for fixture beam cones and gizmos */
    UFUNCTION(BlueprintCallable, Category = "Rship|Visualization")
    URshipVisualizationManager* GetVisualizationManager();

    /** Get the timecode sync for timeline integration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Timecode")
    URshipTimecodeSync* GetTimecodeSync();

    /** Get the fixture library for GDTF profiles */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    URshipFixtureLibrary* GetFixtureLibrary();

    /** Get the multi-camera manager for switcher-style camera control */
    UFUNCTION(BlueprintCallable, Category = "Rship|Cameras")
    URshipMultiCameraManager* GetMultiCameraManager();

    /** Get the scene validator for pre-conversion checks */
    UFUNCTION(BlueprintCallable, Category = "Rship|Validation")
    URshipSceneValidator* GetSceneValidator();

    /** Get the Niagara manager for VFX pulse bindings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Niagara")
    URshipNiagaraManager* GetNiagaraManager();

    /** Get the sequencer sync for timeline integration */
    UFUNCTION(BlueprintCallable, Category = "Rship|Sequencer")
    URshipSequencerSync* GetSequencerSync();

    /** Get the material binding manager for reactive materials */
    UFUNCTION(BlueprintCallable, Category = "Rship|Materials")
    URshipMaterialManager* GetMaterialManager();

    /** Get the DMX output for real-world fixture control */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    URshipDMXOutput* GetDMXOutput();

    /** Get the OSC bridge for external controller integration */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    URshipOSCBridge* GetOSCBridge();

    /** Get the Live Link service for streaming data */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    URshipLiveLinkService* GetLiveLinkService();

    /** Get the Audio manager for audio-reactive components */
    UFUNCTION(BlueprintCallable, Category = "Rship|Audio")
    URshipAudioManager* GetAudioManager();

    /** Get the Recorder for pulse recording/playback */
    UFUNCTION(BlueprintCallable, Category = "Rship|Recording")
    URshipRecorder* GetRecorder();

    /** Get the Control Rig manager for binding pulse data to Control Rigs */
    UFUNCTION(BlueprintCallable, Category = "Rship|ControlRig")
    URshipControlRigManager* GetControlRigManager();

    /** Get the PCG manager for binding pulse data to PCG graphs.
     *  Note: Returns nullptr if PCG plugin is not enabled.
     *  Not exposed to Blueprint because UHT requires full UCLASS definition. */
    URshipPCGManager* GetPCGManager();

    /** Get the Spatial Audio manager for loudspeaker management and spatialization.
     *  Note: Returns nullptr if RshipSpatialAudio plugin is not enabled.
     *  Not exposed to Blueprint because UHT requires full UCLASS definition. */
    URshipSpatialAudioManager* GetSpatialAudioManager();

    // ========================================================================
    // SELECTION (for bulk operations)
    // ========================================================================

    /** Delegate fired when target selection changes (bind in Blueprint to update UI) */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Selection")
    FOnRshipSelectionChanged OnSelectionChanged;

    // ========================================================================
    // PUBLIC DIAGNOSTICS
    // These methods can be called from Blueprint or debug UI for monitoring
    // ========================================================================

    // Connection state
    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    bool IsConnected() const;

    // Queue metrics
    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetQueueLength() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetQueueBytes() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    float GetQueuePressure() const;

    // Throughput metrics
    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetMessagesSentPerSecond() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetBytesSentPerSecond() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetMessagesDropped() const;

    // Rate limiting state
    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    bool IsRateLimiterBackingOff() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    float GetBackoffRemaining() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    float GetCurrentRateLimit() const;

    // Reset statistics (useful for testing)
    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    void ResetRateLimiterStats();

    // Legacy compatibility - direct send (use sparingly, bypasses queue)
    void SendJson(TSharedPtr<FJsonObject> Payload);
};
