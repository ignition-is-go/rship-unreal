# Display Management Architecture Plan (Windows-first, Portable Core)

## 1) Problem Summary
Windows display identity is unstable across layers:
- Windows Display Settings index (Display 1/2/3)
- GPU vendor control panel index (NVIDIA/AMD)
- Engine or app monitor ordering
- Runtime index changes on reboot, hotplug, driver update, or power events

This causes non-deterministic behavior in live rendering systems where we need exact pixel ownership.

## 2) Product Goals
1. Deterministically identify every physical output independent of index ordering.
2. Deterministically apply and re-apply topology/layout after reboots/hotplug.
3. Support mosaic groups for single render context workflows.
4. Provide explicit pixel routing: source canvas rect -> destination output rect.
5. Verify at runtime what pixels are going where, with diagnostics and confidence scoring.
6. Build as portable libraries usable by:
- `RshipExec` Unreal plugin
- standalone Rust application/daemon

## 3) Non-Goals (Phase 1)
- Replace all vendor control-panel functionality.
- Full multi-OS parity before Windows is production-ready.
- Automatic correction for physically bad cables/hubs (we can detect and report instability).

## 3.1) Current Implementation Status (2026-02-12)
Implemented:
- Rust workspace with `core`, `windows`, `vendor-nv`, `ffi`, and `cli` crates.
- Deterministic identity matching with weighted evidence and pin support.
- Profile validation with topology/mosaic/route checks and canonical-ID-aware resolution.
- Planning outputs now include `plan`, `identity`, `validation`, and `ledger`.
- Windows apply path supports topology staging/commit, rollback attempt, and post-apply verification.
- Unreal `URshipDisplayManager` integration with target/actions/emitters and console commands.

Pending hardening gaps:
- NVIDIA Mosaic backend is currently scaffolded, not linked to NVAPI.
- Windows-specific code is not yet target-verified in this repo environment (non-Windows host).
- No rig-lab certification matrix execution yet (reboot/hotplug/driver churn scenarios).

## 4) Proposed Architecture
Rust-first core with thin host adapters.

### 4.1 Crate/Module Boundaries
1. `rship-display-core` (pure Rust)
- Domain model (displays, paths, mosaics, pixel maps)
- Identity matching algorithm
- Layout planner and safety checks
- Validation and diff engine
- No direct OS or vendor API calls

2. `rship-display-windows` (Win32 + DXGI)
- Snapshot collection from Windows display APIs
- Apply topology/mode changes
- Event watcher (hotplug, display change)
- Capability probing (HDR, refresh, bit depth)

3. `rship-display-vendor-nv` (optional feature)
- NVIDIA mosaic and vendor-only capabilities
- Strict feature gate to keep core vendor-neutral

4. `rship-display-ffi` (C ABI)
- Stable C ABI exports for UE C++ calls
- Opaque handles + JSON/flatbuffer payload boundaries
- Generated headers via `cbindgen` (same pattern as NDI crate)

5. `rship-display-cli`
- Standalone controller/daemon
- Commands: `snapshot`, `plan`, `apply`, `verify`, `watch`
- Writes audit logs and manifests

### 4.2 Unreal Integration
Add a new manager under `RshipExec` modeled after current manager patterns:
- Runtime manager similar lifecycle to `URshipContentMappingManager`
- Initialized/ticked via `URshipSubsystem` (`/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExec/Private/RshipSubsystem.cpp`)
- Exposes targets/actions/emitters for remote control similar to content mapping target pattern

Suggested class names:
- `URshipDisplayManager`
- `FRshipDisplaySnapshot`
- `FRshipDisplayPlan`
- `FRshipDisplayApplyResult`

## 5) Canonical Data Model

### 5.1 Display Identity Model
Each physical output gets a `CanonicalDisplayId` (UUID) resolved from weighted identity evidence.

Identity evidence fields (strong -> weak):
1. EDID serial + manufacturer + product code
2. EDID full hash
3. Adapter LUID + target ID + connector type
4. Monitor instance path / PnP ID
5. Native/preferred timing tuple (width, height, refresh)
6. Friendly name fallback

Persist:
- `canonical_display_id`
- confidence score
- first seen / last seen timestamps
- alias history (old OS IDs previously mapped)

### 5.2 Topology Model
```text
DisplaySnapshot
  adapters[]
  targets[]
  paths[]
  modes[]
  capability[]
  timestamp
```

`DisplayPath` includes:
- source mode: desktop-space origin, size, rotation, scaling mode
- target mode: output timing (resolution/refresh/interlaced)
- active/inactive flags

### 5.3 Mosaic Model
`MosaicGroup`:
- id
- members: ordered `CanonicalDisplayId[]`
- arrangement: rows/cols or explicit tile map
- expected total canvas size
- vendor backend (`none|nvidia|amd`)
- requirement flags (`must_be_single_os_display`, `allow_software_fallback`)

