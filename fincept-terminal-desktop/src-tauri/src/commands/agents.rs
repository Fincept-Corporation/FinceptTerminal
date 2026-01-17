// agents.rs - Simplified agent commands following yfinance pattern
use crate::utils::python::get_script_path;
use crate::python_runtime;
use serde::{Deserialize, Serialize};
use tauri::Manager;

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

/// Execute CoreAgent - unified single agent system
#[tauri::command]
pub async fn execute_core_agent(
    app: tauri::AppHandle,
    query_json: String,
    config_json: String,
    api_keys_json: String,
) -> Result<String, String> {
    let script_path = get_script_path(&app, "agents/core_agent_cli.py")?;
    let args = vec![query_json, config_json, api_keys_json];
    python_runtime::execute_python_script(&script_path, args)
}

/// DEPRECATED - Old agent manager commands removed
/// Use execute_core_agent instead for all agent operations

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
        "renaissance" | "two_sigma" | "de_shaw" | "elliott" |
        "pershing_square" | "arq_capital" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
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

    let full_script_path = if cfg!(debug_assertions) {
        let current_dir = std::env::current_dir()
            .map_err(|e| format!("Failed to get current directory: {}", e))?;
        let base_dir = if current_dir.ends_with("src-tauri") {
            current_dir
        } else {
            current_dir.join("src-tauri")
        };
        base_dir.join("resources").join("scripts").join(script_path)
    } else {
        let resource_dir = app.path().resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;
        resource_dir.join("resources").join("scripts").join(script_path)
    };

    if !full_script_path.exists() {
        return Err(format!("Script not found: {}", full_script_path.display()));
    }

    let args = vec![
        "--parameters".to_string(),
        serde_json::to_string(&parameters).unwrap_or_default(),
        "--inputs".to_string(),
        serde_json::to_string(&inputs).unwrap_or_default(),
    ];

    let result_str = python_runtime::execute_python_script(&full_script_path, args)
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

    // Define agent categories and their config files
    let categories = vec![
        ("TraderInvestorsAgent", "agent_definitions.json", "trader", "📈", "#22c55e"),
        ("hedgeFundAgents", "team_config.json", "hedge-fund", "🏦", "#6366f1"),
        ("EconomicAgents", "agent_definitions.json", "economic", "💹", "#eab308"),
        ("GeopoliticsAgents", "agent_definitions.json", "geopolitics", "🌍", "#ef4444"),
    ];

    for (folder, config_file, category_id, icon, color) in categories {
        let config_path = format!("resources/scripts/agents/{}/configs/{}", folder, config_file);

        let full_path = if cfg!(debug_assertions) {
            let current_dir = std::env::current_dir()
                .map_err(|e| format!("Failed to get current directory: {}", e))?;
            let base_dir = if current_dir.ends_with("src-tauri") {
                current_dir
            } else {
                current_dir.join("src-tauri")
            };
            base_dir.join(&config_path)
        } else {
            let resource_dir = app.path().resource_dir()
                .map_err(|e| format!("Failed to get resource directory: {}", e))?;
            resource_dir.join(&config_path)
        };

        // Read and parse the config file
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
            } else {
                eprintln!("Failed to parse agent config: {}", full_path.display());
            }
        } else {
            eprintln!("Agent config file not found: {}", full_path.display());
        }
    }

    Ok(all_agents)
}

#[tauri::command]
pub async fn get_agent_metadata(agent_type: String) -> Result<AgentMetadata, String> {
    Err(format!("Legacy command - use get_agent_config instead for: {}", agent_type))
}

/// Read agent configuration file from resources
#[tauri::command]
pub async fn read_agent_config(
    app: tauri::AppHandle,
    category: String,
    config_file: String,
) -> Result<String, String> {
    let config_path = format!("resources/scripts/agents/{}/configs/{}", category, config_file);

    let full_path = if cfg!(debug_assertions) {
        let current_dir = std::env::current_dir()
            .map_err(|e| format!("Failed to get current directory: {}", e))?;
        let base_dir = if current_dir.ends_with("src-tauri") {
            current_dir
        } else {
            current_dir.join("src-tauri")
        };
        base_dir.join(&config_path)
    } else {
        let resource_dir = app.path().resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;
        resource_dir.join(&config_path)
    };

    std::fs::read_to_string(&full_path)
        .map_err(|e| format!("Failed to read config file {}: {}", full_path.display(), e))
}

#[tauri::command]
pub async fn execute_python_agent_command(
    _app: tauri::AppHandle,
    action: String,
    _parameters: serde_json::Value,
    _inputs: Option<serde_json::Value>,
) -> Result<serde_json::Value, String> {
    Err(format!("Legacy command '{}' is deprecated. Use new agent commands instead.", action))
}
