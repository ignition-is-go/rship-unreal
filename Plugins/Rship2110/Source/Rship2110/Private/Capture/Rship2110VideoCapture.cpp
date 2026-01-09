// Copyright Rocketship. All Rights Reserved.

#include "Capture/Rship2110VideoCapture.h"
#include "Rship2110.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ScreenRendering.h"
#include "CommonRenderResources.h"
#include "Components/SceneCaptureComponent2D.h"

// Color management integration
#include "RshipColorManagementSubsystem.h"
#include "RshipColorConfig.h"

bool URship2110VideoCapture::Initialize(const FRship2110VideoFormat& InVideoFormat)
{
    VideoFormat = InVideoFormat;

    // Allocate buffers
    if (!AllocateBuffers())
    {
        UE_LOG(LogRship2110, Error, TEXT("VideoCapture: Failed to allocate buffers"));
        return false;
    }

    // Initialize color conversion LUTs
    InitializeColorLUTs();

    // Check GPUDirect availability
#if RSHIP_GPUDIRECT_AVAILABLE
    bGPUDirectAvailable = true;
#else
    bGPUDirectAvailable = false;
#endif

    bIsInitialized = true;

    UE_LOG(LogRship2110, Log, TEXT("VideoCapture: Initialized for %dx%d"),
           VideoFormat.Width, VideoFormat.Height);

    return true;
}

void URship2110VideoCapture::Shutdown()
{
    FreeBuffers();

    RtoYLUT.Empty();
    GtoYLUT.Empty();
    BtoYLUT.Empty();
    RtoCbLUT.Empty();
    GtoCbLUT.Empty();
    BtoCbLUT.Empty();
    RtoCrLUT.Empty();
    GtoCrLUT.Empty();
    BtoCrLUT.Empty();

    bIsInitialized = false;

    UE_LOG(LogRship2110, Log, TEXT("VideoCapture: Shutdown complete"));
}

void URship2110VideoCapture::CaptureViewport(const FRshipPTPTimestamp& Timestamp, FOnFrameCaptured Callback)
{
    if (!bIsInitialized)
    {
        return;
    }

    int32 BufferIndex = AcquireBuffer();
    if (BufferIndex < 0)
    {
        UE_LOG(LogRship2110, Warning, TEXT("VideoCapture: No free buffers available"));
        return;
    }

    FCaptureBuffer& Buffer = CaptureBuffers[BufferIndex];
    Buffer.bInUse = true;
    Buffer.Timestamp = Timestamp;
    Buffer.Callback = Callback;
    Buffer.CaptureStartTime = FPlatformTime::Seconds();

    // Queue pending capture
    FPendingCapture Pending;
    Pending.SourceType = FPendingCapture::ESourceType::Viewport;
    Pending.Timestamp = Timestamp;
    Pending.Callback = Callback;
    Pending.BufferIndex = BufferIndex;

    {
        FScopeLock Lock(&CaptureLock);
        PendingCaptures.Add(Pending);
    }
}

void URship2110VideoCapture::CaptureRenderTarget(
    UTextureRenderTarget2D* RenderTarget,
    const FRshipPTPTimestamp& Timestamp,
    FOnFrameCaptured Callback)
{
    if (!bIsInitialized || !RenderTarget)
    {
        return;
    }

    int32 BufferIndex = AcquireBuffer();
    if (BufferIndex < 0)
    {
        UE_LOG(LogRship2110, Warning, TEXT("VideoCapture: No free buffers available"));
        return;
    }

    FCaptureBuffer& Buffer = CaptureBuffers[BufferIndex];
    Buffer.bInUse = true;
    Buffer.Timestamp = Timestamp;
    Buffer.Callback = Callback;
    Buffer.CaptureStartTime = FPlatformTime::Seconds();

    // Queue pending capture
    FPendingCapture Pending;
    Pending.SourceType = FPendingCapture::ESourceType::RenderTarget;
    Pending.RenderTarget = RenderTarget;
    Pending.Timestamp = Timestamp;
    Pending.Callback = Callback;
    Pending.BufferIndex = BufferIndex;

    {
        FScopeLock Lock(&CaptureLock);
        PendingCaptures.Add(Pending);
    }
}

