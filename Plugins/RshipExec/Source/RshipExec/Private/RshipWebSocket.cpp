/**
 * High-Performance WebSocket Implementation
 *
 * Two modes:
 * 1. RSHIP_USE_IXWEBSOCKET=1: Uses IXWebSocket library (best performance)
 * 2. RSHIP_USE_IXWEBSOCKET=0: Falls back to UE's WebSocket + dedicated send thread
 */

#include "RshipWebSocket.h"
#include "Logs.h"

#if RSHIP_USE_IXWEBSOCKET
// Include IXWebSocket headers
#include "ixwebsocket/IXWebSocket.h"
#include "ixwebsocket/IXNetSystem.h"
#endif

// For UE WebSocket fallback
#include "WebSocketsModule.h"
#include "IWebSocket.h"

// Platform includes for socket options
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif

// ============================================================================
// FRshipWebSocket Implementation
// ============================================================================

FRshipWebSocket::FRshipWebSocket()
    : bIsConnected(false)
{
#if RSHIP_USE_IXWEBSOCKET
    // Initialize network system for IXWebSocket
    ix::initNetSystem();
#endif
}

FRshipWebSocket::~FRshipWebSocket()
{
    Close();

#if RSHIP_USE_IXWEBSOCKET
    IXSocket.Reset();
    ix::uninitNetSystem();
#else
    SocketThread.Reset();
    UEWebSocket.Reset();
#endif
}

void FRshipWebSocket::Connect(const FString& Url, const FRshipWebSocketConfig& Config)
{
    CurrentUrl = Url;
    CurrentConfig = Config;

    UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Connecting to %s (TcpNoDelay=%d, Compression=%d)"),
        *Url, Config.bTcpNoDelay, !Config.bDisableCompression);

#if RSHIP_USE_IXWEBSOCKET
    SetupIXWebSocket(Config);
#else
    SetupUEWebSocket(Url);
#endif
}

void FRshipWebSocket::Close(int32 Code, const FString& Reason)
{
    bIsConnected = false;

    // Unbind delegates before closing to prevent callbacks during/after shutdown
    OnConnected.Unbind();
    OnConnectionError.Unbind();
    OnClosed.Unbind();
    OnMessage.Unbind();
    OnBinaryMessage.Unbind();
    OnMessageSent.Unbind();

#if RSHIP_USE_IXWEBSOCKET
    if (IXSocket)
    {
        IXSocket->stop();
    }
#else
    if (SocketThread)
    {
        SocketThread->Stop();
    }
    if (UEWebSocket && UEWebSocket->IsConnected())
    {
        UEWebSocket->Close(Code, TCHAR_TO_UTF8(*Reason));
    }
#endif

    UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Closed (code=%d, reason=%s)"), Code, *Reason);
}

bool FRshipWebSocket::Send(const FString& Message)
{
    if (!bIsConnected)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipWebSocket::Send called but not connected"));
        return false;
    }

#if RSHIP_USE_IXWEBSOCKET
    if (IXSocket)
    {
        std::string StdMsg = TCHAR_TO_UTF8(*Message);
        UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket::Send IXWebSocket sending %d bytes, readyState=%d"),
            StdMsg.length(), (int)IXSocket->getReadyState());

        ix::WebSocketSendInfo info = IXSocket->send(StdMsg);

        UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket::Send result: success=%d, payloadSize=%d, wireSize=%d, compressionError=%d"),
            info.success, (int)info.payloadSize, (int)info.wireSize, info.compressionError);

        if (info.success && OnMessageSent.IsBound())
        {
            // Fire on game thread
            AsyncTask(ENamedThreads::GameThread, [this, Message]()
            {
                OnMessageSent.ExecuteIfBound(Message);
            });
        }
        return info.success;
    }
#else
    // Queue for background thread to send
    if (SocketThread)
    {
        UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket::Send UE queuing %d bytes to background thread"), Message.Len());
        SocketThread->QueueSend(Message);
        return true;
    }
    else if (UEWebSocket && UEWebSocket->IsConnected())
    {
        UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket::Send UE direct sending %d bytes"), Message.Len());
        UEWebSocket->Send(Message);
        if (OnMessageSent.IsBound())
        {
            OnMessageSent.Execute(Message);
        }
        return true;
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipWebSocket::Send UE no socket available"));
    }
#endif

    return false;
}

bool FRshipWebSocket::SendBinary(const TArray<uint8>& Data)
{
    if (!bIsConnected)
    {
        return false;
    }

#if RSHIP_USE_IXWEBSOCKET
    if (IXSocket)
    {
        std::string BinaryData(reinterpret_cast<const char*>(Data.GetData()), Data.Num());
        ix::WebSocketSendInfo info = IXSocket->sendBinary(BinaryData);
        return info.success;
    }
#else
    // UE's WebSocket Send can handle binary via TArray<uint8> overload
    // But we're using string-based interface for simplicity
    UE_LOG(LogRshipExec, Warning, TEXT("RshipWebSocket: Binary send not implemented in fallback mode"));
#endif

    return false;
}

