"""
MCP Server for Unreal Engine 5.7 control via UltimateControl plugin.

This module provides a comprehensive MCP server that exposes tools for:
- Project and file management
- Asset operations (list, search, import, export)
- Blueprint manipulation
- Level and actor control
- Play-in-Editor (PIE) management
- Automation and testing
- Profiling and logging
- Console command execution
"""

import asyncio
import json
import os
from typing import Any, Optional

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent
from pydantic import AnyUrl

from .client import UltimateControlClient, UltimateControlError


# Configuration from environment
UE_HOST = os.environ.get("UE5_MCP_HOST", "127.0.0.1")
UE_PORT = int(os.environ.get("UE5_MCP_PORT", "7777"))
UE_TOKEN = os.environ.get("UE5_MCP_TOKEN", "")


def create_server() -> Server:
    """Create and configure the MCP server."""
    server = Server("ue5-mcp-bridge")

    @server.list_tools()
    async def list_tools() -> list[Tool]:
        """Return the list of available tools."""
        return [
            # System Tools
            Tool(
                name="ue5_system_info",
                description="Get information about the UE5 editor and UltimateControl server",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_list_methods",
                description="List all available JSON-RPC methods exposed by the UltimateControl plugin",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            # Project Tools
            Tool(
                name="ue5_project_info",
                description="Get information about the current Unreal Engine project",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_project_save",
                description="Save all dirty (modified) packages in the project",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "prompt": {
                            "type": "boolean",
                            "description": "Whether to prompt the user before saving",
                            "default": False,
                        }
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_list_plugins",
                description="List all enabled plugins in the project",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "category": {
                            "type": "string",
                            "description": "Filter plugins by category",
                        }
                    },
                    "required": [],
                },
            ),
            # Asset Tools
            Tool(
                name="ue5_asset_list",
                description="List assets in a content directory with optional filtering",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Content path to list (e.g., /Game/Blueprints)",
                            "default": "/Game",
                        },
                        "class": {
                            "type": "string",
                            "description": "Filter by asset class (e.g., Blueprint, StaticMesh)",
                        },
                        "recursive": {
                            "type": "boolean",
                            "description": "Search subdirectories",
                            "default": True,
                        },
                        "limit": {
                            "type": "integer",
                            "description": "Maximum number of assets to return",
                            "default": 100,
                        },
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_asset_get",
                description="Get detailed information about a specific asset",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Full asset path (e.g., /Game/Blueprints/BP_Player)",
                        }
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_asset_search",
                description="Search for assets by name pattern",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "query": {
                            "type": "string",
                            "description": "Search query (matches asset names)",
                        },
                        "class": {
                            "type": "string",
                            "description": "Filter by asset class",
                        },
                        "limit": {
                            "type": "integer",
                            "description": "Maximum results",
                            "default": 50,
                        },
                    },
                    "required": ["query"],
                },
            ),
            Tool(
                name="ue5_asset_duplicate",
                description="Duplicate an asset to a new location",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "source": {
                            "type": "string",
                            "description": "Source asset path",
                        },
                        "destination": {
                            "type": "string",
                            "description": "Destination asset path",
                        },
                    },
                    "required": ["source", "destination"],
                },
            ),
            Tool(
                name="ue5_asset_delete",
                description="Delete an asset (use with caution)",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Asset path to delete",
                        }
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_asset_import",
                description="Import an external file as an asset",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "file": {
                            "type": "string",
                            "description": "Path to the file to import",
                        },
                        "destination": {
                            "type": "string",
                            "description": "Destination content path",
                        },
                    },
                    "required": ["file", "destination"],
                },
            ),
            # Blueprint Tools
            Tool(
                name="ue5_blueprint_list",
                description="List all blueprints in a path",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Content path to search",
                            "default": "/Game",
                        },
                        "limit": {
                            "type": "integer",
                            "description": "Maximum results",
                            "default": 100,
                        },
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_blueprint_get",
                description="Get detailed blueprint information including graphs and variables",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Blueprint asset path",
                        }
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_blueprint_get_graphs",
                description="Get all graphs (event graph, functions) in a blueprint",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Blueprint asset path",
                        }
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_blueprint_get_variables",
                description="Get all variables defined in a blueprint",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Blueprint asset path",
                        }
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_blueprint_compile",
                description="Compile a blueprint",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Blueprint asset path",
                        }
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_blueprint_create",
                description="Create a new blueprint class",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Path for the new blueprint (e.g., /Game/Blueprints/BP_MyActor)",
                        },
                        "parentClass": {
                            "type": "string",
                            "description": "Parent class (Actor, Pawn, Character, etc.)",
                            "default": "Actor",
                        },
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_blueprint_add_variable",
                description="Add a variable to a blueprint",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Blueprint asset path",
                        },
                        "name": {
                            "type": "string",
                            "description": "Variable name",
                        },
                        "type": {
                            "type": "string",
                            "description": "Variable type (bool, int, float, string, etc.)",
                            "default": "bool",
                        },
                    },
                    "required": ["path", "name"],
                },
            ),
            # Level/Actor Tools
            Tool(
                name="ue5_level_get_current",
                description="Get information about the currently loaded level",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_level_open",
                description="Open a level by path",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Level asset path",
                        },
                        "promptSave": {
                            "type": "boolean",
                            "description": "Prompt to save current level",
                            "default": True,
                        },
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_level_save",
                description="Save the current level",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_level_list",
                description="List all level assets in the project",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Content path to search",
                            "default": "/Game",
                        },
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_actor_list",
                description="List actors in the current level",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "class": {
                            "type": "string",
                            "description": "Filter by actor class",
                        },
                        "tag": {
                            "type": "string",
                            "description": "Filter by actor tag",
                        },
                        "limit": {
                            "type": "integer",
                            "description": "Maximum results",
                            "default": 100,
                        },
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_actor_get",
                description="Get detailed information about an actor",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {
                            "type": "string",
                            "description": "Actor name or path",
                        }
                    },
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_actor_spawn",
                description="Spawn a new actor in the level",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "class": {
                            "type": "string",
                            "description": "Actor class (StaticMeshActor, PointLight, etc.)",
                        },
                        "name": {
                            "type": "string",
                            "description": "Optional actor name/label",
                        },
                        "location": {
                            "type": "object",
                            "description": "Spawn location {x, y, z}",
                            "properties": {
                                "x": {"type": "number"},
                                "y": {"type": "number"},
                                "z": {"type": "number"},
                            },
                        },
                        "rotation": {
                            "type": "object",
                            "description": "Spawn rotation {pitch, yaw, roll}",
                            "properties": {
                                "pitch": {"type": "number"},
                                "yaw": {"type": "number"},
                                "roll": {"type": "number"},
                            },
                        },
                    },
                    "required": ["class"],
                },
            ),
            Tool(
                name="ue5_actor_destroy",
                description="Destroy an actor from the level",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {
                            "type": "string",
                            "description": "Actor name or path",
                        }
                    },
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_actor_set_transform",
                description="Set an actor's location, rotation, and/or scale",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {
                            "type": "string",
                            "description": "Actor name or path",
                        },
                        "location": {
                            "type": "object",
                            "description": "New location {x, y, z}",
                        },
                        "rotation": {
                            "type": "object",
                            "description": "New rotation {pitch, yaw, roll}",
                        },
                        "scale": {
                            "type": "object",
                            "description": "New scale {x, y, z}",
                        },
                    },
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_actor_get_components",
                description="Get all components on an actor",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {
                            "type": "string",
                            "description": "Actor name or path",
                        }
                    },
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_selection_get",
                description="Get currently selected actors in the editor",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_selection_set",
                description="Set the selected actors",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actors": {
                            "type": "array",
                            "items": {"type": "string"},
                            "description": "List of actor names to select",
                        },
                        "add": {
                            "type": "boolean",
                            "description": "Add to current selection instead of replacing",
                            "default": False,
                        },
                    },
                    "required": ["actors"],
                },
            ),
            # PIE Tools
            Tool(
                name="ue5_pie_play",
                description="Start Play In Editor session",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "mode": {
                            "type": "string",
                            "description": "Play mode (SelectedViewport, NewWindow, MobilePreview)",
                            "default": "SelectedViewport",
                        }
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_pie_stop",
                description="Stop the current Play In Editor session",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_pie_pause",
                description="Pause or resume the Play In Editor session",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "pause": {
                            "type": "boolean",
                            "description": "True to pause, False to resume",
                        }
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_pie_get_state",
                description="Get the current Play In Editor state",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            # Console Tools
            Tool(
                name="ue5_console_execute",
                description="Execute a console command",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "command": {
                            "type": "string",
                            "description": "Console command to execute",
                        }
                    },
                    "required": ["command"],
                },
            ),
            Tool(
                name="ue5_console_get_variable",
                description="Get a console variable value",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "name": {
                            "type": "string",
                            "description": "Console variable name",
                        }
                    },
                    "required": ["name"],
                },
            ),
            Tool(
                name="ue5_console_set_variable",
                description="Set a console variable value",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "name": {
                            "type": "string",
                            "description": "Console variable name",
                        },
                        "value": {
                            "type": "string",
                            "description": "New value",
                        },
                    },
                    "required": ["name", "value"],
                },
            ),
            Tool(
                name="ue5_console_list_variables",
                description="List console variables matching a filter",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "filter": {
                            "type": "string",
                            "description": "Filter string",
                            "default": "",
                        },
                        "limit": {
                            "type": "integer",
                            "description": "Maximum results",
                            "default": 50,
                        },
                    },
                    "required": [],
                },
            ),
            # Profiling Tools
            Tool(
                name="ue5_profiling_get_stats",
                description="Get current engine performance statistics",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_profiling_get_memory",
                description="Get memory usage statistics",
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                },
            ),
            # Automation Tools
            Tool(
                name="ue5_automation_list_tests",
                description="List available automation tests",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "filter": {
                            "type": "string",
                            "description": "Filter test names",
                        }
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_automation_run_tests",
                description="Run automation tests",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "tests": {
                            "type": "array",
                            "items": {"type": "string"},
                            "description": "List of test names to run",
                        },
                        "filter": {
                            "type": "string",
                            "description": "Filter to match test names",
                        },
                    },
                    "required": [],
                },
            ),
            # File Tools
            Tool(
                name="ue5_file_read",
                description="Read a file from the project",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "File path (relative to project or absolute)",
                        }
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_file_write",
                description="Write content to a file",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "File path",
                        },
                        "content": {
                            "type": "string",
                            "description": "Content to write",
                        },
                        "append": {
                            "type": "boolean",
                            "description": "Append instead of overwrite",
                            "default": False,
                        },
                    },
                    "required": ["path", "content"],
                },
            ),
            Tool(
                name="ue5_file_list",
                description="List files in a directory",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Directory path",
                            "default": "",
                        },
                        "pattern": {
                            "type": "string",
                            "description": "File pattern (e.g., *.cpp)",
                            "default": "*",
                        },
                        "recursive": {
                            "type": "boolean",
                            "description": "Search recursively",
                            "default": False,
                        },
                    },
                    "required": [],
                },
            ),
            # ===================================================================
            # Viewport/Camera Tools
            # ===================================================================
            Tool(
                name="ue5_viewport_list",
                description="List all active viewports in the editor",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_viewport_get_active",
                description="Get the currently active viewport",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_viewport_set_active",
                description="Set the active viewport by index",
                inputSchema={
                    "type": "object",
                    "properties": {"index": {"type": "integer", "description": "Viewport index"}},
                    "required": ["index"],
                },
            ),
            Tool(
                name="ue5_viewport_get_camera",
                description="Get the camera position and rotation for a viewport",
                inputSchema={
                    "type": "object",
                    "properties": {"viewportIndex": {"type": "integer", "default": 0}},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_viewport_set_camera",
                description="Set the camera position and rotation for a viewport",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "viewportIndex": {"type": "integer", "default": 0},
                        "location": {"type": "object", "properties": {"x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}},
                        "rotation": {"type": "object", "properties": {"pitch": {"type": "number"}, "yaw": {"type": "number"}, "roll": {"type": "number"}}},
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_viewport_focus_actor",
                description="Focus viewport on a specific actor",
                inputSchema={
                    "type": "object",
                    "properties": {"actor": {"type": "string", "description": "Actor name or path"}},
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_viewport_take_screenshot",
                description="Take a screenshot of the viewport",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "filename": {"type": "string", "description": "Output filename"},
                        "viewportIndex": {"type": "integer", "default": 0},
                    },
                    "required": ["filename"],
                },
            ),
            Tool(
                name="ue5_viewport_set_view_mode",
                description="Set the viewport view mode (Lit, Unlit, Wireframe, etc.)",
                inputSchema={
                    "type": "object",
                    "properties": {"mode": {"type": "string", "description": "View mode name"}},
                    "required": ["mode"],
                },
            ),
            # ===================================================================
            # Transaction/Undo Tools
            # ===================================================================
            Tool(
                name="ue5_transaction_begin",
                description="Begin a new undo transaction",
                inputSchema={
                    "type": "object",
                    "properties": {"description": {"type": "string", "description": "Transaction description"}},
                    "required": ["description"],
                },
            ),
            Tool(
                name="ue5_transaction_end",
                description="End the current transaction",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_transaction_cancel",
                description="Cancel the current transaction and revert changes",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_undo",
                description="Undo the last action",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_redo",
                description="Redo the last undone action",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_undo_history",
                description="Get the undo/redo history",
                inputSchema={
                    "type": "object",
                    "properties": {"limit": {"type": "integer", "default": 20}},
                    "required": [],
                },
            ),
            # ===================================================================
            # Material/Shader Tools
            # ===================================================================
            Tool(
                name="ue5_material_list",
                description="List materials in a path",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "default": "/Game"}},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_material_get",
                description="Get detailed material information",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "description": "Material asset path"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_material_create",
                description="Create a new material",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {"type": "string", "description": "Path for the new material"},
                        "template": {"type": "string", "description": "Optional template material"},
                    },
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_material_get_parameters",
                description="Get material parameters (scalar, vector, texture)",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "description": "Material asset path"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_material_set_scalar",
                description="Set a scalar parameter on a material instance",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {"type": "string"},
                        "parameter": {"type": "string"},
                        "value": {"type": "number"},
                    },
                    "required": ["path", "parameter", "value"],
                },
            ),
            Tool(
                name="ue5_material_set_vector",
                description="Set a vector parameter on a material instance",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {"type": "string"},
                        "parameter": {"type": "string"},
                        "r": {"type": "number"}, "g": {"type": "number"}, "b": {"type": "number"}, "a": {"type": "number", "default": 1.0},
                    },
                    "required": ["path", "parameter", "r", "g", "b"],
                },
            ),
            Tool(
                name="ue5_material_set_texture",
                description="Set a texture parameter on a material instance",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {"type": "string"},
                        "parameter": {"type": "string"},
                        "texturePath": {"type": "string"},
                    },
                    "required": ["path", "parameter", "texturePath"],
                },
            ),
            Tool(
                name="ue5_material_instance_create",
                description="Create a material instance from a parent material",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {"type": "string", "description": "Path for the new instance"},
                        "parent": {"type": "string", "description": "Parent material path"},
                    },
                    "required": ["path", "parent"],
                },
            ),
            # ===================================================================
            # Animation Tools
            # ===================================================================
            Tool(
                name="ue5_animation_list",
                description="List animation assets",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "default": "/Game"}},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_animation_get",
                description="Get animation asset details",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_animation_play",
                description="Play an animation on an actor's skeletal mesh",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "animation": {"type": "string"},
                        "loop": {"type": "boolean", "default": False},
                    },
                    "required": ["actor", "animation"],
                },
            ),
            Tool(
                name="ue5_animation_stop",
                description="Stop animation on an actor",
                inputSchema={
                    "type": "object",
                    "properties": {"actor": {"type": "string"}},
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_animation_blueprint_list",
                description="List Animation Blueprints",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "default": "/Game"}},
                    "required": [],
                },
            ),
            # ===================================================================
            # Sequencer Tools
            # ===================================================================
            Tool(
                name="ue5_sequencer_list",
                description="List Level Sequences in the project",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "default": "/Game"}},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_sequencer_get",
                description="Get sequence details",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_sequencer_open",
                description="Open a sequence in the Sequencer editor",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_sequencer_play",
                description="Play the current sequence",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_sequencer_stop",
                description="Stop sequence playback",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_sequencer_set_time",
                description="Set the playhead time in the sequence",
                inputSchema={
                    "type": "object",
                    "properties": {"time": {"type": "number", "description": "Time in seconds"}},
                    "required": ["time"],
                },
            ),
            Tool(
                name="ue5_sequencer_get_tracks",
                description="Get all tracks in a sequence",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_sequencer_add_actor",
                description="Add an actor binding to a sequence",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "sequence": {"type": "string"},
                        "actor": {"type": "string"},
                    },
                    "required": ["sequence", "actor"],
                },
            ),
            # ===================================================================
            # Audio Tools
            # ===================================================================
            Tool(
                name="ue5_audio_list",
                description="List audio assets (SoundWave, SoundCue)",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "default": "/Game"}},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_audio_play",
                description="Play a sound at a location or attached to an actor",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "sound": {"type": "string", "description": "Sound asset path"},
                        "actor": {"type": "string", "description": "Actor to attach to (optional)"},
                        "location": {"type": "object", "description": "World location if not attached"},
                    },
                    "required": ["sound"],
                },
            ),
            Tool(
                name="ue5_audio_stop_all",
                description="Stop all playing sounds",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_audio_set_volume",
                description="Set the master audio volume",
                inputSchema={
                    "type": "object",
                    "properties": {"volume": {"type": "number", "description": "Volume (0-1)"}},
                    "required": ["volume"],
                },
            ),
            # ===================================================================
            # Physics Tools
            # ===================================================================
            Tool(
                name="ue5_physics_simulate",
                description="Enable or disable physics simulation in editor",
                inputSchema={
                    "type": "object",
                    "properties": {"enable": {"type": "boolean"}},
                    "required": ["enable"],
                },
            ),
            Tool(
                name="ue5_physics_get_body_info",
                description="Get physics body information for an actor",
                inputSchema={
                    "type": "object",
                    "properties": {"actor": {"type": "string"}},
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_physics_set_simulate",
                description="Set physics simulation for an actor component",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "simulate": {"type": "boolean"},
                    },
                    "required": ["actor", "simulate"],
                },
            ),
            Tool(
                name="ue5_physics_apply_impulse",
                description="Apply an impulse to a physics body",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "impulse": {"type": "object", "properties": {"x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}},
                    },
                    "required": ["actor", "impulse"],
                },
            ),
            Tool(
                name="ue5_collision_list_profiles",
                description="List collision profiles",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            # ===================================================================
            # Lighting Tools
            # ===================================================================
            Tool(
                name="ue5_light_list",
                description="List all lights in the current level",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_light_get",
                description="Get light properties",
                inputSchema={
                    "type": "object",
                    "properties": {"actor": {"type": "string"}},
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_light_set_intensity",
                description="Set light intensity",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "intensity": {"type": "number"},
                    },
                    "required": ["actor", "intensity"],
                },
            ),
            Tool(
                name="ue5_light_set_color",
                description="Set light color",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "r": {"type": "number"}, "g": {"type": "number"}, "b": {"type": "number"},
                    },
                    "required": ["actor", "r", "g", "b"],
                },
            ),
            Tool(
                name="ue5_light_build",
                description="Build lighting for the level",
                inputSchema={
                    "type": "object",
                    "properties": {"quality": {"type": "string", "default": "Production"}},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_light_build_reflection_captures",
                description="Build reflection captures",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            # ===================================================================
            # World Partition / Data Layer Tools
            # ===================================================================
            Tool(
                name="ue5_world_partition_status",
                description="Get World Partition status",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_world_partition_list_cells",
                description="List World Partition cells",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_world_partition_load_cells",
                description="Load specific cells",
                inputSchema={
                    "type": "object",
                    "properties": {"cells": {"type": "array", "items": {"type": "string"}}},
                    "required": ["cells"],
                },
            ),
            Tool(
                name="ue5_data_layer_list",
                description="List all data layers",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_data_layer_set_visibility",
                description="Set data layer visibility",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "layer": {"type": "string"},
                        "visible": {"type": "boolean"},
                    },
                    "required": ["layer", "visible"],
                },
            ),
            Tool(
                name="ue5_data_layer_get_actors",
                description="Get actors in a data layer",
                inputSchema={
                    "type": "object",
                    "properties": {"layer": {"type": "string"}},
                    "required": ["layer"],
                },
            ),
            # ===================================================================
            # Niagara/VFX Tools
            # ===================================================================
            Tool(
                name="ue5_niagara_list",
                description="List Niagara systems",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "default": "/Game"}},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_niagara_spawn",
                description="Spawn a Niagara system at a location",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "system": {"type": "string", "description": "Niagara system asset path"},
                        "location": {"type": "object", "properties": {"x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}},
                    },
                    "required": ["system"],
                },
            ),
            Tool(
                name="ue5_niagara_activate",
                description="Activate a Niagara component",
                inputSchema={
                    "type": "object",
                    "properties": {"actor": {"type": "string"}},
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_niagara_deactivate",
                description="Deactivate a Niagara component",
                inputSchema={
                    "type": "object",
                    "properties": {"actor": {"type": "string"}},
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_niagara_set_float",
                description="Set a float parameter on a Niagara component",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "parameter": {"type": "string"},
                        "value": {"type": "number"},
                    },
                    "required": ["actor", "parameter", "value"],
                },
            ),
            Tool(
                name="ue5_niagara_set_vector",
                description="Set a vector parameter on a Niagara component",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "parameter": {"type": "string"},
                        "x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"},
                    },
                    "required": ["actor", "parameter", "x", "y", "z"],
                },
            ),
            # ===================================================================
            # Landscape Tools
            # ===================================================================
            Tool(
                name="ue5_landscape_list",
                description="List landscapes in the level",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_landscape_get_height",
                description="Get height at a world location",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "x": {"type": "number"},
                        "y": {"type": "number"},
                    },
                    "required": ["x", "y"],
                },
            ),
            Tool(
                name="ue5_landscape_list_layers",
                description="List landscape paint layers",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_landscape_get_material",
                description="Get landscape material",
                inputSchema={
                    "type": "object",
                    "properties": {"landscape": {"type": "string"}},
                    "required": ["landscape"],
                },
            ),
            # ===================================================================
            # AI/Navigation Tools
            # ===================================================================
            Tool(
                name="ue5_navigation_build",
                description="Build navigation mesh",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_navigation_status",
                description="Get navigation mesh status",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_navigation_find_path",
                description="Find a navigation path between two points",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "start": {"type": "object", "properties": {"x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}},
                        "end": {"type": "object", "properties": {"x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}},
                    },
                    "required": ["start", "end"],
                },
            ),
            Tool(
                name="ue5_ai_list_controllers",
                description="List AI controllers in the level",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_ai_move_to",
                description="Command an AI to move to a location",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "controller": {"type": "string"},
                        "location": {"type": "object", "properties": {"x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}},
                    },
                    "required": ["controller", "location"],
                },
            ),
            Tool(
                name="ue5_behavior_tree_list",
                description="List Behavior Tree assets",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string", "default": "/Game"}},
                    "required": [],
                },
            ),
            Tool(
                name="ue5_behavior_tree_run",
                description="Run a behavior tree on an AI controller",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "controller": {"type": "string"},
                        "tree": {"type": "string", "description": "Behavior tree asset path"},
                    },
                    "required": ["controller", "tree"],
                },
            ),
            # ===================================================================
            # Render/PostProcess Tools
            # ===================================================================
            Tool(
                name="ue5_render_get_settings",
                description="Get current render/quality settings",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_render_set_quality",
                description="Set render quality level",
                inputSchema={
                    "type": "object",
                    "properties": {"level": {"type": "integer", "description": "Quality level 0-4"}},
                    "required": ["level"],
                },
            ),
            Tool(
                name="ue5_postprocess_list_volumes",
                description="List PostProcess volumes in the level",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_postprocess_get",
                description="Get PostProcess volume settings",
                inputSchema={
                    "type": "object",
                    "properties": {"actor": {"type": "string"}},
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_postprocess_set_bloom",
                description="Set bloom intensity on a PostProcess volume",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "intensity": {"type": "number"},
                    },
                    "required": ["actor", "intensity"],
                },
            ),
            Tool(
                name="ue5_postprocess_set_exposure",
                description="Set exposure settings",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "minEV": {"type": "number"},
                        "maxEV": {"type": "number"},
                    },
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_render_get_show_flags",
                description="Get viewport show flags",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_render_set_show_flag",
                description="Set a viewport show flag",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "flag": {"type": "string"},
                        "enabled": {"type": "boolean"},
                    },
                    "required": ["flag", "enabled"],
                },
            ),
            # ===================================================================
            # Outliner/Hierarchy Tools
            # ===================================================================
            Tool(
                name="ue5_outliner_get_hierarchy",
                description="Get the actor hierarchy from the scene outliner",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_outliner_set_parent",
                description="Set an actor's parent (attach)",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "child": {"type": "string"},
                        "parent": {"type": "string"},
                    },
                    "required": ["child", "parent"],
                },
            ),
            Tool(
                name="ue5_outliner_detach",
                description="Detach an actor from its parent",
                inputSchema={
                    "type": "object",
                    "properties": {"actor": {"type": "string"}},
                    "required": ["actor"],
                },
            ),
            Tool(
                name="ue5_outliner_list_folders",
                description="List folders in the scene outliner",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_outliner_create_folder",
                description="Create a folder in the scene outliner",
                inputSchema={
                    "type": "object",
                    "properties": {"name": {"type": "string"}},
                    "required": ["name"],
                },
            ),
            Tool(
                name="ue5_outliner_set_folder",
                description="Move an actor to a folder",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "folder": {"type": "string"},
                    },
                    "required": ["actor", "folder"],
                },
            ),
            Tool(
                name="ue5_layer_list",
                description="List editor layers",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_layer_create",
                description="Create an editor layer",
                inputSchema={
                    "type": "object",
                    "properties": {"name": {"type": "string"}},
                    "required": ["name"],
                },
            ),
            Tool(
                name="ue5_layer_add_actor",
                description="Add an actor to a layer",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "actor": {"type": "string"},
                        "layer": {"type": "string"},
                    },
                    "required": ["actor", "layer"],
                },
            ),
            Tool(
                name="ue5_outliner_search",
                description="Search for actors in the outliner",
                inputSchema={
                    "type": "object",
                    "properties": {"query": {"type": "string"}},
                    "required": ["query"],
                },
            ),
            # ===================================================================
            # Source Control Tools
            # ===================================================================
            Tool(
                name="ue5_source_control_status",
                description="Get source control provider status",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_source_control_file_status",
                description="Get source control status for a file",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_source_control_checkout",
                description="Check out a file",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_source_control_checkin",
                description="Check in a file with a description",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "path": {"type": "string"},
                        "description": {"type": "string"},
                    },
                    "required": ["path", "description"],
                },
            ),
            Tool(
                name="ue5_source_control_revert",
                description="Revert changes to a file",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_source_control_sync",
                description="Sync file(s) to latest",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            Tool(
                name="ue5_source_control_history",
                description="Get file history",
                inputSchema={
                    "type": "object",
                    "properties": {"path": {"type": "string"}},
                    "required": ["path"],
                },
            ),
            # ===================================================================
            # Live Coding Tools
            # ===================================================================
            Tool(
                name="ue5_live_coding_status",
                description="Get Live Coding status",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_live_coding_compile",
                description="Trigger a Live Coding compile",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_live_coding_enable",
                description="Enable Live Coding",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_live_coding_disable",
                description="Disable Live Coding",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_hot_reload",
                description="Trigger a hot reload (legacy)",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_module_list",
                description="List loaded modules",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            # ===================================================================
            # Multi-User Session Tools
            # ===================================================================
            Tool(
                name="ue5_session_list",
                description="List available Multi-User sessions",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_session_current",
                description="Get current session info",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_session_join",
                description="Join a Multi-User session",
                inputSchema={
                    "type": "object",
                    "properties": {"session": {"type": "string"}},
                    "required": ["session"],
                },
            ),
            Tool(
                name="ue5_session_leave",
                description="Leave the current Multi-User session",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_session_users",
                description="List users in the current session",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_session_lock",
                description="Lock an object for editing",
                inputSchema={
                    "type": "object",
                    "properties": {"object": {"type": "string"}},
                    "required": ["object"],
                },
            ),
            Tool(
                name="ue5_session_unlock",
                description="Unlock an object",
                inputSchema={
                    "type": "object",
                    "properties": {"object": {"type": "string"}},
                    "required": ["object"],
                },
            ),
            # ===================================================================
            # Editor UI Tools
            # ===================================================================
            Tool(
                name="ue5_editor_list_windows",
                description="List open editor windows",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_editor_focus_window",
                description="Focus a window by title",
                inputSchema={
                    "type": "object",
                    "properties": {"title": {"type": "string"}},
                    "required": ["title"],
                },
            ),
            Tool(
                name="ue5_editor_list_tabs",
                description="List tabs in the level editor",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_editor_open_tab",
                description="Open a tab by ID",
                inputSchema={
                    "type": "object",
                    "properties": {"tabId": {"type": "string"}},
                    "required": ["tabId"],
                },
            ),
            Tool(
                name="ue5_editor_get_mode",
                description="Get current editor mode",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_editor_set_mode",
                description="Set editor mode (e.g., Landscape, Foliage)",
                inputSchema={
                    "type": "object",
                    "properties": {"modeId": {"type": "string"}},
                    "required": ["modeId"],
                },
            ),
            Tool(
                name="ue5_editor_get_transform_mode",
                description="Get current transform gizmo mode",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_editor_set_transform_mode",
                description="Set transform mode (Translate, Rotate, Scale)",
                inputSchema={
                    "type": "object",
                    "properties": {"mode": {"type": "string"}},
                    "required": ["mode"],
                },
            ),
            Tool(
                name="ue5_editor_get_snap_settings",
                description="Get current snap settings",
                inputSchema={"type": "object", "properties": {}, "required": []},
            ),
            Tool(
                name="ue5_editor_set_snap_settings",
                description="Configure snap settings",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "gridSnapEnabled": {"type": "boolean"},
                        "gridSize": {"type": "number"},
                        "rotationSnapEnabled": {"type": "boolean"},
                        "rotationSnapAngle": {"type": "number"},
                    },
                    "required": [],
                },
            ),
            Tool(
                name="ue5_editor_show_notification",
                description="Show a notification in the editor",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "message": {"type": "string"},
                        "type": {"type": "string", "description": "success, error, warning, or info"},
                        "duration": {"type": "number", "default": 3.0},
                    },
                    "required": ["message"],
                },
            ),
            Tool(
                name="ue5_editor_execute_command",
                description="Execute an editor command",
                inputSchema={
                    "type": "object",
                    "properties": {"command": {"type": "string"}},
                    "required": ["command"],
                },
            ),
            Tool(
                name="ue5_editor_open_settings",
                description="Open project settings to a category",
                inputSchema={
                    "type": "object",
                    "properties": {"category": {"type": "string"}},
                    "required": [],
                },
            ),
        ]

    @server.call_tool()
    async def call_tool(name: str, arguments: dict) -> list[TextContent]:
        """Handle tool calls."""
        try:
            async with UltimateControlClient(
                host=UE_HOST,
                port=UE_PORT,
                token=UE_TOKEN if UE_TOKEN else None,
            ) as client:
                result = await _execute_tool(client, name, arguments)
                return [TextContent(type="text", text=json.dumps(result, indent=2))]
        except UltimateControlError as e:
            return [
                TextContent(
                    type="text",
                    text=json.dumps(
                        {
                            "error": True,
                            "code": e.code,
                            "message": e.message,
                            "data": e.data,
                        },
                        indent=2,
                    ),
                )
            ]
        except Exception as e:
            return [
                TextContent(
                    type="text",
                    text=json.dumps(
                        {
                            "error": True,
                            "message": str(e),
                        },
                        indent=2,
                    ),
                )
            ]

    return server


