# Content Mapping Performance Overhaul Master Plan

Date: 2026-02-25  
Workspace: /Users/lucid/rship-unreal

## 1. Goal
Deliver a content-mapping runtime that remains live, pixel-accurate, and interactive at scale.

Non-negotiable outcomes:
- Feed/direct/projection mappings stay visually live in Editor and PIE.
- Mapping edits produce visible pixel changes in under one frame whenever possible.
- Deleting/disabling mappings removes output immediately.
- Runtime scales to many mappings/routes/screens without frame collapse.
- Same core behavior across macOS, Windows, and Linux.

## 2. Performance Targets

### 2.1 Frame budgets
- Editor idle (no active mappings): < 0.5 ms CPU in content mapping manager.
- Editor active mapping (moderate): < 2.0 ms CPU + < 1.0 ms GPU for mapping pipeline.
- PIE active mapping (heavy): < 4.0 ms CPU + < 3.0 ms GPU for mapping pipeline.

### 2.2 Latency targets
- Property edit -> visible result: <= 33 ms (<= 2 frames at 60 fps).
- Route add/remove -> visible result: <= 50 ms.
- Mapping delete -> unapply visible next frame.

### 2.3 Scale targets
- 32+ screens (mapping surfaces).
- 16+ simultaneous render contexts.
- 256+ active feed routes.
- 64+ active mappings.
- Stable > 60 fps on reference workstation in Editor viewport with representative scene complexity.

## 3. Current Bottleneck Summary
Current hot-path risks in existing architecture:
- Per-tick iteration over full mapping/surface sets even when unchanged.
- Feed composition work triggered too broadly (not destination-dirty scoped).
- Material parameters set repeatedly even when values/textures are unchanged.
- Surface resolution and world-path correction logic can execute too often.
- Context activation policy has oscillated between too aggressive (sleep) and too expensive (always-live).
- Diagnostic logging can accidentally become frame-time dominant.

## 4. Architectural Principles
- Compile once, execute incrementally.
- Dirty-driven updates only.
- Destination-scoped recomposition for feed path.
- Context updates are policy-driven and budgeted.
- Material rebinding is hash-based and minimized.
- Game-thread work is bounded and predictable.
- Strong observability: no black-box failure states.

## 5. New Runtime Architecture

### 5.1 Split authoring and runtime state
- Authoring state (source of truth): exact persisted mapping JSON.
- Compiled runtime graph: normalized, indexed, immutable snapshot used by hot path.

### 5.2 Compiled graph model
Introduce a compiled graph layer (new internal structs):
- `FCompiledContextNode`
  - ContextId
  - SourceType
  - Capture profile (resolution, source mode)
  - Active policy state
- `FCompiledSurfaceNode`
  - SurfaceId
  - Mesh binding info
  - Slot bindings
- `FCompiledMappingNode`
  - MappingId
  - Mode (direct/feed/projection subtype)
  - Context dependency set
  - Destination dependency set
- `FCompiledDestinationNode`
  - DestinationId
  - SurfaceId
  - Render target descriptor
  - Route list (pre-sorted)
- `FCompiledRoute`
  - RouteId
  - SourceContextId
  - SourceRectPx
  - DestinationRectPx
  - Opacity
  - Blend mode

### 5.3 Indexes for O(1)/O(k)
Maintain precomputed indexes:
- `MappingsBySurfaceId`
- `DestinationsBySurfaceId`
- `RoutesByDestinationId`
- `DestinationsByContextId`
- `MappingsByContextId`

This removes repeated scans through unrelated mappings/routes.

## 6. Dirty-Bit Pipeline

### 6.1 Dirty domains
Use explicit dirty flags:
- `Dirty.ContextConfig`
- `Dirty.ContextTexture`
- `Dirty.MappingConfig`
- `Dirty.SurfaceBinding`
- `Dirty.FeedDestination`
- `Dirty.FeedRoutes`
- `Dirty.MaterialBinding`

### 6.2 Event-to-dirty mapping
- `setContextId`, source changes -> mark dependent mappings/destinations dirty.
- Route changes -> only affected destination dirty.
- Destination changes -> only that destination + bound surfaces dirty.
- Surface mesh path change -> only that surface binding dirty.

### 6.3 Tick stages
Stage A: ingest events -> dirty sets  
Stage B: compile incremental graph diff (if config dirty)  
Stage C: update contexts by policy  
Stage D: recomposite dirty destinations only  
Stage E: apply material updates only where input hash changed  
Stage F: emit diagnostics and clear dirty sets

