// Functime Analytics Commands - Time series forecasting via subprocess
#![allow(dead_code)]
use crate::utils::python::execute_python_subprocess;

// ============================================================================
// FUNCTIME ANALYTICS COMMANDS
// ============================================================================

/// Check functime library status and availability
#[tauri::command]
pub async fn functime_check_status(app: tauri::AppHandle) -> Result<String, String> {
    let args = vec!["check_status".to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
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
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

// ============================================================================
// ADVANCED FUNCTIME COMMANDS
// ============================================================================

/// Advanced forecasting with additional models (naive, ses, holt, theta, croston, xgboost, catboost)
#[tauri::command]
pub async fn functime_advanced_forecast(
    app: tauri::AppHandle,
    data_json: String,
    model: Option<String>,
    horizon: Option<i32>,
    frequency: Option<String>,
    seasonal_period: Option<i32>,
    alpha: Option<f64>,
    beta: Option<f64>,
    theta: Option<f64>,
    lags: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "model": model.unwrap_or_else(|| "naive".to_string()),
        "horizon": horizon.unwrap_or(7),
        "frequency": frequency.unwrap_or_else(|| "1d".to_string()),
        "seasonal_period": seasonal_period.unwrap_or(7),
        "alpha": alpha.unwrap_or(0.3),
        "beta": beta.unwrap_or(0.1),
        "theta": theta.unwrap_or(2.0),
        "lags": lags.unwrap_or(10)
    });

    let args = vec!["advanced_forecast".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

/// Ensemble forecasting combining multiple models
#[tauri::command]
pub async fn functime_ensemble(
    app: tauri::AppHandle,
    data_json: String,
    models: Option<Vec<String>>,
    weights: Option<Vec<f64>>,
    horizon: Option<i32>,
    frequency: Option<String>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "models": models.unwrap_or_else(|| vec!["naive".to_string(), "ses".to_string(), "linear".to_string()]),
        "weights": weights,
        "horizon": horizon.unwrap_or(7),
        "frequency": frequency.unwrap_or_else(|| "1d".to_string())
    });

    let args = vec!["ensemble".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

/// Automatic ensemble with weight optimization
#[tauri::command]
pub async fn functime_auto_ensemble(
    app: tauri::AppHandle,
    data_json: String,
    horizon: Option<i32>,
    frequency: Option<String>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "horizon": horizon.unwrap_or(7),
        "frequency": frequency.unwrap_or_else(|| "1d".to_string())
    });

    let args = vec!["auto_ensemble".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

/// Anomaly detection in time series
#[tauri::command]
pub async fn functime_anomaly_detection(
    app: tauri::AppHandle,
    data_json: String,
    method: Option<String>,
    threshold: Option<f64>,
    window_size: Option<i32>,
    contamination: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "method": method.unwrap_or_else(|| "zscore".to_string()),
        "threshold": threshold.unwrap_or(3.0),
        "window_size": window_size.unwrap_or(10),
        "contamination": contamination.unwrap_or(0.1)
    });

    let args = vec!["anomaly_detection".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

/// Seasonality analysis (decomposition, strength, detection)
#[tauri::command]
pub async fn functime_seasonality(
    app: tauri::AppHandle,
    data_json: String,
    // Accept both parameter naming conventions from frontend
    operation: Option<String>,
    analysis_type: Option<String>,
    period: Option<i32>,
    method: Option<String>,
    max_period: Option<i32>,
    robust: Option<bool>,
    model: Option<String>,
    model_type: Option<String>,
) -> Result<String, String> {
    // Use 'operation' if provided, fall back to 'analysis_type', then default
    let op = operation
        .or(analysis_type)
        .unwrap_or_else(|| "decompose".to_string());

    // Use 'model' if provided, fall back to 'model_type', then default
    let model_val = model
        .or(model_type)
        .unwrap_or_else(|| "additive".to_string());

    let params = serde_json::json!({
        "data": data_json,
        "operation": op,
        "period": period,
        "method": method.unwrap_or_else(|| "fft".to_string()),
        "max_period": max_period.unwrap_or(365),
        "robust": robust.unwrap_or(true),
        "model_type": model_val
    });

    let args = vec!["seasonality".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

/// Calculate confidence intervals for forecasts
#[tauri::command]
pub async fn functime_confidence_intervals(
    app: tauri::AppHandle,
    data_json: String,
    method: Option<String>,
    confidence_level: Option<f64>,
    horizon: Option<i32>,
    n_bootstrap: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "method": method.unwrap_or_else(|| "bootstrap".to_string()),
        "confidence_level": confidence_level.unwrap_or(0.95),
        "horizon": horizon.unwrap_or(7),
        "n_bootstrap": n_bootstrap.unwrap_or(100)
    });

    let args = vec!["confidence_intervals".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

/// Feature importance analysis for ML models
#[tauri::command]
pub async fn functime_feature_importance(
    app: tauri::AppHandle,
    data_json: String,
    model: Option<String>,
    method: Option<String>,
    lags: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "model": model.unwrap_or_else(|| "lightgbm".to_string()),
        "method": method.unwrap_or_else(|| "shap".to_string()),
        "lags": lags.unwrap_or(10)
    });

    let args = vec!["feature_importance".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

/// Advanced cross-validation with model evaluation
#[tauri::command]
pub async fn functime_advanced_cv(
    app: tauri::AppHandle,
    data_json: String,
    model: Option<String>,
    cv_method: Option<String>,
    n_splits: Option<i32>,
    horizon: Option<i32>,
    gap: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "model": model.unwrap_or_else(|| "linear".to_string()),
        "cv_method": cv_method.unwrap_or_else(|| "expanding".to_string()),
        "n_splits": n_splits.unwrap_or(5),
        "horizon": horizon.unwrap_or(7),
        "gap": gap.unwrap_or(0)
    });

    let args = vec!["advanced_cv".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}

/// Strategy backtesting for forecasting models
#[tauri::command]
pub async fn functime_backtest(
    app: tauri::AppHandle,
    data_json: String,
    model: Option<String>,
    strategy: Option<String>,
    initial_capital: Option<f64>,
    horizon: Option<i32>,
    refit_frequency: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "data": data_json,
        "model": model.unwrap_or_else(|| "linear".to_string()),
        "strategy": strategy.unwrap_or_else(|| "long_only".to_string()),
        "initial_capital": initial_capital.unwrap_or(10000.0),
        "horizon": horizon.unwrap_or(7),
        "refit_frequency": refit_frequency.unwrap_or(30)
    });

    let args = vec!["backtest".to_string(), params.to_string()];
    execute_python_subprocess(
        &app,
        "Analytics/functime_wrapper/functime_service.py",
        &args,
        Some("functime"),
    )
}
