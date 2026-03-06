# RshipField GPU Displacement System

This document records the implemented architecture and locked product constraints for the `RshipField` plugin.

## Implemented components

- `Plugins/RshipField/RshipField.uplugin`
- Runtime module `RshipField`
  - `URshipFieldSubsystem`
  - `URshipFieldControllerComponent` (`RS_` action bridge)
  - `URshipFieldTargetComponent` (per-target binding + identity)
  - `URshipFieldBindingRegistryAsset`
  - `URshipFieldPresetAsset`
  - `FRshipField*` public types in `RshipFieldTypes.h`
  - RDG compute pipeline in `RshipFieldShaders.{h,cpp}` and `Shaders/Private/RshipFieldCS.usf`
- Editor module `RshipFieldEditor`
  - Field Studio nomad tab scaffold

## Locked constraints captured

- GPU-native runtime deformation path (RDG compute).
- Global field sampled by many targets.
- Scalar and vector field channels with separate gains.
- Signed blend behavior including multiply modulation form `acc = acc * (1 + signal * w)`.
- Clamp stages: emitter, layer, final output.
- Max displacement default `100cm`.
- Fixed world domain default `10000cm` cube.
- Settable fixed update rate (`Hz`).
- Spline emitters with curvature-adaptive tolerance baseline `1cm`.
- Debug controls always available and triggerable via `RS_` actions.
- Deterministic identity support with stable GUID + visible target path.

## Rship integration updates

- `RshipExec` target registration now injects optional `metadata.rshipField` when an actor has a `RshipFieldTargetComponent`.
- Metadata includes:
  - `stableGuid`
  - `visiblePath`
  - `fingerprint` (`actorPath`, `componentName`, `meshPath`)
  - `componentClass`

## RS_ action bridge

Implemented on `URshipFieldControllerComponent`:

- `RS_ApplyFieldPacket(FString PacketJson)`
- `RS_FieldSetUpdateHz(float Hz)`
- `RS_FieldSetBpm(float Bpm)`
- `RS_FieldSetTransport(float BeatPhase, bool bPlaying)`
- `RS_FieldSetMasterScalarGain(float Gain)`
- `RS_FieldSetMasterVectorGain(float Gain)`
- `RS_FieldDebugSetEnabled(bool bEnabled)`
- `RS_FieldDebugSetMode(FString Mode)`
- `RS_FieldDebugSetSlice(FString Axis, float Position01)`
- `RS_FieldDebugSetSelection(FString SelectionId)`

## Notes

- The packet parser enforces `schemaVersion == 1` and requires `sequence`.
- Packets are queued and applied deterministically by `applyFrame` then `sequence`.
- Fixed-step simulation advances transport phase deterministically when `playing=true`.
- Target override precedence is `stableGuid` then `visibleTargetPath`.
- Global field atlases are generated as UAV-capable RTs and consumed by target deformation passes.
- Global compute now consumes packet-driven packed descriptor buffers for layers, phase groups, emitters, and spline-compiled emitters.
- Spline emitters are compiled into deterministic sample emitters using curvature-sensitive step size derived from tolerance.
- Runtime perf counters are exposed for queued packets, active emitters/splines, and dispatched targets.
- Binding registry supports upsert and lookup by stable GUID, visible path, and fingerprint keys.
- Added dev automation tests for packet ingest validation, deterministic apply-frame ordering, and binding registry behavior.
- The current editor UI is scaffold-level and intended to be expanded.

## Remaining work tracked

- Extend test coverage for parser determinism, clamp ordering, and RS action integration.
- Expand Field Studio from tab scaffold into full authoring/debug UX.
