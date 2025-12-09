# UltimateControl Plugin

A comprehensive HTTP JSON-RPC API plugin for Unreal Engine 5.7 that enables AI agents and external tools to control the editor.

## Overview

UltimateControl exposes a local HTTP server that accepts JSON-RPC 2.0 requests, providing programmatic access to:

**Core Tools:**
- **Assets**: List, search, create, import, export, and delete assets
- **Blueprints**: Inspect and modify blueprint graphs, variables, and functions
- **Levels & Actors**: Open levels, spawn/destroy actors, modify transforms
- **PIE**: Start/stop play sessions, pause/resume, simulation control
- **Automation**: Run tests, cook content, package builds
- **Console**: Execute commands, manage console variables
- **Profiling**: Performance stats, memory usage, tracing
- **Files**: Read/write project files with security restrictions

**Extended Tools (27 handler categories, 350+ methods):**
- **Viewport/Camera**: Camera control, screenshots, view modes
- **Transaction/Undo**: Undo/redo management and history
- **Materials**: Material/shader parameters, instances
- **Animation**: Skeletal animations, Animation Blueprints
- **Sequencer**: Level Sequences, tracks, playback control
- **Audio**: Sound playback, audio settings
- **Physics**: Physics simulation, collision, bodies
- **Lighting**: Light properties, light building
- **World Partition**: Cell loading, data layers
- **Niagara/VFX**: Particle systems, parameters
- **Landscape**: Terrain queries, layers, materials
- **AI/Navigation**: NavMesh, AI controllers, behavior trees
- **Render/PostProcess**: Quality settings, post-process volumes
- **Outliner**: Actor hierarchy, folders, layers
- **Source Control**: Check-out, check-in, sync, history
- **Live Coding**: Hot reload, live compile
- **Multi-User Sessions**: Concert/Multi-User editing
- **Editor UI**: Windows, tabs, modes, notifications

## Installation

1. Copy the `UltimateControl` folder to your project's `Source/` directory
2. Add `UltimateControl` to your project's `.uproject` file:

```json
{
    "Plugins": [
        {
            "Name": "UltimateControl",
            "Enabled": true
        }
    ]
}
```

3. Regenerate project files and build

## Configuration

Configure the plugin in **Project Settings > Plugins > Ultimate Control**:

| Setting | Default | Description |
|---------|---------|-------------|
| Auto Start Server | true | Start HTTP server on editor load |
| Server Port | 7777 | HTTP server port |
| Localhost Only | true | Bind only to 127.0.0.1 |
| Require Authentication | true | Require auth token |
| Auth Token | (generated) | Token for X-Ultimate-Control-Token header |
| Max Connections | 10 | Maximum concurrent connections |
| Request Timeout | 30s | Request timeout |
| Log Verbosity | 1 | 0=Errors, 1=Warnings, 2=Info, 3=Verbose |

### Feature Flags

Enable/disable specific tool categories:
- Enable Asset Tools
- Enable Blueprint Tools
- Enable Level Tools
- Enable PIE Tools
- Enable Automation Tools
- Enable Profiling Tools
- Enable File Tools
- Enable Console Commands

### Safety Settings

- **Require Confirmation for Dangerous Ops**: Requires explicit confirmation for destructive operations
- **Create Backups**: Create backups before modifying files
- **Max Actors Per Operation**: Limit bulk operations

## API Reference

### Endpoint

```
POST http://127.0.0.1:7777/rpc
Content-Type: application/json
X-Ultimate-Control-Token: your-auth-token
```

### JSON-RPC 2.0 Format

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "asset.list",
    "params": {
        "path": "/Game",
        "recursive": true
    },
    "id": "1"
}
```

**Response (success):**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "assets": [...],
        "totalCount": 42
    },
    "id": "1"
}
```

**Response (error):**
```json
{
    "jsonrpc": "2.0",
    "error": {
        "code": -32601,
        "message": "Method not found"
    },
    "id": "1"
}
```

