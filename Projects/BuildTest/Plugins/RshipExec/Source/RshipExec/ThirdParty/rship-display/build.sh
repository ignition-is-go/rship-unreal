#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

cargo build --release -p rship-display-ffi
cargo build --release -p rship-display-cli

echo "Built rship-display FFI and CLI artifacts in $ROOT_DIR/target/release"
