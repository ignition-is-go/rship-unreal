// Copyright Rocketship. All Rights Reserved.
// IPMX / NMOS Discovery and Registration Service
//
// Implements AIMS IPMX profile based on AMWA NMOS specifications:
// - IS-04: Discovery and Registration
// - IS-05: Connection Management
//
// Key features:
// - Node/Device/Sender resource registration with NMOS registry
// - SDP manifest generation and serving
// - Connection management (sender-side)
// - mDNS-SD discovery fallback
// - Heartbeat maintenance

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Rship2110Types.h"
#include "Http.h"
#include "RshipIPMXService.generated.h"

class URship2110Subsystem;
class URship2110VideoSender;

/**
 * IPMX / NMOS Discovery and Registration Service.
 *
 * Handles registration of this UE instance as an IPMX-compliant
 * media node, exposing senders for connection by NMOS controllers.
 */
UCLASS(BlueprintType)
class RSHIP2110_API URshipIPMXService : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initialize the IPMX service.
     * @param InSubsystem Parent subsystem
     * @return true if initialization succeeded
     */
    bool Initialize(URship2110Subsystem* InSubsystem);

    /**
     * Shutdown and unregister from registry.
     */
    void Shutdown();

    /**
     * Tick update for heartbeats and registry maintenance.
     * @param DeltaTime Time since last tick
     */
    void Tick(float DeltaTime);

    // ========================================================================
    // REGISTRY CONNECTION
    // ========================================================================

    /**
     * Connect to NMOS registry.
     * @param RegistryUrl URL of the registry (empty = use mDNS discovery)
     * @return true if connection initiated
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool ConnectToRegistry(const FString& RegistryUrl = TEXT(""));

    /**
     * Disconnect from registry and unregister all resources.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    void DisconnectFromRegistry();

    /**
     * Check if connected to registry.
     * @return true if connected and registered
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool IsConnected() const;

    /**
     * Get connection state.
     * @return Current state
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    ERshipIPMXConnectionState GetState() const { return State; }

    /**
     * Get full status.
     * @return Status structure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    FRshipIPMXStatus GetStatus() const;

    // ========================================================================
    // NODE CONFIGURATION
    // ========================================================================

    /**
     * Set node label.
     * @param Label Human-readable label
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    void SetNodeLabel(const FString& Label);

    /**
     * Set node description.
     * @param Description Longer description
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    void SetNodeDescription(const FString& Description);

    /**
     * Add tag to node.
     * @param Key Tag key
     * @param Value Tag value
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    void AddNodeTag(const FString& Key, const FString& Value);

    /**
     * Get node configuration.
     * @return Node structure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    FRshipNMOSNode GetNodeConfig() const { return NodeConfig; }

    /**
     * Get node ID.
     * @return UUID string
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    FString GetNodeId() const { return NodeConfig.Id; }

    // ========================================================================
    // SENDER MANAGEMENT
    // ========================================================================

    /**
     * Register a video sender with the IPMX registry.
     * @param VideoSender Video sender to register
     * @return Sender ID or empty string on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    FString RegisterSender(URship2110VideoSender* VideoSender);

    /**
     * Unregister a sender from the registry.
     * @param SenderId Sender ID to unregister
     * @return true if unregistered
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool UnregisterSender(const FString& SenderId);

    /**
     * Get registered sender by ID.
     * @param SenderId Sender ID
     * @param OutSender Sender configuration
     * @return true if found
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool GetSender(const FString& SenderId, FRshipNMOSSender& OutSender) const;

    /**
     * Get all registered sender IDs.
     * @return Array of sender IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    TArray<FString> GetRegisteredSenderIds() const;

    /**
     * Update sender transport parameters.
     * @param SenderId Sender ID
     * @param NewParams New transport parameters
     * @return true if updated
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool UpdateSenderTransport(const FString& SenderId, const FRship2110TransportParams& NewParams);

    /**
     * Activate sender (begin streaming).
     * @param SenderId Sender ID
     * @return true if activated
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool ActivateSender(const FString& SenderId);

    /**
     * Deactivate sender (stop streaming).
     * @param SenderId Sender ID
     * @return true if deactivated
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool DeactivateSender(const FString& SenderId);

    // ========================================================================
    // SDP / MANIFEST
    // ========================================================================

    /**
     * Get SDP manifest for a sender.
     * @param SenderId Sender ID
     * @return SDP string or empty
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    FString GetSenderSDP(const FString& SenderId) const;

    /**
     * Get manifest URL for a sender.
     * @param SenderId Sender ID
     * @return Manifest URL
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    FString GetSenderManifestUrl(const FString& SenderId) const;

    // ========================================================================
    // LOCAL API SERVER
    // ========================================================================

    /**
     * Start local HTTP API server for IS-04/IS-05.
     * @param Port Port to listen on
     * @return true if started
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool StartLocalAPIServer(int32 Port);

    /**
     * Stop local API server.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    void StopLocalAPIServer();

    /**
     * Check if local API server is running.
     * @return true if running
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IPMX")
    bool IsLocalAPIRunning() const { return bLocalAPIRunning; }

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when connection state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|IPMX")
    FOnIPMXConnectionStateChanged OnStateChanged;

private:
    UPROPERTY()
    URship2110Subsystem* Subsystem = nullptr;

    // State
    ERshipIPMXConnectionState State = ERshipIPMXConnectionState::Disconnected;
    FString RegistryUrl;
    FString LastError;
    double LastHeartbeatTime = 0.0;

    // Configuration
    FRshipNMOSNode NodeConfig;
    FString DeviceId;

    // Registered resources
    TMap<FString, FRshipNMOSSender> RegisteredSenders;
    TMap<FString, FString> SenderToVideoSenderId;  // Maps NMOS sender ID to video sender stream ID

    // Local API server
    bool bLocalAPIRunning = false;
    int32 LocalAPIPort = 3212;
    // HTTP server would be initialized here (using FHttpServerModule or custom)

    // Heartbeat tracking
    double HeartbeatInterval = 5.0;
    FTimerHandle HeartbeatTimerHandle;

    // HTTP requests
    TSharedPtr<IHttpRequest> PendingRequest;

    // Internal methods
    void SetState(ERshipIPMXConnectionState NewState);
    FString GenerateUUID() const;
    void InitializeNodeConfig();
    void InitializeDeviceConfig();

    // Registry API calls
    void RegisterNode();
    void RegisterDevice();
    void RegisterSourceAndFlow(const FString& SenderId, URship2110VideoSender* VideoSender);
    void RegisterSenderResource(const FString& SenderId);
    void UnregisterResource(const FString& ResourceType, const FString& ResourceId);
    void SendHeartbeat();

    // HTTP helpers
    void SendRegistryRequest(
        const FString& Method,
        const FString& Endpoint,
        const TSharedPtr<FJsonObject>& Body,
        TFunction<void(bool bSuccess, const FString& Response)> Callback);

    // JSON builders
    TSharedPtr<FJsonObject> BuildNodeJson() const;
    TSharedPtr<FJsonObject> BuildDeviceJson() const;
    TSharedPtr<FJsonObject> BuildSourceJson(const FString& SenderId, URship2110VideoSender* VideoSender) const;
    TSharedPtr<FJsonObject> BuildFlowJson(const FString& SenderId, URship2110VideoSender* VideoSender) const;
    TSharedPtr<FJsonObject> BuildSenderJson(const FString& SenderId) const;

    // Local API handlers (IS-04 Node API)
    void HandleAPIRequest(const FString& Path, const FString& Method, const FString& Body, FString& OutResponse);
    FString HandleNodeAPI(const FString& Path) const;
    FString HandleSendersAPI(const FString& Path) const;
    FString HandleSingleSenderAPI(const FString& SenderId) const;

    // mDNS discovery (fallback)
    bool DiscoverRegistryViaMDNS();
};
