# Display Management Quickstart

This guide covers the new Windows-first deterministic display workflow in `RshipExec`.

## 1) Build the Rust Runtime (optional but recommended)

From repo root:

```bash
cd Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display
./build.sh
```

Windows:

```bat
cd Plugins\RshipExec\Source\RshipExec\ThirdParty\rship-display
build.bat
```

If the static library is missing, `RshipExec` compiles with fallback mode (`RSHIP_HAS_DISPLAY_RUST=0`).

## 2) CLI Usage

### Collect snapshot

```bash
cargo run --manifest-path Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/Cargo.toml -p rship-display-cli -- snapshot --pretty
```

### Build known display identities from snapshot

```bash
cargo run --manifest-path Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/Cargo.toml -p rship-display-cli -- build-known --snapshot snapshot.json --out known.json --pretty
```

### Resolve canonical identity

```bash
cargo run --manifest-path Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/Cargo.toml -p rship-display-cli -- resolve --known known.json --snapshot snapshot.json --out identity.json --pretty
```

### Validate/plan/apply

```bash
cargo run --manifest-path Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/Cargo.toml -p rship-display-cli -- validate --profile profile.json --snapshot snapshot.json --pretty
cargo run --manifest-path Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/Cargo.toml -p rship-display-cli -- plan --profile profile.json --snapshot snapshot.json --known known.json --pretty
cargo run --manifest-path Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/Cargo.toml -p rship-display-cli -- apply --plan plan.json --dry-run --pretty
```

`plan` now includes:
- `plan`: ordered apply steps
- `identity`: canonical-to-observed mapping
- `validation`: profile issues
- `ledger`: route-by-route pixel mapping (`source -> canonical dest -> observed dest`)

### Generate ledger only

```bash
cargo run --manifest-path Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/Cargo.toml -p rship-display-cli -- ledger --profile profile.json --snapshot snapshot.json --known known.json --pretty
```

Reference examples:
- `Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/examples/snapshot.example.json`
- `Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/examples/profile.example.json`

## 3) Unreal Runtime Commands

In UE console:

```text
rship.display.snapshot
rship.display.resolve
rship.display.validate
rship.display.plan
rship.display.apply
rship.display.debug
```

## 4) Runtime Integration

The display manager is available from:

- `URshipSubsystem::GetDisplayManager()`
- target path: `/display-management/system`

The manager emits JSON state payloads on:

- `state`, `status`, `snapshot`, `known`, `identity`, `validation`, `plan`, `ledger`, `apply`

## 5) Guarded Apply

By default, `bDisplayManagementGuardedApply=true` to avoid destructive topology mutation during rollout.

When set to `false`, apply uses Win32 `ChangeDisplaySettingsExW` to stage and commit topology updates from the planned `set-topology` step.

Configure in Project Settings > Rship Exec > Display Management.
