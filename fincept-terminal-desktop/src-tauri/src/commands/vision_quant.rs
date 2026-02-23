// VisionQuant Pattern Intelligence Commands
// AI K-line pattern recognition using AttentionCAE + FAISS + DTW
use crate::python;

/// Merge an optional JSON config string into a serde_json::Value object.
/// This lets the frontend pass advanced settings without bloating every function signature.
fn merge_config(base: &mut serde_json::Value, config: Option<String>) {
    if let Some(cfg_str) = config {
        if let Ok(cfg_val) = serde_json::from_str::<serde_json::Value>(&cfg_str) {
            if let Some(obj) = cfg_val.as_object() {
                if let Some(base_obj) = base.as_object_mut() {
                    for (k, v) in obj {
                        base_obj.insert(k.clone(), v.clone());
                    }
                }
            }
        }
    }
}

/// Search for similar K-line patterns using the VisionQuant engine
#[tauri::command]
pub async fn vision_quant_search_patterns(
    app: tauri::AppHandle,
    symbol: String,
    date: Option<String>,
    top_k: Option<u32>,
    lookback: Option<u32>,
    config: Option<String>,
) -> Result<String, String> {
    let mut params = serde_json::json!({
        "symbol": symbol,
        "date": date,
        "top_k": top_k.unwrap_or(10),
        "lookback": lookback.unwrap_or(60)
    });
    merge_config(&mut params, config);

    let args = vec!["search".to_string(), params.to_string()];
    python::execute_sync(&app, "vision_quant/engine.py", args)
}

/// Get multi-factor scorecard (Vision + Fundamental + Technical)
#[tauri::command]
pub async fn vision_quant_score(
    app: tauri::AppHandle,
    symbol: String,
    win_rate: f64,
    date: Option<String>,
    config: Option<String>,
) -> Result<String, String> {
    let mut params = serde_json::json!({
        "symbol": symbol,
        "win_rate": win_rate,
        "date": date
    });
    merge_config(&mut params, config);

    let args = vec!["score".to_string(), params.to_string()];
    python::execute_sync(&app, "vision_quant/scorer.py", args)
}

/// Run adaptive strategy backtest
#[tauri::command]
pub async fn vision_quant_backtest(
    app: tauri::AppHandle,
    symbol: String,
    start_date: String,
    end_date: String,
    initial_capital: Option<f64>,
    config: Option<String>,
) -> Result<String, String> {
    let mut params = serde_json::json!({
        "symbol": symbol,
        "start": start_date,
        "end": end_date,
        "capital": initial_capital.unwrap_or(100000.0)
    });
    merge_config(&mut params, config);

    let args = vec!["backtest".to_string(), params.to_string()];
    python::execute_sync(&app, "vision_quant/backtester.py", args)
}

/// Build FAISS index (first-run setup)
#[tauri::command]
pub async fn vision_quant_setup_index(
    app: tauri::AppHandle,
    symbols: Option<Vec<String>>,
    start_date: Option<String>,
    stride: Option<u32>,
    epochs: Option<u32>,
    config: Option<String>,
) -> Result<String, String> {
    let mut params = serde_json::json!({
        "symbols": symbols,
        "start": start_date.unwrap_or_else(|| "2020-01-01".to_string()),
        "stride": stride.unwrap_or(5),
        "epochs": epochs.unwrap_or(30)
    });
    merge_config(&mut params, config);

    let args = vec!["build".to_string(), params.to_string()];
    python::execute_sync(&app, "vision_quant/setup_index.py", args)
}

/// Check VisionQuant index status (model, index, metadata)
#[tauri::command]
pub async fn vision_quant_status(
    app: tauri::AppHandle,
) -> Result<String, String> {
    let args = vec!["status".to_string()];
    python::execute_sync(&app, "vision_quant/engine.py", args)
}
