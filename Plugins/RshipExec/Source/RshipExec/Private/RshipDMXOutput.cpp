// Rship DMX Output Implementation

#include "RshipDMXOutput.h"
#include "RshipSubsystem.h"
#include "RshipFixtureManager.h"
#include "RshipCalibrationTypes.h"
#include "Logs.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"

void URshipDMXOutput::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

    if (Subsystem)
    {
        FixtureManager = Subsystem->GetFixtureManager();
    }

    CreateDefaultProfiles();

    UE_LOG(LogRshipExec, Log, TEXT("DMXOutput initialized with %d profiles"), Profiles.Num());
}

void URshipDMXOutput::Shutdown()
{
    bOutputEnabled = false;
    FixtureOutputs.Empty();
    UniverseBuffers.Empty();
    Profiles.Empty();

    Subsystem = nullptr;
    FixtureManager = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("DMXOutput shutdown"));
}

void URshipDMXOutput::Tick(float DeltaTime)
{
    if (!bOutputEnabled || bBlackout) return;

    // Update all fixture outputs
    for (const FRshipDMXFixtureOutput& Output : FixtureOutputs)
    {
        if (Output.bEnabled)
        {
            UpdateFixtureToBuffer(Output);
        }
    }

    // Send at configured frame rate
    double CurrentTime = FPlatformTime::Seconds();
    double FrameInterval = 1.0 / FrameRate;

    if (CurrentTime - LastSendTime >= FrameInterval)
    {
        SendDirtyUniverses();
        LastSendTime = CurrentTime;
    }
}

