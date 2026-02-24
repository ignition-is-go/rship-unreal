// Rship OSC Bridge Implementation

#include "RshipOSCBridge.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "RshipFixtureManager.h"
#include "Logs.h"
#include "Async/Async.h"
#include "Serialization/ArrayReader.h"

void URshipOSCBridge::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

    // Subscribe to pulse events for output mappings
    if (Subsystem)
    {
        URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
        if (Receiver)
        {
            PulseHandle = Receiver->OnEmitterPulseReceived.AddUObject(this, &URshipOSCBridge::OnPulseReceived);
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge initialized"));
}

void URshipOSCBridge::Shutdown()
{
    StopServer();

    // Unsubscribe from pulses
    if (Subsystem)
    {
        URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
        if (Receiver && PulseHandle.IsValid())
        {
            Receiver->OnEmitterPulseReceived.Remove(PulseHandle);
            PulseHandle.Reset();
        }
    }

    Destinations.Empty();
    Mappings.Empty();
    Subsystem = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge shutdown"));
}

void URshipOSCBridge::Tick(float DeltaTime)
{
    // Tick handled by socket receiver callback
}

// ============================================================================
// SERVER
// ============================================================================

bool URshipOSCBridge::StartServer(int32 Port)
{
    if (bServerRunning)
    {
        StopServer();
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogRshipExec, Error, TEXT("OSCBridge: Socket subsystem not available"));
        return false;
    }

    // Create UDP socket
    ServerSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("RshipOSCServer"), false);
    if (!ServerSocket)
    {
        UE_LOG(LogRshipExec, Error, TEXT("OSCBridge: Failed to create socket"));
        return false;
    }

    // Set socket options
    ServerSocket->SetReuseAddr(true);
    ServerSocket->SetNonBlocking(true);
    ServerSocket->SetRecvErr(true);

    // Bind to port
    FIPv4Endpoint Endpoint(FIPv4Address::Any, Port);
    if (!ServerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogRshipExec, Error, TEXT("OSCBridge: Failed to bind to port %d"), Port);
        SocketSubsystem->DestroySocket(ServerSocket);
        ServerSocket = nullptr;
        return false;
    }

    // Create receiver
    SocketReceiver = MakeShared<FUdpSocketReceiver>(
        ServerSocket,
        FTimespan::FromMilliseconds(100),
        TEXT("RshipOSCReceiver")
    );

    SocketReceiver->OnDataReceived().BindUObject(this, &URshipOSCBridge::OnDataReceived);
    SocketReceiver->Start();

    ServerPort = Port;
    bServerRunning = true;

    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge: Server started on port %d"), Port);
    return true;
}

void URshipOSCBridge::StopServer()
{
    if (!bServerRunning)
    {
        return;
    }

    if (SocketReceiver)
    {
        SocketReceiver->Stop();
        SocketReceiver.Reset();
    }

    if (ServerSocket)
    {
        ServerSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ServerSocket);
        ServerSocket = nullptr;
    }

    bServerRunning = false;
    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge: Server stopped"));
}

// ============================================================================
// DESTINATIONS
// ============================================================================

void URshipOSCBridge::AddDestination(const FRshipOSCDestination& Destination)
{
    // Remove existing with same name
    RemoveDestination(Destination.Name);
    Destinations.Add(Destination);

    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge: Added destination '%s' (%s:%d)"),
        *Destination.Name, *Destination.IPAddress, Destination.Port);
}

void URshipOSCBridge::RemoveDestination(const FString& Name)
{
    Destinations.RemoveAll([&Name](const FRshipOSCDestination& D) { return D.Name == Name; });
}

void URshipOSCBridge::ClearDestinations()
{
    Destinations.Empty();
}

// ============================================================================
// MAPPINGS
// ============================================================================

void URshipOSCBridge::AddMapping(const FRshipOSCMapping& Mapping)
{
    // Remove existing with same address
    RemoveMapping(Mapping.OSCAddress);
    Mappings.Add(Mapping);

    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge: Added mapping '%s' -> %s"),
        *Mapping.OSCAddress, *Mapping.TargetId);
}

