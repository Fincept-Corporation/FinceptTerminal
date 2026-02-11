use crate::python;
use tauri::command;

/// Helper to execute a py_vollib operation via descriptive_service.py
fn execute_vollib(
    app: &tauri::AppHandle,
    operation: &str,
    data: serde_json::Value,
) -> Result<String, String> {
    let args = vec![operation.to_string(), data.to_string()];
    python::execute_sync(app, "Analytics/py_vollib_wrapper/vollib_service.py", args)
}

// ==================== BLACK MODEL (futures/forwards) ====================

#[command]
pub async fn vollib_black_price(
    app: tauri::AppHandle,
    s: f64, k: f64, t: f64, r: f64, sigma: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "S": s, "K": k, "t": t, "r": r, "sigma": sigma, "flag": flag });
    execute_vollib(&app, "black_price", data)
}

#[command]
pub async fn vollib_black_greeks(
    app: tauri::AppHandle,
    s: f64, k: f64, t: f64, r: f64, sigma: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "S": s, "K": k, "t": t, "r": r, "sigma": sigma, "flag": flag });
    execute_vollib(&app, "black_greeks", data)
}

#[command]
pub async fn vollib_black_iv(
    app: tauri::AppHandle,
    price: f64, s: f64, k: f64, t: f64, r: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "price": price, "S": s, "K": k, "t": t, "r": r, "flag": flag });
    execute_vollib(&app, "black_iv", data)
}

// ==================== BLACK-SCHOLES MODEL (equity, no dividends) ====================

#[command]
pub async fn vollib_bs_price(
    app: tauri::AppHandle,
    s: f64, k: f64, t: f64, r: f64, sigma: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "S": s, "K": k, "t": t, "r": r, "sigma": sigma, "flag": flag });
    execute_vollib(&app, "bs_price", data)
}

#[command]
pub async fn vollib_bs_greeks(
    app: tauri::AppHandle,
    s: f64, k: f64, t: f64, r: f64, sigma: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "S": s, "K": k, "t": t, "r": r, "sigma": sigma, "flag": flag });
    execute_vollib(&app, "bs_greeks", data)
}

#[command]
pub async fn vollib_bs_iv(
    app: tauri::AppHandle,
    price: f64, s: f64, k: f64, t: f64, r: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "price": price, "S": s, "K": k, "t": t, "r": r, "flag": flag });
    execute_vollib(&app, "bs_iv", data)
}

// ==================== BLACK-SCHOLES-MERTON MODEL (equity with dividends) ====================

#[command]
pub async fn vollib_bsm_price(
    app: tauri::AppHandle,
    s: f64, k: f64, t: f64, r: f64, sigma: f64, q: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "S": s, "K": k, "t": t, "r": r, "sigma": sigma, "q": q, "flag": flag });
    execute_vollib(&app, "bsm_price", data)
}

#[command]
pub async fn vollib_bsm_greeks(
    app: tauri::AppHandle,
    s: f64, k: f64, t: f64, r: f64, sigma: f64, q: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "S": s, "K": k, "t": t, "r": r, "sigma": sigma, "q": q, "flag": flag });
    execute_vollib(&app, "bsm_greeks", data)
}

#[command]
pub async fn vollib_bsm_iv(
    app: tauri::AppHandle,
    price: f64, s: f64, k: f64, t: f64, r: f64, q: f64, flag: String,
) -> Result<String, String> {
    let data = serde_json::json!({ "price": price, "S": s, "K": k, "t": t, "r": r, "q": q, "flag": flag });
    execute_vollib(&app, "bsm_iv", data)
}
