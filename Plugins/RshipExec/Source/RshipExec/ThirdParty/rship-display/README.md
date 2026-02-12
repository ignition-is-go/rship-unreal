# rship-display

Portable display management libraries for:
- Unreal plugin integration (`RshipExec`)
- standalone Rust CLI/runtime

## Crates
- `rship-display-core`: data model, validation, identity matching, planning, pixel-ledger generation
- `rship-display-windows`: Windows snapshot/apply adapters
- `rship-display-ffi`: C ABI bridge for Unreal C++
- `rship-display-cli`: standalone command-line operations

## Build
```bash
./build.sh
```

On Windows:
```bat
build.bat
```

Artifacts:
- static lib: `target/release/rship_display.lib` (Win) or `target/release/librship_display.a` (Unix)
- CLI: `target/release/rship-display-cli`
- generated header: `include/rship_display.h`

## Examples
- `examples/snapshot.example.json`
- `examples/profile.example.json`

## CLI Highlights
- `plan` returns `plan`, `identity`, `validation`, and `ledger` payload sections.
- `ledger` builds a standalone pixel-route ledger (`source rect -> canonical display -> observed display`).
