// Rship OSC Bridge
// OSC input/output for external controller integration (TouchOSC, QLab, etc.)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Common/UdpSocketReceiver.h"
#include "Common/UdpSocketSender.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "RshipOSCBridge.generated.h"

class URshipSubsystem;

// ============================================================================
// OSC MESSAGE TYPES
// ============================================================================

/** OSC argument types */
UENUM(BlueprintType)
enum class ERshipOSCArgumentType : uint8
{
    Int32       UMETA(DisplayName = "Integer"),
    Float       UMETA(DisplayName = "Float"),
    String      UMETA(DisplayName = "String"),
    Blob        UMETA(DisplayName = "Blob"),
    BoolTrue    UMETA(DisplayName = "True"),
    BoolFalse   UMETA(DisplayName = "False"),
    Nil         UMETA(DisplayName = "Nil"),
    Color       UMETA(DisplayName = "Color")
};

/** Single OSC argument */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipOSCArgument
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    ERshipOSCArgumentType Type = ERshipOSCArgumentType::Float;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    int32 IntValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    float FloatValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FString StringValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FColor ColorValue = FColor::White;
};

/** OSC message */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipOSCMessage
{
    GENERATED_BODY()

    /** OSC address pattern (e.g., "/fixture/1/intensity") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FString Address;

    /** Message arguments */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    TArray<FRshipOSCArgument> Arguments;

    /** Source IP (for received messages) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|OSC")
    FString SourceIP;

    /** Source port (for received messages) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|OSC")
    int32 SourcePort = 0;
};

// ============================================================================
// OSC MAPPINGS
// ============================================================================

/** Mapping direction */
UENUM(BlueprintType)
enum class ERshipOSCMappingDirection : uint8
{
    Input       UMETA(DisplayName = "Input (OSC -> Rship)"),
    Output      UMETA(DisplayName = "Output (Rship -> OSC)"),
    Bidirectional UMETA(DisplayName = "Bidirectional")
};

/** How to transform OSC values */
UENUM(BlueprintType)
enum class ERshipOSCValueTransform : uint8
{
    Direct      UMETA(DisplayName = "Direct"),
    Scale       UMETA(DisplayName = "Scale"),
    RangeMap    UMETA(DisplayName = "Range Map"),
    Invert      UMETA(DisplayName = "Invert (1-x)"),
    Toggle      UMETA(DisplayName = "Toggle (>0.5 = on)")
};

/** Mapping between OSC address and rship entity */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipOSCMapping
{
    GENERATED_BODY()

    /** OSC address pattern (supports wildcards: * and ?) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FString OSCAddress;

    /** Target type (emitter or action) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    bool bIsAction = false;

    /** Target ID (emitter ID or action ID) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FString TargetId;

    /** Field name in pulse data (for emitters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FString FieldName = TEXT("intensity");

    /** Mapping direction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    ERshipOSCMappingDirection Direction = ERshipOSCMappingDirection::Input;

    /** Value transformation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    ERshipOSCValueTransform Transform = ERshipOSCValueTransform::Direct;

    /** Scale factor (for Scale transform) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC", meta = (EditCondition = "Transform == ERshipOSCValueTransform::Scale"))
    float Scale = 1.0f;

    /** Input range (for RangeMap transform) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC", meta = (EditCondition = "Transform == ERshipOSCValueTransform::RangeMap"))
    FVector2D InputRange = FVector2D(0.0f, 1.0f);

    /** Output range (for RangeMap transform) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC", meta = (EditCondition = "Transform == ERshipOSCValueTransform::RangeMap"))
    FVector2D OutputRange = FVector2D(0.0f, 1.0f);

    /** Whether this mapping is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    bool bEnabled = true;

    /** Description for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FString Description;
};

/** OSC output destination */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipOSCDestination
{
    GENERATED_BODY()

    /** Destination IP address */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FString IPAddress = TEXT("127.0.0.1");

    /** Destination port */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    int32 Port = 8000;

    /** Name for identification */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    FString Name = TEXT("Default");

    /** Whether this destination is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|OSC")
    bool bEnabled = true;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOSCMessageReceived, const FRshipOSCMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOSCMessageSent, const FRshipOSCMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOSCError, const FString&, ErrorMessage, bool, bIsSendError);

// ============================================================================
// OSC BRIDGE SERVICE
// ============================================================================

/**
 * Service for OSC communication with external controllers.
 * Supports both receiving OSC messages and sending to external software.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipOSCBridge : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    // ========================================================================
    // SERVER CONFIGURATION
    // ========================================================================

    /** Start the OSC server on specified port */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    bool StartServer(int32 Port = 9000);

    /** Stop the OSC server */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void StopServer();

    /** Is server running */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|OSC")
    bool IsServerRunning() const { return bServerRunning; }

    /** Get server port */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|OSC")
    int32 GetServerPort() const { return ServerPort; }

    // ========================================================================
    // DESTINATION MANAGEMENT
    // ========================================================================

    /** Add an output destination */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void AddDestination(const FRshipOSCDestination& Destination);

    /** Remove a destination by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void RemoveDestination(const FString& Name);

    /** Get all destinations */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    TArray<FRshipOSCDestination> GetAllDestinations() const { return Destinations; }

    /** Clear all destinations */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void ClearDestinations();

    // ========================================================================
    // MAPPING MANAGEMENT
    // ========================================================================

    /** Add a mapping */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void AddMapping(const FRshipOSCMapping& Mapping);

    /** Remove a mapping by OSC address */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void RemoveMapping(const FString& OSCAddress);

    /** Get all mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    TArray<FRshipOSCMapping> GetAllMappings() const { return Mappings; }

    /** Clear all mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void ClearMappings();

    /** Create common mappings for rship fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void CreateFixtureMappings(const FString& BaseAddress = TEXT("/fixture"));

    /** Create TouchOSC-style mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void CreateTouchOSCMappings();

    /** Create QLab-style mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void CreateQLabMappings();

    // ========================================================================
    // SENDING MESSAGES
    // ========================================================================

    /** Send an OSC message to all destinations */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void SendMessage(const FRshipOSCMessage& Message);

    /** Send a simple float value */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void SendFloat(const FString& Address, float Value);

    /** Send a simple int value */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void SendInt(const FString& Address, int32 Value);

    /** Send a simple string value */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void SendString(const FString& Address, const FString& Value);

    /** Send a color value */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void SendColor(const FString& Address, FLinearColor Color);

    /** Send multiple float values */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void SendFloats(const FString& Address, const TArray<float>& Values);

    /** Send to a specific destination only */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void SendMessageTo(const FRshipOSCMessage& Message, const FString& DestinationName);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /** Get messages received count */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|OSC")
    int32 GetMessagesReceived() const { return MessagesReceived; }

    /** Get messages sent count */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|OSC")
    int32 GetMessagesSent() const { return MessagesSent; }

    /** Get error count */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|OSC")
    int32 GetErrorCount() const { return ErrorCount; }

    /** Reset statistics */
    UFUNCTION(BlueprintCallable, Category = "Rship|OSC")
    void ResetStats();

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when an OSC message is received */
    UPROPERTY(BlueprintAssignable, Category = "Rship|OSC")
    FOnOSCMessageReceived OnMessageReceived;

    /** Fired when an OSC message is sent */
    UPROPERTY(BlueprintAssignable, Category = "Rship|OSC")
    FOnOSCMessageSent OnMessageSent;

    /** Fired on OSC errors */
    UPROPERTY(BlueprintAssignable, Category = "Rship|OSC")
    FOnOSCError OnError;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Server
    FSocket* ServerSocket = nullptr;
    TSharedPtr<FUdpSocketReceiver> SocketReceiver;
    bool bServerRunning = false;
    int32 ServerPort = 9000;

    // Destinations and mappings
    TArray<FRshipOSCDestination> Destinations;
    TArray<FRshipOSCMapping> Mappings;

    // Statistics
    int32 MessagesReceived = 0;
    int32 MessagesSent = 0;
    int32 ErrorCount = 0;

    // Pulse subscription
    FDelegateHandle PulseHandle;

    // Internal methods
    void OnDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
    bool ParseOSCMessage(const TArray<uint8>& Data, FRshipOSCMessage& OutMessage);
    TArray<uint8> SerializeOSCMessage(const FRshipOSCMessage& Message);
    void ProcessIncomingMessage(const FRshipOSCMessage& Message);
    void OnPulseReceived(const FString& EmitterId, TSharedPtr<FJsonObject> Data);
    float TransformValue(float Value, const FRshipOSCMapping& Mapping);
    float InverseTransformValue(float Value, const FRshipOSCMapping& Mapping);
    bool MatchesPattern(const FString& Address, const FString& Pattern);
    void SendToDestination(const TArray<uint8>& Data, const FRshipOSCDestination& Destination);

    // OSC parsing helpers
    int32 ReadInt32(const TArray<uint8>& Data, int32& Offset);
    float ReadFloat(const TArray<uint8>& Data, int32& Offset);
    FString ReadString(const TArray<uint8>& Data, int32& Offset);
    FColor ReadColor(const TArray<uint8>& Data, int32& Offset);

    // OSC serialization helpers
    void WriteInt32(TArray<uint8>& Data, int32 Value);
    void WriteFloat(TArray<uint8>& Data, float Value);
    void WriteString(TArray<uint8>& Data, const FString& Value);
    void WriteColor(TArray<uint8>& Data, const FColor& Color);
    void PadToFourBytes(TArray<uint8>& Data);
};
