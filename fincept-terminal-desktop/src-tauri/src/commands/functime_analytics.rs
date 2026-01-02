// Functime Analytics Commands - Time series forecasting via PyO3
#![allow(dead_code)]
use crate::python_runtime;
use crate::utils::python::get_script_path;

// ============================================================================
// FUNCTIME ANALYTICS COMMANDS
// ============================================================================

/// Check functime library status and availability
#[tauri::command]
pub async fn functime_check_status(app: tauri::AppHandle) -> Result<String, String> {
    let args = vec!["check_status".to_string()];
    let script_path = get_script_path(&app, "Analytics/functime_wrapper/functime_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Run forecasting with specified model
/// Models: linear, lasso, ridge, elasticnet, knn, lightgbm, auto_lasso, auto_ridge, etc.
#[tauri::command]
pub async fn functime_forecast(
    app: tauri::AppHandle,
    data_json: String,
    model: Option<String>,
    horizon: Option<i32>,
    frequency: Option<String>,
    alpha: Option<f64>,
    l1_ratio: Option<f64>,
    n_neighbors: Option<i32>,
    lags: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "model": model.unwrap_or_else(|| "linear".to_string()),
        "horizon": horizon.unwrap_or(7),
        "frequency": frequency.unwrap_or_else(|| "1d".to_string()),
        "alpha": alpha.unwrap_or(1.0),
        "l1_ratio": l1_ratio.unwrap_or(0.5),
        "n_neighbors": n_neighbors.unwrap_or(5),
        "lags": lags.unwrap_or(3)
    });

    let args = vec!["forecast".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/functime_wrapper/functime_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Apply preprocessing transformations to time series data
/// Methods: boxcox, scale, difference, lags, rolling, impute
#[tauri::command]
pub async fn functime_preprocess(
    app: tauri::AppHandle,
    data_json: String,
    method: Option<String>,
    scale_method: Option<String>,
    order: Option<i32>,
    lags: Option<Vec<i32>>,
    window_sizes: Option<Vec<i32>>,
    stats: Option<Vec<String>>,
    impute_method: Option<String>,
    lambda: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "method": method.unwrap_or_else(|| "scale".to_string()),
        "scale_method": scale_method.unwrap_or_else(|| "standard".to_string()),
        "order": order.unwrap_or(1),
        "lags": lags.unwrap_or_else(|| vec![1, 2, 3]),
        "window_sizes": window_sizes.unwrap_or_else(|| vec![3, 7]),
        "stats": stats.unwrap_or_else(|| vec!["mean".to_string()]),
        "impute_method": impute_method.unwrap_or_else(|| "forward".to_string()),
        "lambda": lambda
    });

    let args = vec!["preprocess".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/functime_wrapper/functime_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Perform cross-validation splits
/// Methods: train_test, expanding, sliding
#[tauri::command]
pub async fn functime_cross_validate(
    app: tauri::AppHandle,
    data_json: String,
    method: Option<String>,
    test_size: Option<i32>,
    n_splits: Option<i32>,
    train_size: Option<i32>,
    step: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "method": method.unwrap_or_else(|| "train_test".to_string()),
        "test_size": test_size.unwrap_or(1),
        "n_splits": n_splits.unwrap_or(3),
        "train_size": train_size.unwrap_or(5),
        "step": step.unwrap_or(1)
    });

    let args = vec!["cross_validate".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/functime_wrapper/functime_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate forecast accuracy metrics
/// Metrics: mae, rmse, smape, mape, mse, mase, rmsse
#[tauri::command]
pub async fn functime_metrics(
    app: tauri::AppHandle,
    y_true_json: String,
    y_pred_json: String,
    metrics: Option<Vec<String>>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "y_true": y_true_json,
        "y_pred": y_pred_json,
        "metrics": metrics.unwrap_or_else(|| vec!["mae".to_string(), "rmse".to_string(), "smape".to_string()])
    });

    let args = vec!["metrics".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/functime_wrapper/functime_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Add calendar or holiday features
/// Feature types: calendar, future_calendar
#[tauri::command]
pub async fn functime_add_features(
    app: tauri::AppHandle,
    data_json: String,
    feature_type: Option<String>,
    frequency: Option<String>,
    horizon: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "feature_type": feature_type.unwrap_or_else(|| "calendar".to_string()),
        "frequency": frequency.unwrap_or_else(|| "1d".to_string()),
        "horizon": horizon.unwrap_or(7)
    });

    let args = vec!["add_features".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/functime_wrapper/functime_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Get seasonal period for a frequency
#[tauri::command]
pub async fn functime_seasonal_period(
    app: tauri::AppHandle,
    frequency: Option<String>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "frequency": frequency.unwrap_or_else(|| "1d".to_string())
    });

    let args = vec!["seasonal_period".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/functime_wrapper/functime_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Run a complete forecasting pipeline with preprocessing and evaluation
#[tauri::command]
pub async fn functime_full_pipeline(
    app: tauri::AppHandle,
    data_json: String,
    model: Option<String>,
    horizon: Option<i32>,
    frequency: Option<String>,
    test_size: Option<i32>,
    preprocess: Option<String>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "model": model.unwrap_or_else(|| "linear".to_string()),
        "horizon": horizon.unwrap_or(7),
        "frequency": frequency.unwrap_or_else(|| "1d".to_string()),
        "test_size": test_size,
        "preprocess": preprocess
    });

    let args = vec!["full_pipeline".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/functime_wrapper/functime_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}
