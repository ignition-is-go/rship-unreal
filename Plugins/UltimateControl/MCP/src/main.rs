//! UE5 MCP Server - Model Context Protocol server for UltimateControl plugin
//!
//! This is a standalone executable that implements the MCP protocol over stdio,
//! bridging AI agents to the UltimateControl plugin's HTTP JSON-RPC API.

use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::env;
use std::io::{self, BufRead, Write};
use uuid::Uuid;

/// Configuration from environment variables
struct Config {
    host: String,
    port: u16,
    token: Option<String>,
}

impl Config {
    fn from_env() -> Self {
        Self {
            host: env::var("UE5_MCP_HOST").unwrap_or_else(|_| "127.0.0.1".to_string()),
            port: env::var("UE5_MCP_PORT")
                .ok()
                .and_then(|s| s.parse().ok())
                .unwrap_or(7777),
            token: env::var("UE5_MCP_TOKEN").ok(),
        }
    }

    fn base_url(&self) -> String {
        format!("http://{}:{}/rpc", self.host, self.port)
    }
}

/// JSON-RPC client for UltimateControl
struct UltimateControlClient {
    config: Config,
    client: reqwest::Client,
}

impl UltimateControlClient {
    fn new(config: Config) -> Self {
        Self {
            config,
            client: reqwest::Client::new(),
        }
    }

    async fn call(&self, method: &str, params: Option<Value>) -> Result<Value> {
        let request_id = Uuid::new_v4().to_string();

        let mut payload = json!({
            "jsonrpc": "2.0",
            "method": method,
            "id": request_id,
        });

        if let Some(p) = params {
            payload["params"] = p;
        }

        let mut request = self.client.post(&self.config.base_url()).json(&payload);

        if let Some(ref token) = self.config.token {
            request = request.header("X-Ultimate-Control-Token", token);
        }

        let response = request.send().await?;
        let result: Value = response.json().await?;

        if let Some(error) = result.get("error") {
            let code = error.get("code").and_then(|v| v.as_i64()).unwrap_or(-1);
            let message = error
                .get("message")
                .and_then(|v| v.as_str())
                .unwrap_or("Unknown error");
            return Err(anyhow!("[{}] {}", code, message));
        }

        Ok(result.get("result").cloned().unwrap_or(Value::Null))
    }
}

/// MCP Message Types
#[derive(Debug, Deserialize)]
struct McpRequest {
    jsonrpc: String,
    id: Value,
    method: String,
    #[serde(default)]
    params: Option<Value>,
}

#[derive(Debug, Serialize)]
struct McpResponse {
    jsonrpc: String,
    id: Value,
    #[serde(skip_serializing_if = "Option::is_none")]
    result: Option<Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<McpError>,
}

#[derive(Debug, Serialize)]
struct McpError {
    code: i32,
    message: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    data: Option<Value>,
}

/// Tool definition
#[derive(Debug, Serialize)]
struct Tool {
    name: String,
    description: String,
    #[serde(rename = "inputSchema")]
    input_schema: Value,
}

/// Helper to create a tool definition
fn tool(name: &str, description: &str, schema: Value) -> Tool {
    Tool {
        name: name.to_string(),
        description: description.to_string(),
        input_schema: schema,
    }
}

