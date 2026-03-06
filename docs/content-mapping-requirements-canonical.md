# Canonical Requirements Baseline: RshipMapping (Code-First)

Date: 2026-03-01  
Workspace: `/Users/lucid/rship-unreal`  
Branch baseline: `content-mapping`

## Summary
This document is the authoritative requirements baseline for content mapping in this workspace.

Policy used for this baseline:
1. Code-first truth source.
2. Full-stack scope (`RshipMapping` runtime + editor, `RshipExec` bridge, tests, and docs conflict audit).
3. Requirements are line-traceable and ID-addressable.

## Requirement Schema
- ID families:
  - `CM-RT-###`: runtime behavior
  - `CM-ACT-###`: action/API behavior
  - `CM-DATA-###`: schema/type/default behavior
  - `CM-UX-###`: editor UX behavior
  - `CM-NFR-###`: non-functional/performance/observability behavior
  - `CM-INT-###`: integration/bridge behavior
- Priority values: `MUST`, `SHOULD`, `MAY`
- Canonical phrasing: `System MUST ... when ...`
- Requirement status values: `implemented`, `untested`, `conflict`

## Requirements Catalog

### Data / Schema Requirements
| ID | Priority | Requirement Statement | Status |
|---|---|---|---|
| CM-DATA-001 | MUST | System MUST expose persisted mapping state via `FRshipRenderContextState`, `FRshipMappingSurfaceState`, and `FRshipContentMappingState` when manager state is queried or serialized. | implemented |
| CM-DATA-002 | MUST | System MUST normalize render-context source aliases to canonical `camera` or `asset-store` when ingesting or updating context state. | implemented |
| CM-DATA-003 | MUST | System MUST default context dimensions and capture modes (`1920x1080`, `FinalColorLDR`, `SceneDepth`) when incoming values are missing/invalid. | implemented |
| CM-DATA-004 | MUST | System MUST normalize mapping-surface UV/material slots by clamping UV channel to non-negative and deduplicating valid non-negative slots. | implemented |
| CM-DATA-005 | MUST | System MUST normalize mapping type into canonical `surface-uv` or `surface-projection` forms when ingesting mapping state. | implemented |
| CM-DATA-006 | MUST | System MUST normalize UV mode tokens to `direct` or `feed` when writing canonical mapping config. | implemented |
| CM-DATA-007 | MUST | System MUST normalize projection mode aliases (including `surface-uv`/`surface-feed`, direct/feed tokens, matrix/camera-plate/depth-map, and mesh aliases) into canonical mapping/projection tokens when writing canonical mapping config. | implemented |
| CM-DATA-008 | MUST | System MUST ensure default UV/projection config blocks exist (uvTransform, projector transforms, clipping/mask/border values) when mapping config is normalized. | implemented |
| CM-DATA-009 | MUST | System MUST ensure projection subtype config defaults exist (cylindrical, spherical, parallel, fisheye, camera plate, spatial, depth-map, custom matrix) when those subtypes are selected. | implemented |
| CM-DATA-010 | MUST | System MUST keep `feedV2.coordinateSpace` canonicalized to `pixel` and ensure `sources`, `destinations`, `routes` arrays exist when feed mode is normalized. | implemented |
| CM-DATA-011 | MUST | System MUST sanitize `feedV2` entities (ID generation, dimension defaults, route defaults, rect defaults/clamps) when preparing feed runtime state. | implemented |
| CM-DATA-012 | MUST | System MUST persist and reload cache using top-level arrays `renderContexts`, `mappingSurfaces`, and `mappings` when saving/loading cache. | implemented |
| CM-DATA-013 | MUST | System MUST resolve mapping material path by trying settings override first, then runtime candidates in `/RshipMapping`, `/RshipExec`, then `/Game/Rship` material paths. | implemented |
| CM-DATA-014 | MUST | System MUST treat `RshipContextTexture` as the required material contract texture parameter when validating mapping material compatibility. | conflict |
| CM-DATA-015 | MUST | System MUST honor `URshipSettings` content-mapping settings (`bEnableContentMapping`, `AssetStoreUrl`, cache/material path overrides) when bootstrapping runtime behavior. | implemented |
| CM-DATA-016 | MUST | System MUST define project-level deterministic pipeline graph schema types (`URshipTexturePipelineAsset`, node/pin/edge/diagnostic/compiled-plan specs) as serializable code-first contracts. | implemented |