void URshipOSCBridge::RemoveMapping(const FString& OSCAddress)
{
    Mappings.RemoveAll([&OSCAddress](const FRshipOSCMapping& M) { return M.OSCAddress == OSCAddress; });
}

void URshipOSCBridge::ClearMappings()
{
    Mappings.Empty();
}

void URshipOSCBridge::CreateFixtureMappings(const FString& BaseAddress)
{
    if (!Subsystem) return;

    URshipFixtureManager* FixtureMgr = Subsystem->GetFixtureManager();
    if (!FixtureMgr) return;

    TArray<FRshipFixtureInfo> Fixtures = FixtureMgr->GetAllFixtures();

    int32 Index = 1;
    for (const FRshipFixtureInfo& Fixture : Fixtures)
    {
        // Intensity mapping
        FRshipOSCMapping IntensityMapping;
        IntensityMapping.OSCAddress = FString::Printf(TEXT("%s/%d/intensity"), *BaseAddress, Index);
        IntensityMapping.TargetId = Fixture.Id;
        IntensityMapping.FieldName = TEXT("intensity");
        IntensityMapping.Direction = ERshipOSCMappingDirection::Bidirectional;
        IntensityMapping.Description = FString::Printf(TEXT("%s Intensity"), *Fixture.Name);
        AddMapping(IntensityMapping);

        // Color mapping (RGB as 3 floats)
        FRshipOSCMapping ColorMapping;
        ColorMapping.OSCAddress = FString::Printf(TEXT("%s/%d/color"), *BaseAddress, Index);
        ColorMapping.TargetId = Fixture.Id;
        ColorMapping.FieldName = TEXT("color");
        ColorMapping.Direction = ERshipOSCMappingDirection::Bidirectional;
        ColorMapping.Description = FString::Printf(TEXT("%s Color"), *Fixture.Name);
        AddMapping(ColorMapping);

        Index++;
    }

    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge: Created %d fixture mappings"), (Index - 1) * 2);
}

void URshipOSCBridge::CreateTouchOSCMappings()
{
    // TouchOSC common patterns
    // /1/fader1 - /1/fader5 -> fixture intensities
    // /1/rotary1 - /1/rotary4 -> fixture colors
    // /1/toggle1 - /1/toggle4 -> fixture on/off
    // /2/xy -> pan/tilt

    for (int32 i = 1; i <= 8; i++)
    {
        FRshipOSCMapping FaderMapping;
        FaderMapping.OSCAddress = FString::Printf(TEXT("/1/fader%d"), i);
        FaderMapping.TargetId = FString::Printf(TEXT("fixture:%d"), i);
        FaderMapping.FieldName = TEXT("intensity");
        FaderMapping.Direction = ERshipOSCMappingDirection::Input;
        FaderMapping.Description = FString::Printf(TEXT("TouchOSC Fader %d"), i);
        AddMapping(FaderMapping);
    }

    for (int32 i = 1; i <= 4; i++)
    {
        FRshipOSCMapping ToggleMapping;
        ToggleMapping.OSCAddress = FString::Printf(TEXT("/1/toggle%d"), i);
        ToggleMapping.TargetId = FString::Printf(TEXT("fixture:%d"), i);
        ToggleMapping.FieldName = TEXT("on");
        ToggleMapping.Direction = ERshipOSCMappingDirection::Input;
        ToggleMapping.Transform = ERshipOSCValueTransform::Toggle;
        ToggleMapping.Description = FString::Printf(TEXT("TouchOSC Toggle %d"), i);
        AddMapping(ToggleMapping);
    }

    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge: Created TouchOSC mappings"));
}

