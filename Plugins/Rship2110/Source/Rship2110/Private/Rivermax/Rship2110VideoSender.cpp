// Copyright Rocketship. All Rights Reserved.

#include "Rivermax/Rship2110VideoSender.h"
#include "Rivermax/RivermaxManager.h"
#include "PTP/RshipPTPService.h"
#include "Rship2110.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Engine/TextureRenderTarget2D.h"

#if RSHIP_RIVERMAX_AVAILABLE
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>  // For inet_pton
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "rivermax_api.h"
#endif

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

int32 FRship2110VideoFormat::GetBytesPerLine() const
{
    int32 BitsPerPixel = 0;
    int32 BitsPerSample = GetBitDepthInt();

    switch (ColorFormat)
    {
        case ERship2110ColorFormat::YCbCr_422:
            BitsPerPixel = BitsPerSample * 2;  // 2 samples per pixel average
            break;
        case ERship2110ColorFormat::YCbCr_444:
        case ERship2110ColorFormat::RGB_444:
            BitsPerPixel = BitsPerSample * 3;
            break;
        case ERship2110ColorFormat::RGBA_4444:
            BitsPerPixel = BitsPerSample * 4;
            break;
    }

    return (Width * BitsPerPixel + 7) / 8;
}

int64 FRship2110VideoFormat::GetFrameSizeBytes() const
{
    return static_cast<int64>(GetBytesPerLine()) * Height;
}

FString FRship2110VideoFormat::GetSDPMediaType() const
{
    return TEXT("video/raw");
}

FString FRship2110VideoFormat::GetSampling() const
{
    switch (ColorFormat)
    {
        case ERship2110ColorFormat::YCbCr_422:
            return TEXT("YCbCr-4:2:2");
        case ERship2110ColorFormat::YCbCr_444:
            return TEXT("YCbCr-4:4:4");
        case ERship2110ColorFormat::RGB_444:
            return TEXT("RGB");
        case ERship2110ColorFormat::RGBA_4444:
            return TEXT("RGBA");
        default:
            return TEXT("YCbCr-4:2:2");
    }
}

// ============================================================================
// VIDEO SENDER IMPLEMENTATION
// ============================================================================

bool URship2110VideoSender::Initialize(
    URivermaxManager* InManager,
    URshipPTPService* InPTPService,
    const FRship2110VideoFormat& InVideoFormat,
    const FRship2110TransportParams& InTransportParams)
{
    if (!InManager)
    {
        UE_LOG(LogRship2110, Error, TEXT("VideoSender: Invalid manager"));
        return false;
    }

    Manager = InManager;
    PTPService = InPTPService;
    VideoFormat = InVideoFormat;
    TransportParams = InTransportParams;

    // Generate SSRC if not specified
    if (TransportParams.SSRC == 0)
    {
        TransportParams.SSRC = FMath::Rand();
    }
    SSRC = TransportParams.SSRC;

    // Allocate buffers
    if (!AllocateBuffers())
    {
        UE_LOG(LogRship2110, Error, TEXT("VideoSender: Failed to allocate buffers"));
        return false;
    }

#if RSHIP_RIVERMAX_AVAILABLE
    // Create Rivermax stream
    if (!CreateRivermaxStream())
    {
        UE_LOG(LogRship2110, Warning, TEXT("VideoSender: Failed to create Rivermax stream, using stub"));
    }
#endif

    State = ERship2110StreamState::Stopped;

    UE_LOG(LogRship2110, Log, TEXT("VideoSender: Initialized %dx%d @ %.2f fps"),
           VideoFormat.Width, VideoFormat.Height, VideoFormat.GetFrameRateDecimal());

    return true;
}

void URship2110VideoSender::Shutdown()
{
    StopStream();

#if RSHIP_RIVERMAX_AVAILABLE
    DestroyRivermaxStream();
#endif

    FreeBuffers();

    Manager = nullptr;
    PTPService = nullptr;

    UE_LOG(LogRship2110, Log, TEXT("VideoSender: Shutdown complete"));
}

void URship2110VideoSender::Tick()
{
    if (State != ERship2110StreamState::Running)
    {
        return;
    }

    // Capture and transmit frame
    CaptureFrame();
    TransmitFrame();
}