void URshipDMXOutput::CreateDefaultProfiles()
{
    // Generic Dimmer (1 channel)
    {
        FRshipDMXProfile Profile;
        Profile.Name = TEXT("Dimmer");
        Profile.ChannelCount = 1;

        FRshipDMXChannel Ch;
        Ch.ChannelOffset = 0;
        Ch.Type = ERshipDMXChannelType::Dimmer;
        Profile.Channels.Add(Ch);

        RegisterProfile(Profile);
    }

    // Generic RGB (3 channels)
    {
        FRshipDMXProfile Profile;
        Profile.Name = TEXT("Generic RGB");
        Profile.ChannelCount = 3;

        FRshipDMXChannel ChR; ChR.ChannelOffset = 0; ChR.Type = ERshipDMXChannelType::Red;
        FRshipDMXChannel ChG; ChG.ChannelOffset = 1; ChG.Type = ERshipDMXChannelType::Green;
        FRshipDMXChannel ChB; ChB.ChannelOffset = 2; ChB.Type = ERshipDMXChannelType::Blue;

        Profile.Channels.Add(ChR);
        Profile.Channels.Add(ChG);
        Profile.Channels.Add(ChB);

        RegisterProfile(Profile);
    }

    // Dimmer + RGB (4 channels)
    {
        FRshipDMXProfile Profile;
        Profile.Name = TEXT("Dimmer RGB");
        Profile.ChannelCount = 4;

        FRshipDMXChannel ChD; ChD.ChannelOffset = 0; ChD.Type = ERshipDMXChannelType::Dimmer;
        FRshipDMXChannel ChR; ChR.ChannelOffset = 1; ChR.Type = ERshipDMXChannelType::Red;
        FRshipDMXChannel ChG; ChG.ChannelOffset = 2; ChG.Type = ERshipDMXChannelType::Green;
        FRshipDMXChannel ChB; ChB.ChannelOffset = 3; ChB.Type = ERshipDMXChannelType::Blue;

        Profile.Channels.Add(ChD);
        Profile.Channels.Add(ChR);
        Profile.Channels.Add(ChG);
        Profile.Channels.Add(ChB);

        RegisterProfile(Profile);
    }

    // RGBW (4 channels)
    {
        FRshipDMXProfile Profile;
        Profile.Name = TEXT("RGBW");
        Profile.ChannelCount = 4;

        FRshipDMXChannel ChR; ChR.ChannelOffset = 0; ChR.Type = ERshipDMXChannelType::Red;
        FRshipDMXChannel ChG; ChG.ChannelOffset = 1; ChG.Type = ERshipDMXChannelType::Green;
        FRshipDMXChannel ChB; ChB.ChannelOffset = 2; ChB.Type = ERshipDMXChannelType::Blue;
        FRshipDMXChannel ChW; ChW.ChannelOffset = 3; ChW.Type = ERshipDMXChannelType::White;

        Profile.Channels.Add(ChR);
        Profile.Channels.Add(ChG);
        Profile.Channels.Add(ChB);
        Profile.Channels.Add(ChW);

        RegisterProfile(Profile);
    }

    // Dimmer + RGBW (5 channels)
    {
        FRshipDMXProfile Profile;
        Profile.Name = TEXT("Dimmer RGBW");
        Profile.ChannelCount = 5;

        FRshipDMXChannel ChD; ChD.ChannelOffset = 0; ChD.Type = ERshipDMXChannelType::Dimmer;
        FRshipDMXChannel ChR; ChR.ChannelOffset = 1; ChR.Type = ERshipDMXChannelType::Red;
        FRshipDMXChannel ChG; ChG.ChannelOffset = 2; ChG.Type = ERshipDMXChannelType::Green;
        FRshipDMXChannel ChB; ChB.ChannelOffset = 3; ChB.Type = ERshipDMXChannelType::Blue;
        FRshipDMXChannel ChW; ChW.ChannelOffset = 4; ChW.Type = ERshipDMXChannelType::White;

        Profile.Channels.Add(ChD);
        Profile.Channels.Add(ChR);
        Profile.Channels.Add(ChG);
        Profile.Channels.Add(ChB);
        Profile.Channels.Add(ChW);

        RegisterProfile(Profile);
    }

    // Moving Head Basic (16 channels)
    {
        FRshipDMXProfile Profile;
        Profile.Name = TEXT("Moving Head Basic");
        Profile.ChannelCount = 16;

        Profile.Channels.Add({ 0, ERshipDMXChannelType::Pan, TEXT(""), 128, false, true });
        Profile.Channels.Add({ 1, ERshipDMXChannelType::PanFine, TEXT(""), 0, false, false });
        Profile.Channels.Add({ 2, ERshipDMXChannelType::Tilt, TEXT(""), 128, false, true });
        Profile.Channels.Add({ 3, ERshipDMXChannelType::TiltFine, TEXT(""), 0, false, false });
        Profile.Channels.Add({ 4, ERshipDMXChannelType::Dimmer, TEXT(""), 0, false, false });
        Profile.Channels.Add({ 5, ERshipDMXChannelType::Shutter, TEXT(""), 0, false, false });
        Profile.Channels.Add({ 6, ERshipDMXChannelType::Red, TEXT(""), 255, false, false });
        Profile.Channels.Add({ 7, ERshipDMXChannelType::Green, TEXT(""), 255, false, false });
        Profile.Channels.Add({ 8, ERshipDMXChannelType::Blue, TEXT(""), 255, false, false });
        Profile.Channels.Add({ 9, ERshipDMXChannelType::White, TEXT(""), 0, false, false });
        Profile.Channels.Add({ 10, ERshipDMXChannelType::ColorWheel, TEXT(""), 0, false, false });
        Profile.Channels.Add({ 11, ERshipDMXChannelType::Gobo, TEXT(""), 0, false, false });
        Profile.Channels.Add({ 12, ERshipDMXChannelType::Zoom, TEXT(""), 128, false, false });
        Profile.Channels.Add({ 13, ERshipDMXChannelType::Focus, TEXT(""), 128, false, false });
        Profile.Channels.Add({ 14, ERshipDMXChannelType::Prism, TEXT(""), 0, false, false });
        Profile.Channels.Add({ 15, ERshipDMXChannelType::Control, TEXT(""), 0, false, false });

        RegisterProfile(Profile);
    }
}

void URshipDMXOutput::AddFixtureOutput(const FRshipDMXFixtureOutput& Output)
{
    // Remove existing output for this fixture
    RemoveFixtureOutput(Output.FixtureId);

    FixtureOutputs.Add(Output);

    UE_LOG(LogRshipExec, Log, TEXT("DMXOutput: Added fixture %s -> Universe %d, Address %d"),
        *Output.FixtureId, Output.Universe, Output.StartAddress);
}

void URshipDMXOutput::RemoveFixtureOutput(const FString& FixtureId)
{
    FixtureOutputs.RemoveAll([&](const FRshipDMXFixtureOutput& O) { return O.FixtureId == FixtureId; });
}

