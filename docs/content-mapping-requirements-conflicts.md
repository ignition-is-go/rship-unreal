# Content Mapping Requirements Conflict Ledger (Code-First)

Date: 2026-03-01

This ledger records doc/code conflicts found during canonical requirements enumeration.

Resolution policy for this pass:
1. Canonical requirements follow current code.
2. This file records the doc delta and recommended remediation.
3. No runtime/editor code behavior is changed by this document.

## Conflicts

| Priority | doc_claim | code_evidence | impact | recommended_fix |
|---|---|---|---|---|
| High | `docs/content-mapping-material-contract-implementation-plan.md` claims broad mandatory scalar/vector/texture contract enforcement, including many required parameters. | `ValidateMaterialContract` only enforces `RequiredTextures = { RshipContextTexture }` and leaves required scalars/vectors empty in [`/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp:8186`](/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp:8186). | Engineers may assume strict contract validation exists and miss runtime behavior gaps that rely on optional params. | Doc update (recommended): clearly mark current runtime contract as minimal; add explicit future-work section for strict contract mode if desired. |
| High | `docs/content-mapping-performance-overhaul-master-plan.md` section 23.2 states `rship.cm.capture_quality_profile` default is `0` (performance). | CVar declaration default is `1` in [`/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp:111`](/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp:111). | Incorrect perf tuning assumptions and inconsistent benchmark baselines. | Doc update (recommended): change documented default to `1` (balanced), or explicitly note branch/version where `0` was used. |
| High | `docs/content-mapping-performance-overhaul-master-plan.md` documents `rship.cm.feed_single_rt_mode` and `TryResolveFeedSingleRtBinding(...)` as implemented default path. | No `feed_single_rt_mode` CVar or `TryResolveFeedSingleRtBinding` symbol exists in current mapping runtime file; active path uses `BuildFeedCompositeTextureForSurface(...)` compositor in [`/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp:5211`](/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp:5211). | Future optimizations may be planned against a non-existent runtime path. | Doc update (recommended): move single-RT path to historical note or mark as removed/superseded. |
| Medium | Multiple docs still point to legacy `RshipExec` file paths for content-mapping implementation (for example file maps and UX file targets). | Active implementation is in `Plugins/RshipMapping/...`; `RshipExec` mapping sources are deleted in branch state; manager class path resolves to `/Script/RshipMapping.RshipContentMappingManager` in [`/Users/lucid/rship-unreal/Plugins/RshipExec/Source/RshipExec/Private/RshipSubsystem.cpp:3126`](/Users/lucid/rship-unreal/Plugins/RshipExec/Source/RshipExec/Private/RshipSubsystem.cpp:3126). | Reviewers and implementers inspect stale files and miss current behavior. | Doc update (recommended): replace legacy file targets with `Plugins/RshipMapping/Source/...` paths. |
| Medium | `docs/content-mapping-ux-parity-matrix.md` references external absolute paths under `/Users/nicholasfletcher/.../RshipExecEditor/...`. | Current workspace implementation paths are under `/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMappingEditor/...`. | Broken traceability and incorrect code review pointers. | Doc update (recommended): normalize all path references to current workspace and `RshipMappingEditor`. |
| Medium | `docs/content-mapping-speed-ux-audit.md` file targets still reference `RshipExecEditor` sources. | Active editor panel/canvas/mode selectors are in `RshipMappingEditor` (`SRshipContentMappingPanel.cpp`, `SRshipMappingCanvas.cpp`, `SRshipModeSelector.cpp`, `SRshipContentModeSelector.cpp`). | Implementation tasks can be misdirected to stale/deleted locations. | Doc update (recommended): migrate file-target section to current `RshipMappingEditor` paths. |
| Medium | Deterministic node-pipeline architecture target includes full blend/composite compile coverage in v1. | Current compile bridge intentionally fails closed on blend/composite nodes with diagnostic `pipeline.compile.blend_unsupported` in [`/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp:3814`](/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp:3814). | Teams may assume blend/composite graphs execute when they currently reject at compile time. | Doc update (recommended): mark blend/composite compile as explicit follow-up milestone; keep fail-closed behavior documented as current state. |
| Low | `docs/content-mapping-material-contract-implementation-plan.md` execution log reports only 3 automation tests as current set. | Current tests file defines 19 content-mapping automation tests in [`/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/Tests/RshipContentMappingManagerTests.cpp:1700`](/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/Tests/RshipContentMappingManagerTests.cpp:1700). | Testing expectations may be understated in planning docs. | Doc update (recommended): refresh test inventory and map each test to requirement IDs. |
| Low | Historical docs describe implementation file map under `Plugins/RshipExec/Source/RshipExec/...` for manager internals. | Current manager implementation is in [`/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp`](/Users/lucid/rship-unreal/Plugins/RshipMapping/Source/RshipMapping/Private/RshipContentMappingManager.cpp). | New contributors may not find active code quickly. | Doc update (recommended): convert file map sections to plugin-split architecture (`RshipMapping` runtime/editor + `RshipExec` bridge only). |

## Suggested Remediation Order
1. Fix high-priority runtime-behavior conflicts (contract strictness and CVar defaults/path claims).
2. Fix stale path references across all mapping docs.
3. Update test inventory sections to match current 8-test suite.

## Guardrail
When a doc describes behavior not present in code, add one of:
- `Current behavior (code)`
- `Target behavior (not implemented yet)`

This prevents future plans from mixing shipped and aspirational behavior.
