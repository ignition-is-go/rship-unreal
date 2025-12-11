// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExternalProcessorTypes.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"

/**
 * Delegate for received OSC messages.
 */
DECLARE_DELEGATE_OneParam(FOnOSCMessageReceivedNative, const FOSCMessage&);

/**
 * Delegate for connection state changes.
 */
DECLARE_DELEGATE_OneParam(FOnOSCConnectionStateChanged, bool /* bConnected */);

/**
 * Delegate for errors.
 */
DECLARE_DELEGATE_OneParam(FOnOSCError, const FString& /* ErrorMessage */);

/**
 * Low-level OSC over UDP client.
 *
 * Handles:
 * - UDP socket creation and management
 * - OSC message serialization and parsing
 * - Message queuing and rate limiting
 * - Async receive with callback
 *
 * Thread Safety:
 * - Send methods are thread-safe
 * - Receive callbacks are called from the socket thread
 * - Initialize/Shutdown must be called from the same thread
 *
 * Usage:
 *   FOSCClient Client;
 *   Client.OnMessageReceived.BindLambda([](const FOSCMessage& Msg) { ... });
 *   Client.Initialize(TEXT("192.168.1.100"), 50010, 50011);
 *
 *   FOSCMessage Msg;
 *   Msg.Address = TEXT("/some/address");
 *   Msg.AddFloat(1.0f);
 *   Client.Send(Msg);
 */
class RSHIPSPATIALAUDIORUNTIME_API FOSCClient
{
public:
	FOSCClient();
	~FOSCClient();

	// Non-copyable
	FOSCClient(const FOSCClient&) = delete;
	FOSCClient& operator=(const FOSCClient&) = delete;

	// ========================================================================
	// LIFECYCLE
	// ========================================================================

	/**
	 * Initialize the OSC client.
	 *
	 * @param RemoteHost Remote host IP address.
	 * @param RemotePort Remote port to send to.
	 * @param LocalPort Local port to receive on.
	 * @return True if initialization succeeded.
	 */
	bool Initialize(const FString& RemoteHost, int32 RemotePort, int32 LocalPort);

	/**
	 * Shutdown and release resources.
	 */
	void Shutdown();

	/**
	 * Check if client is initialized.
	 */
	bool IsInitialized() const { return bInitialized; }

	/**
	 * Check if client appears to be connected (has sent/received recently).
	 */
	bool IsConnected() const;

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/**
	 * Set rate limiting parameters.
	 *
	 * @param MaxMessagesPerSecond Maximum send rate (0 = unlimited).
	 * @param MaxBundleSize Maximum bundle size in bytes.
	 */
	void SetRateLimits(int32 MaxMessagesPerSecond, int32 MaxBundleSize);

	/**
	 * Enable/disable message bundling.
	 */
	void SetBundlingEnabled(bool bEnabled);

	/**
	 * Set the remote address (can be changed after initialization).
	 */
	bool SetRemoteAddress(const FString& Host, int32 Port);

	// ========================================================================
	// SENDING
	// ========================================================================

	/**
	 * Send a single OSC message.
	 *
	 * @param Message The message to send.
	 * @return True if message was sent/queued successfully.
	 */
	bool Send(const FOSCMessage& Message);

	/**
	 * Send an OSC bundle.
	 *
	 * @param Bundle The bundle to send.
	 * @return True if bundle was sent successfully.
	 */
	bool Send(const FOSCBundle& Bundle);

	/**
	 * Send raw bytes (already-serialized OSC data).
	 *
	 * @param Data The data to send.
	 * @return True if data was sent successfully.
	 */
	bool SendRaw(const TArray<uint8>& Data);

	/**
	 * Send multiple messages as a bundle.
	 *
	 * @param Messages The messages to bundle and send.
	 * @return True if bundle was sent successfully.
	 */
	bool SendBundle(const TArray<FOSCMessage>& Messages);

	/**
	 * Flush any queued messages immediately.
	 */
	void Flush();

	// ========================================================================
	// STATISTICS
	// ========================================================================

	/** Get messages sent count */
	int64 GetMessagesSent() const { return MessagesSent; }

