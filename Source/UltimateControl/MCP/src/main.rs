//! UE5 MCP Server - Model Context Protocol server for UltimateControl plugin
//!
//! This is a standalone executable that implements the MCP protocol over stdio,
//! bridging AI agents to the UltimateControl plugin's HTTP JSON-RPC API.

use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::collections::HashMap;
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

/// Get all available tools
fn get_tools() -> Vec<Tool> {
    vec![
        // System Tools
        Tool {
            name: "ue5_system_info".to_string(),
            description: "Get information about the UE5 editor and UltimateControl server".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        Tool {
            name: "ue5_list_methods".to_string(),
            description: "List all available JSON-RPC methods".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        // Project Tools
        Tool {
            name: "ue5_project_info".to_string(),
            description: "Get information about the current Unreal Engine project".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        Tool {
            name: "ue5_project_save".to_string(),
            description: "Save all dirty packages in the project".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "prompt": {"type": "boolean", "description": "Prompt user before saving", "default": false}
                },
                "required": []
            }),
        },
        // Asset Tools
        Tool {
            name: "ue5_asset_list".to_string(),
            description: "List assets in a content directory".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string", "description": "Content path (e.g., /Game)", "default": "/Game"},
                    "class": {"type": "string", "description": "Filter by class"},
                    "recursive": {"type": "boolean", "default": true},
                    "limit": {"type": "integer", "default": 100}
                },
                "required": []
            }),
        },
        Tool {
            name: "ue5_asset_get".to_string(),
            description: "Get detailed information about a specific asset".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string", "description": "Asset path"}
                },
                "required": ["path"]
            }),
        },
        Tool {
            name: "ue5_asset_search".to_string(),
            description: "Search for assets by name pattern".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "query": {"type": "string", "description": "Search query"},
                    "class": {"type": "string", "description": "Filter by class"},
                    "limit": {"type": "integer", "default": 50}
                },
                "required": ["query"]
            }),
        },
        Tool {
            name: "ue5_asset_duplicate".to_string(),
            description: "Duplicate an asset".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "source": {"type": "string"},
                    "destination": {"type": "string"}
                },
                "required": ["source", "destination"]
            }),
        },
        Tool {
            name: "ue5_asset_delete".to_string(),
            description: "Delete an asset (dangerous)".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string"}
                },
                "required": ["path"]
            }),
        },
        // Blueprint Tools
        Tool {
            name: "ue5_blueprint_list".to_string(),
            description: "List all blueprints in a path".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string", "default": "/Game"},
                    "limit": {"type": "integer", "default": 100}
                },
                "required": []
            }),
        },
        Tool {
            name: "ue5_blueprint_get".to_string(),
            description: "Get detailed blueprint information".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string"}
                },
                "required": ["path"]
            }),
        },
        Tool {
            name: "ue5_blueprint_compile".to_string(),
            description: "Compile a blueprint".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string"}
                },
                "required": ["path"]
            }),
        },
        Tool {
            name: "ue5_blueprint_create".to_string(),
            description: "Create a new blueprint class".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string"},
                    "parentClass": {"type": "string", "default": "Actor"}
                },
                "required": ["path"]
            }),
        },
        // Level/Actor Tools
        Tool {
            name: "ue5_level_get_current".to_string(),
            description: "Get information about the currently loaded level".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        Tool {
            name: "ue5_level_open".to_string(),
            description: "Open a level by path".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string"},
                    "promptSave": {"type": "boolean", "default": true}
                },
                "required": ["path"]
            }),
        },
        Tool {
            name: "ue5_level_save".to_string(),
            description: "Save the current level".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        Tool {
            name: "ue5_actor_list".to_string(),
            description: "List actors in the current level".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "class": {"type": "string"},
                    "tag": {"type": "string"},
                    "limit": {"type": "integer", "default": 100}
                },
                "required": []
            }),
        },
        Tool {
            name: "ue5_actor_get".to_string(),
            description: "Get detailed information about an actor".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "actor": {"type": "string"}
                },
                "required": ["actor"]
            }),
        },
        Tool {
            name: "ue5_actor_spawn".to_string(),
            description: "Spawn a new actor in the level".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "class": {"type": "string"},
                    "name": {"type": "string"},
                    "location": {"type": "object"},
                    "rotation": {"type": "object"}
                },
                "required": ["class"]
            }),
        },
        Tool {
            name: "ue5_actor_destroy".to_string(),
            description: "Destroy an actor".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "actor": {"type": "string"}
                },
                "required": ["actor"]
            }),
        },
        Tool {
            name: "ue5_actor_set_transform".to_string(),
            description: "Set an actor's transform".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "actor": {"type": "string"},
                    "location": {"type": "object"},
                    "rotation": {"type": "object"},
                    "scale": {"type": "object"}
                },
                "required": ["actor"]
            }),
        },
        Tool {
            name: "ue5_selection_get".to_string(),
            description: "Get currently selected actors".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        Tool {
            name: "ue5_selection_set".to_string(),
            description: "Set the selected actors".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "actors": {"type": "array", "items": {"type": "string"}},
                    "add": {"type": "boolean", "default": false}
                },
                "required": ["actors"]
            }),
        },
        // PIE Tools
        Tool {
            name: "ue5_pie_play".to_string(),
            description: "Start Play In Editor session".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "mode": {"type": "string", "default": "SelectedViewport"}
                },
                "required": []
            }),
        },
        Tool {
            name: "ue5_pie_stop".to_string(),
            description: "Stop PIE session".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        Tool {
            name: "ue5_pie_pause".to_string(),
            description: "Pause/resume PIE".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "pause": {"type": "boolean"}
                },
                "required": []
            }),
        },
        Tool {
            name: "ue5_pie_get_state".to_string(),
            description: "Get PIE state".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        // Console Tools
        Tool {
            name: "ue5_console_execute".to_string(),
            description: "Execute a console command".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "command": {"type": "string"}
                },
                "required": ["command"]
            }),
        },
        Tool {
            name: "ue5_console_get_variable".to_string(),
            description: "Get a console variable value".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "name": {"type": "string"}
                },
                "required": ["name"]
            }),
        },
        Tool {
            name: "ue5_console_set_variable".to_string(),
            description: "Set a console variable".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "name": {"type": "string"},
                    "value": {"type": "string"}
                },
                "required": ["name", "value"]
            }),
        },
        // Profiling Tools
        Tool {
            name: "ue5_profiling_get_stats".to_string(),
            description: "Get performance statistics".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        Tool {
            name: "ue5_profiling_get_memory".to_string(),
            description: "Get memory usage statistics".to_string(),
            input_schema: json!({"type": "object", "properties": {}, "required": []}),
        },
        // File Tools
        Tool {
            name: "ue5_file_read".to_string(),
            description: "Read a file from the project".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string"}
                },
                "required": ["path"]
            }),
        },
        Tool {
            name: "ue5_file_write".to_string(),
            description: "Write content to a file".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string"},
                    "content": {"type": "string"},
                    "append": {"type": "boolean", "default": false}
                },
                "required": ["path", "content"]
            }),
        },
        Tool {
            name: "ue5_file_list".to_string(),
            description: "List files in a directory".to_string(),
            input_schema: json!({
                "type": "object",
                "properties": {
                    "path": {"type": "string", "default": ""},
                    "pattern": {"type": "string", "default": "*"},
                    "recursive": {"type": "boolean", "default": false}
                },
                "required": []
            }),
        },
    ]
}

