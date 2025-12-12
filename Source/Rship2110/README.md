# Rship2110 Module - SMPTE 2110 / PTP / IPMX Integration

This module provides professional media-over-IP streaming for Unreal Engine 5.7, implementing:

- **PTP (IEEE 1588 / SMPTE 2059)** - Precision time synchronization with grandmaster clocks
- **SMPTE ST 2110** - Uncompressed video streaming via NVIDIA Rivermax SDK
- **IPMX (NMOS IS-04/IS-05)** - Discovery and connection management

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         URship2110Subsystem                              │
│  ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────────┐  │
│  │  URshipPTPService │ │ URivermaxManager  │ │  URshipIPMXService    │  │
│  │                   │ │                   │ │                       │  │
│  │  • PTP Time       │ │ • Device Mgmt     │ │  • Node Registration  │  │
│  │  • Frame Timing   │ │ • Stream Creation │ │  • Sender Registration│  │
│  │  • RTP Timestamps │ │ • GPUDirect       │ │  • SDP Generation     │  │
│  └─────────┬─────────┘ └─────────┬─────────┘ └───────────┬───────────┘  │
│            │                     │                       │              │
└────────────┼─────────────────────┼───────────────────────┼──────────────┘
             │                     │                       │
             ▼                     ▼                       ▼
    ┌────────────────┐   ┌────────────────┐     ┌────────────────┐
    │ PTP Grandmaster│   │  ConnectX NIC  │     │ NMOS Registry  │
    │ (IEEE 1588)    │   │  (Rivermax)    │     │ (IS-04/IS-05)  │
    └────────────────┘   └────────────────┘     └────────────────┘
```

## Prerequisites

### Hardware Requirements
- **NVIDIA ConnectX-5/6/7** network adapter (for Rivermax)
- GPU with CUDA support (for optional GPUDirect RDMA)
- Network with PTP grandmaster for timing

### Software Requirements
- Unreal Engine 5.7
- Windows 10/11 or Linux
- NVIDIA Rivermax SDK (optional, for full 2110 support)

## Installation

### 1. Install Rivermax SDK (Optional but Recommended)

Download and install the NVIDIA Rivermax SDK from the NVIDIA website.

Set the environment variable:
```bash
set RIVERMAX_SDK_PATH=C:\Program Files\Mellanox\Rivermax
```

Or place the SDK in `Source/Rship2110/ThirdParty/Rivermax/`

### 2. Enable the Module

The module is automatically enabled when the plugin loads. Configure via Project Settings.

### 3. Configure Settings

Open **Project Settings > Plugins > Rship 2110 Settings**:

**PTP Settings:**
- Enable PTP: On
- PTP Domain: 127 (SMPTE 2059)
- Interface IP: (leave empty for auto-detect)

**Rivermax Settings:**
- Enable Rivermax: On
- Enable GPUDirect: On (if available)
- Interface IP: (your ConnectX NIC IP)

**IPMX Settings:**
- Enable IPMX: On
- Registry URL: (your NMOS registry or empty for mDNS)
- Auto-Register: On

## Usage

### Blueprint API

```cpp
// Get the 2110 subsystem
URship2110Subsystem* Subsystem = URship2110BlueprintLibrary::GetRship2110Subsystem(this);

// Check PTP lock status
if (Subsystem->IsPTPLocked())
{
    FRshipPTPTimestamp PTPTime = Subsystem->GetPTPTime();
}

// Create a video stream
FRship2110VideoFormat Format;
Format.Width = 1920;
Format.Height = 1080;
Format.FrameRateNumerator = 60;
Format.FrameRateDenominator = 1;

FRship2110TransportParams Transport;
Transport.DestinationIP = TEXT("239.0.0.1");
Transport.DestinationPort = 5004;

FString StreamId = Subsystem->CreateVideoStream(Format, Transport, true);

// Start streaming
Subsystem->StartStream(StreamId);
```

### C++ API

```cpp
#include "Rship2110Subsystem.h"
#include "PTP/RshipPTPService.h"
#include "Rivermax/Rship2110VideoSender.h"