### Error Codes

| Code | Meaning |
|------|---------|
| -32700 | Parse error |
| -32600 | Invalid request |
| -32601 | Method not found |
| -32602 | Invalid params |
| -32603 | Internal error |
| -32000 | Unauthorized |
| -32001 | Feature disabled |
| -32002 | Operation failed |
| -32003 | Not found |
| -32004 | Confirmation required |
| -32005 | Timeout |

## Methods

### System Methods

#### `system.getInfo`
Get server and engine information.

**Returns:**
```json
{
    "serverVersion": "1.0.0",
    "engineVersion": "5.7.0",
    "platform": "Windows",
    "isRunning": true,
    "registeredMethods": 50,
    "features": {
        "assetTools": true,
        "blueprintTools": true
    }
}
```

#### `system.listMethods`
List all available methods with descriptions.

#### `system.echo`
Echo back parameters (for testing connectivity).

### Project Methods

#### `project.getInfo`
Get project information including paths and configuration.

#### `project.save`
Save all dirty packages.

**Params:** `{ "prompt": boolean }`

#### `project.listPlugins`
List enabled plugins.

**Params:** `{ "category": string (optional) }`

#### `project.compileBlueprints`
Recompile all blueprints.

**Params:** `{ "path": string (optional) }`

### Asset Methods

#### `asset.list`
List assets with filtering.

**Params:**
```json
{
    "path": "/Game",
    "class": "Blueprint",
    "recursive": true,
    "limit": 100,
    "offset": 0,
    "includeMetadata": false
}
```

#### `asset.get`
Get detailed asset information.

**Params:** `{ "path": "/Game/MyAsset" }`

#### `asset.search`
Search assets by name.

**Params:** `{ "query": "Player", "class": "Blueprint", "limit": 50 }`

#### `asset.exists`
Check if asset exists.

**Params:** `{ "path": "/Game/MyAsset" }`

#### `asset.duplicate`
Duplicate an asset.

**Params:** `{ "source": "/Game/Original", "destination": "/Game/Copy" }`

#### `asset.rename`
Rename/move an asset.

**Params:** `{ "source": "/Game/Old", "destination": "/Game/New" }`

#### `asset.delete`
Delete an asset. **[Dangerous]**

**Params:** `{ "path": "/Game/ToDelete" }`

#### `asset.import`
Import external file.

**Params:** `{ "file": "C:/textures/wood.png", "destination": "/Game/Textures" }`

#### `asset.getProperty` / `asset.setProperty`
Get/set asset properties.

### Blueprint Methods

#### `blueprint.list`
List blueprints.

**Params:** `{ "path": "/Game", "recursive": true, "limit": 500 }`

#### `blueprint.get`
Get blueprint details.

#### `blueprint.getGraphs`
Get all graphs in a blueprint.

#### `blueprint.getNodes`
Get nodes in a specific graph.

**Params:** `{ "path": "/Game/BP_Player", "graph": "EventGraph" }`

#### `blueprint.getVariables`
Get blueprint variables.

#### `blueprint.getFunctions`
Get blueprint functions.

#### `blueprint.compile`
Compile a blueprint.

#### `blueprint.create`
Create a new blueprint.

**Params:** `{ "path": "/Game/Blueprints/BP_New", "parentClass": "Actor" }`

#### `blueprint.addVariable`
Add a variable.

**Params:** `{ "path": "/Game/BP", "name": "Health", "type": "float" }`

### Level Methods

#### `level.getCurrent`
Get current level info.

#### `level.open`
Open a level.

**Params:** `{ "path": "/Game/Maps/MainLevel", "promptSave": true }`

#### `level.save`
Save current level.

#### `level.list`
List level assets.

#### `level.getStreamingLevels`
Get streaming levels.

### Actor Methods

#### `actor.list`
List actors with filtering.

**Params:** `{ "class": "PointLight", "tag": "Enemy", "limit": 100 }`

#### `actor.get`
Get actor details.

