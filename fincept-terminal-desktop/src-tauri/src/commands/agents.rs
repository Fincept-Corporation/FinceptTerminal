// agents.rs - Unified agent commands with single JSON payload
use crate::python;
use serde::{Deserialize, Serialize};
use tauri::Manager;
use std::time::Duration;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::collections::HashMap;
use tokio::time::timeout;
use tokio::sync::Mutex;
use once_cell::sync::Lazy;

// Global registry for active streams (for cancellation)
static ACTIVE_STREAMS: Lazy<Mutex<HashMap<String, Arc<AtomicBool>>>> = Lazy::new(|| Mutex::new(HashMap::new()));

/// Default timeout for agent operations (300 seconds - Fincept endpoint can take up to 70s+)
const AGENT_TIMEOUT_SECS: u64 = 300;
/// Timeout for discovery/listing operations (60 seconds)
const DISCOVERY_TIMEOUT_SECS: u64 = 60;
/// Timeout for routing operations (includes Python startup + rate limiter + LLM call)
const ROUTING_TIMEOUT_SECS: u64 = 300;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AgentParameter {
    pub name: String,
    pub label: String,
    #[serde(rename = "type")]
    pub param_type: String,
    pub required: bool,
    pub default_value: Option<serde_json::Value>,
    pub description: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct AgentMetadata {
    pub id: String,
    pub name: String,
    #[serde(rename = "type")]
    pub agent_type: String,
    pub category: String,
    pub description: String,
    pub script_path: String,
    pub parameters: Vec<AgentParameter>,
    pub required_inputs: Vec<String>,
    pub outputs: Vec<String>,
    pub icon: String,
    pub color: String,
}

/// Execute CoreAgent with single JSON payload (unified entry point)
///
/// Payload format:
/// {
///     "action": "run|run_team|run_workflow|list_tools|system_info|...",
///     "api_keys": {...},
///     "user_id": "optional",
///     "config": {...},
///     "params": {...}
/// }
///
/// Supported actions:
/// - Core: run, run_team, run_workflow, run_structured
/// - Agent loader: discover_agents, list_agents, create_agent
/// - SuperAgent: route_query, execute_query, execute_multi_query
/// - Planner: create_stock_plan, create_portfolio_plan, execute_plan
/// - Paper trading: paper_execute_trade, paper_get_portfolio, paper_get_positions
/// - Repository: save_session, get_session, add_message, save_memory, search_memories
/// - System: system_info, list_tools, list_models, list_output_models
#[tauri::command]
pub async fn execute_core_agent(
    app: tauri::AppHandle,
    payload: serde_json::Value,
) -> Result<String, String> {
    execute_core_agent_with_timeout(app, payload, AGENT_TIMEOUT_SECS).await
}

/// Execute CoreAgent with custom timeout using direct subprocess execution
/// This bypasses the worker pool for reliability on Windows
async fn execute_core_agent_with_timeout(
    app: tauri::AppHandle,
    mut payload: serde_json::Value,
    timeout_secs: u64,
) -> Result<String, String> {
    // Inject terminal MCP bridge endpoint so Python can call TypeScript tools
    let bridge_port = crate::mcp_bridge::get_or_start_bridge(app.clone());
    if let Some(config) = payload.get_mut("config").and_then(|c| c.as_object_mut()) {
        config.insert(
            "terminal_mcp_endpoint".to_string(),
            serde_json::Value::String(format!("http://127.0.0.1:{}", bridge_port)),
        );
    } else if payload.is_object() {
        // No config key yet — create it
        let map = payload.as_object_mut().unwrap();
        let mut config = serde_json::Map::new();
        config.insert(
            "terminal_mcp_endpoint".to_string(),
            serde_json::Value::String(format!("http://127.0.0.1:{}", bridge_port)),
        );
        map.insert("config".to_string(), serde_json::Value::Object(config));
    }

    let payload_str = serde_json::to_string(&payload)
        .map_err(|e| format!("Failed to serialize payload: {}", e))?;

    // DEBUG: Log the action and model config being sent to Python
    let action = payload.get("action").and_then(|v| v.as_str()).unwrap_or("unknown");
    let model_info = payload.get("config")
        .and_then(|c| c.get("model"))
        .map(|m| m.to_string())
        .unwrap_or_else(|| "no model config".to_string());

    // Log API keys presence (not values for security)
    let api_keys_info = payload.get("api_keys")
        .and_then(|k| k.as_object())
        .map(|obj| {
            let keys: Vec<String> = obj.keys().map(|k| k.clone()).collect();
            format!("keys={:?}", keys)
        })
        .unwrap_or_else(|| "no api_keys".to_string());

    eprintln!("[AgentCmd] action={}, model_config={}, payload_len={}, {}",
              action, model_info, payload_str.len(), api_keys_info);

    // Log first 1000 chars of payload for debugging
    let payload_preview: String = payload_str.chars().take(1000).collect();
    eprintln!("[AgentCmd] Payload preview: {}", payload_preview);

    let app_clone = app.clone();

    eprintln!("[AgentCmd] Starting Python execution with timeout={}s...", timeout_secs);
    let start_time = std::time::Instant::now();

    // Pass payload via stdin to avoid Windows command-line length limit (os error 206)
    let result = timeout(
        Duration::from_secs(timeout_secs),
        tokio::task::spawn_blocking(move || {
            eprintln!("[AgentCmd] Inside blocking task, calling python::execute_with_stdin...");
            let exec_result = python::execute_with_stdin(&app_clone, "agents/finagent_core/main.py", vec![], &payload_str);
            eprintln!("[AgentCmd] python::execute_with_stdin returned");
            exec_result
        })
    ).await;

    let elapsed = start_time.elapsed();
    eprintln!("[AgentCmd] Python execution completed in {:.2}s", elapsed.as_secs_f64());

    match &result {
        Ok(Ok(Ok(ref output))) => {
            // Log first 500 chars of successful output
            let preview: String = output.chars().take(500).collect();
            eprintln!("[AgentCmd] SUCCESS: {}", preview);
        }
        Ok(Ok(Err(ref e))) => {
            eprintln!("[AgentCmd] PYTHON ERROR: {}", e);
            eprintln!("[AgentCmd] This likely means the Python script failed to execute or returned an error.");
        }
        Ok(Err(ref e)) => {
            eprintln!("[AgentCmd] TASK ERROR (tokio spawn_blocking failed): {}", e);
        }
        Err(_) => {
            eprintln!("[AgentCmd] TIMEOUT after {}s - Python script did not complete in time", timeout_secs);
            eprintln!("[AgentCmd] This could mean:");
            eprintln!("[AgentCmd]   1. Python script is hanging/waiting for response");
            eprintln!("[AgentCmd]   2. API call to LLM provider is taking too long");
            eprintln!("[AgentCmd]   3. Network issues preventing API access");
            eprintln!("[AgentCmd]   4. API key is invalid or missing");
        }
    }

    match result {
        Ok(Ok(inner_result)) => inner_result,
        Ok(Err(e)) => Err(format!("Task execution error: {}", e)),
        Err(_) => Err(format!("Operation timed out after {} seconds. Check if API key is configured and LLM endpoint is accessible.", timeout_secs)),
    }
}

/// Deliver a tool execution result from TypeScript back to the waiting Python agent.
/// TypeScript calls this after executing a terminal MCP tool triggered by the Python agent.
///
/// id: The request ID from the mcp-tool-call event
/// result: JSON string of { success, data, error }
#[tauri::command]
pub fn register_mcp_tool_result(id: String, result: String) -> bool {
    crate::mcp_bridge::deliver_tool_result(&id, result)
}

/// Cancel an active streaming operation
#[tauri::command]
pub async fn cancel_agent_stream(stream_id: String) -> Result<bool, String> {
    let streams = ACTIVE_STREAMS.lock().await;
    if let Some(cancel_flag) = streams.get(&stream_id) {
        cancel_flag.store(true, Ordering::Relaxed);
        Ok(true)
    } else {
        Ok(false) // Stream not found or already completed
    }
}

/// Route a query to the appropriate agent using SuperAgent
/// Uses shorter timeout since routing is just keyword matching (no LLM call)
#[tauri::command]
pub async fn route_query(
    app: tauri::AppHandle,
    query: String,
    api_keys: Option<std::collections::HashMap<String, String>>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "action": "route_query",
        "api_keys": api_keys.unwrap_or_default(),
        "params": { "query": query }
    });
    execute_core_agent_with_timeout(app, payload, ROUTING_TIMEOUT_SECS).await
}