void MyClass::StartStreaming()
{
    URship2110Subsystem* Subsystem = GEngine->GetEngineSubsystem<URship2110Subsystem>();

    // Access services directly
    URshipPTPService* PTP = Subsystem->GetPTPService();
    URivermaxManager* Rivermax = Subsystem->GetRivermaxManager();
    URshipIPMXService* IPMX = Subsystem->GetIPMXService();

    // Get PTP-aligned frame timing
    if (PTP && PTP->IsLocked())
    {
        FFrameRate FrameRate(60, 1);
        FRshipPTPTimestamp NextFrame = PTP->GetNextFrameBoundary(FrameRate);
        int64 TimeUntilFrame = PTP->GetTimeUntilNextFrameNs(FrameRate);
    }
}
```

### Console Commands

Debug and test via console:

```
rship.2110.help              # Show all commands

# PTP
rship.ptp.status             # Display PTP sync status
rship.ptp.resync             # Force resynchronization

# Rivermax
rship.rivermax.status        # Show device info
rship.rivermax.enumerate     # Re-scan devices
rship.rivermax.select <n>    # Select device

# Streams
rship.stream.list            # List active streams
rship.stream.starttest       # Start test 1080p60 stream
rship.stream.stop <id>       # Stop stream

# IPMX
rship.ipmx.status            # Show IPMX status
rship.ipmx.connect [url]     # Connect to registry
rship.ipmx.dumphandles       # Dump resources
```

## Supported Formats

### Video (ST 2110-20)
- Resolutions: 720p to 8K (arbitrary supported)
- Frame rates: Up to 120 fps
- Color formats: YCbCr 4:2:2, YCbCr 4:4:4, RGB 4:4:4, RGBA 4:4:4:4
- Bit depths: 8, 10, 12, 16-bit

### Audio (ST 2110-30) - Coming Soon
- Sample rates: 48kHz, 96kHz
- Bit depths: 16, 24, 32-bit
- Channels: Up to 64

### Ancillary (ST 2110-40) - Coming Soon
- Timecodes (VITC, LTC)
- Closed captions
- Custom metadata

## Configuration (DefaultGame.ini)

```ini
[/Script/Rship2110.URship2110Settings]
bEnablePTP=True
PTPDomain=127
PTPInterfaceIP=

bEnableRivermax=True
RivermaxInterfaceIP=
bEnableGPUDirect=True
MaxConcurrentStreams=4
BufferPoolSizeMB=256

bEnableIPMX=True
IPMXRegistryUrl=
IPMXNodeLabel=Unreal Engine IPMX Node
bIPMXAutoRegister=True
IPMXHeartbeatIntervalSeconds=5

bAlignFramesToPTP=True
MaxFrameLatencyMs=16
bEnablePrerollBuffering=True
PrerollFrames=2

LogVerbosity=1
bShowDebugOverlay=False
```

## Fallback Behavior

The module gracefully handles missing components:

- **No Rivermax SDK**: Operates in stub mode, simulates streaming
- **No PTP Grandmaster**: Uses system clock for timing
- **No IPMX Registry**: Operates standalone, local API available

## Performance Considerations

For optimal performance:

1. **Network**: Use dedicated 25/100GbE network for 2110 traffic
2. **GPUDirect**: Enable for zero-copy GPU-to-NIC transfer
3. **Pre-roll**: Use 2-3 frame pre-roll buffer for smooth playback
4. **DSCP**: Configure network QoS (default DSCP=46 for EF)

## Troubleshooting

### PTP not locking
- Verify grandmaster is reachable on network
- Check PTP domain (default 127 for SMPTE 2059)
- Ensure NIC supports hardware timestamping

### No devices found
- Install Rivermax SDK and drivers
- Verify ConnectX firmware is current
- Check `RIVERMAX_SDK_PATH` environment variable

### Stream not starting
- Verify destination multicast route exists
- Check firewall allows UDP traffic
- Ensure source IP matches NIC IP

## API Reference

See header files in `Source/Rship2110/Public/` for complete API documentation:

- [Rship2110Subsystem.h](Public/Rship2110Subsystem.h) - Main subsystem
- [Rship2110Types.h](Public/Rship2110Types.h) - Type definitions
- [PTP/RshipPTPService.h](Public/PTP/RshipPTPService.h) - PTP service
- [Rivermax/RivermaxManager.h](Public/Rivermax/RivermaxManager.h) - Device management
- [Rivermax/Rship2110VideoSender.h](Public/Rivermax/Rship2110VideoSender.h) - Video streaming
- [IPMX/RshipIPMXService.h](Public/IPMX/RshipIPMXService.h) - NMOS discovery
