/**
 * pmdarima Tauri Commands
 * Bridges frontend to pmdarima Python wrapper for time series forecasting
 * Auto-ARIMA, ARIMA modeling, preprocessing, and diagnostics
 */

use std::process::Command;
use serde_json::Value;

/// Execute Python script and return JSON result
fn execute_python_script(script_name: &str, function: &str, args: &str) -> Result<String, String> {
    let script_path = format!("resources/scripts/Analytics/pmdarima_wrapper/{}", script_name);

    let output = Command::new("python")
        .arg(&script_path)
        .arg("--function")
        .arg(function)
        .arg("--args")
        .arg(args)
        .output()
        .map_err(|e| format!("Failed to execute Python: {}", e))?;

    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script error: {}", error));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

// ==================== ARIMA COMMANDS ====================

#[tauri::command]
pub async fn pmdarima_fit_auto_arima(
    data: Vec<f64>,
    seasonal: Option<bool>,
    m: Option<i32>,
    max_p: Option<i32>,
    max_q: Option<i32>
) -> Result<String, String> {
    let args = serde_json::json!({
        "y": data,
        "seasonal": seasonal.unwrap_or(false),
        "m": m.unwrap_or(1),
        "max_p": max_p.unwrap_or(5),
        "max_q": max_q.unwrap_or(5)
    }).to_string();

    execute_python_script("arima.py", "fit_auto_arima", &args)
}

#[tauri::command]
pub async fn pmdarima_forecast_auto_arima(
    data: Vec<f64>,
    n_periods: i32,
    seasonal: Option<bool>,
    return_conf_int: Option<bool>,
    alpha: Option<f64>
) -> Result<String, String> {
    let args = serde_json::json!({
        "y": data,
        "n_periods": n_periods,
        "seasonal": seasonal.unwrap_or(false),
        "return_conf_int": return_conf_int.unwrap_or(true),
        "alpha": alpha.unwrap_or(0.05)
    }).to_string();

    execute_python_script("arima.py", "forecast_auto_arima", &args)
}

#[tauri::command]
pub async fn pmdarima_forecast_arima(
    data: Vec<f64>,
    p: i32,
    d: i32,
    q: i32,
    n_periods: i32,
    return_conf_int: Option<bool>,
    alpha: Option<f64>
) -> Result<String, String> {
    let args = serde_json::json!({
        "y": data,
        "order": [p, d, q],
        "n_periods": n_periods,
        "return_conf_int": return_conf_int.unwrap_or(true),
        "alpha": alpha.unwrap_or(0.05)
    }).to_string();

    execute_python_script("arima.py", "forecast_arima", &args)
}

// ==================== PREPROCESSING COMMANDS ====================

#[tauri::command]
pub async fn pmdarima_boxcox_transform(
    data: Vec<f64>
) -> Result<String, String> {
    let args = serde_json::json!({
        "y": data
    }).to_string();

    execute_python_script("preprocessing.py", "apply_boxcox_transform", &args)
}

#[tauri::command]
pub async fn pmdarima_inverse_boxcox(
    data: Vec<f64>,
    lambda: f64
) -> Result<String, String> {
    let args = serde_json::json!({
        "transformed": data,
        "lambda": lambda
    }).to_string();

    execute_python_script("preprocessing.py", "inverse_boxcox_transform", &args)
}

// ==================== DIAGNOSTICS COMMANDS ====================

#[tauri::command]
pub async fn pmdarima_calculate_acf(
    data: Vec<f64>,
    nlags: Option<i32>
) -> Result<String, String> {
    let args = serde_json::json!({
        "y": data,
        "nlags": nlags.unwrap_or(40)
    }).to_string();

    execute_python_script("utils.py", "calculate_acf", &args)
}

#[tauri::command]
pub async fn pmdarima_calculate_pacf(
    data: Vec<f64>,
    nlags: Option<i32>
) -> Result<String, String> {
    let args = serde_json::json!({
        "y": data,
        "nlags": nlags.unwrap_or(40)
    }).to_string();

    execute_python_script("utils.py", "calculate_pacf", &args)
}

#[tauri::command]
pub async fn pmdarima_decompose_timeseries(
    data: Vec<f64>,
    decomp_type: String,
    period: i32
) -> Result<String, String> {
    let args = serde_json::json!({
        "y": data,
        "type": decomp_type,
        "m": period
    }).to_string();

    execute_python_script("utils.py", "decompose_timeseries", &args)
}

// ==================== MODEL SELECTION COMMANDS ====================

#[tauri::command]
pub async fn pmdarima_cross_validate(
    data: Vec<f64>,
    p: i32,
    d: i32,
    q: i32,
    cv_splits: i32
) -> Result<String, String> {
    let args = serde_json::json!({
        "y": data,
        "order": [p, d, q],
        "cv_splits": cv_splits
    }).to_string();

    execute_python_script("model_selection.py", "cross_validate_arima", &args)
}