## 7. Context Lifecycle Policy

### 7.1 Problem to avoid
- Sleeping too aggressively causes black frames.
- Keeping all contexts live wastes capture cost when unused.

### 7.2 New policy
Per context, compute `DesiredActivity`:
- `RequiredNow` if referenced by any enabled mapping or dirty destination.
- `GraceWindow` for N frames after last use to prevent flapping.
- `Idle` after grace expires.

Configurable CVars:
- `rship.cm.context_grace_frames` (default 30)
- `rship.cm.max_context_updates_per_tick` (default unlimited, clampable)

### 7.3 Guarantees
- If a context is needed, it is active before composition stage.
- No context enters idle while a dependent destination is dirty.

## 8. Feed Compositor Overhaul

### 8.1 Destination-centric compositor
Replace broad per-mapping composition behavior with destination work queue.
- Queue key: `(MappingId, DestinationId)`.
- Recompose only if destination dirty or source texture revision changed.

### 8.2 Route execution optimization
- Pre-validate and clamp rects during compile phase.
- Precompute normalized UV rectangles once.
- Merge full-frame opaque single-route case into direct texture pass-through.
- Skip zero-opacity/empty routes at compile time.

### 8.3 GPU path
Primary path:
- Render target per destination.
- Route draw list emitted in one pass.
Fallback path:
- Existing Canvas draw path retained behind flag until migration complete.

### 8.4 Reuse and pooling
- Pool destination render targets by descriptor (W/H/format).
- Reuse allocated RTs across destination rebuilds.
- Evict via LRU with memory cap.

## 9. Material Binding Overhaul

### 9.1 Current issue
Repeated `SetTextureParameterValue` / scalar updates per slot per tick even when unchanged.

### 9.2 Binding cache
Per surface-slot, keep a lightweight binding state hash:
- Source texture object ptr + revision
- Mode key
- UV/projection param hash
- Coverage/debug hash

Only call MID setters when hash differs.

### 9.3 Unapply behavior
On mapping delete/disable:
- Remove mapping references from binding graph.
- If slot has no contributing mapping, restore original material immediately.
- If slot still has contributors, update to next resolved mapping same frame.

## 10. Surface Resolution Overhaul

### 10.1 Rules
- Never resolve surfaces in broad loops unless surface binding dirty.
- ActorPath-based resolution first, deterministic fallback second.
- Cache resolved mesh weak refs with generation IDs.

### 10.2 Dirty triggers
- Surface config change
- Actor destroyed/replaced
- World switch PIE/editor

### 10.3 Avoid repeated scans
- Build world actor lookup cache by name/token per world tick (or on world events), not per surface attempt.

## 11. Threading Model
- Game Thread:
  - UObject access, material mutation, capture component mutation, render target operations.
- Worker Thread(s):
  - Graph compile diffs, route validation, hash computation, dependency expansion.

Synchronization model:
- Double-buffer compiled graph snapshots.
- Atomic pointer swap once compile finishes.
- Game thread consumes latest stable snapshot only.

## 12. Observability and Telemetry
Add always-on cheap counters + optional detailed profiling.

### 12.1 Counters per tick
- `cm_enabled_mappings`
- `cm_active_contexts`
- `cm_dirty_destinations`
- `cm_recomposited_destinations`
- `cm_material_updates`
- `cm_surfaces_applied`

### 12.2 Stage timings
- `cm_ms_ingest`
- `cm_ms_compile`
- `cm_ms_context_update`
- `cm_ms_composite`
- `cm_ms_material_apply`
- `cm_ms_total`

### 12.3 Log policy
- Warnings only for real fault states.
- No per-surface spam at normal verbosity.
- Burst dedupe and rate-limit for repeated errors.

## 13. Cross-Platform Requirements
- No editor-only fallback assumptions in runtime behavior.
- Material contract and required params validated the same way on all targets.
- Avoid platform-specific shader branches unless guarded and tested.
- Maintain parity for Metal/D3D/Vulkan texture coordinate conventions and depth interpretation.

## 14. Memory and Resource Budgets
- Render target pool hard cap (configurable): default 512 MB.
- Max per-destination RT resolution clamp with warning.
- LRU eviction when pool cap exceeded.
- Context capture target reuse when descriptors match.

## 15. Migration Plan (Phased)