**Params:** `{ "actor": "BP_Player_C_0" }`

#### `actor.spawn`
Spawn an actor.

**Params:**
```json
{
    "class": "PointLight",
    "name": "MyLight",
    "location": { "x": 0, "y": 0, "z": 200 },
    "rotation": { "pitch": 0, "yaw": 0, "roll": 0 }
}
```

#### `actor.destroy`
Destroy an actor. **[Dangerous]**

#### `actor.setTransform` / `actor.getTransform`
Set/get actor transform.

#### `actor.setProperty` / `actor.getProperty`
Set/get actor properties.

#### `actor.getComponents`
Get actor components.

#### `actor.addComponent`
Add a component.

#### `actor.callFunction`
Call a function on an actor.

### Selection Methods

#### `selection.get`
Get selected actors.

#### `selection.set`
Set selection.

**Params:** `{ "actors": ["Actor1", "Actor2"], "add": false }`

#### `selection.focus`
Focus viewport on selection.

### PIE Methods

#### `pie.play`
Start PIE.

**Params:** `{ "mode": "SelectedViewport" }`

#### `pie.stop`
Stop PIE.

#### `pie.pause`
Pause/resume.

**Params:** `{ "pause": true }`

#### `pie.getState`
Get PIE state.

#### `pie.simulate`
Start simulation mode.

#### `pie.eject`
Eject from player.

### Console Methods

#### `console.execute`
Execute console command. **[Dangerous]**

**Params:** `{ "command": "stat fps" }`

#### `console.getVariable` / `console.setVariable`
Get/set console variables.

#### `console.listVariables` / `console.listCommands`
List console variables/commands.

### Automation Methods

#### `automation.listTests`
List available tests.

#### `automation.runTests`
Run tests.

**Params:** `{ "tests": ["TestName1"], "filter": "Smoke" }`

#### `automation.getTestResults`
Get test results.

#### `build.cook`
Cook content. **[Dangerous]**

#### `build.package`
Package project. **[Dangerous]**

### Profiling Methods

#### `profiling.getStats`
Get performance stats.

#### `profiling.getMemory`
Get memory usage.

#### `profiling.startTrace` / `profiling.stopTrace`
Control profiling traces.

### Logging Methods

#### `logging.getLogs`
Get recent logs.

#### `logging.getCategories`
Get log categories.

#### `logging.setVerbosity`
Set log verbosity.

### File Methods

#### `file.read`
Read a file.

**Params:** `{ "path": "Config/DefaultGame.ini" }`

#### `file.write`
Write a file. **[Dangerous]**

**Params:** `{ "path": "test.txt", "content": "Hello", "append": false }`

#### `file.exists`
Check if file exists.

#### `file.list`
List directory contents.

#### `file.delete`
Delete a file. **[Dangerous]**

#### `file.copy` / `file.move`
Copy/move files.

### Viewport Methods

#### `viewport.list`
List all viewports.

#### `viewport.getActive` / `viewport.setActive`
Get/set active viewport.

#### `viewport.getCamera` / `viewport.setCamera`
Get/set viewport camera position and rotation.

#### `viewport.focusActor`
Focus viewport on an actor.

**Params:** `{ "actor": "ActorName" }`

#### `viewport.screenshot`
Take a screenshot.

**Params:** `{ "filename": "shot.png", "viewportIndex": 0 }`

#### `viewport.setViewMode`
Set view mode (Lit, Unlit, Wireframe, etc.).

### Transaction Methods

#### `transaction.begin` / `transaction.end` / `transaction.cancel`
Begin/end/cancel undo transactions.

#### `transaction.undo` / `transaction.redo`
Perform undo/redo.

#### `transaction.getHistory`
Get undo history.

### Material Methods

#### `material.list`
List materials.

#### `material.get`
Get material details.

#### `material.create`
Create a new material.

#### `material.getParameters`
Get material parameters.

#### `material.setScalar` / `material.setVector` / `material.setTexture`
Set material instance parameters.

