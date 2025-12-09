# UE5 MCP Bridge

Control Unreal Engine 5 with AI assistants like Claude. **One command to install.**

## Quick Start

### Step 1: Install & Configure (One Command)

```bash
# Using uvx (recommended - no install needed)
uvx ue5-mcp-bridge ue5-mcp-install

# Or using pip
pip install ue5-mcp-bridge
ue5-mcp-install
```

This automatically configures both **Claude Desktop** and **Claude Code**.

### Step 2: Enable the Unreal Plugin

Copy the `UltimateControl` plugin to your project's Plugins folder:

```
YourProject/
  Plugins/
    UltimateControl/    <-- Copy the entire folder here
```

Or add to your `.uproject` file:

```json
{
  "Plugins": [
    { "Name": "UltimateControl", "Enabled": true }
  ]
}
```

Build and launch the editor. The HTTP server starts automatically on **port 7777**.

### Step 3: Talk to Claude

Open Claude Desktop or Claude Code and try:

- *"List all actors in my Unreal level"*
- *"Spawn a point light at position 0, 0, 500"*
- *"Take a screenshot of the viewport"*
- *"Set the sun light intensity to 10"*
- *"Open the level /Game/Maps/MainLevel"*

## What You Can Do

**350+ methods** across 27 categories:

| Category | Examples |
|----------|----------|
| **Actors** | Spawn, destroy, transform, select, modify properties |
| **Assets** | List, search, create, import, export, duplicate |
| **Blueprints** | Inspect graphs, add variables, compile |
| **Viewport** | Camera control, screenshots, view modes |
| **Lighting** | Set intensity, color, build lighting |
| **Materials** | Get/set parameters, create instances |
| **Sequencer** | Control playback, scrub timeline, add tracks |
| **Physics** | Simulate, apply impulses, collision profiles |
| **Audio** | Play sounds, set volume |
| **Animation** | Play animations, list Animation Blueprints |
| **Navigation** | Build navmesh, find paths |
| **Source Control** | Check out, check in, revert, sync |
| **Niagara** | Spawn VFX, set parameters |
| **Landscape** | Query heights, list layers |
| **World Partition** | Load/unload cells, data layers |
| **Editor UI** | Windows, tabs, notifications, snap settings |
| **Live Coding** | Trigger recompile, hot reload |
| **Multi-User** | Session management, object locking |
| **And more...** | Console commands, automation, profiling |

## Manual Configuration

If the automatic installer doesn't work:

### Claude Desktop

Edit the config file:
- **macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`
- **Windows**: `%APPDATA%\Claude\claude_desktop_config.json`

```json
{
  "mcpServers": {
    "ue5-control": {
      "command": "ue5-mcp",
      "args": [],
      "env": {
        "UE5_RPC_URL": "http://localhost:7777/rpc"
      }
    }
  }
}
```

### Claude Code

```bash
claude mcp add ue5-control -- ue5-mcp
```

## Requirements

- **Python 3.10+** (for the MCP server)
- **Unreal Engine 5.4+** (tested with 5.7)
- **Claude Desktop** or **Claude Code**

## Plugin Settings

In Unreal, go to **Project Settings > Plugins > Ultimate Control**:

| Setting | Default | Description |
|---------|---------|-------------|
| Server Port | 7777 | HTTP server port |
| Localhost Only | true | Only accept local connections |
| Enable Asset Tools | true | Asset management methods |
| Enable Blueprint Tools | true | Blueprint editing methods |
| Enable Level Tools | true | Level/actor methods |
| Enable PIE Tools | true | Play-in-editor methods |
| Enable Console Commands | true | Console execution |
| Enable File System | true | File read/write |

## Troubleshooting

### Server Not Connecting

1. Check the UE5 editor is running
2. Look for `LogUltimateControlServer` in the Output Log
3. Verify port 7777 is not blocked: `curl http://localhost:7777/rpc`
4. Check the server started: `Starting HTTP server on port 7777`

### Methods Not Working

Run `system.listMethods` to see available methods. Some handlers require settings to be enabled.

### Tools Not Appearing in Claude

1. Restart Claude Desktop after configuration
2. Check the config file syntax is valid JSON
3. Verify `ue5-mcp` command works: `ue5-mcp --help`

## Architecture

```
┌─────────────────┐     MCP Protocol      ┌─────────────────┐
│                 │ ◄───────────────────► │                 │
│  Claude         │                       │  UE5 MCP Bridge │
│  (Desktop/Code) │                       │  (Python)       │
│                 │                       │                 │
└─────────────────┘                       └────────┬────────┘
                                                   │
                                          HTTP JSON-RPC
                                                   │
                                          ┌────────▼────────┐
                                          │                 │
                                          │ UltimateControl │
                                          │ Plugin (C++)    │
                                          │                 │
                                          │   UE5 Editor    │
                                          │                 │
                                          └─────────────────┘
```

## Development

### Running from Source

```bash
git clone https://github.com/rocketship/ue5-mcp-bridge
cd ue5-mcp-bridge
pip install -e .
ue5-mcp
```

### Testing

```bash
pip install -e ".[dev]"
pytest
```

## License

MIT License
