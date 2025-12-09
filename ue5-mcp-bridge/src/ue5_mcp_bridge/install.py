#!/usr/bin/env python3
"""
One-click installer for UE5 MCP Bridge.
Automatically configures Claude Desktop and Claude Code.
"""

import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def get_claude_desktop_config_path() -> Path:
    """Get the Claude Desktop config file path based on OS."""
    system = platform.system()

    if system == "Darwin":  # macOS
        return Path.home() / "Library" / "Application Support" / "Claude" / "claude_desktop_config.json"
    elif system == "Windows":
        return Path(os.environ.get("APPDATA", "")) / "Claude" / "claude_desktop_config.json"
    elif system == "Linux":
        return Path.home() / ".config" / "Claude" / "claude_desktop_config.json"
    else:
        raise RuntimeError(f"Unsupported platform: {system}")


def get_claude_code_config_path() -> Path:
    """Get the Claude Code config file path based on OS."""
    system = platform.system()

    if system == "Darwin":  # macOS
        return Path.home() / ".claude" / "settings.json"
    elif system == "Windows":
        return Path(os.environ.get("USERPROFILE", "")) / ".claude" / "settings.json"
    elif system == "Linux":
        return Path.home() / ".claude" / "settings.json"
    else:
        raise RuntimeError(f"Unsupported platform: {system}")


def find_python_executable() -> str:
    """Find the Python executable path."""
    # Try to find the installed ue5-mcp command
    ue5_mcp_path = shutil.which("ue5-mcp")
    if ue5_mcp_path:
        return ue5_mcp_path

    # Fall back to python -m
    return sys.executable


def get_mcp_server_config() -> dict:
    """Get the MCP server configuration for Claude."""
    python_exe = sys.executable

    # Check if we're installed as a package
    ue5_mcp_path = shutil.which("ue5-mcp")

    if ue5_mcp_path:
        return {
            "command": ue5_mcp_path,
            "args": [],
            "env": {
                "UE5_RPC_URL": "http://localhost:7777/rpc"
            }
        }
    else:
        return {
            "command": python_exe,
            "args": ["-m", "ue5_mcp_bridge.server"],
            "env": {
                "UE5_RPC_URL": "http://localhost:7777/rpc"
            }
        }


def install_claude_desktop():
    """Install MCP server configuration for Claude Desktop."""
    config_path = get_claude_desktop_config_path()

    print(f"Claude Desktop config: {config_path}")

    # Create directory if it doesn't exist
    config_path.parent.mkdir(parents=True, exist_ok=True)

    # Load existing config or create new
    if config_path.exists():
        with open(config_path, "r") as f:
            try:
                config = json.load(f)
            except json.JSONDecodeError:
                config = {}
    else:
        config = {}

    # Ensure mcpServers key exists
    if "mcpServers" not in config:
        config["mcpServers"] = {}

    # Add our server config
    config["mcpServers"]["ue5-control"] = get_mcp_server_config()

    # Write config
    with open(config_path, "w") as f:
        json.dump(config, f, indent=2)

    print("  Added 'ue5-control' MCP server to Claude Desktop")
    return True


def install_claude_code():
    """Install MCP server configuration for Claude Code CLI."""
    # Claude Code uses a different config mechanism - we use the claude mcp add command
    ue5_mcp_path = shutil.which("ue5-mcp")

    if ue5_mcp_path:
        cmd = ["claude", "mcp", "add", "ue5-control", "--", ue5_mcp_path]
    else:
        cmd = ["claude", "mcp", "add", "ue5-control", "--", sys.executable, "-m", "ue5_mcp_bridge.server"]

    print(f"Running: {' '.join(cmd)}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print("  Added 'ue5-control' MCP server to Claude Code")
            return True
        else:
            # Try alternative approach - direct config edit
            print(f"  claude mcp add failed, trying direct config...")
            return install_claude_code_direct()
    except FileNotFoundError:
        print("  'claude' command not found - is Claude Code installed?")
        print("  Trying direct config edit...")
        return install_claude_code_direct()


def install_claude_code_direct():
    """Directly edit Claude Code settings.json."""
    config_path = get_claude_code_config_path()

    print(f"Claude Code config: {config_path}")

    # Create directory if it doesn't exist
    config_path.parent.mkdir(parents=True, exist_ok=True)

    # Load existing config or create new
    if config_path.exists():
        with open(config_path, "r") as f:
            try:
                config = json.load(f)
            except json.JSONDecodeError:
                config = {}
    else:
        config = {}

    # Ensure mcpServers key exists
    if "mcpServers" not in config:
        config["mcpServers"] = {}

    # Add our server config
    server_config = get_mcp_server_config()
    config["mcpServers"]["ue5-control"] = {
        "command": server_config["command"],
        "args": server_config.get("args", []),
        "env": server_config.get("env", {})
    }

    # Write config
    with open(config_path, "w") as f:
        json.dump(config, f, indent=2)

    print("  Added 'ue5-control' MCP server to Claude Code settings")
    return True


def print_banner():
    """Print installation banner."""
    print()
    print("=" * 60)
    print("  UE5 MCP Bridge - One-Click Installer")
    print("  Control Unreal Engine with AI Assistants")
    print("=" * 60)
    print()


def print_success():
    """Print success message."""
    print()
    print("=" * 60)
    print("  Installation Complete!")
    print("=" * 60)
    print()
    print("Next steps:")
    print()
    print("1. UNREAL ENGINE:")
    print("   - Copy the UltimateControl plugin to your project's Plugins/ folder")
    print("   - Or enable it in your .uproject file")
    print("   - Build and launch the editor")
    print("   - The HTTP server starts automatically on port 7777")
    print()
    print("2. CLAUDE DESKTOP:")
    print("   - Restart Claude Desktop to load the MCP server")
    print("   - You should see 100+ UE5 tools available")
    print()
    print("3. CLAUDE CODE:")
    print("   - Run 'claude' in your terminal")
    print("   - The MCP server should connect automatically")
    print()
    print("Test it:")
    print("   Ask Claude: 'List all actors in my Unreal Engine level'")
    print()


def install_claude():
    """Main installation entry point."""
    print_banner()

    success = True

    print("Installing for Claude Desktop...")
    try:
        install_claude_desktop()
    except Exception as e:
        print(f"  Warning: {e}")
        success = False

    print()
    print("Installing for Claude Code...")
    try:
        install_claude_code()
    except Exception as e:
        print(f"  Warning: {e}")
        success = False

    if success:
        print_success()
    else:
        print()
        print("Some installations had warnings. Check the output above.")
        print("You may need to configure Claude manually.")

    return 0 if success else 1


def main():
    """CLI entry point."""
    sys.exit(install_claude())


if __name__ == "__main__":
    main()
