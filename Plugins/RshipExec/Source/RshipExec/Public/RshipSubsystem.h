// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RshipActorRegistrationComponent.h"
#include "GameFramework/Actor.h"

// Forward declaration for optional SpatialAudio plugin
class URshipSpatialAudioManager;
#if RSHIP_HAS_DISPLAY_CLUSTER
class UPrimitiveComponent;
class USceneComponent;
class ADisplayClusterRootActor;
#endif
#include "Containers/List.h"
#include "Containers/Ticker.h"
#include "Core/Target.h"
#include "Network/RshipRateLimiter.h"
#include "Network/RshipWebSocket.h"
#include "RshipSubsystem.generated.h"


DECLARE_DYNAMIC_DELEGATE(FRshipMessageDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipSelectionChanged);

// Connection state for tracking
// Connection state for tracking
enum class ERshipConnectionState : uint8
{
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    BackingOff
};

struct FRshipManagedTargetView
{
    FString Id;
    FString Name;
    TArray<FString> ParentTargetIds;
    int32 ActionCount = 0;
    int32 EmitterCount = 0;
    TWeakObjectPtr<URshipActorRegistrationComponent> BoundTargetComponent;
    bool bBoundToComponent = false;
};

struct FRshipPendingExecTargetAction
{
    FString TargetId;
    FString ActionId;
    TSharedPtr<FJsonObject> Data;
    TArray<FString> TxIds;
};

struct FRshipPendingBatchActionItem
{
    FString TargetId;
    FString ActionId;
    TSharedPtr<FJsonObject> Data;
};

struct FRshipPendingBatchTargetAction
{
    FString TxId;
    FString CommandId;
    TArray<FRshipPendingBatchActionItem> Actions;
};

enum class ERshipSyncQueryKind : uint8
{
    Targets,
    Actions,
    Emitters,
    TargetStatuses
};

struct FRshipPendingSyncQuery
{
    FString TxId;
    FString QueryId;
    FString QueryItemType;
    ERshipSyncQueryKind Kind = ERshipSyncQueryKind::Targets;
    double StartedAtSeconds = 0.0;
    bool bCompleted = false;
};

struct FRshipTopologySyncState
{
    bool bInFlight = false;
    FString Reason;
    FString PendingReason;
    double StartedAtSeconds = 0.0;
    TMap<FString, FRshipPendingSyncQuery> PendingQueries;
    TMap<FString, TSharedPtr<FJsonObject>> RemoteTargets;
    TMap<FString, TSharedPtr<FJsonObject>> RemoteActions;
    TMap<FString, TSharedPtr<FJsonObject>> RemoteEmitters;
    TMap<FString, TSharedPtr<FJsonObject>> RemoteTargetStatuses;
};

struct FRshipTopologySyncSnapshot
{
    bool bInFlight = false;
    bool bLastSyncSucceeded = false;
    FString Reason;
    FString Detail;
    double StartedAtSeconds = 0.0;
    double CompletedAtSeconds = 0.0;
    int32 LocalTargets = 0;
    int32 RemoteTargets = 0;
    int32 SentTargets = 0;
    int32 LocalActions = 0;
    int32 RemoteActions = 0;
    int32 SentActions = 0;
    int32 LocalEmitters = 0;
    int32 RemoteEmitters = 0;
    int32 SentEmitters = 0;
    int32 LocalTargetStatuses = 0;
    int32 RemoteTargetStatuses = 0;
    int32 SentTargetStatuses = 0;
};

/**
 * Main subsystem for managing Rocketship WebSocket connection and message routing.
 * Uses a lossless outbound queue while disconnected and direct sends while connected.
 */