### Runtime Requirements
| ID | Priority | Requirement Statement | Status |
|---|---|---|---|
| CM-RT-001 | MUST | System MUST initialize content mapping by creating/connecting asset-store client (if configured), loading material, loading cache, and marking mappings dirty when content mapping is enabled. | implemented |
| CM-RT-002 | MUST | System MUST shutdown content mapping by persisting dirty cache, disconnecting asset-store client, restoring mapped materials, and clearing runtime caches/references. | implemented |
| CM-RT-003 | MUST | System MUST run rebuild stage when mappings are dirty and run refresh stage only when required by rebuild, continuous-refresh need, or runtime-prepare pending. | implemented |
| CM-RT-004 | MUST | System MUST debounce cache writes and flush after delay rather than writing every mutation frame. | implemented |
| CM-RT-005 | MUST | System MUST calculate required contexts from enabled mappings and disable/destroy non-required context capture resources during live refresh. | implemented |
| CM-RT-006 | MUST | System MUST resolve camera contexts using camera-id lookup plus fallback behavior (auto-selected source camera, source anchor, player view, editor view) when primary source resolution fails. | implemented |
| CM-RT-007 | MUST | System MUST configure scene-capture behavior using capture quality/profile CVars and main-view integration settings when resolving camera contexts. | implemented |
| CM-RT-008 | MUST | System MUST create/maintain depth render target and depth capture component only when depth capture is enabled for a context. | implemented |
| CM-RT-009 | MUST | System MUST resolve asset-store textures using on-disk cache first and async download fallback for missing assets. | implemented |
| CM-RT-010 | MUST | System MUST resolve mapping surfaces by actor-path exact resolution first and fallback scored actor/mesh search when exact actor-path lookup fails. | implemented |
| CM-RT-011 | MUST | System MUST apply mappings only when surface mesh is mutable, mapping material exists, and material contract validation passes. | implemented |
| CM-RT-012 | MUST | System MUST report explicit missing-texture errors and skip material application when no effective texture is available. | implemented |
| CM-RT-013 | MUST | System MUST compose feed destination output from feed routes using destination filtering, source context resolution, and route rect transforms when feed mapping is active. | implemented |
| CM-RT-014 | SHOULD | System SHOULD bypass feed composition RT and bind source texture directly when exactly one full-frame, opaque, destination-matching route is present. | implemented |
| CM-RT-015 | MUST | System MUST protect against stale mapping reappearance by tracking pending upsert/delete tombstones and rejecting out-of-order stale payloads within guard windows. | implemented |
| CM-RT-016 | MUST | System MUST mark mappings dirty when coverage-preview flag changes so coverage shader params refresh on next apply. | implemented |
| CM-RT-017 | MUST | System MUST expose render-context JSON snapshot for subsystem bridge including enabled state, dimensions, source metadata, render-target presence, and last error. | implemented |
| CM-RT-018 | MUST | System MUST restore original materials and clear MID/hash caches when surface mappings are removed or rebuilt. | implemented |
| CM-RT-019 | MUST | System MUST apply compiled node pipeline plans transactionally by snapshotting current runtime state and rolling back on apply failure. | implemented |
| CM-RT-020 | MUST | System MUST force fail-closed strict behavior during compiled pipeline apply by disabling semantic/material fallback paths for the transaction. | implemented |

### Action / API Requirements
| ID | Priority | Requirement Statement | Status |
|---|---|---|---|
| CM-ACT-001 | MUST | System MUST route actions by target path prefix (`/content-mapping/context/`, `/surface/`, `/mapping/`) when handling routed actions. | implemented |
| CM-ACT-002 | MUST | System MUST support context actions: `setEnabled`, `setCameraId`, `setAssetId`, `setDepthAssetId`, `setDepthCaptureEnabled`, `setDepthCaptureMode`, `setResolution`, `setCaptureMode`. | implemented |
| CM-ACT-003 | MUST | System MUST support surface actions: `setEnabled`, `setActorPath`, `setUvChannel`, `setMaterialSlots`, `setMeshComponentName`. | implemented |
| CM-ACT-004 | MUST | System MUST support mapping actions: `setEnabled`, `setOpacity`, `setContextId`, `setSurfaceIds`, `setProjection`, `setUVTransform`, `setType`, `setConfig`, `setFeedV2`, and feed upsert/remove actions for source/destination/route. | implemented |
| CM-ACT-005 | MUST | System MUST parse JSON safely in `Process*EventJson` and no-op invalid payloads when ingress JSON is malformed. | implemented |
| CM-ACT-006 | MUST | System MUST implement render-context upsert/delete ingest semantics including resolve/register/emit/dirty behavior when context events arrive. | implemented |
| CM-ACT-007 | MUST | System MUST implement surface upsert/delete ingest semantics including material restore, feed-composite cleanup, target delete/register, and dirty behavior when surface events arrive. | implemented |
| CM-ACT-008 | MUST | System MUST implement mapping upsert/delete ingest semantics including type alias normalization, stale echo guards, target register/delete, and dirty/cache updates when mapping events arrive. | implemented |
| CM-ACT-009 | MUST | System MUST ensure manager CRUD methods (`Create/Update/Delete`) call transport sync (`SetItem`/`DelItem`) and set mapping/cache dirty flags on mutation. | implemented |
| CM-ACT-010 | MUST | System MUST register context target actions/emitters so every registered context action has a corresponding context action handler branch. | implemented |
| CM-ACT-011 | MUST | System MUST register surface target actions/emitters so every registered surface action has a corresponding surface action handler branch. | implemented |
| CM-ACT-012 | MUST | System MUST register mapping target actions/emitters so every registered mapping action has a corresponding mapping action handler branch. | implemented |
| CM-ACT-013 | MUST | System MUST emit updated state and mark mappings/cache dirty after successful action handling. | implemented |
| CM-ACT-014 | MUST | System MUST expose pipeline graph validate/compile/apply/rollback manager APIs plus JSON wrappers for reflection bridge invocation. | implemented |