void URship2110VideoCapture::CaptureTexture(
    UTexture2D* SourceTexture,
    const FRshipPTPTimestamp& Timestamp,
    FOnFrameCaptured Callback)
{
    if (!bIsInitialized || !SourceTexture)
    {
        return;
    }

    int32 BufferIndex = AcquireBuffer();
    if (BufferIndex < 0)
    {
        return;
    }

    FCaptureBuffer& Buffer = CaptureBuffers[BufferIndex];
    Buffer.bInUse = true;
    Buffer.Timestamp = Timestamp;
    Buffer.Callback = Callback;
    Buffer.CaptureStartTime = FPlatformTime::Seconds();

    FPendingCapture Pending;
    Pending.SourceType = FPendingCapture::ESourceType::Texture;
    Pending.Texture = SourceTexture;
    Pending.Timestamp = Timestamp;
    Pending.Callback = Callback;
    Pending.BufferIndex = BufferIndex;

    {
        FScopeLock Lock(&CaptureLock);
        PendingCaptures.Add(Pending);
    }
}

void URship2110VideoCapture::ProcessPendingCaptures()
{
    TArray<FPendingCapture> CapturesToProcess;

    {
        FScopeLock Lock(&CaptureLock);
        CapturesToProcess = MoveTemp(PendingCaptures);
        PendingCaptures.Empty();
    }

    for (FPendingCapture& Pending : CapturesToProcess)
    {
        FCaptureBuffer& Buffer = CaptureBuffers[Pending.BufferIndex];

        switch (Pending.SourceType)
        {
            case FPendingCapture::ESourceType::Viewport:
                // Viewport capture requires render thread
                ENQUEUE_RENDER_COMMAND(CaptureViewport)(
                    [this, BufferIndex = Pending.BufferIndex, Timestamp = Pending.Timestamp](FRHICommandListImmediate& RHICmdList)
                    {
                        CaptureViewport_RenderThread(RHICmdList, BufferIndex, Timestamp);
                    });
                break;

            case FPendingCapture::ESourceType::RenderTarget:
                if (Pending.RenderTarget)
                {
                    ENQUEUE_RENDER_COMMAND(CaptureRenderTarget)(
                        [this, RT = Pending.RenderTarget, BufferIndex = Pending.BufferIndex, Timestamp = Pending.Timestamp](FRHICommandListImmediate& RHICmdList)
                        {
                            CaptureRenderTarget_RenderThread(RHICmdList, RT, BufferIndex, Timestamp);
                        });
                }
                break;

            case FPendingCapture::ESourceType::Texture:
                // TODO: Implement texture capture
                ReleaseBuffer(Pending.BufferIndex);
                break;
        }
    }

    // Check for completed async readbacks
    for (int32 i = 0; i < CaptureBuffers.Num(); i++)
    {
        FCaptureBuffer& Buffer = CaptureBuffers[i];
        if (Buffer.bGPUReadbackPending)
        {
            CompleteAsyncReadback(i);
        }
    }
}

void URship2110VideoCapture::SetVideoFormat(const FRship2110VideoFormat& NewFormat)
{
    if (NewFormat.Width != VideoFormat.Width || NewFormat.Height != VideoFormat.Height)
    {
        FreeBuffers();
        VideoFormat = NewFormat;
        AllocateBuffers();
    }
    else
    {
        VideoFormat = NewFormat;
    }
}

void URship2110VideoCapture::SetBufferCount(int32 NumBuffers)
{
    NumBuffers = FMath::Clamp(NumBuffers, 2, 8);
    if (NumBuffers != CaptureBuffers.Num())
    {
        FreeBuffers();
        CaptureBuffers.SetNum(NumBuffers);
        AllocateBuffers();
    }
}

void URship2110VideoCapture::ConfigureSceneCaptureFromColorManagement(USceneCaptureComponent2D* SceneCapture, UWorld* World)
{
    if (!SceneCapture)
    {
        return;
    }

    URshipColorManagementSubsystem* ColorSubsystem = nullptr;
    if (World)
    {
        ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>();
    }

    if (ColorSubsystem)
    {
        // Use color management subsystem to configure the scene capture
        ColorSubsystem->ConfigureSceneCapture(SceneCapture);
        UE_LOG(LogRship2110, Verbose, TEXT("VideoCapture: Using RshipColorManagement subsystem for capture settings"));

        // Sync our colorimetry from the color config
        SyncColorimetryFromColorManagement(World);
    }
    else
    {
        UE_LOG(LogRship2110, Verbose, TEXT("VideoCapture: Color management subsystem not available, using defaults"));
    }
}

