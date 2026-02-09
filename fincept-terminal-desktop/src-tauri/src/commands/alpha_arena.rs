#![allow(dead_code)]
/**
 * Alpha Arena Tauri Commands
 * Rust commands for AI trading competition (refactored v2)
 * Now with streaming support for real-time progress updates
 * Refactored to use unified python module API
 */

use tauri::{command, AppHandle, Emitter};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::collections::HashMap;
use crate::python;

/// Extract JSON object from output that may contain log lines before it
fn extract_json_from_output(output: &str) -> Result<String, String> {
    // Find all potential JSON objects by looking for matching braces
    let mut brace_count = 0;
    let mut json_start: Option<usize> = None;
    let mut last_valid_json: Option<String> = None;

    let chars: Vec<char> = output.chars().collect();

    for (i, ch) in chars.iter().enumerate() {
        match ch {
            '{' => {
                if brace_count == 0 {
                    json_start = Some(i);
                }
                brace_count += 1;
            }
            '}' => {
                if brace_count <= 0 { continue; }
                brace_count -= 1;
                if brace_count == 0 {
                    if let Some(start) = json_start {
                        let potential_json: String = chars[start..=i].iter().collect();
                        // Validate it's actually JSON
                        if serde_json::from_str::<Value>(&potential_json).is_ok() {
                            last_valid_json = Some(potential_json);
                        }
                        json_start = None;
                    }
                }
            }
            _ => {}
        }
    }

    last_valid_json.ok_or_else(|| {
        // Truncate output for error message
        let preview: String = output.chars().take(500).collect();
        format!("No valid JSON found in output. Preview: {}", preview)
    })
}

/// Progress event for streaming updates
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AlphaArenaProgress {
    pub event_type: String,  // "step", "info", "warning", "error", "complete"
    pub step: u32,
    pub total_steps: u32,
    pub message: String,
    pub details: Option<Value>,
}

/// Execute Python alpha_arena main.py with JSON payload.
/// Uses spawn_blocking to avoid blocking the Tauri async runtime.
async fn execute_alpha_arena_action(
    app: &AppHandle,
    action: &str,
    params: Value,
    api_keys: Option<HashMap<String, String>>
) -> Result<Value, String> {
    // Create JSON payload with API keys
    let payload = json!({
        "action": action,
        "params": params,
        "api_keys": api_keys.unwrap_or_default()
    });
    let payload_str = serde_json::to_string(&payload)
        .map_err(|e| format!("Failed to serialize payload: {}", e))?;

    eprintln!("[Alpha Arena v2] Executing action: {}", action);

    // Run Python subprocess on a blocking thread to avoid stalling the async runtime
    let app_clone = app.clone();
    let stdout = tokio::task::spawn_blocking(move || {
        python::execute_sync(
            &app_clone,
            "alpha_arena/main.py",
            vec![payload_str]
        )
    })
    .await
    .map_err(|e| format!("Task join error: {}", e))??;

    eprintln!("[Alpha Arena v2] Output received, length: {} chars", stdout.len());

    // Try to parse JSON from output
    let stdout_trimmed = stdout.trim();
    if stdout_trimmed.is_empty() {
        eprintln!("[Alpha Arena v2] ERROR: Python script returned empty output!");
        return Err("Python script returned empty output".to_string());
    }

    // Find the LAST complete JSON object in output (skip any logging before it)
    eprintln!("[Alpha Arena v2] Extracting JSON from output...");
    let json_str = extract_json_from_output(stdout_trimmed)?;

    // Log extracted JSON preview (up to 1000 chars)
    let json_preview: String = json_str.chars().take(1000).collect();
    eprintln!("[Alpha Arena v2] Extracted JSON preview: {}", json_preview);

    let parsed: Value = serde_json::from_str(&json_str)
        .map_err(|e| format!("Failed to parse JSON: {}\nExtracted JSON: {}", e, json_str))?;

    // Log key fields from parsed JSON
    if let Some(success) = parsed.get("success") {
        eprintln!("[Alpha Arena v2] Response success: {}", success);
    }
    if let Some(error) = parsed.get("error") {
        eprintln!("[Alpha Arena v2] Response error: {}", error);
    }

    Ok(parsed)
}