### 5.4 Pixel Routing Model
`PixelRoute` (authoritative contract):
- `route_id`
- `source_canvas_id`
- `source_rect_px` (x,y,w,h)
- `dest_display_id`
- `dest_rect_px`
- `transform` (rotation/flip if needed)
- `sampling` (nearest/linear/custom)

Validation invariants:
- no negative dimensions
- no out-of-bounds source or destination rects
- configurable overlap policy (`forbid|allow_with_priority`)
- route coverage report (`assigned_px`, `unassigned_px`, `overlap_px`)

## 6) Deterministic Identity Resolution
Use weighted matching from previous known state to current OS snapshot.

### 6.1 Matching Algorithm
1. Build candidate pairs (`known display` <-> `current observed output`).
2. Score each pair from identity evidence.
3. Solve max-score one-to-one assignment.
4. Apply confidence thresholds:
- high confidence: auto-bind
- medium: auto-bind + warning
- low: unresolved, require operator pinning

### 6.2 Manual Pinning
Allow explicit pin rules:
- `pin canonical_display_id -> specific monitor instance/connector`
- stored in profile and re-applied before automatic matching

### 6.3 Drift and Churn Detection
Emit warnings when:
- canonical ID remaps to different EDID hash
- same EDID appears on multiple outputs unexpectedly (EDID clone/hub issues)
- display reorders more than N times per hour

## 7) Windows Adapter Design

### 7.1 Snapshot Collection APIs
Primary APIs:
- `QueryDisplayConfig` (+ active and database modes)
- `DisplayConfigGetDeviceInfo`
- `EnumDisplayDevices` (supplemental names)
- DXGI adapter/output metadata

Optional enrichment:
- SetupAPI / registry EDID reads
- WMI monitor metadata if needed

### 7.2 Apply APIs
- `SetDisplayConfig` for topology/mode operations
- optional vendor API call path for mosaic apply

Apply pipeline:
1. Preflight capability checks
2. Plan simulation against current snapshot
3. Transactional apply steps
4. Post-apply snapshot and diff verification
5. Rollback if critical invariant fails

### 7.3 Eventing
Watch and debounce:
- `WM_DISPLAYCHANGE`
- display arrival/removal notifications
- power/sleep resume events

Debounce rule: hold event stream for short settle window, then re-snapshot once stable.

## 8) Mosaic Strategy

### 8.1 Preferred Path
When hardware supports driver-level mosaic, treat mosaic as a single OS display:
- one render context at combined resolution
- explicit internal tile map for audit/diagnostics

### 8.2 Fallback Path
If hardware/vendor API is unavailable:
- software mosaic mode
- multiple windows/viewports with explicit placement
- same `PixelRoute` contract used for audit consistency

### 8.3 Safety Checks
Before enabling mosaic:
- same timing compatibility across members
- bandwidth headroom check
- connector compatibility
- VRR/HDR compatibility matrix

## 9) Pixel Truth and Verification

### 9.1 Runtime "Pixel Ledger"
For every active route, emit:
- source canvas dimensions
- destination display and exact rect
- checksum/timestamp of route config

### 9.2 Test Pattern Verification
Provide built-in verifier patterns:
- tile index grid
- per-tile color code
- text overlay of canonical display ID + rect

Capture/verification options:
- local readback checks where possible
- operator camera-based confirmation path (optional phase)

### 9.3 Telemetry
Per display and per route:
- resolution/refresh/timing in use
- dropped frames/present jitter (where available)
- mismatch flags: expected vs observed mode

## 10) Profile and Persistence
Profiles are declarative desired state:
- displays (required/optional)
- topology and arrangement
- mosaic definitions
- pixel routes
- apply policy (`strict|best_effort`)

Storage:
- JSON or TOML profile files
- separate runtime state cache with history and confidence metadata

## 11) Safety and Rollback

### 11.1 Two-Phase Apply
1. `plan`: compute changes, no OS mutation
2. `apply`: execute in ordered steps with checkpoints

### 11.2 Rollback Trigger Conditions
Rollback when:
- a required display becomes unresolved
- post-apply snapshot violates strict profile invariant
- mosaic creation fails in strict mode

### 11.3 Dry Run UX
Expose reasons before apply:
- unsupported capability
- missing displays
- confidence too low
- conflicting routes

## 12) Execution Plan (Phased)

### Phase 0 - Contracts and Scaffolding (1-2 weeks)
Deliverables:
1. Create Rust workspace under RshipExec third-party path (or adjacent shared workspace).
2. Implement `rship-display-core` domain types and validators.
3. Implement `rship-display-ffi` shell with cbindgen-generated C headers.
4. Add Unreal Build.cs detection/linking path, mirroring NDI integration style.