void URshipDMXOutput::ClearAllOutputs()
{
    FixtureOutputs.Empty();
    UniverseBuffers.Empty();
}

void URshipDMXOutput::RegisterProfile(const FRshipDMXProfile& Profile)
{
    Profiles.Add(Profile.Name, Profile);
}

bool URshipDMXOutput::GetProfile(const FString& Name, FRshipDMXProfile& OutProfile) const
{
    if (const FRshipDMXProfile* Found = Profiles.Find(Name))
    {
        OutProfile = *Found;
        return true;
    }
    return false;
}

TArray<FRshipDMXProfile> URshipDMXOutput::GetAllProfiles() const
{
    TArray<FRshipDMXProfile> Result;
    Profiles.GenerateValueArray(Result);
    return Result;
}

int32 URshipDMXOutput::AutoMapAllFixtures(int32 StartUniverse, int32 StartAddress, const FString& DefaultProfile)
{
    if (!FixtureManager) return 0;

    FRshipDMXProfile Profile;
    if (!GetProfile(DefaultProfile, Profile))
    {
        if (!GetProfile(TEXT("Generic RGB"), Profile))
        {
            return 0;
        }
    }

    TArray<FRshipFixtureInfo> Fixtures = FixtureManager->GetAllFixtures();
    int32 CurrentUniverse = StartUniverse;
    int32 CurrentAddress = StartAddress;
    int32 MappedCount = 0;

    for (const FRshipFixtureInfo& Fixture : Fixtures)
    {
        // Check if we need to move to next universe
        if (CurrentAddress + Profile.ChannelCount > 513)
        {
            CurrentUniverse++;
            CurrentAddress = 1;
        }

        FRshipDMXFixtureOutput Output;
        Output.FixtureId = Fixture.Id;
        Output.Universe = CurrentUniverse;
        Output.StartAddress = CurrentAddress;
        Output.ProfileName = DefaultProfile;
        Output.bEnabled = true;

        AddFixtureOutput(Output);

        CurrentAddress += Profile.ChannelCount;
        MappedCount++;
    }

    UE_LOG(LogRshipExec, Log, TEXT("DMXOutput: Auto-mapped %d fixtures"), MappedCount);
    return MappedCount;
}

int32 URshipDMXOutput::AutoMapRshipUniverse(int32 RshipUniverse, int32 DMXUniverse)
{
    if (!FixtureManager) return 0;

    TArray<FRshipFixtureInfo> Fixtures = FixtureManager->GetAllFixtures();
    int32 MappedCount = 0;

    for (const FRshipFixtureInfo& Fixture : Fixtures)
    {
        if (Fixture.Universe == RshipUniverse)
        {
            FRshipDMXFixtureOutput Output;
            Output.FixtureId = Fixture.Id;
            Output.Universe = DMXUniverse;
            Output.StartAddress = Fixture.StartAddress;
            Output.ProfileName = TEXT("Generic RGB");
            Output.bEnabled = true;

            AddFixtureOutput(Output);
            MappedCount++;
        }
    }

    return MappedCount;
}

void URshipDMXOutput::SetOutputEnabled(bool bEnabled)
{
    bOutputEnabled = bEnabled;
    if (bEnabled)
    {
        LastSendTime = 0.0;  // Force immediate send
    }
}

void URshipDMXOutput::SetGlobalMaster(float Master)
{
    GlobalMaster = FMath::Clamp(Master, 0.0f, 1.0f);
}

void URshipDMXOutput::Blackout()
{
    bBlackout = true;

    // Zero all universe buffers
    for (auto& Pair : UniverseBuffers)
    {
        FMemory::Memzero(Pair.Value.Channels.GetData(), 512);
        Pair.Value.bDirty = true;
    }

    SendDirtyUniverses();
}

void URshipDMXOutput::ReleaseBlackout()
{
    bBlackout = false;
    // Next tick will update with current values
}

void URshipDMXOutput::SetChannel(int32 Universe, int32 Channel, uint8 Value)
{
    if (Channel < 1 || Channel > 512) return;

    FRshipDMXUniverseBuffer& Buffer = GetOrCreateBuffer(Universe);
    Buffer.Channels[Channel - 1] = Value;
    Buffer.bDirty = true;
}

