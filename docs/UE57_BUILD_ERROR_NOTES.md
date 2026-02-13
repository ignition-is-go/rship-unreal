# UE 5.7 Build Error Notepad

Purpose: capture the exact build failures hit during plugin packaging and the fixes that worked, so we do not repeat them.

## Environment
- Engine: UE 5.7 (Mac, arm64+x64)
- Build flow: `./scripts/package-plugin.sh 5.7 ./dist`
- Targets validated: `UnrealEditor` + `UnrealGame` (`Development` + `Shipping`)

## Error Patterns, Fixes, Guardrails

1. `fatal error: 'Containers/DoubleLinkedList.h' file not found`
- Root cause: wrong include for `TDoubleLinkedList` on UE 5.7.
- Fix: use `#include "Containers/List.h"` in `Util.h`.
- Guardrail: verify container headers against engine source before changing includes.

2. `no matching function ... const FName -> const TCHAR*` in metadata parsing
- Root cause: lambdas in PCG metadata parser accepted `const TCHAR*`, but metadata keys were `FName`.
- Fix: switch helpers to take `const FName&`; convert literal keys via `FName(TEXT("..."))`.
- Guardrail: keep metadata key types consistent (`FName` end-to-end).

3. `FMulticastDelegateProperty` no `GetPropertyValue_InContainer` / `SetPropertyValue_InContainer`
- Root cause: delegate property API usage drifted.
- Fix: use `EmitterProp->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Obj)` and mutate through pointer.
- Guardrail: for delegate properties, prefer `ContainerPtrToValuePtr` accessors.

4. `no member named 'GetActorLabel'` in runtime builds
- Root cause: editor-facing label API used in runtime path.
- Fix: use `GetActorNameOrLabel()` or wrap editor-only label calls with `#if WITH_EDITOR`.
- Guardrail: avoid raw `GetActorLabel()` unless editor-gated.

5. `unknown type name 'FTimerHandle'`
- Root cause: missing timer header in public headers.
- Fix: add `#include "TimerManager.h"` where `FTimerHandle` appears.
- Guardrail: do not rely on transitive includes for timer types.

6. `use of undeclared identifier 'FJsonObject'` in headers
- Root cause: missing forward declaration in public header.
- Fix: add `class FJsonObject;` (or include Json headers in cpp where needed).
- Guardrail: forward-declare JSON types in headers; include full headers in implementation files.

7. `TFieldIterator` / `CastField` / `CPF_Parm` undeclared in `SchemaHelpers.cpp`
- Root cause: missing reflection includes.
- Fix: include `UObject/FieldIterator.h` and `UObject/UnrealType.h`.
- Guardrail: files doing reflection iteration must include reflection headers explicitly.

8. Material/mesh incomplete-type errors in fixture visualizer
- Examples: `UMaterialInterface` conversion complaints, `UStaticMesh` incomplete in `FObjectFinder`.
- Root cause: missing concrete engine includes.
- Fix: include `Materials/Material.h` and `Engine/StaticMesh.h`.
- Guardrail: when template/static load or class conversion fails with forward-declare notes, add concrete headers.

9. Runtime target compiling editor-only procedural material authoring (`UMaterial` expression graph APIs)
- Examples: `MD_Surface`/`GetExpressionCollection`/`GetEditorOnlyData`/`PreEditChange` failures.
- Root cause: editor-only APIs compiled in runtime target.
- Fix: wrap fallback material authoring in `#if WITH_EDITOR`; runtime fallback uses `DefaultMaterial`.
- Guardrail: keep material graph construction strictly editor-only.

10. `FTexture2DMipMap` incomplete type when writing texture mip bulk data
- Root cause: missing `TextureResource.h`.
- Fix: include `TextureResource.h`.
- Guardrail: direct mip/bulk writes require texture resource headers.

11. Packaging output created under engine tree + zip failed (`Nothing to do!`)
- Root cause: relative `-Package` path interpreted from UAT working directory.
- Fix: normalize output path to absolute in `scripts/package-plugin.sh` before calling UAT.
- Guardrail: always pass absolute package paths to UAT in scripts.

12. Build artifacts polluting git status
- Root cause: `dist/` was not ignored.
- Fix: add `dist/` to `.gitignore`.
- Guardrail: keep packaging outputs ignored to avoid accidental commits.

13. `unterminated conditional directive` in `SRship2110MappingPanel.cpp`
- Root cause: missing closing `#endif` for `#if !RSHIP_EDITOR_HAS_2110` block in `RefreshStreams()`.
- Fix: add the missing `#endif` before function close.
- Guardrail: when touching compatibility guards, always verify every `#if/#else/#endif` pair in edited scope.

14. `error: version control conflict marker in file` (`<<<<<<< HEAD`)
- Root cause: unresolved merge markers left in `SRship2110MappingPanel.cpp` during branch integration.
- Fix: remove conflict markers and keep a single resolved implementation path.
- Guardrail: run a conflict-marker scan before packaging:
  - `rg -n "^(<<<<<<<|=======|>>>>>>>)" Plugins/RshipExec/Source`

## Fast Preflight Checklist (before pushing build-related changes)
- Confirm headers use UE 5.7-valid include paths.
- Check runtime code paths for editor-only API usage.
- Ensure public headers forward-declare where possible and include required engine headers where necessary.
- Run packaging script once end-to-end:
  - `./scripts/package-plugin.sh 5.7 ./dist`
- Run conflict-marker scan:
  - `rg -n "^(<<<<<<<|=======|>>>>>>>)" Plugins/RshipExec/Source`
- Verify artifact exists:
  - `dist/RshipExec-5.7-Mac.zip`
