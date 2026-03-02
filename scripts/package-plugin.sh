#!/bin/bash
# Package Rocketship plugins for distribution
# Usage: ./scripts/package-plugin.sh [ue-version] [output-dir]
#
# Examples:
#   ./scripts/package-plugin.sh 5.7 ./dist
#   ./scripts/package-plugin.sh 5.7 /path/to/output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PLUGIN_EXEC_DIR="$REPO_ROOT/Plugins/RshipExec"
PLUGIN_MAPPING_DIR="$REPO_ROOT/Plugins/RshipMapping"

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

# Verify plugins exist
if [ ! -f "$PLUGIN_EXEC_DIR/RshipExec.uplugin" ]; then
    echo "Error: RshipExec.uplugin not found at $PLUGIN_EXEC_DIR"
    exit 1
fi
if [ ! -f "$PLUGIN_MAPPING_DIR/RshipMapping.uplugin" ]; then
    echo "Error: RshipMapping.uplugin not found at $PLUGIN_MAPPING_DIR"
    exit 1
fi

# Verify content-mapping runtime readiness before packaging.
if [ -x "$SCRIPT_DIR/verify-content-mapping-readiness.sh" ]; then
    "$SCRIPT_DIR/verify-content-mapping-readiness.sh"
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
EXEC_OUTPUT_PATH="$OUTPUT_DIR/RshipExec-$UE_VERSION-$PLATFORM"
MAPPING_OUTPUT_PATH="$OUTPUT_DIR/RshipMapping-$UE_VERSION-$PLATFORM"
BUNDLE_OUTPUT_PATH="$OUTPUT_DIR/RshipPlugins-$UE_VERSION-$PLATFORM"

echo "=========================================="
echo "Packaging Rocketship Plugins"
echo "=========================================="
echo "UE Version:  $UE_VERSION"
echo "Platform:    $PLATFORM"
echo "RshipExec:   $PLUGIN_EXEC_DIR"
echo "RshipMapping:$PLUGIN_MAPPING_DIR"
echo "Output:      $BUNDLE_OUTPUT_PATH"
echo "=========================================="

# Run UAT BuildPlugin command for RshipExec.
"$UAT" BuildPlugin \
    -Plugin="$PLUGIN_EXEC_DIR/RshipExec.uplugin" \
    -Package="$EXEC_OUTPUT_PATH" \
    -TargetPlatforms="$PLATFORM" \
    -Rocket \
    -VS2022

# Run UAT BuildPlugin command for RshipMapping.
"$UAT" BuildPlugin \
    -Plugin="$PLUGIN_MAPPING_DIR/RshipMapping.uplugin" \
    -Package="$MAPPING_OUTPUT_PATH" \
    -TargetPlatforms="$PLATFORM" \
    -Rocket \
    -VS2022

# Build one-click project install bundle: drop this folder into <Project>/Plugins.
rm -rf "$BUNDLE_OUTPUT_PATH"
mkdir -p "$BUNDLE_OUTPUT_PATH/Plugins"
cp -R "$EXEC_OUTPUT_PATH" "$BUNDLE_OUTPUT_PATH/Plugins/RshipExec"
cp -R "$MAPPING_OUTPUT_PATH" "$BUNDLE_OUTPUT_PATH/Plugins/RshipMapping"

echo ""
echo "=========================================="
echo "Plugins packaged successfully!"
echo "Exec Output:    $EXEC_OUTPUT_PATH"
echo "Mapping Output: $MAPPING_OUTPUT_PATH"
echo "Bundle Output:  $BUNDLE_OUTPUT_PATH"
echo "=========================================="

# Create zip archive
if command -v zip &> /dev/null; then
    cd "$OUTPUT_DIR"
    EXEC_ZIP_NAME="RshipExec-$UE_VERSION-$PLATFORM.zip"
    MAPPING_ZIP_NAME="RshipMapping-$UE_VERSION-$PLATFORM.zip"
    BUNDLE_ZIP_NAME="RshipPlugins-$UE_VERSION-$PLATFORM.zip"
    rm -f "$EXEC_ZIP_NAME" "$MAPPING_ZIP_NAME" "$BUNDLE_ZIP_NAME"

    if [ -d "RshipExec-$UE_VERSION-$PLATFORM" ]; then
        zip -r "$EXEC_ZIP_NAME" "RshipExec-$UE_VERSION-$PLATFORM"
        echo "Created: $OUTPUT_DIR/$EXEC_ZIP_NAME"
    fi
    if [ -d "RshipMapping-$UE_VERSION-$PLATFORM" ]; then
        zip -r "$MAPPING_ZIP_NAME" "RshipMapping-$UE_VERSION-$PLATFORM"
        echo "Created: $OUTPUT_DIR/$MAPPING_ZIP_NAME"
    fi
    if [ -d "RshipPlugins-$UE_VERSION-$PLATFORM" ]; then
        zip -r "$BUNDLE_ZIP_NAME" "RshipPlugins-$UE_VERSION-$PLATFORM"
        echo "Created: $OUTPUT_DIR/$BUNDLE_ZIP_NAME"
    fi
fi