void URshipOSCBridge::CreateQLabMappings()
{
    // QLab common patterns
    // /cue/{number}/start -> trigger cue
    // /go -> go button
    // /stop -> stop all
    // /panic -> panic button

    FRshipOSCMapping GoMapping;
    GoMapping.OSCAddress = TEXT("/go");
    GoMapping.bIsAction = true;
    GoMapping.TargetId = TEXT("timecode:play");
    GoMapping.Direction = ERshipOSCMappingDirection::Input;
    GoMapping.Description = TEXT("QLab Go");
    AddMapping(GoMapping);

    FRshipOSCMapping StopMapping;
    StopMapping.OSCAddress = TEXT("/stop");
    StopMapping.bIsAction = true;
    StopMapping.TargetId = TEXT("timecode:stop");
    StopMapping.Direction = ERshipOSCMappingDirection::Input;
    StopMapping.Description = TEXT("QLab Stop");
    AddMapping(StopMapping);

    FRshipOSCMapping PanicMapping;
    PanicMapping.OSCAddress = TEXT("/panic");
    PanicMapping.bIsAction = true;
    PanicMapping.TargetId = TEXT("dmx:blackout");
    PanicMapping.Direction = ERshipOSCMappingDirection::Input;
    PanicMapping.Description = TEXT("QLab Panic/Blackout");
    AddMapping(PanicMapping);

    UE_LOG(LogRshipExec, Log, TEXT("OSCBridge: Created QLab mappings"));
}

// ============================================================================
// SENDING
// ============================================================================

void URshipOSCBridge::SendMessage(const FRshipOSCMessage& Message)
{
    TArray<uint8> Data = SerializeOSCMessage(Message);
    if (Data.Num() == 0)
    {
        return;
    }

    for (const FRshipOSCDestination& Dest : Destinations)
    {
        if (Dest.bEnabled)
        {
            SendToDestination(Data, Dest);
        }
    }

    MessagesSent++;
    OnMessageSent.Broadcast(Message);
}

void URshipOSCBridge::SendFloat(const FString& Address, float Value)
{
    FRshipOSCMessage Message;
    Message.Address = Address;

    FRshipOSCArgument Arg;
    Arg.Type = ERshipOSCArgumentType::Float;
    Arg.FloatValue = Value;
    Message.Arguments.Add(Arg);

    SendMessage(Message);
}

void URshipOSCBridge::SendInt(const FString& Address, int32 Value)
{
    FRshipOSCMessage Message;
    Message.Address = Address;

    FRshipOSCArgument Arg;
    Arg.Type = ERshipOSCArgumentType::Int32;
    Arg.IntValue = Value;
    Message.Arguments.Add(Arg);

    SendMessage(Message);
}

void URshipOSCBridge::SendString(const FString& Address, const FString& Value)
{
    FRshipOSCMessage Message;
    Message.Address = Address;

    FRshipOSCArgument Arg;
    Arg.Type = ERshipOSCArgumentType::String;
    Arg.StringValue = Value;
    Message.Arguments.Add(Arg);

    SendMessage(Message);
}

void URshipOSCBridge::SendColor(const FString& Address, FLinearColor Color)
{
    FRshipOSCMessage Message;
    Message.Address = Address;

    FRshipOSCArgument Arg;
    Arg.Type = ERshipOSCArgumentType::Color;
    Arg.ColorValue = Color.ToFColor(true);
    Message.Arguments.Add(Arg);

    SendMessage(Message);
}

void URshipOSCBridge::SendFloats(const FString& Address, const TArray<float>& Values)
{
    FRshipOSCMessage Message;
    Message.Address = Address;

    for (float V : Values)
    {
        FRshipOSCArgument Arg;
        Arg.Type = ERshipOSCArgumentType::Float;
        Arg.FloatValue = V;
        Message.Arguments.Add(Arg);
    }

    SendMessage(Message);
}

void URshipOSCBridge::SendMessageTo(const FRshipOSCMessage& Message, const FString& DestinationName)
{
    TArray<uint8> Data = SerializeOSCMessage(Message);
    if (Data.Num() == 0)
    {
        return;
    }

    for (const FRshipOSCDestination& Dest : Destinations)
    {
        if (Dest.Name == DestinationName && Dest.bEnabled)
        {
            SendToDestination(Data, Dest);
            MessagesSent++;
            OnMessageSent.Broadcast(Message);
            return;
        }
    }
}

