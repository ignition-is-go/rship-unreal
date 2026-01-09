// Copyright Rocketship. All Rights Reserved.
// SMPTE ST 2110-40 Ancillary Data Sender (Stub)
//
// Placeholder for future ancillary data streaming implementation.
// Will support:
// - Timecodes (VITC, LTC)
// - Closed captions
// - Active format description
// - Custom metadata

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Rship2110Types.h"
#include "Rship2110AncSender.generated.h"

/**
 * Ancillary data type
 */
UENUM(BlueprintType)
enum class ERship2110AncDataType : uint8
{
    Timecode_VITC   UMETA(DisplayName = "Timecode (VITC)"),
    Timecode_LTC    UMETA(DisplayName = "Timecode (LTC)"),
    ClosedCaption   UMETA(DisplayName = "Closed Captions"),
    AFD             UMETA(DisplayName = "Active Format Description"),
    Custom          UMETA(DisplayName = "Custom Metadata")
};

/**
 * SMPTE ST 2110-40 Ancillary Data Sender (Stub Implementation).
 *
 * This class is a placeholder for future ancillary data support.
 * When fully implemented, it will handle:
 * - Packing timecodes and metadata into 2110-40 packets
 * - Synchronizing with video frames
 * - Supporting multiple ancillary data types
 */
UCLASS(BlueprintType)
class RSHIP2110_API URship2110AncSender : public UObject
{
    GENERATED_BODY()

public:
    // TODO: Implement ancillary sender methods

    /** Check if ancillary sending is supported (stub returns false) */
    UFUNCTION(BlueprintCallable, Category = "Rship|2110")
    static bool IsSupported() { return false; }
};
