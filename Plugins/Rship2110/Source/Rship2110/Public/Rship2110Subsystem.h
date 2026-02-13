// Copyright Rocketship. All Rights Reserved.
// Main Subsystem for SMPTE 2110 / PTP / IPMX Integration
//
// This is the primary entry point for 2110 streaming functionality.
// It orchestrates:
// - PTP time synchronization
// - Rivermax device management
// - Video/Audio/Ancillary stream lifecycle
// - IPMX registration and discovery
//
// Integrates with the existing RshipSubsystem for timecode sync.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tickable.h"
#include "Rship2110Types.h"
#include "Rship2110Subsystem.generated.h"

// Forward declarations
class URshipPTPService;
class URivermaxManager;
class URshipIPMXService;
class URship2110VideoSender;
class URship2110VideoCapture;
class URship2110Settings;
class URshipSubsystem;
class UTextureRenderTarget2D;

/**
 * Main subsystem for SMPTE 2110 streaming.
 *
 * Provides a unified API for:
 * - PTP-disciplined timing
 * - Rivermax-based 2110 streaming
 * - IPMX/NMOS discovery and registration
 *
 * Automatically initializes based on project settings and integrates
 * with the existing Rship subsystem for timecode synchronization.
 */
UCLASS()
class RSHIP2110_API URship2110Subsystem : public UEngineSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // ========================================================================
    // UEngineSubsystem Interface
    // ========================================================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // ========================================================================
    // FTickableGameObject Interface
    // ========================================================================

    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return bIsInitialized; }

    // ========================================================================
    // SERVICE ACCESS
    // ========================================================================

    /**
     * Get the PTP service for time synchronization.
     * @return PTP service or nullptr if not enabled
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URshipPTPService* GetPTPService() const { return PTPService; }

    /**
     * Get the Rivermax manager for device and stream management.
     * @return Rivermax manager or nullptr if not available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URivermaxManager* GetRivermaxManager() const { return RivermaxManager; }

    /**
     * Get the IPMX service for discovery and registration.
     * @return IPMX service or nullptr if not enabled
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URshipIPMXService* GetIPMXService() const { return IPMXService; }

    /**
     * Get the video capture helper.
     * @return Video capture or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URship2110VideoCapture* GetVideoCapture() const { return VideoCapture; }

    /**
     * Get the Rship main subsystem (for timecode integration).
     * @return Rship subsystem or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URshipSubsystem* GetRshipSubsystem() const;

    // ========================================================================
    // QUICK ACCESS - PTP
    // ========================================================================

    /**
     * Get current PTP time.
     * @return PTP timestamp or zero if not locked
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|PTP")
    FRshipPTPTimestamp GetPTPTime() const;

    /**
     * Check if PTP is locked to grandmaster.
     * @return true if locked
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|PTP")
    bool IsPTPLocked() const;

    /**
     * Get PTP status.
     * @return PTP status
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|PTP")
    FRshipPTPStatus GetPTPStatus() const;

    // ========================================================================
    // QUICK ACCESS - STREAMS
    // ========================================================================

    /**
     * Create and start a video sender stream.
     * Convenience method that creates stream, registers with IPMX, and starts.
     * @param VideoFormat Video format
     * @param TransportParams Transport parameters
     * @param bAutoRegisterIPMX Auto-register with IPMX
     * @return Stream ID or empty string on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    FString CreateVideoStream(
        const FRship2110VideoFormat& VideoFormat,
        const FRship2110TransportParams& TransportParams,
        bool bAutoRegisterIPMX = true);

    /**
     * Stop and destroy a video stream.
     * @param StreamId Stream to destroy
     * @return true if destroyed
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool DestroyVideoStream(const FString& StreamId);

    /**
     * Get a video sender by ID.
     * @param StreamId Stream ID
     * @return Video sender or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    URship2110VideoSender* GetVideoSender(const FString& StreamId) const;

    /**
     * Get all active stream IDs.
     * @return Array of stream IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    TArray<FString> GetActiveStreamIds() const;

    /**
     * Start streaming on a video sender.
     * @param StreamId Stream ID
     * @return true if started
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool StartStream(const FString& StreamId);

    /**
     * Stop streaming on a video sender.
     * @param StreamId Stream ID
     * @return true if stopped
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool StopStream(const FString& StreamId);

    /**
     * Bind a video stream to an RshipExec render context.
     * The stream uses the context's resolved render target every tick.
     * @param StreamId Stream ID to bind
     * @param RenderContextId Render context ID from content mapping
     * @return true if binding was created
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool BindVideoStreamToRenderContext(const FString& StreamId, const FString& RenderContextId);

    /**
     * Bind a video stream to an RshipExec render context with an explicit capture region.
     * @param StreamId Stream ID to bind
     * @param RenderContextId Render context ID from content mapping
     * @param CaptureRect Capture region in source render target pixels (min inclusive, max exclusive)
     * @return true if binding was created
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool BindVideoStreamToRenderContextWithRect(const FString& StreamId, const FString& RenderContextId, const FIntRect& CaptureRect);

    /**
     * Remove any render context binding from a stream.
     * @param StreamId Stream ID to unbind
     * @return true if binding existed
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    bool UnbindVideoStreamFromRenderContext(const FString& StreamId);

    /**
     * Query which render context is currently bound to a stream.
     * @param StreamId Stream ID
     * @return Bound render context ID (empty if unbound)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Streams")
    FString GetBoundRenderContextForStream(const FString& StreamId) const;

    /**
     * Query full stream-to-context binding metadata.
     * @param StreamId Stream ID
     * @param OutRenderContextId Output render context ID
     * @param OutCaptureRect Output capture rectangle in source render target pixels
     * @param bOutUseCaptureRect true when stream is bound to a clipped region
     * @return true if stream is currently bound
     */
    bool GetBoundRenderContextBinding(const FString& StreamId, FString& OutRenderContextId, FIntRect& OutCaptureRect, bool& bOutUseCaptureRect) const;

    // ========================================================================
    // QUICK ACCESS - IPMX
    // ========================================================================

    /**
     * Connect to IPMX registry.
     * @param RegistryUrl Registry URL (empty = auto-discover)
     * @return true if connection initiated
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|IPMX")
    bool ConnectIPMX(const FString& RegistryUrl = TEXT(""));

    /**
     * Disconnect from IPMX registry.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|IPMX")
    void DisconnectIPMX();

    /**
     * Check if connected to IPMX registry.
     * @return true if connected
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|IPMX")
    bool IsIPMXConnected() const;

    /**
     * Get IPMX status.
     * @return IPMX status
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|IPMX")
    FRshipIPMXStatus GetIPMXStatus() const;

    // ========================================================================
    // QUICK ACCESS - RIVERMAX
    // ========================================================================

    /**
     * Get Rivermax status.
     * @return Rivermax status
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Rivermax")
    FRshipRivermaxStatus GetRivermaxStatus() const;

    /**
     * Get available Rivermax devices.
     * @return Array of devices
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Rivermax")
    TArray<FRshipRivermaxDevice> GetRivermaxDevices() const;

    /**
     * Select Rivermax device by IP.
     * @param IPAddress Device IP
     * @return true if selected
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Rivermax")
    bool SelectRivermaxDevice(const FString& IPAddress);

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * Get settings.
     * @return Settings object
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    URship2110Settings* GetSettings() const;

    /**
     * Check if subsystem is fully initialized.
     * @return true if initialized
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsInitialized() const { return bIsInitialized; }

    /**
     * Check if Rivermax SDK is available.
     * @return true if available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsRivermaxAvailable() const;

    /**
     * Check if PTP is available.
     * @return true if available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsPTPAvailable() const;

    /**
     * Check if IPMX is available.
     * @return true if available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsIPMXAvailable() const;

    // ========================================================================
    // CLUSTER CONTROL
    // ========================================================================

    /**
     * Get resolved local cluster node ID used for ownership decisions.
     * Resolution order: explicit override, command line dc_node, env var, machine name.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    FString GetLocalClusterNodeId() const { return LocalClusterNodeId; }

    /**
     * Override local cluster node ID at runtime.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    void SetLocalClusterNodeId(const FString& NodeId);

    /**
     * Get current local cluster role from active cluster state.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    ERship2110ClusterRole GetLocalClusterRole() const;

    /**
     * Check if this node is currently authority for cluster control state.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool IsLocalNodeAuthority() const;

    /**
     * Frame counter used for frame-indexed state application.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    int64 GetClusterFrameCounter() const { return ClusterFrameCounter; }

    /** Get frame counter for a specific sync domain (empty = default). */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    int64 GetClusterFrameCounterForDomain(const FString& SyncDomainId) const;

    /** Set shared default cluster sync rate in Hz (live). */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    void SetClusterSyncRateHz(float InSyncRateHz);

    /** Get shared default cluster sync rate in Hz. */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    float GetClusterSyncRateHz() const { return ClusterSyncRateHz; }

    /** Set local render pacing multiplier (live). */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    void SetLocalRenderSubsteps(int32 InSubsteps);

    /** Get local render pacing multiplier. */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    int32 GetLocalRenderSubsteps() const { return LocalRenderSubsteps; }

    /** Set max sync catch-up steps per engine tick (live). */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    void SetMaxSyncCatchupSteps(int32 InMaxSteps);

    /** Get max sync catch-up steps. */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    int32 GetMaxSyncCatchupSteps() const { return MaxSyncCatchupSteps; }

    /** Set current active sync domain used for local authoritative outbound payloads. */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    void SetActiveSyncDomainId(const FString& SyncDomainId);

    /** Get current active sync domain id used for local authoritative outbound payloads. */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    FString GetActiveSyncDomainId() const { return ActiveSyncDomainId; }

    /** Set a specific sync domain frame rate in Hz (non-default domains can run independently). */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool SetSyncDomainRateHz(const FString& SyncDomainId, float SyncRateHz);

    /** Get a specific sync domain frame rate in Hz. */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    float GetSyncDomainRateHz(const FString& SyncDomainId) const;

    /** List known sync domains. */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    TArray<FString> GetSyncDomainIds() const;

    /**
     * Queue an authoritative cluster state update to apply at ClusterState.ApplyFrame.
     * Update is ignored if stale (older epoch/version).
     * @return true if accepted
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool QueueClusterStateUpdate(const FRship2110ClusterState& ClusterState);

    /**
     * Get active cluster state.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    FRship2110ClusterState GetClusterState() const { return ActiveClusterState; }

    /**
     * Set stream ownership to a target node.
     * @param StreamId Stream identifier
     * @param OwnerNodeId Owning node identifier
     * @param bApplyNextFrame Apply on next frame (true) or immediately (false)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    void SetClusterOwnershipForStream(const FString& StreamId, const FString& OwnerNodeId, bool bApplyNextFrame = true);

    /**
     * Update failover and ownership enforcement flags in authoritative cluster state.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool UpdateClusterFailoverConfig(
        bool bFailoverEnabled,
        bool bAllowAutoPromotion,
        float FailoverTimeoutSeconds,
        bool bStrictNodeOwnership,
        bool bApplyNextFrame = true);

    /**
     * Get owner node for a stream (empty when unassigned).
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    FString GetClusterOwnershipForStream(const FString& StreamId) const;

    /**
     * Get streams currently owned by local node.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    TArray<FString> GetLocallyOwnedStreams() const;

    /**
     * Record authority heartbeat for failover monitoring.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    void NotifyClusterAuthorityHeartbeat(const FString& AuthorityNodeId, int32 Epoch, int32 Version);

    /**
     * Promote local node to authority and advance epoch.
     * @param bApplyNextFrame Apply authority handoff on next frame (true) or immediately (false)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    void PromoteLocalNodeToPrimary(bool bApplyNextFrame = true);

    /**
     * Compute deterministic hash for a cluster state payload.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    FString ComputeClusterStateHash(const FRship2110ClusterState& ClusterState) const;

    /**
     * Authority entrypoint for prepare phase.
     * Broadcasts OnClusterPrepareOutbound on success.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool SubmitAuthorityClusterStatePrepare(FRship2110ClusterState ClusterState, bool bAutoCommitOnQuorum = true);

    /**
     * Receive prepare message from transport.
     * If accepted, emits OnClusterAckOutbound.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool ReceiveClusterStatePrepare(const FRship2110ClusterPrepareMessage& PrepareMessage);

    /**
     * Receive prepare ACK from transport.
     * Authority may emit OnClusterCommitOutbound when quorum is met.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool ReceiveClusterStateAck(const FRship2110ClusterAckMessage& AckMessage);

    /**
     * Receive commit message from transport.
     * Queues frame-indexed application of the previously prepared state.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool ReceiveClusterStateCommit(const FRship2110ClusterCommitMessage& CommitMessage);

    /**
     * Authority entrypoint for deterministic control payload replication.
     * Emits OnClusterDataOutbound for transport forwarding.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool SubmitAuthorityClusterDataMessage(const FString& Payload, int64 ApplyFrame = -1);

    /** Authority entrypoint for deterministic control payload replication with explicit sync domain id. */
    bool SubmitAuthorityClusterDataMessageForDomain(const FString& Payload, const FString& SyncDomainId, int64 ApplyFrame = INDEX_NONE);

    /**
     * Receive replicated authoritative control payload from transport.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110|Cluster")
    bool ReceiveClusterDataMessage(const FRship2110ClusterDataMessage& DataMessage);

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when PTP state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOnPTPStateChanged OnPTPStateChanged;

    /** Fired when a stream state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110StreamStateChanged OnStreamStateChanged;

    /** Fired when IPMX connection state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOnIPMXConnectionStateChanged OnIPMXConnectionStateChanged;

    /** Fired when Rivermax device changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOnRivermaxDeviceChanged OnRivermaxDeviceChanged;

    /** Fired when cluster control state is applied */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110ClusterStateApplied OnClusterStateApplied;

    /** Fired when a prepare message should be sent via cluster transport */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110ClusterPrepareOutbound OnClusterPrepareOutbound;

    /** Fired when an ACK message should be sent via cluster transport */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110ClusterAckOutbound OnClusterAckOutbound;

    /** Fired when a commit message should be sent via cluster transport */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110ClusterCommitOutbound OnClusterCommitOutbound;

    /** Fired when a deterministic control payload should be sent via cluster transport */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110ClusterDataOutbound OnClusterDataOutbound;

    /** Fired when a deterministic control payload is applied locally */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110ClusterDataApplied OnClusterDataApplied;

