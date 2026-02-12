# Content Mapping UX Scratchpad

Last updated: 2026-02-11 20:53:14 EST

## Working (Confirmed)
- Editor panel has full top-level structure: quick create, inputs, screens, mappings, canvas preview.
- Mapping mode selector includes direct, feed, perspective, camera plate, cylindrical, spherical, parallel, radial, spatial, mesh, fisheye, depth map.
- Content mode selector exists (stretch, crop, fit, pixel-perfect).
- Feed rectangle editing exists in both numeric controls and interactive canvas.
- Projection edit gizmo flow exists (`Edit Projection` / `Stop Projection Edit`).
- Inline CRUD exists for inputs, screens, and mappings.
- Lists are now sorted by display name (fallback to ID) for inputs/screens/mappings.
- Filters now exist for Inputs, Screens, and Mappings.
- Each filter now has a dedicated `Clear` action.
- Each list now supports `Errors`-only filtering for triage.
- Count display now reflects filtered visibility (`visible/total`) when filters are active.
- Bulk actions now exist on filtered results for Inputs/Screens/Mappings (enable/disable visible).
- Mapping inline rows now support collapsing advanced config (`Show Config` / `Hide Config`) to reduce visual clutter.
- Mapping list now has a direct `Duplicate` action for rapid branching.
- Input and Screen rows now have direct `Edit` actions to jump into the dedicated form state.
- Lists now support persistent row selection with explicit checkboxes for Inputs, Screens, and Mappings.
- Bulk actions now scope to selected rows when selection exists (fallback: visible rows).
- Feed mapping supports table copy/paste/reset workflows for per-screen feed rectangles.
- Mappings now have `Find/Replace Usages` controls for both Input IDs and Screen IDs (selected-or-visible scope).
- Mode selector now exposes Camera Plate, Spatial, and Depth Map as first-class options (projection-family path).
- Focused object compile checks pass for touched files (`SRshipContentMappingPanel.cpp`, `SRshipModeSelector.cpp`, `RshipContentMappingManager.cpp`) for both `arm64` and `x64`.

## Not Working / UX Gaps (Confirmed)
- UX parity with Disguise manager workflow is incomplete for high-density mapping operations.
- Camera Plate / Spatial / Depth Map now exist as first-class modes, but their dedicated property coverage and runtime behavior are still partial.
- Common mapping properties from manual are still incomplete (for example filtering/masking variants and full spatial/camera-plate/depth-map property coverage).
- Some high-volume operations are still editor-form heavy compared to Disguise manager ergonomics (bulk workflows now better but still partial vs manager-level tooling).

## In Progress
- Build UX parity matrix against Disguise manager workflows (step-by-step).
- Complete dedicated Camera Plate / Spatial / Depth Map property forms and runtime behavior.
- Validate `Find/Replace Usages` behavior against manager semantics from manual.

## Next Checks
- Validate filter behavior with inline-create and inline-edit rows.
- Verify keyboard/text workflows for map-heavy sessions.
- Continue parity matrix document with explicit Disguise workflow mapping.
- Re-run full package build/link pass after local UBT executor instability is resolved (current env repeatedly hangs before reliable final status emission).
