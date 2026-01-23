/**
 * High-Performance WebSocket Client for Rocketship
 *
 * This wrapper provides a high-throughput WebSocket implementation using IXWebSocket.
 * Key advantages over UE's built-in WebSocket:
 *
 * - Dedicated send thread (no 30Hz throttle)
 * - TCP_NODELAY enabled by default (no Nagle delay)
 * - No permessage-deflate compression (no buffering)
 * - Configurable ping/pong heartbeat
 * - Built-in auto-reconnect
 *
 * Usage:
 *   auto WebSocket = MakeShared<FRshipWebSocket>();
 *   WebSocket->OnConnected.BindLambda([]() { ... });
 *   WebSocket->OnMessage.BindLambda([](const FString& Msg) { ... });
 *   WebSocket->Connect("ws://localhost:5155/myko");
 *   WebSocket->Send("Hello");
 */

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "HAL/CriticalSection.h"

// Forward declare IXWebSocket types (actual implementation uses IXWebSocket library)
// If IXWebSocket is not available, falls back to UE's WebSocket
#ifndef RSHIP_USE_IXWEBSOCKET
#define RSHIP_USE_IXWEBSOCKET 0
#endif

#if RSHIP_USE_IXWEBSOCKET
namespace ix { class WebSocket; }
#endif

// Delegates for WebSocket events
DECLARE_DELEGATE(FOnRshipWebSocketConnected);
DECLARE_DELEGATE_OneParam(FOnRshipWebSocketConnectionError, const FString& /* Error */);
DECLARE_DELEGATE_ThreeParams(FOnRshipWebSocketClosed, int32 /* Code */, const FString& /* Reason */, bool /* bWasClean */);
DECLARE_DELEGATE_OneParam(FOnRshipWebSocketMessage, const FString& /* Message */);
DECLARE_DELEGATE_OneParam(FOnRshipWebSocketBinaryMessage, const TArray<uint8>& /* Data */);
DECLARE_DELEGATE_OneParam(FOnRshipWebSocketMessageSent, const FString& /* Message */);

/**
 * WebSocket configuration options
 */
struct FRshipWebSocketConfig
{
    // Enable TCP_NODELAY (disable Nagle's algorithm)
    bool bTcpNoDelay = true;

    // Disable permessage-deflate compression
    bool bDisableCompression = true;

    // Ping interval in seconds (0 = disabled)
    int32 PingIntervalSeconds = 30;

    // Enable auto-reconnect on disconnect
    bool bAutoReconnect = true;

    // Min/Max reconnect wait time
    int32 MinReconnectWaitSeconds = 1;
    int32 MaxReconnectWaitSeconds = 60;

    // Handshake timeout in seconds
    int32 HandshakeTimeoutSeconds = 10;

    // Enable per-message deflate (compression) - should be false for low latency
    bool bEnablePerMessageDeflate = false;

    // Maximum message size (bytes) - 0 = unlimited
    int32 MaxMessageSize = 0;
};

/**
 * High-performance WebSocket client
 */
class RSHIPEXEC_API FRshipWebSocket : public TSharedFromThis<FRshipWebSocket>
{
public:
    FRshipWebSocket();
    ~FRshipWebSocket();

    // Connect to a WebSocket URL
    void Connect(const FString& Url, const FRshipWebSocketConfig& Config = FRshipWebSocketConfig());

    // Disconnect
    void Close(int32 Code = 1000, const FString& Reason = TEXT(""));

    // Send a text message
    bool Send(const FString& Message);

    // Send binary data
    bool SendBinary(const TArray<uint8>& Data);

    // Check connection state
    bool IsConnected() const;

    // Get pending send queue size (for backpressure detection)
    int32 GetPendingSendCount() const;

    // Event delegates
    FOnRshipWebSocketConnected OnConnected;
    FOnRshipWebSocketConnectionError OnConnectionError;
    FOnRshipWebSocketClosed OnClosed;
    FOnRshipWebSocketMessage OnMessage;
    FOnRshipWebSocketBinaryMessage OnBinaryMessage;
    FOnRshipWebSocketMessageSent OnMessageSent;

private:
#if RSHIP_USE_IXWEBSOCKET
    // IXWebSocket implementation
    TUniquePtr<ix::WebSocket> IXSocket;
    void SetupIXWebSocket(const FRshipWebSocketConfig& Config);
#else
    // Fallback: Custom socket implementation with dedicated thread
    class FWebSocketThread;
    TUniquePtr<FWebSocketThread> SocketThread;

    // Or use UE's WebSocket with optimization hints
    TSharedPtr<class IWebSocket> UEWebSocket;
    void SetupUEWebSocket(const FString& Url);
#endif

    FString CurrentUrl;
    FRshipWebSocketConfig CurrentConfig;
    FThreadSafeBool bIsConnected;

    // Send queue for async sends
    TQueue<FString> SendQueue;
    mutable FCriticalSection SendLock;
};

// ============================================================================
// FALLBACK: Custom WebSocket Thread (when IXWebSocket not available)
// This provides a hot service loop even with UE's WebSocket
// ============================================================================

#if !RSHIP_USE_IXWEBSOCKET

/**
 * Background thread that services the WebSocket more frequently than UE's default.
 * This mitigates the 30Hz throttle by calling Send() from a dedicated thread.
 */
class FRshipWebSocketServiceThread : public FRunnable
{
public:
    FRshipWebSocketServiceThread(TSharedPtr<class IWebSocket> InWebSocket);
    virtual ~FRshipWebSocketServiceThread();

    // FRunnable interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    // Queue a message for sending
    void QueueSend(const FString& Message);

    // Get pending count
    int32 GetPendingCount() const;

    // Delegate for when message is sent
    FOnRshipWebSocketMessageSent OnMessageSent;

private:
    TSharedPtr<class IWebSocket> WebSocket;
    TQueue<FString> SendQueue;
    mutable FCriticalSection QueueLock;
    FThreadSafeBool bShouldStop;
    FRunnableThread* Thread;
    FEvent* WakeEvent;
};

#endif // !RSHIP_USE_IXWEBSOCKET