void URshipOSCBridge::SendToDestination(const TArray<uint8>& Data, const FRshipOSCDestination& Destination)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return;
    }

    // Create a temporary socket for sending
    FSocket* SendSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("RshipOSCSend"), false);
    if (!SendSocket)
    {
        ErrorCount++;
        return;
    }

    // Parse destination address
    FIPv4Address DestIP;
    if (!FIPv4Address::Parse(Destination.IPAddress, DestIP))
    {
        SocketSubsystem->DestroySocket(SendSocket);
        ErrorCount++;
        OnError.Broadcast(FString::Printf(TEXT("Invalid IP address: %s"), *Destination.IPAddress), true);
        return;
    }

    FIPv4Endpoint Endpoint(DestIP, Destination.Port);
    int32 BytesSent = 0;
    if (!SendSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *Endpoint.ToInternetAddr()))
    {
        ErrorCount++;
        OnError.Broadcast(FString::Printf(TEXT("Failed to send to %s:%d"), *Destination.IPAddress, Destination.Port), true);
    }

    SocketSubsystem->DestroySocket(SendSocket);
}

// ============================================================================
// RECEIVING
// ============================================================================

void URshipOSCBridge::OnDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
    TArray<uint8> DataArray;
    DataArray.SetNum(Data->TotalSize());
    Data->Serialize(DataArray.GetData(), DataArray.Num());

    FRshipOSCMessage Message;
    if (ParseOSCMessage(DataArray, Message))
    {
        Message.SourceIP = Endpoint.Address.ToString();
        Message.SourcePort = Endpoint.Port;

        MessagesReceived++;

        // Process on game thread
        AsyncTask(ENamedThreads::GameThread, [this, Message]()
        {
            ProcessIncomingMessage(Message);
            OnMessageReceived.Broadcast(Message);
        });
    }
    else
    {
        ErrorCount++;
    }
}

void URshipOSCBridge::ProcessIncomingMessage(const FRshipOSCMessage& Message)
{
    // Find matching mappings
    for (const FRshipOSCMapping& Mapping : Mappings)
    {
        if (!Mapping.bEnabled) continue;
        if (Mapping.Direction == ERshipOSCMappingDirection::Output) continue;

        if (MatchesPattern(Message.Address, Mapping.OSCAddress))
        {
            // Extract value from first argument
            float Value = 0.0f;
            if (Message.Arguments.Num() > 0)
            {
                const FRshipOSCArgument& Arg = Message.Arguments[0];
                switch (Arg.Type)
                {
                    case ERshipOSCArgumentType::Float:
                        Value = Arg.FloatValue;
                        break;
                    case ERshipOSCArgumentType::Int32:
                        Value = (float)Arg.IntValue;
                        break;
                    case ERshipOSCArgumentType::BoolTrue:
                        Value = 1.0f;
                        break;
                    case ERshipOSCArgumentType::BoolFalse:
                    case ERshipOSCArgumentType::NilValue:
                        Value = 0.0f;
                        break;
                    default:
                        break;
                }
            }

            // Transform value
            Value = TransformValue(Value, Mapping);

            // Apply to target
            if (Mapping.bIsAction && Subsystem)
            {
                // Trigger action - TargetId format is "targetId:actionId"
                FString TargetId, ActionId;
                if (Mapping.TargetId.Split(TEXT(":"), &TargetId, &ActionId))
                {
                    // Build action data
                    TSharedRef<FJsonObject> ActionData = MakeShared<FJsonObject>();
                    ActionData->SetNumberField(Mapping.FieldName, Value);

                    // Find target and execute action - O(1) lookup
                    URshipTargetComponent* Comp = Subsystem->FindTargetComponent(TargetId);
                    if (Comp && Comp->TargetData)
                    {
                        AActor* Owner = Comp->GetOwner();
                        if (Owner)
                        {
                            bool bResult = Comp->TargetData->TakeAction(Owner, ActionId, ActionData);
                            if (bResult)
                            {
                                UE_LOG(LogRshipExec, Verbose, TEXT("OSCBridge: Executed action %s on %s"), *ActionId, *TargetId);
                            }
                        }
                    }
                }
                else
                {
                    UE_LOG(LogRshipExec, Warning, TEXT("OSCBridge: Invalid action target format '%s' - expected 'targetId:actionId'"), *Mapping.TargetId);
                }
            }
            else if (Subsystem)
            {
                // Create pulse data
                TSharedPtr<FJsonObject> PulseData = MakeShareable(new FJsonObject);
                PulseData->SetNumberField(Mapping.FieldName, Value);

                // For color messages with multiple arguments
                if (Message.Arguments.Num() >= 3 && Mapping.FieldName == TEXT("color"))
                {
                    TSharedPtr<FJsonObject> ColorObj = MakeShareable(new FJsonObject);
                    ColorObj->SetNumberField(TEXT("r"), Message.Arguments[0].FloatValue);
                    ColorObj->SetNumberField(TEXT("g"), Message.Arguments[1].FloatValue);
                    ColorObj->SetNumberField(TEXT("b"), Message.Arguments[2].FloatValue);
                    if (Message.Arguments.Num() > 3)
                    {
                        ColorObj->SetNumberField(TEXT("a"), Message.Arguments[3].FloatValue);
                    }
                    PulseData->SetObjectField(Mapping.FieldName, ColorObj);
                }

                // Route through pulse receiver
                URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
                if (Receiver)
                {
                    Receiver->ProcessPulseEvent(Mapping.TargetId, PulseData);
                }
            }
        }
    }
}

