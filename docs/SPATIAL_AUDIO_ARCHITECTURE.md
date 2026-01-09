# Unreal Spatial Audio & Loudspeaker Management System

> **Status**: ✅ COMPLETE - Production Ready
> **Last Updated**: 2025-12-10
> **Target**: Unreal Engine 5.7
> **Completion**: 100% - All milestones implemented

## Executive Summary

This document specifies an Unreal Engine 5.7 native spatial audio and loudspeaker management system that:
- Competes with L-Acoustics L-ISA, d&b Soundscape/DS100, and Meyer Spacemap Go in capability
- Integrates exclusively through the existing rShip/Myko control plane
- Supports 256+ output channels with ultra-low latency (<10ms)
- Provides vendor-agnostic, production-grade loudspeaker control

---

## Table of Contents

1. [Requirements](#1-requirements)
   - [Functional Requirements](#11-functional-requirements)
   - [Non-Functional Requirements](#12-non-functional-requirements)
   - [rShip/Myko Schema Design](#13-rshipmyko-schema-design)
   - [Assumptions & Constraints](#14-assumptions--constraints)
2. [Architecture](#2-architecture)
   - [Plugin/Module Layout](#21-pluginmodule-layout)
   - [Core Data Model](#22-core-data-model)
   - [Rendering Pathway](#23-rendering-pathway)
   - [Renderer Strategies](#24-renderer-strategies)
   - [DSP Architecture](#25-dsp-architecture)
   - [rShip/Myko Integration](#26-rshipmyko-integration)
   - [Editor Experience](#27-editor-experience)
3. [Implementation Roadmap](#3-implementation-roadmap)
   - [Milestone Overview](#31-milestone-overview)
   - [Detailed Milestones](#32-detailed-milestones)

---

## 1. Requirements

### 1.1 Functional Requirements

#### FR-1: Loudspeaker Topology & Layout

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-1.1 | Support hierarchical speaker organization: Venue → Zone → Array → Speaker | Must |
| FR-1.2 | Store per-speaker metadata: position (3D), orientation (3D), make/model, dispersion pattern, power rating | Must |
| FR-1.3 | Support speaker types: point source, line array element, subwoofer, fill, delay, surround, overhead | Must |
| FR-1.4 | Manual speaker placement in Unreal Editor 3D viewport | Must |
| FR-1.5 | Import speaker layouts from external tools (EASE, ArrayCalc, Soundvision) via JSON/CSV | Must |
| FR-1.6 | Runtime dynamic speaker creation, modification, and removal | Must |
| FR-1.7 | Speaker grouping for bulk operations (mute, solo, level trim) | Must |

#### FR-2: Audio Object Management

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-2.1 | Audio objects represent spatialized sound sources with 3D position, size/spread, and rendering parameters | Must |
| FR-2.2 | Audio objects can be attached to Unreal Actors or positioned independently | Must |
| FR-2.3 | Support object metadata: name, color, group assignment, zone routing | Must |
| FR-2.4 | Object automation via Sequencer timeline or real-time rShip control | Must |
| FR-2.5 | Object count scales with hardware (target: 256+ simultaneous objects) | Should |

#### FR-3: Spatial Rendering

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-3.1 | **VBAP** (Vector Base Amplitude Panning) for arbitrary speaker arrays | Must |
| FR-3.2 | **DBAP** (Distance-Based Amplitude Panning) for installations | Must |
| FR-3.3 | **Phase-coherent panning**: gain AND delay per speaker, not amplitude-only | Must |
| FR-3.4 | HOA/Ambisonics rendering with decode to arbitrary layouts | Should |
| FR-3.5 | Stereo downmix for monitoring/preview | Should |
| FR-3.6 | Binaural rendering for headphone preview | Should |
| FR-3.7 | Per-zone renderer assignment (e.g., VBAP for main array, DBAP for surrounds) | Must |
| FR-3.8 | Renderer hot-swapping without audio interruption | Should |

#### FR-4: Routing & Bus Structure

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-4.1 | Flexible routing: Audio Objects → Buses → Output Channels → Physical IO | Must |
| FR-4.2 | Routing matrix with arbitrary source-to-destination mapping | Must |
| FR-4.3 | Bus types: object bus, zone bus, master bus, aux send/return | Must |
| FR-4.4 | Per-bus gain, mute, solo, polarity invert | Must |
| FR-4.5 | Output channel mapping to physical IO channels (1:1, N:M) | Must |

#### FR-5: Loudspeaker Management DSP

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-5.1 | Per-output parametric EQ (minimum 8 bands, IIR) | Must |
| FR-5.2 | Per-output delay alignment (sample-accurate, 0-1000ms range) | Must |
| FR-5.3 | Per-output gain staging with fine resolution (0.1dB steps) | Must |
| FR-5.4 | Per-output limiter with configurable threshold, attack, release | Must |
| FR-5.5 | Per-output polarity/phase invert | Must |
| FR-5.6 | Per-output high-pass/low-pass filters (crossover) | Should |
| FR-5.7 | FIR filter support for advanced EQ/alignment | Should |
| FR-5.8 | Compressor/dynamics per bus | Should |

#### FR-6: Calibration & Alignment

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-6.1 | Import SMAART measurement data (transfer functions, delay, EQ curves) | Must |
| FR-6.2 | Store calibration presets per speaker/zone | Must |
| FR-6.3 | A/B comparison between calibration states | Should |
| FR-6.4 | Auto-alignment based on speaker positions (geometric delay calculation) | Should |
| FR-6.5 | Auto-calibration with impulse response measurement (future) | Could |

#### FR-7: Scene/Preset Management

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-7.1 | Scenes capture complete system state: routing, DSP, object positions | Must |
| FR-7.2 | Scene recall via rShip Actions (no internal interpolation—rShip handles blending) | Must |
| FR-7.3 | Partial scene recall (e.g., only DSP, only routing) | Should |
| FR-7.4 | Scene export/import for backup and transfer | Should |

#### FR-8: IO Abstraction

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-8.1 | Abstract IO layer supporting multiple transports | Must |
| FR-8.2 | Marian Dante card support (primary target) | Must |
| FR-8.3 | Generic ASIO/CoreAudio support for development | Should |
| FR-8.4 | MADI support | Should |
| FR-8.5 | AES67/AVB support | Could |
| FR-8.6 | "Controller-only" mode: output metadata/control to external processors (no direct audio) | Must |

#### FR-9: External Processor Integration

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-9.1 | External speaker processors (DS100, P1, etc.) controlled as separate rShip Executors | Must |
| FR-9.2 | Common control protocol abstraction (OSC, proprietary) per processor type | Must |
| FR-9.3 | Bidirectional sync: Unreal state ↔ external processor state | Should |

#### FR-10: rShip/Myko Integration

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-10.1 | Venue as Myko Instance | Must |
| FR-10.2 | Zones, arrays, buses as Myko Targets | Must |
| FR-10.3 | Speakers as sub-entities within Targets (or individual Targets for large systems) | Must |
| FR-10.4 | Actions for: parameter changes, routing updates, scene recall, object positioning | Must |
| FR-10.5 | Emitters for: real-time meters (~60Hz), status/health, active configuration | Must |
| FR-10.6 | All audio system configuration via rShip—no parallel control APIs | Must |
| FR-10.7 | Reuse existing RshipExec plugin connection, threading, and rate limiting | Must |

---

### 1.2 Non-Functional Requirements

#### NFR-1: Performance

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-1.1 | End-to-end audio latency | <10ms |
| NFR-1.2 | Maximum output channels | 256+ |
| NFR-1.3 | Maximum simultaneous audio objects | 256+ |
| NFR-1.4 | DSP CPU budget | <25% of available cores |
| NFR-1.5 | Memory footprint | <500MB for 256-channel system |
| NFR-1.6 | Telemetry update rate | 60Hz |

#### NFR-2: Scalability

| ID | Requirement |
|----|-------------|
| NFR-2.1 | Performance scales linearly with hardware capability |
| NFR-2.2 | Graceful degradation under load (drop objects, not crash) |
| NFR-2.3 | Dynamic resource allocation based on active object/channel count |

#### NFR-3: Reliability

| ID | Requirement |
|----|-------------|
| NFR-3.1 | No audio dropouts during normal operation |
| NFR-3.2 | Graceful handling of IO device disconnection/reconnection |
| NFR-3.3 | State persistence across editor/game sessions |
| NFR-3.4 | Comprehensive error logging for diagnostics |

#### NFR-4: Real-Time Safety

| ID | Requirement |
|----|-------------|
| NFR-4.1 | Audio thread must be lock-free (no mutex, no allocation) |
| NFR-4.2 | Parameter changes via lock-free queues/atomics |
| NFR-4.3 | No blocking operations on audio thread |

---

### 1.3 rShip/Myko Schema Design

#### Entity Hierarchy

```
Myko Instance: SpatialAudioVenue
├── Target: Zone_MainArray
│   ├── Emitter: meters (per-speaker levels)
│   ├── Emitter: status (online, clipping, limiting)
│   ├── Action: setGain(speakerId, dB)
│   ├── Action: setDelay(speakerId, ms)
│   ├── Action: setEQ(speakerId, bands[])
│   └── Action: setMute(speakerId, bool)
├── Target: Zone_Surrounds
│   └── ...
├── Target: Bus_Master
│   ├── Emitter: masterLevel
│   ├── Action: setMasterGain(dB)
│   └── Action: setMasterMute(bool)
├── Target: Renderer_Main
│   ├── Emitter: activeRenderer
│   ├── Action: setRendererType(type)
│   └── Action: setRendererParams(params)
├── Target: AudioObject_001
│   ├── Emitter: position (x, y, z)
│   ├── Emitter: level
│   ├── Action: setPosition(x, y, z)
│   ├── Action: setSpread(degrees)
│   └── Action: setZoneRouting(zones[])
└── Target: SceneManager
    ├── Emitter: activeScene
    └── Action: recallScene(sceneId)
```

#### Pulse Data Formats

**Meter Pulse** (60Hz):
```json
{
  "emitterId": "venue:zone_main:meters",
  "data": {
    "speakers": [
      {"id": "spk_001", "rms": 0.72, "peak": 0.89, "limiting": false},
      {"id": "spk_002", "rms": 0.68, "peak": 0.82, "limiting": false}
    ],
    "timestamp": 1234567890.123
  }
}
```

**Object Position Pulse**:
```json
{
  "emitterId": "venue:object_001:position",
  "data": {
    "x": 10.5, "y": 0.0, "z": 2.0,
    "spread": 15.0,
    "level": 0.85
  }
}
```

---

### 1.4 Assumptions & Constraints

#### Assumptions

1. Unreal Engine 5.7 audio engine provides sufficient low-level access for custom rendering
2. Marian Dante cards expose ASIO interface on Windows
3. External processors support OSC or documented control protocols
4. rShip server handles scene parameter interpolation
5. Users have professional audio system design knowledge

#### Out of Scope (This Epic)

1. Acoustic simulation/raytracing (separate epic)
2. Room modeling and reverb zones
3. Immersive audio format encoding (Dolby Atmos, MPEG-H)
4. Microphone input and feedback suppression
5. Network audio transport implementation (Dante/AES67 at protocol level)

#### Constraints

1. Must integrate with existing RshipExec plugin (no parallel connections)
2. Must work with vanilla UE audio (no Wwise/FMOD dependency)
3. Ultra-low latency requires careful buffer management
4. Real-time safety precludes certain convenient patterns

---

## 2. Architecture

### 2.1 Plugin/Module Layout

#### Recommended Structure

```
Plugins/
└── RshipSpatialAudio/
    ├── RshipSpatialAudio.uplugin
    ├── Source/
    │   ├── RshipSpatialAudioRuntime/     # Runtime module (audio engine)
    │   │   ├── Public/
    │   │   │   ├── Core/                 # Core data types
    │   │   │   ├── Topology/             # Speaker layouts, zones
    │   │   │   ├── Rendering/            # VBAP, DBAP, HOA renderers
    │   │   │   ├── DSP/                  # EQ, delay, limiter, filters
    │   │   │   ├── Routing/              # Buses, routing matrix
    │   │   │   ├── IO/                   # Abstract IO layer
    │   │   │   ├── Objects/              # Audio objects
    │   │   │   └── RshipBinding/         # Myko integration
    │   │   └── Private/
    │   │
    │   └── RshipSpatialAudioEditor/      # Editor module (tools)
    │       ├── Public/
    │       │   ├── Widgets/              # Slate widgets
    │       │   ├── Visualizers/          # 3D visualizers
    │       │   └── Importers/            # SMAART, layout importers
    │       └── Private/
    │
    └── Content/
        └── Presets/                      # Default speaker profiles, DSP presets
```

#### Module Dependency Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    RshipSpatialAudioEditor                   │
│  (Editor tools, visualizers, importers - Editor only)        │
└─────────────────────────────┬───────────────────────────────┘
                              │ depends on
┌─────────────────────────────▼───────────────────────────────┐
│                   RshipSpatialAudioRuntime                   │
│  (Core audio engine, DSP, renderers - Runtime)               │
└─────────────────────────────┬───────────────────────────────┘
                              │ depends on
┌─────────────────────────────▼───────────────────────────────┐
│                        RshipExec                             │
│  (Existing plugin - WebSocket, Myko entities, rate limiter)  │
└─────────────────────────────────────────────────────────────┘
```

---

### 2.2 Core Data Model

#### Speaker & Topology

```cpp
// Core speaker definition
USTRUCT(BlueprintType)
struct FSpatialSpeaker
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;                           // Unique identifier

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;                       // Display name

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldPosition;              // 3D position in Unreal units

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Orientation;               // Speaker facing direction

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpeakerType Type;                  // PointSource, LineArray, Sub, Fill, etc.

    // Physical characteristics
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NominalDispersionH;           // Horizontal dispersion (degrees)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NominalDispersionV;           // Vertical dispersion (degrees)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSPL;                       // Maximum SPL capability

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MakeModel;                  // "L-Acoustics K2", "d&b V8", etc.

    // DSP state (per-speaker processing)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSpeakerDSPState DSP;               // EQ, delay, gain, limit

    // Routing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 OutputChannel;                // Physical output channel (0-based)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ParentArrayId;                // Parent array (if part of array)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ZoneId;                       // Zone membership
};

// Speaker array (grouped speakers with shared characteristics)
USTRUCT(BlueprintType)
struct FSpatialSpeakerArray
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> SpeakerIds;           // Ordered list of speakers

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESpeakerArrayType ArrayType;        // Line, Column, Cluster, Point

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ArrayPosition;              // Array reference point

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator ArrayOrientation;          // Array reference orientation

    // Array-level processing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArrayGain;                    // Master trim for array

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bMuted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSoloed;
};

// Zone (rendering region with assigned renderer)
USTRUCT(BlueprintType)
struct FSpatialZone
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> ArrayIds;             // Arrays in this zone

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> SpeakerIds;           // Direct speakers (not in arrays)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ActiveRendererId;             // Which renderer handles this zone

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FBox BoundingBox;                   // Spatial extent of zone

    // Zone-level processing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ZoneGain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bMuted;
};

// Venue (top-level container)
USTRUCT(BlueprintType)
struct FSpatialVenue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FGuid, FSpatialZone> Zones;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FGuid, FSpatialSpeakerArray> Arrays;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FGuid, FSpatialSpeaker> Speakers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector VenueOrigin;                // World offset

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VenueScale;                   // Units conversion (cm per meter)
};
```

#### Audio Objects

```cpp
USTRUCT(BlueprintType)
struct FSpatialAudioObject
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color;                 // For visualization

    // Spatial state
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position;                   // World position

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Spread;                       // Source width (0-180 degrees)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Size;                         // Source size for DBAP

    // Level and routing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Gain;                         // Object level (linear)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> ZoneRouting;          // Which zones receive this object

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DirectivityAngle;             // Directivity pattern (degrees)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator DirectivityOrientation;    // Directivity facing

    // Source binding
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TWeakObjectPtr<UAudioComponent> BoundAudioComponent;  // Optional UE audio source

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExternalInputChannel;         // Or external audio input
};

// Computed panning result per speaker
USTRUCT()
struct FSpeakerGains
{
    GENERATED_BODY()

    UPROPERTY()
    float Gain;                         // Linear amplitude (0-1+)

    UPROPERTY()
    float DelayMs;                      // Phase alignment delay

    UPROPERTY()
    float PhaseShift;                   // Additional phase (radians)
};
```

#### DSP State

```cpp
USTRUCT(BlueprintType)
struct FSpeakerDSPState
{
    GENERATED_BODY()

    // Gain staging
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InputGain;                    // Pre-DSP trim (dB)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OutputGain;                   // Post-DSP trim (dB)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bPolarity;                     // Phase invert

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bMuted;

    // Delay
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DelayMs;                      // Alignment delay (0-1000ms)

    // EQ
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FParametricEQBand> EQBands;  // Up to 16 bands

    // Dynamics
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLimiterSettings Limiter;

    // Filters
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FHighPassFilter HPF;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLowPassFilter LPF;
};

USTRUCT(BlueprintType)
struct FParametricEQBand
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EEQBandType Type;                   // Peak, LowShelf, HighShelf, Notch

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FrequencyHz;                  // Center frequency

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GainDb;                       // Boost/cut

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Q;                            // Bandwidth
};

USTRUCT(BlueprintType)
struct FLimiterSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThresholdDb;                  // Limiting threshold

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackMs;                     // Attack time

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReleaseMs;                    // Release time

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float KneeDb;                       // Soft knee width
};
```

#### Routing

```cpp
// Bus hierarchy
USTRUCT(BlueprintType)
struct FSpatialBus
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBusType Type;                      // Object, Zone, Master, Aux

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> InputSourceIds;       // Objects or other buses

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> OutputDestinationIds; // Buses or output channels

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Gain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bMuted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSoloed;

    // Bus processing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBusInsert> Inserts;         // DSP inserts on this bus
};

// Routing matrix entry
USTRUCT()
struct FRoutingMatrixEntry
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid SourceId;                     // Object, bus, or input

    UPROPERTY()
    FGuid DestinationId;                // Bus or output

    UPROPERTY()
    float Gain;                         // Send level

    UPROPERTY()
    bool bEnabled;
};
```

---

### 2.3 Rendering Pathway

#### Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        AUDIO SOURCES                                     │
├─────────────────────────────────────────────────────────────────────────┤
│  UE AudioComponent  │  External Input  │  Sequencer Timeline           │
└──────────┬──────────┴────────┬─────────┴─────────┬──────────────────────┘
           │                   │                   │
           ▼                   ▼                   ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                      AUDIO OBJECT MANAGER                                │
│  - Object state (position, spread, routing)                              │
│  - Object audio buffers (per-object mono/stereo)                         │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
           ┌───────────────────┼───────────────────┐
           ▼                   ▼                   ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│  ZONE RENDERER  │  │  ZONE RENDERER  │  │  ZONE RENDERER  │
│  (Main - VBAP)  │  │ (Surr - DBAP)   │  │  (Over - HOA)   │
│                 │  │                 │  │                 │
│ Computes gains  │  │ Computes gains  │  │ Computes gains  │
│ + delays per    │  │ + delays per    │  │ + delays per    │
│ speaker         │  │ speaker         │  │ speaker         │
└────────┬────────┘  └────────┬────────┘  └────────┬────────┘
         │                    │                    │
         └────────────────────┼────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                      MIXING ENGINE                                       │
│  - Sum object contributions per output channel                           │
│  - Apply per-channel gains and delays from renderers                     │
└──────────────────────────────┬──────────────────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                      BUS STRUCTURE                                       │
│  Zone Buses → Master Bus → Output Bus                                    │
│  (with aux sends/returns)                                                │
└──────────────────────────────┬──────────────────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                  SPEAKER MANAGEMENT DSP                                  │
│  Per-output: Input Gain → EQ → Delay → Limiter → Output Gain            │
└──────────────────────────────┬──────────────────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                      IO ABSTRACTION LAYER                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌────────────────────┐ │
│  │ Marian/ASIO │ │ CoreAudio   │ │ MADI        │ │ Controller-Only    │ │
│  │ Dante       │ │             │ │             │ │ (metadata to ext)  │ │
│  └─────────────┘ └─────────────┘ └─────────────┘ └────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

#### Threading Model

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         GAME THREAD                                      │
│  - Object position updates (from Actors, Sequencer, rShip)               │
│  - Configuration changes (via rShip Actions)                             │
│  - UI updates, editor operations                                         │
│  - Queues parameter changes to audio thread                              │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │ Lock-free queue (SPSC/MPSC)
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        AUDIO THREAD                                      │
│  - Runs at audio buffer rate (e.g., 512 samples @ 48kHz = ~10.6ms)       │
│  - Consumes parameter updates from queue                                 │
│  - Executes rendering + DSP pipeline                                     │
│  - Pushes audio to IO layer                                              │
│  - NO allocations, NO locks, NO blocking                                 │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │ Direct buffer submission
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                      IO DRIVER THREAD                                    │
│  - Managed by ASIO/CoreAudio driver                                      │
│  - Callback-based buffer exchange                                        │
└─────────────────────────────────────────────────────────────────────────┘
```

---

### 2.4 Renderer Strategies

#### VBAP (Vector Base Amplitude Panning)

**Algorithm**:
1. Triangulate speaker positions (Delaunay triangulation in 2D/3D)
2. For each audio object, find enclosing triangle/tetrahedron
3. Compute barycentric coordinates → speaker gains
4. **Phase-coherent extension**: Add propagation delay per speaker based on distance

**Pros**:
- Well-understood, efficient algorithm
- Natural for concert/theater arrays
- Explicit speaker selection (no ghost images from distant speakers)

**Cons**:
- Requires triangulation preprocessing (update on speaker layout change)
- Collinear speakers need special handling

**Phase-Coherent VBAP Enhancement**:
```cpp
// For each speaker in active triangle:
for (Speaker& spk : activeTriangle)
{
    float barycentricGain = ComputeBarycentric(objectPos, triangle, spk);
    float distance = FVector::Dist(objectPos, spk.Position);
    float delayMs = distance / SpeedOfSound * 1000.0f;  // ~2.94ms per meter

    result.Add({spk.Id, barycentricGain, delayMs});
}
```

#### DBAP (Distance-Based Amplitude Panning)

**Algorithm**:
1. For each speaker, compute inverse-distance weight: `w = 1 / (distance^rolloff)`
2. Normalize weights so they sum to 1
3. Apply spatial blur based on object `Spread` parameter
4. **Phase-coherent extension**: Delay based on distance from virtual source

**Pros**:
- No triangulation needed
- Graceful degradation (any speaker count)
- Natural for installations with irregular layouts

**Cons**:
- All speakers get some signal (may need distance cutoff)
- Less precise imaging than VBAP

```cpp
// DBAP with phase coherence
for (Speaker& spk : allSpeakers)
{
    float distance = FVector::Dist(objectPos, spk.Position);
    float weight = 1.0f / FMath::Pow(FMath::Max(distance, 0.1f), RolloffExponent);
    float delayMs = distance / SpeedOfSound * 1000.0f;

    gains.Add({spk.Id, weight, delayMs});
}
NormalizeGains(gains);
```

#### HOA (Higher-Order Ambisonics)

**Algorithm**:
1. Encode audio object into spherical harmonic representation (order 1-7)
2. Apply rotation/processing in ambisonic domain
3. Decode to speaker layout using AllRAD, VBAP-based, or mode-matching decoder

**Pros**:
- Format-agnostic (same encoding for any speaker layout)
- Excellent for dense speaker arrays (immersive domes)
- Supports height and 3D periphonic coverage

**Cons**:
- Higher computational cost
- Sweet spot limitations
- Requires decoder design per layout

#### Recommended Approach

**Hybrid Zone-Based Rendering**:
- Each zone has an assigned renderer type
- Main PA / line arrays: VBAP with phase coherence
- Surrounds / distributed speakers: DBAP
- Overhead / immersive: HOA
- Renderer selection stored per-zone, hot-swappable at runtime

---

### 2.5 DSP Architecture

#### DSP Chain per Output Channel

```
Input (from mixing engine)
    │
    ▼
┌─────────────┐
│ Input Gain  │  ← Trim before processing
└─────┬───────┘
      ▼
┌─────────────┐
│ Polarity    │  ← Phase invert (multiply by -1)
└─────┬───────┘
      ▼
┌─────────────┐
│ High Pass   │  ← Butterworth/Linkwitz-Riley (12-48dB/oct)
└─────┬───────┘
      ▼
┌─────────────┐
│ Parametric  │  ← Up to 16 bands, IIR biquads
│ EQ          │     (Peak, Shelf, Notch, AllPass)
└─────┬───────┘
      ▼
┌─────────────┐
│ Low Pass    │  ← Crossover for subs
└─────┬───────┘
      ▼
┌─────────────┐
│ Delay       │  ← Sample-accurate, up to 48000 samples (1s @ 48kHz)
└─────┬───────┘     Uses ring buffer for efficiency
      ▼
┌─────────────┐
│ Limiter     │  ← Feedforward, lookahead optional
└─────┬───────┘
      ▼
┌─────────────┐
│ Output Gain │  ← Final trim
└─────┬───────┘
      ▼
Output (to IO layer)
```

#### Real-Time Safe DSP Implementation

```cpp
// All DSP state pre-allocated, no runtime allocation
class FSpeakerDSPProcessor
{
    // Biquad coefficients updated from game thread via atomic swap
    alignas(64) FBiquadCoefficients EQCoeffs[16];  // Cache-line aligned

    // Delay line - fixed allocation
    TArray<float> DelayBuffer;  // Pre-allocated to max delay
    int32 DelayWriteIndex;
    std::atomic<int32> DelaySamples;  // Atomic read from game thread

    // State variables for IIR filters
    float EQState[16][4];  // z^-1, z^-2 per band, per channel

public:
    // Called from audio thread - must be lock-free
    void Process(float* Buffer, int32 NumSamples)
    {
        // Read parameters atomically
        int32 CurrentDelay = DelaySamples.load(std::memory_order_relaxed);

        for (int32 i = 0; i < NumSamples; ++i)
        {
            float sample = Buffer[i] * InputGainLinear;

            // Polarity
            if (bPolarityInvert) sample = -sample;

            // HPF, EQ, LPF (biquad cascades)
            sample = ProcessFilters(sample);

            // Delay
            sample = ProcessDelay(sample, CurrentDelay);

            // Limiter
            sample = ProcessLimiter(sample);

            // Output gain
            Buffer[i] = sample * OutputGainLinear;
        }
    }
};
```

---

### 2.6 rShip/Myko Integration

#### Integration with Existing RshipExec

The spatial audio system extends RshipSubsystem with a new manager:

```cpp
// In RshipSubsystem.h (existing file, add new manager)
UPROPERTY()
URshipSpatialAudioManager* SpatialAudioManager;

UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
URshipSpatialAudioManager* GetSpatialAudioManager();
```

#### Manager Structure

```cpp
UCLASS(BlueprintType)
class RSHIPSPATIALAUDIORUNTIME_API URshipSpatialAudioManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    // ========================================================================
    // VENUE MANAGEMENT
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void CreateVenue(const FString& VenueName);

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    FSpatialVenue& GetVenue();

    // ========================================================================
    // SPEAKER MANAGEMENT
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    FGuid AddSpeaker(const FSpatialSpeaker& Speaker);

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void UpdateSpeaker(const FGuid& SpeakerId, const FSpatialSpeaker& Speaker);

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void RemoveSpeaker(const FGuid& SpeakerId);

    // ========================================================================
    // AUDIO OBJECTS
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    FGuid CreateAudioObject(const FString& Name);

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void SetObjectPosition(const FGuid& ObjectId, FVector Position);

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void SetObjectSpread(const FGuid& ObjectId, float Spread);

    // ========================================================================
    // RENDERING
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void SetZoneRenderer(const FGuid& ZoneId, ESpatialRendererType RendererType);

    // ========================================================================
    // DSP CONTROL
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void SetSpeakerGain(const FGuid& SpeakerId, float GainDb);

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void SetSpeakerDelay(const FGuid& SpeakerId, float DelayMs);

    UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio")
    void SetSpeakerEQ(const FGuid& SpeakerId, const TArray<FParametricEQBand>& Bands);

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Core state
    FSpatialVenue Venue;
    TMap<FGuid, FSpatialAudioObject> AudioObjects;

    // Audio engine
    TUniquePtr<FSpatialAudioEngine> AudioEngine;

    // Myko registration
    void RegisterMykoTargets();
    void SendMeters();
    void ProcessIncomingActions(const FString& ActionId, const TSharedRef<FJsonObject>& Data);
};
```

#### Myko Target Registration

```cpp
void URshipSpatialAudioManager::RegisterMykoTargets()
{
    // Register venue as Instance
    TSharedPtr<FJsonObject> VenueData = MakeShareable(new FJsonObject);
    VenueData->SetStringField("id", Venue.Id.ToString());
    VenueData->SetStringField("name", Venue.Name);
    VenueData->SetStringField("serviceTypeCode", "spatial-audio");
    Subsystem->SetItem("Instance", VenueData, ERshipMessagePriority::High);

    // Register each zone as Target
    for (auto& ZonePair : Venue.Zones)
    {
        FSpatialZone& Zone = ZonePair.Value;
        RegisterZoneTarget(Zone);
    }

    // Register scene manager target
    RegisterSceneManagerTarget();
}
```

#### Real-Time Metering (60Hz)

```cpp
void URshipSpatialAudioManager::SendMeters()
{
    // Called from Tick at 60Hz

    for (auto& ZonePair : Venue.Zones)
    {
        FSpatialZone& Zone = ZonePair.Value;

        TSharedPtr<FJsonObject> MeterData = MakeShareable(new FJsonObject);
        TArray<TSharedPtr<FJsonValue>> SpeakerMeters;

        for (const FGuid& SpeakerId : Zone.SpeakerIds)
        {
            FSpeakerMeterReading Reading = AudioEngine->GetMeterReading(SpeakerId);

            TSharedPtr<FJsonObject> SpkMeter = MakeShareable(new FJsonObject);
            SpkMeter->SetStringField("id", SpeakerId.ToString());
            SpkMeter->SetNumberField("rms", Reading.RMS);
            SpkMeter->SetNumberField("peak", Reading.Peak);
            SpkMeter->SetBoolField("limiting", Reading.bLimiting);

            SpeakerMeters.Add(MakeShareable(new FJsonValueObject(SpkMeter)));
        }

        MeterData->SetArrayField("speakers", SpeakerMeters);
        MeterData->SetNumberField("timestamp", FPlatformTime::Seconds());

        FString EmitterId = FString::Printf(TEXT("%s:meters"), *Zone.Id.ToString());
        Subsystem->PulseEmitter(Zone.Id.ToString(), "meters", MeterData);
    }
}
```

---

### 2.7 Editor Experience

#### Proposed Editor Panels

##### Speaker Layout Editor

**3D Viewport Mode**:
- Speakers rendered as 3D cone gizmos showing dispersion
- Drag to position, rotate with handles
- Multi-select for bulk operations
- Speaker arrays rendered as connected lines

**List View Mode**:
- Spreadsheet-style editor for precise coordinate entry
- Columns: Name, X, Y, Z, Type, Channel, Zone, Delay, Gain
- CSV import/export

##### Routing Matrix Panel

```
           │ Zone_Main │ Zone_Surr │ Zone_Over │ Master │
───────────┼───────────┼───────────┼───────────┼────────┤
Object_001 │    [X]    │    [ ]    │    [ ]    │        │
Object_002 │    [X]    │    [X]    │    [ ]    │        │
Object_003 │    [ ]    │    [ ]    │    [X]    │        │
───────────┼───────────┼───────────┼───────────┼────────┤
Zone_Main  │           │           │           │  [X]   │
Zone_Surr  │           │           │           │  [X]   │
Zone_Over  │           │           │           │  [X]   │
```

##### DSP Channel Strip Panel

```
┌─────────────────────────────────────┐
│  Speaker: Main_L01                  │
├─────────────────────────────────────┤
│  Input Gain: [-12.0 dB] ═══════○    │
│  Polarity:   [Normal ▼]             │
├─────────────────────────────────────┤
│  EQ                          [+Band]│
│  ┌────────────────────────────────┐ │
│  │  ~~~~~╱╲~~~~~╲/~~~~~~          │ │ ← EQ curve visualization
│  └────────────────────────────────┘ │
│  Band 1: Peak  1000Hz  +3dB  Q2.0   │
│  Band 2: HShelf 8kHz  -2dB          │
├─────────────────────────────────────┤
│  Delay:  [  3.45 ms ] ════○         │
├─────────────────────────────────────┤
│  Limiter                    [Enable]│
│  Threshold: [-6.0 dB]               │
│  Attack:    [0.1 ms]                │
│  Release:   [50 ms]                 │
├─────────────────────────────────────┤
│  Output Gain: [0.0 dB] ════════○    │
│                                     │
│  ▮▮▮▮▮▮▮▮░░░░  -18dB    LIMIT: ○    │
└─────────────────────────────────────┘
```

##### Live Monitoring Panel

- Per-output level meters (60Hz update)
- Limiting indicators
- Object position radar view
- Active renderer status per zone

---

### Architecture Decision Summary

| Decision | Recommended Choice | Rationale |
|----------|-------------------|-----------|
| Plugin structure | Separate plugin depending on RshipExec | Clean separation, optional inclusion |
| Module split | Runtime + Editor | Standard UE pattern, keeps editor code out of packaged builds |
| Primary renderer | VBAP + DBAP with phase coherence | Best balance of precision and flexibility |
| Threading | Lock-free SPSC queues | Required for <10ms latency |
| DSP implementation | Biquad IIR filters | Efficient, well-understood, real-time safe |
| IO abstraction | Interface-based with driver plugins | Vendor-agnostic, extensible |
| Myko mapping | Hierarchical (Venue → Zone → Speaker) | Matches physical reality, scalable |
| Metering transport | Existing Pulse mechanism at 60Hz | Reuses proven infrastructure |

---

## 2.8 Implementation Status (M1-M10)

The following components are fully implemented and functional:

### Completed Components

#### Core Data Model (M1)
- `FSpatialSpeaker`, `FSpatialZone`, `FSpatialVenue`, `FSpatialAudioObject`
- DSP types: `FSpatialSpeakerDSPState`, `FSpatialEQBand`, `FSpatialLimiterSettings`
- Plugin scaffold with `RshipSpatialAudioRuntime` and `RshipSpatialAudioEditor` modules

#### VBAP/DBAP Rendering (M2, M4)
- `FSpatialRendererVBAP` with Delaunay triangulation
- `FSpatialRendererDBAP` with distance-based rolloff
- Phase-coherent panning (gain + delay per speaker)
- Zone system with per-zone renderer assignment

#### Audio Engine (M3)
- `FSpatialAudioProcessor` - Lock-free real-time audio processing
- `FSpatialRenderingEngine` - Coordinates rendering + processing
- SPSC lock-free queues for game→audio thread communication
- Per-object gain smoothing (32-sample ramp)

#### DSP Chain (M5)
- `FSpatialSpeakerDSP` - Per-output DSP processor
- Parametric EQ (up to 16 bands, IIR biquads)
- Sample-accurate delay (0-1000ms)
- Lookahead limiter with attack/release
- High-pass/low-pass crossover filters

#### rShip/Myko Integration (M6)
- Full Myko entity registration (Instance, Targets, Actions, Emitters)
- 60Hz meter pulses
- Scene store/recall via rShip Actions
- Real-time object positioning via Actions

#### Editor Tools (M7)
- Component visualizers for speakers, objects, zones
- `USpatialAudioVisualizerComponent` for editor viewport rendering
- Speaker coverage cone visualization
- Menu integration (Window → Rocketship → Spatial Audio Manager)

#### Real-Time Pipeline (M8)
- `FSpatialRenderingEngine` integration with manager
- Smooth scene interpolation with cubic easing
- `USpatialAudioEngineComponent` for easy setup
- `USpatialAudioSubmixEffect` for Unreal audio integration

#### SMAART Import & Calibration (M9)
- `FSMAARTImporter` - Multi-format measurement file parser
- `FSMAARTMeasurement` - Transfer function data with interpolation
- `FCalibrationPresetManager` - Auto-EQ generation from measurements
- `FSpeakerCalibrationPreset` - Per-speaker calibration storage
- `FVenueCalibrationSet` - Complete venue calibration with normalization

#### HOA/Ambisonics Renderer (M10)
- `FAmbisonicsEncoder` - Spherical harmonic encoding (1st-5th order)
- `FAmbisonicsDecoder` - AllRAD, MaxRE, InPhase decode matrices
- `FSpatialRendererHOA` - Full ISpatialRenderer implementation
- Multiple normalization schemes (SN3D, N3D, FuMa)
- Spread handling via order reduction

### M8 Implementation Details

#### Rendering Engine Integration

The manager connects to the rendering engine which owns the audio processor:

```cpp
// Connect rendering engine to manager
Manager->SetRenderingEngine(RenderingEngine.Get());

// Object position changes flow through rendering engine
void NotifyObjectChange(const FGuid& ObjectId)
{
    if (RenderingEngine)
    {
        // Computes VBAP/DBAP gains and queues to processor
        RenderingEngine->UpdateObject(*Object);
    }
}
```

**Key Files:**
- [RshipSpatialAudioManager.h:421-458](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Public/RshipSpatialAudioManager.h#L421-L458) - Rendering engine API
- [RshipSpatialAudioManager.cpp:1737-1841](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Private/RshipSpatialAudioManager.cpp#L1737-L1841) - Integration implementation

#### Scene Interpolation

Smooth transitions between scenes with configurable duration:

```cpp
// Recall scene with 2-second interpolation
Manager->RecallScene("scene_id", true, 2000.0f);
```

**Interpolation Features:**
- Cubic ease in-out for professional feel
- Gain interpolation in dB space for perceptual linearity
- Mute state snapping (unmute at 5%, mute at 95%)
- Per-speaker: InputGain, OutputGain, Delay, Mute
- Per-object: Position, Spread, Gain, Mute

**Key Files:**
- [RshipSpatialAudioManager.h:455-541](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Public/RshipSpatialAudioManager.h#L455-L541) - Interpolation state structures
- [RshipSpatialAudioManager.cpp:1857-1975](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Private/RshipSpatialAudioManager.cpp#L1857-L1975) - UpdateSceneInterpolation()

#### SpatialAudioEngineComponent

Blueprint-friendly component for easy setup:

```cpp
// Add to actor in level
UPROPERTY(EditAnywhere)
int32 OutputChannelCount = 64;

UPROPERTY(EditAnywhere)
ESpatialRendererType DefaultRendererType = ESpatialRendererType::VBAP;

// Auto-connects to SpatialAudioManager and SubmixEffect
```

**Key Files:**
- [SpatialAudioEngineComponent.h](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Public/Components/SpatialAudioEngineComponent.h)
- [SpatialAudioEngineComponent.cpp](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Private/Components/SpatialAudioEngineComponent.cpp)

### Architecture Flow (Implemented)

```
Game Thread                          Audio Thread
─────────────────────────────────────────────────────
URshipSpatialAudioManager
  │ SetObjectPosition()
  ▼
FSpatialRenderingEngine
  │ UpdateObject()
  │ ComputeVBAPGains() or
  │ ComputeDBAPGains()
  ▼
FSpatialAudioProcessor
  │ QueueGainUpdate()
  ├─────────────────────────────→    ProcessCommands()
                                       │ Apply smoothed gains
                                       ▼
                                     FSpatialSpeakerDSP
                                       │ Input Gain
                                       │ Polarity
                                       │ HPF → EQ → LPF
                                       │ Delay
                                       │ Limiter
                                       │ Output Gain
                                       ▼
                                     FSpatialAudioSubmixEffect
                                       │ Interleaved output
                                       ▼
                                     Unreal Audio Output
```

### Lock-Free Communication

All game→audio thread communication uses lock-free SPSC queues:

| Queue | Purpose | Producer | Consumer |
|-------|---------|----------|----------|
| CommandQueue | Parameter changes | Game Thread | Audio Thread |
| FeedbackQueue | Meter readings | Audio Thread | Game Thread |
| GainUpdateQueue | Per-speaker gains | Rendering Engine | Audio Processor |
| DSPConfigQueue | DSP settings | Manager | DSP Processor |

### API Summary

**URshipSpatialAudioManager:**
- `SetRenderingEngine(FSpatialRenderingEngine*)` - Connect rendering pipeline
- `SetGlobalRendererType(ESpatialRendererType)` - VBAP/DBAP/HOA/Stereo/Direct
- `SetListenerPosition(FVector)` - Reference point for rendering
- `RecallScene(SceneId, bInterpolate, InterpolateTimeMs)` - Scene recall with optional smooth transition
- `StoreScene(Name)` - Capture current state

**USpatialAudioEngineComponent:**
- `InitializeEngine()` - Start rendering pipeline
- `ShutdownEngine()` - Stop rendering pipeline
- `SetRendererType(ESpatialRendererType)` - Change algorithm
- `SetListenerPosition(FVector)` - Update reference
- `SetMasterGain(float GainDb)` - Master output level

### M9 Implementation Details

#### SMAART Import System

Multi-format measurement file importer supporting SMAART, REW, and generic CSV:

```cpp
// Import SMAART measurement files
FSMAARTImporter Importer;
FSMAARTImportResult Result = Importer.ImportFromFiles({
    "speaker_01_TF.txt",
    "speaker_02_TF.txt"
});

if (Result.bSuccess)
{
    for (const FSMAARTMeasurement& Measurement : Result.Measurements)
    {
        // Access frequency/magnitude/phase/coherence data
        float MagAt1k = Measurement.GetMagnitudeAtFrequency(1000.0f);
    }
}
```

**Supported Formats:**
- SMAART 7/8 Transfer Function exports (TXT/CSV)
- Room EQ Wizard (REW) measurement files
- Generic frequency/magnitude/phase CSV

**Key Files:**
- [SSMAARTImporter.h](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioEditor/Public/Calibration/SSMAARTImporter.h)
- [SSMAARTImporter.cpp](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioEditor/Private/Calibration/SSMAARTImporter.cpp)

#### Auto-EQ Generation

Automatic parametric EQ generation from measurement data:

```cpp
FCalibrationPresetManager CalManager;

// Generate auto-EQ with custom settings
FAutoEQSettings Settings;
Settings.MaxBands = 8;
Settings.MaxGainDb = 12.0f;
Settings.CoherenceThreshold = 0.6f;
Settings.bPreferCuts = true;  // Better headroom

FAutoEQResult Result = CalManager.GenerateAutoEQ(Measurement, Settings);

// Apply generated EQ to speaker
if (Result.bSuccess)
{
    for (const FSpatialEQBand& Band : Result.EQBands)
    {
        // Band.FrequencyHz, Band.GainDb, Band.Q
    }
}
```

**Auto-EQ Features:**
- Smoothed measurement analysis (configurable octave smoothing)
- Peak/dip detection with coherence weighting
- Q value optimization for minimum deviation
- Cut preference mode for better headroom
- High-pass filter suggestion based on speaker capability

#### Calibration Preset System

Complete calibration workflow from measurement to speaker:

```cpp
// Create preset from measurement
FSpeakerCalibrationPreset Preset = CalManager.CreatePreset(
    Measurement,
    SpeakerId,
    "Main L",
    AutoEQSettings
);

// Preset contains:
// - SuggestedDelayMs (from measurement)
// - SuggestedGainDb (normalized)
// - GeneratedEQBands (auto-EQ)
// - SuggestedHighPass/LowPass

// Apply to speaker via manager
CalManager.ApplyPresetToSpeaker(SpatialAudioManager, SpeakerId, Preset);
```

#### Venue Calibration Sets

Manage calibrations for entire venues:

```cpp
// Create venue calibration set
FVenueCalibrationSet& CalSet = CalManager.GetOrCreateVenueCalibrationSet("Madison Square Garden");

// Add speaker calibrations
CalSet.SetSpeakerPreset(Speaker1Id, Preset1);
CalSet.SetSpeakerPreset(Speaker2Id, Preset2);

// Set reference speaker for delay normalization
CalSet.ReferenceDelaySpeakerId = Speaker1Id;

// Normalize all delays relative to reference
CalSet.NormalizeDelays();

// Normalize gains to common level
CalSet.NormalizeGains();

// Save to file
CalManager.SaveCalibrationSet("Madison Square Garden", "/calibrations/msg.rcal");

// Apply entire venue calibration
CalManager.ApplyVenueCalibration(SpatialAudioManager, CalSet);
```

**Key Files:**
- [SpatialCalibrationTypes.h](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioEditor/Public/Calibration/SpatialCalibrationTypes.h)
- [SCalibrationPresetManager.h](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioEditor/Public/Calibration/SCalibrationPresetManager.h)
- [SCalibrationPresetManager.cpp](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioEditor/Private/Calibration/SCalibrationPresetManager.cpp)

#### Manager Integration

New DSP API for calibration application:

```cpp
// Apply complete DSP state from calibration
Manager->SetSpeakerDSP(SpeakerId, DSPState);

// Or apply individual components
Manager->SetSpeakerHighPass(SpeakerId, HighPass);
Manager->SetSpeakerLowPass(SpeakerId, LowPass);
```

### M10 Implementation Details

#### HOA/Ambisonics Renderer

Higher-Order Ambisonics renderer for immersive audio installations:

```cpp
// Configure HOA renderer
FSpatialRendererHOA* HOARenderer = static_cast<FSpatialRendererHOA*>(
    Registry.GetOrCreateRenderer(ESpatialRendererType::HOA, Speakers));

// Set Ambisonics order (1-5)
HOARenderer->SetOrder(EAmbisonicsOrder::Third);  // 16 channels

// Set decoder type
HOARenderer->SetDecoderType(EAmbisonicsDecoderType::AllRAD);

// Set listener position
HOARenderer->SetListenerPosition(FVector::ZeroVector);
```

**Ambisonics Orders:**
| Order | Channels | Best For |
|-------|----------|----------|
| 1st | 4 | Basic surround, VR |
| 2nd | 9 | Room-filling ambience |
| 3rd | 16 | Precise localization |
| 4th | 25 | High-resolution domes |
| 5th | 36 | Research/premium installations |

**Decoder Types:**
- **Basic**: Simple sampling decode (fast, less accurate)
- **MaxRE**: Maximum energy vector (better HF localization)
- **InPhase**: Reduced side lobes (smooth, less precise)
- **AllRAD**: Optimal pseudoinverse (recommended)
- **EPAD**: Energy-preserving (consistent loudness)

#### Spherical Harmonic Encoding

The encoder converts 3D positions to Ambisonics coefficients:

```cpp
FAmbisonicsEncoder Encoder;
Encoder.SetOrder(EAmbisonicsOrder::Third);
Encoder.SetNormalization(EAmbisonicsNormalization::SN3D);  // AmbiX standard

TArray<float> Coefficients;
float Distance;
Encoder.EncodePosition(ObjectPosition, ListenerPosition, Coefficients, Distance);
```

**Normalization Schemes:**
- **SN3D**: Schmidt semi-normalized (AmbiX standard, recommended)
- **N3D**: Full 3D normalization
- **FuMa**: Furse-Malham (legacy B-format)
- **MaxN**: Max-normalized

#### Spread Handling

Source width (spread) is implemented via order reduction:

```cpp
// Point source (no spread)
ComputeGains(Position, 0.0f, OutGains);

// Wide source (180° = omnidirectional)
ComputeGains(Position, 90.0f, OutGains);  // 90° spread
```

Higher spread progressively attenuates higher-order harmonics, creating a more diffuse sound.

**Key Files:**
- [SpatialRendererHOA.h](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Public/Rendering/SpatialRendererHOA.h)
- [SpatialRendererHOA.cpp](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Private/Rendering/SpatialRendererHOA.cpp)

---

## 3. Implementation Roadmap

### 3.1 Milestone Overview

| # | Milestone | Complexity | Status | L-ISA/Soundscape Parity |
|---|-----------|------------|--------|-------------------------|
| 1 | Plugin scaffold + core data models | M | ✅ Complete | 5% - Foundation only |
| 2 | Basic VBAP renderer + phase coherence | L | ✅ Complete | 15% - Basic panning works |
| 3 | Audio engine + IO abstraction | L | ✅ Complete | 25% - Audio flows |
| 4 | DBAP renderer + zone system | M | ✅ Complete | 35% - Multi-renderer |
| 5 | Speaker DSP chain | L | ✅ Complete | 50% - EQ/delay/limit |
| 6 | rShip/Myko binding layer | M | ✅ Complete | 60% - External control |
| 7 | Editor: Component visualizers + viz | M | ✅ Complete | 70% - Visual editing |
| 8 | Real-time pipeline + scene interpolation | M | ✅ Complete | 80% - Full pipeline |
| 9 | SMAART import + calibration | S | ✅ Complete | 85% - Pro workflow |
| 10 | HOA renderer | M | ✅ Complete | 90% - Immersive |
| 11 | External processor executor (DS100) | M | ✅ Complete | 95% - Hardware integration |
| 12 | Polish, optimization, documentation | M | ✅ Complete | 100% - Production ready |

---

### 3.2 Detailed Milestones

#### Milestone 1: Plugin Scaffold + Core Data Models

**Description**: Create the plugin structure, Build.cs files, and all core data types (speakers, zones, arrays, objects, DSP state). No audio processing yet.

**Complexity**: Medium

**Dependencies**: Existing RshipExec plugin

**Files to Create**:
```
Plugins/RshipSpatialAudio/
├── RshipSpatialAudio.uplugin
├── Source/
│   ├── RshipSpatialAudioRuntime/
│   │   ├── RshipSpatialAudioRuntime.Build.cs
│   │   ├── Public/
│   │   │   ├── RshipSpatialAudioModule.h
│   │   │   ├── Core/
│   │   │   │   ├── SpatialAudioTypes.h         # All structs
│   │   │   │   ├── SpatialSpeaker.h
│   │   │   │   ├── SpatialZone.h
│   │   │   │   ├── SpatialVenue.h
│   │   │   │   ├── SpatialAudioObject.h
│   │   │   │   └── SpatialDSPTypes.h
│   │   │   └── RshipSpatialAudioManager.h
│   │   └── Private/
│   │       ├── RshipSpatialAudioModule.cpp
│   │       └── RshipSpatialAudioManager.cpp
│   │
│   └── RshipSpatialAudioEditor/
│       ├── RshipSpatialAudioEditor.Build.cs
│       ├── Public/
│       │   └── RshipSpatialAudioEditorModule.h
│       └── Private/
│           └── RshipSpatialAudioEditorModule.cpp
```

**rShip/Myko Surfaces**: None yet (data models only)

**Deliverables**:
- Compiling plugin with runtime + editor modules
- All core structs with UPROPERTY/USTRUCT for Blueprint exposure
- Manager class skeleton integrated into RshipSubsystem

---

#### Milestone 2: Basic VBAP Renderer + Phase Coherence

**Description**: Implement VBAP algorithm with Delaunay triangulation and phase-coherent gain/delay computation. CPU-only, no audio yet.

**Complexity**: Large

**Dependencies**: M1

**Files to Create/Modify**:
```
Source/RshipSpatialAudioRuntime/
├── Public/
│   └── Rendering/
│       ├── ISpatialRenderer.h              # Renderer interface
│       ├── SpatialRendererVBAP.h           # VBAP implementation
│       ├── SpatialRendererRegistry.h       # Renderer factory
│       └── DelaunayTriangulation.h         # Triangulation helper
└── Private/
    └── Rendering/
        ├── SpatialRendererVBAP.cpp
        ├── SpatialRendererRegistry.cpp
        └── DelaunayTriangulation.cpp
```

**Key Implementation Details**:
```cpp
// Renderer interface
class ISpatialRenderer
{
public:
    virtual void Configure(const TArray<FSpatialSpeaker>& Speakers) = 0;
    virtual void ComputeGains(const FVector& ObjectPosition, float Spread,
                               TArray<FSpeakerGains>& OutGains) = 0;
    virtual ESpatialRendererType GetType() const = 0;
};
```

**Deliverables**:
- Working VBAP panner that outputs gain+delay per speaker
- Unit tests for triangulation and panning
- Performance benchmark (target: <1ms for 256 speakers, 256 objects)

---

#### Milestone 3: Audio Engine + IO Abstraction

**Description**: Create the core audio processing engine that takes object audio, applies renderer gains/delays, and outputs to abstract IO layer.

**Complexity**: Large

**Dependencies**: M1, M2

**Files to Create**:
```
Source/RshipSpatialAudioRuntime/
├── Public/
│   ├── Engine/
│   │   ├── SpatialAudioEngine.h            # Main engine
│   │   ├── AudioMixer.h                     # Mixing logic
│   │   ├── ObjectProcessor.h               # Per-object processing
│   │   └── LockFreeQueue.h                 # SPSC queue
│   └── IO/
│       ├── ISpatialAudioIO.h               # IO interface
│       ├── SpatialAudioIOASIO.h            # ASIO implementation
│       └── SpatialAudioIONull.h            # Null/test IO
└── Private/
    ├── Engine/
    │   ├── SpatialAudioEngine.cpp
    │   ├── AudioMixer.cpp
    │   └── ObjectProcessor.cpp
    └── IO/
        ├── SpatialAudioIOASIO.cpp
        └── SpatialAudioIONull.cpp
```

**Deliverables**:
- Audio engine with proper threading
- ASIO output (Windows) and CoreAudio output (Mac)
- Null IO for testing
- End-to-end audio path working (object → renderer → output)

---

#### Milestone 4: DBAP Renderer + Zone System

**Description**: Add DBAP renderer and implement zone-based rendering where each zone can use different renderers.

**Complexity**: Medium

**Dependencies**: M3

**Files to Create**:
```
Source/RshipSpatialAudioRuntime/
├── Public/
│   ├── Rendering/
│   │   └── SpatialRendererDBAP.h
│   └── Zones/
│       └── SpatialZoneManager.h
└── Private/
    ├── Rendering/
    │   └── SpatialRendererDBAP.cpp
    └── Zones/
        └── SpatialZoneManager.cpp
```

**Deliverables**:
- DBAP renderer with distance rolloff and phase coherence
- Zone management with per-zone renderer assignment
- Objects can route to multiple zones simultaneously

---

#### Milestone 5: Speaker DSP Chain

**Description**: Implement per-output DSP processing: EQ, delay, limiter, filters.

**Complexity**: Large

**Dependencies**: M3

**Files to Create**:
```
Source/RshipSpatialAudioRuntime/
├── Public/
│   └── DSP/
│       ├── SpatialDSPProcessor.h           # Per-channel processor
│       ├── BiquadFilter.h                  # IIR biquad
│       ├── DelayLine.h                     # Ring buffer delay
│       ├── Limiter.h                       # Lookahead limiter
│       └── DSPMath.h                       # dB/linear conversions
└── Private/
    └── DSP/
        ├── SpatialDSPProcessor.cpp
        ├── BiquadFilter.cpp
        ├── DelayLine.cpp
        └── Limiter.cpp
```

**Deliverables**:
- Full DSP chain operational
- Parametric EQ with smooth coefficient updates
- Sample-accurate delay (up to 1 second)
- Lookahead limiter with proper attack/release

---

#### Milestone 6: rShip/Myko Binding Layer

**Description**: Full integration with rShip - register targets, handle actions, emit pulses.

**Complexity**: Medium

**Dependencies**: M1-M5

**Files to Create/Modify**:
```
Source/RshipSpatialAudioRuntime/
├── Public/
│   └── RshipBinding/
│       ├── SpatialAudioTargetFactory.h     # Creates Myko targets
│       ├── SpatialAudioActionHandler.h     # Processes incoming actions
│       └── SpatialAudioEmitterService.h    # Meter/status pulses
└── Private/
    └── RshipBinding/
        ├── SpatialAudioTargetFactory.cpp
        ├── SpatialAudioActionHandler.cpp
        └── SpatialAudioEmitterService.cpp

# Modify existing RshipExec:
Source/RshipExec/
├── Public/
│   └── RshipSubsystem.h                    # Add GetSpatialAudioManager()
└── Private/
    └── RshipSubsystem.cpp                  # Lazy-init SpatialAudioManager
```

**rShip/Myko Surfaces Added**:

| Entity Type | Items | Actions | Emitters |
|-------------|-------|---------|----------|
| Instance | Venue | - | status |
| Target | Zone_* | setGain, setDelay, setEQ, setMute | meters, status |
| Target | Object_* | setPosition, setSpread, setRouting | position, level |
| Target | SceneManager | recallScene, storeScene | activeScene |
| Target | Renderer_* | setType, setParams | config |

**Deliverables**:
- Full Myko entity registration on startup
- All DSP/routing parameters controllable via Actions
- 60Hz meter pulses
- Scene store/recall via Actions

---

#### Milestone 7: Editor - Speaker Layout Panel

**Description**: Slate panel for viewing and editing speaker layouts in 3D viewport and list view.

**Complexity**: Medium

**Dependencies**: M1-M6

**Files to Create**:
```
Source/RshipSpatialAudioEditor/
├── Public/
│   └── Widgets/
│       ├── SSpatialSpeakerLayoutEditor.h   # Main widget
│       ├── SSpatialSpeaker3DViewport.h     # 3D view
│       ├── SSpatialSpeakerListView.h       # List/table view
│       └── SSpatialSpeakerDetails.h        # Property panel
└── Private/
    └── Widgets/
        ├── SSpatialSpeakerLayoutEditor.cpp
        ├── SSpatialSpeaker3DViewport.cpp
        ├── SSpatialSpeakerListView.cpp
        └── SSpatialSpeakerDetails.cpp
```

**Deliverables**:
- Dockable editor panel
- 3D viewport with speaker gizmos
- List view with inline editing
- Multi-select and bulk operations
- Undo/redo support

---

#### Milestone 8: Editor - Routing Matrix + DSP Panel

**Description**: Routing matrix visualization and per-channel DSP editing.

**Complexity**: Medium

**Dependencies**: M7

**Files to Create**:
```
Source/RshipSpatialAudioEditor/
├── Public/
│   └── Widgets/
│       ├── SSpatialRoutingMatrix.h         # Matrix grid
│       ├── SSpatialDSPChannelStrip.h       # Channel strip
│       ├── SSpatialEQCurveEditor.h         # EQ visualization
│       └── SSpatialMeterBank.h             # Level meters
└── Private/
    └── Widgets/
        ├── SSpatialRoutingMatrix.cpp
        ├── SSpatialDSPChannelStrip.cpp
        ├── SSpatialEQCurveEditor.cpp
        └── SSpatialMeterBank.cpp
```

**Deliverables**:
- Interactive routing matrix
- Channel strip with all DSP controls
- EQ curve visualization
- Real-time meters

---

#### Milestone 9: SMAART Import + Calibration Presets

**Description**: Import measurement data from SMAART and store as calibration presets.

**Complexity**: Small

**Dependencies**: M5

**Files to Create**:
```
Source/RshipSpatialAudioEditor/
├── Public/
│   └── Importers/
│       ├── SSMAARTImporter.h
│       └── SCalibrationPresetManager.h
└── Private/
    └── Importers/
        ├── SSMAARTImporter.cpp
        └── SCalibrationPresetManager.cpp
```

**Deliverables**:
- Parse SMAART transfer function exports
- Generate EQ curves from measurement
- Import delay/phase data
- Preset save/load/compare

---

#### Milestone 10: HOA Renderer

**Description**: Higher-Order Ambisonics encoding and decoding for immersive zones.

**Complexity**: Medium

**Dependencies**: M4

**Files to Create**:
```
Source/RshipSpatialAudioRuntime/
├── Public/
│   └── Rendering/
│       ├── SpatialRendererHOA.h
│       ├── AmbisonicEncoder.h
│       └── AmbisonicDecoder.h
└── Private/
    └── Rendering/
        ├── SpatialRendererHOA.cpp
        ├── AmbisonicEncoder.cpp
        └── AmbisonicDecoder.cpp
```

**Deliverables**:
- HOA encoding up to 3rd order
- AllRAD decoder for arbitrary layouts
- Mode-matching decoder option
- Smooth panning between zones using different renderers

---

#### Milestone 11: External Processor Executor

**Status**: ✅ COMPLETE

**Description**: Integrated external processor support for controlling d&b DS100 and other spatial audio processors via OSC.

**Complexity**: Medium

**Dependencies**: M6

**Files Created**:
```
Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/
├── Public/ExternalProcessor/
│   ├── ExternalProcessorTypes.h      # OSC types, processor enums, config structs
│   ├── IExternalSpatialProcessor.h   # Abstract processor interface
│   ├── OSCClient.h                   # UDP/OSC communication layer
│   ├── DS100Processor.h              # d&b DS100 implementation
│   └── ExternalProcessorRegistry.h   # Processor factory and registry
└── Private/ExternalProcessor/
    ├── ExternalProcessorTypes.cpp    # OSC serialization/parsing
    ├── ExternalSpatialProcessorBase.cpp  # Base implementation
    ├── OSCClient.cpp                 # UDP socket management
    ├── DS100Processor.cpp            # DS100 OSC protocol
    └── ExternalProcessorRegistry.cpp # Global processor management
```

**Implementation Highlights**:

1. **OSC Communication Layer**:
   - Complete OSC 1.0 message/bundle serialization
   - UDP socket with async receive
   - Rate limiting and message bundling
   - Thread-safe send operations

2. **DS100 Processor**:
   - Full coordinate mapping (XY/XYZ positioning)
   - Source spread control
   - En-Space reverb send
   - Delay mode control
   - Matrix input/output gain and mute
   - Mapping areas 1-4 support
   - 64 source object support

3. **Manager Integration**:
   - `ConfigureExternalProcessor()` - Setup processor connection
   - `ConnectExternalProcessor()` / `DisconnectExternalProcessor()`
   - `MapObjectToExternalProcessor()` - Link internal objects to DS100 sources
   - `SetExternalProcessorForwarding()` - Auto-forward position updates
   - Position/Spread/Gain updates forwarded when enabled

4. **Processor Registry**:
   - Factory pattern for processor creation
   - Global registry for managed processors
   - Support for multiple simultaneous processors
   - Type information and capability queries

**API Usage Example**:
```cpp
// Configure DS100 processor
FExternalProcessorConfig Config;
Config.ProcessorType = EExternalProcessorType::DS100;
Config.DisplayName = TEXT("Main DS100");
Config.Network.Host = TEXT("192.168.1.100");
Config.Network.SendPort = 50010;
Config.Network.ReceivePort = 50011;

// Initialize and connect
Manager->ConfigureExternalProcessor(Config);
Manager->ConnectExternalProcessor();

// Map audio objects to DS100 sources
Manager->MapObjectToExternalProcessor(AudioObjectGuid, 1, 1);  // Object -> Source 1, Area 1

// Enable automatic forwarding
Manager->SetExternalProcessorForwarding(true);

// Now SetObjectPosition() automatically updates DS100
Manager->SetObjectPosition(AudioObjectGuid, NewPosition);
```

**Supported Processors**:
| Processor | Status | Notes |
|-----------|--------|-------|
| d&b DS100 | ✅ Full | All coordinate mapping features |
| d&b P1 | ✅ Basic | Uses DS100 protocol |
| L-ISA | ⏳ Planned | Future implementation |
| Spacemap Go | ⏳ Planned | Future implementation |
| Custom OSC | ✅ Basic | Configurable addresses |

---

#### Milestone 12: Polish, Optimization, Documentation

**Status**: ✅ COMPLETE

**Description**: Performance optimization, edge case handling, documentation, example maps.

**Complexity**: Medium

**Dependencies**: All

**Files Created**:
```
Plugins/RshipSpatialAudio/
├── README.md                              # Comprehensive quickstart guide
├── Source/RshipSpatialAudioRuntime/
│   ├── Public/
│   │   ├── Diagnostics/
│   │   │   └── SpatialAudioBenchmark.h    # Performance benchmarking
│   │   └── Blueprint/
│   │       └── SpatialAudioBlueprintLibrary.h   # BP function library
│   └── Private/
│       ├── Diagnostics/
│       │   └── SpatialAudioBenchmark.cpp
│       └── Blueprint/
│           └── SpatialAudioBlueprintLibrary.cpp
```

**Implementation Highlights**:

1. **Performance Benchmarking** ([SpatialAudioBenchmark.h](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Public/Diagnostics/SpatialAudioBenchmark.h)):
   - Benchmarks for VBAP, DBAP, HOA encode/decode
   - Biquad filter and Speaker DSP chain benchmarks
   - OSC serialization performance tests
   - Performance target validation
   - `RunAllBenchmarks()` for comprehensive testing

2. **Blueprint Function Library** ([SpatialAudioBlueprintLibrary.h](Plugins/RshipSpatialAudio/Source/RshipSpatialAudioRuntime/Public/Blueprint/SpatialAudioBlueprintLibrary.h)):
   - **Quick Setup Functions**:
     - `CreateStereoPair()` - Standard L/R speaker setup
     - `Create51SurroundLayout()` - ITU-R BS.775-1 5.1 layout
     - `CreateSpeakerRing()` - Circular speaker arrays
     - `CreateSpeakerDome()` - Hemispherical installations
   - **External Processor Helpers**:
     - `QuickConnectDS100()` - One-call DS100 setup
     - `AutoMapObjectsToDS100()` - Sequential source mapping
   - **DSP Utilities**:
     - `AutoAlignSpeakerDelays()` - Geometric delay alignment
     - `SetAllSpeakersGain()` / `MuteAllSpeakers()` - Bulk operations
   - **Conversion Functions**:
     - `DbToLinear()` / `LinearToDb()`
     - `MsToSamples()` / `SamplesToMs()`
     - `DistanceToDelayMs()`

3. **README / Quickstart Guide** ([README.md](Plugins/RshipSpatialAudio/README.md)):
   - Feature overview and capabilities
   - Installation and setup instructions
   - Quick start examples (C++ and Blueprint)
   - Architecture diagram
   - Performance guidelines
   - API reference summary

4. **SIMD Optimization Hints**:
   - Cache-line alignment for DSP buffers (`alignas(64)`)
   - Contiguous memory access patterns
   - Loop-friendly data structures for auto-vectorization
   - Pre-allocated buffers to avoid runtime allocation

**API Usage Examples**:

```cpp
// Quick 5.1 setup in Blueprint
TArray<FGuid> SpeakerIds = USpatialAudioBlueprintLibrary::Create51SurroundLayout(
    this, 400.0f, 150.0f);

// One-line DS100 connection
bool bConnected = USpatialAudioBlueprintLibrary::QuickConnectDS100(
    this, TEXT("192.168.1.100"));

// Auto-map all objects to DS100
int32 MappedCount = USpatialAudioBlueprintLibrary::AutoMapObjectsToDS100(this);

// Auto-align speaker delays
USpatialAudioBlueprintLibrary::AutoAlignSpeakerDelays(
    this, ListenerPosition, 34300.0f);

// Run performance benchmarks
TArray<FSpatialAudioBenchmarkResult> Results =
    USpatialAudioBenchmark::RunAllBenchmarks();
USpatialAudioBenchmark::LogBenchmarkResults(Results);
```

**Deliverables**: ✅ All Complete
- ✅ SIMD optimization hints in DSP code
- ✅ Memory optimization with pre-allocation
- ✅ README and quickstart guide
- ✅ Blueprint function library for easy access
- ✅ Performance benchmarks with targets
- ✅ Complete API documentation

---

## Appendix A: Comparison with Competing Systems

| Feature | L-ISA | d&b Soundscape | This System (Target) |
|---------|-------|----------------|----------------------|
| Max Objects | 96 | 64 | 256+ |
| Max Outputs | 128 | 64 | 256+ |
| Renderers | VBAP-like | Room, Object | VBAP, DBAP, HOA |
| Phase Control | Yes | Yes | Yes |
| Per-Output DSP | External | DS100 | In-engine |
| Control Protocol | OSC | OSC | rShip/Myko |
| Editor Tools | L-ISA Controller | R1 | UE Editor integrated |
| Price | $$$$$ | $$$$ | Included with rShip |

---

## Appendix B: Glossary

- **VBAP**: Vector Base Amplitude Panning - triangulation-based panner
- **DBAP**: Distance-Based Amplitude Panning - inverse-distance weighted panner
- **HOA**: Higher-Order Ambisonics - spherical harmonic based format
- **Phase-coherent**: Panning that adjusts both amplitude AND delay
- **Zone**: Group of speakers with shared renderer
- **Array**: Group of speakers treated as single unit (e.g., line array)
- **Audio Object**: Virtual sound source with position and routing
- **DSP**: Digital Signal Processing (EQ, delay, limiting, etc.)
