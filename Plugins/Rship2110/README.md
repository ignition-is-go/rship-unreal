# Rship2110 Plugin

SMPTE ST 2110 / PTP / IPMX professional video streaming with NVIDIA Rivermax SDK integration.

> **Part of [rship-unreal](../../README.md)** - See the main README for an overview of all plugins.
>
> **Beta** - This plugin requires specific hardware and SDK setup.

## Features

- **SMPTE ST 2110-20** - Uncompressed video streaming over IP
- **SMPTE ST 2110-30** - Professional audio streaming (planned)
- **SMPTE ST 2110-40** - Ancillary data / metadata (planned)
- **PTP Synchronization** - IEEE 1588 Precision Time Protocol for frame-accurate sync
- **IPMX Compatibility** - Interoperable with broadcast infrastructure
- **Rivermax Integration** - NVIDIA GPU Direct for zero-copy video transmission
- **Color Management** - HDR/Rec.2020 support via RshipColorManagement

## Requirements

- **NVIDIA GPU** with Rivermax support (Quadro/RTX professional series)
- **Mellanox NIC** (ConnectX-5 or later) with DPDK drivers
- **NVIDIA Rivermax SDK 1.8+** installed
- **PTP Grandmaster** on the network (or software PTP for testing)
- RshipExec plugin
- RshipColorManagement plugin

## Quick Start

```json
// In your .uproject file
{
  "Plugins": [
    { "Name": "Rship2110", "Enabled": true }
  ]
}
```

### SDK Setup

1. Download Rivermax SDK from [NVIDIA Developer](https://developer.nvidia.com/rivermax)
2. Install SDK to default location or set `RIVERMAX_SDK_PATH` environment variable
3. Copy required DLLs to plugin's `ThirdParty/Rivermax/` directory (see README there)

### Configuration

**Project Settings > Rocketship > 2110 Settings:**

| Setting | Default | Description |
|---------|---------|-------------|
| Local IP | Auto | NIC IP for streaming |
| Multicast Group | `239.0.0.1` | Destination multicast address |
| Video Port | `5004` | RTP video port |
| PTP Domain | `0` | PTP clock domain |
| Video Format | `1080p60` | Output format |

### C++ Usage

```cpp
#include "Rship2110Subsystem.h"

// Get subsystem
URship2110Subsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<URship2110Subsystem>();

// Configure video sender
FRship2110VideoConfig Config;
Config.Width = 1920;
Config.Height = 1080;
Config.FrameRate = 60;
Config.ColorFormat = ERship2110ColorFormat::YCbCr_10bit;
Config.MulticastAddress = TEXT("239.0.0.1");
Config.Port = 5004;

// Start streaming
Subsystem->StartVideoStream(Config);

// Submit frames (typically from SceneCapture)
Subsystem->SubmitFrame(RenderTarget);
```

## PTP Synchronization

The plugin includes a PTP client for synchronizing with network time:

```cpp
// Get PTP status
FRshipPTPStatus Status = Subsystem->GetPTPStatus();
if (Status.bSynchronized)
{
    // PTP time available
    FTimespan PTPTime = Status.CurrentTime;
}
```

For production, use a dedicated PTP grandmaster. For testing, software PTP (ptp4l on Linux) can work.

## Console Commands

```
rship.2110.status     # Show stream status and PTP sync
rship.2110.start      # Start video stream
rship.2110.stop       # Stop video stream
rship.2110.ptp        # PTP clock status
```

## Network Requirements

| Traffic | Port | Protocol |
|---------|------|----------|
| PTP | 319, 320 | UDP |
| Video (RTP) | 5004+ | UDP Multicast |
| IGMP | - | Multicast group management |

Ensure your network switches support:
- IGMP snooping
- PTP boundary/transparent clock (for multi-switch setups)
- Jumbo frames (recommended for video)

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Rivermax not found | Check SDK installation, verify DLLs in ThirdParty |
| PTP not syncing | Verify grandmaster on network, check domain setting |
| No video output | Verify multicast routing, check NIC IP configuration |
| Frame drops | Enable jumbo frames, verify NIC offload settings |
