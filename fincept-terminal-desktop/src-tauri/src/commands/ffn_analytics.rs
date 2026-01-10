// FFN Analytics Commands - Portfolio performance analysis via subprocess
#![allow(dead_code)]
use crate::utils::python::execute_python_subprocess;

// ============================================================================
// FFN ANALYTICS COMMANDS
// ============================================================================

/// Check FFN library status and availability
#[tauri::command]
pub async fn ffn_check_status(app: tauri::AppHandle) -> Result<String, String> {
    let args = vec!["check_status".to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
}

/// Portfolio optimization - calculate optimal weights using various methods
#[tauri::command]
pub async fn ffn_portfolio_optimization(
    app: tauri::AppHandle,
    prices_json: String,
    method: Option<String>,
    risk_free_rate: Option<f64>,
    weight_bounds: Option<Vec<f64>>,
) -> Result<String, String> {
    let bounds = weight_bounds.unwrap_or_else(|| vec![0.0, 1.0]);
    let params = serde_json::json!({
        "prices": prices_json,
        "method": method.unwrap_or_else(|| "erc".to_string()),
        "risk_free_rate": risk_free_rate.unwrap_or(0.0),
        "weight_bounds": bounds
    });

    let args = vec!["portfolio_optimization".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
}

/// Benchmark comparison - compare portfolio against a benchmark
#[tauri::command]
pub async fn ffn_benchmark_comparison(
    app: tauri::AppHandle,
    prices_json: String,
    benchmark_prices_json: String,
    benchmark_name: Option<String>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "prices": prices_json,
        "benchmark_prices": benchmark_prices_json,
        "benchmark_name": benchmark_name.unwrap_or_else(|| "Benchmark".to_string()),
        "risk_free_rate": risk_free_rate.unwrap_or(0.0)
    });

    let args = vec!["benchmark_comparison".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/ffn_wrapper/ffn_service.py",
        &args,
        Some("ffn"),
    )
}