/// Get all available tools - 100+ tools for comprehensive UE5 control
fn get_tools() -> Vec<Tool> {
    vec![
        // ===== System Tools =====
        tool(
            "ue5_system_info",
            "Get information about the UE5 editor and UltimateControl server",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_list_methods",
            "List all available JSON-RPC methods",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_system_health",
            "Get health/readiness details for orchestration loops",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_rpc_call",
            "Call any UltimateControl JSON-RPC method directly",
            json!({"type": "object", "properties": {
                "method": {"type": "string"},
                "params": {"type": "object"}
            }, "required": ["method"]}),
        ),
        // ===== Project Tools =====
        tool(
            "ue5_project_info",
            "Get information about the current Unreal Engine project",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_project_save",
            "Save all dirty packages in the project",
            json!({"type": "object", "properties": {"prompt": {"type": "boolean", "default": false}}, "required": []}),
        ),
        // ===== Asset Tools =====
        tool(
            "ue5_asset_list",
            "List assets in a content directory",
            json!({"type": "object", "properties": {
                "path": {"type": "string", "default": "/Game"},
                "class": {"type": "string"},
                "recursive": {"type": "boolean", "default": true},
                "limit": {"type": "integer", "default": 100}
            }, "required": []}),
        ),
        tool(
            "ue5_asset_get",
            "Get detailed information about a specific asset",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_asset_search",
            "Search for assets by name pattern",
            json!({"type": "object", "properties": {"query": {"type": "string"}, "class": {"type": "string"}, "limit": {"type": "integer", "default": 50}}, "required": ["query"]}),
        ),
        tool(
            "ue5_asset_duplicate",
            "Duplicate an asset",
            json!({"type": "object", "properties": {"source": {"type": "string"}, "destination": {"type": "string"}}, "required": ["source", "destination"]}),
        ),
        tool(
            "ue5_asset_delete",
            "Delete an asset (dangerous)",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_asset_import",
            "Import an external file as an asset",
            json!({"type": "object", "properties": {"file": {"type": "string"}, "destination": {"type": "string"}}, "required": ["file", "destination"]}),
        ),
        tool(
            "ue5_asset_export",
            "Export an asset to a file",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "destination": {"type": "string"}}, "required": ["path", "destination"]}),
        ),
        // ===== Blueprint Tools =====
        tool(
            "ue5_blueprint_list",
            "List all blueprints in a path",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}, "limit": {"type": "integer", "default": 100}}, "required": []}),
        ),
        tool(
            "ue5_blueprint_get",
            "Get detailed blueprint information",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_blueprint_compile",
            "Compile a blueprint",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_blueprint_create",
            "Create a new blueprint class",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "parentClass": {"type": "string", "default": "Actor"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_blueprint_get_graphs",
            "Get blueprint graph information",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_blueprint_get_variables",
            "Get blueprint variables",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_blueprint_add_variable",
            "Add a variable to a blueprint",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "name": {"type": "string"}, "type": {"type": "string"}}, "required": ["path", "name", "type"]}),
        ),
        // ===== Level/Actor Tools =====
        tool(
            "ue5_level_get_current",
            "Get information about the currently loaded level",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_level_open",
            "Open a level by path",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "promptSave": {"type": "boolean", "default": true}}, "required": ["path"]}),
        ),
        tool(
            "ue5_level_save",
            "Save the current level",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_level_list",
            "List all levels in the project",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}}, "required": []}),
        ),
        tool(
            "ue5_actor_list",
            "List actors in the current level",
            json!({"type": "object", "properties": {"class": {"type": "string"}, "tag": {"type": "string"}, "limit": {"type": "integer", "default": 100}}, "required": []}),
        ),
        tool(
            "ue5_actor_get",
            "Get detailed information about an actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_actor_spawn",
            "Spawn a new actor in the level",
            json!({"type": "object", "properties": {"class": {"type": "string"}, "name": {"type": "string"}, "location": {"type": "object"}, "rotation": {"type": "object"}}, "required": ["class"]}),
        ),
        tool(
            "ue5_actor_destroy",
            "Destroy an actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_actor_set_transform",
            "Set an actor's transform",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "location": {"type": "object"}, "rotation": {"type": "object"}, "scale": {"type": "object"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_actor_get_property",
            "Get a property value from an actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "property": {"type": "string"}}, "required": ["actor", "property"]}),
        ),
        tool(
            "ue5_actor_set_property",
            "Set a property value on an actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "property": {"type": "string"}, "value": {}}, "required": ["actor", "property", "value"]}),
        ),
        tool(
            "ue5_selection_get",
            "Get currently selected actors",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_selection_set",
            "Set the selected actors",
            json!({"type": "object", "properties": {"actors": {"type": "array", "items": {"type": "string"}}, "add": {"type": "boolean", "default": false}}, "required": ["actors"]}),
        ),
        // ===== PIE Tools =====
        tool(
            "ue5_pie_play",
            "Start Play In Editor session",
            json!({"type": "object", "properties": {"mode": {"type": "string", "default": "SelectedViewport"}}, "required": []}),
        ),
        tool(
            "ue5_pie_stop",
            "Stop PIE session",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_pie_pause",
            "Pause/resume PIE",
            json!({"type": "object", "properties": {"pause": {"type": "boolean"}}, "required": []}),
        ),
        tool(
            "ue5_pie_get_state",
            "Get PIE state",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        // ===== Console Tools =====
        tool(
            "ue5_console_execute",
            "Execute a console command",
            json!({"type": "object", "properties": {"command": {"type": "string"}}, "required": ["command"]}),
        ),
        tool(
            "ue5_console_get_variable",
            "Get a console variable value",
            json!({"type": "object", "properties": {"name": {"type": "string"}}, "required": ["name"]}),
        ),
        tool(
            "ue5_console_set_variable",
            "Set a console variable",
            json!({"type": "object", "properties": {"name": {"type": "string"}, "value": {"type": "string"}}, "required": ["name", "value"]}),
        ),
        tool(
            "ue5_console_list_variables",
            "List console variables",
            json!({"type": "object", "properties": {"filter": {"type": "string"}}, "required": []}),
        ),
        // ===== Viewport/Camera Tools =====
        tool(
            "ue5_viewport_list",
            "List all active viewports",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_viewport_get_active",
            "Get the currently active viewport",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_viewport_set_active",
            "Set the active viewport by index",
            json!({"type": "object", "properties": {"index": {"type": "integer"}}, "required": ["index"]}),
        ),
        tool(
            "ue5_viewport_get_camera",
            "Get viewport camera position and rotation",
            json!({"type": "object", "properties": {"viewportIndex": {"type": "integer", "default": 0}}, "required": []}),
        ),
        tool(
            "ue5_viewport_set_camera",
            "Set viewport camera position and rotation",
            json!({"type": "object", "properties": {"viewportIndex": {"type": "integer", "default": 0}, "location": {"type": "object"}, "rotation": {"type": "object"}}, "required": []}),
        ),
        tool(
            "ue5_viewport_focus_actor",
            "Focus viewport on a specific actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_viewport_screenshot",
            "Take a screenshot of the viewport",
            json!({"type": "object", "properties": {"filename": {"type": "string"}, "viewportIndex": {"type": "integer", "default": 0}}, "required": ["filename"]}),
        ),
        tool(
            "ue5_viewport_set_view_mode",
            "Set viewport view mode (Lit, Unlit, Wireframe)",
            json!({"type": "object", "properties": {"mode": {"type": "string"}}, "required": ["mode"]}),
        ),
        // ===== Transaction/Undo Tools =====
        tool(
            "ue5_transaction_begin",
            "Begin a new undo transaction",
            json!({"type": "object", "properties": {"description": {"type": "string"}}, "required": ["description"]}),
        ),
        tool(
            "ue5_transaction_end",
            "End the current transaction",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_transaction_cancel",
            "Cancel the current transaction",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_undo",
            "Undo the last action",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_redo",
            "Redo the last undone action",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_undo_history",
            "Get the undo/redo history",
            json!({"type": "object", "properties": {"limit": {"type": "integer", "default": 20}}, "required": []}),
        ),
        // ===== Material Tools =====
        tool(
            "ue5_material_list",
            "List materials in a path",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}}, "required": []}),
        ),
        tool(
            "ue5_material_get",
            "Get detailed material information",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_material_create",
            "Create a new material",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "template": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_material_get_parameters",
            "Get material parameters",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_material_set_scalar",
            "Set a scalar parameter on a material instance",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "parameter": {"type": "string"}, "value": {"type": "number"}}, "required": ["path", "parameter", "value"]}),
        ),
        tool(
            "ue5_material_set_vector",
            "Set a vector parameter on a material instance",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "parameter": {"type": "string"}, "r": {"type": "number"}, "g": {"type": "number"}, "b": {"type": "number"}, "a": {"type": "number", "default": 1.0}}, "required": ["path", "parameter", "r", "g", "b"]}),
        ),
        tool(
            "ue5_material_set_texture",
            "Set a texture parameter on a material instance",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "parameter": {"type": "string"}, "texturePath": {"type": "string"}}, "required": ["path", "parameter", "texturePath"]}),
        ),
        tool(
            "ue5_material_instance_create",
            "Create a material instance from a parent",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "parent": {"type": "string"}}, "required": ["path", "parent"]}),
        ),
        // ===== Animation Tools =====
        tool(
            "ue5_animation_list",
            "List animation assets",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}}, "required": []}),
        ),
        tool(
            "ue5_animation_get",
            "Get animation asset details",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_animation_play",
            "Play an animation on an actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "animation": {"type": "string"}, "loop": {"type": "boolean", "default": false}}, "required": ["actor", "animation"]}),
        ),
        tool(
            "ue5_animation_stop",
            "Stop animation on an actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_animation_blueprint_list",
            "List Animation Blueprints",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}}, "required": []}),
        ),
        // ===== Sequencer Tools =====
        tool(
            "ue5_sequencer_list",
            "List Level Sequences in the project",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}}, "required": []}),
        ),
        tool(
            "ue5_sequencer_get",
            "Get sequence details",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_sequencer_open",
            "Open a sequence in the Sequencer editor",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_sequencer_play",
            "Play the current sequence",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_sequencer_stop",
            "Stop sequence playback",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_sequencer_set_time",
            "Set the playhead time in the sequence",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "time": {"type": "number"}}, "required": ["path", "time"]}),
        ),
        tool(
            "ue5_sequencer_get_tracks",
            "Get all tracks in a sequence",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_sequencer_add_actor",
            "Add an actor binding to a sequence",
            json!({"type": "object", "properties": {"sequence": {"type": "string"}, "actor": {"type": "string"}}, "required": ["sequence", "actor"]}),
        ),
        // ===== Audio Tools =====
        tool(
            "ue5_audio_list",
            "List audio assets",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}}, "required": []}),
        ),
        tool(
            "ue5_audio_play",
            "Play a sound at a location or attached to actor",
            json!({"type": "object", "properties": {"sound": {"type": "string"}, "actor": {"type": "string"}, "location": {"type": "object"}}, "required": ["sound"]}),
        ),
        tool(
            "ue5_audio_stop_all",
            "Stop all playing sounds",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_audio_set_volume",
            "Set the master audio volume",
            json!({"type": "object", "properties": {"volume": {"type": "number"}}, "required": ["volume"]}),
        ),
        // ===== Physics Tools =====
        tool(
            "ue5_physics_simulate",
            "Enable or disable physics simulation in editor",
            json!({"type": "object", "properties": {"enable": {"type": "boolean"}}, "required": ["enable"]}),
        ),
        tool(
            "ue5_physics_get_body_info",
            "Get physics body information for an actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_physics_set_simulate",
            "Set physics simulation for an actor",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "simulate": {"type": "boolean"}}, "required": ["actor", "simulate"]}),
        ),
        tool(
            "ue5_physics_apply_impulse",
            "Apply an impulse to a physics body",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "impulse": {"type": "object"}}, "required": ["actor", "impulse"]}),
        ),
        tool(
            "ue5_collision_list_profiles",
            "List collision profiles",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        // ===== Lighting Tools =====
        tool(
            "ue5_light_list",
            "List all lights in the current level",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_light_get",
            "Get light properties",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_light_set_intensity",
            "Set light intensity",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "intensity": {"type": "number"}}, "required": ["actor", "intensity"]}),
        ),
        tool(
            "ue5_light_set_color",
            "Set light color",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "r": {"type": "number"}, "g": {"type": "number"}, "b": {"type": "number"}}, "required": ["actor", "r", "g", "b"]}),
        ),
        tool(
            "ue5_light_build",
            "Build lighting for the level",
            json!({"type": "object", "properties": {"quality": {"type": "string", "default": "Production"}}, "required": []}),
        ),
        tool(
            "ue5_light_build_reflection_captures",
            "Build reflection captures",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        // ===== World Partition / Data Layer Tools =====
        tool(
            "ue5_world_partition_status",
            "Get World Partition status",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_world_partition_list_cells",
            "List World Partition cells",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_world_partition_load_cells",
            "Load specific cells",
            json!({"type": "object", "properties": {"cells": {"type": "array", "items": {"type": "string"}}}, "required": ["cells"]}),
        ),
        tool(
            "ue5_data_layer_list",
            "List all data layers",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_data_layer_set_visibility",
            "Set data layer visibility",
            json!({"type": "object", "properties": {"layer": {"type": "string"}, "visible": {"type": "boolean"}}, "required": ["layer", "visible"]}),
        ),
        tool(
            "ue5_data_layer_get_actors",
            "Get actors in a data layer",
            json!({"type": "object", "properties": {"layer": {"type": "string"}}, "required": ["layer"]}),
        ),
        // ===== Niagara/VFX Tools =====
        tool(
            "ue5_niagara_list",
            "List Niagara systems",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}}, "required": []}),
        ),
        tool(
            "ue5_niagara_spawn",
            "Spawn a Niagara system at a location",
            json!({"type": "object", "properties": {"system": {"type": "string"}, "location": {"type": "object"}}, "required": ["system"]}),
        ),
        tool(
            "ue5_niagara_activate",
            "Activate a Niagara component",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_niagara_deactivate",
            "Deactivate a Niagara component",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_niagara_set_float",
            "Set a float parameter on a Niagara component",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "parameter": {"type": "string"}, "value": {"type": "number"}}, "required": ["actor", "parameter", "value"]}),
        ),
        tool(
            "ue5_niagara_set_vector",
            "Set a vector parameter on a Niagara component",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "parameter": {"type": "string"}, "x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}, "required": ["actor", "parameter", "x", "y", "z"]}),
        ),
        // ===== Landscape Tools =====
        tool(
            "ue5_landscape_list",
            "List landscapes in the level",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_landscape_get_height",
            "Get height at a world location",
            json!({"type": "object", "properties": {"x": {"type": "number"}, "y": {"type": "number"}}, "required": ["x", "y"]}),
        ),
        tool(
            "ue5_landscape_list_layers",
            "List landscape paint layers",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_landscape_get_material",
            "Get landscape material",
            json!({"type": "object", "properties": {"landscape": {"type": "string"}}, "required": ["landscape"]}),
        ),
        // ===== AI/Navigation Tools =====
        tool(
            "ue5_navigation_build",
            "Build navigation mesh",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_navigation_status",
            "Get navigation mesh status",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_navigation_find_path",
            "Find a navigation path between two points",
            json!({"type": "object", "properties": {"start": {"type": "object"}, "end": {"type": "object"}}, "required": ["start", "end"]}),
        ),
        tool(
            "ue5_ai_list_controllers",
            "List AI controllers in the level",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_ai_move_to",
            "Command an AI to move to a location",
            json!({"type": "object", "properties": {"controller": {"type": "string"}, "location": {"type": "object"}}, "required": ["controller", "location"]}),
        ),
        tool(
            "ue5_behavior_tree_list",
            "List Behavior Tree assets",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": "/Game"}}, "required": []}),
        ),
        tool(
            "ue5_behavior_tree_run",
            "Run a behavior tree on an AI controller",
            json!({"type": "object", "properties": {"controller": {"type": "string"}, "tree": {"type": "string"}}, "required": ["controller", "tree"]}),
        ),
        // ===== Render/PostProcess Tools =====
        tool(
            "ue5_render_get_settings",
            "Get current render/quality settings",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_render_set_quality",
            "Set render quality level",
            json!({"type": "object", "properties": {"level": {"type": "integer"}}, "required": ["level"]}),
        ),
        tool(
            "ue5_postprocess_list_volumes",
            "List PostProcess volumes in the level",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_postprocess_get",
            "Get PostProcess volume settings",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_postprocess_set_bloom",
            "Set bloom intensity on a PostProcess volume",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "intensity": {"type": "number"}}, "required": ["actor", "intensity"]}),
        ),
        tool(
            "ue5_postprocess_set_exposure",
            "Set exposure settings",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "minEV": {"type": "number"}, "maxEV": {"type": "number"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_render_get_show_flags",
            "Get viewport show flags",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_render_set_show_flag",
            "Set a viewport show flag",
            json!({"type": "object", "properties": {"flag": {"type": "string"}, "enabled": {"type": "boolean"}}, "required": ["flag", "enabled"]}),
        ),
        // ===== Outliner/Hierarchy Tools =====
        tool(
            "ue5_outliner_get_hierarchy",
            "Get the actor hierarchy from the scene outliner",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_outliner_set_parent",
            "Set an actor's parent (attach)",
            json!({"type": "object", "properties": {"child": {"type": "string"}, "parent": {"type": "string"}}, "required": ["child", "parent"]}),
        ),
        tool(
            "ue5_outliner_detach",
            "Detach an actor from its parent",
            json!({"type": "object", "properties": {"actor": {"type": "string"}}, "required": ["actor"]}),
        ),
        tool(
            "ue5_outliner_list_folders",
            "List folders in the scene outliner",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_outliner_create_folder",
            "Create a folder in the scene outliner",
            json!({"type": "object", "properties": {"name": {"type": "string"}}, "required": ["name"]}),
        ),
        tool(
            "ue5_outliner_set_folder",
            "Move an actor to a folder",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "folder": {"type": "string"}}, "required": ["actor", "folder"]}),
        ),
        tool(
            "ue5_layer_list",
            "List editor layers",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_layer_create",
            "Create an editor layer",
            json!({"type": "object", "properties": {"name": {"type": "string"}}, "required": ["name"]}),
        ),
        tool(
            "ue5_layer_add_actor",
            "Add an actor to a layer",
            json!({"type": "object", "properties": {"actor": {"type": "string"}, "layer": {"type": "string"}}, "required": ["actor", "layer"]}),
        ),
        tool(
            "ue5_outliner_search",
            "Search for actors in the outliner",
            json!({"type": "object", "properties": {"query": {"type": "string"}}, "required": ["query"]}),
        ),
        // ===== Source Control Tools =====
        tool(
            "ue5_source_control_status",
            "Get source control provider status",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_source_control_file_status",
            "Get source control status for a file",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_source_control_checkout",
            "Check out a file",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_source_control_checkin",
            "Check in a file with a description",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "description": {"type": "string"}}, "required": ["path", "description"]}),
        ),
        tool(
            "ue5_source_control_revert",
            "Revert changes to a file",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_source_control_sync",
            "Sync file(s) to latest",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_source_control_history",
            "Get file history",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        // ===== Live Coding Tools =====
        tool(
            "ue5_live_coding_status",
            "Get Live Coding status",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_live_coding_compile",
            "Trigger a Live Coding compile",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_live_coding_enable",
            "Enable Live Coding",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_live_coding_disable",
            "Disable Live Coding",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_hot_reload",
            "Trigger a hot reload (legacy)",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_module_list",
            "List loaded modules",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        // ===== Multi-User Session Tools =====
        tool(
            "ue5_session_list",
            "List available Multi-User sessions",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_session_current",
            "Get current session info",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_session_join",
            "Join a Multi-User session",
            json!({"type": "object", "properties": {"session": {"type": "string"}}, "required": ["session"]}),
        ),
        tool(
            "ue5_session_leave",
            "Leave the current Multi-User session",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_session_users",
            "List users in the current session",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_session_lock",
            "Lock an object for editing",
            json!({"type": "object", "properties": {"object": {"type": "string"}}, "required": ["object"]}),
        ),
        tool(
            "ue5_session_unlock",
            "Unlock an object",
            json!({"type": "object", "properties": {"object": {"type": "string"}}, "required": ["object"]}),
        ),
        // ===== Agent Orchestration Tools =====
        tool(
            "ue5_agent_register",
            "Register an AI agent with capability metadata",
            json!({"type": "object", "properties": {
                "agentId": {"type": "string"},
                "role": {"type": "string"},
                "sessionId": {"type": "string"},
                "status": {"type": "string"},
                "capabilities": {"type": "array", "items": {"type": "string"}},
                "metadata": {"type": "object"}
            }, "required": ["agentId"]}),
        ),
        tool(
            "ue5_agent_heartbeat",
            "Update heartbeat for a registered agent",
            json!({"type": "object", "properties": {
                "agentId": {"type": "string"},
                "status": {"type": "string"},
                "currentTaskId": {"type": "string"},
                "metadata": {"type": "object"}
            }, "required": ["agentId"]}),
        ),
        tool(
            "ue5_agent_list",
            "List known agents with online/offline state",
            json!({"type": "object", "properties": {
                "includeOffline": {"type": "boolean", "default": true},
                "staleAfterSeconds": {"type": "integer", "default": 120}
            }, "required": []}),
        ),
        tool(
            "ue5_agent_unregister",
            "Unregister an agent and release owned claims",
            json!({"type": "object", "properties": {
                "agentId": {"type": "string"}
            }, "required": ["agentId"]}),
        ),
        tool(
            "ue5_agent_claim_resource",
            "Claim a resource lease for conflict-free edits",
            json!({"type": "object", "properties": {
                "agentId": {"type": "string"},
                "resourcePath": {"type": "string"},
                "leaseSeconds": {"type": "integer", "default": 300},
                "force": {"type": "boolean", "default": false},
                "metadata": {"type": "object"}
            }, "required": ["agentId", "resourcePath"]}),
        ),
        tool(
            "ue5_agent_release_resource",
            "Release a claimed resource lease",
            json!({"type": "object", "properties": {
                "agentId": {"type": "string"},
                "resourcePath": {"type": "string"},
                "leaseId": {"type": "string"},
                "force": {"type": "boolean", "default": false}
            }, "required": []}),
        ),
        tool(
            "ue5_agent_list_claims",
            "List active resource claims",
            json!({"type": "object", "properties": {
                "agentId": {"type": "string"},
                "resourcePrefix": {"type": "string"}
            }, "required": []}),
        ),
        tool(
            "ue5_agent_create_task",
            "Create a task in the shared agent queue",
            json!({"type": "object", "properties": {
                "taskId": {"type": "string"},
                "title": {"type": "string"},
                "description": {"type": "string"},
                "priority": {"type": "integer", "default": 50},
                "assignee": {"type": "string"},
                "createdBy": {"type": "string"},
                "tags": {"type": "array", "items": {"type": "string"}},
                "payload": {"type": "object"}
            }, "required": ["title"]}),
        ),
        tool(
            "ue5_agent_assign_task",
            "Assign a task to a specific agent",
            json!({"type": "object", "properties": {
                "taskId": {"type": "string"},
                "agentId": {"type": "string"},
                "status": {"type": "string", "default": "assigned"}
            }, "required": ["taskId", "agentId"]}),
        ),
        tool(
            "ue5_agent_take_task",
            "Take the next matching queued task",
            json!({"type": "object", "properties": {
                "agentId": {"type": "string"},
                "tags": {"type": "array", "items": {"type": "string"}},
                "maxPriority": {"type": "integer", "default": 1000}
            }, "required": ["agentId"]}),
        ),
        tool(
            "ue5_agent_update_task",
            "Update task state, assignee, or result data",
            json!({"type": "object", "properties": {
                "taskId": {"type": "string"},
                "status": {"type": "string"},
                "assignee": {"type": "string"},
                "priority": {"type": "integer"},
                "error": {"type": "string"},
                "result": {"type": "object"},
                "payload": {"type": "object"},
                "tags": {"type": "array", "items": {"type": "string"}}
            }, "required": ["taskId"]}),
        ),
        tool(
            "ue5_agent_list_tasks",
            "List tasks with queue filters",
            json!({"type": "object", "properties": {
                "status": {"type": "string"},
                "assignee": {"type": "string"},
                "tag": {"type": "string"},
                "includeClosed": {"type": "boolean", "default": true},
                "limit": {"type": "integer", "default": 500}
            }, "required": []}),
        ),
        tool(
            "ue5_agent_dashboard",
            "Get aggregate orchestration status for all agents",
            json!({"type": "object", "properties": {
                "staleAfterSeconds": {"type": "integer", "default": 120}
            }, "required": []}),
        ),
        // ===== Editor UI Tools =====
        tool(
            "ue5_editor_list_windows",
            "List open editor windows",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_editor_focus_window",
            "Focus a window by title",
            json!({"type": "object", "properties": {"title": {"type": "string"}}, "required": ["title"]}),
        ),
        tool(
            "ue5_editor_list_tabs",
            "List tabs in the level editor",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_editor_open_tab",
            "Open a tab by ID",
            json!({"type": "object", "properties": {"tabId": {"type": "string"}}, "required": ["tabId"]}),
        ),
        tool(
            "ue5_editor_get_mode",
            "Get current editor mode",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_editor_set_mode",
            "Set editor mode (e.g., Landscape, Foliage)",
            json!({"type": "object", "properties": {"modeId": {"type": "string"}}, "required": ["modeId"]}),
        ),
        tool(
            "ue5_editor_get_transform_mode",
            "Get current transform gizmo mode",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_editor_set_transform_mode",
            "Set transform mode (Translate, Rotate, Scale)",
            json!({"type": "object", "properties": {"mode": {"type": "string"}}, "required": ["mode"]}),
        ),
        tool(
            "ue5_editor_get_snap_settings",
            "Get current snap settings",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_editor_set_snap_settings",
            "Configure snap settings",
            json!({"type": "object", "properties": {"gridSnapEnabled": {"type": "boolean"}, "gridSize": {"type": "number"}, "rotationSnapEnabled": {"type": "boolean"}, "rotationSnapAngle": {"type": "number"}}, "required": []}),
        ),
        tool(
            "ue5_editor_show_notification",
            "Show a notification in the editor",
            json!({"type": "object", "properties": {"message": {"type": "string"}, "type": {"type": "string"}, "duration": {"type": "number", "default": 3.0}}, "required": ["message"]}),
        ),
        tool(
            "ue5_editor_execute_command",
            "Execute an editor command",
            json!({"type": "object", "properties": {"command": {"type": "string"}}, "required": ["command"]}),
        ),
        tool(
            "ue5_editor_open_settings",
            "Open project settings to a category",
            json!({"type": "object", "properties": {"category": {"type": "string"}}, "required": []}),
        ),
        // ===== Automation/Profiling Tools =====
        tool(
            "ue5_automation_list_tests",
            "List available automation tests",
            json!({"type": "object", "properties": {"filter": {"type": "string"}}, "required": []}),
        ),
        tool(
            "ue5_automation_run_tests",
            "Run automation tests",
            json!({"type": "object", "properties": {"tests": {"type": "array", "items": {"type": "string"}}}, "required": ["tests"]}),
        ),
        tool(
            "ue5_profiling_get_stats",
            "Get performance statistics",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_profiling_get_memory",
            "Get memory usage statistics",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        tool(
            "ue5_profiling_get_frame_stats",
            "Get frame statistics",
            json!({"type": "object", "properties": {}, "required": []}),
        ),
        // ===== File Tools =====
        tool(
            "ue5_file_read",
            "Read a file from the project",
            json!({"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]}),
        ),
        tool(
            "ue5_file_write",
            "Write content to a file",
            json!({"type": "object", "properties": {"path": {"type": "string"}, "content": {"type": "string"}, "append": {"type": "boolean", "default": false}}, "required": ["path", "content"]}),
        ),
        tool(
            "ue5_file_list",
            "List files in a directory",
            json!({"type": "object", "properties": {"path": {"type": "string", "default": ""}, "pattern": {"type": "string", "default": "*"}, "recursive": {"type": "boolean", "default": false}}, "required": []}),
        ),
    ]
}

/// Map tool names to JSON-RPC methods
fn tool_to_method(tool_name: &str) -> Option<&'static str> {
    match tool_name {
        // System
        "ue5_system_info" => Some("system.getInfo"),
        "ue5_list_methods" => Some("system.listMethods"),
        "ue5_system_health" => Some("system.health"),
        // Project
        "ue5_project_info" => Some("project.getInfo"),
        "ue5_project_save" => Some("project.save"),
        // Asset
        "ue5_asset_list" => Some("asset.list"),
        "ue5_asset_get" => Some("asset.get"),
        "ue5_asset_search" => Some("asset.search"),
        "ue5_asset_duplicate" => Some("asset.duplicate"),
        "ue5_asset_delete" => Some("asset.delete"),
        "ue5_asset_import" => Some("asset.import"),
        "ue5_asset_export" => Some("asset.export"),
        // Blueprint
        "ue5_blueprint_list" => Some("blueprint.list"),
        "ue5_blueprint_get" => Some("blueprint.get"),
        "ue5_blueprint_compile" => Some("blueprint.compile"),
        "ue5_blueprint_create" => Some("blueprint.create"),
        "ue5_blueprint_get_graphs" => Some("blueprint.getGraphs"),
        "ue5_blueprint_get_variables" => Some("blueprint.getVariables"),
        "ue5_blueprint_add_variable" => Some("blueprint.addVariable"),
        // Level
        "ue5_level_get_current" => Some("level.getCurrent"),
        "ue5_level_open" => Some("level.open"),
        "ue5_level_save" => Some("level.save"),
        "ue5_level_list" => Some("level.list"),
        // Actor
        "ue5_actor_list" => Some("actor.list"),
        "ue5_actor_get" => Some("actor.get"),
        "ue5_actor_spawn" => Some("actor.spawn"),
        "ue5_actor_destroy" => Some("actor.destroy"),
        "ue5_actor_set_transform" => Some("actor.setTransform"),
        "ue5_actor_get_property" => Some("actor.getProperty"),
        "ue5_actor_set_property" => Some("actor.setProperty"),
        // Selection
        "ue5_selection_get" => Some("selection.get"),
        "ue5_selection_set" => Some("selection.set"),
        // PIE
        "ue5_pie_play" => Some("pie.play"),
        "ue5_pie_stop" => Some("pie.stop"),
        "ue5_pie_pause" => Some("pie.pause"),
        "ue5_pie_get_state" => Some("pie.getState"),
        // Console
        "ue5_console_execute" => Some("console.execute"),
        "ue5_console_get_variable" => Some("console.getVariable"),
        "ue5_console_set_variable" => Some("console.setVariable"),
        "ue5_console_list_variables" => Some("console.listVariables"),
        // Viewport
        "ue5_viewport_list" => Some("viewport.list"),
        "ue5_viewport_get_active" => Some("viewport.get"),
        "ue5_viewport_set_active" => Some("viewport.maximize"),
        "ue5_viewport_get_camera" => Some("viewport.getCamera"),
        "ue5_viewport_set_camera" => Some("viewport.setCamera"),
        "ue5_viewport_focus_actor" => Some("viewport.focusOnActor"),
        "ue5_viewport_screenshot" => Some("viewport.takeScreenshot"),
        "ue5_viewport_set_view_mode" => Some("viewport.setViewMode"),
        // Transaction
        "ue5_transaction_begin" => Some("transaction.begin"),
        "ue5_transaction_end" => Some("transaction.end"),
        "ue5_transaction_cancel" => Some("transaction.cancel"),
        "ue5_undo" => Some("transaction.undo"),
        "ue5_redo" => Some("transaction.redo"),
        "ue5_undo_history" => Some("transaction.getUndoHistory"),
        // Material
        "ue5_material_list" => Some("material.list"),
        "ue5_material_get" => Some("material.get"),
        "ue5_material_create" => Some("material.create"),
        "ue5_material_get_parameters" => Some("material.getParameters"),
        "ue5_material_set_scalar" => Some("materialInstance.setScalar"),
        "ue5_material_set_vector" => Some("materialInstance.setVector"),
        "ue5_material_set_texture" => Some("materialInstance.setTexture"),
        "ue5_material_instance_create" => Some("material.createInstance"),
        // Animation
        "ue5_animation_list" => Some("animation.list"),
        "ue5_animation_get" => Some("animation.get"),
        "ue5_animation_play" => Some("animation.play"),
        "ue5_animation_stop" => Some("animation.stop"),
        "ue5_animation_blueprint_list" => Some("animation.listBlueprints"),
        // Sequencer
        "ue5_sequencer_list" => Some("sequencer.list"),
        "ue5_sequencer_get" => Some("sequencer.get"),
        "ue5_sequencer_open" => Some("sequencer.open"),
        "ue5_sequencer_play" => Some("sequencer.play"),
        "ue5_sequencer_stop" => Some("sequencer.stop"),
        "ue5_sequencer_set_time" => Some("sequencer.setCurrentTime"),
        "ue5_sequencer_get_tracks" => Some("sequencer.getTracks"),
        "ue5_sequencer_add_actor" => Some("sequencer.addBinding"),
        // Audio
        "ue5_audio_list" => Some("audio.listSounds"),
        "ue5_audio_play" => Some("audio.play2D"),
        "ue5_audio_stop_all" => Some("audio.stopAll"),
        "ue5_audio_set_volume" => Some("audio.setMasterVolume"),
        // Physics
        "ue5_physics_simulate" => Some("physics.resume"),
        "ue5_physics_get_body_info" => Some("physics.getEnabled"),
        "ue5_physics_set_simulate" => Some("physics.setEnabled"),
        "ue5_physics_apply_impulse" => Some("physics.applyImpulse"),
        "ue5_collision_list_profiles" => Some("physics.listCollisionProfiles"),
        // Lighting
        "ue5_light_list" => Some("light.list"),
        "ue5_light_get" => Some("light.get"),
        "ue5_light_set_intensity" => Some("light.setIntensity"),
        "ue5_light_set_color" => Some("light.setColor"),
        "ue5_light_build" => Some("light.buildLighting"),
        "ue5_light_build_reflection_captures" => Some("light.recaptureSkyLight"),
        // World Partition
        "ue5_world_partition_status" => Some("worldPartition.getStatus"),
        "ue5_world_partition_list_cells" => Some("worldPartition.listCells"),
        "ue5_world_partition_load_cells" => Some("worldPartition.loadCells"),
        "ue5_data_layer_list" => Some("dataLayer.list"),
        "ue5_data_layer_set_visibility" => Some("dataLayer.setVisibility"),
        "ue5_data_layer_get_actors" => Some("dataLayer.getActors"),
        // Niagara
        "ue5_niagara_list" => Some("niagara.listSystems"),
        "ue5_niagara_spawn" => Some("niagara.spawn"),
        "ue5_niagara_activate" => Some("niagara.activate"),
        "ue5_niagara_deactivate" => Some("niagara.deactivate"),
        "ue5_niagara_set_float" => Some("niagara.setFloat"),
        "ue5_niagara_set_vector" => Some("niagara.setVector"),
        // Landscape
        "ue5_landscape_list" => Some("landscape.list"),
        "ue5_landscape_get_height" => Some("landscape.getHeightAtLocation"),
        "ue5_landscape_list_layers" => Some("landscape.listLayers"),
        "ue5_landscape_get_material" => Some("landscape.getMaterial"),
        // AI/Navigation
        "ue5_navigation_build" => Some("navigation.build"),
        "ue5_navigation_status" => Some("navigation.getStatus"),
        "ue5_navigation_find_path" => Some("navigation.findPath"),
        "ue5_ai_list_controllers" => Some("ai.listControllers"),
        "ue5_ai_move_to" => Some("ai.moveToLocation"),
        "ue5_behavior_tree_list" => Some("behaviorTree.list"),
        "ue5_behavior_tree_run" => Some("behaviorTree.run"),
        // Render
        "ue5_render_get_settings" => Some("render.getQualitySettings"),
        "ue5_render_set_quality" => Some("render.setQualitySettings"),
        "ue5_postprocess_list_volumes" => Some("postProcess.listVolumes"),
        "ue5_postprocess_get" => Some("postProcess.getVolume"),
        "ue5_postprocess_set_bloom" => Some("postProcess.setBloomIntensity"),
        "ue5_postprocess_set_exposure" => Some("postProcess.setExposure"),
        "ue5_render_get_show_flags" => Some("render.getShowFlags"),
        "ue5_render_set_show_flag" => Some("render.setShowFlag"),
        // Outliner
        "ue5_outliner_get_hierarchy" => Some("outliner.getHierarchy"),
        "ue5_outliner_set_parent" => Some("outliner.setParent"),
        "ue5_outliner_detach" => Some("outliner.detachFromParent"),
        "ue5_outliner_list_folders" => Some("outliner.listFolders"),
        "ue5_outliner_create_folder" => Some("outliner.createFolder"),
        "ue5_outliner_set_folder" => Some("outliner.setActorFolder"),
        "ue5_layer_list" => Some("layer.list"),
        "ue5_layer_create" => Some("layer.create"),
        "ue5_layer_add_actor" => Some("layer.addActor"),
        "ue5_outliner_search" => Some("outliner.search"),
        // Source Control
        "ue5_source_control_status" => Some("sourceControl.getProviderStatus"),
        "ue5_source_control_file_status" => Some("sourceControl.getFileStatus"),
        "ue5_source_control_checkout" => Some("sourceControl.checkOut"),
        "ue5_source_control_checkin" => Some("sourceControl.checkIn"),
        "ue5_source_control_revert" => Some("sourceControl.revert"),
        "ue5_source_control_sync" => Some("sourceControl.sync"),
        "ue5_source_control_history" => Some("sourceControl.getHistory"),
        // Live Coding
        "ue5_live_coding_status" => Some("liveCoding.isEnabled"),
        "ue5_live_coding_compile" => Some("liveCoding.compile"),
        "ue5_live_coding_enable" => Some("liveCoding.enable"),
        "ue5_live_coding_disable" => Some("liveCoding.disable"),
        "ue5_hot_reload" => Some("hotReload.reload"),
        "ue5_module_list" => Some("module.list"),
        // Session
        "ue5_session_list" => Some("session.list"),
        "ue5_session_current" => Some("session.getCurrent"),
        "ue5_session_join" => Some("session.join"),
        "ue5_session_leave" => Some("session.leave"),
        "ue5_session_users" => Some("session.listUsers"),
        "ue5_session_lock" => Some("session.lockObject"),
        "ue5_session_unlock" => Some("session.unlockObject"),
        // Editor
        "ue5_editor_list_windows" => Some("editor.listWindows"),
        "ue5_editor_focus_window" => Some("editor.focusWindow"),
        "ue5_editor_list_tabs" => Some("editor.listTabs"),
        "ue5_editor_open_tab" => Some("editor.openTab"),
        "ue5_editor_get_mode" => Some("editor.getCurrentMode"),
        "ue5_editor_set_mode" => Some("editor.setMode"),
        "ue5_editor_get_transform_mode" => Some("editor.getTransformMode"),
        "ue5_editor_set_transform_mode" => Some("editor.setTransformMode"),
        "ue5_editor_get_snap_settings" => Some("editor.getSnapSettings"),
        "ue5_editor_set_snap_settings" => Some("editor.setSnapSettings"),
        "ue5_editor_show_notification" => Some("editor.showNotification"),
        "ue5_editor_execute_command" => Some("editor.executeCommand"),
        "ue5_editor_open_settings" => Some("editor.openProjectSettings"),
        // Automation/Profiling
        "ue5_automation_list_tests" => Some("automation.listTests"),
        "ue5_automation_run_tests" => Some("automation.runTests"),
        "ue5_profiling_get_stats" => Some("profiling.getStats"),
        "ue5_profiling_get_memory" => Some("profiling.getMemory"),
        "ue5_profiling_get_frame_stats" => Some("profiling.getStats"),
        // File
        "ue5_file_read" => Some("file.read"),
        "ue5_file_write" => Some("file.write"),
        "ue5_file_list" => Some("file.list"),
        // Agent orchestration
        "ue5_agent_register" => Some("agent.register"),
        "ue5_agent_heartbeat" => Some("agent.heartbeat"),
        "ue5_agent_list" => Some("agent.list"),
        "ue5_agent_unregister" => Some("agent.unregister"),
        "ue5_agent_claim_resource" => Some("agent.claimResource"),
        "ue5_agent_release_resource" => Some("agent.releaseResource"),
        "ue5_agent_list_claims" => Some("agent.listClaims"),
        "ue5_agent_create_task" => Some("agent.createTask"),
        "ue5_agent_assign_task" => Some("agent.assignTask"),
        "ue5_agent_take_task" => Some("agent.takeTask"),
        "ue5_agent_update_task" => Some("agent.updateTask"),
        "ue5_agent_list_tasks" => Some("agent.listTasks"),
        "ue5_agent_dashboard" => Some("agent.getDashboard"),
        _ => None,
    }
}

fn rename_argument(arguments: &mut Value, from: &str, to: &str) {
    if let Some(obj) = arguments.as_object_mut() {
        if obj.contains_key(to) {
            obj.remove(from);
            return;
        }
        if let Some(value) = obj.remove(from) {
            obj.insert(to.to_string(), value);
        }
    }
}

fn normalize_tool_call(tool_name: &str, method: &str, arguments: Value) -> (String, Value) {
    let mut normalized_method = method.to_string();
    let mut normalized_args = if arguments.is_object() {
        arguments
    } else {
        json!({})
    };

    match tool_name {
        "ue5_audio_play" => {
            if normalized_args
                .get("actor")
                .and_then(|v| v.as_str())
                .map(|v| !v.is_empty())
                .unwrap_or(false)
            {
                normalized_method = "audio.playAttached".to_string();
            } else if normalized_args.get("location").is_some() {
                normalized_method = "audio.playAtLocation".to_string();
            } else {
                normalized_method = "audio.play2D".to_string();
            }
        }
        "ue5_physics_simulate" => {
            let enable = normalized_args
                .get("enable")
                .and_then(|v| v.as_bool())
                .unwrap_or(true);
            normalized_method = if enable {
                "physics.resume".to_string()
            } else {
                "physics.pause".to_string()
            };
            normalized_args = json!({});
        }
        "ue5_physics_set_simulate" => {
            rename_argument(&mut normalized_args, "simulate", "enabled");
        }
        "ue5_material_set_scalar" => {
            rename_argument(&mut normalized_args, "parameter", "name");
        }
        "ue5_material_set_vector" => {
            rename_argument(&mut normalized_args, "parameter", "name");
            if let Some(obj) = normalized_args.as_object_mut() {
                if !obj.contains_key("value") {
                    let r = obj.remove("r").unwrap_or(Value::from(0.0));
                    let g = obj.remove("g").unwrap_or(Value::from(0.0));
                    let b = obj.remove("b").unwrap_or(Value::from(0.0));
                    let a = obj.remove("a").unwrap_or(Value::from(1.0));
                    obj.insert(
                        "value".to_string(),
                        json!({
                            "r": r,
                            "g": g,
                            "b": b,
                            "a": a
                        }),
                    );
                }
            }
        }
        "ue5_material_set_texture" => {
            rename_argument(&mut normalized_args, "parameter", "name");
            rename_argument(&mut normalized_args, "texturePath", "value");
        }
        "ue5_session_join" => {
            rename_argument(&mut normalized_args, "session", "sessionName");
        }
        "ue5_session_lock" | "ue5_session_unlock" => {
            rename_argument(&mut normalized_args, "object", "objectPath");
        }
        "ue5_viewport_get_active"
        | "ue5_viewport_set_active"
        | "ue5_viewport_get_camera"
        | "ue5_viewport_set_camera" => {
            rename_argument(&mut normalized_args, "viewportIndex", "index");
        }
        "ue5_viewport_screenshot" => {
            rename_argument(&mut normalized_args, "viewportIndex", "index");
            rename_argument(&mut normalized_args, "filename", "path");
        }
        "ue5_sequencer_add_actor" => {
            rename_argument(&mut normalized_args, "sequence", "path");
        }
        _ => {}
    }

    (normalized_method, normalized_args)
}

fn format_tool_success(result: Value) -> Value {
    json!({
        "content": [{
            "type": "text",
            "text": serde_json::to_string_pretty(&result).unwrap_or_else(|_| "{}".to_string())
        }],
        "structuredContent": result
    })
}

fn format_tool_error(message: String) -> Value {
    json!({
        "content": [{
            "type": "text",
            "text": format!("Error: {}", message)
        }],
        "isError": true
    })
}

/// Handle MCP requests
async fn handle_request(client: &UltimateControlClient, request: McpRequest) -> McpResponse {
    if request.jsonrpc != "2.0" {
        return McpResponse {
            jsonrpc: "2.0".to_string(),
            id: request.id,
            result: None,
            error: Some(McpError {
                code: -32600,
                message: "Invalid JSON-RPC version".to_string(),
                data: None,
            }),
        };
    }

    let result = match request.method.as_str() {
        "initialize" => Ok(json!({
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "tools": {}
            },
            "serverInfo": {
                "name": "ue5-mcp",
                "version": env!("CARGO_PKG_VERSION")
            }
        })),
        "tools/list" => {
            let tools = get_tools();
            Ok(json!({ "tools": tools }))
        }
        "tools/call" => {
            let params = request.params.unwrap_or(Value::Null);
            let tool_name = params.get("name").and_then(|v| v.as_str()).unwrap_or("");
            let arguments = params.get("arguments").cloned().unwrap_or(json!({}));

            if tool_name == "ue5_rpc_call" {
                let method =
                    if let Some(method_name) = arguments.get("method").and_then(|v| v.as_str()) {
                        method_name
                    } else {
                        return McpResponse {
                            jsonrpc: "2.0".to_string(),
                            id: request.id,
                            result: None,
                            error: Some(McpError {
                                code: -32602,
                                message: "ue5_rpc_call requires string field: method".to_string(),
                                data: None,
                            }),
                        };
                    };

                let rpc_params = arguments.get("params").cloned().unwrap_or(json!({}));

                match client.call(method, Some(rpc_params)).await {
                    Ok(result) => Ok(format_tool_success(result)),
                    Err(e) => Ok(format_tool_error(e.to_string())),
                }
            } else if let Some(base_method) = tool_to_method(tool_name) {
                let (method, normalized_arguments) =
                    normalize_tool_call(tool_name, base_method, arguments);

                match client.call(&method, Some(normalized_arguments)).await {
                    Ok(result) => Ok(format_tool_success(result)),
                    Err(e) => Ok(format_tool_error(e.to_string())),
                }
            } else {
                Err(McpError {
                    code: -32601,
                    message: format!("Unknown tool: {}", tool_name),
                    data: None,
                })
            }
        }
        "notifications/initialized" | "notifications/cancelled" => {
            return McpResponse {
                jsonrpc: "2.0".to_string(),
                id: request.id,
                result: Some(Value::Null),
                error: None,
            };
        }
        _ => Err(McpError {
            code: -32601,
            message: format!("Method not found: {}", request.method),
            data: None,
        }),
    };

    match result {
        Ok(r) => McpResponse {
            jsonrpc: "2.0".to_string(),
            id: request.id,
            result: Some(r),
            error: None,
        },
        Err(e) => McpResponse {
            jsonrpc: "2.0".to_string(),
            id: request.id,
            result: None,
            error: Some(e),
        },
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_writer(io::stderr)
        .with_env_filter(
            tracing_subscriber::EnvFilter::from_default_env()
                .add_directive(tracing::Level::INFO.into()),
        )
        .init();

    tracing::info!("UE5 MCP Server v{} starting...", env!("CARGO_PKG_VERSION"));

    let config = Config::from_env();
    tracing::info!("Connecting to UltimateControl at {}", config.base_url());

    let client = UltimateControlClient::new(config);

    let stdin = io::stdin();
    let stdout = io::stdout();
    let mut stdout = stdout.lock();

    for line in stdin.lock().lines() {
        let line = match line {
            Ok(l) => l,
            Err(e) => {
                tracing::error!("Error reading stdin: {}", e);
                continue;
            }
        };

        if line.is_empty() {
            continue;
        }

        tracing::debug!("Received: {}", line);

        let request: McpRequest = match serde_json::from_str(&line) {
            Ok(r) => r,
            Err(e) => {
                tracing::error!("Failed to parse request: {}", e);
                let error_response = json!({
                    "jsonrpc": "2.0",
                    "id": null,
                    "error": {
                        "code": -32700,
                        "message": format!("Parse error: {}", e)
                    }
                });
                writeln!(stdout, "{}", error_response)?;
                stdout.flush()?;
                continue;
            }
        };

        let response = handle_request(&client, request).await;
        let response_json = serde_json::to_string(&response)?;

        tracing::debug!("Sending: {}", response_json);
        writeln!(stdout, "{}", response_json)?;
        stdout.flush()?;
    }

    Ok(())
}
