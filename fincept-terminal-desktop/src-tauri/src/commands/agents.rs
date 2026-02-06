// agents.rs - Unified agent commands with single JSON payload
use crate::python;
use serde::{Deserialize, Serialize};
use tauri::Manager;
use std::time::Duration;
use tokio::time::timeout;

/// Default timeout for agent operations (120 seconds - LLM calls can take 30-60s+)
const AGENT_TIMEOUT_SECS: u64 = 120;
/// Shorter timeout for discovery/listing operations (15 seconds)
const DISCOVERY_TIMEOUT_SECS: u64 = 15;
/// Timeout for routing-only operations (no LLM call, just keyword matching)
const ROUTING_TIMEOUT_SECS: u64 = 10;

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

#[derive(Debug, Serialize, Deserialize)]
pub struct AgentExecutionResult {
    pub success: bool,
    pub data: Option<serde_json::Value>,
    pub error: Option<String>,
    pub execution_time_ms: u64,
    pub agent_type: String,
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
    payload: serde_json::Value,
    timeout_secs: u64,
) -> Result<String, String> {
    let payload_str = serde_json::to_string(&payload)
        .map_err(|e| format!("Failed to serialize payload: {}", e))?;

    let app_clone = app.clone();
    let args: Vec<String> = vec![payload_str];

    // Execute Python subprocess in a blocking task with timeout
    let result = timeout(
        Duration::from_secs(timeout_secs),
        tokio::task::spawn_blocking(move || {
            python::execute_sync(&app_clone, "agents/finagent_core/main.py", args)
        })
    ).await;

    match result {
        Ok(Ok(inner_result)) => inner_result,
        Ok(Err(e)) => Err(format!("Task execution error: {}", e)),
        Err(_) => Err(format!("Operation timed out after {} seconds", timeout_secs)),
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

// Legacy commands for Node Editor compatibility
fn get_agent_script_path(agent_type: &str) -> Option<&'static str> {
    match agent_type {
        "warren_buffett" => Some("agents/TraderInvestorsAgent/warren_buffett_agent_cli.py"),
        "charlie_munger" => Some("agents/TraderInvestorsAgent/charlie_munger_agent_cli.py"),
        "benjamin_graham" => Some("agents/TraderInvestorsAgent/benjamin_graham_agent_cli.py"),
        "seth_klarman" => Some("agents/TraderInvestorsAgent/seth_klarman_agent_cli.py"),
        "howard_marks" => Some("agents/TraderInvestorsAgent/howard_marks_agent_cli.py"),
        "joel_greenblatt" => Some("agents/TraderInvestorsAgent/joel_greenblatt_agent_cli.py"),
        "david_einhorn" => Some("agents/TraderInvestorsAgent/david_einhorn_agent_cli.py"),
        "bill_miller" => Some("agents/TraderInvestorsAgent/bill_miller_agent_cli.py"),
        "jean_marie_eveillard" => Some("agents/TraderInvestorsAgent/jean_marie_eveillard_agent_cli.py"),
        "marty_whitman" => Some("agents/TraderInvestorsAgent/marty_whitman_agent_cli.py"),
        "macro_cycle" | "central_bank" | "behavioral" | "institutional_flow" |
        "innovation" | "geopolitical" | "currency" | "supply_chain" |
        "sentiment" | "regulatory" | "bridgewater" | "citadel" |
        "two_sigma" | "de_shaw" | "elliott" |
        "pershing_square" | "arq_capital" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "renaissance" | "renaissance_technologies" | "rentech" => Some("agents/hedgeFundAgents/renaissance_technologies_hedge_fund_agent/rentech_cli.py"),
        "capitalism" => Some("agents/EconomicAgents/capitalism_agent_cli.py"),
        "keynesian" => Some("agents/EconomicAgents/keynesian_agent_cli.py"),
        "neoliberal" => Some("agents/EconomicAgents/neoliberal_agent_cli.py"),
        "socialism" => Some("agents/EconomicAgents/socialism_agent_cli.py"),
        "mixed_economy" => Some("agents/EconomicAgents/mixed_economy_agent_cli.py"),
        "mercantilist" => Some("agents/EconomicAgents/mercantilist_agent_cli.py"),
        _ => None,
    }
}

#[tauri::command]
pub async fn execute_python_agent(
    app: tauri::AppHandle,
    agent_type: String,
    parameters: serde_json::Value,
    inputs: serde_json::Value,
) -> Result<AgentExecutionResult, String> {
    let start_time = std::time::Instant::now();
    let script_path = get_agent_script_path(&agent_type)
        .ok_or_else(|| format!("Unknown agent type: {}", agent_type))?;

    let args = vec![
        "--parameters".to_string(),
        serde_json::to_string(&parameters).unwrap_or_default(),
        "--inputs".to_string(),
        serde_json::to_string(&inputs).unwrap_or_default(),
    ];

    let result_str = python::execute_sync(&app, script_path, args)
        .map_err(|e| {
            let execution_time = start_time.elapsed().as_millis() as u64;
            format!("Python execution failed after {}ms: {}", execution_time, e)
        })?;

    let execution_time = start_time.elapsed().as_millis() as u64;

    match serde_json::from_str::<serde_json::Value>(&result_str) {
        Ok(data) => Ok(AgentExecutionResult {
            success: true,
            data: Some(data),
            error: None,
            execution_time_ms: execution_time,
            agent_type,
        }),
        Err(e) => Ok(AgentExecutionResult {
            success: true,
            data: Some(serde_json::json!({ "raw_output": result_str })),
            error: Some(format!("JSON parse warning: {}", e)),
            execution_time_ms: execution_time,
            agent_type,
        }),
    }
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

    Ok(all_agents)
}

#[tauri::command]
pub async fn get_agent_metadata(agent_type: String) -> Result<AgentMetadata, String> {
    Err(format!("Legacy command - use execute_core_agent with action='system_info' instead for: {}", agent_type))
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
