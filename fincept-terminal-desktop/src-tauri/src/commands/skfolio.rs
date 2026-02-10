// skfolio Portfolio Optimization Commands
// Uses python_skfolio_lib/worker_handler.py with operation + JSON dispatch
use crate::python;
use tauri::command;

/// Helper to execute a skfolio operation via worker_handler.py
fn execute_skfolio_op(
    app: &tauri::AppHandle,
    operation: &str,
    data: serde_json::Value,
) -> Result<String, String> {
    let args = vec![operation.to_string(), data.to_string()];
    python::execute_sync(app, "Analytics/python_skfolio_lib/worker_handler.py", args)
}

// ==================== PORTFOLIO OPTIMIZATION ====================

#[command]
pub async fn skfolio_optimize_portfolio(
    app: tauri::AppHandle,
    prices_data: String,
    optimization_method: Option<String>,
    objective_function: Option<String>,
    risk_measure: Option<String>,
    config: Option<String>,
) -> Result<String, String> {
    let config_val: serde_json::Value = config
        .and_then(|c| serde_json::from_str(&c).ok())
        .unwrap_or(serde_json::json!({}));

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "optimization_method": optimization_method.unwrap_or_else(|| "mean_risk".to_string()),
        "objective_function": objective_function.unwrap_or_else(|| "maximize_ratio".to_string()),
        "risk_measure": risk_measure.unwrap_or_else(|| "variance".to_string()),
        "config": config_val
    });
    execute_skfolio_op(&app, "optimize", payload)
}

// ==================== EFFICIENT FRONTIER ====================

#[command]
pub async fn skfolio_efficient_frontier(
    app: tauri::AppHandle,
    prices_data: String,
    n_portfolios: Option<i32>,
    config: Option<String>,
) -> Result<String, String> {
    let config_val: serde_json::Value = config
        .and_then(|c| serde_json::from_str(&c).ok())
        .unwrap_or(serde_json::json!({}));

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "n_portfolios": n_portfolios.unwrap_or(100),
        "risk_measure": config_val.get("risk_measure").and_then(|v| v.as_str()).unwrap_or("variance")
    });
    execute_skfolio_op(&app, "efficient_frontier", payload)
}

// ==================== RISK METRICS ====================

#[command]
pub async fn skfolio_risk_metrics(
    app: tauri::AppHandle,
    prices_data: String,
    weights: Option<String>,
) -> Result<String, String> {
    let weights_val: serde_json::Value = weights
        .and_then(|w| serde_json::from_str(&w).ok())
        .unwrap_or(serde_json::Value::Null);

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "weights": weights_val
    });
    execute_skfolio_op(&app, "risk_metrics", payload)
}

// ==================== STRESS TESTING ====================

#[command]
pub async fn skfolio_stress_test(
    app: tauri::AppHandle,
    prices_data: String,
    weights: String,
    scenarios: Option<String>,
    n_simulations: Option<i32>,
) -> Result<String, String> {
    let weights_val: serde_json::Value = serde_json::from_str(&weights)
        .unwrap_or(serde_json::Value::Null);
    let scenarios_val: serde_json::Value = scenarios
        .and_then(|s| serde_json::from_str(&s).ok())
        .unwrap_or(serde_json::Value::Null);

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "weights": weights_val,
        "scenarios": scenarios_val,
        "n_simulations": n_simulations.unwrap_or(10000)
    });
    execute_skfolio_op(&app, "stress_test", payload)
}

// ==================== BACKTESTING ====================

#[command]
pub async fn skfolio_backtest_strategy(
    app: tauri::AppHandle,
    prices_data: String,
    rebalance_freq: Option<i32>,
    window_size: Option<i32>,
    config: Option<String>,
) -> Result<String, String> {
    let config_val: serde_json::Value = config
        .and_then(|c| serde_json::from_str(&c).ok())
        .unwrap_or(serde_json::json!({}));

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "rebalance_freq": rebalance_freq.unwrap_or(21),
        "window_size": window_size.unwrap_or(252),
        "optimization_method": config_val.get("optimization_method").and_then(|v| v.as_str()).unwrap_or("mean_risk"),
        "risk_measure": config_val.get("risk_measure").and_then(|v| v.as_str()).unwrap_or("variance")
    });
    execute_skfolio_op(&app, "backtest", payload)
}

// ==================== COMPARE STRATEGIES ====================

#[command]
pub async fn skfolio_compare_strategies(
    app: tauri::AppHandle,
    prices_data: String,
    strategies: String,
    metric: Option<String>,
) -> Result<String, String> {
    let strategies_val: serde_json::Value = serde_json::from_str(&strategies)
        .unwrap_or(serde_json::Value::Null);

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "strategies": strategies_val,
        "metric": metric.unwrap_or_else(|| "sharpe_ratio".to_string())
    });
    execute_skfolio_op(&app, "compare_strategies", payload)
}

