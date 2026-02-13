// Copyright Rocketship. All Rights Reserved.
// SMPTE ST 2110-20 Video Sender
//
// Streams uncompressed video over SMPTE 2110-20 using Rivermax SDK.
// Integrates with UE's rendering pipeline to capture frames and
// transmit them with PTP-aligned timing.
//
// Key features:
// - Captures from UE render targets or viewport
// - Packs frames into 2110-20 RTP packets
// - Aligns transmission to PTP frame boundaries
// - Supports GPUDirect RDMA for zero-copy GPU-to-NIC transfer
// - Maintains inter-packet gap (IPG) for standard compliance

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Rship2110Types.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Rship2110VideoSender.generated.h"

class URivermaxManager;
class URshipPTPService;
class URship2110Subsystem;
class FSocket;
class FInternetAddr;

/**
 * Capture source for video sender
 */
UENUM(BlueprintType)
enum class ERship2110CaptureSource : uint8
{
    /** Capture from a specified render target */
    RenderTarget    UMETA(DisplayName = "Render Target"),

    /** Capture from the main viewport */
    Viewport        UMETA(DisplayName = "Viewport"),

    /** Capture from Scene Capture 2D component */
    SceneCapture    UMETA(DisplayName = "Scene Capture"),

    /** Use externally provided frame data */
    External        UMETA(DisplayName = "External Data")
};

/**
 * SMPTE ST 2110-20 Video Sender.
 *
 * Handles capture of UE rendered frames and transmission as
 * SMPTE 2110-20 compliant RTP streams via Rivermax.
 */