void URshipDMXOutput::SetChannels(int32 Universe, int32 StartChannel, const TArray<uint8>& Values)
{
    if (StartChannel < 1 || StartChannel > 512) return;

    FRshipDMXUniverseBuffer& Buffer = GetOrCreateBuffer(Universe);

    int32 NumToCopy = FMath::Min(Values.Num(), 512 - StartChannel + 1);
    FMemory::Memcpy(&Buffer.Channels[StartChannel - 1], Values.GetData(), NumToCopy);
    Buffer.bDirty = true;
}

uint8 URshipDMXOutput::GetChannel(int32 Universe, int32 Channel) const
{
    if (Channel < 1 || Channel > 512) return 0;

    if (const FRshipDMXUniverseBuffer* Buffer = UniverseBuffers.Find(Universe))
    {
        return Buffer->Channels[Channel - 1];
    }
    return 0;
}

TArray<uint8> URshipDMXOutput::GetUniverseChannels(int32 Universe) const
{
    if (const FRshipDMXUniverseBuffer* Buffer = UniverseBuffers.Find(Universe))
    {
        return Buffer->Channels;
    }

    TArray<uint8> Empty;
    Empty.SetNumZeroed(512);
    return Empty;
}

void URshipDMXOutput::SetFrameRate(float Hz)
{
    FrameRate = FMath::Clamp(Hz, 1.0f, 44.0f);
}

void URshipDMXOutput::SetArtNetDestination(const FString& IP)
{
    ArtNetDestination = IP;
}

void URshipDMXOutput::SetSACNMulticast(bool bEnable)
{
    bSACNMulticast = bEnable;
}

void URshipDMXOutput::UpdateFixtureToBuffer(const FRshipDMXFixtureOutput& Output)
{
    if (!FixtureManager) return;

    FRshipFixtureInfo Fixture;
    if (!FixtureManager->GetFixture(Output.FixtureId, Fixture))
    {
        return;
    }

    // Get profile
    FRshipDMXProfile Profile;
    if (!Output.ProfileName.IsEmpty())
    {
        if (!GetProfile(Output.ProfileName, Profile))
        {
            Profile = Output.CustomProfile;
        }
    }
    else
    {
        Profile = Output.CustomProfile;
    }

    if (Profile.Channels.Num() == 0) return;

    FRshipDMXUniverseBuffer& Buffer = GetOrCreateBuffer(Output.Universe);

    // Map each channel
    for (const FRshipDMXChannel& Channel : Profile.Channels)
    {
        int32 DMXChannel = Output.StartAddress + Channel.ChannelOffset;
        if (DMXChannel < 1 || DMXChannel > 512) continue;

        float NormalizedValue = 0.0f;

        switch (Channel.Type)
        {
            case ERshipDMXChannelType::Dimmer:
                NormalizedValue = Fixture.Intensity * GlobalMaster * Output.MasterDimmer;
                break;

            case ERshipDMXChannelType::Red:
                NormalizedValue = Fixture.Color.R * Fixture.Intensity * GlobalMaster * Output.MasterDimmer;
                break;

            case ERshipDMXChannelType::Green:
                NormalizedValue = Fixture.Color.G * Fixture.Intensity * GlobalMaster * Output.MasterDimmer;
                break;

            case ERshipDMXChannelType::Blue:
                NormalizedValue = Fixture.Color.B * Fixture.Intensity * GlobalMaster * Output.MasterDimmer;
                break;

            case ERshipDMXChannelType::White:
                // Simple white calculation from RGB
                NormalizedValue = FMath::Min3(Fixture.Color.R, Fixture.Color.G, Fixture.Color.B) *
                                  Fixture.Intensity * GlobalMaster * Output.MasterDimmer;
                break;

            case ERshipDMXChannelType::Pan:
                NormalizedValue = (Fixture.Pan + 270.0f) / 540.0f;  // Assume ±270° range
                break;

            case ERshipDMXChannelType::Tilt:
                NormalizedValue = (Fixture.Tilt + 135.0f) / 270.0f;  // Assume ±135° range
                break;

            case ERshipDMXChannelType::Zoom:
                NormalizedValue = Fixture.Zoom;
                break;

            case ERshipDMXChannelType::Focus:
                NormalizedValue = Fixture.Focus;
                break;

            default:
                NormalizedValue = Channel.DefaultValue / 255.0f;
                break;
        }

        uint8 DMXValue = MapChannelValue(Channel, NormalizedValue);
        Buffer.Channels[DMXChannel - 1] = DMXValue;

        // Handle 16-bit channels
        if (Channel.b16Bit && DMXChannel < 512)
        {
            uint16 Value16 = FMath::Clamp((int32)(NormalizedValue * 65535.0f), 0, 65535);
            Buffer.Channels[DMXChannel - 1] = (Value16 >> 8) & 0xFF;
            Buffer.Channels[DMXChannel] = Value16 & 0xFF;
        }
    }

    Buffer.bDirty = true;
}

