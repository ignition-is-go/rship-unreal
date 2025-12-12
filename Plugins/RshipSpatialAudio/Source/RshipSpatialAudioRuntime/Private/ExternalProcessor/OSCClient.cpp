// Copyright Rocketship. All Rights Reserved.

#include "ExternalProcessor/OSCClient.h"
#include "Async/Async.h"
#include "Misc/ScopeLock.h"

// ============================================================================
// FOSCClient
// ============================================================================

FOSCClient::FOSCClient()
	: bInitialized(false)
	, RemotePort(0)
	, LocalPort(0)
	, SendSocket(nullptr)
	, ReceiveSocket(nullptr)
	, MaxMessagesPerSecond(0)
	, MaxBundleSizeBytes(1472)
	, bBundlingEnabled(true)
	, LastSendTime(0.0)
	, MessagesSentThisSecond(0)
	, SecondStartTime(0.0)
	, MessagesSent(0)
	, MessagesReceived(0)
	, BytesSent(0)
	, BytesReceived(0)
	, bWasConnected(false)
{
}

FOSCClient::~FOSCClient()
{
	if (bInitialized)
	{
		Shutdown();
	}
}

bool FOSCClient::Initialize(const FString& InRemoteHost, int32 InRemotePort, int32 InLocalPort)
{
	if (bInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("OSCClient: Already initialized"));
		return false;
	}

	RemoteHost = InRemoteHost;
	RemotePort = InRemotePort;
	LocalPort = InLocalPort;

	if (!CreateSockets())
	{
		return false;
	}

	bInitialized = true;
	SecondStartTime = FPlatformTime::Seconds();
	MessagesSentThisSecond = 0;

	UE_LOG(LogTemp, Log, TEXT("OSCClient: Initialized - Send to %s:%d, Receive on :%d"),
		*RemoteHost, RemotePort, LocalPort);

	return true;
}

void FOSCClient::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	DestroySockets();
	bInitialized = false;

	UE_LOG(LogTemp, Log, TEXT("OSCClient: Shutdown complete"));
}

bool FOSCClient::IsConnected() const
{
	if (!bInitialized)
	{
		return false;
	}

	// Consider connected if we've sent or received in the last 5 seconds
	FTimespan TimeSinceComm = FDateTime::UtcNow() - LastCommunicationTime;
	return TimeSinceComm.GetTotalSeconds() < 5.0;
}

void FOSCClient::SetRateLimits(int32 InMaxMessagesPerSecond, int32 InMaxBundleSize)
{
	MaxMessagesPerSecond = InMaxMessagesPerSecond;
	MaxBundleSizeBytes = InMaxBundleSize;
}

void FOSCClient::SetBundlingEnabled(bool bEnabled)
{
	bBundlingEnabled = bEnabled;
}

bool FOSCClient::SetRemoteAddress(const FString& Host, int32 Port)
{
	FScopeLock Lock(&SendLock);

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	RemoteAddress = SocketSubsystem->CreateInternetAddr();
	bool bIsValid = false;
	RemoteAddress->SetIp(*Host, bIsValid);

	if (!bIsValid)
	{
		// Try resolving hostname
		ESocketErrors ResolveError = SocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*Host), *RemoteAddress);
		if (ResolveError != SE_NO_ERROR)
		{
			UE_LOG(LogTemp, Error, TEXT("OSCClient: Failed to resolve host '%s': %s"),
				*Host, SocketSubsystem->GetSocketError(ResolveError));
			return false;
		}
	}

	RemoteAddress->SetPort(Port);
	RemoteHost = Host;
	RemotePort = Port;

	return true;
}

bool FOSCClient::Send(const FRshipOSCMessage& Message)
{
	if (!bInitialized || !SendSocket)
	{
		return false;
	}

	TArray<uint8> Data = Message.Serialize();
	return SendRaw(Data);
}

bool FOSCClient::Send(const FRshipOSCBundle& Bundle)
{
	if (!bInitialized || !SendSocket)
	{
		return false;
	}

	TArray<uint8> Data = Bundle.Serialize();
	return SendRaw(Data);
}

bool FOSCClient::SendRaw(const TArray<uint8>& Data)
{
	FScopeLock Lock(&SendLock);

	if (!bInitialized || !SendSocket || !RemoteAddress.IsValid())
	{
		return false;
	}

	// Check rate limit
	if (!CheckRateLimit())
	{
		// Rate limited - drop message
		return false;
	}

	int32 BytesSentNow = 0;
	bool bSuccess = SendSocket->SendTo(Data.GetData(), Data.Num(), BytesSentNow, *RemoteAddress);

	if (bSuccess)
	{
		UpdateSendStats(1, BytesSentNow);
	}
	else
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		ESocketErrors Error = SocketSubsystem->GetLastErrorCode();
		FString ErrorStr = SocketSubsystem->GetSocketError(Error);

		UE_LOG(LogTemp, Warning, TEXT("OSCClient: Send failed: %s"), *ErrorStr);

		if (OnError.IsBound())
		{
			OnError.Execute(FString::Printf(TEXT("Send failed: %s"), *ErrorStr));
		}
	}

	return bSuccess;
}