// ==================== RISK ATTRIBUTION ====================

#[command]
pub async fn skfolio_risk_attribution(
    app: tauri::AppHandle,
    prices_data: String,
    weights: String,
) -> Result<String, String> {
    let weights_val: serde_json::Value = serde_json::from_str(&weights)
        .unwrap_or(serde_json::Value::Null);

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "weights": weights_val
    });
    execute_skfolio_op(&app, "risk_attribution", payload)
}

// ==================== HYPERPARAMETER TUNING ====================

#[command]
pub async fn skfolio_hyperparameter_tuning(
    app: tauri::AppHandle,
    prices_data: String,
    param_grid: Option<String>,
    cv_method: Option<String>,
) -> Result<String, String> {
    let param_grid_val: serde_json::Value = param_grid
        .and_then(|p| serde_json::from_str(&p).ok())
        .unwrap_or(serde_json::Value::Null);

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "param_grid": param_grid_val,
        "cv_method": cv_method.unwrap_or_else(|| "walk_forward".to_string())
    });
    execute_skfolio_op(&app, "hyperparameter_tune", payload)
}

// ==================== MEASURES ====================

#[command]
pub async fn skfolio_measures(
    app: tauri::AppHandle,
    prices_data: String,
    weights: Option<String>,
    measure_name: Option<String>,
) -> Result<String, String> {
    let weights_val: serde_json::Value = weights
        .and_then(|w| serde_json::from_str(&w).ok())
        .unwrap_or(serde_json::Value::Null);

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "weights": weights_val,
        "measure_name": measure_name
    });
    execute_skfolio_op(&app, "measures", payload)
}

// ==================== MODEL VALIDATION ====================

#[command]
pub async fn skfolio_validate_model(
    app: tauri::AppHandle,
    prices_data: String,
    validation_method: Option<String>,
    risk_measure: Option<String>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "prices_data": prices_data,
        "validation_method": validation_method.unwrap_or_else(|| "walk_forward".to_string()),
        "risk_measure": risk_measure.unwrap_or_else(|| "variance".to_string())
    });
    execute_skfolio_op(&app, "validate_model", payload)
}

// ==================== SCENARIO ANALYSIS ====================

#[command]
pub async fn skfolio_scenario_analysis(
    app: tauri::AppHandle,
    prices_data: String,
    weights: String,
    scenarios: String,
) -> Result<String, String> {
    let weights_val: serde_json::Value = serde_json::from_str(&weights)
        .unwrap_or(serde_json::Value::Null);
    let scenarios_val: serde_json::Value = serde_json::from_str(&scenarios)
        .unwrap_or(serde_json::Value::Null);

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "weights": weights_val,
        "scenarios": scenarios_val
    });
    execute_skfolio_op(&app, "scenario_analysis", payload)
}

// ==================== GENERATE REPORT ====================

#[command]
pub async fn skfolio_generate_report(
    app: tauri::AppHandle,
    prices_data: String,
    weights: String,
    config: Option<String>,
) -> Result<String, String> {
    let config_val: serde_json::Value = config
        .and_then(|c| serde_json::from_str(&c).ok())
        .unwrap_or(serde_json::json!({}));

    let payload = serde_json::json!({
        "prices_data": prices_data,
        "weights": weights,
        "optimization_method": config_val.get("optimization_method").and_then(|v| v.as_str()).unwrap_or("mean_risk"),
        "objective_function": config_val.get("objective_function").and_then(|v| v.as_str()).unwrap_or("maximize_ratio"),
        "risk_measure": config_val.get("risk_measure").and_then(|v| v.as_str()).unwrap_or("variance"),
        "config": config_val
    });
    execute_skfolio_op(&app, "generate_report", payload)
}

// ==================== EXPORT WEIGHTS ====================

#[command]
pub async fn skfolio_export_weights(
    app: tauri::AppHandle,
    weights: String,
    asset_names: String,
    filename: Option<String>,
) -> Result<String, String> {
    // Export is handled client-side; this returns weights in a structured format
    let weights_val: serde_json::Value = serde_json::from_str(&weights)
        .unwrap_or(serde_json::Value::Null);
    let names_val: serde_json::Value = serde_json::from_str(&asset_names)
        .unwrap_or(serde_json::Value::Null);

    let result = serde_json::json!({
        "status": "success",
        "weights": weights_val,
        "asset_names": names_val,
        "filename": filename
    });
    Ok(result.to_string())
}