private:
    // Services
    UPROPERTY()
    URshipPTPService* PTPService = nullptr;

    UPROPERTY()
    URivermaxManager* RivermaxManager = nullptr;

    UPROPERTY()
    URshipIPMXService* IPMXService = nullptr;

    UPROPERTY()
    URship2110VideoCapture* VideoCapture = nullptr;

    // State
    bool bIsInitialized = false;
    FString LocalClusterNodeId;
    FRship2110ClusterState ActiveClusterState;
    TArray<FRship2110ClusterState> PendingClusterStates;
    struct FRship2110SyncDomainRuntime
    {
        int64 FrameCounter = 0;
        double FrameAccumulator = 0.0;
        float SyncRateHz = 60.0f;
        TArray<FRship2110ClusterDataMessage> PendingDataMessages;
        TMap<FString, int64> LastAppliedSequenceByAuthority;
    };

    TMap<FString, FRship2110SyncDomainRuntime> SyncDomains;
    FString DefaultSyncDomainId = TEXT("default");
    FString ActiveSyncDomainId = TEXT("default");
    TMap<FString, FString> StreamOwnerNodeCache; // Stream ID -> Owner node ID
    int64 LocalRenderStepCounter = 0;
    int64 ClusterFrameCounter = 0;
    int64 ClusterDataSequenceCounter = 0;
    double ClusterSyncFrameAccumulator = 0.0;
    float ClusterSyncRateHz = 60.0f;
    int32 LocalRenderSubsteps = 1;
    int32 MaxSyncCatchupSteps = 4;
    double LastAuthorityHeartbeatTime = 0.0;
    FDelegateHandle RshipAuthoritativeInboundHandle;

    struct FPreparedClusterStateEntry
    {
        FRship2110ClusterPrepareMessage Prepare;
        TSet<FString> AckedNodeIds;
        bool bAutoCommitOnQuorum = false;
        bool bCommitBroadcast = false;
        double CreatedTimeSeconds = 0.0;
    };
    TMap<FString, FPreparedClusterStateEntry> PreparedClusterStates; // key: epoch:version:hash

    struct FRship2110RenderContextBinding
    {
        FString RenderContextId;
        FIntRect CaptureRect = FIntRect(0, 0, 0, 0);
        bool bUseCaptureRect = false;
    };

    // Stream to IPMX mapping
    TMap<FString, FString> StreamToIPMXSender;  // Stream ID -> IPMX Sender ID
    // Stream to content context mapping
    TMap<FString, FRship2110RenderContextBinding> StreamToContextBinding;  // Stream ID -> Context binding

    // Initialization helpers
    void InitializePTPService();
    void InitializeRivermaxManager();
    void InitializeIPMXService();
    void InitializeVideoCapture();
    void InitializeClusterState();
    void RefreshStreamRenderContextBindings();
    bool ResolveRenderContextRenderTarget(const FString& ContextId, UTextureRenderTarget2D*& OutRenderTarget);
    FString ResolveLocalClusterNodeId() const;
    void ProcessPendingClusterStates();
    void ProcessPendingClusterDataMessages();
    void ProcessPendingClusterDataMessagesForDomain(const FString& SyncDomainId, FRship2110SyncDomainRuntime& DomainRuntime);
    FString ResolveSyncDomainId(const FString& SyncDomainId) const;
    FRship2110SyncDomainRuntime& GetOrCreateSyncDomain(const FString& SyncDomainId);
    const FRship2110SyncDomainRuntime* FindSyncDomain(const FString& SyncDomainId) const;
    void TickNonDefaultSyncDomains(float DeltaTime);
    bool ApplyClusterStateNow(const FRship2110ClusterState& ClusterState);
    bool IsClusterStateStale(const FRship2110ClusterState& ClusterState) const;
    FString MakePreparedStateKey(int32 Epoch, int32 Version, const FString& StateHash) const;
    FString BuildClusterStateCanonicalString(const FRship2110ClusterState& ClusterState) const;
    int32 GetDiscoveredClusterNodeCount(const FRship2110ClusterState& ClusterState) const;
    int32 ResolveRequiredAckCount(const FRship2110ClusterPrepareMessage& PrepareMessage) const;
    bool HasPrepareAckQuorum(const FPreparedClusterStateEntry& PreparedEntry) const;
    bool FinalizePreparedStateCommit(const FString& StateKey);
    void PurgeExpiredPreparedStates();
    void RebuildStreamOwnershipCache();
    bool IsStreamOwnedByLocalNode(const FString& StreamId) const;
    FString GetFailoverCandidateNodeId(const FRship2110ClusterState& ClusterState) const;
    void EvaluateClusterFailover();
    void HandleAuthoritativeRshipInbound(const FString& Payload, int64 SuggestedApplyFrame);

    // Event handlers (UFUNCTION required for AddDynamic)
    UFUNCTION()
    void OnPTPStateChangedInternal(ERshipPTPState NewState);
    UFUNCTION()
    void OnStreamStateChangedInternal(const FString& StreamId, ERship2110StreamState NewState);
    UFUNCTION()
    void OnIPMXStateChangedInternal(ERshipIPMXConnectionState NewState);
    UFUNCTION()
    void OnRivermaxDeviceChangedInternal(int32 DeviceIndex, const FRshipRivermaxDevice& Device);
};