bool URship2110VideoSender::StartStream()
{
    if (State == ERship2110StreamState::Running)
    {
        return true;
    }

    // Reset statistics
    ResetStatistics();

    // Initialize sequence number
    CurrentSequenceNumber = FMath::Rand() & 0xFFFF;

    // Get initial RTP timestamp
    if (PTPService && PTPService->IsLocked())
    {
        CurrentRTPTimestamp = static_cast<uint32>(PTPService->GetRTPTimestamp(90000));
    }
    else
    {
        CurrentRTPTimestamp = FMath::Rand();
    }

    SetState(ERship2110StreamState::Running);

    UE_LOG(LogRship2110, Log, TEXT("VideoSender %s: Stream started"), *StreamId);
    return true;
}

void URship2110VideoSender::StopStream()
{
    if (State == ERship2110StreamState::Stopped)
    {
        return;
    }

    SetState(ERship2110StreamState::Stopped);

    UE_LOG(LogRship2110, Log, TEXT("VideoSender %s: Stream stopped"), *StreamId);
}

void URship2110VideoSender::PauseStream()
{
    if (State == ERship2110StreamState::Running)
    {
        SetState(ERship2110StreamState::Paused);
        UE_LOG(LogRship2110, Log, TEXT("VideoSender %s: Stream paused"), *StreamId);
    }
}

void URship2110VideoSender::ResumeStream()
{
    if (State == ERship2110StreamState::Paused)
    {
        SetState(ERship2110StreamState::Running);
        UE_LOG(LogRship2110, Log, TEXT("VideoSender %s: Stream resumed"), *StreamId);
    }
}

void URship2110VideoSender::SetCaptureSource(ERship2110CaptureSource Source)
{
    CaptureSource = Source;
}

void URship2110VideoSender::SetRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
    SourceRenderTarget = RenderTarget;
    CaptureSource = ERship2110CaptureSource::RenderTarget;
}

bool URship2110VideoSender::SubmitFrame(const void* FrameData, int64 DataSize, const FRshipPTPTimestamp& PTPTimestamp)
{
    if (State != ERship2110StreamState::Running)
    {
        return false;
    }

    if (!FrameData || DataSize != VideoFormat.GetFrameSizeBytes())
    {
        UE_LOG(LogRship2110, Warning, TEXT("VideoSender: Invalid frame data (expected %lld bytes, got %lld)"),
               VideoFormat.GetFrameSizeBytes(), DataSize);
        return false;
    }

#if RSHIP_RIVERMAX_AVAILABLE
    return SendFrameViaRivermax(FrameData, DataSize, PTPTimestamp);
#else
    // Stub mode - simulate successful send
    Stats.FramesSent++;
    Stats.BytesSent += DataSize;
    return true;
#endif
}

bool URship2110VideoSender::SubmitFrameFromTexture(UTexture2D* SourceTexture, const FRshipPTPTimestamp& PTPTimestamp)
{
    // TODO: Implement GPU texture copy path
    UE_LOG(LogRship2110, Warning, TEXT("VideoSender: SubmitFrameFromTexture not yet implemented"));
    return false;
}

bool URship2110VideoSender::UpdateTransportParams(const FRship2110TransportParams& NewParams)
{
    // Some params can be updated while streaming
    TransportParams.DSCP = NewParams.DSCP;
    TransportParams.TTL = NewParams.TTL;

    // Changing destination requires stream restart
    if (NewParams.DestinationIP != TransportParams.DestinationIP ||
        NewParams.DestinationPort != TransportParams.DestinationPort)
    {
        UE_LOG(LogRship2110, Warning,
               TEXT("VideoSender: Destination change requires stream restart"));
        return false;
    }

    return true;
}

void URship2110VideoSender::ResetStatistics()
{
    Stats = FRship2110StreamStats();
}

double URship2110VideoSender::GetBitrateMbps() const
{
    // Calculate theoretical bitrate
    double FrameSizeBits = VideoFormat.GetFrameSizeBytes() * 8.0;
    double FrameRate = VideoFormat.GetFrameRateDecimal();
    return (FrameSizeBits * FrameRate) / 1000000.0;
}