void URshipOSCBridge::OnPulseReceived(const FString& EmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data)
{
    // Check for output mappings
    for (const FRshipOSCMapping& Mapping : Mappings)
    {
        if (!Mapping.bEnabled) continue;
        if (Mapping.Direction == ERshipOSCMappingDirection::Input) continue;

        if (Mapping.TargetId == EmitterId && Data.IsValid())
        {
            // Extract value from pulse data
            float Value = 0.0f;
            if (Data->HasTypedField<EJson::Number>(Mapping.FieldName))
            {
                Value = Data->GetNumberField(Mapping.FieldName);
            }
            else if (Mapping.FieldName == TEXT("color") && Data->HasTypedField<EJson::Object>(TEXT("color")))
            {
                // Handle color extraction - send as RGB message
                const TSharedPtr<FJsonObject>* ColorObj;
                if (Data->TryGetObjectField(TEXT("color"), ColorObj))
                {
                    float R = (*ColorObj)->GetNumberField(TEXT("r"));
                    float G = (*ColorObj)->GetNumberField(TEXT("g"));
                    float B = (*ColorObj)->GetNumberField(TEXT("b"));

                    // Apply inverse transform to each component
                    R = InverseTransformValue(R, Mapping);
                    G = InverseTransformValue(G, Mapping);
                    B = InverseTransformValue(B, Mapping);

                    SendColor(Mapping.OSCAddress, FLinearColor(R, G, B));
                    continue;
                }
            }

            // Inverse transform for output (reverse the input transform)
            Value = InverseTransformValue(Value, Mapping);

            // Send OSC message
            SendFloat(Mapping.OSCAddress, Value);
        }
    }
}

float URshipOSCBridge::TransformValue(float Value, const FRshipOSCMapping& Mapping)
{
    switch (Mapping.Transform)
    {
        case ERshipOSCValueTransform::Direct:
            return Value;

        case ERshipOSCValueTransform::Scale:
            return Value * Mapping.Scale;

        case ERshipOSCValueTransform::RangeMap:
        {
            float InputRange = Mapping.InputRange.Y - Mapping.InputRange.X;
            if (FMath::IsNearlyZero(InputRange)) return Mapping.OutputRange.X;

            float Normalized = (Value - Mapping.InputRange.X) / InputRange;
            Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);
            return Mapping.OutputRange.X + Normalized * (Mapping.OutputRange.Y - Mapping.OutputRange.X);
        }

        case ERshipOSCValueTransform::Invert:
            return 1.0f - FMath::Clamp(Value, 0.0f, 1.0f);

        case ERshipOSCValueTransform::Toggle:
            return Value > 0.5f ? 1.0f : 0.0f;
    }

    return Value;
}