bool FOSCClient::SendBundle(const TArray<FRshipOSCMessage>& Messages)
{
	if (Messages.Num() == 0)
	{
		return true;
	}

	if (Messages.Num() == 1)
	{
		return Send(Messages[0]);
	}

	FRshipOSCBundle Bundle;
	Bundle.TimeTag = 1;  // Immediate
	Bundle.Messages = Messages;

	return Send(Bundle);
}

void FOSCClient::Flush()
{
	// Currently no queuing - messages are sent immediately
}

float FOSCClient::GetCurrentSendRate() const
{
	double CurrentTime = FPlatformTime::Seconds();
	double Elapsed = CurrentTime - SecondStartTime;

	if (Elapsed < 0.1)
	{
		return 0.0f;
	}

	return static_cast<float>(MessagesSentThisSecond) / static_cast<float>(Elapsed);
}

bool FOSCClient::CreateSockets()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("OSCClient: Failed to get socket subsystem"));
		return false;
	}

	// Create send socket
	SendSocket = FUdpSocketBuilder(TEXT("OSCSendSocket"))
		.AsNonBlocking()
		.AsReusable()
		.Build();

	if (!SendSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("OSCClient: Failed to create send socket"));
		return false;
	}

	// Set remote address
	if (!SetRemoteAddress(RemoteHost, RemotePort))
	{
		DestroySockets();
		return false;
	}

	// Create receive socket
	FIPv4Endpoint LocalEndpoint(FIPv4Address::Any, LocalPort);
	ReceiveSocket = FUdpSocketBuilder(TEXT("OSCReceiveSocket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(LocalEndpoint)
		.Build();

	if (!ReceiveSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("OSCClient: Failed to create receive socket on port %d"), LocalPort);
		DestroySockets();
		return false;
	}

	// Set up receiver
	SocketReceiver = MakeUnique<FUdpSocketReceiver>(
		ReceiveSocket,
		FTimespan::FromMilliseconds(100),
		TEXT("OSCReceiver"));

	SocketReceiver->OnDataReceived().BindRaw(this, &FOSCClient::HandleDataReceived);
	SocketReceiver->Start();

	return true;
}

void FOSCClient::DestroySockets()
{
	// Stop receiver first
	if (SocketReceiver.IsValid())
	{
		SocketReceiver->Stop();
		SocketReceiver.Reset();
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (SendSocket)
	{
		SendSocket->Close();
		SocketSubsystem->DestroySocket(SendSocket);
		SendSocket = nullptr;
	}

	if (ReceiveSocket)
	{
		ReceiveSocket->Close();
		SocketSubsystem->DestroySocket(ReceiveSocket);
		ReceiveSocket = nullptr;
	}

	RemoteAddress.Reset();
}

void FOSCClient::HandleDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
	if (!Data.IsValid() || Data->Num() == 0)
	{
		return;
	}

	// Update stats
	BytesReceived += Data->Num();
	LastCommunicationTime = FDateTime::UtcNow();

	// Parse OSC data
	TArray<uint8> DataArray;
	DataArray.SetNum(Data->Num());
	FMemory::Memcpy(DataArray.GetData(), Data->GetData(), Data->Num());

	// Check if bundle or message
	if (DataArray.Num() >= 8 && DataArray[0] == '#')
	{
		// OSC bundle
		FRshipOSCBundle Bundle;
		if (FRshipOSCBundle::Parse(DataArray, Bundle))
		{
			for (const FRshipOSCMessage& Message : Bundle.Messages)
			{
				MessagesReceived++;
				if (OnMessageReceived.IsBound())
				{
					OnMessageReceived.Execute(Message);
				}
			}
		}
	}
	else
	{
		// OSC message
		FRshipOSCMessage Message;
		if (FRshipOSCMessage::Parse(DataArray, Message))
		{
			MessagesReceived++;
			if (OnMessageReceived.IsBound())
			{
				OnMessageReceived.Execute(Message);
			}
		}
	}

	UpdateConnectionState();
}

bool FOSCClient::CheckRateLimit()
{
	if (MaxMessagesPerSecond <= 0)
	{
		return true;  // No limit
	}

	double CurrentTime = FPlatformTime::Seconds();

	// Reset counter every second
	if (CurrentTime - SecondStartTime >= 1.0)
	{
		SecondStartTime = CurrentTime;
		MessagesSentThisSecond = 0;
	}

	// Check if under limit
	if (MessagesSentThisSecond < MaxMessagesPerSecond)
	{
		return true;
	}

	return false;
}

void FOSCClient::UpdateSendStats(int32 NumMessages, int32 ByteCount)
{
	MessagesSent += NumMessages;
	MessagesSentThisSecond += NumMessages;
	BytesSent += ByteCount;
	LastCommunicationTime = FDateTime::UtcNow();

	UpdateConnectionState();
}

void FOSCClient::UpdateConnectionState()
{
	bool bConnected = IsConnected();

	if (bConnected != bWasConnected)
	{
		bWasConnected = bConnected;
		if (OnConnectionStateChanged.IsBound())
		{
			OnConnectionStateChanged.Execute(bConnected);
		}
	}
}