/// Create a new AI trading competition (with streaming progress)
#[command]
pub async fn create_alpha_competition(app: AppHandle, config_json: String, api_keys: Option<HashMap<String, String>>) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] create_alpha_competition called");

    let config: Value = serde_json::from_str(&config_json)
        .map_err(|e| format!("Invalid config JSON: {}", e))?;

    // Emit initial progress
    emit_progress(&app, "step", 1, 5, "Validating competition configuration...", None);
    tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

    // Validate models and extract API keys if not provided
    let models = config.get("models").and_then(|m| m.as_array());
    let model_count = models.map(|m| m.len()).unwrap_or(0);

    // Build API keys map from models if not provided
    let mut final_api_keys: HashMap<String, String> = api_keys.unwrap_or_default();
    if let Some(models) = models {
        for model in models.iter() {
            if let (Some(provider), Some(api_key)) = (
                model.get("provider").and_then(|p| p.as_str()),
                model.get("api_key").and_then(|k| k.as_str())
            ) {
                if !api_key.is_empty() && !final_api_keys.contains_key(provider) {
                    final_api_keys.insert(provider.to_string(), api_key.to_string());
                }
            }
        }
    }

    eprintln!("[Alpha Arena v2] API keys collected for providers: {:?}", final_api_keys.keys().collect::<Vec<_>>());

    emit_progress(&app, "step", 2, 5, &format!("Setting up {} AI models...", model_count), None);
    tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

    // Emit model setup details
    if let Some(models) = models {
        for (i, model) in models.iter().enumerate() {
            let name = model.get("name").and_then(|n| n.as_str()).unwrap_or("Unknown");
            emit_progress(&app, "info", 2, 5, &format!("Initializing model {}: {}", i + 1, name), Some(json!({ "model": name })));
            tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;
        }
    }

    emit_progress(&app, "step", 3, 5, "Connecting to market data providers...", None);
    tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

    emit_progress(&app, "step", 4, 5, "Creating competition in backend...", None);

    // Execute the actual creation with API keys
    let result = execute_alpha_arena_action(&app, "create_competition", config, Some(final_api_keys)).await;

    match &result {
        Ok(value) => {
            let success = value.get("success").and_then(|s| s.as_bool()).unwrap_or(false);
            if success {
                emit_progress(&app, "complete", 5, 5, "Competition created successfully!", Some(value.clone()));
            } else {
                let error = value.get("error").and_then(|e| e.as_str()).unwrap_or("Unknown error");
                emit_progress(&app, "error", 5, 5, error, None);
            }
        }
        Err(e) => {
            emit_progress(&app, "error", 5, 5, e, None);
        }
    }

    result
}

/// Emit progress event to frontend
fn emit_progress(app: &AppHandle, event_type: &str, step: u32, total_steps: u32, message: &str, details: Option<Value>) {
    let progress = AlphaArenaProgress {
        event_type: event_type.to_string(),
        step,
        total_steps,
        message: message.to_string(),
        details,
    };
    let _ = app.emit("alpha-arena-progress", &progress);
}

/// Run a single competition cycle (with streaming progress)
#[command]
pub async fn run_alpha_cycle(app: AppHandle, competition_id: Option<String>, api_keys: Option<HashMap<String, String>>) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] run_alpha_cycle called");

    emit_progress(&app, "step", 1, 4, "Fetching latest market data...", None);
    tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

    emit_progress(&app, "step", 2, 4, "AI models analyzing market conditions...", None);

    let params = json!({
        "competition_id": competition_id
    });

    emit_progress(&app, "step", 3, 4, "Executing trading decisions...", None);

    let result = execute_alpha_arena_action(&app, "run_cycle", params, api_keys).await;

    match &result {
        Ok(value) => {
            let success = value.get("success").and_then(|s| s.as_bool()).unwrap_or(false);
            if success {
                let cycle_num = value.get("cycle_number").and_then(|c| c.as_u64()).unwrap_or(0);
                emit_progress(&app, "complete", 4, 4, &format!("Cycle {} completed!", cycle_num), Some(value.clone()));
            } else {
                let error = value.get("error").and_then(|e| e.as_str()).unwrap_or("Unknown error");
                emit_progress(&app, "error", 4, 4, error, None);
            }
        }
        Err(e) => {
            emit_progress(&app, "error", 4, 4, e, None);
        }
    }

    result
}

