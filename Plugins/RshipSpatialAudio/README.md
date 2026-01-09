# Rship Spatial Audio Plugin for Unreal Engine 5.7

Professional-grade spatial audio and loudspeaker management system for immersive audio installations, live events, and themed entertainment.

## Features

- **Multi-channel output** - Support for 256+ speakers with ultra-low latency (<10ms)
- **Multiple rendering algorithms** - VBAP, DBAP, HOA/Ambisonics with phase-coherent panning
- **Full DSP stack** - Per-speaker EQ, delay, limiting, crossover, polarity
- **External processor integration** - d&b DS100, P1 via OSC
- **SMAART calibration import** - Auto-EQ generation from measurements
- **rShip/Myko integration** - Real-time WebSocket control plane
- **Editor tools** - 3D speaker visualization, component visualization

## Quick Start

### 1. Enable the Plugin

Add to your `.uproject` file:
```json
{
  "Plugins": [
    {
      "Name": "RshipSpatialAudio",
      "Enabled": true
    }
  ]
}
```

### 2. Basic Setup (C++)

```cpp
#include "RshipSpatialAudioManager.h"

// Get the manager
URshipSpatialAudioManager* Manager = GetSpatialAudioManager();

// Create a venue
Manager->CreateVenue(TEXT("My Venue"));

// Add speakers
FSpatialSpeaker Speaker;
Speaker.Name = TEXT("Front Left");
Speaker.Position = FVector(-500, -300, 200);  // cm
Speaker.OutputChannel = 1;
FGuid SpeakerId = Manager->AddSpeaker(Speaker);

// Create audio objects
FGuid ObjectId = Manager->CreateAudioObject(TEXT("Sound Source 1"));

// Move audio object (gains computed automatically)
Manager->SetObjectPosition(ObjectId, FVector(100, 0, 100));
```

### 3. Basic Setup (Blueprint)

1. Get `RshipSpatialAudioManager` reference
2. Call `Create Venue`
3. Use `Add Speaker` to configure your speaker array
4. Call `Create Audio Object` for each sound source
5. Use `Set Object Position` to move sources in real-time

### 4. External Processor (DS100)

```cpp
// Configure DS100 connection
FExternalProcessorConfig Config;
Config.ProcessorType = EExternalProcessorType::DS100;
Config.Network.Host = TEXT("192.168.1.100");
Config.Network.SendPort = 50010;
Config.Network.ReceivePort = 50011;

Manager->ConfigureExternalProcessor(Config);
Manager->ConnectExternalProcessor();

// Map audio objects to DS100 sources
Manager->MapObjectToExternalProcessor(ObjectId, 1);  // Object -> Source 1

// Enable automatic position forwarding
Manager->SetExternalProcessorForwarding(true);
```

## Architecture

### Module Structure

```
Plugins/RshipSpatialAudio/
├── Source/
│   ├── RshipSpatialAudioRuntime/     # Runtime module
│   │   ├── Core/                     # Data types, venue, speaker structs
│   │   ├── Audio/                    # Audio processor, rendering engine
│   │   ├── Rendering/                # VBAP, DBAP, HOA renderers
│   │   ├── DSP/                      # EQ, delay, limiter
│   │   ├── ExternalProcessor/        # DS100, OSC client
│   │   └── Myko/                     # rShip integration types
│   └── RshipSpatialAudioEditor/      # Editor module
│       ├── Visualizers/              # Speaker/zone visualizers
│       └── Calibration/              # SMAART import, auto-EQ
```

### Signal Flow

```
Audio Objects
     │
     ▼ (VBAP/DBAP/HOA gains)
┌─────────────────────────────────┐
│ Per-Speaker DSP Chain           │
│  Input Gain → HP Filter → EQ → │
│  LP Filter → Limiter → Delay → │
│  Polarity → Output Gain         │
└─────────────────────────────────┘
     │
     ▼
Output Channels (Dante/ASIO/MADI)
```

### Rendering Algorithms

| Algorithm | Use Case | Speakers |
|-----------|----------|----------|
| **VBAP** | Precise localization, front arrays | 3+ (triangulated) |
| **DBAP** | Immersive, 360° installations | Any configuration |
| **HOA** | VR/AR, dome/sphere, ambisonic content | 4+ (decoded) |
| **Direct** | Channel-based routing | N/A |

## API Reference

### URshipSpatialAudioManager

Main manager class for all spatial audio operations.

#### Venue Management
- `CreateVenue(Name)` - Initialize a new venue
- `ExportVenueToJson()` / `ImportVenueFromJson()` - Serialization
- `ExportVenueToFile()` / `ImportVenueFromFile()` - File I/O

#### Speaker Management
- `AddSpeaker(Speaker)` - Add speaker, returns ID
- `UpdateSpeaker(Id, Speaker)` - Modify existing speaker
- `RemoveSpeaker(Id)` - Remove speaker
- `GetSpeaker(Id, OutSpeaker)` - Query speaker
- `GetAllSpeakers()` - Get all speakers

