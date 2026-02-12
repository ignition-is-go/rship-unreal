# Content Mapping UX Parity Matrix (Disguise vs Unreal Plugin)

This matrix tracks editor UX parity for content mapping workflows.

- Reference target: Disguise Content Mapping manual workflows.
- Unreal implementation under test:
  - `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExecEditor/Private/SRshipContentMappingPanel.cpp`
  - `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExecEditor/Private/SRshipMappingCanvas.cpp`
  - `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExecEditor/Private/SRshipModeSelector.cpp`
  - `/Users/nicholasfletcher/rship-unreal/Plugins/RshipExec/Source/RshipExecEditor/Private/SRshipContentModeSelector.cpp`

Legend:
- `Matched`: UX is present and usable in equivalent workflow.
- `Partial`: available but missing controls, scale ergonomics, or behavior fidelity.
- `Missing`: no practical equivalent in current editor UX.

## 1) Core Workflow UX

| Workflow | Status | Notes |
|---|---|---|
| Create mapping from source + target quickly | Partial | Quick create exists and reuses context/surface; still form-heavy for large batch mapping operations. |
| Edit existing mapping end-to-end in one place | Partial | Inline edit + detailed form both exist; dual-path can be confusing, and list rows are dense. |
| Screen-based mapping management at scale | Partial | Now has filtering and better sorting; bulk multi-select/edit workflows still limited. |
| Immediate visual feedback while editing | Matched | Mapping canvas + preview label + projector edit mode exist. |

## 2) Navigation and List Ergonomics

| UX Area | Status | Notes |
|---|---|---|
| Name-first list ordering | Matched | Implemented (fallback to ID). |
| List filtering/search | Matched | Implemented for Inputs/Screens/Mappings. |
| Error triage filtering | Matched | Per-list `Errors` filters now isolate problematic items quickly. |
| Visibility-aware counts | Matched | Shows visible/total when filters are active. |
| Dense list readability (badges/status/errors) | Partial | Badges and status are present; still visually crowded for very large projects. |
| Bulk list operations (multi-select, mass update) | Partial | Row-selection checkboxes + selected-scope bulk enable/disable/opacity/expand exist; still missing manager-level operation breadth. |

## 3) Mapping Type UX Coverage

| Mapping Type | Status | Notes |
|---|---|---|
| Direct | Matched | Full editor mode support. |
| Feed | Partial | Core feed rect editing exists; advanced feed map operations are incomplete. |
| Perspective | Partial | Basic projection controls are present; still not full parity for all advanced controls/workflow affordances. |
| Cylindrical | Partial | Main controls present; needs deeper parity verification against manual defaults/behavior. |
| Spherical | Partial | Controls present; behavior parity still under validation. |
| Parallel | Partial | Controls present; behavior parity still under validation. |
| Radial | Partial | Controls present; behavior parity still under validation. |
| Mesh | Partial | Eyepoint controls present; workflow polish still needed. |
| Fisheye | Partial | FOV/lens controls present; behavior parity still needed. |
| Camera Plate | Partial | First-class mode card is exposed; still missing dedicated property workflow parity. |
| Spatial | Partial | First-class mode card is exposed; still missing dedicated property workflow parity. |
| Depth Map | Partial | First-class mode card is exposed; still missing dedicated property workflow parity. |

## 4) Common Property UX

| Property Group | Status | Notes |
|---|---|---|
| Input/screen/mapping references | Matched | Picker + text editing available. |
| Opacity/enabled controls | Matched | Present in both inline and form flow. |
| UV transform controls | Matched | Numeric + canvas interaction. |
| Feed rect overrides per screen | Partial | Present, but UX is still manual and not yet optimized for large surface sets. |
| Find/replace usages (inputs/screens) | Partial | Added selected/visible-scope usage replacement tools in mapping bulk row; still needs full manager parity validation. |
| Projection transform (position/rotation/FOV/aspect/clip) | Partial | Present; requires final behavior parity validation with runtime. |
| Masking controls | Partial | Present in UI; runtime behavior parity needs completion. |
| Content mode controls | Partial | Selector exists; runtime behavior parity still needs completion. |

## 5) Interaction Fidelity

| Interaction | Status | Notes |
|---|---|---|
| Drag handles for feed crop | Matched | Present. |
| UV pan/zoom/rotate interaction | Matched | Present (drag/wheel/alt-drag). |
| Projection manipulator in scene | Partial | Edit mode exists; still validating edge-case synchronization UX. |
| Fast keyboard-centric editing | Partial | Text-driven entry is possible but not yet optimized as a primary flow. |

## 6) Priority Gap Backlog (UX-first)

1. Implement bulk workflows for high-density mapping operations (multi-select + apply patterns).
2. Reduce inline row complexity (mode-aware compact row + expandable advanced details).
3. Complete advanced Feed mapping UX controls and map-set workflows.
4. Complete Camera Plate / Spatial / Depth property workflows and runtime-linked UX parity.
5. Final visual and interaction fidelity pass against Disguise manager flow.

## 7) Current Iteration Summary

Completed in this iteration:
- Added filter bars for Inputs/Screens/Mappings.
- Switched list ordering to display-name-first with ID fallback.
- Added filtered count display (`visible/total`) in header.
- Added “no match” list messages for active filters.
- Added bulk enable/disable actions for visible (filtered) inputs/screens/mappings.
- Added collapsible inline mapping config rows to reduce default list density.
- Added persistent row-selection checkboxes for mappings/screens/inputs.
- Added selected-scope bulk operations (enable/disable/opacity/config expand-collapse).
- Added feed-rect table copy/paste/reset controls.
- Added mapping `Find/Replace Usages` for input IDs and screen IDs.
- Added first-class mode cards and normalization support for Camera Plate / Spatial / Depth Map.

Still required for full UX parity:
- Bulk-edit workflows.
- Advanced feed operations.
- Dedicated property + behavior parity for camera plate, spatial, and depth map.
- Final behavior-linked UX parity (content mode, masking, border, projection fidelity).