/// Get competition leaderboard
#[command]
pub async fn get_alpha_leaderboard(app: AppHandle, competition_id: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] get_alpha_leaderboard called for: {}", competition_id);

    let params = json!({
        "competition_id": competition_id
    });

    execute_alpha_arena_action(&app, "get_leaderboard", params, None).await
}

/// Get model trading decisions
#[command]
pub async fn get_alpha_model_decisions(
    app: AppHandle,
    competition_id: String,
    model_name: Option<String>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] get_alpha_model_decisions called for: {} (model: {:?})", competition_id, model_name);

    let params = json!({
        "competition_id": competition_id,
        "model_name": model_name
    });

    execute_alpha_arena_action(&app, "get_decisions", params, None).await
}

/// Get performance snapshots for charts
#[command]
pub async fn get_alpha_snapshots(app: AppHandle, competition_id: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] get_alpha_snapshots called for: {}", competition_id);

    let params = json!({
        "competition_id": competition_id
    });

    execute_alpha_arena_action(&app, "get_snapshots", params, None).await
}

/// Start auto-run for competition
#[command]
pub async fn start_alpha_competition(app: AppHandle, competition_id: Option<String>) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] start_alpha_competition called");

    let params = json!({
        "competition_id": competition_id
    });

    execute_alpha_arena_action(&app, "start_competition", params, None).await
}

/// Stop competition
#[command]
pub async fn stop_alpha_competition(app: AppHandle, competition_id: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] stop_alpha_competition called");

    let params = json!({
        "competition_id": competition_id
    });

    execute_alpha_arena_action(&app, "stop_competition", params, None).await
}

/// List available trading agents
#[command]
pub async fn list_alpha_agents(app: AppHandle) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] list_alpha_agents called");

    execute_alpha_arena_action(&app, "list_agents", json!({}), None).await
}

/// Get evaluation metrics for a model
#[command]
pub async fn get_alpha_evaluation(
    app: AppHandle,
    competition_id: String,
    model_name: String
) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] get_alpha_evaluation called");

    let params = json!({
        "competition_id": competition_id,
        "model_name": model_name
    });

    execute_alpha_arena_action(&app, "get_evaluation", params, None).await
}

/// List all competitions from the database
#[command]
pub async fn list_alpha_competitions(app: AppHandle, limit: Option<u32>) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] list_alpha_competitions called");

    let params = json!({
        "limit": limit.unwrap_or(50)
    });

    execute_alpha_arena_action(&app, "list_competitions", params, None).await
}

/// Get a specific competition by ID
#[command]
pub async fn get_alpha_competition(app: AppHandle, competition_id: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] get_alpha_competition called");

    let params = json!({
        "competition_id": competition_id
    });

    execute_alpha_arena_action(&app, "get_competition", params, None).await
}

/// Delete a competition
#[command]
pub async fn delete_alpha_competition(app: AppHandle, competition_id: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena v2] delete_alpha_competition called");

    let params = json!({
        "competition_id": competition_id
    });

    execute_alpha_arena_action(&app, "delete_competition", params, None).await
}

// ============================================================================
// HITL (Human-in-the-Loop) Commands
// ============================================================================

/// Check if a decision requires approval
#[command]
pub async fn check_alpha_approval(
    app: AppHandle,
    decision: Value,
    context: Value
) -> Result<Value, String> {
    eprintln!("[Alpha Arena HITL] check_alpha_approval called");

    let params = json!({
        "decision": decision,
        "context": context
    });

    execute_alpha_arena_action(&app, "check_approval", params, None).await
}

/// Approve a pending decision
#[command]
pub async fn approve_alpha_decision(
    app: AppHandle,
    request_id: String,
    approved_by: Option<String>,
    notes: Option<String>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena HITL] approve_alpha_decision called for: {}", request_id);

    let params = json!({
        "request_id": request_id,
        "approved_by": approved_by.unwrap_or_else(|| "user".to_string()),
        "notes": notes.unwrap_or_default()
    });

    execute_alpha_arena_action(&app, "approve_decision", params, None).await
}