#### Speaker DSP
- `SetSpeakerGain(Id, GainDb)` - Set gain
- `SetSpeakerDelay(Id, DelayMs)` - Set alignment delay
- `SetSpeakerMute(Id, bMuted)` - Mute/unmute
- `SetSpeakerPolarity(Id, bInverted)` - Invert polarity
- `SetSpeakerEQ(Id, Bands)` - Set EQ bands
- `SetSpeakerDSP(Id, DSPState)` - Apply full DSP state

#### Audio Objects
- `CreateAudioObject(Name)` - Create object, returns ID
- `RemoveAudioObject(Id)` - Remove object
- `SetObjectPosition(Id, Position)` - Set 3D position
- `SetObjectSpread(Id, Spread)` - Set source width (0-180°)
- `SetObjectGain(Id, GainDb)` - Set object gain
- `SetObjectZoneRouting(Id, ZoneIds)` - Route to zones

#### Zones
- `AddZone(Zone)` - Create zone, returns ID
- `SetZoneRenderer(Id, RendererType)` - Set rendering algorithm

#### External Processors
- `ConfigureExternalProcessor(Config)` - Setup processor
- `ConnectExternalProcessor()` / `DisconnectExternalProcessor()`
- `MapObjectToExternalProcessor(ObjectId, SourceNum)` - Create mapping
- `SetExternalProcessorForwarding(bEnable)` - Auto-forward positions

#### Scene Management
- `StoreScene(Name)` - Capture current state
- `RecallScene(Id, bInterpolate, TimeMs)` - Restore state

### Rendering Configuration

```cpp
// Set global renderer type
Manager->SetGlobalRendererType(ESpatialRendererType::VBAP);

// Configure VBAP
FSpatialRendererRegistry& Registry = GetGlobalRendererRegistry();
Registry.SetVBAPConfig(
    false,                    // bUse2D
    FVector::ZeroVector,      // ReferencePoint
    true                      // bPhaseCoherent
);

// Configure HOA
Registry.SetHOAConfig(
    3,                        // Order (1-5)
    3,                        // DecoderType (AllRAD)
    FVector::ZeroVector       // ListenerPosition
);
```

## Performance

### Optimization Tips

1. **Batch position updates** - Use `BeginBatch()`/`EndBatch()` for external processors
2. **Rate limiting** - Configure `MaxMessagesPerSecond` for external processors
3. **Position threshold** - Small movements are ignored to reduce network traffic
4. **DSP bypass** - Use `SetBypass(true)` on unused speakers

### Benchmarks (Reference)

| Metric | Value |
|--------|-------|
| VBAP gain computation (256 speakers) | <0.5ms |
| HOA 3rd order encode+decode | <1ms |
| Per-speaker DSP (8-band EQ + limiter) | <0.1ms |
| OSC message latency | <2ms network |

## Calibration Workflow

### 1. Import SMAART Measurements

```cpp
#include "Calibration/SSMAARTImporter.h"
#include "Calibration/SCalibrationPresetManager.h"

FSMAARTImporter Importer;
FSMAARTImportResult Result = Importer.ImportFromFile(TEXT("/path/to/measurement.txt"));

if (Result.bSuccess)
{
    FCalibrationPresetManager PresetManager;
    FAutoEQSettings Settings;
    Settings.MaxBands = 8;
    Settings.MinGainDb = -12.0f;
    Settings.MaxGainDb = 6.0f;

    FSpeakerCalibrationPreset Preset = PresetManager.CreatePreset(
        Result.Measurement,
        SpeakerId,
        TEXT("Speaker 1"),
        Settings
    );

    // Apply to speaker
    PresetManager.ApplyPresetToSpeaker(Manager, SpeakerId, Preset);
}
```

### 2. Save/Load Calibration Sets

```cpp
// Save entire venue calibration
PresetManager.SaveCalibrationSet(TEXT("Main Room"), TEXT("/path/to/calibration.json"));

// Load and apply
PresetManager.LoadCalibrationSet(TEXT("/path/to/calibration.json"), Manager);
```

## rShip/Myko Integration

The spatial audio system integrates with rShip through the standard Target/Emitter/Action model:

### Targets Created
- Venue target
- Speaker targets (with DSP actions)
- Zone targets (with renderer actions)
- Audio object targets (with position/gain actions)

### Actions Available
- `setSpeakerGain`, `setSpeakerDelay`, `setSpeakerMute`
- `setObjectPosition`, `setObjectSpread`, `setObjectGain`
- `setZoneRenderer`, `recallScene`

### Emitters (60Hz)
- Speaker meters (peak, RMS)
- Object positions
- Limiter gain reduction

## Troubleshooting

### No audio output
1. Check speaker output channel assignments
2. Verify audio device configuration
3. Check `HasAudioProcessor()` returns true

### External processor not connecting
1. Verify IP address and ports
2. Check firewall settings for UDP
3. Use `GetExternalProcessorStatus()` for diagnostics

### Gain computation issues
1. Ensure speakers are properly positioned (not coplanar for VBAP)
2. Check `ValidateConfiguration()` for warnings
3. Verify renderer type matches speaker layout

## License

Copyright Rocketship. All Rights Reserved.

## Version History

- **v1.0** - M1-M12 complete: Full spatial audio system
  - VBAP, DBAP, HOA rendering
  - Full DSP stack
  - DS100 integration
  - SMAART calibration import
  - rShip/Myko integration