#### `material.createInstance`
Create a material instance.

### Animation Methods

#### `animation.list`
List animation assets.

#### `animation.get`
Get animation details.

#### `animation.play` / `animation.stop`
Play/stop animation on actors.

#### `animation.listBlueprints`
List Animation Blueprints.

### Sequencer Methods

#### `sequencer.list`
List Level Sequences.

#### `sequencer.get`
Get sequence details.

#### `sequencer.open`
Open sequence in Sequencer editor.

#### `sequencer.play` / `sequencer.stop`
Control sequence playback.

#### `sequencer.setTime`
Set playhead time.

#### `sequencer.getTracks`
Get tracks in a sequence.

#### `sequencer.addActor`
Add actor binding to sequence.

### Audio Methods

#### `audio.list`
List audio assets.

#### `audio.play`
Play sound at location or attached to actor.

#### `audio.stopAll`
Stop all playing sounds.

#### `audio.setVolume`
Set master volume.

### Physics Methods

#### `physics.simulate`
Enable/disable physics simulation.

#### `physics.getBodyInfo`
Get physics body info for actor.

#### `physics.setSimulate`
Set physics simulation for actor.

#### `physics.applyImpulse`
Apply impulse to physics body.

#### `physics.listCollisionProfiles`
List collision profiles.

### Lighting Methods

#### `lighting.list`
List lights in level.

#### `lighting.get`
Get light properties.

#### `lighting.setIntensity` / `lighting.setColor`
Set light properties.

#### `lighting.build`
Build lighting.

#### `lighting.buildReflectionCaptures`
Build reflection captures.

### World Partition Methods

#### `worldPartition.getStatus`
Get World Partition status.

#### `worldPartition.listCells`
List cells.

#### `worldPartition.loadCells` / `worldPartition.unloadCells`
Load/unload cells.

#### `dataLayer.list`
List data layers.

#### `dataLayer.setVisibility`
Set data layer visibility.

#### `dataLayer.getActors`
Get actors in data layer.

### Niagara Methods

#### `niagara.listSystems`
List Niagara systems.

#### `niagara.spawn`
Spawn Niagara system at location.

#### `niagara.activate` / `niagara.deactivate`
Activate/deactivate Niagara components.

#### `niagara.setFloat` / `niagara.setVector`
Set Niagara parameters.

### Landscape Methods

#### `landscape.list`
List landscapes.

#### `landscape.getHeightAtLocation`
Get height at world location.

#### `landscape.listLayers`
List paint layers.

#### `landscape.getMaterial`
Get landscape material.

### AI/Navigation Methods

#### `navigation.build`
Build navigation mesh.

#### `navigation.getStatus`
Get nav mesh status.

#### `navigation.findPath`
Find path between two points.

#### `ai.listControllers`
List AI controllers.

#### `ai.moveToLocation`
Command AI to move.

#### `behaviorTree.list`
List Behavior Trees.

#### `behaviorTree.run`
Run behavior tree on AI controller.

### Render Methods

#### `render.getQualitySettings` / `render.setQualitySettings`
Get/set quality settings.

#### `postProcess.listVolumes`
List post-process volumes.

#### `postProcess.getVolume`
Get volume settings.

#### `postProcess.setBloomIntensity` / `postProcess.setExposure`
Set post-process settings.

#### `render.getShowFlags` / `render.setShowFlag`
Get/set show flags.

### Outliner Methods

#### `outliner.getHierarchy`
Get actor hierarchy.

#### `outliner.setParent` / `outliner.detachFromParent`
Attach/detach actors.

#### `outliner.listFolders` / `outliner.createFolder`
Manage folders.

#### `outliner.setActorFolder`
Move actor to folder.

#### `layer.list` / `layer.create`
Manage editor layers.

#### `layer.addActor`
Add actor to layer.

#### `outliner.search`
Search actors.

### Source Control Methods

#### `sourceControl.getProviderStatus`
Get source control status.