float URshipOSCBridge::InverseTransformValue(float Value, const FRshipOSCMapping& Mapping)
{
    // Reverse transform for output mappings
    switch (Mapping.Transform)
    {
        case ERshipOSCValueTransform::Direct:
            return Value;

        case ERshipOSCValueTransform::Scale:
            // Inverse of multiply is divide
            if (FMath::IsNearlyZero(Mapping.Scale)) return 0.0f;
            return Value / Mapping.Scale;

        case ERshipOSCValueTransform::RangeMap:
        {
            // Swap input/output ranges for inverse
            float OutputRange = Mapping.OutputRange.Y - Mapping.OutputRange.X;
            if (FMath::IsNearlyZero(OutputRange)) return Mapping.InputRange.X;

            float Normalized = (Value - Mapping.OutputRange.X) / OutputRange;
            Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);
            return Mapping.InputRange.X + Normalized * (Mapping.InputRange.Y - Mapping.InputRange.X);
        }

        case ERshipOSCValueTransform::Invert:
            // Invert is self-inverse
            return 1.0f - FMath::Clamp(Value, 0.0f, 1.0f);

        case ERshipOSCValueTransform::Toggle:
            // Toggle is self-inverse
            return Value > 0.5f ? 1.0f : 0.0f;
    }

    return Value;
}

bool URshipOSCBridge::MatchesPattern(const FString& Address, const FString& Pattern)
{
    // Simple wildcard matching
    if (Pattern.Contains(TEXT("*")) || Pattern.Contains(TEXT("?")))
    {
        // Convert to regex-style matching
        FString RegexPattern = Pattern;
        RegexPattern = RegexPattern.Replace(TEXT("*"), TEXT(".*"));
        RegexPattern = RegexPattern.Replace(TEXT("?"), TEXT("."));

        FRegexPattern Regex(RegexPattern);
        FRegexMatcher Matcher(Regex, Address);
        return Matcher.FindNext();
    }

    return Address == Pattern;
}

// ============================================================================
// OSC PARSING
// ============================================================================

bool URshipOSCBridge::ParseOSCMessage(const TArray<uint8>& Data, FRshipOSCMessage& OutMessage)
{
    if (Data.Num() < 4) return false;

    int32 Offset = 0;

    // Read address
    OutMessage.Address = ReadString(Data, Offset);
    if (OutMessage.Address.IsEmpty() || OutMessage.Address[0] != '/')
    {
        return false;
    }

    // Read type tag string
    if (Offset >= Data.Num()) return true;  // No arguments

    FString TypeTags = ReadString(Data, Offset);
    if (TypeTags.IsEmpty() || TypeTags[0] != ',')
    {
        return true;  // No type tags = no arguments
    }

    // Parse arguments based on type tags
    for (int32 i = 1; i < TypeTags.Len(); i++)
    {
        TCHAR TypeTag = TypeTags[i];
        FRshipOSCArgument Arg;

        switch (TypeTag)
        {
            case 'i':
                Arg.Type = ERshipOSCArgumentType::Int32;
                Arg.IntValue = ReadInt32(Data, Offset);
                break;

            case 'f':
                Arg.Type = ERshipOSCArgumentType::Float;
                Arg.FloatValue = ReadFloat(Data, Offset);
                break;

            case 's':
                Arg.Type = ERshipOSCArgumentType::String;
                Arg.StringValue = ReadString(Data, Offset);
                break;

            case 'r':
                Arg.Type = ERshipOSCArgumentType::Color;
                Arg.ColorValue = ReadColor(Data, Offset);
                break;

            case 'T':
                Arg.Type = ERshipOSCArgumentType::BoolTrue;
                break;

            case 'F':
                Arg.Type = ERshipOSCArgumentType::BoolFalse;
                break;

            case 'N':
                Arg.Type = ERshipOSCArgumentType::NilValue;
                break;

            default:
                // Skip unknown types
                continue;
        }

        OutMessage.Arguments.Add(Arg);
    }

    return true;
}

int32 URshipOSCBridge::ReadInt32(const TArray<uint8>& Data, int32& Offset)
{
    if (Offset + 4 > Data.Num()) return 0;

    // Big-endian
    int32 Value = (Data[Offset] << 24) | (Data[Offset + 1] << 16) | (Data[Offset + 2] << 8) | Data[Offset + 3];
    Offset += 4;
    return Value;
}

float URshipOSCBridge::ReadFloat(const TArray<uint8>& Data, int32& Offset)
{
    int32 IntBits = ReadInt32(Data, Offset);
    return *reinterpret_cast<float*>(&IntBits);
}