/// Execute a query with automatic routing via SuperAgent
#[tauri::command]
pub async fn execute_routed_query(
    app: tauri::AppHandle,
    query: String,
    api_keys: Option<std::collections::HashMap<String, String>>,
    session_id: Option<String>,
    config: Option<serde_json::Value>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "action": "execute_query",
        "api_keys": api_keys.unwrap_or_default(),
        "config": config.unwrap_or(serde_json::json!({})),
        "params": {
            "query": query,
            "session_id": session_id
        }
    });
    execute_core_agent(app, payload).await
}

/// Discover all available agents from config files and directories
/// Uses shorter timeout since discovery should be fast
#[tauri::command]
pub async fn discover_agents(app: tauri::AppHandle) -> Result<String, String> {
    let payload = serde_json::json!({
        "action": "discover_agents",
        "params": {}
    });
    // Use shorter timeout for discovery - it should be fast
    execute_core_agent_with_timeout(app, payload, DISCOVERY_TIMEOUT_SECS).await
}

/// Create a stock analysis execution plan
#[tauri::command]
pub async fn create_stock_analysis_plan(
    app: tauri::AppHandle,
    symbol: String,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "action": "create_stock_plan",
        "params": { "symbol": symbol }
    });
    execute_core_agent(app, payload).await
}