UCLASS()
class RSHIPEXEC_API URshipSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

    // Friend classes that need access to private methods (SetItem, SendJsonDirect, SendTargetStatus, DeleteTarget)
    friend class URshipSpatialAudioManager;
    friend class URshipActorRegistrationComponent;
    friend class Target;

    // High-performance WebSocket connection
    TSharedPtr<FRshipWebSocket> WebSocket;
    bool bIsManuallyReconnecting = false;                // Prevents auto-reconnect during manual reconnect
    bool bRemoteCommunicationEnabled = true;             // Global hard gate for all remote server communication

    FString InstanceId;
    FString ServiceId;
    FString MachineId;

    FString ClientId;
    FString ClusterId;

    // Legacy rate limiter object retained only to avoid wider refactors; not used for runtime flow.
    TUniquePtr<FRshipRateLimiter> RateLimiter;
    // Lossless outbound queue used only while disconnected or when a send attempt fails.
    TArray<FRshipQueuedMessage> PendingOutboundMessages;
    int32 PendingOutboundBytes = 0;
    // Spatial Audio manager for loudspeaker management and spatialization (lazy initialized)
    // Note: Returns nullptr if RshipSpatialAudio plugin is not enabled
    // Not a UPROPERTY because UHT requires full UCLASS definition which is only available when SpatialAudio plugin is enabled
    URshipSpatialAudioManager* SpatialAudioManager;

    // Connection state management
    ERshipConnectionState ConnectionState;
    int32 ReconnectAttempts;
    int32 DisplayClusterNodeResolveRetries = 0;
    FTSTicker::FDelegateHandle QueueProcessTickerHandle;
    FTSTicker::FDelegateHandle ReconnectTickerHandle;
    FTSTicker::FDelegateHandle SubsystemTickerHandle;
    FTSTicker::FDelegateHandle ConnectionTimeoutTickerHandle;
    FTSTicker::FDelegateHandle DeferredOnDataReceivedTickerHandle;
    double LastTickTime;
    // Components that had successful Take() calls this frame; flushed once per tick.
    TSet<TWeakObjectPtr<URshipActorRegistrationComponent>> PendingOnDataReceivedComponents;
    TMap<FString, FRshipPendingExecTargetAction> PendingExecTargetActions;
    TArray<FRshipPendingBatchTargetAction> PendingBatchTargetActions;

    struct FManagedTargetSnapshot
    {
        FString Id;
        FString Name;
        TArray<FString> ParentTargetIds;
        TSet<FString> ActionIds;
        TSet<FString> EmitterIds;
        TWeakObjectPtr<URshipActorRegistrationComponent> BoundTargetComponent;
        bool bBoundToComponent = false;
    };
    TMap<Target*, FManagedTargetSnapshot> ManagedTargetSnapshots;
    TMultiMap<FString, Target*> RegisteredTargetsById;
    TMap<FString, TUniquePtr<Target>> AutomationOwnedTargets;

#if RSHIP_HAS_DISPLAY_CLUSTER
    struct FDisplayClusterRenderDomain
    {
        FString ViewportId;
        FString ProjectionType;
        TWeakObjectPtr<UPrimitiveComponent> ProjectionComponent;
        TWeakObjectPtr<USceneComponent> ViewPointComponent;
    };

    TWeakObjectPtr<ADisplayClusterRootActor> CachedDisplayClusterRootActor;
    FString CachedDisplayClusterNodeId;
    bool bHasLocalDisplayClusterNodeConfig = false;
    TArray<FDisplayClusterRenderDomain> CachedRenderDomains;
    double LastRenderDomainRefreshTimeSeconds = 0.0;
    double LastRenderDomainPublishTimeSeconds = 0.0;

    TSharedPtr<FJsonObject> BuildRenderDomainJson() const;
    void RefreshRenderDomains();
    void PublishRenderDomains();
    void UpdateRenderDomainMetadata();
