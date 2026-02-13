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

## Live editor workflow: bind a content-mapped render context to a 2110 stream

This path uses the existing `RshipExec` content mapping output texture (camera contexts) directly as a 2110 source, with no extra scene capture stage in 2110.

- Keep `RshipExec` content mapping enabled and create a `RenderContext` with `sourceType = camera`.
- Match the 2110 stream format dimensions to the context render target size.
- Create the stream in 2110 and bind it to the context:

```cpp
URship2110Subsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<URship2110Subsystem>();
FRship2110VideoFormat VideoFormat;
VideoFormat.Width = 1920;
VideoFormat.Height = 1080;
VideoFormat.FrameRateNumerator = 60;
VideoFormat.FrameRateDenominator = 1;
VideoFormat.ColorFormat = ERship2110ColorFormat::RGBA_4444;
VideoFormat.BitDepth = ERship2110BitDepth::Bits_8;

FRship2110TransportParams TransportParams;
TransportParams.DestinationIP = TEXT("239.0.0.1");
TransportParams.DestinationPort = 5004;

FString StreamId = Subsystem->CreateVideoStream(VideoFormat, TransportParams);
if (Subsystem->BindVideoStreamToRenderContext(StreamId, TEXT("context-id-guid")))
{
    Subsystem->StartStream(StreamId);
}
```

```cpp
// Optional: send a crop of the source context (x, y, width, height in pixels)
FIntRect CaptureRect(200, 100, 1680, 880);
Subsystem->BindVideoStreamToRenderContextWithRect(StreamId, TEXT("context-id-guid"), CaptureRect);
```

Notes:
- This stays active in-editor as long as the bound context remains in `RshipExec` and refreshes its render target.
- If you need partial output, keep one context and set `CaptureRect` on the stream bind instead of duplicating mesh UV splits.
- If a context is not found or its texture is not a render target, binding fails and the stream keeps its previous source.

## Cluster ownership and failover (dynamic)

`URship2110Subsystem` now includes a frame-indexed cluster control state:

- `QueueClusterStateUpdate(FRship2110ClusterState)` for deterministic apply at `ApplyFrame`.
- Stream ownership map per node (`NodeStreamAssignments`).
- Strict ownership mode blocks non-owner nodes from transmitting.
- Epoch/version ordering rejects stale updates.
- Heartbeat timeout + deterministic failover candidate selection with `PromoteLocalNodeToPrimary`.

Recommended usage:

1. Authoritative node publishes `FRship2110ClusterState` with `Epoch`, `Version`, `ApplyFrame`.
2. All nodes queue the same state and apply on the same synced frame.
3. Enable `bStrictNodeOwnership` and assign each stream to exactly one node.
4. Feed authority heartbeats to standby nodes via `NotifyClusterAuthorityHeartbeat`.
5. On timeout, standby candidate promotes and increments epoch.

This model is designed to be carried by your nDisplay-synced control plane (cluster events can be a trigger, but frame-indexed state is the source of truth).

Core API hooks:

- `SubmitAuthorityClusterStatePrepare(...)`
- `ReceiveClusterStatePrepare(...)`
- `ReceiveClusterStateAck(...)`
- `ReceiveClusterStateCommit(...)`
- Outbound transport delegates:
  - `OnClusterPrepareOutbound`
  - `OnClusterAckOutbound`
  - `OnClusterCommitOutbound`

### Editor workflow (live, no MRQ)

Use this when you want to map a 2110 stream without creating custom camera projection code.

1. Enable `RshipExec` + `Rship2110` and open **Window > Rship > Rship 2110 Mapping**.
2. In the panel:
   - Pick an active 2110 stream.
   - Pick a render context.
   - Optional: set capture rectangle `x, y, w, h`.
3. Press **Bind Stream -> Context**.
4. Press **Start** on the stream.

Capture rectangle rules:
- Coordinates are in render-target pixel space (origin top-left).
- Leave all fields blank to send the full context.
- If set, `w` and `h` must match the stream output dimensions; otherwise streaming is rejected by the sender.

## Console Commands

```
rship.stream.list                                  # List active streams
rship.stream.starttest                             # Start test stream
rship.stream.stop <stream_id>                      # Stop stream
rship.ptp.status                                   # PTP status
rship.ipmx.status                                  # IPMX status
rship.cluster.status                               # Cluster state + local ownership
rship.cluster.node <node_id>                       # Set local node id
rship.cluster.assign <stream_id> <node_id>         # Assign stream ownership
rship.cluster.promote                              # Promote local node to authority
rship.cluster.heartbeat <authority_node> <e> <v>   # Record authority heartbeat
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