/// Generate a dynamic execution plan using LLM based on a natural-language query
#[tauri::command]
pub async fn generate_dynamic_plan(
    app: tauri::AppHandle,
    query: String,
    api_keys: Option<std::collections::HashMap<String, String>>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "action": "generate_dynamic_plan",
        "api_keys": api_keys.unwrap_or_default(),
        "params": { "query": query }
    });
    execute_core_agent(app, payload).await
}

/// Execute an execution plan
#[tauri::command]
pub async fn execute_plan(
    app: tauri::AppHandle,
    plan: serde_json::Value,
    api_keys: Option<std::collections::HashMap<String, String>>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "action": "execute_plan",
        "api_keys": api_keys.unwrap_or_default(),
        "params": { "plan": plan }
    });
    execute_core_agent(app, payload).await
}

/// Save a trade decision to repository
#[tauri::command]
pub async fn save_trade_decision(
    app: tauri::AppHandle,
    competition_id: String,
    model_name: String,
    cycle_number: i32,
    symbol: String,
    action: String,
    quantity: f64,
    price: Option<f64>,
    reasoning: String,
    confidence: f64,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "action": "save_trade_decision",
        "params": {
            "competition_id": competition_id,
            "model_name": model_name,
            "cycle_number": cycle_number,
            "symbol": symbol,
            "action": action,
            "quantity": quantity,
            "price": price,
            "reasoning": reasoning,
            "confidence": confidence
        }
    });
    execute_core_agent(app, payload).await
}

/// Get trade decisions from repository
#[tauri::command]
pub async fn get_trade_decisions(
    app: tauri::AppHandle,
    competition_id: Option<String>,
    model_name: Option<String>,
    cycle_number: Option<i32>,
    limit: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "action": "get_decisions",
        "params": {
            "competition_id": competition_id,
            "model_name": model_name,
            "cycle_number": cycle_number,
            "limit": limit.unwrap_or(100)
        }
    });
    execute_core_agent(app, payload).await
}

/// Struct for parsing agent definitions from JSON config files
#[derive(Debug, Clone, Serialize, Deserialize)]
struct AgentDefinition {
    id: String,
    name: String,
    role: Option<String>,
    goal: Option<String>,
    description: String,
    #[serde(default)]
    llm_config: Option<serde_json::Value>,
    #[serde(default)]
    tools: Vec<String>,
    #[serde(default)]
    instructions: Option<String>,
    #[serde(default)]
    enable_memory: bool,
    #[serde(default)]
    enable_agentic_memory: bool,
}

