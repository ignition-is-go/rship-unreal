#!/bin/bash
#
# UltimateControl MCP Server Installer
# One command to build and configure Claude Desktop/Code
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MCP_DIR="$SCRIPT_DIR/MCP"
BIN_DIR="$SCRIPT_DIR/Binaries"

echo "=============================================="
echo "  UltimateControl MCP Server Installer"
echo "=============================================="
echo

# Detect OS
OS="$(uname -s)"
case "$OS" in
    Darwin)
        PLATFORM="macos"
        BINARY_NAME="ue5-mcp"
        CLAUDE_DESKTOP_CONFIG="$HOME/Library/Application Support/Claude/claude_desktop_config.json"
        ;;
    Linux)
        PLATFORM="linux"
        BINARY_NAME="ue5-mcp"
        CLAUDE_DESKTOP_CONFIG="$HOME/.config/Claude/claude_desktop_config.json"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        PLATFORM="windows"
        BINARY_NAME="ue5-mcp.exe"
        CLAUDE_DESKTOP_CONFIG="$APPDATA/Claude/claude_desktop_config.json"
        ;;
    *)
        echo "Unsupported platform: $OS"
        exit 1
        ;;
esac

echo "Platform: $PLATFORM"
echo

# Check for pre-built binary
PREBUILT_BINARY="$BIN_DIR/$PLATFORM/$BINARY_NAME"
BINARY_PATH=""

if [ -f "$PREBUILT_BINARY" ]; then
    echo "Using pre-built binary: $PREBUILT_BINARY"
    BINARY_PATH="$PREBUILT_BINARY"
else
    # Build from source
    echo "No pre-built binary found. Building from source..."

    if ! command -v cargo &> /dev/null; then
        echo "Error: Rust/Cargo not found. Please install from https://rustup.rs"
        exit 1
    fi

    echo "Building MCP server..."
    cd "$MCP_DIR"
    cargo build --release

    # Copy binary to Binaries folder
    mkdir -p "$BIN_DIR/$PLATFORM"
    cp "target/release/$BINARY_NAME" "$BIN_DIR/$PLATFORM/"
    BINARY_PATH="$BIN_DIR/$PLATFORM/$BINARY_NAME"

    echo "Built: $BINARY_PATH"
fi

# Make executable
chmod +x "$BINARY_PATH"

echo
echo "Configuring Claude Desktop..."

# Create Claude config directory if needed
mkdir -p "$(dirname "$CLAUDE_DESKTOP_CONFIG")"

# Read or create config
if [ -f "$CLAUDE_DESKTOP_CONFIG" ]; then
    CONFIG=$(cat "$CLAUDE_DESKTOP_CONFIG")
else
    CONFIG='{}'
fi

# Add MCP server config using Python (available on most systems)
python3 << EOF
import json
import sys

config_path = "$CLAUDE_DESKTOP_CONFIG"
binary_path = "$BINARY_PATH"

# Load existing config
try:
    with open(config_path, 'r') as f:
        config = json.load(f)
except (FileNotFoundError, json.JSONDecodeError):
    config = {}

# Ensure mcpServers key exists
if 'mcpServers' not in config:
    config['mcpServers'] = {}

# Add our server
config['mcpServers']['ue5-control'] = {
    'command': binary_path,
    'args': [],
    'env': {}
}

# Write config
with open(config_path, 'w') as f:
    json.dump(config, f, indent=2)

print(f"  Added 'ue5-control' to {config_path}")
EOF

echo
echo "Configuring Claude Code..."

# Try to use claude mcp add command
if command -v claude &> /dev/null; then
    claude mcp add ue5-control -- "$BINARY_PATH" 2>/dev/null || echo "  (Using manual config)"
else
    echo "  Claude Code CLI not found, skipping automatic config"
    echo "  Run this manually: claude mcp add ue5-control -- $BINARY_PATH"
fi

echo
echo "=============================================="
echo "  Installation Complete!"
echo "=============================================="
echo
echo "Next steps:"
echo
echo "1. UNREAL ENGINE:"
echo "   - Copy this UltimateControl folder to your project's Plugins/ directory"
echo "   - Build and launch the editor"
echo "   - The HTTP server starts automatically on port 7777"
echo
echo "2. RESTART CLAUDE:"
echo "   - Restart Claude Desktop to load the MCP server"
echo "   - You should see 100+ UE5 tools available"
echo
echo "3. TEST IT:"
echo "   Ask Claude: 'List all actors in my Unreal Engine level'"
echo
echo "Binary location: $BINARY_PATH"
echo