async def _execute_tool(
    client: UltimateControlClient,
    name: str,
    arguments: dict,
) -> Any:
    """Execute a tool by name."""
    # Map tool names to JSON-RPC methods
    tool_map = {
        # System
        "ue5_system_info": ("system.getInfo", {}),
        "ue5_list_methods": ("system.listMethods", {}),
        # Project
        "ue5_project_info": ("project.getInfo", {}),
        "ue5_project_save": ("project.save", arguments),
        "ue5_list_plugins": ("project.listPlugins", arguments),
        # Asset
        "ue5_asset_list": ("asset.list", arguments),
        "ue5_asset_get": ("asset.get", arguments),
        "ue5_asset_search": ("asset.search", arguments),
        "ue5_asset_duplicate": ("asset.duplicate", arguments),
        "ue5_asset_delete": ("asset.delete", arguments),
        "ue5_asset_import": ("asset.import", arguments),
        # Blueprint
        "ue5_blueprint_list": ("blueprint.list", arguments),
        "ue5_blueprint_get": ("blueprint.get", arguments),
        "ue5_blueprint_get_graphs": ("blueprint.getGraphs", arguments),
        "ue5_blueprint_get_variables": ("blueprint.getVariables", arguments),
        "ue5_blueprint_compile": ("blueprint.compile", arguments),
        "ue5_blueprint_create": ("blueprint.create", arguments),
        "ue5_blueprint_add_variable": ("blueprint.addVariable", arguments),
        # Level
        "ue5_level_get_current": ("level.getCurrent", {}),
        "ue5_level_open": ("level.open", arguments),
        "ue5_level_save": ("level.save", {}),
        "ue5_level_list": ("level.list", arguments),
        # Actor
        "ue5_actor_list": ("actor.list", arguments),
        "ue5_actor_get": ("actor.get", arguments),
        "ue5_actor_spawn": ("actor.spawn", arguments),
        "ue5_actor_destroy": ("actor.destroy", arguments),
        "ue5_actor_set_transform": ("actor.setTransform", arguments),
        "ue5_actor_get_components": ("actor.getComponents", arguments),
        # Selection
        "ue5_selection_get": ("selection.get", {}),
        "ue5_selection_set": ("selection.set", arguments),
        # PIE
        "ue5_pie_play": ("pie.play", arguments),
        "ue5_pie_stop": ("pie.stop", {}),
        "ue5_pie_pause": ("pie.pause", arguments),
        "ue5_pie_get_state": ("pie.getState", {}),
        # Console
        "ue5_console_execute": ("console.execute", arguments),
        "ue5_console_get_variable": ("console.getVariable", arguments),
        "ue5_console_set_variable": ("console.setVariable", arguments),
        "ue5_console_list_variables": ("console.listVariables", arguments),
        # Profiling
        "ue5_profiling_get_stats": ("profiling.getStats", {}),
        "ue5_profiling_get_memory": ("profiling.getMemory", {}),
        # Automation
        "ue5_automation_list_tests": ("automation.listTests", arguments),
        "ue5_automation_run_tests": ("automation.runTests", arguments),
        # File
        "ue5_file_read": ("file.read", arguments),
        "ue5_file_write": ("file.write", arguments),
        "ue5_file_list": ("file.list", arguments),
        # Viewport/Camera
        "ue5_viewport_list": ("viewport.list", {}),
        "ue5_viewport_get_active": ("viewport.getActive", {}),
        "ue5_viewport_set_active": ("viewport.setActive", arguments),
        "ue5_viewport_get_camera": ("viewport.getCamera", arguments),
        "ue5_viewport_set_camera": ("viewport.setCamera", arguments),
        "ue5_viewport_focus_actor": ("viewport.focusActor", arguments),
        "ue5_viewport_take_screenshot": ("viewport.screenshot", arguments),
        "ue5_viewport_set_view_mode": ("viewport.setViewMode", arguments),
        # Transaction/Undo
        "ue5_transaction_begin": ("transaction.begin", arguments),
        "ue5_transaction_end": ("transaction.end", {}),
        "ue5_transaction_cancel": ("transaction.cancel", {}),
        "ue5_undo": ("transaction.undo", {}),
        "ue5_redo": ("transaction.redo", {}),
        "ue5_undo_history": ("transaction.getHistory", arguments),
        # Material
        "ue5_material_list": ("material.list", arguments),
        "ue5_material_get": ("material.get", arguments),
        "ue5_material_create": ("material.create", arguments),
        "ue5_material_get_parameters": ("material.getParameters", arguments),
        "ue5_material_set_scalar": ("material.setScalar", arguments),
        "ue5_material_set_vector": ("material.setVector", arguments),
        "ue5_material_set_texture": ("material.setTexture", arguments),
        "ue5_material_instance_create": ("material.createInstance", arguments),
        # Animation
        "ue5_animation_list": ("animation.list", arguments),
        "ue5_animation_get": ("animation.get", arguments),
        "ue5_animation_play": ("animation.play", arguments),
        "ue5_animation_stop": ("animation.stop", arguments),
        "ue5_animation_blueprint_list": ("animation.listBlueprints", arguments),
        # Sequencer
        "ue5_sequencer_list": ("sequencer.list", arguments),
        "ue5_sequencer_get": ("sequencer.get", arguments),
        "ue5_sequencer_open": ("sequencer.open", arguments),
        "ue5_sequencer_play": ("sequencer.play", {}),
        "ue5_sequencer_stop": ("sequencer.stop", {}),
        "ue5_sequencer_set_time": ("sequencer.setTime", arguments),
        "ue5_sequencer_get_tracks": ("sequencer.getTracks", arguments),
        "ue5_sequencer_add_actor": ("sequencer.addActor", arguments),
        # Audio
        "ue5_audio_list": ("audio.list", arguments),
        "ue5_audio_play": ("audio.play", arguments),
        "ue5_audio_stop_all": ("audio.stopAll", {}),
        "ue5_audio_set_volume": ("audio.setVolume", arguments),
        # Physics
        "ue5_physics_simulate": ("physics.simulate", arguments),
        "ue5_physics_get_body_info": ("physics.getBodyInfo", arguments),
        "ue5_physics_set_simulate": ("physics.setSimulate", arguments),
        "ue5_physics_apply_impulse": ("physics.applyImpulse", arguments),
        "ue5_collision_list_profiles": ("physics.listCollisionProfiles", {}),
        # Lighting
        "ue5_light_list": ("lighting.list", {}),
        "ue5_light_get": ("lighting.get", arguments),
        "ue5_light_set_intensity": ("lighting.setIntensity", arguments),
        "ue5_light_set_color": ("lighting.setColor", arguments),
        "ue5_light_build": ("lighting.build", arguments),
        "ue5_light_build_reflection_captures": ("lighting.buildReflectionCaptures", {}),
        # World Partition / Data Layer
        "ue5_world_partition_status": ("worldPartition.getStatus", {}),
        "ue5_world_partition_list_cells": ("worldPartition.listCells", {}),
        "ue5_world_partition_load_cells": ("worldPartition.loadCells", arguments),
        "ue5_data_layer_list": ("dataLayer.list", {}),
        "ue5_data_layer_set_visibility": ("dataLayer.setVisibility", arguments),
        "ue5_data_layer_get_actors": ("dataLayer.getActors", arguments),
        # Niagara/VFX
        "ue5_niagara_list": ("niagara.listSystems", arguments),
        "ue5_niagara_spawn": ("niagara.spawn", arguments),
        "ue5_niagara_activate": ("niagara.activate", arguments),
        "ue5_niagara_deactivate": ("niagara.deactivate", arguments),
        "ue5_niagara_set_float": ("niagara.setFloat", arguments),
        "ue5_niagara_set_vector": ("niagara.setVector", arguments),
        # Landscape
        "ue5_landscape_list": ("landscape.list", {}),
        "ue5_landscape_get_height": ("landscape.getHeightAtLocation", arguments),
        "ue5_landscape_list_layers": ("landscape.listLayers", {}),
        "ue5_landscape_get_material": ("landscape.getMaterial", arguments),
        # AI/Navigation
        "ue5_navigation_build": ("navigation.build", {}),
        "ue5_navigation_status": ("navigation.getStatus", {}),
        "ue5_navigation_find_path": ("navigation.findPath", arguments),
        "ue5_ai_list_controllers": ("ai.listControllers", {}),
        "ue5_ai_move_to": ("ai.moveToLocation", arguments),
        "ue5_behavior_tree_list": ("behaviorTree.list", arguments),
        "ue5_behavior_tree_run": ("behaviorTree.run", arguments),
        # Render/PostProcess
        "ue5_render_get_settings": ("render.getQualitySettings", {}),
        "ue5_render_set_quality": ("render.setQualitySettings", arguments),
        "ue5_postprocess_list_volumes": ("postProcess.listVolumes", {}),
        "ue5_postprocess_get": ("postProcess.getVolume", arguments),
        "ue5_postprocess_set_bloom": ("postProcess.setBloomIntensity", arguments),
        "ue5_postprocess_set_exposure": ("postProcess.setExposure", arguments),
        "ue5_render_get_show_flags": ("render.getShowFlags", {}),
        "ue5_render_set_show_flag": ("render.setShowFlag", arguments),
        # Outliner/Hierarchy
        "ue5_outliner_get_hierarchy": ("outliner.getHierarchy", {}),
        "ue5_outliner_set_parent": ("outliner.setParent", arguments),
        "ue5_outliner_detach": ("outliner.detachFromParent", arguments),
        "ue5_outliner_list_folders": ("outliner.listFolders", {}),
        "ue5_outliner_create_folder": ("outliner.createFolder", arguments),
        "ue5_outliner_set_folder": ("outliner.setActorFolder", arguments),
        "ue5_layer_list": ("layer.list", {}),
        "ue5_layer_create": ("layer.create", arguments),
        "ue5_layer_add_actor": ("layer.addActor", arguments),
        "ue5_outliner_search": ("outliner.search", arguments),
        # Source Control
        "ue5_source_control_status": ("sourceControl.getProviderStatus", {}),
        "ue5_source_control_file_status": ("sourceControl.getFileStatus", arguments),
        "ue5_source_control_checkout": ("sourceControl.checkOut", arguments),
        "ue5_source_control_checkin": ("sourceControl.checkIn", arguments),
        "ue5_source_control_revert": ("sourceControl.revert", arguments),
        "ue5_source_control_sync": ("sourceControl.sync", arguments),
        "ue5_source_control_history": ("sourceControl.getHistory", arguments),
        # Live Coding
        "ue5_live_coding_status": ("liveCoding.isEnabled", {}),
        "ue5_live_coding_compile": ("liveCoding.compile", {}),
        "ue5_live_coding_enable": ("liveCoding.enable", {}),
        "ue5_live_coding_disable": ("liveCoding.disable", {}),
        "ue5_hot_reload": ("hotReload.reload", {}),
        "ue5_module_list": ("module.list", {}),
        # Multi-User Session
        "ue5_session_list": ("session.list", {}),
        "ue5_session_current": ("session.getCurrent", {}),
        "ue5_session_join": ("session.join", arguments),
        "ue5_session_leave": ("session.leave", {}),
        "ue5_session_users": ("session.listUsers", {}),
        "ue5_session_lock": ("session.lockObject", arguments),
        "ue5_session_unlock": ("session.unlockObject", arguments),
        # Editor UI
        "ue5_editor_list_windows": ("editor.listWindows", {}),
        "ue5_editor_focus_window": ("editor.focusWindow", arguments),
        "ue5_editor_list_tabs": ("editor.listTabs", {}),
        "ue5_editor_open_tab": ("editor.openTab", arguments),
        "ue5_editor_get_mode": ("editor.getCurrentMode", {}),
        "ue5_editor_set_mode": ("editor.setMode", arguments),
        "ue5_editor_get_transform_mode": ("editor.getTransformMode", {}),
        "ue5_editor_set_transform_mode": ("editor.setTransformMode", arguments),
        "ue5_editor_get_snap_settings": ("editor.getSnapSettings", {}),
        "ue5_editor_set_snap_settings": ("editor.setSnapSettings", arguments),
        "ue5_editor_show_notification": ("editor.showNotification", arguments),
        "ue5_editor_execute_command": ("editor.executeCommand", arguments),
        "ue5_editor_open_settings": ("editor.openProjectSettings", arguments),
    }

    if name not in tool_map:
        raise ValueError(f"Unknown tool: {name}")

    method, params = tool_map[name]
    return await client.call(method, params if params else None)


async def run_server():
    """Run the MCP server."""
    server = create_server()
    async with stdio_server() as (read_stream, write_stream):
        await server.run(read_stream, write_stream, server.create_initialization_options())


def main():
    """Entry point for the MCP server."""
    asyncio.run(run_server())


if __name__ == "__main__":
    main()