FString URship2110VideoSender::GenerateSDP() const
{
    // Generate SDP according to RFC 4570 and ST 2110-20
    FString SDP;

    // Session-level
    SDP += TEXT("v=0\r\n");
    SDP += FString::Printf(TEXT("o=- %u 0 IN IP4 %s\r\n"),
                           SSRC, *TransportParams.SourceIP);
    SDP += TEXT("s=Unreal Engine SMPTE 2110 Stream\r\n");
    SDP += FString::Printf(TEXT("c=IN IP4 %s/%d\r\n"),
                           *TransportParams.DestinationIP, TransportParams.TTL);
    SDP += TEXT("t=0 0\r\n");

    // Media-level for video
    SDP += FString::Printf(TEXT("m=video %d RTP/AVP %d\r\n"),
                           TransportParams.DestinationPort, TransportParams.PayloadType);

    // RTP map
    SDP += FString::Printf(TEXT("a=rtpmap:%d raw/90000\r\n"),
                           TransportParams.PayloadType);

    // Format parameters (ST 2110-20)
    SDP += FString::Printf(
        TEXT("a=fmtp:%d sampling=%s; width=%d; height=%d; exactframerate=%d/%d; depth=%d; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017\r\n"),
        TransportParams.PayloadType,
        *VideoFormat.GetSampling(),
        VideoFormat.Width,
        VideoFormat.Height,
        VideoFormat.FrameRateNumerator,
        VideoFormat.FrameRateDenominator,
        VideoFormat.GetBitDepthInt());

    // Source filter (RFC 4570)
    SDP += FString::Printf(TEXT("a=source-filter: incl IN IP4 %s %s\r\n"),
                           *TransportParams.DestinationIP, *TransportParams.SourceIP);

    // PTP reference
    SDP += TEXT("a=ts-refclk:ptp=IEEE1588-2008:00-00-00-00-00-00-00-00:127\r\n");
    SDP += TEXT("a=mediaclk:direct=0\r\n");

    return SDP;
}

FString URship2110VideoSender::GetMediaType() const
{
    return VideoFormat.GetSDPMediaType();
}

void URship2110VideoSender::CaptureFrame()
{
    if (CaptureSource == ERship2110CaptureSource::External)
    {
        // External source - frame is submitted via SubmitFrame()
        return;
    }

    // Get PTP timestamp for this frame
    FRshipPTPTimestamp FrameTimestamp;
    if (PTPService && PTPService->IsLocked())
    {
        FrameTimestamp = PTPService->GetPTPTime();
    }
    else
    {
        // Fall back to system time
        FDateTime Now = FDateTime::UtcNow();
        FrameTimestamp.Seconds = Now.ToUnixTimestamp();
        FrameTimestamp.Nanoseconds = 0;
    }

    // Capture based on source
    switch (CaptureSource)
    {
        case ERship2110CaptureSource::RenderTarget:
            if (SourceRenderTarget)
            {
                // Read render target
                // TODO: Implement async readback
                UE_LOG(LogRship2110, VeryVerbose, TEXT("VideoSender: Capturing from render target"));
            }
            break;

        case ERship2110CaptureSource::Viewport:
            // Capture viewport
            // TODO: Implement viewport capture
            UE_LOG(LogRship2110, VeryVerbose, TEXT("VideoSender: Capturing from viewport"));
            break;

        case ERship2110CaptureSource::SceneCapture:
            // Scene capture handled by component
            break;

        default:
            break;
    }

    LastFrameTime = FrameTimestamp;
}

void URship2110VideoSender::TransmitFrame()
{
    // Get current buffer
    if (CurrentBufferIndex < 0 || CurrentBufferIndex >= FrameBuffers.Num())
    {
        return;
    }

    FFrameBuffer& Buffer = FrameBuffers[CurrentBufferIndex];
    if (!Buffer.Data || !Buffer.bInUse)
    {
        return;
    }

#if RSHIP_RIVERMAX_AVAILABLE
    SendFrameViaRivermax(Buffer.Data, Buffer.Size, Buffer.Timestamp);
#else
    // Stub mode - simulate transmission
    double CurrentTime = FPlatformTime::Seconds();
    double FrameDuration = 1.0 / VideoFormat.GetFrameRateDecimal();

    if (CurrentTime - LastSendTime >= FrameDuration)
    {
        Stats.FramesSent++;
        Stats.BytesSent += Buffer.Size;
        Stats.PacketsSent += CalculatePacketsPerFrame();

        // Update RTP state
        CurrentRTPTimestamp += PTPService ?
            PTPService->GetRTPTimestampIncrement(
                FFrameRate(VideoFormat.FrameRateNumerator, VideoFormat.FrameRateDenominator), 90000) :
            static_cast<int32>(90000.0 / VideoFormat.GetFrameRateDecimal());

        LastSendTime = CurrentTime;
        FrameCounter++;
    }
#endif

    // Move to next buffer
    Buffer.bInUse = false;
    CurrentBufferIndex = (CurrentBufferIndex + 1) % FrameBuffers.Num();
}

