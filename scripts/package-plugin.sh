#!/bin/bash
# Package RshipExec plugin for distribution
# Usage: ./scripts/package-plugin.sh [ue-version] [output-dir]
#
# Examples:
#   ./scripts/package-plugin.sh 5.7 ./dist
#   ./scripts/package-plugin.sh 5.7 /path/to/output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PLUGIN_DIR="$REPO_ROOT/Plugins/RshipExec"

UE_VERSION_REQUEST="${1:-}"
OUTPUT_DIR="${2:-$REPO_ROOT/dist}"

detect_ue_version_from_root() {
    local build_file="$1/Engine/Build/Build.version"
    if [[ ! -f "$build_file" ]]; then
        return 1
    fi

    local major minor
    major=$(grep -o '"MajorVersion"[[:space:]]*:[[:space:]]*[0-9][0-9]*' "$build_file" | head -1 | tr -dc '0-9')
    minor=$(grep -o '"MinorVersion"[[:space:]]*:[[:space:]]*[0-9][0-9]*' "$build_file" | head -1 | tr -dc '0-9')

    if [[ -z "$major" || -z "$minor" ]]; then
        return 1
    fi

    echo "$major.$minor"
}

latest_local_ue_root() {
    local search_roots=(
        "/Users/Shared/Epic Games"
        "/Users/Shared/EpicGames"
    )
    local latest

    for root in "${search_roots[@]}"; do
        [[ -d "$root" ]] || continue
        latest="$(find "$root" -maxdepth 1 -type d -name 'UE_[0-9]*.[0-9]*' 2>/dev/null | sort -V | tail -n 1)"
        if [[ -n "$latest" ]]; then
            echo "$latest"
            return 0
        fi
    done
    return 1
}

# Detect platform and UE installation.
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="Mac"
    if [[ -z "${UE_ROOT:-}" ]]; then
        if [[ -n "${UE_VERSION_REQUEST}" && -d "/Users/Shared/Epic Games/UE_$UE_VERSION_REQUEST" ]]; then
            UE_ROOT="/Users/Shared/Epic Games/UE_$UE_VERSION_REQUEST"
        elif [[ -n "${UE_VERSION_REQUEST}" && -d "/Users/Shared/EpicGames/UE_$UE_VERSION_REQUEST" ]]; then
            UE_ROOT="/Users/Shared/EpicGames/UE_$UE_VERSION_REQUEST"
        else
            UE_ROOT="$(latest_local_ue_root || true)"
            UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.6}"
        fi
    fi
    UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    PLATFORM="Win64"
    UE_ROOT="${UE_ROOT:-C:/Program Files/Epic Games/UE_${UE_VERSION_REQUEST:-5.6}}"
    UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.bat"
else
    PLATFORM="Linux"
    UE_ROOT="${UE_ROOT:-/opt/UnrealEngine/UE_${UE_VERSION_REQUEST:-5.6}}"
    UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
fi

DETECTED_UE_VERSION="$(detect_ue_version_from_root "$UE_ROOT" || true)"

if [[ -n "$UE_VERSION_REQUEST" ]]; then
    UE_VERSION="$UE_VERSION_REQUEST"
else
    UE_VERSION="${DETECTED_UE_VERSION:-5.6}"
fi

if [[ -n "$DETECTED_UE_VERSION" ]]; then
    echo "Detected UE version: $DETECTED_UE_VERSION"
fi

# Verify UE installation
if [ ! -f "$UAT" ]; then
    echo "Error: Unreal Engine $UE_VERSION not found at $UE_ROOT"
    echo "Please install UE $UE_VERSION or set UE_ROOT to your engine path"
    exit 1
fi

# Verify plugin exists
if [ ! -f "$PLUGIN_DIR/RshipExec.uplugin" ]; then
    echo "Error: RshipExec.uplugin not found at $PLUGIN_DIR"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
OUTPUT_PATH="$OUTPUT_DIR/RshipExec-$UE_VERSION-$PLATFORM"

echo "=========================================="
echo "Packaging RshipExec Plugin"
echo "=========================================="
echo "UE Version:  $UE_VERSION"
echo "Platform:    $PLATFORM"
echo "Plugin:      $PLUGIN_DIR"
echo "Output:      $OUTPUT_PATH"
echo "=========================================="

# Run UAT BuildPlugin command
"$UAT" BuildPlugin \
    -Plugin="$PLUGIN_DIR/RshipExec.uplugin" \
    -Package="$OUTPUT_PATH" \
    -TargetPlatforms="$PLATFORM" \
    -Rocket \
    -VS2022

echo ""
echo "=========================================="
echo "Plugin packaged successfully!"
echo "Output: $OUTPUT_PATH"
echo "=========================================="

# Create zip archive
if command -v zip &> /dev/null; then
    cd "$OUTPUT_DIR"
    ZIP_NAME="RshipExec-$UE_VERSION-$PLATFORM.zip"
    rm -f "$ZIP_NAME"
    if [ -d "RshipExec-$UE_VERSION-$PLATFORM" ]; then
        zip -r "$ZIP_NAME" "RshipExec-$UE_VERSION-$PLATFORM"
        echo "Created: $OUTPUT_DIR/$ZIP_NAME"
    else
        echo "Warning: package directory not found for zip: $OUTPUT_PATH"
    fi
fi