#endif
    TSharedPtr<FJsonObject> BuildCoordinateSpaceJson() const;

    // Ticker callbacks (return true to keep ticking, false to stop)
    bool OnQueueProcessTick(float DeltaTime);
    bool OnReconnectTick(float DeltaTime);
    bool OnSubsystemTick(float DeltaTime);
    bool OnConnectionTimeoutTick(float DeltaTime);
    bool OnDeferredOnDataReceivedTick(float DeltaTime);

    // Internal message handling
    void SetItem(FString itemType, TSharedPtr<FJsonObject> data, ERshipMessagePriority Priority = ERshipMessagePriority::Normal, const FString& CoalesceKey = TEXT(""));
    void SendInstanceInfo();  // Send Machine and Instance only (called once on connect)
    void SendTarget(Target* target);
    void DeleteTarget(Target* target);
    void SendAction(const FRshipActionProxy& action, FString targetId);
    void SendEmitter(const FRshipEmitterProxy& emitter, FString targetId);
    void SendTargetStatus(Target* target, bool online);
    void QueueEventBatch(const TArray<TSharedPtr<FJsonObject>>& Events,
                         ERshipMessagePriority Priority,
                         ERshipMessageType Type,
                         const FString& CoalesceKey);
    void ProcessMessage(const FString& message);

    // Queue a message through rate limiter (preferred method)
    void QueueMessage(TSharedPtr<FJsonObject> Payload, ERshipMessagePriority Priority = ERshipMessagePriority::Normal,
                      ERshipMessageType Type = ERshipMessageType::Generic, const FString& CoalesceKey = TEXT(""));

    // Direct send - only used by rate limiter callback
    bool SendJsonDirect(const FString& JsonString);

    // Timer callbacks
    void ProcessMessageQueue();
    void FlushPendingRegistrationBatch();
    void AttemptReconnect();
    void TickSubsystems();
    void OnConnectionTimeout();
    void FlushPendingOnDataReceived();
    void EnqueueExecTargetAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data, const FString& TxId);
    void EnqueueBatchTargetAction(const FString& TxId, TArray<FRshipPendingBatchActionItem>&& Actions, const FString& CommandId = TEXT("BatchTargetAction"));
    void ProcessPendingExecTargetActions();
    void QueueCommandResponse(const FString& TxId, bool bOk, const FString& CommandId, const FString& ErrorMessage = TEXT(""));

    // WebSocket event handlers
    void OnWebSocketConnected();
    void OnWebSocketConnectionError(const FString& Error);
    void OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void OnWebSocketMessage(const FString& Message);
    void OnWebSocketBinaryMessage(const TArray<uint8>& Message);

    void MaybeLogWebSocketSendStats();
    void StartTopologySync(const FString& Reason);
    bool SendQueryRequest(const FString& QueryId, const FString& QueryItemType, const TSharedRef<FJsonObject>& QueryPayload, ERshipSyncQueryKind Kind);
    void HandleQueryResponse(const TSharedPtr<FJsonObject>& DataObj);
    void HandleQueryError(const TSharedPtr<FJsonObject>& DataObj);
    void CancelQuerySubscription(const FString& TxId);
    void FailTopologySync(const FString& Reason);
    void CompleteTopologySyncIfReady();
    void FlushTopologyDiff();
    void CheckTopologySyncTimeout();

    // Initialize outbound queue behavior
    void InitializeRateLimiter();
    FManagedTargetSnapshot BuildManagedTargetSnapshot(Target* ManagedTarget) const;
    int32 PruneInvalidManagedTargetRefs(const FString& TargetId);

    // Schedule reconnection with backoff
    void ScheduleReconnect();

    // Registration batching state
    int32 RegistrationBatchDepth = 0;
    TArray<TSharedPtr<FJsonObject>> PendingRegistrationEvents;

    // Rolling websocket send stats for diagnosing replay/backpressure behavior.
    double LastWebSocketSendStatsLogTime = 0.0;
    int64 WebSocketSendAttemptsSinceLastLog = 0;
    int64 WebSocketSendSuccessSinceLastLog = 0;
    int64 WebSocketSendFailuresSinceLastLog = 0;
    int64 WebSocketSendBytesSinceLastLog = 0;
    int32 MessagesSentPerSecondSnapshot = 0;
    int32 BytesSentPerSecondSnapshot = 0;
    FRshipTopologySyncState TopologySyncState;
    FRshipTopologySyncSnapshot TopologySyncSnapshot;

#if WITH_EDITOR
    void RegisterEditorDelegates();
    void UnregisterEditorDelegates();
    void RefreshAllTargetComponents(const TCHAR* Reason);
    void HandleBeginPIE(bool bIsSimulating);
    void HandleEndPIE(bool bIsSimulating);
    void HandleMapChanged(uint32 MapChangeFlags);
    void HandleBlueprintCompiled();

    FDelegateHandle BeginPIEHandle;
    FDelegateHandle EndPIEHandle;
    FDelegateHandle MapChangedHandle;
    FDelegateHandle BlueprintCompiledHandle;