void URship2110VideoCapture::SyncColorimetryFromColorManagement(UWorld* World)
{
    if (!World)
    {
        return;
    }

    URshipColorManagementSubsystem* ColorSubsystem = World->GetSubsystem<URshipColorManagementSubsystem>();
    if (!ColorSubsystem)
    {
        return;
    }

    FRshipColorConfig ColorConfig = ColorSubsystem->GetColorConfig();

    // Map color management color space to 2110 colorimetry
    ERship2110Colorimetry NewColorimetry = ERship2110Colorimetry::BT709;  // Default

    switch (ColorConfig.ColorSpace)
    {
        case ERshipColorSpace::sRGB:
        case ERshipColorSpace::Rec709:
            NewColorimetry = ERship2110Colorimetry::BT709;
            break;

        case ERshipColorSpace::Rec2020:
            NewColorimetry = ColorConfig.bEnableHDR ? ERship2110Colorimetry::BT2100 : ERship2110Colorimetry::BT2020;
            break;

        case ERshipColorSpace::DCIP3:
            NewColorimetry = ERship2110Colorimetry::DCIP3;
            break;
    }

    // Update if changed
    if (NewColorimetry != VideoFormat.Colorimetry)
    {
        SetColorimetry(NewColorimetry);
        UE_LOG(LogRship2110, Log, TEXT("VideoCapture: Synced colorimetry to %s from color management"),
               *VideoFormat.GetColorimetryString());
    }
}

void URship2110VideoCapture::SetColorimetry(ERship2110Colorimetry NewColorimetry)
{
    if (NewColorimetry == VideoFormat.Colorimetry)
    {
        return;
    }

    VideoFormat.Colorimetry = NewColorimetry;

    // Reinitialize color LUTs with the new colorimetry coefficients
    InitializeColorLUTs();

    UE_LOG(LogRship2110, Log, TEXT("VideoCapture: Set colorimetry to %s"),
           *VideoFormat.GetColorimetryString());
}

void URship2110VideoCapture::SetGPUDirectEnabled(bool bEnable)
{
    if (bEnable && !bGPUDirectAvailable)
    {
        UE_LOG(LogRship2110, Warning, TEXT("VideoCapture: GPUDirect not available"));
        return;
    }
    bGPUDirectEnabled = bEnable;
}

bool URship2110VideoCapture::GetGPUDirectBuffer(void*& OutBufferPtr, size_t& OutSize) const
{
    if (!bGPUDirectEnabled)
    {
        return false;
    }

    // Find a ready buffer
    for (const FCaptureBuffer& Buffer : CaptureBuffers)
    {
        if (Buffer.bReadyForRead && Buffer.MappedPtr)
        {
            OutBufferPtr = Buffer.MappedPtr;
            OutSize = Buffer.Data.Num();
            return true;
        }
    }

    return false;
}

double URship2110VideoCapture::GetAverageCaptureLatencyMs() const
{
    if (CaptureLatencies.Num() == 0)
    {
        return 0.0;
    }

    double Sum = 0.0;
    for (double Latency : CaptureLatencies)
    {
        Sum += Latency;
    }
    return (Sum / CaptureLatencies.Num()) * 1000.0;
}

bool URship2110VideoCapture::AllocateBuffers()
{
    int32 NumBuffers = FMath::Max(CaptureBuffers.Num(), DefaultBufferCount);
    CaptureBuffers.SetNum(NumBuffers);

    int32 FrameSize = VideoFormat.Width * VideoFormat.Height * 4;  // RGBA

    for (int32 i = 0; i < NumBuffers; i++)
    {
        FCaptureBuffer& Buffer = CaptureBuffers[i];
        Buffer.Data.SetNumZeroed(FrameSize);
        Buffer.bInUse = false;
        Buffer.bReadyForRead = false;
        Buffer.bGPUReadbackPending = false;
    }

    UE_LOG(LogRship2110, Log, TEXT("VideoCapture: Allocated %d buffers, %d bytes each"),
           NumBuffers, FrameSize);

    return true;
}

void URship2110VideoCapture::FreeBuffers()
{
    for (FCaptureBuffer& Buffer : CaptureBuffers)
    {
        Buffer.Data.Empty();
        Buffer.bInUse = false;
        Buffer.bReadyForRead = false;
        if (Buffer.StagingTexture.IsValid())
        {
            Buffer.StagingTexture.SafeRelease();
        }
    }
}