// ============================================================================
// FRshipOSCMessageBuilder
// ============================================================================

FRshipOSCMessageBuilder::FRshipOSCMessageBuilder(const FString& Address)
{
	Message.Address = Address;
}

FRshipOSCMessageBuilder& FRshipOSCMessageBuilder::Int(int32 Value)
{
	Message.AddInt(Value);
	return *this;
}

FRshipOSCMessageBuilder& FRshipOSCMessageBuilder::Float(float Value)
{
	Message.AddFloat(Value);
	return *this;
}

FRshipOSCMessageBuilder& FRshipOSCMessageBuilder::String(const FString& Value)
{
	Message.AddString(Value);
	return *this;
}

FRshipOSCMessageBuilder& FRshipOSCMessageBuilder::Blob(const TArray<uint8>& Value)
{
	FRshipOSCArgument Arg;
	Arg.Type = ERshipOSCArgumentType::Blob;
	Arg.BlobValue = Value;
	Message.Arguments.Add(Arg);
	return *this;
}

FRshipOSCMessageBuilder& FRshipOSCMessageBuilder::True()
{
	FRshipOSCArgument Arg;
	Arg.Type = ERshipOSCArgumentType::BoolTrue;
	Message.Arguments.Add(Arg);
	return *this;
}

FRshipOSCMessageBuilder& FRshipOSCMessageBuilder::False()
{
	FRshipOSCArgument Arg;
	Arg.Type = ERshipOSCArgumentType::BoolFalse;
	Message.Arguments.Add(Arg);
	return *this;
}

// ============================================================================
// FOSCAddress
// ============================================================================

bool FOSCAddress::Matches(const FString& Pattern, const FString& Address)
{
	// Simple wildcard matching
	int32 PatternIdx = 0;
	int32 AddressIdx = 0;

	while (PatternIdx < Pattern.Len() && AddressIdx < Address.Len())
	{
		TCHAR P = Pattern[PatternIdx];
		TCHAR A = Address[AddressIdx];

		if (P == '*')
		{
			// Skip to next '/' in pattern
			++PatternIdx;
			if (PatternIdx >= Pattern.Len())
			{
				return true;  // * at end matches everything
			}

			// Find next non-wildcard char in pattern
			while (PatternIdx < Pattern.Len() && Pattern[PatternIdx] == '*')
			{
				++PatternIdx;
			}

			if (PatternIdx >= Pattern.Len())
			{
				return true;
			}

			// Skip to matching char in address (but not past /)
			while (AddressIdx < Address.Len() && Address[AddressIdx] != Pattern[PatternIdx])
			{
				if (Address[AddressIdx] == '/' && Pattern[PatternIdx - 1] != '/')
				{
					return false;
				}
				++AddressIdx;
			}
		}
		else if (P == '?')
		{
			// Match any single char except /
			if (A == '/')
			{
				return false;
			}
			++PatternIdx;
			++AddressIdx;
		}
		else if (P == '[')
		{
			// Character class
			++PatternIdx;
			bool bInvert = false;
			if (PatternIdx < Pattern.Len() && Pattern[PatternIdx] == '!')
			{
				bInvert = true;
				++PatternIdx;
			}

			bool bMatched = false;
			while (PatternIdx < Pattern.Len() && Pattern[PatternIdx] != ']')
			{
				if (Pattern[PatternIdx] == A)
				{
					bMatched = true;
				}
				++PatternIdx;
			}

			if (bInvert)
			{
				bMatched = !bMatched;
			}

			if (!bMatched)
			{
				return false;
			}

			++PatternIdx;  // Skip ]
			++AddressIdx;
		}
		else
		{
			// Exact match
			if (P != A)
			{
				return false;
			}
			++PatternIdx;
			++AddressIdx;
		}
	}

	// Check remaining
	while (PatternIdx < Pattern.Len() && Pattern[PatternIdx] == '*')
	{
		++PatternIdx;
	}

	return PatternIdx >= Pattern.Len() && AddressIdx >= Address.Len();
}

FString FOSCAddress::GetMethod(const FString& Address)
{
	int32 LastSlash;
	if (Address.FindLastChar('/', LastSlash))
	{
		return Address.Mid(LastSlash + 1);
	}
	return Address;
}

TArray<FString> FOSCAddress::GetComponents(const FString& Address)
{
	TArray<FString> Components;
	Address.ParseIntoArray(Components, TEXT("/"), true);
	return Components;
}

FString FOSCAddress::Build(const TArray<FString>& Components)
{
	if (Components.Num() == 0)
	{
		return TEXT("/");
	}

	FString Result;
	for (const FString& Component : Components)
	{
		Result += TEXT("/") + Component;
	}
	return Result;
}

bool FOSCAddress::IsValid(const FString& Address)
{
	if (Address.IsEmpty() || Address[0] != '/')
	{
		return false;
	}

	// Check for invalid characters
	for (TCHAR C : Address)
	{
		if (C == ' ' || C == '#')
		{
			return false;
		}
	}

	return true;
}