#### `sourceControl.getFileStatus`
Get file status.

#### `sourceControl.checkOut` / `sourceControl.checkIn`
Check out/in files.

#### `sourceControl.revert`
Revert changes.

#### `sourceControl.sync`
Sync to latest.

#### `sourceControl.getHistory`
Get file history.

### Live Coding Methods

#### `liveCoding.isEnabled`
Check if live coding is enabled.

#### `liveCoding.enable` / `liveCoding.disable`
Enable/disable live coding.

#### `liveCoding.compile`
Trigger live coding compile.

#### `hotReload.reload`
Trigger hot reload.

#### `module.list`
List loaded modules.

### Session Methods (Multi-User)

#### `session.list`
List available sessions.

#### `session.getCurrent`
Get current session info.

#### `session.join` / `session.leave`
Join/leave session.

#### `session.listUsers`
List users in session.

#### `session.lockObject` / `session.unlockObject`
Lock/unlock objects.

### Editor Methods

#### `editor.listWindows` / `editor.focusWindow`
Manage editor windows.

#### `editor.listTabs` / `editor.openTab`
Manage tabs.

#### `editor.getCurrentMode` / `editor.setMode`
Get/set editor mode.

#### `editor.getTransformMode` / `editor.setTransformMode`
Get/set transform gizmo mode.

#### `editor.getSnapSettings` / `editor.setSnapSettings`
Manage snap settings.

#### `editor.showNotification`
Show editor notification.

#### `editor.executeCommand`
Execute editor command.

#### `editor.openProjectSettings`
Open project settings.

## Security

### Authentication

All requests must include the auth token:
```
X-Ultimate-Control-Token: your-token-here
```

### Path Restrictions

File operations are restricted to:
- Project directory
- Engine directory (read-only for most operations)

Blocked directories:
- `Saved/Config`
- `Intermediate`
- `.git`
- `Binaries`

### Blocked Commands

Console execution blocks dangerous commands like:
- `exit`, `quit`
- `crash`
- `debug crash`

## MCP Integration

The plugin includes a bundled MCP (Model Context Protocol) server that enables AI agents like Claude to control Unreal Engine directly.

### Building the MCP Server

The MCP server is a Rust executable located in `Source/UltimateControl/MCP/`:

```bash
cd Source/UltimateControl/MCP
./build.sh  # Builds and copies to Binaries/
```

Or manually:
```bash
cd Source/UltimateControl/MCP
cargo build --release
```

### Using with Claude Desktop

Add to your Claude Desktop configuration (`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "ue5": {
      "command": "/path/to/project/Plugins/UltimateControl/Binaries/ue5-mcp",
      "env": {
        "UE5_MCP_HOST": "127.0.0.1",
        "UE5_MCP_PORT": "7777",
        "UE5_MCP_TOKEN": "your-auth-token-from-settings"
      }
    }
  }
}
```

### Alternative: Python MCP Server

A Python MCP server is also available in `ue5-mcp-bridge/`:

```bash
cd ue5-mcp-bridge
pip install -e .
UE5_MCP_TOKEN=your-token ue5-mcp
```

### Architecture

```
┌─────────────────┐     stdio (MCP)      ┌─────────────────┐
│                 │ ◄─────────────────── │                 │
│  Claude Desktop │                      │  ue5-mcp        │
│  (MCP Client)   │                      │  (Rust binary)  │
│                 │                      │                 │
└─────────────────┘                      └────────┬────────┘
                                                  │
                                         HTTP JSON-RPC
                                                  │
                                         ┌────────▼────────┐
                                         │                 │
                                         │ UltimateControl │
                                         │ Plugin (C++)    │
                                         │                 │
                                         └────────┬────────┘
                                                  │
                                            UE5 APIs
                                                  │
                                         ┌────────▼────────┐
                                         │                 │
                                         │ Unreal Engine   │
                                         │ 5.7 Editor      │
                                         │                 │
                                         └─────────────────┘
```

## License

MIT License