### Phase 0: Stabilize and Instrument (Immediate)
- Add counters/timers.
- Add no-active-mapping fast path.
- Ensure delete/unapply immediate and deterministic.
- Keep logs low overhead.

### Phase 1: Compiled Graph + Dirty Sets
- Implement compiled graph snapshot structs.
- Replace broad loops with indexed dependency traversal.
- Wire event handlers to dirty domains.

### Phase 2: Destination-Scoped Feed Runtime
- Destination work queue.
- Route precompilation and rect preclamp.
- RT pooling and reuse.

### Phase 3: Material Binding Cache
- Hash-based parameter update suppression.
- Slot-level contributor resolution.
- Deterministic unapply stack.

### Phase 4: Context Lifecycle Policy
- Grace-window activity control.
- Context budget scheduling and throttles.
- Eliminate black-frame transitions.

### Phase 5: GPU Compositor Optimization
- Move expensive per-route CPU path to batched GPU pass where applicable.
- Preserve fallback compatibility path.

### Phase 6: Hardening + Perf Certification
- Automated perf tests and regression CI gates.
- Platform-by-platform acceptance run.

## 16. Acceptance Gates

A change is accepted only if all pass:
1. Correctness
- No black screens under valid configuration.
- Runtime edits reflect immediately.
- Delete/disabling unapply works same frame.

2. Performance
- Meets Section 2 budgets on reference scenes.
- No continuous per-frame cost when no active mappings.

3. Stability
- 30-minute soak in PIE without runaway memory or FPS drift.
- No warning spam loops.

4. Cross-platform
- macOS + Windows parity verified at minimum.

## 17. Perf Test Matrix
- Idle Editor, no mappings.
- 1 mapping / 1 surface / 1 route.
- 8 mappings / 8 surfaces / 32 routes.
- 32 mappings / 32 surfaces / 256 routes.
- Rapid UI edit stress (dragging rects continuously).
- Mapping create/delete churn.
- PIE enter/exit repeatedly.

Metrics captured:
- Frame time (avg/p95/p99)
- Mapping stage timings
- RT pool usage
- Context active counts

## 18. Implemented in This Pass (2026-02-25)

### 18.1 Tick idle fast-path
- `URshipContentMappingManager::Tick` now skips `RefreshLiveMappings()` when there are no enabled mappings and no rebuild happened that frame.
- Rebuild frames still force one refresh pass to guarantee immediate unapply/deactivation behavior.
- Added `HasAnyEnabledMappings()` helper for this gate.

### 18.2 Low-overhead perf telemetry
- Added per-tick stage timing capture:
  - rebuild ms
  - refresh ms
  - cache-save ms
  - total manager ms
- Added runtime counters:
  - enabled mappings
  - active contexts
  - applied surfaces
- Added CVar:
  - `rship.cm.perf_stats` (`0`/`1`) to log one perf line per second.

### 18.3 Material binding hash cache
- Added per-surface-slot binding hash cache (`MaterialBindingHashes`).
- Hash includes mapping type/id/context, opacity, feed-v2 mode, coverage mode, relevant texture pointers, config identity, and runtime revision.
- `ApplyMappingToSurface` now skips MID parameter writes when slot hash is unchanged.
- Hash cache is cleared on material restore/shutdown to avoid stale state.

### 18.4 Feed composite static redraw suppression
- Added feed composite signature cache (`FeedCompositeStaticSignatures`) keyed by `(mappingId, surfaceId)`.
- Signature includes destination dimensions, route topology/rects/opacity, source texture identity, and runtime revision.
- Static-source composites now reuse the prior render target when signature is unchanged.
- Dynamic sources (camera/render-target/non-asset contexts) always redraw to preserve live output.
- Composite signatures are cleaned up when mapping/surface composites are removed.

## 19. Validation Performed
- BuildTestEditor compile after implementation: succeeded.
- Synced modified plugin files into BuildTest project plugin copy.
- BuildTestEditor compile after sync: succeeded.

## 19.1 Single-RT Feed Path Update (2026-02-25)
- Added `rship.cm.feed_single_rt_mode` (default `1`) to force feed mappings onto a single render-target + shader UV transform path.
- Added `TryResolveFeedSingleRtBinding(...)`:
  - resolves source texture directly from feed route source context,
  - computes pixel-accurate normalized `sourceRect` and `destinationRect`,
  - avoids per-surface CPU canvas compositing in normal active runtime path.