	/** Get messages received count */
	int64 GetMessagesReceived() const { return MessagesReceived; }

	/** Get bytes sent */
	int64 GetBytesSent() const { return BytesSent; }

	/** Get bytes received */
	int64 GetBytesReceived() const { return BytesReceived; }

	/** Get current send rate (messages/sec) */
	float GetCurrentSendRate() const;

	/** Get last communication time */
	FDateTime GetLastCommunicationTime() const { return LastCommunicationTime; }

	// ========================================================================
	// DELEGATES
	// ========================================================================

	/** Called when OSC message is received */
	FOnOSCMessageReceivedNative OnMessageReceived;

	/** Called when connection state appears to change */
	FOnOSCConnectionStateChanged OnConnectionStateChanged;

	/** Called on errors */
	FOnOSCError OnError;

private:
	// ========================================================================
	// INTERNAL STATE
	// ========================================================================

	bool bInitialized;
	FString RemoteHost;
	int32 RemotePort;
	int32 LocalPort;

	// Socket
	FSocket* SendSocket;
	FSocket* ReceiveSocket;
	TSharedPtr<FInternetAddr> RemoteAddress;
	TUniquePtr<FUdpSocketReceiver> SocketReceiver;

	// Rate limiting
	int32 MaxMessagesPerSecond;
	int32 MaxBundleSizeBytes;
	bool bBundlingEnabled;
	double LastSendTime;
	int32 MessagesSentThisSecond;
	double SecondStartTime;

	// Statistics
	TAtomic<int64> MessagesSent;
	TAtomic<int64> MessagesReceived;
	TAtomic<int64> BytesSent;
	TAtomic<int64> BytesReceived;
	FDateTime LastCommunicationTime;
	bool bWasConnected;

	// Thread safety
	FCriticalSection SendLock;

	// ========================================================================
	// INTERNAL METHODS
	// ========================================================================

	/** Create sockets */
	bool CreateSockets();

	/** Destroy sockets */
	void DestroySockets();

	/** Handle received UDP data */
	void HandleDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

	/** Check rate limit - returns true if can send */
	bool CheckRateLimit();

	/** Update send statistics */
	void UpdateSendStats(int32 NumMessages, int32 ByteCount);

	/** Check and update connection state */
	void UpdateConnectionState();
};

// ============================================================================
// OSC MESSAGE BUILDER
// ============================================================================

/**
 * Helper class for building OSC messages with fluent API.
 */
class RSHIPSPATIALAUDIORUNTIME_API FOSCMessageBuilder
{
public:
	explicit FOSCMessageBuilder(const FString& Address);

	FOSCMessageBuilder& Int(int32 Value);
	FOSCMessageBuilder& Float(float Value);
	FOSCMessageBuilder& String(const FString& Value);
	FOSCMessageBuilder& Blob(const TArray<uint8>& Value);
	FOSCMessageBuilder& True();
	FOSCMessageBuilder& False();

	FOSCMessage Build() const { return Message; }

	// Implicit conversion
	operator FOSCMessage() const { return Message; }

private:
	FOSCMessage Message;
};

// Convenience function
inline FOSCMessageBuilder OSCMsg(const FString& Address)
{
	return FOSCMessageBuilder(Address);
}

// ============================================================================
// OSC ADDRESS UTILITIES
// ============================================================================

/**
 * OSC address pattern matching utilities.
 */
class RSHIPSPATIALAUDIORUNTIME_API FOSCAddress
{
public:
	/**
	 * Check if an address matches a pattern.
	 * Supports wildcards: * (any string), ? (any char), [abc] (char class)
	 */
	static bool Matches(const FString& Pattern, const FString& Address);

	/**
	 * Extract the method name from an address (last component).
	 */
	static FString GetMethod(const FString& Address);

	/**
	 * Get address components.
	 */
	static TArray<FString> GetComponents(const FString& Address);

	/**
	 * Build address from components.
	 */
	static FString Build(const TArray<FString>& Components);

	/**
	 * Validate address format.
	 */
	static bool IsValid(const FString& Address);
};
