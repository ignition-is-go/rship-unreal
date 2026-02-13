# Content Mapping Speed UX Audit ("Slick as Hell")

Last updated: 2026-02-13

## Goal
Design for fastest path to a correct mapping with low cognitive load.

This is not a "can it be done" audit.
This is a "can a power user do it fast, repeatedly, without friction" audit.

## Success Criteria
- Time-to-first-map under 20 seconds from opening panel.
- Time-to-duplicate-and-adjust map under 8 seconds.
- Bulk apply (opacity/enable/source/screen replacement) under 3 interactions.
- Zero mandatory mode/context switching for common operations.
- No hidden state surprises (selection scope, filter scope, save scope always explicit).

## Current Friction (High Impact)
1. Create flow is still form-heavy for rapid repetitive work.
2. Inline rows are improved but still dense under large mapping counts.
3. Bulk operations exist but are not yet "command-palette fast."
4. Advanced mode controls are available, but not progressively revealed by task intent.
5. Keyboard-first workflow is partial; mouse travel is still too high.

## "Slick" Interaction Model
1. Command-first create:
`Cmd/Ctrl+K` -> "Map input to screen" -> choose input -> choose screen -> Enter.
2. Single-row fast edit:
each row supports quick fields (enabled, opacity, source, target) without opening full form.
3. Progressive detail:
row defaults to compact; advanced controls only on explicit expand.
4. Scope certainty:
bulk action bar always states target scope: `Selected (N)` or `Visible (N)`.
5. One-keystroke duplication:
`D` duplicates selected mapping and focuses first editable field.

## Priority Backlog (Order Matters)

## P0: Speed Path (must-do)
1. Add command palette actions for:
- Create mapping
- Duplicate mapping
- Replace input IDs
- Replace screen IDs
- Toggle enable/disable selected
2. Add keyboard bindings for common row actions:
- Duplicate
- Toggle enabled
- Focus opacity
- Expand/collapse config
3. Add "quick create presets" (last-used source/screen/mode defaults).
4. Keep "selected vs visible" scope locked and always visible in bulk toolbar.

## P1: Density and Readability
1. Compact row mode default for large lists (badge-lite, one-line summary).
2. Optional two-pane mode:
- left list, right sticky editor for current selection.
3. Visual hierarchy cleanup:
- stronger row selection highlight
- reduce badge noise
- status/error grouping instead of scattered labels.

## P2: Workflow Polish
1. Multi-step edit macro:
`Duplicate -> rename -> rebind input -> apply`.
2. Better empty/no-match states with direct recovery actions.
3. Undo-friendly micro-actions for all inline edits.

## Measurement Plan
Track these metrics in-panel (local telemetry/debug counters):
- `time_to_first_mapping_ms`
- `actions_per_mapping_create`
- `bulk_operation_count_per_session`
- `open_full_form_rate` (lower is better if quick-edit is effective)
- `mapping_error_fix_time_ms`

## Acceptance Test Scenarios
1. Create 10 mappings to one screen from mixed sources in under 2 minutes.
2. Replace input ID across 40 selected mappings in under 15 seconds.
3. Duplicate one mapping to 8 screens with only quick-row operations.
4. Recover from bad filter state without clearing panel context.

## Immediate Next Sprint (Concrete)
1. Implement command palette hooks in `SRshipContentMappingPanel`.
2. Add keyboard shortcuts for row-level quick actions.
3. Add quick-create defaults panel ("Use last source/screen/mode").
4. Add compact-row visual mode toggle + persisted preference.
5. Add scope pill in bulk bar with explicit selection source.

## File Targets
- `Plugins/RshipExec/Source/RshipExecEditor/Private/SRshipContentMappingPanel.cpp`
- `Plugins/RshipExec/Source/RshipExecEditor/Private/SRshipMappingCanvas.cpp`
- `Plugins/RshipExec/Source/RshipExecEditor/Private/SRshipModeSelector.cpp`
- `Plugins/RshipExec/Source/RshipExecEditor/Private/SRshipContentModeSelector.cpp`