- `ApplyMappingToSurface(...)` now uses this single-RT binding path when enabled, and only falls back to legacy feed compositor when the mode is explicitly disabled.
- `ApplyMaterialParameters(...)` now:
  - accepts optional feed binding override,
  - binds route source texture directly,
  - applies UV transform derived from source/destination pixel rectangles.

Implementation impact:
- Removes the biggest active-frame CPU/GPU hotspot (canvas composite pass per destination) from default runtime.
- Keeps pixel-driven route data in control of the material UV transform path.

Known limitation:
- Destination-rect clipping outside mapped region still depends on material behavior; current path computes transform from destination/source rects but does not introduce a new hard clip-mask parameter contract in this pass.

## 20. Implementation File Map
Primary files to change:
- `Plugins/RshipExec/Source/RshipExec/Private/RshipContentMappingManager.cpp`
- `Plugins/RshipExec/Source/RshipExec/Public/RshipContentMappingManager.h`
- `Plugins/RshipExec/Source/RshipExec/Private/RshipSubsystem.cpp` (tick scheduling hooks)
- `Plugins/RshipExec/Source/RshipExec/Private/RshipConsoleCommands.cpp` (perf diagnostics commands)

New internal modules (recommended):
- `RshipContentMappingRuntimeGraph.*`
- `RshipContentMappingCompositor.*`
- `RshipContentMappingPerfStats.*`

## 21. Guardrails for Every Future Change
Before merging any content-mapping change:
- Must declare expected CPU/GPU delta.
- Must include dirty-domain impact statement.
- Must include one regression check from perf matrix.
- Must avoid adding per-surface logs at normal verbosity.

## 22. Immediate Next Implementation Steps
1. Implement destination-dirty feed queue so multi-route feeds avoid full per-frame redraw outside dirty destinations.
2. Add context grace-window policy (`RequiredNow` -> `GraceWindow` -> `Idle`) with explicit frame budget controls.
3. Add render-target pooling with configurable memory cap and LRU eviction.
4. Add automated perf matrix commandlet and baseline gating for regression checks.

## 23. Capture Quality Profile Pass (2026-02-26)

### 23.1 Why this pass was needed
- Unreal Insights delta showed mapping-active cost dominated by additional capture render work on render thread + GPU.
- Active windows also showed capture-side expensive passes (including sky/environment-related capture work), indicating scene-capture quality needed explicit control rather than implicit defaults.

### 23.2 New runtime controls
- Added CVar: `rship.cm.capture_quality_profile`
  - `0` = `performance` (default)
  - `1` = `balanced`
  - `2` = `fidelity`
- Added CVar: `rship.cm.capture_max_view_distance`
  - `0` (default) disables max view distance override.
  - `> 0` applies `MaxViewDistanceOverride` to mapping captures.

### 23.3 Profile behavior in code
- `performance`
  - Forces minimum capture divisor `>= 2`.
  - Forces minimum capture LOD factor `>= 2.0`.
  - Disables expensive capture features and post processing in capture show flags.
  - Disables ray tracing for mapping captures.
- `balanced`
  - Keeps default divisor (from existing CVar).
  - Forces minimum capture LOD factor `>= 1.35`.
  - Keeps core post-process/tone mapping, but disables high-cost GI/reflection/volumetric capture features.
  - Disables ray tracing for mapping captures.
- `fidelity`
  - Uses requested divisor/LOD settings without forced degradation.
  - Re-enables full capture features for highest visual match to main view.
  - Enables ray tracing for mapping captures.

### 23.4 Architectural integration details
- Capture profile now contributes to render-context setup hash.
- Profile changes trigger deterministic capture component reconfiguration without needing mapping recreation.
- Both color and optional depth capture components receive profile-appropriate show-flag setup.
- Existing single-RT feed route path remains unchanged; this pass targets capture cost.

### 23.5 Expected outcome
- Default runtime shifts toward stable interactive performance when mappings are active.
- Visual fidelity remains available by switching to profile `2` when exact parity is required.

## 24. Runtime Path Unification (2026-02-26)

### 24.1 Feed mapping path
- Removed feed runtime bifurcation for active rendering path.
- Feed now always composes from `feedV2` route graph through the compositor path per destination/surface.
- This guarantees multi-route behavior is consistent for active feed mappings instead of diverging by mode.

### 24.2 Mesh/ndisplay projection behavior
- Normalized legacy `mesh` / `ndisplay` projection tokens to perspective behavior.
- Projection routing now treats these tokens as perspective so output matches ndisplay-style camera projection semantics.