#endif

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;
    virtual void BeginDestroy() override;

    /** Clean up tickers and connections before hot reload. Called by module ShutdownModule. */
    void PrepareForHotReload();

    /** Reinitialize tickers and connections after hot reload. Called by module StartupModule. */
    void ReinitializeAfterHotReload();

    // ========================================================================
    // CONNECTION MANAGEMENT
    // ========================================================================

    /** Reconnect to the rship server using current settings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Connection")
    void Reconnect();

    /** Connect to a specific server (updates settings and reconnects) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Connection")
    void ConnectTo(const FString& Host, int32 Port);

    /** Get the current server address from settings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Connection")
    FString GetServerAddress() const;

    /** Get the current server port from settings */
    UFUNCTION(BlueprintCallable, Category = "Rship|Connection")
    int32 GetServerPort() const;

    /** Enable/disable all remote server communication. Disabling immediately disconnects and blocks reconnect/send. */
    UFUNCTION(BlueprintCallable, Category = "Rship|Connection")
    void SetRemoteCommunicationEnabled(bool bEnabled);
    /** Whether remote server communication is globally enabled. */
    UFUNCTION(BlueprintCallable, Category = "Rship|Connection")
    bool IsRemoteCommunicationEnabled() const { return bRemoteCommunicationEnabled; }
    /** Rebuild all tracked target/action/emitter registrations and republish the current cache. */
    UFUNCTION(BlueprintCallable, Category = "Rship|Connection")
    void RefreshTargetCache();

    void PulseEmitter(FString TargetId, FString EmitterId, TSharedPtr<FJsonObject> data);
	void SendAll();

	const FRshipEmitterProxy* GetEmitterInfo(FString targetId, FString emitterId);

	// Identity-first registration helpers.
	FRshipTargetProxy EnsureTargetIdentity(const FString& FullTargetId, const FString& DisplayName = TEXT(""),
		const TArray<FString>& ParentTargetIds = {});
	FRshipTargetProxy EnsureActorIdentity(AActor* Actor);
	FRshipTargetProxy GetTargetProxyForActor(AActor* Actor);

	// Managed target lifecycle (owned by subsystem, fed by automation components/controllers)
	void RegisterManagedTarget(Target* ManagedTarget);
    void UnregisterManagedTarget(Target* ManagedTarget);
    void OnManagedTargetChanged(Target* ManagedTarget);
    Target* EnsureAutomationTarget(const FString& FullTargetId, const FString& Name, const TArray<FString>& ParentTargetIds);
    bool RemoveAutomationTarget(const FString& FullTargetId);
    bool RegisterFunctionActionForTarget(const FString& FullTargetId, UObject* Owner, const FName& FunctionName, const FString& ExposedActionName = TEXT(""));
    bool RegisterPropertyActionForTarget(const FString& FullTargetId, UObject* Owner, const FName& PropertyName, const FString& ExposedActionName = TEXT(""));
    bool RegisterEmitterForTarget(const FString& FullTargetId, UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName = TEXT(""));
    void GetManagedTargetsSnapshot(TArray<FRshipManagedTargetView>& OutTargets) const;

    // Target component registry - keyed by full target ID for O(1) lookups
    // Key format: "ServiceId:TargetName"
    // Uses TMultiMap to allow multiple components with the same target ID
    // (e.g., during actor duplication before re-ID)
    TMultiMap<FString, URshipActorRegistrationComponent*>* TargetComponents;

    // Register a target component (called by URshipActorRegistrationComponent)
    void RegisterTargetComponent(URshipActorRegistrationComponent* Component);

    // Unregister a target component (called by URshipActorRegistrationComponent)
    void UnregisterTargetComponent(URshipActorRegistrationComponent* Component);

    // Find a target component by full target ID - returns first match (O(1) lookup)
    URshipActorRegistrationComponent* FindTargetComponent(const FString& FullTargetId) const;

    // Find all target components with the given target ID
    TArray<URshipActorRegistrationComponent*> FindAllTargetComponents(const FString& FullTargetId) const;

    // Execute an action using the same routing/guards as server commands.
    bool ExecuteTargetAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data);

    // Queue OnRshipData broadcast for end-of-frame dispatch.
    void QueueOnDataReceived(URshipActorRegistrationComponent* Component);

    FString GetServiceId();
    FString GetInstanceId();
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

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    uint8 GetConnectionStateValue() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    bool IsTopologySyncInFlight() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    FString GetTopologySyncReason() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    FString GetTopologySyncDetail() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    float GetTopologySyncAgeSeconds() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetLocalTargetCount() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetRemoteTargetCount() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetLocalActionCount() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetRemoteActionCount() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetLocalEmitterCount() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetRemoteEmitterCount() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetLocalTargetStatusCount() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Diagnostics")
    int32 GetRemoteTargetStatusCount() const;

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

    // Registration batching (for multi-target component registration)
    void BeginRegistrationBatch();
    void EndRegistrationBatch();

    // Legacy compatibility - direct send (use sparingly, bypasses queue)
    void SendJson(TSharedPtr<FJsonObject> Payload);
};