### UX / Editor Requirements
| ID | Priority | Requirement Statement | Status |
|---|---|---|---|
| CM-UX-001 | SHOULD | System SHOULD provide keyboard shortcuts in panel (`Enter` quick create, `D` duplicate, `E` toggle enable, `+` expand config, `-` collapse config). | untested |
| CM-UX-002 | SHOULD | System SHOULD support quick-create defaults persistence and selected-actor fallbacks for source/screen creation workflows. | untested |
| CM-UX-003 | MUST | System MUST expose mode selector with canonical mapping modes (`direct`, `feed`, `perspective`, `custom-matrix`, `cylindrical`, `spherical`, `parallel`, `radial`, `mesh`, `fisheye`, `camera-plate`, `spatial`, `depth-map`) and alias normalization. | implemented |
| CM-UX-004 | MUST | System MUST expose content mode selector options (`stretch`, `crop`, `fit`, `pixel-perfect`) in mapping editor UI. | implemented |
| CM-UX-005 | MUST | System MUST write mode-specific mapping config keys and remove incompatible keys when user changes mapping mode in editor form. | implemented |
| CM-UX-006 | SHOULD | System SHOULD provide projection edit workflow using preview actor spawn/sync/stop controls for projection mappings. | untested |
| CM-UX-007 | MUST | System MUST provide interactive mapping canvas behaviors for UV and feed (paint overlays, hit testing, drag/resize, wheel zoom/adjust). | implemented |
| CM-UX-008 | SHOULD | System SHOULD support list filters and errors-only toggles for contexts/surfaces/mappings with visible/total counts. | untested |
| CM-UX-009 | SHOULD | System SHOULD support explicit row selection and selected-vs-visible bulk scope operations for contexts/surfaces/mappings. | untested |
| CM-UX-010 | MUST | System MUST provide feedV2 authoring controls (source/destination/route lists, per-destination canvases, read/write of `feedV2` config). | implemented |
| CM-UX-011 | MUST | System MUST provide docked node pipeline editor workflow with one-click Validate/Apply/Revert/Diagnostics controls while retaining legacy form controls during rollout. | implemented |

### NFR / Performance / Observability Requirements
| ID | Priority | Requirement Statement | Status |
|---|---|---|---|
| CM-NFR-001 | MUST | System MUST define runtime mapping CVars for perf stats and capture settings (`rship.cm.perf_stats`, capture main-view controls, LOD/divisor/profile/max view distance). | implemented |
| CM-NFR-002 | MUST | System MUST default `rship.cm.capture_quality_profile` to `1` (balanced) when not overridden. | conflict |
| CM-NFR-003 | MUST | System MUST rate-limit perf-stat logging to roughly once per second when perf stats CVar is enabled. | implemented |
| CM-NFR-004 | SHOULD | System SHOULD rate-limit repeated missing-source and missing-texture warnings to prevent per-frame log spam. | implemented |
| CM-NFR-005 | MUST | System MUST skip redundant MID updates using per-surface-slot binding hash cache when bindings are unchanged. | implemented |
| CM-NFR-006 | SHOULD | System SHOULD skip static feed recomposition by using feed composite static signatures when route/source signature is unchanged and sources are non-dynamic. | implemented |
| CM-NFR-007 | MUST | System MUST avoid explicit `CaptureScene()` calls when `bCaptureEveryFrame` is enabled to avoid known inefficiency warnings. | implemented |
| CM-NFR-008 | MUST | System MUST enforce DAG-only compile semantics and deterministic topological ordering for node pipeline execution plans. | implemented |