UCLASS(BlueprintType)
class RSHIP2110_API URship2110VideoSender : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initialize the video sender.
     * @param InManager Rivermax manager reference
     * @param InPTPService PTP service reference
     * @param InVideoFormat Video format specification
     * @param InTransportParams Transport parameters
     * @return true if initialization succeeded
     */
    bool Initialize(
        URivermaxManager* InManager,
        URshipPTPService* InPTPService,
        const FRship2110VideoFormat& InVideoFormat,
        const FRship2110TransportParams& InTransportParams);

    /**
     * Shutdown and release resources.
     */
    void Shutdown();

    /**
     * Called each frame to process capture and transmission.
     */
    void Tick();

    // ========================================================================
    // STREAM CONTROL
    // ========================================================================

    /**
     * Start streaming.
     * @return true if started successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool StartStream();

    /**
     * Stop streaming.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    void StopStream();

    /**
     * Pause streaming (holds last frame).
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    void PauseStream();

    /**
     * Resume from pause.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    void ResumeStream();

    /**
     * Get current stream state.
     * @return Stream state
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    ERship2110StreamState GetState() const { return State; }

    /**
     * Check if stream is running.
     * @return true if streaming
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool IsStreaming() const { return State == ERship2110StreamState::Running; }

    // ========================================================================
    // CAPTURE CONFIGURATION
    // ========================================================================

    /**
     * Set capture source.
     * @param Source Capture source type
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    void SetCaptureSource(ERship2110CaptureSource Source);

    /**
     * Get capture source.
     * @return Current capture source
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    ERship2110CaptureSource GetCaptureSource() const { return CaptureSource; }

    /**
     * Set render target for capture (when using RenderTarget source).
     * @param RenderTarget Render target to capture from
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    void SetRenderTarget(UTextureRenderTarget2D* RenderTarget);

    /**
     * Set an optional capture region in render target pixel coordinates.
     * @param CaptureRect Capture rectangle (min inclusive, max exclusive)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    void SetCaptureRect(const FIntRect& CaptureRect);

    /**
     * Clear any capture rectangle and capture the full render target.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    void ClearCaptureRect();

    /**
     * Get the currently configured capture rectangle.
     * @return Capture rectangle (min inclusive, max exclusive)
     */
    UFUNCTION(BlueprintPure, Category = "Rship|2110")
    FIntRect GetCaptureRect() const;

    /**
     * Get currently set render target.
     * @return Render target or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    UTextureRenderTarget2D* GetRenderTarget() const { return SourceRenderTarget; }

    // ========================================================================
    // FRAME SUBMISSION (External source)
    // ========================================================================

    /**
     * Submit a frame for streaming (when using External source).
     * Frame data must match the configured video format.
     * @param FrameData Pointer to frame data
     * @param DataSize Size of frame data in bytes
     * @param PTPTimestamp PTP timestamp for this frame
     * @return true if frame was accepted
     */
    bool SubmitFrame(const void* FrameData, int64 DataSize, const FRshipPTPTimestamp& PTPTimestamp);

    /**
     * Submit a frame from a texture (GPU copy path).
     * @param SourceTexture Source texture to copy
     * @param PTPTimestamp PTP timestamp for this frame
     * @return true if frame was accepted
     */
    bool SubmitFrameFromTexture(UTexture2D* SourceTexture, const FRshipPTPTimestamp& PTPTimestamp);

    // ========================================================================
    // FORMAT & TRANSPORT
    // ========================================================================

    /**
     * Get video format.
     * @return Video format specification
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    FRship2110VideoFormat GetVideoFormat() const { return VideoFormat; }

    /**
     * Get transport parameters.
     * @return Transport parameters
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    FRship2110TransportParams GetTransportParams() const { return TransportParams; }

    /**
     * Update transport parameters (can be done while streaming).
     * @param NewParams New transport parameters
     * @return true if update succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    bool UpdateTransportParams(const FRship2110TransportParams& NewParams);

    /**
     * Get stream ID.
     * @return Unique stream identifier
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    FString GetStreamId() const { return StreamId; }

    /**
     * Set stream ID (internal use).
     */
    void SetStreamId(const FString& InStreamId) { StreamId = InStreamId; }

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * Get stream statistics.
     * @return Statistics structure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    FRship2110StreamStats GetStatistics() const { return Stats; }

    /**
     * Reset statistics.
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    void ResetStatistics();

    /**
     * Get calculated bitrate in Mbps.
     * @return Current bitrate
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    double GetBitrateMbps() const;

    // ========================================================================
    // SDP GENERATION
    // ========================================================================

    /**
     * Generate SDP (Session Description Protocol) for this stream.
     * @return SDP string
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    FString GenerateSDP() const;

    /**
     * Get media type string for SDP.
     * @return Media type (e.g., "video/raw")
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    FString GetMediaType() const;

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when stream state changes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|2110")
    FOn2110StreamStateChanged OnStateChanged;

private:
    // References
    UPROPERTY()
    URivermaxManager* Manager = nullptr;

    UPROPERTY()
    URshipPTPService* PTPService = nullptr;

    UPROPERTY()
    UTextureRenderTarget2D* SourceRenderTarget = nullptr;

    // Configuration
    FRship2110VideoFormat VideoFormat;
    FRship2110TransportParams TransportParams;
    FString StreamId;
    ERship2110CaptureSource CaptureSource = ERship2110CaptureSource::Viewport;

    // State
    ERship2110StreamState State = ERship2110StreamState::Stopped;
    FRship2110StreamStats Stats;

    // RTP state
    uint32 CurrentRTPTimestamp = 0;
    uint16 CurrentSequenceNumber = 0;
    uint32 SSRC = 0;

    // Timing
    FRshipPTPTimestamp LastFrameTime;
    double LastSendTime = 0.0;
    int64 FrameCounter = 0;
    FIntRect CaptureRect = FIntRect(0, 0, 0, 0);
    bool bUseCaptureRect = false;

    // Buffers (managed externally or via Rivermax)
    TArray<uint8> CaptureBuffer;
    TArray<uint8> PacketBuffer;

    // UDP socket for fallback transmission
    FSocket* UDPSocket = nullptr;
    TSharedPtr<FInternetAddr> DestinationAddr;

    // Frame buffer pool for pipelining
    struct FFrameBuffer
    {
        void* Data = nullptr;
        size_t Size = 0;
        bool bInUse = false;
        FRshipPTPTimestamp Timestamp;
    };
    TArray<FFrameBuffer> FrameBuffers;
    int32 CurrentBufferIndex = 0;
    static constexpr int32 NumFrameBuffers = 4;

#if RSHIP_RIVERMAX_AVAILABLE
    // Rivermax stream handle
    void* RivermaxStream = nullptr;

    // Rivermax-specific methods
    bool CreateRivermaxStream();
    void DestroyRivermaxStream();
    bool SendFrameViaRivermax(const void* FrameData, int64 DataSize, const FRshipPTPTimestamp& Timestamp);
#endif

    // Internal methods
    void CaptureFrame();
    void TransmitFrame();
    bool AllocateBuffers();
    void FreeBuffers();
    bool ReadRenderTargetPixels(UTextureRenderTarget2D* RenderTarget, TArray<FColor>& OutPixels) const;
    FIntRect ResolveCaptureRect(int32 SourceWidth, int32 SourceHeight) const;
    int32 FindFreeFrameBufferIndex() const;
    int64 GetExpectedCaptureBytes(int32 Width, int32 Height) const;
    void PackRTPPackets(const void* FrameData, int64 DataSize, const FRshipPTPTimestamp& Timestamp);
    void SendPacket(const void* PacketData, int32 PacketSize);
    void UpdateStatistics(int64 BytesSent, bool bLateFrame);
    void SetState(ERship2110StreamState NewState);

    // Color format conversion
    void ConvertRGBAToYCbCr422(const uint8* RGBAData, uint8* YCbCrData, int32 Width, int32 Height);
    void ConvertRGBAToYCbCr444(const uint8* RGBAData, uint8* YCbCrData, int32 Width, int32 Height);

    // 2110-20 specific
    int32 CalculatePacketsPerFrame() const;
    int32 CalculatePacketPayloadSize() const;
    int32 CalculatePixelsPerPacket() const;
};
