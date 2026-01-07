# RshipExec Plugin

Core Rocketship executor for Unreal Engine - connect your UE project to the rship control platform.

> **Part of [rship-unreal](../../README.md)** - See the main README for an overview of all plugins.

## Features

- **WebSocket Client** - High-performance connection to rship server with auto-reconnect
- **Target System** - Expose actors as controllable entities with the `RS_` prefix convention
- **Emitters & Actions** - Bidirectional property binding (read/write from rship)
- **Fixture Control** - DMX output, GDTF/MVR import, IES profiles, beam visualization
- **Material Bindings** - Control material parameters, Substrate materials, and Niagara systems
- **Control Rig** - Bind Control Rig parameters for character/mechanical animation
- **LiveLink** - Bidirectional LiveLink subject integration
- **Timecode** - Multi-source SMPTE timecode synchronization
- **Sequencer Sync** - Sync Level Sequence playback with rship
- **OSC Bridge** - Route OSC messages through rship bindings
- **PCG Auto-Bind** - Automatic binding for procedurally spawned actors
- **Recording** - Record and playback emitter/action sessions

## Quick Start

```json
// In your .uproject file
{
  "Plugins": [
    { "Name": "RshipExec", "Enabled": true }
  ]
}
```

1. Add `URshipTargetComponent` to any actor
2. Set the **Target Id** (unique name in rship)
3. Add `RS_` prefixed properties to expose them:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
float RS_Intensity = 1.0f;

UPROPERTY(EditAnywhere, BlueprintReadWrite)
FLinearColor RS_Color = FLinearColor::White;

UFUNCTION(BlueprintCallable)
void RS_PlayEffect(FName EffectName);
```

4. Configure connection in **Project Settings > Rocketship**
5. Press Play - your targets appear in rship

See [Getting Started Guide](../../docs/GETTING_STARTED.md) for detailed setup.

## Configuration

**Project Settings > Game > Rocketship Settings:**

| Setting | Default | Description |
|---------|---------|-------------|
| Host | `localhost` | Rship server address |
| Port | `5155` | WebSocket port |
| Auto Connect | `true` | Connect on play |
| TCP No Delay | `true` | Disable Nagle's algorithm |
| Auto Reconnect | `true` | Reconnect on disconnect |

## Editor Panels

Access via **Window > Rship**:

| Panel | Purpose |
|-------|---------|
| Status | Connection, target list, diagnostics |
| Timecode | SMPTE timecode configuration |
| LiveLink | LiveLink subject mapping |
| Materials | Material parameter bindings |
| Fixtures | Lighting fixture management |
| NDI | Camera NDI streaming (requires RshipNDI) |
| Test | Offline testing and validation |

## Console Commands

```
rship.status          # Connection status
rship.targets         # List registered targets
rship.validate        # Validate scene
rship.reconnect       # Force reconnection
rship.timecode        # Timecode status
```

## Modules

| Module | Type | Description |
|--------|------|-------------|
| RshipExec | Runtime | Core executor, WebSocket, targets, bindings |
| RshipExecEditor | Editor | UI panels, editor integration |

## Documentation

- [Getting Started](../../docs/GETTING_STARTED.md) - New user onboarding
- [Upgrade Guide](../../docs/UPGRADE_GUIDE.md) - Feature reference
- [Rate Limiting](../../docs/README_RATE_LIMITING.md) - Message throttling
- [PCG Binding](Source/RshipExec/Public/PCG/README_PCG_BINDING.md) - PCG auto-bind system