bool URship2110VideoSender::AllocateBuffers()
{
    int64 FrameSize = VideoFormat.GetFrameSizeBytes();

    FrameBuffers.SetNum(NumFrameBuffers);
    for (int32 i = 0; i < NumFrameBuffers; i++)
    {
        if (Manager)
        {
            FrameBuffers[i].Data = Manager->AllocateStreamMemory(FrameSize, 4096);
        }
        else
        {
            FrameBuffers[i].Data = FMemory::Malloc(FrameSize, 4096);
        }

        if (!FrameBuffers[i].Data)
        {
            UE_LOG(LogRship2110, Error, TEXT("VideoSender: Failed to allocate buffer %d"), i);
            FreeBuffers();
            return false;
        }

        FrameBuffers[i].Size = FrameSize;
        FrameBuffers[i].bInUse = false;
    }

    // Allocate packet buffer
    PacketBuffer.SetNumZeroed(CalculatePacketPayloadSize() + 64);  // Extra for headers

    // Allocate capture buffer if needed
    CaptureBuffer.SetNumZeroed(VideoFormat.Width * VideoFormat.Height * 4);  // RGBA

    UE_LOG(LogRship2110, Log, TEXT("VideoSender: Allocated %d buffers, %lld bytes each"),
           NumFrameBuffers, FrameSize);

    return true;
}

void URship2110VideoSender::FreeBuffers()
{
    for (FFrameBuffer& Buffer : FrameBuffers)
    {
        if (Buffer.Data)
        {
            if (Manager)
            {
                Manager->FreeStreamMemory(Buffer.Data);
            }
            else
            {
                FMemory::Free(Buffer.Data);
            }
            Buffer.Data = nullptr;
        }
    }
    FrameBuffers.Empty();

    PacketBuffer.Empty();
    CaptureBuffer.Empty();
}

void URship2110VideoSender::SetState(ERship2110StreamState NewState)
{
    if (State != NewState)
    {
        State = NewState;
        OnStateChanged.Broadcast(StreamId, NewState);
    }
}

int32 URship2110VideoSender::CalculatePacketsPerFrame() const
{
    // ST 2110-20 typically uses ~1400 byte payloads
    int64 FrameSize = VideoFormat.GetFrameSizeBytes();
    int32 PayloadSize = CalculatePacketPayloadSize();
    return static_cast<int32>((FrameSize + PayloadSize - 1) / PayloadSize);
}

int32 URship2110VideoSender::CalculatePacketPayloadSize() const
{
    // Max payload size based on MTU (1500) minus headers
    // IP(20) + UDP(8) + RTP(12) + ST2110 payload header(varies)
    return 1400;
}

int32 URship2110VideoSender::CalculatePixelsPerPacket() const
{
    int32 PayloadSize = CalculatePacketPayloadSize();
    int32 BitsPerPixel = 0;

    switch (VideoFormat.ColorFormat)
    {
        case ERship2110ColorFormat::YCbCr_422:
            BitsPerPixel = VideoFormat.GetBitDepthInt() * 2;
            break;
        case ERship2110ColorFormat::YCbCr_444:
        case ERship2110ColorFormat::RGB_444:
            BitsPerPixel = VideoFormat.GetBitDepthInt() * 3;
            break;
        case ERship2110ColorFormat::RGBA_4444:
            BitsPerPixel = VideoFormat.GetBitDepthInt() * 4;
            break;
    }

    return (PayloadSize * 8) / BitsPerPixel;
}

void URship2110VideoSender::UpdateStatistics(int64 BytesSent, bool bLateFrame)
{
    Stats.BytesSent += BytesSent;
    Stats.PacketsSent++;

    if (bLateFrame)
    {
        Stats.LateFrames++;
    }

    // Update bitrate (rolling average)
    // TODO: Implement proper bitrate calculation
}

