# Rship-Unreal

Unreal Engine integration for [Rocketship](https://rocketship.io) - a reactive control platform for live entertainment, broadcast, and immersive experiences.

## Plugins

| Plugin | Description | Status |
|--------|-------------|--------|
| [RshipExec](Plugins/RshipExec/README.md) | Core executor - WebSocket, targets, emitters, actions, fixtures, bindings | Stable |
| [RshipNDI](Plugins/RshipNDI/README.md) | NDI video streaming for CineCamera output | Stable |
| [Rship2110](Plugins/Rship2110/README.md) | SMPTE ST 2110 / PTP professional broadcast streaming | Beta |
| [RshipColorManagement](Plugins/RshipColorManagement/README.md) | Broadcast-grade color pipeline management | Beta |
| [RshipSpatialAudio](Plugins/RshipSpatialAudio/README.md) | Multi-channel spatial audio with VBAP/DBAP/HOA | Beta |
| [UltimateControl](Plugins/UltimateControl/README.md) | AI control - 350+ tools for Claude/LLM integration | Stable |

## Quick Start

### 1. Install Plugins

Copy the `Plugins/` folder to your UE project, or copy individual plugins you need.

### 2. Enable in Project

```json
// YourProject.uproject
{
  "Plugins": [
    { "Name": "RshipExec", "Enabled": true }
  ]
}
```

### 3. Configure Connection

**Project Settings > Game > Rocketship Settings:**

| Setting | Default | Description |
|---------|---------|-------------|
| Host | `localhost` | Rship server address |
| Port | `5155` | WebSocket port |

### 4. Create a Target

1. Add `URshipTargetComponent` to any Actor
2. Set the **Target Id** (unique name in rship)
3. Add `RS_` prefixed properties/functions:

```cpp
// These automatically become rship Actions and Emitters
UPROPERTY(EditAnywhere, BlueprintReadWrite)
float RS_Intensity = 1.0f;

UPROPERTY(EditAnywhere, BlueprintReadWrite)
FLinearColor RS_Color = FLinearColor::White;

UFUNCTION(BlueprintCallable)
void RS_PlayEffect(FName EffectName);
```

5. Press Play - your targets appear in rship!

See the [Getting Started Guide](docs/GETTING_STARTED.md) for detailed setup.

## Documentation

| Document | Description |
|----------|-------------|
| [Getting Started](docs/GETTING_STARTED.md) | New user onboarding guide |
| [Upgrade Guide](docs/UPGRADE_GUIDE.md) | Full feature reference |
| [Rate Limiting](docs/README_RATE_LIMITING.md) | Message throttling configuration |
| [Spatial Audio Architecture](docs/SPATIAL_AUDIO_ARCHITECTURE.md) | Spatial audio system design |
| [Windows Build Environment](docs/WINDOWS_BUILD_ENVIRONMENT.md) | Windows build setup |

## Features

### RshipExec (Core)
- **Target System** - Expose actors with `RS_` prefix convention
- **Fixture Control** - DMX output, GDTF/MVR import, IES profiles, beam visualization
- **Material Bindings** - Material parameters, Substrate materials, Niagara systems
- **Control Rig** - Bind Control Rig parameters for animation
- **LiveLink** - Bidirectional LiveLink subject integration
- **Timecode** - Multi-source SMPTE timecode sync
- **Sequencer Sync** - Level Sequence playback control
- **OSC Bridge** - Route OSC through rship bindings
- **PCG Auto-Bind** - Automatic binding for procedurally spawned actors
- **Recording** - Record and playback sessions

### Video Streaming
- **NDI** - Stream CineCamera views to NDI receivers
- **ST 2110** - Professional uncompressed video over IP (requires Rivermax)

### Audio
- **Spatial Audio** - 256+ speaker support, VBAP/DBAP/HOA rendering
- **External Processors** - d&b DS100 integration via OSC

### AI Control
- **UltimateControl** - 350+ methods for Claude/LLM control of the editor

## Requirements

- Unreal Engine 5.6+
- Windows or macOS
- Rocketship server for full functionality

## Console Commands

```
rship.status          # Connection status
rship.targets         # List registered targets
rship.validate        # Validate scene configuration
rship.reconnect       # Force reconnection
rship.timecode        # Timecode sync status
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         RSHIP SERVER                            │
│   Bindings • Scenes • Calendars • Event Routing                │
└─────────────────────────────────────────────────────────────────┘
        │ Actions                                    ▲ Pulses
        ▼                                            │
┌─────────────────────────────────────────────────────────────────┐
│                      UNREAL ENGINE                              │
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ RshipExec   │  │ RshipNDI    │  │ Rship2110   │             │
│  │ Targets     │  │ NDI Streams │  │ ST 2110     │             │
│  │ Fixtures    │  └─────────────┘  └─────────────┘             │
│  │ Bindings    │                                               │
│  └─────────────┘  ┌─────────────┐  ┌─────────────┐             │
│                   │ ColorMgmt   │  │ SpatialAudio│             │
│                   │ Color Pipe  │  │ VBAP/HOA    │             │
│                   └─────────────┘  └─────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

## Support

- GitHub Issues: [github.com/ignition-is-go/rship-unreal](https://github.com/ignition-is-go/rship-unreal/issues)
- Rocketship Docs: [docs.rocketship.io](https://docs.rocketship.io)

## License

Proprietary - Contact [Lucid](https://lucid.rocks) for licensing.