### Integration / Bridge Requirements
| ID | Priority | Requirement Statement | Status |
|---|---|---|---|
| CM-INT-001 | MUST | System MUST lazy-load `RshipMapping` runtime from `RshipExec` subsystem and instantiate `/Script/RshipMapping.RshipContentMappingManager` via reflection. | implemented |
| CM-INT-002 | MUST | System MUST call manager lifecycle bridge methods (`InitializeForSubsystem`, `TickForSubsystem`, `ShutdownForSubsystem`) through reflection from `RshipExec` subsystem. | implemented |
| CM-INT-003 | MUST | System MUST forward routed target actions from subsystem ingest to manager `RouteActionJson`. | implemented |
| CM-INT-004 | MUST | System MUST forward render-context/surface/mapping event payloads from subsystem ingest to manager `Process*EventJson` handlers. | implemented |
| CM-INT-005 | MUST | System MUST expose subsystem bridge methods for mapping debug overlay get/set and render-context JSON retrieval via manager bridge UFUNCTIONs. | implemented |
| CM-INT-006 | MUST | System MUST return null content-mapping manager from subsystem when `URshipSettings::bEnableContentMapping` is disabled. | implemented |
| CM-INT-007 | MUST | System MUST declare plugin modules `RshipMapping` (Runtime) and `RshipMappingEditor` (Editor), enabled by default, with plugin dependency on `RshipExec`. | implemented |
| CM-INT-008 | MUST | System MUST keep build/module dependencies wired so runtime/editor mapping modules both depend on `RshipExec` and editor module depends on `RshipMapping`. | implemented |
| CM-INT-009 | MUST | System MUST forward pipeline graph validate/compile/apply/rollback calls through `RshipExec` reflection bridge methods for automation/remoting parity. | implemented |
| CM-INT-010 | MUST | System MUST support pluggable endpoint-adapter validation interface for media-profile input/output endpoint bindings in pipeline plans. | implemented |

## Public API Coverage Map (Manager Header)
All public manager methods/UFUNCTIONs are mapped below.

| Public API | Covered By |
|---|---|
| `Initialize`, `Shutdown`, `Tick` | CM-RT-001, CM-RT-002, CM-RT-003 |
| `InitializeForSubsystem`, `ShutdownForSubsystem`, `TickForSubsystem` | CM-INT-002 |
| `ProcessRenderContextEvent`, `ProcessMappingSurfaceEvent`, `ProcessMappingEvent` | CM-ACT-006, CM-ACT-007, CM-ACT-008 |
| `ProcessRenderContextEventJson`, `ProcessMappingSurfaceEventJson`, `ProcessMappingEventJson` | CM-ACT-005 |
| `RouteAction`, `RouteActionJson` | CM-ACT-001, CM-INT-003 |
| `GetRenderContexts`, `GetMappingSurfaces`, `GetMappings` | CM-DATA-001 |
| `SetDebugOverlayEnabled`, `IsDebugOverlayEnabled` | CM-RT-003, CM-INT-005 |
| `SetDebugOverlayEnabledForSubsystem`, `IsDebugOverlayEnabledForSubsystem` | CM-INT-005 |
| `GetRenderContextsJsonForSubsystem` | CM-RT-017, CM-INT-005 |
| `SetCoveragePreviewEnabled`, `IsCoveragePreviewEnabled` | CM-RT-016 |
| `ValidatePipelineGraph`, `CompilePipelineGraph`, `ApplyCompiledPipelinePlan`, `RollbackLastPipelineApply` | CM-ACT-014, CM-RT-019, CM-RT-020 |
| `ValidatePipelineGraphJson`, `CompilePipelineGraphJson`, `ApplyCompiledPipelinePlanJson`, `RollbackLastPipelineApplyJson` | CM-ACT-014, CM-INT-009 |
| `CreateRenderContext`, `UpdateRenderContext`, `DeleteRenderContext` | CM-ACT-009 |
| `CreateMappingSurface`, `UpdateMappingSurface`, `DeleteMappingSurface` | CM-ACT-009 |
| `CreateMapping`, `UpdateMapping`, `DeleteMapping` | CM-ACT-009, CM-RT-015 |
| Test helper methods (`ValidateMaterialContractForTest`, `ApplyMaterialParametersForTest`, `BuildRenderContextJsonForTest`) | CM-DATA-014, CM-RT-011, CM-RT-017 |