// ============================================================================
// BLUEPRINT FUNCTION LIBRARY
// ============================================================================

/**
 * Blueprint function library for 2110 functionality.
 * Provides static functions for convenient Blueprint access.
 */
UCLASS()
class RSHIP2110_API URship2110BlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Get the 2110 subsystem.
     * @return Subsystem or nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110", meta = (WorldContext = "WorldContextObject"))
    static URship2110Subsystem* GetRship2110Subsystem(UObject* WorldContextObject);

    /**
     * Get current PTP time as seconds.
     * @return PTP time in seconds since epoch
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|PTP", meta = (WorldContext = "WorldContextObject"))
    static double GetPTPTimeSeconds(UObject* WorldContextObject);

    /**
     * Check if PTP is locked.
     * @return true if locked to grandmaster
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|PTP", meta = (WorldContext = "WorldContextObject"))
    static bool IsPTPLocked(UObject* WorldContextObject);

    /**
     * Convert frame rate to frame duration in nanoseconds.
     * @param FrameRate Frame rate
     * @return Duration in nanoseconds
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|Util")
    static int64 FrameRateToNanoseconds(const FFrameRate& FrameRate);

    /**
     * Convert video format to bitrate in Mbps.
     * @param VideoFormat Video format
     * @return Bitrate in Mbps
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|Util")
    static double VideoFormatToBitrate(const FRship2110VideoFormat& VideoFormat);

    /**
     * Create default video format for common resolution.
     * @param Width Resolution width
     * @param Height Resolution height
     * @param FrameRate Frame rate
     * @return Default video format
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|Util")
    static FRship2110VideoFormat CreateVideoFormat(
        int32 Width,
        int32 Height,
        const FFrameRate& FrameRate);

    /**
     * Create default transport params for multicast.
     * @param MulticastIP Multicast IP address
     * @param Port UDP port
     * @return Default transport params
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110|Util")
    static FRship2110TransportParams CreateTransportParams(
        const FString& MulticastIP,
        int32 Port = 5004);
};