// Color space conversion helpers
void URship2110VideoSender::ConvertRGBAToYCbCr422(const uint8* RGBAData, uint8* YCbCrData, int32 Width, int32 Height)
{
    // BT.709 coefficients
    const float Kr = 0.2126f;
    const float Kb = 0.0722f;
    const float Kg = 1.0f - Kr - Kb;

    for (int32 y = 0; y < Height; y++)
    {
        for (int32 x = 0; x < Width; x += 2)
        {
            int32 RGBAOffset = (y * Width + x) * 4;
            int32 YCbCrOffset = (y * Width + x) * 2;

            // First pixel
            float R0 = RGBAData[RGBAOffset + 0] / 255.0f;
            float G0 = RGBAData[RGBAOffset + 1] / 255.0f;
            float B0 = RGBAData[RGBAOffset + 2] / 255.0f;

            // Second pixel
            float R1 = RGBAData[RGBAOffset + 4] / 255.0f;
            float G1 = RGBAData[RGBAOffset + 5] / 255.0f;
            float B1 = RGBAData[RGBAOffset + 6] / 255.0f;

            // Calculate Y for both pixels
            float Y0 = Kr * R0 + Kg * G0 + Kb * B0;
            float Y1 = Kr * R1 + Kg * G1 + Kb * B1;

            // Calculate Cb and Cr (averaged for 4:2:2)
            float Cb = 0.5f * ((B0 - Y0) / (1.0f - Kb) + (B1 - Y1) / (1.0f - Kb));
            float Cr = 0.5f * ((R0 - Y0) / (1.0f - Kr) + (R1 - Y1) / (1.0f - Kr));

            // Scale to 10-bit range [64-940] for Y, [64-960] for C
            int32 Y0_10 = FMath::Clamp(static_cast<int32>(Y0 * 876.0f + 64.0f), 64, 940);
            int32 Y1_10 = FMath::Clamp(static_cast<int32>(Y1 * 876.0f + 64.0f), 64, 940);
            int32 Cb_10 = FMath::Clamp(static_cast<int32>(Cb * 448.0f + 512.0f), 64, 960);
            int32 Cr_10 = FMath::Clamp(static_cast<int32>(Cr * 448.0f + 512.0f), 64, 960);

            // Pack into YCbCr 4:2:2 (Cb Y0 Cr Y1 pattern for 10-bit)
            // TODO: Proper 10-bit packing
            YCbCrData[YCbCrOffset + 0] = static_cast<uint8>(Cb_10 >> 2);
            YCbCrData[YCbCrOffset + 1] = static_cast<uint8>(Y0_10 >> 2);
            YCbCrData[YCbCrOffset + 2] = static_cast<uint8>(Cr_10 >> 2);
            YCbCrData[YCbCrOffset + 3] = static_cast<uint8>(Y1_10 >> 2);
        }
    }
}

void URship2110VideoSender::ConvertRGBAToYCbCr444(const uint8* RGBAData, uint8* YCbCrData, int32 Width, int32 Height)
{
    // Similar to 422 but no chroma subsampling
    const float Kr = 0.2126f;
    const float Kb = 0.0722f;
    const float Kg = 1.0f - Kr - Kb;

    for (int32 y = 0; y < Height; y++)
    {
        for (int32 x = 0; x < Width; x++)
        {
            int32 RGBAOffset = (y * Width + x) * 4;
            int32 YCbCrOffset = (y * Width + x) * 3;

            float R = RGBAData[RGBAOffset + 0] / 255.0f;
            float G = RGBAData[RGBAOffset + 1] / 255.0f;
            float B = RGBAData[RGBAOffset + 2] / 255.0f;

            float Y = Kr * R + Kg * G + Kb * B;
            float Cb = (B - Y) / (1.0f - Kb) * 0.5f;
            float Cr = (R - Y) / (1.0f - Kr) * 0.5f;

            // Scale to 10-bit
            int32 Y_10 = FMath::Clamp(static_cast<int32>(Y * 876.0f + 64.0f), 64, 940);
            int32 Cb_10 = FMath::Clamp(static_cast<int32>(Cb * 448.0f + 512.0f), 64, 960);
            int32 Cr_10 = FMath::Clamp(static_cast<int32>(Cr * 448.0f + 512.0f), 64, 960);

            YCbCrData[YCbCrOffset + 0] = static_cast<uint8>(Y_10 >> 2);
            YCbCrData[YCbCrOffset + 1] = static_cast<uint8>(Cb_10 >> 2);
            YCbCrData[YCbCrOffset + 2] = static_cast<uint8>(Cr_10 >> 2);
        }
    }
}

#if RSHIP_RIVERMAX_AVAILABLE

