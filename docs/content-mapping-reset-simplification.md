# Content Mapping Reset: Detailed Evaluation + Simplification Plan

## Why this reset
The current mapping stack has grown into one large runtime/editor surface where normalization, auto-repair, capture orchestration, feed composition, material binding, persistence, and transport are tightly coupled. This has made regressions hard to isolate (blank output, stale regen behavior, perf cliffs) and has encouraged fallback behavior that can hide real failures.

This document defines a simpler baseline architecture and records the first simplification pass implemented in this change set.

## Current-state evaluation

### 1) Runtime manager scope is too broad
`URshipContentMappingManager` currently owns all of the following in one file:
- data normalization and schema migration
- context/surface/mapping CRUD + event ingestion
- runtime capture actor management
- feed composition and per-destination render target lifecycle
- material contract checks + per-slot MID application
- cache persistence and stale-event guards
- target registration/state emitters for exec transport

Impact:
- hidden coupling between render output and non-render concerns
- difficult to reason about update order
- expensive tick path for correctness-sensitive code

### 2) Implicit runtime regeneration can conflict with user intent
`EnsureFeedMappingRuntimeReady` has historically auto-created feed sources/destinations/routes under some missing-field conditions.

Impact:
- deleted entities can appear to return when older/incomplete payloads arrive
- difficult to maintain deterministic "what user authored is what runtime executes"

### 3) Runtime fallback textures can mask real failures
When feed/context textures are unavailable, runtime has used preview/default textures.

Impact:
- apparent output can become a solid color unrelated to real source state
- makes debugging source resolution and route validity harder

### 4) Plugin split introduced asset-path fragility
After moving mapping code to `RshipMapping`, material lookup paths still prioritized legacy `RshipExec` locations.

Impact:
- valid mapping assets can fail to resolve depending on project/plugin layout

### 5) Default capture profile favored fidelity over baseline responsiveness
Capture quality default was `fidelity`.

Impact:
- large frame-time tax for scenes that need immediate interactive feedback while mapping

## Simplified target architecture

### A) Authoritative state model
- Runtime must not invent feed graph entities after creation.
- Runtime may sanitize/clamp fields, but should not auto-create routes/destinations/sources from missing arrays.
- Editor is the source of default graph generation for new mappings.

### B) Strict render contract
- Live pixels come only from resolved context textures / feed composite textures.
- No generic "preview texture" substitution in runtime output path.
- Missing texture states should be explicit errors, not synthetic image output.

### C) Deterministic asset lookup
- Material resolution should check paths for both current (`RshipMapping`) and legacy (`RshipExec`) plugin layouts, then project content fallback.

### D) Performance-first defaults
- Keep quality tunable, but default to balanced capture profile for iterative mapping workflows.

## Phase 1 implemented in this pass

### 1) Removed implicit feed entity regeneration
In `EnsureFeedMappingRuntimeReady`:
- removed auto-creation of default source when `sources` is absent/empty
- removed auto-creation of destinations from surface ids when `destinations` is absent/empty
- removed auto-creation of default route fan-out when `routes` is absent/empty

Result:
- user-authored deletions remain authoritative
- runtime normalization is now sanitize/clamp only

### 2) Removed runtime fake-texture fallback in apply path
In `ApplyMappingToSurface` + `ApplyMaterialParameters`:
- removed default preview texture substitution
- mapping now binds resolved texture (or null) directly
- missing-texture state is reported via `LastError` rather than masked by synthetic texture output

Result:
- output reflects true pipeline state
- easier debugging of real source/route failures

### 3) Hardened material lookup for split plugin layouts
Material candidate order now includes:
- `/RshipMapping/Materials/...`
- `/RshipExec/Materials/...`
- `/Game/Rship/Materials/...`

Result:
- fewer project-layout-dependent failures after plugin separation

### 4) Lowered default capture quality profile
`rship.cm.capture_quality_profile` default changed:
- from `2` (fidelity) to `1` (balanced)

Result:
- lower default capture overhead without removing quality controls

## Remaining simplification work (next phases)

1. Split manager responsibilities into focused runtime services
- `ContextCaptureRuntime` (camera/depth capture lifecycle)
- `FeedCompositorRuntime` (route->destination texture composition)
- `SurfaceMaterialRuntime` (MID lifecycle and parameter binding)
- manager keeps orchestration only

2. Remove rendering-side dependency on transport registration concerns
- target registration and emitter state should not live in rendering execution path

3. Reduce per-tick hashing overhead
- cache config hashes on mutation instead of recomputing from JSON every refresh tick

4. Add automated runtime verification harness
- deterministic editor-world and PIE tests that validate:
  - context has live texture
  - mapping writes texture to surface
  - feed delete operations do not regenerate routes/destinations

5. Add explicit runtime diagnostics panel fields
- source texture valid
- feed route count per destination
- applied surface count
- last compositor/update timestamps

## Non-goals of this pass
- no functional redesign of projection math itself
- no UI redesign; this pass focuses on runtime determinism and simplification