/// Reject a pending decision
#[command]
pub async fn reject_alpha_decision(
    app: AppHandle,
    request_id: String,
    rejected_by: Option<String>,
    notes: Option<String>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena HITL] reject_alpha_decision called for: {}", request_id);

    let params = json!({
        "request_id": request_id,
        "rejected_by": rejected_by.unwrap_or_else(|| "user".to_string()),
        "notes": notes.unwrap_or_default()
    });

    execute_alpha_arena_action(&app, "reject_decision", params, None).await
}

/// Get all pending approval requests
#[command]
pub async fn get_alpha_pending_approvals(app: AppHandle) -> Result<Value, String> {
    eprintln!("[Alpha Arena HITL] get_alpha_pending_approvals called");

    execute_alpha_arena_action(&app, "get_pending_approvals", json!({}), None).await
}

/// Get HITL manager status and rules
#[command]
pub async fn get_alpha_hitl_status(app: AppHandle) -> Result<Value, String> {
    eprintln!("[Alpha Arena HITL] get_alpha_hitl_status called");

    execute_alpha_arena_action(&app, "get_hitl_status", json!({}), None).await
}

/// Update HITL rule settings
#[command]
pub async fn update_alpha_hitl_rule(
    app: AppHandle,
    rule_name: String,
    enabled: bool
) -> Result<Value, String> {
    eprintln!("[Alpha Arena HITL] update_alpha_hitl_rule called: {} = {}", rule_name, enabled);

    let params = json!({
        "rule_name": rule_name,
        "enabled": enabled
    });

    execute_alpha_arena_action(&app, "update_hitl_rule", params, None).await
}

// ============================================================================
// Portfolio Metrics Commands
// ============================================================================

/// Get portfolio metrics (Sharpe ratio, drawdown, etc.)
#[command]
pub async fn get_alpha_portfolio_metrics(
    app: AppHandle,
    competition_id: String,
    model_name: Option<String>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena] get_alpha_portfolio_metrics called");

    let params = json!({
        "competition_id": competition_id,
        "model_name": model_name
    });

    execute_alpha_arena_action(&app, "get_portfolio_metrics", params, None).await
}

/// Get equity curve data for charting
#[command]
pub async fn get_alpha_equity_curve(
    app: AppHandle,
    competition_id: String,
    model_name: Option<String>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena] get_alpha_equity_curve called");

    let params = json!({
        "competition_id": competition_id,
        "model_name": model_name
    });

    execute_alpha_arena_action(&app, "get_equity_curve", params, None).await
}

// ============================================================================
// Grid Strategy Commands
// ============================================================================

/// Create a grid strategy agent
#[command]
pub async fn create_alpha_grid_agent(
    app: AppHandle,
    name: String,
    upper_price: f64,
    lower_price: f64,
    grid_levels: u32,
    grid_type: Option<String>,
    total_investment: Option<f64>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Grid] create_alpha_grid_agent called: {}", name);

    let params = json!({
        "name": name,
        "upper_price": upper_price,
        "lower_price": lower_price,
        "grid_levels": grid_levels,
        "grid_type": grid_type.unwrap_or_else(|| "arithmetic".to_string()),
        "total_investment": total_investment.unwrap_or(10000.0)
    });

    execute_alpha_arena_action(&app, "create_grid_agent", params, None).await
}

/// Get grid agent status
#[command]
pub async fn get_alpha_grid_status(app: AppHandle, agent_name: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena Grid] get_alpha_grid_status called: {}", agent_name);

    let params = json!({
        "agent_name": agent_name
    });

    execute_alpha_arena_action(&app, "get_grid_status", params, None).await
}

// ============================================================================
// Research Commands
// ============================================================================

/// Get research report for a ticker
#[command]
pub async fn get_alpha_research(app: AppHandle, ticker: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena Research] get_alpha_research called: {}", ticker);

    let params = json!({
        "ticker": ticker
    });

    execute_alpha_arena_action(&app, "get_research", params, None).await
}

