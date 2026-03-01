// AngelOne Symbol Lookup Commands (delegates to master contract DB)

use serde_json::{json, Value};
use super::super::common::ApiResponse;
use crate::database::symbol_master;

/// Search AngelOne symbols from local master contract DB
#[tauri::command]
pub async fn angelone_search_symbols(
    _api_key: String,
    _access_token: String,
    query: String,
    exchange: Option<String>,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match symbol_master::search_symbols(
        "angelone",
        &query,
        exchange.as_deref(),
        None,
        50,
    ) {
        Ok(results) => {
            let data: Vec<Value> = results.iter().map(|r| {
                json!({
                    "symbol": r.symbol,
                    "tradingsymbol": r.br_symbol,
                    "name": r.name,
                    "exchange": r.exchange,
                    "symboltoken": r.token,
                    "token": r.token,
                    "instrumenttype": r.instrument_type,
                    "lotsize": r.lot_size,
                    "ticksize": r.tick_size,
                    "expiry": r.expiry,
                    "strike": r.strike,
                })
            }).collect();

            ApiResponse {
                success: true,
                data: Some(json!(data)),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Search failed: {}", e)),
            timestamp,
        },
    }
}

/// Get a specific instrument from local master contract DB
#[tauri::command]
pub async fn angelone_get_instrument(
    _api_key: String,
    symbol: String,
    exchange: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match symbol_master::get_symbol("angelone", &symbol, &exchange) {
        Ok(Some(result)) => {
            ApiResponse {
                success: true,
                data: Some(json!({
                    "symbol": result.symbol,
                    "tradingsymbol": result.br_symbol,
                    "name": result.name,
                    "exchange": result.exchange,
                    "symboltoken": result.token,
                    "token": result.token,
                    "instrumenttype": result.instrument_type,
                    "lotsize": result.lot_size,
                    "ticksize": result.tick_size,
                    "expiry": result.expiry,
                    "strike": result.strike,
                })),
                error: None,
                timestamp,
            }
        }
        Ok(None) => {
            // Try fuzzy search as fallback
            match symbol_master::search_symbols("angelone", &symbol, Some(&exchange), None, 1) {
                Ok(results) if !results.is_empty() => {
                    let r = &results[0];
                    ApiResponse {
                        success: true,
                        data: Some(json!({
                            "symbol": r.symbol,
                            "tradingsymbol": r.br_symbol,
                            "name": r.name,
                            "exchange": r.exchange,
                            "symboltoken": r.token,
                            "token": r.token,
                            "instrumenttype": r.instrument_type,
                            "lotsize": r.lot_size,
                            "ticksize": r.tick_size,
                            "expiry": r.expiry,
                            "strike": r.strike,
                        })),
                        error: None,
                        timestamp,
                    }
                }
                _ => ApiResponse {
                    success: false,
                    data: None,
                    error: Some(format!("Instrument not found: {} on {}", symbol, exchange)),
                    timestamp,
                },
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Lookup failed: {}", e)),
            timestamp,
        },
    }
}

/// Download master contract (wrapper that matches the TS adapter's expected command name)
#[tauri::command]
pub async fn angelone_download_master_contract(
    _api_key: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Delegate to the new symbol master download
    let result = crate::commands::broker_downloads::angelone::angelone_download_symbols().await;
    if result.success {
        ApiResponse {
            success: true,
            data: Some(json!({
                "symbol_count": result.total_symbols,
                "message": result.message,
            })),
            error: None,
            timestamp,
        }
    } else {
        ApiResponse {
            success: false,
            data: None,
            error: Some(result.message),
            timestamp,
        }
    }
}

/// Get master contract info
#[tauri::command]
pub async fn angelone_master_contract_info() -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match symbol_master::get_status("angelone") {
        Ok(info) => {
            let now = chrono::Utc::now().timestamp();
            let cache_age = now - info.last_updated;
            ApiResponse {
                success: true,
                data: Some(json!({
                    "timestamp": info.last_updated,
                    "symbol_count": info.total_symbols,
                    "cache_age_seconds": cache_age,
                    "status": info.status,
                    "is_ready": info.is_ready,
                })),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to get info: {}", e)),
            timestamp,
        },
    }
}
