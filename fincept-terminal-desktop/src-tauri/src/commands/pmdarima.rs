/**
 * pmdarima Tauri Commands
 * Bridges frontend to pmdarima Python wrapper for time series forecasting
 * Auto-ARIMA, ARIMA modeling, preprocessing, and diagnostics
 */

use crate::python;

// ==================== ARIMA COMMANDS ====================

#[tauri::command]
pub async fn pmdarima_fit_auto_arima(
    app: tauri::AppHandle,
    data: Vec<f64>,
    seasonal: Option<bool>,
    m: Option<i32>,
    max_p: Option<i32>,
    max_q: Option<i32>
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "y": data,
        "seasonal": seasonal.unwrap_or(false),
        "m": m.unwrap_or(1),
        "max_p": max_p.unwrap_or(5),
        "max_q": max_q.unwrap_or(5)
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/arima.py",
        vec!["--function".to_string(), "fit_auto_arima".to_string(), "--args".to_string(), args_json]
    )
}

#[tauri::command]
pub async fn pmdarima_forecast_auto_arima(
    app: tauri::AppHandle,
    data: Vec<f64>,
    n_periods: i32,
    seasonal: Option<bool>,
    return_conf_int: Option<bool>,
    alpha: Option<f64>
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "y": data,
        "n_periods": n_periods,
        "seasonal": seasonal.unwrap_or(false),
        "return_conf_int": return_conf_int.unwrap_or(true),
        "alpha": alpha.unwrap_or(0.05)
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/arima.py",
        vec!["--function".to_string(), "forecast_auto_arima".to_string(), "--args".to_string(), args_json]
    )
}

#[tauri::command]
pub async fn pmdarima_forecast_arima(
    app: tauri::AppHandle,
    data: Vec<f64>,
    p: i32,
    d: i32,
    q: i32,
    n_periods: i32,
    return_conf_int: Option<bool>,
    alpha: Option<f64>
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "y": data,
        "order": [p, d, q],
        "n_periods": n_periods,
        "return_conf_int": return_conf_int.unwrap_or(true),
        "alpha": alpha.unwrap_or(0.05)
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/arima.py",
        vec!["--function".to_string(), "forecast_arima".to_string(), "--args".to_string(), args_json]
    )
}

// ==================== PREPROCESSING COMMANDS ====================

#[tauri::command]
pub async fn pmdarima_boxcox_transform(
    app: tauri::AppHandle,
    data: Vec<f64>
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "y": data
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/preprocessing.py",
        vec!["--function".to_string(), "apply_boxcox_transform".to_string(), "--args".to_string(), args_json]
    )
}

#[tauri::command]
pub async fn pmdarima_inverse_boxcox(
    app: tauri::AppHandle,
    data: Vec<f64>,
    lambda: f64
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "transformed": data,
        "lambda": lambda
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/preprocessing.py",
        vec!["--function".to_string(), "inverse_boxcox_transform".to_string(), "--args".to_string(), args_json]
    )
}

// ==================== DIAGNOSTICS COMMANDS ====================

#[tauri::command]
pub async fn pmdarima_calculate_acf(
    app: tauri::AppHandle,
    data: Vec<f64>,
    nlags: Option<i32>
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "y": data,
        "nlags": nlags.unwrap_or(40)
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/utils.py",
        vec!["--function".to_string(), "calculate_acf".to_string(), "--args".to_string(), args_json]
    )
}

#[tauri::command]
pub async fn pmdarima_calculate_pacf(
    app: tauri::AppHandle,
    data: Vec<f64>,
    nlags: Option<i32>
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "y": data,
        "nlags": nlags.unwrap_or(40)
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/utils.py",
        vec!["--function".to_string(), "calculate_pacf".to_string(), "--args".to_string(), args_json]
    )
}

#[tauri::command]
pub async fn pmdarima_decompose_timeseries(
    app: tauri::AppHandle,
    data: Vec<f64>,
    decomp_type: String,
    period: i32
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "y": data,
        "type": decomp_type,
        "m": period
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/utils.py",
        vec!["--function".to_string(), "decompose_timeseries".to_string(), "--args".to_string(), args_json]
    )
}

// ==================== MODEL SELECTION COMMANDS ====================

#[tauri::command]
pub async fn pmdarima_cross_validate(
    app: tauri::AppHandle,
    data: Vec<f64>,
    p: i32,
    d: i32,
    q: i32,
    cv_splits: i32
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "y": data,
        "order": [p, d, q],
        "cv_splits": cv_splits
    }).to_string();

    python::execute_sync(
        &app,
        "Analytics/pmdarima_wrapper/model_selection.py",
        vec!["--function".to_string(), "cross_validate_arima".to_string(), "--args".to_string(), args_json]
    )
}