/// Get SEC filings for a ticker
#[command]
pub async fn get_alpha_sec_filings(
    app: AppHandle,
    ticker: String,
    form_types: Option<Vec<String>>,
    limit: Option<u32>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Research] get_alpha_sec_filings called: {}", ticker);

    let params = json!({
        "ticker": ticker,
        "form_types": form_types,
        "limit": limit.unwrap_or(10)
    });

    execute_alpha_arena_action(&app, "get_sec_filings", params, None).await
}

// ============================================================================
// Features Pipeline Commands
// ============================================================================

/// Get technical features for a symbol
#[command]
pub async fn get_alpha_features(
    app: AppHandle,
    symbol: String,
    price: f64
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Features] get_alpha_features called: {}", symbol);

    let params = json!({
        "symbol": symbol,
        "price": price
    });

    execute_alpha_arena_action(&app, "get_features", params, None).await
}

// ============================================================================
// Broker Commands
// ============================================================================

/// List all supported brokers
#[command]
pub async fn list_alpha_brokers(
    app: AppHandle,
    broker_type: Option<String>,
    region: Option<String>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Brokers] list_alpha_brokers called");

    let params = json!({
        "type": broker_type,
        "region": region
    });

    execute_alpha_arena_action(&app, "list_brokers", params, None).await
}

/// Get broker details by ID
#[command]
pub async fn get_alpha_broker(app: AppHandle, broker_id: String) -> Result<Value, String> {
    eprintln!("[Alpha Arena Brokers] get_alpha_broker called: {}", broker_id);

    let params = json!({
        "broker_id": broker_id
    });

    execute_alpha_arena_action(&app, "get_broker", params, None).await
}

/// Get ticker data from specific broker
#[command]
pub async fn get_alpha_broker_ticker(
    app: AppHandle,
    symbol: String,
    broker_id: Option<String>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Brokers] get_alpha_broker_ticker called: {} from {:?}", symbol, broker_id);

    let params = json!({
        "symbol": symbol,
        "broker_id": broker_id.unwrap_or_else(|| "binance".to_string())
    });

    execute_alpha_arena_action(&app, "get_broker_ticker", params, None).await
}

// ============================================================================
// Sentiment Commands
// ============================================================================

/// Get sentiment analysis for a symbol
#[command]
pub async fn get_alpha_sentiment(
    app: AppHandle,
    symbol: String,
    max_articles: Option<u32>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Sentiment] get_alpha_sentiment called: {}", symbol);

    let params = json!({
        "symbol": symbol,
        "max_articles": max_articles.unwrap_or(15)
    });

    execute_alpha_arena_action(&app, "get_sentiment", params, None).await
}

/// Get overall market mood from multiple symbols
#[command]
pub async fn get_alpha_market_mood(
    app: AppHandle,
    symbols: Option<Vec<String>>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Sentiment] get_alpha_market_mood called");

    let params = json!({
        "symbols": symbols.unwrap_or_else(|| vec!["SPY".to_string(), "QQQ".to_string(), "BTC".to_string()])
    });

    execute_alpha_arena_action(&app, "get_market_mood", params, None).await
}

// ============================================================================
// Trading Styles Commands (v2.2)
// ============================================================================

/// List all available trading styles
#[command]
pub async fn list_alpha_trading_styles(app: AppHandle) -> Result<Value, String> {
    eprintln!("[Alpha Arena Styles] list_alpha_trading_styles called");

    execute_alpha_arena_action(&app, "list_trading_styles", json!({}), None).await
}

/// Create multiple agents with different trading styles using the same provider
#[command]
pub async fn create_alpha_styled_agents(
    app: AppHandle,
    provider: String,
    model_id: String,
    styles: Option<Vec<String>>,
    initial_capital: Option<f64>
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Styles] create_alpha_styled_agents called: {}:{}", provider, model_id);

    let params = json!({
        "provider": provider,
        "model_id": model_id,
        "styles": styles,
        "initial_capital": initial_capital.unwrap_or(10000.0)
    });

    execute_alpha_arena_action(&app, "create_styled_agents", params, None).await
}

/// Generic action executor - allows calling any Alpha Arena action dynamically
#[command]
pub async fn run_alpha_action(
    app: AppHandle,
    action: String,
    params: Value
) -> Result<Value, String> {
    eprintln!("[Alpha Arena Generic] run_alpha_action called: {}", action);

    execute_alpha_arena_action(&app, &action, params, None).await
}
