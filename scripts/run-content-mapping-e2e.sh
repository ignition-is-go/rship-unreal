#!/bin/bash
# Run the content-mapping PIE end-to-end automation gate on a target project.
# Usage:
#   ./scripts/run-content-mapping-e2e.sh --uproject "/abs/path/Project.uproject" [--map "/Game/Maps/Main"] [--engine-root "/Users/Shared/Epic Games/UE_5.7"] [--skip-build]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

UPROJECT_PATH=""
MAP_PATH="/Game/VprodProject/Maps/Main"
ENGINE_ROOT="/Users/Shared/Epic Games/UE_5.7"
TEST_NAME="Rship.ContentMapping.E2E.PIE"
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --uproject)
      UPROJECT_PATH="${2:-}"
      shift 2
      ;;
    --map)
      MAP_PATH="${2:-}"
      shift 2
      ;;
    --engine-root)
      ENGINE_ROOT="${2:-}"
      shift 2
      ;;
    --test)
      TEST_NAME="${2:-}"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 64
      ;;
  esac
done

if [[ -z "$UPROJECT_PATH" ]]; then
  echo "Missing --uproject argument." >&2
  exit 64
fi

if [[ ! -f "$UPROJECT_PATH" ]]; then
  echo "UProject not found: $UPROJECT_PATH" >&2
  exit 66
fi

EDITOR_BIN="$ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor"
BUILD_BIN="$ENGINE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"

if [[ ! -x "$EDITOR_BIN" ]]; then
  echo "UnrealEditor binary not found/executable: $EDITOR_BIN" >&2
  exit 66
fi

if [[ ! -x "$BUILD_BIN" ]]; then
  echo "Build script not found/executable: $BUILD_BIN" >&2
  exit 66
fi

PROJECT_NAME="$(basename "$UPROJECT_PATH" .uproject)"
EDITOR_TARGET="${PROJECT_NAME}Editor"
REPORT_DIR="$REPO_ROOT/Saved/Automation/ContentMappingE2E-$(date +%Y%m%d-%H%M%S)"
LOG_PATH="$REPORT_DIR/unreal-automation.log"
mkdir -p "$REPORT_DIR"

echo "== Content Mapping E2E =="
echo "UProject: $UPROJECT_PATH"
echo "Map: $MAP_PATH"
echo "Test: $TEST_NAME"
echo "Engine: $ENGINE_ROOT"
echo "Report: $REPORT_DIR"

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  echo "== Build =="
  "$BUILD_BIN" "$EDITOR_TARGET" Mac Development "$UPROJECT_PATH" -waitmutex
fi

echo "== Automation =="
"$EDITOR_BIN" "$UPROJECT_PATH" "$MAP_PATH" \
  -unattended -nop4 -nosplash -NoSound \
  "-ini:Engine:[SystemSettings]:CommonUI.Debug.CheckGameViewportClientValid=0" \
  -CommonUI.Debug.CheckGameViewportClientValid=0 \
  -stdout -FullStdOutLogOutput \
  -ExecCmds="Automation RunTests $TEST_NAME" \
  -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="$REPORT_DIR" \
  -RshipContentMappingE2EMap="$MAP_PATH" \
  -log="$LOG_PATH"

RESULT_CHECK=0
RESULT_SUMMARY="$(python3 - "$REPORT_DIR/index.json" <<'PY'
import json
import sys

path = sys.argv[1]

try:
    with open(path, "r", encoding="utf-8-sig") as f:
        report = json.load(f)
except Exception as exc:
    print(f"error=report-read-failed detail={exc}")
    sys.exit(3)

failed = int(report.get("failed", 0))
succeeded = int(report.get("succeeded", 0))
succeeded_with_warnings = int(report.get("succeededWithWarnings", 0))
not_run = int(report.get("notRun", 0))

print(
    f"succeeded={succeeded} "
    f"succeeded_with_warnings={succeeded_with_warnings} "
    f"failed={failed} not_run={not_run}"
)

if failed > 0:
    sys.exit(2)
PY
)" || RESULT_CHECK=$?

echo "Automation summary: $RESULT_SUMMARY"
if [[ "$RESULT_CHECK" -ne 0 ]]; then
  echo "FAIL: automation reported one or more failed tests." >&2
  echo "Report output: $REPORT_DIR"
  exit "$RESULT_CHECK"
fi

echo "PASS: automation completed."
echo "Report output: $REPORT_DIR"
