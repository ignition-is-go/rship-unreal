// Copyright Rocketship. All Rights Reserved.
// Video Capture Integration with UE Rendering Pipeline
//
// Provides frame capture from various UE rendering sources:
// - Viewport capture (main game view)
// - Render target capture
// - Scene capture component integration
//
// Handles:
// - GPU readback with minimal latency
// - Format conversion (RGBA to YCbCr)
// - Double/triple buffering for pipelining
// - GPUDirect RDMA integration points

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Rship2110Types.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Rship2110VideoCapture.generated.h"

// UE 5.7+ renamed FTexture2DRHIRef to FTextureRHIRef
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
using FRship2110TextureRHIRef = FTextureRHIRef;
#else
using FRship2110TextureRHIRef = FTexture2DRHIRef;
#endif

class URship2110VideoSender;
class URshipColorManagementSubsystem;
class USceneCaptureComponent2D;
struct FRshipColorConfig;

/**
 * Capture completion delegate
 */
DECLARE_DELEGATE_ThreeParams(FOnFrameCaptured, const void* /*FrameData*/, int64 /*DataSize*/, const FRshipPTPTimestamp& /*Timestamp*/);

/**
 * Video capture handler for UE rendering pipeline integration.
 *
 * Captures frames from UE rendering and prepares them for 2110 streaming.
 * Supports various capture sources and handles GPU readback efficiently.
 */
UCLASS(BlueprintType)
class RSHIP2110_API URship2110VideoCapture : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initialize capture system.
     * @param InVideoFormat Video format for capture
     * @return true if initialization succeeded
     */
    bool Initialize(const FRship2110VideoFormat& InVideoFormat);

    /**
     * Shutdown and release resources.
     */
    void Shutdown();

    /**
     * Request a frame capture from the viewport.
     * Capture happens asynchronously; callback is invoked when complete.
     * @param Timestamp PTP timestamp to associate with frame
     * @param Callback Callback for captured frame data
     */
    void CaptureViewport(const FRshipPTPTimestamp& Timestamp, FOnFrameCaptured Callback);

    /**
     * Request a frame capture from a render target.
     * @param RenderTarget Source render target
     * @param Timestamp PTP timestamp
     * @param Callback Callback for captured frame data
     */
    void CaptureRenderTarget(
        UTextureRenderTarget2D* RenderTarget,
        const FRshipPTPTimestamp& Timestamp,
        FOnFrameCaptured Callback);

    /**
     * Request a frame capture from a texture.
     * @param SourceTexture Source texture
     * @param Timestamp PTP timestamp
     * @param Callback Callback for captured frame data
     */
    void CaptureTexture(
        UTexture2D* SourceTexture,
        const FRshipPTPTimestamp& Timestamp,
        FOnFrameCaptured Callback);

    /**
     * Process pending captures (call from game thread).
     */
    void ProcessPendingCaptures();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * Set video format.
     * @param NewFormat New video format
     */
    void SetVideoFormat(const FRship2110VideoFormat& NewFormat);

    /**
     * Get current video format.
     * @return Video format
     */
    FRship2110VideoFormat GetVideoFormat() const { return VideoFormat; }

    /**
     * Enable/disable color space conversion.
     * When enabled, converts RGBA to YCbCr as specified by video format.
     * @param bEnable true to enable conversion
     */
    void SetColorConversionEnabled(bool bEnable) { bDoColorConversion = bEnable; }

    /**
     * Check if color conversion is enabled.
     * @return true if enabled
     */
    bool IsColorConversionEnabled() const { return bDoColorConversion; }

    /**
     * Set number of capture buffers (for pipelining).
     * @param NumBuffers Number of buffers (2-8)
     */
    void SetBufferCount(int32 NumBuffers);

    /**
     * Get number of capture buffers.
     * @return Buffer count
     */
    int32 GetBufferCount() const { return CaptureBuffers.Num(); }

    // ========================================================================
    // COLOR MANAGEMENT INTEGRATION
    // ========================================================================

    /**
     * Configure a scene capture component using color management settings.
     * Uses RshipColorManagementSubsystem for consistent color pipeline.
     * @param SceneCapture Scene capture component to configure
     * @param World World context for getting subsystem
     */
    void ConfigureSceneCaptureFromColorManagement(USceneCaptureComponent2D* SceneCapture, UWorld* World);

    /**
     * Update video format colorimetry from color management config.
     * Call this to sync the 2110 video format with the global color config.
     * @param World World context for getting subsystem
     */
    void SyncColorimetryFromColorManagement(UWorld* World);

    /**
     * Set colorimetry and reinitialize color conversion LUTs.
     * @param NewColorimetry New colorimetry to use
     */
    void SetColorimetry(ERship2110Colorimetry NewColorimetry);

    /**
     * Get current colorimetry setting.
     * @return Current colorimetry
     */
    ERship2110Colorimetry GetColorimetry() const { return VideoFormat.Colorimetry; }

    // ========================================================================
    // GPUDIRECT INTEGRATION
    // ========================================================================

    /**
     * Check if GPUDirect capture path is available.
     * @return true if GPUDirect can be used
     */
    bool IsGPUDirectAvailable() const { return bGPUDirectAvailable; }

    /**
     * Enable/disable GPUDirect capture path.
     * @param bEnable true to enable
     */
    void SetGPUDirectEnabled(bool bEnable);

    /**
     * Check if GPUDirect is enabled.
     * @return true if enabled
     */
    bool IsGPUDirectEnabled() const { return bGPUDirectEnabled; }

    /**
     * Get GPU buffer pointer for GPUDirect zero-copy.
     * Only valid when GPUDirect is enabled and a capture is ready.
     * @param OutBufferPtr GPU buffer pointer
     * @param OutSize Buffer size
     * @return true if valid buffer available
     */
    bool GetGPUDirectBuffer(void*& OutBufferPtr, size_t& OutSize) const;

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * Get average capture latency in milliseconds.
     * @return Capture latency
     */
    double GetAverageCaptureLatencyMs() const;

    /**
     * Get number of pending capture requests.
     * @return Pending count
     */
    int32 GetPendingCaptureCount() const { return PendingCaptures.Num(); }

    /**
     * Get total frames captured.
     * @return Frame count
     */
    int64 GetTotalFramesCaptured() const { return TotalFramesCaptured; }