## Registered Action ↔ Handler Parity
Every `RegisterAction(...)` action name has a corresponding handler branch.

- Context target parity: CM-ACT-010, CM-ACT-002
- Surface target parity: CM-ACT-011, CM-ACT-003
- Mapping target parity: CM-ACT-012, CM-ACT-004

## Test-to-Requirement Mapping
| Automation Test | Requirement IDs |
|---|---|
| `Rship.ContentMapping.Contract.Validation` | CM-DATA-014, CM-RT-011 |
| `Rship.ContentMapping.Parameters.ProjectionTypeRouting` | CM-DATA-007, CM-RT-011 |
| `Rship.ContentMapping.Context.DepthRoundTrip` | CM-DATA-003, CM-DATA-012 |
| `Rship.ContentMapping.Mapping.DeleteTombstoneGuardsStaleUpsert` | CM-RT-015, CM-ACT-008 |
| `Rship.ContentMapping.Feed.ExplicitEmptyArraysPreserved` | CM-DATA-010, CM-DATA-011 |
| `Rship.ContentMapping.Feed.RemoveRouteGuardsStaleEcho` | CM-ACT-004, CM-RT-015 |
| `Rship.ContentMapping.Feed.RemoveDestinationGuardsStaleEcho` | CM-ACT-004, CM-RT-015 |
| `Rship.ContentMapping.Feed.RemoveSourceGuardsStaleEcho` | CM-ACT-004, CM-RT-015 |
| `Rship.ContentMapping.Pipeline.ValidateRejectsCycle` | CM-NFR-008, CM-ACT-014 |
| `Rship.ContentMapping.Pipeline.CompileDeterministicOrder` | CM-NFR-008, CM-DATA-016 |
| `Rship.ContentMapping.Pipeline.ApplyReplacesMappingMode` | CM-RT-019, CM-ACT-014 |
| `Rship.ContentMapping.Pipeline.ApplyRollbackOnFailure` | CM-RT-019, CM-RT-020 |

## Source-to-Requirement Index (Bidirectional Traceability Support)
| Source File | Key Requirement IDs |
|---|---|
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Public/RshipContentMappingManager.h` | CM-DATA-001, CM-INT-002 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp` | CM-DATA-002..014, CM-RT-001..018, CM-ACT-001..013, CM-NFR-001..007 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Public/RshipTexturePipelineAsset.h` | CM-DATA-016, CM-INT-010 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp` | CM-ACT-014, CM-RT-019, CM-RT-020, CM-NFR-008 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/Tests/RshipContentMappingManagerTests.cpp` | CM-DATA-014, CM-RT-011, CM-RT-015, CM-ACT-004, CM-ACT-008 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/Tests/RshipContentMappingManagerTests.cpp` | CM-NFR-008, CM-ACT-014, CM-RT-019, CM-RT-020 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Private/SRshipContentMappingPanel.cpp` | CM-UX-001..006, CM-UX-008..010 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Public/RshipTexturePipelineEdGraph.h` | CM-UX-011 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Public/RshipTexturePipelineEdGraphNode.h` | CM-UX-011 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Public/RshipTexturePipelineEdGraphSchema.h` | CM-UX-011 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Private/SRshipContentMappingPanel.cpp` | CM-UX-011 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Private/SRshipModeSelector.cpp` | CM-UX-003 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Private/SRshipContentModeSelector.cpp` | CM-UX-004 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Private/SRshipMappingCanvas.cpp` | CM-UX-007, CM-UX-010 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/Private/RshipContentMappingPreviewActor.cpp` | CM-UX-006 |
| `/Users/lucid/rship-unreal/Plugins/RshipExec/Source/RshipExec/Private/RshipSubsystem.cpp` | CM-INT-001..006 |
| `/Users/lucid/rship-unreal/Plugins/RshipExec/Source/RshipExec/Private/RshipSubsystem.cpp` | CM-INT-009 |
| `/Users/lucid/rship-unreal/Plugins/RshipExec/Source/RshipExec/Public/RshipSettings.h` | CM-DATA-015, CM-INT-006 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/RshipMapping.uplugin` | CM-INT-007 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/RshipMapping.Build.cs` | CM-INT-008 |
| `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/RshipMappingEditor.Build.cs` | CM-INT-008 |

## Notes
- Requirements with `conflict` status are code-implemented but currently contradicted by one or more existing docs. See conflict ledger: `/Users/lucid/rship-unreal/docs/content-mapping-requirements-conflicts.md`.
- `untested` means no direct automation test currently found in `RshipContentMappingManagerTests.cpp` for that behavior.