FString URshipOSCBridge::ReadString(const TArray<uint8>& Data, int32& Offset)
{
    FString Result;
    while (Offset < Data.Num() && Data[Offset] != 0)
    {
        Result.AppendChar((TCHAR)Data[Offset]);
        Offset++;
    }

    // Skip null terminator and padding
    Offset++;
    while (Offset % 4 != 0 && Offset < Data.Num())
    {
        Offset++;
    }

    return Result;
}

FColor URshipOSCBridge::ReadColor(const TArray<uint8>& Data, int32& Offset)
{
    if (Offset + 4 > Data.Num()) return FColor::White;

    FColor Color;
    Color.R = Data[Offset];
    Color.G = Data[Offset + 1];
    Color.B = Data[Offset + 2];
    Color.A = Data[Offset + 3];
    Offset += 4;
    return Color;
}

// ============================================================================
// OSC SERIALIZATION
// ============================================================================

TArray<uint8> URshipOSCBridge::SerializeOSCMessage(const FRshipOSCMessage& Message)
{
    TArray<uint8> Data;

    // Write address
    WriteString(Data, Message.Address);

    // Build type tag string
    FString TypeTags = TEXT(",");
    for (const FRshipOSCArgument& Arg : Message.Arguments)
    {
        switch (Arg.Type)
        {
            case ERshipOSCArgumentType::Int32: TypeTags.AppendChar('i'); break;
            case ERshipOSCArgumentType::Float: TypeTags.AppendChar('f'); break;
            case ERshipOSCArgumentType::String: TypeTags.AppendChar('s'); break;
            case ERshipOSCArgumentType::Color: TypeTags.AppendChar('r'); break;
            case ERshipOSCArgumentType::BoolTrue: TypeTags.AppendChar('T'); break;
            case ERshipOSCArgumentType::BoolFalse: TypeTags.AppendChar('F'); break;
            case ERshipOSCArgumentType::NilValue: TypeTags.AppendChar('N'); break;
            default: break;
        }
    }

    WriteString(Data, TypeTags);

    // Write arguments
    for (const FRshipOSCArgument& Arg : Message.Arguments)
    {
        switch (Arg.Type)
        {
            case ERshipOSCArgumentType::Int32:
                WriteInt32(Data, Arg.IntValue);
                break;

            case ERshipOSCArgumentType::Float:
                WriteFloat(Data, Arg.FloatValue);
                break;

            case ERshipOSCArgumentType::String:
                WriteString(Data, Arg.StringValue);
                break;

            case ERshipOSCArgumentType::Color:
                WriteColor(Data, Arg.ColorValue);
                break;

            default:
                // No data for True, False, Nil
                break;
        }
    }

    return Data;
}

void URshipOSCBridge::WriteInt32(TArray<uint8>& Data, int32 Value)
{
    // Big-endian
    Data.Add((Value >> 24) & 0xFF);
    Data.Add((Value >> 16) & 0xFF);
    Data.Add((Value >> 8) & 0xFF);
    Data.Add(Value & 0xFF);
}

void URshipOSCBridge::WriteFloat(TArray<uint8>& Data, float Value)
{
    int32 IntBits = *reinterpret_cast<int32*>(&Value);
    WriteInt32(Data, IntBits);
}

void URshipOSCBridge::WriteString(TArray<uint8>& Data, const FString& Value)
{
    // Write characters
    for (TCHAR Char : Value)
    {
        Data.Add((uint8)Char);
    }

    // Null terminator
    Data.Add(0);

    // Pad to 4 bytes
    PadToFourBytes(Data);
}

void URshipOSCBridge::WriteColor(TArray<uint8>& Data, const FColor& Color)
{
    Data.Add(Color.R);
    Data.Add(Color.G);
    Data.Add(Color.B);
    Data.Add(Color.A);
}

void URshipOSCBridge::PadToFourBytes(TArray<uint8>& Data)
{
    while (Data.Num() % 4 != 0)
    {
        Data.Add(0);
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

void URshipOSCBridge::ResetStats()
{
    MessagesReceived = 0;
    MessagesSent = 0;
    ErrorCount = 0;
}
