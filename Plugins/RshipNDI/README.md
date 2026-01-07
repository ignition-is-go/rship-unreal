# RshipNDI Plugin

NDI video streaming for CineCamera output - stream camera views to NDI receivers on the network.

> **Part of [rship-unreal](../../README.md)** - See the main README for an overview of all plugins.

## Features

- **CineCamera Streaming** - Stream any CineCamera view as an NDI source
- **Multiple Streams** - Run multiple simultaneous NDI outputs
- **Exposure Matching** - Optional viewport exposure matching for consistent output
- **Color Management** - Integration with RshipColorManagement for broadcast-accurate color
- **Rust FFI** - High-performance NDI sender built with Rust/libloading

## Requirements

- NDI SDK runtime installed on the machine
- RshipColorManagement plugin (optional, for color management)
- CineCameraSceneCapture plugin (included with UE)

## Quick Start

```json
// In your .uproject file
{
  "Plugins": [
    { "Name": "RshipNDI", "Enabled": true }
  ]
}
```

### Blueprint Setup

1. Add `URshipNDIStreamComponent` to your CineCamera actor
2. Configure the stream settings:
   - **Stream Name** - NDI source name visible on the network
   - **Resolution** - Output resolution (default: 1920x1080)
   - **Frame Rate** - Target frame rate (default: 30)
3. Call `StartStreaming()` to begin

### C++ Setup

```cpp
#include "RshipNDIStreamComponent.h"

// Add component to camera
URshipNDIStreamComponent* NDIStream = NewObject<URshipNDIStreamComponent>(CameraActor);
NDIStream->StreamName = TEXT("UE5-Camera-Main");
NDIStream->Resolution = FIntPoint(1920, 1080);
NDIStream->FrameRate = 30;
NDIStream->RegisterComponent();

// Start streaming
NDIStream->StartStreaming();

// Stop when done
NDIStream->StopStreaming();
```

## Configuration

| Property | Default | Description |
|----------|---------|-------------|
| Stream Name | `UE5-NDI` | NDI source name on network |
| Resolution | `1920x1080` | Output resolution |
| Frame Rate | `30` | Target frame rate |
| Match Viewport Exposure | `false` | Sync exposure with editor viewport |
| Enable Alpha | `false` | Include alpha channel (RGBA vs RGB) |

## Editor Panel

Access via **Window > Rship > NDI**:

- View active NDI streams
- Start/stop streams
- Configure stream settings
- Monitor frame rates and performance

## NDI SDK Installation

### Windows
Download and install [NDI Tools](https://ndi.tv/tools/) - the SDK runtime is included.

### macOS
```bash
brew install --cask ndi-tools
```

Or download from [ndi.tv/tools](https://ndi.tv/tools/).

## Architecture

```
CineCamera → SceneCapture2D → RenderTarget → Rust NDI Sender → Network
```

The plugin uses a Rust-based NDI sender (`rship-ndi-sender`) for high-performance frame submission, avoiding the overhead of Blueprint/C++ NDI SDK bindings.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Black output | Check camera has valid view, verify SceneCapture settings |
| NDI source not visible | Verify NDI SDK installed, check firewall allows mDNS |
| Low frame rate | Reduce resolution, check GPU performance |
| Color mismatch | Enable RshipColorManagement, configure color space |