/// Map tool names to JSON-RPC methods
fn tool_to_method(tool_name: &str) -> Option<&'static str> {
    match tool_name {
        // System
        "ue5_system_info" => Some("system.getInfo"),
        "ue5_list_methods" => Some("system.listMethods"),
        // Project
        "ue5_project_info" => Some("project.getInfo"),
        "ue5_project_save" => Some("project.save"),
        // Asset
        "ue5_asset_list" => Some("asset.list"),
        "ue5_asset_get" => Some("asset.get"),
        "ue5_asset_search" => Some("asset.search"),
        "ue5_asset_duplicate" => Some("asset.duplicate"),
        "ue5_asset_delete" => Some("asset.delete"),
        // Blueprint
        "ue5_blueprint_list" => Some("blueprint.list"),
        "ue5_blueprint_get" => Some("blueprint.get"),
        "ue5_blueprint_compile" => Some("blueprint.compile"),
        "ue5_blueprint_create" => Some("blueprint.create"),
        // Level
        "ue5_level_get_current" => Some("level.getCurrent"),
        "ue5_level_open" => Some("level.open"),
        "ue5_level_save" => Some("level.save"),
        // Actor
        "ue5_actor_list" => Some("actor.list"),
        "ue5_actor_get" => Some("actor.get"),
        "ue5_actor_spawn" => Some("actor.spawn"),
        "ue5_actor_destroy" => Some("actor.destroy"),
        "ue5_actor_set_transform" => Some("actor.setTransform"),
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
        // Profiling
        "ue5_profiling_get_stats" => Some("profiling.getStats"),
        "ue5_profiling_get_memory" => Some("profiling.getMemory"),
        // File
        "ue5_file_read" => Some("file.read"),
        "ue5_file_write" => Some("file.write"),
        "ue5_file_list" => Some("file.list"),
        _ => None,
    }
}

/// Handle MCP requests
async fn handle_request(client: &UltimateControlClient, request: McpRequest) -> McpResponse {
    let result = match request.method.as_str() {
        "initialize" => {
            Ok(json!({
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {}
                },
                "serverInfo": {
                    "name": "ue5-mcp",
                    "version": env!("CARGO_PKG_VERSION")
                }
            }))
        }
        "tools/list" => {
            let tools = get_tools();
            Ok(json!({ "tools": tools }))
        }
        "tools/call" => {
            let params = request.params.unwrap_or(Value::Null);
            let tool_name = params
                .get("name")
                .and_then(|v| v.as_str())
                .unwrap_or("");
            let arguments = params
                .get("arguments")
                .cloned()
                .unwrap_or(json!({}));

            if let Some(method) = tool_to_method(tool_name) {
                match client.call(method, Some(arguments)).await {
                    Ok(result) => Ok(json!({
                        "content": [{
                            "type": "text",
                            "text": serde_json::to_string_pretty(&result).unwrap_or_else(|_| "{}".to_string())
                        }]
                    })),
                    Err(e) => Ok(json!({
                        "content": [{
                            "type": "text",
                            "text": format!("Error: {}", e)
                        }],
                        "isError": true
                    })),
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
            // Notifications don't need responses, but we return success for robustness
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
    // Initialize logging
    tracing_subscriber::fmt()
        .with_writer(io::stderr)
        .with_env_filter(
            tracing_subscriber::EnvFilter::from_default_env()
                .add_directive(tracing::Level::INFO.into()),
        )
        .init();

    tracing::info!("UE5 MCP Server starting...");

    let config = Config::from_env();
    tracing::info!("Connecting to UltimateControl at {}", config.base_url());

    let client = UltimateControlClient::new(config);

    // Read from stdin, write to stdout
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
