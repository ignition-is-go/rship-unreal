# Content Mapping Material Contract + Advanced Mode Reliability Plan

Date: 2026-02-25
Workspace: /Users/lucid/rship-unreal

## Objective
Eliminate the material-asset weakness by making content-mapping behavior deterministic from code and validated at runtime:
- GUI changes must always cause immediate pixel updates.
- Every mapping mode must route distinct runtime values.
- Unmapped regions must be visible (coverage debug) in a deterministic path.
- Missing/incompatible mapping materials must be detected and handled automatically.

## Scope
1. Material contract validator and enforcement.
2. Advanced mode runtime data path (camera-plate / spatial / depth-map) including depth texture support.
3. Deterministic fallback material behavior with mode branching and coverage output.
4. Context API/cache/schema updates for depth capture/asset depth.
5. Automation tests for contract + mode parameter routing.
6. Build validation.

## Implementation Plan

### A. Material Contract Enforcement
- Add required material parameter contract in `URshipContentMappingManager`:
  - Required scalar params:
    - `RshipMappingMode`, `RshipProjectionType`, `RshipUVRotation`, `RshipUVScaleU`, `RshipUVScaleV`, `RshipUVOffsetU`, `RshipUVOffsetV`, `RshipOpacity`, `RshipMappingIntensity`, `RshipUVChannel`, `RshipDebugCoverage`, `RshipRadialFlag`, `RshipContentMode`, `RshipBorderExpansion`.
  - Required vector params:
    - `RshipProjectorRow0..3`, `RshipUVTransform`, `RshipPreviewTint`, `RshipDebugUnmappedColor`, `RshipDebugMappedColor`,
      `RshipCylinderParams`, `RshipCylinderExtent`, `RshipSphereParams`, `RshipSphereArc`, `RshipParallelSize`,
      `RshipMaskAngle`, `RshipFisheyeParams`, `RshipMeshEyepoint`, plus new advanced vectors (`RshipCameraPlateParams`,
      `RshipSpatialParams0`, `RshipSpatialParams1`, `RshipDepthMapParams`).
  - Required texture params:
    - `RshipContextTexture`, new `RshipContextDepthTexture`.
- Validate loaded material at initialization.
- If contract fails:
  - log missing params clearly,
  - auto-fallback to managed fallback material,
  - mark contract state so diagnostics are visible.

### B. Advanced Mode Runtime Data Path
- Extend `FRshipRenderContextState` with depth support:
  - `DepthAssetId`, `DepthCaptureMode`, `bDepthCaptureEnabled`, transient `ResolvedDepthTexture`, transient `DepthRenderTarget`, transient `DepthCaptureComponent`.
- Normalize/serialize/cache/context-event/action handling for new fields.
- Camera source path:
  - create/maintain secondary `USceneCaptureComponent2D` for depth capture when enabled,
  - create/update depth RT and capture source (`SceneDepth` / `DeviceDepth`),
  - keep color and depth capture independent.
- Asset-store source path:
  - resolve depth texture via `DepthAssetId` (same asset cache flow).

### C. ApplyMaterialParameters Extensions
- Always bind both texture channels:
  - color -> `RshipContextTexture`,
  - depth -> `RshipContextDepthTexture` (default texture fallback if unavailable).
- Keep projection type routing distinct:
  - `camera-plate=9`, `spatial=10`, `depth-map=11`.
- Add advanced mode params from mapping config:
  - camera-plate: fit/anchor style scalar packing in `RshipCameraPlateParams`.
  - spatial: volume params in `RshipSpatialParams0/1`.
  - depth-map: `depthScale`, `depthBias`, `depthNear`, `depthFar` -> `RshipDepthMapParams`.

### D. Deterministic Fallback Material Behavior
- Rebuild fallback material with `UMaterialExpressionCustom` shader branch:
  - UV mode branch uses UV transform.
  - Projection branch computes projected UV from projector rows.
  - Advanced projection indices (9/10/11) run distinct branches.
  - Depth-map branch consumes `RshipContextDepthTexture`.
  - Coverage branch paints unmapped area red when `RshipDebugCoverage > 0`.
- This ensures deterministic behavior even if project material is missing/incompatible.

### E. API + Target Registration
- Register new context actions and schemas:
  - `setDepthAssetId`, `setDepthCaptureEnabled`, `setDepthCaptureMode`.
- Ensure action handlers update context state + trigger resolve/reapply immediately.

### F. Automation Tests
- Add dev automation tests for manager-level guarantees:
  1. Material contract validator catches missing required params.
  2. Projection type mapping yields expected scalar index for each mode.
  3. Context depth field normalization and serialization roundtrip.
- Keep tests deterministic and editor-safe.

### G. Verification Checklist
- Build succeeds for `BuildTestEditor`.
- Runtime smoke checks:
  - Feed/direct/projection edits update immediately.
  - `camera-plate/spatial/depth-map` produce distinct param values and fallback branch changes.
  - Coverage mode marks unmapped pixels red.

## Execution Log
- [x] A implemented
- [x] B implemented
- [x] C implemented
- [x] D implemented
- [x] E implemented
- [x] F implemented
- [x] G validated

### Validation Notes
- `./scripts/package-plugin.sh 5.7 ./dist/tmp-build-check` completed successfully (BuildPlugin for Mac):
  - `UnrealEditor Mac Development`: success
  - `UnrealGame Mac Development`: success
  - `UnrealGame Mac Shipping`: success
- Headless automation run on a temporary host project:
  - Command: `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests Rship.ContentMapping.; Quit" -TestExit="Automation Test Queue Empty"`
  - Result: 3/3 tests passed
    - `Rship.ContentMapping.Context.DepthRoundTrip`
    - `Rship.ContentMapping.Contract.Validation`
    - `Rship.ContentMapping.Parameters.ProjectionTypeRouting`
  - Log: `~/Library/Logs/Unreal Engine/HostProjectEditor/HostProject.log`