bool URship2110VideoSender::CreateRivermaxStream()
{
    // Create Rivermax output stream
    rmax_out_stream_params_t params;
    rmax_out_stream_params_init(&params);

    // Set stream parameters
    params.type = RMAX_OUT_STREAM_TYPE_GENERIC;

    // Set local interface
    inet_pton(AF_INET, TCHAR_TO_ANSI(*TransportParams.SourceIP), &params.local_addr.sin_addr);
    params.local_addr.sin_port = htons(TransportParams.SourcePort);

    // Set destination
    inet_pton(AF_INET, TCHAR_TO_ANSI(*TransportParams.DestinationIP), &params.dest_addr.sin_addr);
    params.dest_addr.sin_port = htons(TransportParams.DestinationPort);

    // Set QoS
    params.dscp = TransportParams.DSCP;
    params.ttl = TransportParams.TTL;

    rmax_status_t status = rmax_out_create_stream(&params, &RivermaxStream);
    if (status != RMAX_OK)
    {
        UE_LOG(LogRship2110, Error, TEXT("VideoSender: Failed to create Rivermax stream: %d"), status);
        RivermaxStream = nullptr;
        return false;
    }

    UE_LOG(LogRship2110, Log, TEXT("VideoSender: Created Rivermax stream"));
    return true;
}

void URship2110VideoSender::DestroyRivermaxStream()
{
    if (RivermaxStream)
    {
        rmax_out_destroy_stream(RivermaxStream);
        RivermaxStream = nullptr;
        UE_LOG(LogRship2110, Log, TEXT("VideoSender: Destroyed Rivermax stream"));
    }
}

bool URship2110VideoSender::SendFrameViaRivermax(const void* FrameData, int64 DataSize, const FRshipPTPTimestamp& Timestamp)
{
    if (!RivermaxStream)
    {
        return false;
    }

    // Pack frame into RTP packets and send via Rivermax
    // This is where the real ST 2110-20 packetization happens

    const uint8* DataPtr = static_cast<const uint8*>(FrameData);
    int64 RemainingBytes = DataSize;
    int32 PacketPayloadSize = CalculatePacketPayloadSize();
    int32 LineNumber = 0;
    int32 PixelOffset = 0;

    while (RemainingBytes > 0)
    {
        // Build RTP header
        uint8 RTPHeader[12];
        RTPHeader[0] = 0x80;  // Version 2, no padding, no extension, no CSRC
        RTPHeader[1] = (RemainingBytes <= PacketPayloadSize) ? (0x80 | TransportParams.PayloadType) : TransportParams.PayloadType;

        // Sequence number
        RTPHeader[2] = (CurrentSequenceNumber >> 8) & 0xFF;
        RTPHeader[3] = CurrentSequenceNumber & 0xFF;
        CurrentSequenceNumber++;

        // RTP timestamp (from PTP)
        uint32 RTPTimestamp = PTPService ?
            static_cast<uint32>(PTPService->GetRTPTimestampForTime(Timestamp, 90000)) :
            CurrentRTPTimestamp;
        RTPHeader[4] = (RTPTimestamp >> 24) & 0xFF;
        RTPHeader[5] = (RTPTimestamp >> 16) & 0xFF;
        RTPHeader[6] = (RTPTimestamp >> 8) & 0xFF;
        RTPHeader[7] = RTPTimestamp & 0xFF;

        // SSRC
        RTPHeader[8] = (SSRC >> 24) & 0xFF;
        RTPHeader[9] = (SSRC >> 16) & 0xFF;
        RTPHeader[10] = (SSRC >> 8) & 0xFF;
        RTPHeader[11] = SSRC & 0xFF;

        // Build ST 2110-20 payload header (2 bytes per line)
        // Format: Extended sequence number, Line number, Offset, Continuation

        int32 PayloadSize = FMath::Min(static_cast<int64>(PacketPayloadSize), RemainingBytes);

        // Create packet with header + payload
        TArray<uint8> Packet;
        Packet.Append(RTPHeader, 12);
        // Add 2110-20 payload header here...
        Packet.Append(DataPtr, PayloadSize);

        // Send via Rivermax
        rmax_out_send_params_t send_params;
        send_params.data = Packet.GetData();
        send_params.size = Packet.Num();
        send_params.timestamp = Timestamp.ToNanoseconds();

        rmax_status_t status = rmax_out_send(RivermaxStream, &send_params);
        if (status != RMAX_OK)
        {
            UE_LOG(LogRship2110, Warning, TEXT("VideoSender: Send failed: %d"), status);
            return false;
        }

        DataPtr += PayloadSize;
        RemainingBytes -= PayloadSize;
        Stats.PacketsSent++;
    }

    Stats.FramesSent++;
    Stats.BytesSent += DataSize;

    return true;
}

#endif  // RSHIP_RIVERMAX_AVAILABLE
