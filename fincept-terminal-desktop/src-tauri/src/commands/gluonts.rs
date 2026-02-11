use crate::python;
use tauri::command;

/// Helper to execute a GluonTS operation via gluonts_service.py
fn execute_gluonts(
    app: &tauri::AppHandle,
    operation: &str,
    data: serde_json::Value,
) -> Result<String, String> {
    let args = vec![operation.to_string(), data.to_string()];
    python::execute_sync(app, "Analytics/gluonts_wrapper/gluonts_service.py", args)
}

// ==================== DEEP LEARNING FORECASTERS ====================

#[command]
pub async fn gluonts_forecast_feedforward(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_feedforward", payload)
}

#[command]
pub async fn gluonts_forecast_deepar(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    freq: Option<String>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "freq": freq.unwrap_or_else(|| "D".to_string()),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_deepar", payload)
}

#[command]
pub async fn gluonts_forecast_tft(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    freq: Option<String>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "freq": freq.unwrap_or_else(|| "D".to_string()),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_tft", payload)
}

#[command]
pub async fn gluonts_forecast_wavenet(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    freq: Option<String>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "freq": freq.unwrap_or_else(|| "D".to_string()),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_wavenet", payload)
}

#[command]
pub async fn gluonts_forecast_dlinear(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_dlinear", payload)
}

#[command]
pub async fn gluonts_forecast_patchtst(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    freq: Option<String>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "freq": freq.unwrap_or_else(|| "D".to_string()),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_patchtst", payload)
}

#[command]
pub async fn gluonts_forecast_tide(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    freq: Option<String>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "freq": freq.unwrap_or_else(|| "D".to_string()),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_tide", payload)
}

#[command]
pub async fn gluonts_forecast_lagtst(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    freq: Option<String>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "freq": freq.unwrap_or_else(|| "D".to_string()),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_lagtst", payload)
}

#[command]
pub async fn gluonts_forecast_deepnpts(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    freq: Option<String>,
    epochs: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "freq": freq.unwrap_or_else(|| "D".to_string()),
        "epochs": epochs.unwrap_or(10)
    });
    execute_gluonts(&app, "forecast_deepnpts", payload)
}

// ==================== BASELINE PREDICTORS ====================

#[command]
pub async fn gluonts_predict_seasonal_naive(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    season_length: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "season_length": season_length.unwrap_or(7)
    });
    execute_gluonts(&app, "predict_seasonal_naive", payload)
}

#[command]
pub async fn gluonts_predict_mean(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10)
    });
    execute_gluonts(&app, "predict_mean", payload)
}

#[command]
pub async fn gluonts_predict_constant(
    app: tauri::AppHandle,
    data: Vec<f64>,
    prediction_length: Option<i32>,
    constant_value: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "data": data,
        "prediction_length": prediction_length.unwrap_or(10),
        "constant_value": constant_value.unwrap_or(0.0)
    });
    execute_gluonts(&app, "predict_constant", payload)
}

// ==================== EVALUATION ====================

#[command]
pub async fn gluonts_evaluate(
    app: tauri::AppHandle,
    train_data: Vec<f64>,
    test_data: Vec<f64>,
    prediction_length: Option<i32>,
    freq: Option<String>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "train_data": train_data,
        "test_data": test_data,
        "prediction_length": prediction_length.unwrap_or(10),
        "freq": freq.unwrap_or_else(|| "D".to_string())
    });
    execute_gluonts(&app, "evaluate", payload)
}