Exit criteria:
- UE can call FFI to parse profile and return validation report.

### Phase 1 - Windows Snapshot + Deterministic IDs (2-3 weeks)
Deliverables:
1. `snapshot()` implementation in `rship-display-windows`.
2. Identity matching engine with confidence scoring.
3. Persistence of canonical IDs and alias history.
4. CLI command: `snapshot --json`.

Exit criteria:
- repeated reboot/hotplug tests keep canonical IDs stable in >95% of controlled cases.

### Phase 2 - Planning and Apply Engine (2-3 weeks)
Deliverables:
1. Diff planner (`current snapshot` -> `desired profile`).
2. Apply engine via SetDisplayConfig.
3. Rollback and post-apply verification.
4. CLI commands: `plan`, `apply`, `verify`.

Exit criteria:
- deterministic re-apply from cold boot across test rigs.

### Phase 3 - Mosaic Backends (2-4 weeks)
Deliverables:
1. Mosaic abstraction in core.
2. NVIDIA backend (feature-gated), fallback software path.
3. Capability and preflight diagnostics.

Exit criteria:
- one-command bring-up for at least one validated mosaic rig.

### Phase 4 - Unreal Runtime + Editor Tooling (2-4 weeks)
Deliverables:
1. `URshipDisplayManager` lifecycle in subsystem.
2. Target/action/emitter model for display operations and state pulses.
3. Editor panel for topology graph, confidence, and pixel route preview.
4. Optional bridge to existing content mapping model for render context alignment.

Exit criteria:
- operator can inspect, apply, verify, and recover from UI + remote actions.

### Phase 5 - Hardening and Certification (ongoing)
Deliverables:
1. Rig matrix automation (GPU vendors, driver versions, MST, mixed DPI/HDR).
2. Fault-injection tests (disconnect/reconnect, sleep/wake, driver reset).
3. Operational playbooks and monitoring alerts.

Exit criteria:
- documented supported configurations and known limitations.

## 13) Test Matrix (Must-Have)
1. Single GPU, mixed monitors, mixed refresh.
2. NVIDIA + mosaic capable rig.
3. MST hub topology.
4. Sleep/resume and reboot persistence.
5. Windows update / driver update regression pass.
6. Duplicate EDID edge case (identical panels/splitters).
7. Remote desktop interference scenarios.

## 14) Key Gotchas and Mitigations
1. Index churn: never key by OS display index.
- Mitigation: canonical identity + weighted matching.

2. Duplicate EDID (splitters/walls): ID collisions.
- Mitigation: include adapter/target path and connector evidence; require manual pin in low confidence cases.

3. Hotplug storms and transient invalid snapshots.
- Mitigation: debounce and settle window before planning/apply.

4. Driver-level mosaic availability varies by SKU/driver.
- Mitigation: capability probe + explicit strict/best-effort policy.

5. Mixed DPI/scaling confusion.
- Mitigation: keep all planning in physical pixels; treat DPI as UI concern only.

6. HDR/SDR mode flips change output characteristics.
- Mitigation: profile includes expected color/HDR flags and verify after apply.

7. Fullscreen heuristics in Windows can move/resize unexpectedly.
- Mitigation: controlled window mode policy and explicit window placement checks.

## 15) Suggested Initial Integration Points in Current Codebase
1. Manager lifecycle:
- `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExec/Private/RshipSubsystem.cpp`
- `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExec/Public/RshipSubsystem.h`

2. Settings extensions:
- `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExec/Public/RshipSettings.h`

3. Existing pattern to mirror for action/target/emitter and cache behavior:
- `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExec/Public/RshipContentMappingManager.h`
- `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExec/Private/RshipContentMappingManager.cpp`

4. Rust FFI integration pattern to mirror:
- `/Users/nicholasfletcher/rship-unreal/Plugins/RshipNDI/Source/RshipNDI/RshipNDI.Build.cs`
- `/Users/nicholasfletcher/rship-unreal/Plugins/RshipNDI/Source/RshipNDI/ThirdParty/rship-ndi-sender/Cargo.toml`
- `/Users/nicholasfletcher/rship-unreal/Plugins/RshipNDI/Source/RshipNDI/ThirdParty/rship-ndi-sender/build.rs`

## 16) First Implementation Slice (Recommended)
Smallest useful vertical slice:
1. Rust core schema + validators.
2. Windows snapshot collector returning canonicalized JSON.
3. CLI `snapshot` output and deterministic ID matching against previous snapshot.
4. Unreal call path to request snapshot and display confidence/errors in a debug panel.

This gives immediate visibility into why current display state is unstable before we attempt full topology mutation.
