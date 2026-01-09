#!/bin/bash
# Build the MCP server for all platforms

set -e

echo "Building UE5 MCP Server..."

# Build for current platform (release)
cargo build --release

# The binary will be in target/release/ue5-mcp (or .exe on Windows)

# Copy to plugin binaries
PLUGIN_DIR="$(dirname "$0")/.."
mkdir -p "$PLUGIN_DIR/Binaries"

if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    cp target/release/ue5-mcp.exe "$PLUGIN_DIR/Binaries/"
    echo "Built: $PLUGIN_DIR/Binaries/ue5-mcp.exe"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    cp target/release/ue5-mcp "$PLUGIN_DIR/Binaries/ue5-mcp-mac"
    echo "Built: $PLUGIN_DIR/Binaries/ue5-mcp-mac"
else
    cp target/release/ue5-mcp "$PLUGIN_DIR/Binaries/ue5-mcp-linux"
    echo "Built: $PLUGIN_DIR/Binaries/ue5-mcp-linux"
fi

echo "Done!"