uint8 URshipDMXOutput::MapChannelValue(const FRshipDMXChannel& Channel, float NormalizedValue)
{
    int32 Value = FMath::RoundToInt(FMath::Clamp(NormalizedValue, 0.0f, 1.0f) * 255.0f);

    if (Channel.bInvert)
    {
        Value = 255 - Value;
    }

    return (uint8)Value;
}

FRshipDMXUniverseBuffer& URshipDMXOutput::GetOrCreateBuffer(int32 Universe)
{
    if (!UniverseBuffers.Contains(Universe))
    {
        FRshipDMXUniverseBuffer NewBuffer;
        NewBuffer.Universe = Universe;
        UniverseBuffers.Add(Universe, NewBuffer);
    }
    return UniverseBuffers[Universe];
}

void URshipDMXOutput::SendDirtyUniverses()
{
    for (auto& Pair : UniverseBuffers)
    {
        if (Pair.Value.bDirty)
        {
            // Send via both protocols
            SendArtNet(Pair.Key, Pair.Value.Channels);

            if (bSACNMulticast)
            {
                SendSACN(Pair.Key, Pair.Value.Channels);
            }

            OnUniverseUpdated.Broadcast(Pair.Key, Pair.Value.Channels);
            Pair.Value.bDirty = false;
        }
    }
}

void URshipDMXOutput::SendArtNet(int32 Universe, const TArray<uint8>& Channels)
{
    // Art-Net packet structure
    // Header: "Art-Net\0" (8 bytes)
    // OpCode: 0x5000 (ArtDmx) (2 bytes little endian)
    // ProtVer: 14 (2 bytes big endian)
    // Sequence: 0 (1 byte)
    // Physical: 0 (1 byte)
    // SubUni: universe low byte (1 byte)
    // Net: universe high byte (1 byte)
    // Length: 512 (2 bytes big endian)
    // Data: 512 bytes

    TArray<uint8> Packet;
    Packet.SetNumZeroed(18 + 512);

    // Header
    FMemory::Memcpy(Packet.GetData(), "Art-Net", 8);

    // OpCode (ArtDmx = 0x5000)
    Packet[8] = 0x00;
    Packet[9] = 0x50;

    // Protocol version (14)
    Packet[10] = 0x00;
    Packet[11] = 14;

    // Sequence (could be used for ordering)
    static uint8 Sequence = 0;
    Packet[12] = Sequence++;

    // Physical port
    Packet[13] = 0;

    // Universe (15-bit, split into SubUni and Net)
    int32 ArtNetUniverse = Universe - 1;  // Art-Net is 0-based
    Packet[14] = ArtNetUniverse & 0xFF;
    Packet[15] = (ArtNetUniverse >> 8) & 0x7F;

    // Length (512, big endian)
    Packet[16] = 0x02;
    Packet[17] = 0x00;

    // DMX data
    FMemory::Memcpy(&Packet[18], Channels.GetData(), FMath::Min(Channels.Num(), 512));

    // Send UDP packet
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem) return;

    FSocket* Socket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("RshipArtNet"), false);
    if (!Socket) return;

    Socket->SetReuseAddr(true);
    Socket->SetBroadcast(true);

    TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid;
    Addr->SetIp(*ArtNetDestination, bIsValid);
    if (bIsValid)
    {
        Addr->SetPort(6454);  // Art-Net port

        int32 BytesSent = 0;
        Socket->SendTo(Packet.GetData(), Packet.Num(), BytesSent, *Addr);
    }

    Socket->Close();
    SocketSubsystem->DestroySocket(Socket);
}