#[derive(Debug, Deserialize)]
struct AgentConfigFile {
    agents: Vec<AgentDefinition>,
}

/// List all available agents from config files
#[tauri::command]
pub async fn list_available_agents(app: tauri::AppHandle) -> Result<Vec<AgentMetadata>, String> {
    let mut all_agents = Vec::new();

    let categories = vec![
        ("TraderInvestorsAgent", "agent_definitions.json", "trader", "📈", "#22c55e"),
        ("hedgeFundAgents", "team_config.json", "hedge-fund", "🏦", "#6366f1"),
        ("EconomicAgents", "agent_definitions.json", "economic", "💹", "#eab308"),
        ("GeopoliticsAgents", "agent_definitions.json", "geopolitics", "🌍", "#ef4444"),
    ];

    for (folder, config_file, category_id, icon, color) in categories {
        let relative_path = format!("scripts/agents/{}/configs/{}", folder, config_file);

        let full_path = if cfg!(debug_assertions) {
            let current_dir = std::env::current_dir()
                .map_err(|e| format!("Failed to get current directory: {}", e))?;
            let base_dir = if current_dir.ends_with("src-tauri") {
                current_dir
            } else {
                current_dir.join("src-tauri")
            };
            base_dir.join("resources").join(&relative_path)
        } else {
            let resource_dir = app.path().resource_dir()
                .map_err(|e| format!("Failed to get resource directory: {}", e))?;
            resource_dir.join(&relative_path)
        };

        if let Ok(content) = std::fs::read_to_string(&full_path) {
            if let Ok(config) = serde_json::from_str::<AgentConfigFile>(&content) {
                for agent_def in config.agents {
                    all_agents.push(AgentMetadata {
                        id: agent_def.id.clone(),
                        name: agent_def.name,
                        agent_type: "python-agent".to_string(),
                        category: category_id.to_string(),
                        description: agent_def.description,
                        script_path: format!("agents/{}/", folder),
                        parameters: vec![
                            AgentParameter {
                                name: "query".to_string(),
                                label: "Query".to_string(),
                                param_type: "string".to_string(),
                                required: true,
                                default_value: None,
                                description: Some("The analysis query or question".to_string()),
                            },
                        ],
                        required_inputs: vec![],
                        outputs: vec!["analysis".to_string()],
                        icon: icon.to_string(),
                        color: color.to_string(),
                    });
                }
            }
        }
    }

    // Load Renaissance Technologies agent (single agent config format)
    let rentech_path = if cfg!(debug_assertions) {
        let current_dir = std::env::current_dir()
            .map_err(|e| format!("Failed to get current directory: {}", e))?;
        let base_dir = if current_dir.ends_with("src-tauri") {
            current_dir
        } else {
            current_dir.join("src-tauri")
        };
        base_dir.join("resources/scripts/agents/finagent_core/configs/renaissance_technologies_agent.json")
    } else {
        let resource_dir = app.path().resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;
        resource_dir.join("scripts/agents/finagent_core/configs/renaissance_technologies_agent.json")
    };

    if let Ok(content) = std::fs::read_to_string(&rentech_path) {
        if let Ok(agent_def) = serde_json::from_str::<AgentDefinition>(&content) {
            all_agents.push(AgentMetadata {
                id: agent_def.id.clone(),
                name: agent_def.name,
                agent_type: "python-agent".to_string(),
                category: "hedge-fund".to_string(),
                description: agent_def.description,
                script_path: "agents/hedgeFundAgents/renaissance_technologies_hedge_fund_agent/".to_string(),
                parameters: vec![
                    AgentParameter {
                        name: "query".to_string(),
                        label: "Query".to_string(),
                        param_type: "string".to_string(),
                        required: true,
                        default_value: None,
                        description: Some("The analysis query or question".to_string()),
                    },
                ],
                required_inputs: vec![],
                outputs: vec!["analysis".to_string()],
                icon: "🏆".to_string(),
                color: "#f59e0b".to_string(),
            });
        }
    }

    Ok(all_agents)
}

