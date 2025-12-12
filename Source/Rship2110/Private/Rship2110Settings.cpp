// Copyright Rocketship. All Rights Reserved.

#include "Rship2110Settings.h"

URship2110Settings::URship2110Settings()
{
    // Default video format: 1080p60 YCbCr 4:2:2 10-bit
    DefaultVideoFormat.Width = 1920;
    DefaultVideoFormat.Height = 1080;
    DefaultVideoFormat.FrameRateNumerator = 60;
    DefaultVideoFormat.FrameRateDenominator = 1;
    DefaultVideoFormat.ColorFormat = ERship2110ColorFormat::YCbCr_422;
    DefaultVideoFormat.BitDepth = ERship2110BitDepth::Bits_10;
    DefaultVideoFormat.bInterlaced = false;

    // Default transport: multicast on 239.0.0.1:5004
    DefaultTransportParams.DestinationIP = TEXT("239.0.0.1");
    DefaultTransportParams.DestinationPort = 5004;
    DefaultTransportParams.SourcePort = 5004;
    DefaultTransportParams.PayloadType = 96;
    DefaultTransportParams.DSCP = 46;  // EF (Expedited Forwarding)
    DefaultTransportParams.TTL = 64;
}

URship2110Settings* URship2110Settings::Get()
{
    return GetMutableDefault<URship2110Settings>();
}