int32 URship2110VideoCapture::AcquireBuffer()
{
    for (int32 i = 0; i < CaptureBuffers.Num(); i++)
    {
        int32 Index = (CurrentCaptureIndex + i) % CaptureBuffers.Num();
        if (!CaptureBuffers[Index].bInUse)
        {
            CurrentCaptureIndex = (Index + 1) % CaptureBuffers.Num();
            return Index;
        }
    }
    return -1;
}

void URship2110VideoCapture::ReleaseBuffer(int32 Index)
{
    if (Index >= 0 && Index < CaptureBuffers.Num())
    {
        CaptureBuffers[Index].bInUse = false;
        CaptureBuffers[Index].bReadyForRead = false;
        CaptureBuffers[Index].bGPUReadbackPending = false;
    }
}

void URship2110VideoCapture::InitializeColorLUTs()
{
    // Select YCbCr coefficients based on colorimetry
    // Kr and Kb are the luma coefficients that define the color space
    float Kr, Kb;

    switch (VideoFormat.Colorimetry)
    {
        case ERship2110Colorimetry::BT2020:
        case ERship2110Colorimetry::BT2100:
            // BT.2020/2100 coefficients (Rec. 2020 wide color gamut)
            Kr = 0.2627f;
            Kb = 0.0593f;
            UE_LOG(LogRship2110, Verbose, TEXT("VideoCapture: Using BT.2020/2100 YCbCr coefficients"));
            break;

        case ERship2110Colorimetry::DCIP3:
            // DCI-P3 uses same coefficients as BT.709 for YCbCr
            // (DCI-P3 is primarily about different primaries, not luma coefficients)
            Kr = 0.2126f;
            Kb = 0.0722f;
            UE_LOG(LogRship2110, Verbose, TEXT("VideoCapture: Using DCI-P3/BT.709 YCbCr coefficients"));
            break;

        case ERship2110Colorimetry::BT709:
        default:
            // BT.709 coefficients (standard HD)
            Kr = 0.2126f;
            Kb = 0.0722f;
            UE_LOG(LogRship2110, Verbose, TEXT("VideoCapture: Using BT.709 YCbCr coefficients"));
            break;
    }

    const float Kg = 1.0f - Kr - Kb;

    RtoYLUT.SetNum(256);
    GtoYLUT.SetNum(256);
    BtoYLUT.SetNum(256);
    RtoCbLUT.SetNum(256);
    GtoCbLUT.SetNum(256);
    BtoCbLUT.SetNum(256);
    RtoCrLUT.SetNum(256);
    GtoCrLUT.SetNum(256);
    BtoCrLUT.SetNum(256);

    for (int32 i = 0; i < 256; i++)
    {
        float Val = i / 255.0f;

        // Y coefficients (scaled to 10-bit range 64-940)
        RtoYLUT[i] = FMath::RoundToInt(Kr * Val * 876.0f * 65536.0f);
        GtoYLUT[i] = FMath::RoundToInt(Kg * Val * 876.0f * 65536.0f);
        BtoYLUT[i] = FMath::RoundToInt(Kb * Val * 876.0f * 65536.0f);

        // Cb coefficients (scaled to 10-bit range 64-960)
        RtoCbLUT[i] = FMath::RoundToInt(-0.5f * Kr / (1.0f - Kb) * Val * 448.0f * 65536.0f);
        GtoCbLUT[i] = FMath::RoundToInt(-0.5f * Kg / (1.0f - Kb) * Val * 448.0f * 65536.0f);
        BtoCbLUT[i] = FMath::RoundToInt(0.5f * Val * 448.0f * 65536.0f);

        // Cr coefficients
        RtoCrLUT[i] = FMath::RoundToInt(0.5f * Val * 448.0f * 65536.0f);
        GtoCrLUT[i] = FMath::RoundToInt(-0.5f * Kg / (1.0f - Kr) * Val * 448.0f * 65536.0f);
        BtoCrLUT[i] = FMath::RoundToInt(-0.5f * Kb / (1.0f - Kr) * Val * 448.0f * 65536.0f);
    }
}