#[tauri::command]
pub async fn get_agent_metadata(agent_type: String) -> Result<AgentMetadata, String> {
    Err(format!("Legacy command - use execute_core_agent with action='system_info' instead for: {}", agent_type))
}

/// Execute Renaissance Technologies CLI directly
#[tauri::command]
pub async fn execute_renaissance_cli(
    app: tauri::AppHandle,
    command: String,
    data: String,
) -> Result<String, String> {
    use std::process::Command;
    use std::time::Duration;
    use tokio::time::timeout;
    use crate::python;

    println!("[RenTech Rust] Starting execution - command: {}, data length: {}", command, data.len());

    let relative_path = "scripts/agents/hedgeFundAgents/renaissance_technologies_hedge_fund_agent/rentech_cli.py";

    let script_path = if cfg!(debug_assertions) {
        let current_dir = std::env::current_dir()
            .map_err(|e| format!("Failed to get current directory: {}", e))?;
        let base_dir = if current_dir.ends_with("src-tauri") {
            current_dir
        } else {
            current_dir.join("src-tauri")
        };
        base_dir.join("resources").join(relative_path)
    } else {
        let resource_dir = app.path().resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;
        resource_dir.join(relative_path)
    };

    println!("[RenTech Rust] Script path: {:?}", script_path);

    let python_path = python::get_python_path(&app, None)?;
    println!("[RenTech Rust] Python path: {:?}", python_path);

    // Get the parent directory to add to PYTHONPATH
    let agents_dir = script_path.parent()
        .and_then(|p| p.parent())
        .ok_or("Failed to get agents directory".to_string())?;

    println!("[RenTech Rust] Adding to PYTHONPATH: {:?}", agents_dir);

    // Run as module: python -m renaissance_technologies_hedge_fund_agent.rentech_cli
    let child = Command::new(&python_path)
        .env("PYTHONPATH", agents_dir)
        .arg("-m")
        .arg("renaissance_technologies_hedge_fund_agent.rentech_cli")
        .arg(&command)
        .arg(&data)
        .current_dir(agents_dir)
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to spawn Renaissance CLI process: {}", e))?;

    println!("[RenTech Rust] Process spawned, waiting for output...");

    let output_result = timeout(Duration::from_secs(120), async move {
        tokio::task::spawn_blocking(move || child.wait_with_output()).await
    })
    .await
    .map_err(|_| "Renaissance CLI execution timed out after 120 seconds".to_string())?;

    let output = output_result
        .map_err(|e| format!("Failed to wait for process: {}", e))?
        .map_err(|e| format!("Process execution error: {}", e))?;

    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    let stderr = String::from_utf8_lossy(&output.stderr).to_string();

    println!("[RenTech Rust] Exit status: {:?}", output.status);
    println!("[RenTech Rust] Stdout: {}", stdout);
    println!("[RenTech Rust] Stderr: {}", stderr);

    if output.status.success() {
        Ok(stdout)
    } else {
        Err(format!("Renaissance CLI failed with exit code {:?}\nStderr: {}", output.status.code(), stderr))
    }
}

/// Read agent configuration file from resources
#[tauri::command]
pub async fn read_agent_config(
    app: tauri::AppHandle,
    category: String,
    config_file: String,
) -> Result<String, String> {
    let relative_path = format!("scripts/agents/{}/configs/{}", category, config_file);

    let full_path = if cfg!(debug_assertions) {
        let current_dir = std::env::current_dir()
            .map_err(|e| format!("Failed to get current directory: {}", e))?;
        let base_dir = if current_dir.ends_with("src-tauri") {
            current_dir
        } else {
            current_dir.join("src-tauri")
        };
        base_dir.join("resources").join(&relative_path)
    } else {
        let resource_dir = app.path().resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;
        resource_dir.join(&relative_path)
    };

    std::fs::read_to_string(&full_path)
        .map_err(|e| format!("Failed to read config file {}: {}", full_path.display(), e))
}
