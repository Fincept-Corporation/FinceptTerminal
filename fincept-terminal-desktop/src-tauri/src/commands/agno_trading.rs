// Agno Trading Agent Commands
// Provides Rust/Tauri interface to Python-based Agno trading agents

use crate::utils::python::execute_python_command;

/// List available LLM models
#[tauri::command]
pub async fn agno_list_models(
    app: tauri::AppHandle,
    provider: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["list_models".to_string()];

    if let Some(p) = provider {
        args.push(p);
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Recommend best model for a task
#[tauri::command]
pub async fn agno_recommend_model(
    app: tauri::AppHandle,
    task: String,
    budget: Option<String>,
    reasoning: Option<bool>,
) -> Result<String, String> {
    let mut args = vec!["recommend_model".to_string(), task];

    if let Some(b) = budget {
        args.push(b);
    } else {
        args.push("medium".to_string());
    }

    if let Some(r) = reasoning {
        args.push(r.to_string());
    } else {
        args.push("false".to_string());
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Validate Agno configuration
#[tauri::command]
pub async fn agno_validate_config(
    app: tauri::AppHandle,
    config_json: String,
) -> Result<String, String> {
    let args = vec!["validate_config".to_string(), config_json];
    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Create a new trading agent
#[tauri::command]
pub async fn agno_create_agent(
    app: tauri::AppHandle,
    agent_config_json: String,
) -> Result<String, String> {
    let args = vec!["create_agent".to_string(), agent_config_json];
    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Run an agent with a prompt
#[tauri::command]
pub async fn agno_run_agent(
    app: tauri::AppHandle,
    agent_id: String,
    prompt: String,
    session_id: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["run_agent".to_string(), agent_id, prompt];

    if let Some(sid) = session_id {
        args.push(sid);
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Analyze market for a given symbol
#[tauri::command]
pub async fn agno_analyze_market(
    app: tauri::AppHandle,
    symbol: String,
    agent_model: Option<String>,
    analysis_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze_market".to_string(), symbol];

    if let Some(model) = agent_model {
        args.push(model);
    } else {
        args.push("openai:gpt-4o-mini".to_string());
    }

    if let Some(atype) = analysis_type {
        args.push(atype);
    } else {
        args.push("comprehensive".to_string());
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Generate trading signal
#[tauri::command]
pub async fn agno_generate_trade_signal(
    app: tauri::AppHandle,
    symbol: String,
    strategy: Option<String>,
    agent_model: Option<String>,
    market_data: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["generate_trade_signal".to_string(), symbol];

    if let Some(strat) = strategy {
        args.push(strat);
    } else {
        args.push("momentum".to_string());
    }

    if let Some(model) = agent_model {
        args.push(model);
    } else {
        args.push("anthropic:claude-sonnet-4".to_string());
    }

    if let Some(data) = market_data {
        args.push(data);
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Run risk management analysis
#[tauri::command]
pub async fn agno_manage_risk(
    app: tauri::AppHandle,
    portfolio_data: String,
    agent_model: Option<String>,
    risk_tolerance: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["manage_risk".to_string(), portfolio_data];

    if let Some(model) = agent_model {
        args.push(model);
    } else {
        args.push("openai:gpt-4".to_string());
    }

    if let Some(tolerance) = risk_tolerance {
        args.push(tolerance);
    } else {
        args.push("moderate".to_string());
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Get configuration template
#[tauri::command]
pub async fn agno_get_config_template(
    app: tauri::AppHandle,
    config_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["get_config_template".to_string()];

    if let Some(ctype) = config_type {
        args.push(ctype);
    } else {
        args.push("default".to_string());
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Create multi-model competition
#[tauri::command]
pub async fn agno_create_competition(
    app: tauri::AppHandle,
    name: String,
    models_json: String,
    task_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["create_competition".to_string(), name, models_json];

    if let Some(task) = task_type {
        args.push(task);
    } else {
        args.push("trading".to_string());
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Run multi-model competition
#[tauri::command]
pub async fn agno_run_competition(
    app: tauri::AppHandle,
    team_id: String,
    symbol: String,
    task: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["run_competition".to_string(), team_id, symbol];

    if let Some(t) = task {
        args.push(t);
    } else {
        args.push("analyze".to_string());
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Get model performance leaderboard
#[tauri::command]
pub async fn agno_get_leaderboard(
    app: tauri::AppHandle,
) -> Result<String, String> {
    let args = vec!["get_leaderboard".to_string()];
    execute_python_command(&app, "agno_trading_service.py", &args)
}

/// Get recent model decisions
#[tauri::command]
pub async fn agno_get_recent_decisions(
    app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["get_recent_decisions".to_string()];

    if let Some(l) = limit {
        args.push(l.to_string());
    } else {
        args.push("50".to_string());
    }

    execute_python_command(&app, "agno_trading_service.py", &args)
}
