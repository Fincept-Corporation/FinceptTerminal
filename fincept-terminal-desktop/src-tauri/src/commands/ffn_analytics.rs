// FFN Analytics Commands - Portfolio performance analysis via PyO3
#![allow(dead_code)]
use crate::python_runtime;
use crate::utils::python::get_script_path;

// ============================================================================
// FFN ANALYTICS COMMANDS
// ============================================================================

/// Check FFN library status and availability
#[tauri::command]
pub async fn ffn_check_status(app: tauri::AppHandle) -> Result<String, String> {
    let args = vec!["check_status".to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate comprehensive performance statistics for price data
#[tauri::command]
pub async fn ffn_calculate_performance(
    app: tauri::AppHandle,
    prices_json: String,
    config: Option<String>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json,
        "config": config
    });

    let args = vec!["calculate_performance".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate drawdown analysis with details
#[tauri::command]
pub async fn ffn_calculate_drawdowns(
    app: tauri::AppHandle,
    prices_json: String,
    threshold: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json,
        "threshold": threshold.unwrap_or(0.10)
    });

    let args = vec!["calculate_drawdowns".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate rolling performance metrics
#[tauri::command]
pub async fn ffn_calculate_rolling_metrics(
    app: tauri::AppHandle,
    prices_json: String,
    window: Option<i32>,
    metrics: Option<Vec<String>>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json,
        "window": window.unwrap_or(252),
        "metrics": metrics.unwrap_or_else(|| vec!["sharpe".to_string(), "volatility".to_string(), "returns".to_string()])
    });

    let args = vec!["calculate_rolling_metrics".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate monthly returns table
#[tauri::command]
pub async fn ffn_monthly_returns(
    app: tauri::AppHandle,
    prices_json: String,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json
    });

    let args = vec!["monthly_returns".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Rebase prices to a starting value
#[tauri::command]
pub async fn ffn_rebase_prices(
    app: tauri::AppHandle,
    prices_json: String,
    base_value: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json,
        "base_value": base_value.unwrap_or(100.0)
    });

    let args = vec!["rebase_prices".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Compare multiple assets performance
#[tauri::command]
pub async fn ffn_compare_assets(
    app: tauri::AppHandle,
    prices_json: String,
    benchmark: Option<String>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json,
        "benchmark": benchmark,
        "risk_free_rate": risk_free_rate.unwrap_or(0.0)
    });

    let args = vec!["compare_assets".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate risk metrics (Ulcer Index, etc.)
#[tauri::command]
pub async fn ffn_risk_metrics(
    app: tauri::AppHandle,
    prices_json: String,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json,
        "risk_free_rate": risk_free_rate.unwrap_or(0.0)
    });

    let args = vec!["risk_metrics".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Full portfolio analysis - combines all metrics
#[tauri::command]
pub async fn ffn_full_analysis(
    app: tauri::AppHandle,
    prices_json: String,
    config: Option<String>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json,
        "config": config
    });

    let args = vec!["full_analysis".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/ffn_wrapper/ffn_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}
