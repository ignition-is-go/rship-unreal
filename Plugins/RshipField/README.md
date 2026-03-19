# RshipField Plugin

GPU-native 3D field generation, sampling, and mesh deformation for Unreal Engine, integrated with the rship executor system.

## Overview

RshipField evaluates a volumetric scalar+vector field on the GPU every frame using compute shaders. Artists place **effectors** (waves, noise, attractors) inside a bounded domain. The field is written to atlas textures that downstream systems — Niagara particles, Optimus deformer graphs, light drivers — can sample in real time.

```
Effectors  ──►  GPU Compute  ──►  ScalarAtlas + VectorAtlas
                                        │
                    ┌───────────────┬────┴────────────┐
                    ▼               ▼                  ▼
              LightSampler    RawSampler     Optimus / Niagara
              (drive lights)  (direct GPU)   (deform meshes,
                                              drive particles)
```

## Field Component (`URshipFieldComponent`)

The field component defines the simulation domain and owns all effectors. Attach it to any actor to create a field.

| Property | Description |
|---|---|
| `FieldId` | String identifier so samplers can look up this field |
| `FieldResolution` | Voxel grid size (64–320). Higher = finer detail, more GPU cost |
| `DomainCenterCm` / `DomainSizeCm` | World-space bounds of the field volume |
| `MasterScalarGain` / `MasterVectorGain` | Global output multipliers |
| `Bpm` / `BeatPhase` / `bPlaying` | Transport clock for tempo-synced effectors |
| `UpdateHz` | Fixed-timestep simulation rate (default 60) |

### Effector Types

All effectors live inside the field component and are evaluated per-voxel on the GPU.

**Wave Effectors** — Spatial oscillations propagating from a point or along a direction.
- Position, Direction, RadiusCm, Amplitude, WavelengthCm, FrequencyHz
- Waveform: Sine, Triangle, Saw, Square
- FalloffExponent controls distance attenuation

**Noise Effectors** — Procedural noise volumes.
- Modes: Value, Simplex, Curl
- Scale, Amplitude, RadiusCm

**Attractor Effectors** — Radial forces (positive = attract, negative = repel).
- Strength, RadiusCm, FalloffExponent

### Phase Groups

Phase groups sync effector oscillations to the transport clock at different tempo subdivisions. Each group has a `TempoMultiplier` (e.g. 0.5 = half-time) and `PhaseOffset`. Effectors reference groups by `PhaseGroupId`.

### Atlas Output

The field is packed into two 2D render target atlases using a tiling scheme (voxel Z slices → tile grid):

- **ScalarAtlas** (`RTF_R16f`) — single float per voxel
- **VectorAtlas** (`RTF_RGBA16f`) — XYZ direction + intensity in W

## Subsystem (`URshipFieldSubsystem`)

A `UWorldSubsystem` that coordinates all fields in the level.

- Maintains a registry of active field components (`RegisterField` / `UnregisterField` / `FindFieldById`)
- Runs fixed-timestep simulation ticks (beat phase progression, effector state)
- Converts artist-facing effector structs into packed GPU buffers
- Dispatches RDG compute passes via `RshipFieldRDG::AddFieldPasses`

### GPU Pipeline

1. Pack effector data into 7 structured buffers per effector + layer/phase-group buffers
2. Dispatch `BuildGlobalFieldCS` (4x4x4 thread groups) to evaluate all effectors at every voxel
3. Output ScalarAtlas and VectorAtlas
4. (Optional per-target) Dispatch `SampleAndDeformTargetCS` + `RecomputeNormalsCS` for mesh deformation

## Sampling

### RshipFieldSamplerComponent (Base)

Abstract base class for components that consume field data. Resolves its target field by `FieldId` through the subsystem.

### RshipFieldLightSampler

Drives a light component's properties from the field.

- **Intensity**: samples scalar field, scales by `IntensityScale`
- **Color**: samples scalar field, lerps between `ColorA` and `ColorB`
- References a field by `IntensityFieldId` / `ColorFieldId`
- Toggles: `bDriveIntensity`, `bDriveColor`

### RshipFieldMaterialSampler

Drives material parameters from the field. Automatically creates dynamic material instances and pushes atlas textures + domain parameters each tick.

**Artist setup:**

1. In the material graph, add `MF_SampleField` (from `Plugins/RshipField Content/Materials/`). Wire `Scalar` and/or `Vector` outputs to material properties (emissive, opacity, WPO, etc.)
2. On the actor with the mesh, add the `Rship Field Material Sampler` component. Set `FieldId` to match the field in the scene.
3. Done — the component handles all parameter binding automatically.

The material function contains embedded parameter nodes (`FieldScalarAtlas`, `FieldVectorAtlas`, `FieldDomainMin`, `FieldDomainMax`, `FieldResolution`) and an Absolute World Position lookup. No manual parameter creation needed.

### RshipFieldRawSampler

Exposes raw atlas render targets (`GetScalarAtlas()` / `GetVectorAtlas()`) for direct GPU access by materials, Niagara, or custom shaders.

### Optimus Data Interface (`UOptimusFieldSamplerDataInterface`)

Binds field atlases into Optimus deformer graph kernels. Provides HLSL helpers `SampleFieldScalar(worldPos)` and `SampleFieldVector(worldPos)` that handle world-to-atlas coordinate conversion.

### Niagara Data Interface (`UNiagaraDataInterfaceFieldSampler`)

GPU-only Niagara data interface. Particles can sample the field in their simulation shader for position, color, or size modulation.

## RotundaDeformerComponent (`URotundaDeformerComponent`)

A specialized component that drives an Optimus deformer on a skeletal mesh. It pushes a set of named kernel variables to the `MateoKernel.usf` shader each tick via `SetFloatVariable` / `SetVectorVariable`.

This is **not** a field sampler — it's an independent deformation system with its own parameter set, designed for a specific cylindrical/rotunda mesh topology.

### Parameters

| Group | Variables | Description |
|---|---|---|
| **Global** | `RS_Enabled`, `RS_RotundaCenter`, `RS_Energy`, `RS_Amplitude`, `RS_RingRadius` | Master enable, world center, energy remapping, base magnitude, ring radius |
| **Orbital** | `RS_Lobes`, `RS_Speed1`, `RS_Speed2`, `RS_OrbitalWeight` | Number of lobes, two rotation speeds, blend weight |
| **Vertical** | `RS_VertLobes`, `RS_VertSpeed`, `RS_VerticalWeight` | Vertical frequency, speed, blend weight |
| **Structural** | `RS_VerticalProfile`, `RS_AnchorWidth` | Height profile shaping, stable anchor zone width |
| **Shaping** | `RS_WaveShape`, `RS_CrestSharpness`, `RS_RestThreshold`, `RS_DecayK` | Waveform blend, peak sharpness, dead zone, decay rate |

All properties are prefixed `RS_` and exposed as rship actions for remote control. The `Energy` parameter acts as a macro control that remaps amplitude, speeds, and thresholds together.

## Debug Visualization

The field component exposes three debug toggles:

- `bShowWireframes` — draws domain bounds and effector radii
- `bShowDebugText` — prints time, beat, BPM, effector counts on screen
- `bShowVisualizer` — spawns a Niagara particle system at the domain center that visualizes the field

## Rship Integration

All components register targets and expose actions through the rship executor system, allowing remote control of field parameters, effector states, sampler settings, and deformer variables from the rship server.