void URshipDMXOutput::SendSACN(int32 Universe, const TArray<uint8>& Channels)
{
    // sACN (E1.31) packet structure is more complex
    // For now, this is a simplified implementation

    // sACN uses multicast addresses: 239.255.{universe high}.{universe low}
    // Port: 5568

    TArray<uint8> Packet;
    Packet.SetNumZeroed(126 + 512);  // Root + framing + DMP layers

    // Root Layer
    // Preamble Size (0x0010)
    Packet[0] = 0x00;
    Packet[1] = 0x10;

    // Post-amble Size (0x0000)
    Packet[2] = 0x00;
    Packet[3] = 0x00;

    // ACN Packet Identifier
    FMemory::Memcpy(&Packet[4], "ASC-E1.17\0\0\0", 12);

    // Flags and Length (Root layer)
    uint16 RootLength = 0x7000 | (110 + 512);  // High nibble = 0x7
    Packet[16] = (RootLength >> 8) & 0xFF;
    Packet[17] = RootLength & 0xFF;

    // Vector (sACN uses 0x00000004 for E1.31)
    Packet[18] = 0x00;
    Packet[19] = 0x00;
    Packet[20] = 0x00;
    Packet[21] = 0x04;

    // CID (Component Identifier) - 16 bytes UUID
    // Using a fixed CID for simplicity
    static uint8 CID[16] = { 0x52, 0x73, 0x68, 0x69, 0x70, 0x44, 0x4D, 0x58,
                             0x4F, 0x75, 0x74, 0x70, 0x75, 0x74, 0x00, 0x01 };
    FMemory::Memcpy(&Packet[22], CID, 16);

    // Framing Layer (starts at offset 38)
    uint16 FramingLength = 0x7000 | (88 + 512);
    Packet[38] = (FramingLength >> 8) & 0xFF;
    Packet[39] = FramingLength & 0xFF;

    // Vector (0x00000002 for DMP)
    Packet[40] = 0x00;
    Packet[41] = 0x00;
    Packet[42] = 0x00;
    Packet[43] = 0x02;

    // Source Name (64 bytes)
    const char* SourceName = "Rship DMX Output";
    FMemory::Memcpy(&Packet[44], SourceName, FMath::Min(strlen(SourceName), (size_t)63));

    // Priority (108)
    Packet[108] = 100;

    // Sync Address (109-110)
    Packet[109] = 0;
    Packet[110] = 0;

    // Sequence (111)
    static uint8 Sequence = 0;
    Packet[111] = Sequence++;

    // Options (112)
    Packet[112] = 0;

    // Universe (113-114)
    Packet[113] = (Universe >> 8) & 0xFF;
    Packet[114] = Universe & 0xFF;

    // DMP Layer (starts at offset 115)
    uint16 DMPLength = 0x7000 | (11 + 512);
    Packet[115] = (DMPLength >> 8) & 0xFF;
    Packet[116] = DMPLength & 0xFF;

    // Vector (0x02)
    Packet[117] = 0x02;

    // Address Type & Data Type
    Packet[118] = 0xA1;

    // First Property Address
    Packet[119] = 0x00;
    Packet[120] = 0x00;

    // Address Increment
    Packet[121] = 0x00;
    Packet[122] = 0x01;

    // Property value count (513 = start code + 512 channels)
    Packet[123] = 0x02;
    Packet[124] = 0x01;

    // Start code
    Packet[125] = 0x00;

    // DMX data
    FMemory::Memcpy(&Packet[126], Channels.GetData(), FMath::Min(Channels.Num(), 512));

    // Send to multicast address
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem) return;

    FSocket* Socket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("RshipSACN"), false);
    if (!Socket) return;

    Socket->SetReuseAddr(true);
    Socket->SetMulticastTtl(1);

    // sACN multicast address: 239.255.{high}.{low}
    FString MulticastAddr = FString::Printf(TEXT("239.255.%d.%d"),
        (Universe >> 8) & 0xFF, Universe & 0xFF);

    TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
    bool bIsValid;
    Addr->SetIp(*MulticastAddr, bIsValid);
    if (bIsValid)
    {
        Addr->SetPort(5568);  // sACN port

        int32 BytesSent = 0;
        Socket->SendTo(Packet.GetData(), Packet.Num(), BytesSent, *Addr);
    }

    Socket->Close();
    SocketSubsystem->DestroySocket(Socket);
}