void URship2110VideoCapture::ConvertRGBAToYCbCr422_CPU(const uint8* RGBA, uint8* YCbCr, int32 Width, int32 Height)
{
    // Fast LUT-based conversion
    for (int32 y = 0; y < Height; y++)
    {
        for (int32 x = 0; x < Width; x += 2)
        {
            int32 RGBAOffset = (y * Width + x) * 4;
            int32 YCbCrOffset = (y * Width + x) * 2;

            // First pixel
            uint8 R0 = RGBA[RGBAOffset + 0];
            uint8 G0 = RGBA[RGBAOffset + 1];
            uint8 B0 = RGBA[RGBAOffset + 2];

            // Second pixel
            uint8 R1 = RGBA[RGBAOffset + 4];
            uint8 G1 = RGBA[RGBAOffset + 5];
            uint8 B1 = RGBA[RGBAOffset + 6];

            // Calculate Y for both pixels using LUTs
            int32 Y0 = (RtoYLUT[R0] + GtoYLUT[G0] + BtoYLUT[B0] + 32768) >> 16;
            int32 Y1 = (RtoYLUT[R1] + GtoYLUT[G1] + BtoYLUT[B1] + 32768) >> 16;

            // Calculate Cb and Cr (averaged)
            int32 Cb = ((RtoCbLUT[R0] + GtoCbLUT[G0] + BtoCbLUT[B0] +
                         RtoCbLUT[R1] + GtoCbLUT[G1] + BtoCbLUT[B1]) / 2 + 32768) >> 16;
            int32 Cr = ((RtoCrLUT[R0] + GtoCrLUT[G0] + BtoCrLUT[B0] +
                         RtoCrLUT[R1] + GtoCrLUT[G1] + BtoCrLUT[B1]) / 2 + 32768) >> 16;

            // Add offsets and clamp to 10-bit (output 8-bit MSBs for now)
            Y0 = FMath::Clamp(Y0 + 64, 64, 940) >> 2;
            Y1 = FMath::Clamp(Y1 + 64, 64, 940) >> 2;
            Cb = FMath::Clamp(Cb + 512, 64, 960) >> 2;
            Cr = FMath::Clamp(Cr + 512, 64, 960) >> 2;

            // Output: Cb Y0 Cr Y1
            YCbCr[YCbCrOffset + 0] = static_cast<uint8>(Cb);
            YCbCr[YCbCrOffset + 1] = static_cast<uint8>(Y0);
            YCbCr[YCbCrOffset + 2] = static_cast<uint8>(Cr);
            YCbCr[YCbCrOffset + 3] = static_cast<uint8>(Y1);
        }
    }
}

void URship2110VideoCapture::ConvertRGBAToYCbCr444_CPU(const uint8* RGBA, uint8* YCbCr, int32 Width, int32 Height)
{
    for (int32 y = 0; y < Height; y++)
    {
        for (int32 x = 0; x < Width; x++)
        {
            int32 RGBAOffset = (y * Width + x) * 4;
            int32 YCbCrOffset = (y * Width + x) * 3;

            uint8 R = RGBA[RGBAOffset + 0];
            uint8 G = RGBA[RGBAOffset + 1];
            uint8 B = RGBA[RGBAOffset + 2];

            int32 Y = (RtoYLUT[R] + GtoYLUT[G] + BtoYLUT[B] + 32768) >> 16;
            int32 Cb = (RtoCbLUT[R] + GtoCbLUT[G] + BtoCbLUT[B] + 32768) >> 16;
            int32 Cr = (RtoCrLUT[R] + GtoCrLUT[G] + BtoCrLUT[B] + 32768) >> 16;

            Y = FMath::Clamp(Y + 64, 64, 940) >> 2;
            Cb = FMath::Clamp(Cb + 512, 64, 960) >> 2;
            Cr = FMath::Clamp(Cr + 512, 64, 960) >> 2;

            YCbCr[YCbCrOffset + 0] = static_cast<uint8>(Y);
            YCbCr[YCbCrOffset + 1] = static_cast<uint8>(Cb);
            YCbCr[YCbCrOffset + 2] = static_cast<uint8>(Cr);
        }
    }
}

void URship2110VideoCapture::CaptureViewport_RenderThread(FRHICommandListImmediate& RHICmdList, int32 BufferIndex, const FRshipPTPTimestamp& Timestamp)
{
    check(IsInRenderingThread());

    // TODO: Implement actual viewport capture using RHI commands
    // This would typically involve:
    // 1. Getting the backbuffer or scene color texture
    // 2. Copying to a staging texture
    // 3. Reading back to CPU

    FCaptureBuffer& Buffer = CaptureBuffers[BufferIndex];
    Buffer.bGPUReadbackPending = true;
}

