#!/bin/bash
# Package RshipExec plugin for distribution
# Usage: ./scripts/package-plugin.sh [ue-version] [output-dir]
#
# Examples:
#   ./scripts/package-plugin.sh 5.6 ./dist
#   ./scripts/package-plugin.sh 5.5 /path/to/output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PLUGIN_DIR="$REPO_ROOT/Plugins/RshipExec"

UE_VERSION="${1:-5.6}"
OUTPUT_DIR="${2:-$REPO_ROOT/dist}"

# Detect platform and UE installation
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="Mac"
    UE_ROOT="/Users/Shared/Epic Games/UE_$UE_VERSION"
    UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    PLATFORM="Win64"
    UE_ROOT="C:/Program Files/Epic Games/UE_$UE_VERSION"
    UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.bat"
else
    PLATFORM="Linux"
    UE_ROOT="/opt/UnrealEngine/UE_$UE_VERSION"
    UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
fi

# Verify UE installation
if [ ! -f "$UAT" ]; then
    echo "Error: Unreal Engine $UE_VERSION not found at $UE_ROOT"
    echo "Please install UE $UE_VERSION or specify correct path"
    exit 1
fi

# Verify plugin exists
if [ ! -f "$PLUGIN_DIR/RshipExec.uplugin" ]; then
    echo "Error: RshipExec.uplugin not found at $PLUGIN_DIR"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"
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
    zip -r "$ZIP_NAME" "RshipExec-$UE_VERSION-$PLATFORM"
    echo "Created: $OUTPUT_DIR/$ZIP_NAME"
fi
