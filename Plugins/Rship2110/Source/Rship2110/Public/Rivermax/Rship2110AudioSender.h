// Copyright Rocketship. All Rights Reserved.
// SMPTE ST 2110-30 Audio Sender (Stub)
//
// Placeholder for future audio streaming implementation.
// Will support:
// - PCM audio (2110-30)
// - AES3 audio (2110-31)
// - PTP-aligned audio timing
// - Multi-channel audio (up to 64 channels)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Rship2110Types.h"
#include "Rship2110AudioSender.generated.h"

/**
 * Audio format for 2110-30 streams
 */
USTRUCT(BlueprintType)
struct RSHIP2110_API FRship2110AudioFormat
{
    GENERATED_BODY()

    /** Sample rate (typically 48000 or 96000) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    int32 SampleRate = 48000;

    /** Bits per sample (16, 24, or 32) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    int32 BitsPerSample = 24;

    /** Number of channels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    int32 NumChannels = 2;

    /** Packet time in microseconds (125, 250, 333, 1000, 4000) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|2110")
    int32 PacketTimeUs = 1000;
};

/**
 * SMPTE ST 2110-30 Audio Sender (Stub Implementation).
 *
 * This class is a placeholder for future audio streaming support.
 * When fully implemented, it will handle:
 * - Capturing audio from UE audio engine
 * - Packing into 2110-30 RTP packets
 * - PTP-aligned transmission
 */
UCLASS(BlueprintType)
class RSHIP2110_API URship2110AudioSender : public UObject
{
    GENERATED_BODY()

public:
    // TODO: Implement audio sender methods

    /** Check if audio sending is supported (stub returns false) */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    static bool IsSupported() { return false; }
};