bool FRshipWebSocket::IsConnected() const
{
    return bIsConnected;
}

int32 FRshipWebSocket::GetPendingSendCount() const
{
#if RSHIP_USE_IXWEBSOCKET
    // IXWebSocket doesn't expose this directly, return 0
    return 0;
#else
    if (SocketThread)
    {
        return SocketThread->GetPendingCount();
    }
    return 0;
#endif
}

bool FRshipWebSocket::HasPendingMessages() const
{
    return !PendingBinaryMessages.IsEmpty() || !PendingTextMessages.IsEmpty();
}

int32 FRshipWebSocket::ProcessPendingMessages()
{
    int32 ProcessedCount = 0;

    // Process all pending binary messages
    TArray<uint8> BinaryData;
    while (PendingBinaryMessages.Dequeue(BinaryData))
    {
        if (OnBinaryMessage.IsBound())
        {
            OnBinaryMessage.Execute(BinaryData);
        }
        ProcessedCount++;
    }

    // Process all pending text messages
    FString TextMessage;
    while (PendingTextMessages.Dequeue(TextMessage))
    {
        if (OnMessage.IsBound())
        {
            OnMessage.Execute(TextMessage);
        }
        ProcessedCount++;
    }

    return ProcessedCount;
}

// ============================================================================
// IXWebSocket Implementation
// ============================================================================

#if RSHIP_USE_IXWEBSOCKET

void FRshipWebSocket::SetupIXWebSocket(const FRshipWebSocketConfig& Config)
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Setting up IXWebSocket for %s"), *CurrentUrl);

    IXSocket = MakeUnique<ix::WebSocket>();

    // Configure URL
    std::string StdUrl = TCHAR_TO_UTF8(*CurrentUrl);
    IXSocket->setUrl(StdUrl);

    // Disable compression for low latency
    if (Config.bDisableCompression)
    {
        UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Disabling perMessageDeflate compression"));
        IXSocket->disablePerMessageDeflate();
    }

    // Set ping interval
    if (Config.PingIntervalSeconds > 0)
    {
        UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Setting ping interval to %d seconds"), Config.PingIntervalSeconds);
        IXSocket->setPingInterval(Config.PingIntervalSeconds);
    }

    // Configure auto-reconnect
    if (Config.bAutoReconnect)
    {
        IXSocket->enableAutomaticReconnection();
        IXSocket->setMinWaitBetweenReconnectionRetries(Config.MinReconnectWaitSeconds * 1000);
        IXSocket->setMaxWaitBetweenReconnectionRetries(Config.MaxReconnectWaitSeconds * 1000);
    }
    else
    {
        IXSocket->disableAutomaticReconnection();
    }

    // Set handshake timeout
    IXSocket->setHandshakeTimeout(Config.HandshakeTimeoutSeconds);
    UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Configuration complete (compression=%d, ping=%ds, autoReconnect=%d)"),
        !Config.bDisableCompression, Config.PingIntervalSeconds, Config.bAutoReconnect);

    // Set message callback
    IXSocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg)
    {
        switch (msg->type)
        {
        case ix::WebSocketMessageType::Open:
            bIsConnected = true;
            UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Connected (IXWebSocket Open event received, readyState=%d)"),
                (int)IXSocket->getReadyState());
            AsyncTask(ENamedThreads::GameThread, [this]()
            {
                UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Firing OnConnected delegate on game thread"));
                OnConnected.ExecuteIfBound();
            });
            break;

        case ix::WebSocketMessageType::Close:
            bIsConnected = false;
            {
                int32 Code = msg->closeInfo.code;
                FString Reason = UTF8_TO_TCHAR(msg->closeInfo.reason.c_str());
                bool bWasClean = msg->closeInfo.code == 1000;

                UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Closed (code=%d, reason=%s)"), Code, *Reason);

                AsyncTask(ENamedThreads::GameThread, [this, Code, Reason, bWasClean]()
                {
                    OnClosed.ExecuteIfBound(Code, Reason, bWasClean);
                });
            }
            break;

        case ix::WebSocketMessageType::Error:
            {
                FString Error = UTF8_TO_TCHAR(msg->errorInfo.reason.c_str());
                UE_LOG(LogRshipExec, Warning, TEXT("RshipWebSocket: Error - %s"), *Error);

                AsyncTask(ENamedThreads::GameThread, [this, Error]()
                {
                    OnConnectionError.ExecuteIfBound(Error);
                });
            }
            break;

        case ix::WebSocketMessageType::Message:
            {
                if (msg->binary)
                {
                    // Binary message (msgpack) - queue for high-frequency processing
                    TArray<uint8> BinaryData;
                    BinaryData.Append(reinterpret_cast<const uint8*>(msg->str.data()), msg->str.size());
                    UE_LOG(LogRshipExec, Verbose, TEXT("RshipWebSocket: Queued binary message (%d bytes)"), BinaryData.Num());

                    // Queue instead of AsyncTask for lower latency
                    PendingBinaryMessages.Enqueue(MoveTemp(BinaryData));
                }
                else
                {
                    // Text message (JSON) - queue for high-frequency processing
                    FString Message = UTF8_TO_TCHAR(msg->str.c_str());
                    UE_LOG(LogRshipExec, Verbose, TEXT("RshipWebSocket: Queued text message (%d bytes)"), Message.Len());

                    // Queue instead of AsyncTask for lower latency
                    PendingTextMessages.Enqueue(MoveTemp(Message));
                }
            }
            break;

        case ix::WebSocketMessageType::Ping:
            UE_LOG(LogRshipExec, Verbose, TEXT("RshipWebSocket: Ping received"));
            break;
        case ix::WebSocketMessageType::Pong:
            UE_LOG(LogRshipExec, Verbose, TEXT("RshipWebSocket: Pong received"));
            break;
        case ix::WebSocketMessageType::Fragment:
            // Handled automatically
            break;
        }
    });

    // Start connection
    UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Calling IXSocket->start() to begin connection..."));
    IXSocket->start();
    UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: IXSocket->start() returned (connection initiated asynchronously)"));
}

