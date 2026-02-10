use crate::python;
use tauri::command;

/// Helper to execute a PyPME operation via worker_handler.py
fn execute_pypme(
    app: &tauri::AppHandle,
    operation: &str,
    data: serde_json::Value,
) -> Result<String, String> {
    let args = vec![operation.to_string(), data.to_string()];
    python::execute_sync(app, "Analytics/pypme_wrapper/worker_handler.py", args)
}

// ==================== BASIC PME ====================

#[command]
pub async fn pypme_calculate(
    app: tauri::AppHandle,
    cashflows: Vec<f64>,
    prices: Vec<f64>,
    pme_prices: Vec<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "cashflows": cashflows,
        "prices": prices,
        "pme_prices": pme_prices
    });
    execute_pypme(&app, "pme", payload)
}

#[command]
pub async fn pypme_verbose(
    app: tauri::AppHandle,
    cashflows: Vec<f64>,
    prices: Vec<f64>,
    pme_prices: Vec<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "cashflows": cashflows,
        "prices": prices,
        "pme_prices": pme_prices
    });
    execute_pypme(&app, "verbose_pme", payload)
}

// ==================== EXTENDED PME (date-aware) ====================

#[command]
pub async fn pypme_xpme(
    app: tauri::AppHandle,
    dates: Vec<String>,
    cashflows: Vec<f64>,
    prices: Vec<f64>,
    pme_prices: Vec<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "dates": dates,
        "cashflows": cashflows,
        "prices": prices,
        "pme_prices": pme_prices
    });
    execute_pypme(&app, "xpme", payload)
}

#[command]
pub async fn pypme_verbose_xpme(
    app: tauri::AppHandle,
    dates: Vec<String>,
    cashflows: Vec<f64>,
    prices: Vec<f64>,
    pme_prices: Vec<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "dates": dates,
        "cashflows": cashflows,
        "prices": prices,
        "pme_prices": pme_prices
    });
    execute_pypme(&app, "verbose_xpme", payload)
}

// ==================== TESSA xPME (auto market data) ====================

#[command]
pub async fn pypme_tessa_xpme(
    app: tauri::AppHandle,
    dates: Vec<String>,
    cashflows: Vec<f64>,
    prices: Vec<f64>,
    pme_ticker: String,
    pme_source: Option<String>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "dates": dates,
        "cashflows": cashflows,
        "prices": prices,
        "pme_ticker": pme_ticker,
        "pme_source": pme_source.unwrap_or_else(|| "yahoo".to_string())
    });
    execute_pypme(&app, "tessa_xpme", payload)
}

#[command]
pub async fn pypme_tessa_verbose_xpme(
    app: tauri::AppHandle,
    dates: Vec<String>,
    cashflows: Vec<f64>,
    prices: Vec<f64>,
    pme_ticker: String,
    pme_source: Option<String>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "dates": dates,
        "cashflows": cashflows,
        "prices": prices,
        "pme_ticker": pme_ticker,
        "pme_source": pme_source.unwrap_or_else(|| "yahoo".to_string())
    });
    execute_pypme(&app, "tessa_verbose_xpme", payload)
}