void URship2110VideoCapture::CaptureRenderTarget_RenderThread(FRHICommandListImmediate& RHICmdList, UTextureRenderTarget2D* RT, int32 BufferIndex, const FRshipPTPTimestamp& Timestamp)
{
    check(IsInRenderingThread());

    if (!RT || !RT->GetResource())
    {
        ReleaseBuffer(BufferIndex);
        return;
    }

    FCaptureBuffer& Buffer = CaptureBuffers[BufferIndex];

    // Get the render target's RHI texture
    FTextureRenderTargetResource* RTResource = RT->GetRenderTargetResource();
    if (!RTResource)
    {
        ReleaseBuffer(BufferIndex);
        return;
    }

    FRHITexture* SourceTexture = RTResource->GetRenderTargetTexture();
    if (!SourceTexture)
    {
        ReleaseBuffer(BufferIndex);
        return;
    }

    // Create staging texture if needed
    if (!Buffer.StagingTexture.IsValid())
    {
        FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2D(TEXT("CaptureStaging"))
            .SetExtent(VideoFormat.Width, VideoFormat.Height)
            .SetFormat(PF_B8G8R8A8)
            .SetFlags(ETextureCreateFlags::CPUReadback);

        Buffer.StagingTexture = RHICmdList.CreateTexture(Desc);
    }

    // Copy from render target to staging
    FRHICopyTextureInfo CopyInfo;
    CopyInfo.Size = FIntVector(VideoFormat.Width, VideoFormat.Height, 1);
    RHICmdList.CopyTexture(SourceTexture, Buffer.StagingTexture, CopyInfo);

    Buffer.bGPUReadbackPending = true;
}

void URship2110VideoCapture::CompleteAsyncReadback(int32 BufferIndex)
{
    FCaptureBuffer& Buffer = CaptureBuffers[BufferIndex];

    if (!Buffer.bGPUReadbackPending)
    {
        return;
    }

    // Map staging texture and copy data
    if (Buffer.StagingTexture.IsValid())
    {
        // TODO: Implement proper async readback using FRHIGPUTextureReadback
        // For now, this is a placeholder

        int32 Width = VideoFormat.Width;
        int32 Height = VideoFormat.Height;

        // Calculate latency
        double Latency = FPlatformTime::Seconds() - Buffer.CaptureStartTime;
        CaptureLatencies.Add(Latency);
        if (CaptureLatencies.Num() > MaxLatencySamples)
        {
            CaptureLatencies.RemoveAt(0);
        }

        // Perform color conversion if needed
        if (bDoColorConversion && Buffer.Data.Num() > 0)
        {
            // Convert RGBA to YCbCr based on format
            switch (VideoFormat.ColorFormat)
            {
                case ERship2110ColorFormat::YCbCr_422:
                    // Allocate YCbCr buffer
                    {
                        TArray<uint8> YCbCrBuffer;
                        YCbCrBuffer.SetNumZeroed(Width * Height * 2);
                        ConvertRGBAToYCbCr422_CPU(Buffer.Data.GetData(), YCbCrBuffer.GetData(), Width, Height);

                        // Invoke callback with converted data
                        if (Buffer.Callback.IsBound())
                        {
                            Buffer.Callback.Execute(YCbCrBuffer.GetData(), YCbCrBuffer.Num(), Buffer.Timestamp);
                        }
                    }
                    break;

                case ERship2110ColorFormat::YCbCr_444:
                case ERship2110ColorFormat::RGB_444:
                    {
                        TArray<uint8> YCbCrBuffer;
                        YCbCrBuffer.SetNumZeroed(Width * Height * 3);
                        ConvertRGBAToYCbCr444_CPU(Buffer.Data.GetData(), YCbCrBuffer.GetData(), Width, Height);

                        if (Buffer.Callback.IsBound())
                        {
                            Buffer.Callback.Execute(YCbCrBuffer.GetData(), YCbCrBuffer.Num(), Buffer.Timestamp);
                        }
                    }
                    break;

                default:
                    // Pass through RGBA
                    if (Buffer.Callback.IsBound())
                    {
                        Buffer.Callback.Execute(Buffer.Data.GetData(), Buffer.Data.Num(), Buffer.Timestamp);
                    }
                    break;
            }
        }
        else if (Buffer.Callback.IsBound())
        {
            // Pass through raw data
            Buffer.Callback.Execute(Buffer.Data.GetData(), Buffer.Data.Num(), Buffer.Timestamp);
        }

        TotalFramesCaptured++;
    }

    Buffer.bGPUReadbackPending = false;
    Buffer.bReadyForRead = true;
    Buffer.bInUse = false;
}