#else

// ============================================================================
// UE WebSocket Fallback with Dedicated Send Thread
// ============================================================================

void FRshipWebSocket::SetupUEWebSocket(const FString& Url)
{
    // Ensure WebSockets module is loaded
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    // Create WebSocket
    UEWebSocket = FWebSocketsModule::Get().CreateWebSocket(Url);

    // Set up event handlers
    UEWebSocket->OnConnected().AddLambda([this]()
    {
        bIsConnected = true;
        UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Connected (UE fallback)"));

        // Start background send thread for hot service loop
        SocketThread = MakeUnique<FRshipWebSocketServiceThread>(UEWebSocket);
        SocketThread->OnMessageSent = OnMessageSent;

        OnConnected.ExecuteIfBound();
    });

    UEWebSocket->OnConnectionError().AddLambda([this](const FString& Error)
    {
        bIsConnected = false;
        UE_LOG(LogRshipExec, Warning, TEXT("RshipWebSocket: Connection error - %s"), *Error);
        OnConnectionError.ExecuteIfBound(Error);
    });

    UEWebSocket->OnClosed().AddLambda([this](int32 Code, const FString& Reason, bool bWasClean)
    {
        bIsConnected = false;

        // Stop send thread
        if (SocketThread)
        {
            SocketThread->Stop();
        }

        UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Closed (code=%d, reason=%s, clean=%d)"),
            Code, *Reason, bWasClean);
        OnClosed.ExecuteIfBound(Code, Reason, bWasClean);
    });

    UEWebSocket->OnMessage().AddLambda([this](const FString& Message)
    {
        OnMessage.ExecuteIfBound(Message);
    });

    // Connect
    UEWebSocket->Connect();
}

// ============================================================================
// Background Send Thread Implementation
// ============================================================================

FRshipWebSocketServiceThread::FRshipWebSocketServiceThread(TSharedPtr<IWebSocket> InWebSocket)
    : WebSocket(InWebSocket)
    , bShouldStop(false)
    , Thread(nullptr)
    , WakeEvent(nullptr)
{
    WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
    Thread = FRunnableThread::Create(this, TEXT("RshipWebSocketSendThread"), 0, TPri_AboveNormal);

    UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Started dedicated send thread"));
}

FRshipWebSocketServiceThread::~FRshipWebSocketServiceThread()
{
    Stop();

    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }

    if (WakeEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
        WakeEvent = nullptr;
    }
}

bool FRshipWebSocketServiceThread::Init()
{
    return true;
}

uint32 FRshipWebSocketServiceThread::Run()
{
    while (!bShouldStop)
    {
        // Wait for data or timeout (1ms for hot loop)
        WakeEvent->Wait(1);

        if (bShouldStop)
        {
            break;
        }

        // Drain send queue
        FString Message;
        while (SendQueue.Dequeue(Message))
        {
            if (WebSocket.IsValid() && WebSocket->IsConnected())
            {
                WebSocket->Send(Message);

                // Fire delegate on game thread
                if (OnMessageSent.IsBound())
                {
                    AsyncTask(ENamedThreads::GameThread, [this, Message]()
                    {
                        OnMessageSent.ExecuteIfBound(Message);
                    });
                }
            }
        }
    }

    return 0;
}

void FRshipWebSocketServiceThread::Stop()
{
    bShouldStop = true;
    if (WakeEvent)
    {
        WakeEvent->Trigger();
    }
}

void FRshipWebSocketServiceThread::Exit()
{
    UE_LOG(LogRshipExec, Log, TEXT("RshipWebSocket: Send thread exiting"));
}

void FRshipWebSocketServiceThread::QueueSend(const FString& Message)
{
    SendQueue.Enqueue(Message);

    // Wake the thread immediately
    if (WakeEvent)
    {
        WakeEvent->Trigger();
    }
}

int32 FRshipWebSocketServiceThread::GetPendingCount() const
{
    // TQueue doesn't have a count, but we can check if empty
    return SendQueue.IsEmpty() ? 0 : 1;  // Approximate
}

#endif // !RSHIP_USE_IXWEBSOCKET