private:
    // Configuration
    FRship2110VideoFormat VideoFormat;
    bool bDoColorConversion = true;
    bool bGPUDirectAvailable = false;
    bool bGPUDirectEnabled = false;
    bool bIsInitialized = false;

    // Buffer pool
    struct FCaptureBuffer
    {
        TArray<uint8> Data;
        FRshipPTPTimestamp Timestamp;
        bool bInUse = false;
        bool bReadyForRead = false;
        double CaptureStartTime = 0.0;
        FOnFrameCaptured Callback;

        // GPU resources for async readback
        FRship2110TextureRHIRef StagingTexture;
        void* MappedPtr = nullptr;
        bool bGPUReadbackPending = false;
    };
    TArray<FCaptureBuffer> CaptureBuffers;
    int32 CurrentCaptureIndex = 0;
    static constexpr int32 DefaultBufferCount = 3;

    // Pending capture requests
    struct FPendingCapture
    {
        enum class ESourceType { Viewport, RenderTarget, Texture };
        ESourceType SourceType;
        UTextureRenderTarget2D* RenderTarget = nullptr;
        UTexture2D* Texture = nullptr;
        FRshipPTPTimestamp Timestamp;
        FOnFrameCaptured Callback;
        int32 BufferIndex = -1;
    };
    TArray<FPendingCapture> PendingCaptures;
    FCriticalSection CaptureLock;

    // Statistics
    int64 TotalFramesCaptured = 0;
    TArray<double> CaptureLatencies;
    static constexpr int32 MaxLatencySamples = 100;

    // Color conversion LUTs (for fast CPU conversion)
    TArray<int32> RtoYLUT;
    TArray<int32> GtoYLUT;
    TArray<int32> BtoYLUT;
    TArray<int32> RtoCbLUT;
    TArray<int32> GtoCbLUT;
    TArray<int32> BtoCbLUT;
    TArray<int32> RtoCrLUT;
    TArray<int32> GtoCrLUT;
    TArray<int32> BtoCrLUT;

    // Internal methods
    bool AllocateBuffers();
    void FreeBuffers();
    int32 AcquireBuffer();
    void ReleaseBuffer(int32 Index);

    void InitializeColorLUTs();
    void ConvertRGBAToYCbCr422_CPU(const uint8* RGBA, uint8* YCbCr, int32 Width, int32 Height);
    void ConvertRGBAToYCbCr444_CPU(const uint8* RGBA, uint8* YCbCr, int32 Width, int32 Height);

    // GPU capture methods
    void CaptureViewport_RenderThread(FRHICommandListImmediate& RHICmdList, int32 BufferIndex, const FRshipPTPTimestamp& Timestamp);
    void CaptureRenderTarget_RenderThread(FRHICommandListImmediate& RHICmdList, UTextureRenderTarget2D* RT, int32 BufferIndex, const FRshipPTPTimestamp& Timestamp);
    void CompleteAsyncReadback(int32 BufferIndex);

    // Render thread delegates
    FDelegateHandle ViewportCaptureHandle;
    void OnViewportRendered(FRHICommandListImmediate& RHICmdList);
};
