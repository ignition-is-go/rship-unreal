// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "IWebSocket.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RshipTargetComponent.h"
#include "GameFramework/Actor.h"
#include "Containers/List.h"
#include "Target.h"
#include "RshipRateLimiter.h"
#include "RshipWebSocket.h"
#include "RshipSubsystem.generated.h"


DECLARE_DYNAMIC_DELEGATE(FRshipMessageDelegate);

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

    // Connection state management
    ERshipConnectionState ConnectionState;
    int32 ReconnectAttempts;
    FTimerHandle QueueProcessTimerHandle;
    FTimerHandle ReconnectTimerHandle;

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
