// commands/brokers/dhan/market_data.rs - Market data and master contract commands

use serde_json::{json, Value};
use std::collections::HashMap;
use super::{ApiResponse, get_timestamp, dhan_request};

/// Get quotes for instruments
#[tauri::command]
pub async fn dhan_get_quotes(
    access_token: String,
    client_id: String,
    instrument_keys: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    // Group instruments by exchange segment
    let mut exchange_groups: HashMap<String, Vec<i64>> = HashMap::new();

    for key in &instrument_keys {
        let parts: Vec<&str> = key.split('|').collect();
        if parts.len() == 2 {
            let exchange = parts[0].to_string();
            if let Ok(security_id) = parts[1].parse::<i64>() {
                exchange_groups
                    .entry(exchange)
                    .or_insert_with(Vec::new)
                    .push(security_id);
            }
        }
    }

    let payload = serde_json::to_value(&exchange_groups).map_err(|e| e.to_string())?;

    match dhan_request(
        "/v2/marketfeed/quote",
        "POST",
        &access_token,
        &client_id,
        Some(payload),
    )
    .await
    {
        Ok(data) => Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get historical OHLCV data
#[tauri::command]
pub async fn dhan_get_history(
    access_token: String,
    client_id: String,
    security_id: String,
    exchange_segment: String,
    instrument: String,
    interval: String,
    from_date: String,
    to_date: String,
) -> Result<ApiResponse<Value>, String> {
    let endpoint = if interval == "D" || interval == "1D" {
        "/v2/charts/historical"
    } else {
        "/v2/charts/intraday"
    };

    let mut payload = json!({
        "securityId": security_id,
        "exchangeSegment": exchange_segment,
        "instrument": instrument,
        "fromDate": from_date,
        "toDate": to_date,
        "oi": true
    });

    // Add interval for intraday
    if endpoint == "/v2/charts/intraday" {
        payload["interval"] = json!(interval.replace("m", "").replace("h", "60"));
    }

    // Add expiryCode for equity
    if instrument == "EQUITY" {
        payload["expiryCode"] = json!(0);
    }

    match dhan_request(endpoint, "POST", &access_token, &client_id, Some(payload)).await {
        Ok(data) => Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get market depth
#[tauri::command]
pub async fn dhan_get_depth(
    access_token: String,
    client_id: String,
    security_id: String,
    exchange_segment: String,
) -> Result<ApiResponse<Value>, String> {
    let mut payload: HashMap<String, Vec<i64>> = HashMap::new();

    if let Ok(sec_id) = security_id.parse::<i64>() {
        payload.insert(exchange_segment.clone(), vec![sec_id]);
    }

    let payload_value = serde_json::to_value(&payload).map_err(|e| e.to_string())?;

    match dhan_request(
        "/v2/marketfeed/quote",
        "POST",
        &access_token,
        &client_id,
        Some(payload_value),
    )
    .await
    {
        Ok(data) => {
            // Extract depth from response
            let depth = data
                .get("data")
                .and_then(|d| d.get(&exchange_segment))
                .and_then(|e| e.get(&security_id))
                .cloned()
                .unwrap_or(json!({}));

            Ok(ApiResponse {
                success: true,
                data: Some(depth),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Download and store master contract
#[tauri::command]
pub async fn dhan_download_master_contract() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    // Use the new unified broker downloads system
    let result = crate::commands::broker_downloads::dhan::dhan_download_symbols().await;

    if result.success {
        Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "total_symbols": result.total_symbols,
                "message": result.message
            })),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(result.message),
            timestamp,
        })
    }
}

/// Search symbols in master contract
#[tauri::command]
pub async fn dhan_search_symbol(
    keyword: String,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    use crate::database::symbol_master;

    let search_limit = limit.unwrap_or(20);

    match symbol_master::search_symbols("dhan", &keyword, exchange.as_deref(), None, search_limit) {
        Ok(results) => {
            let symbols: Vec<Value> = results.iter().map(|r| {
                json!({
                    "symbol": r.symbol,
                    "name": r.name,
                    "exchange": r.exchange,
                    "br_symbol": r.br_symbol,
                    "token": r.token,
                    "lot_size": r.lot_size,
                    "tick_size": r.tick_size,
                    "instrument_type": r.instrument_type,
                    "strike": r.strike,
                    "expiry": r.expiry,
                })
            }).collect();

            Ok(ApiResponse {
                success: true,
                data: Some(symbols),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: Some(vec![]),
                error: Some(format!("Search failed: {}", e)),
                timestamp: get_timestamp(),
            })
        }
    }
}

/// Get security ID for a symbol
#[tauri::command]
pub async fn dhan_get_security_id(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<String>, String> {
    use crate::database::symbol_master;

    match symbol_master::get_token_by_symbol("dhan", &symbol, &exchange) {
        Ok(Some(token)) => Ok(ApiResponse {
            success: true,
            data: Some(token),
            error: None,
            timestamp: get_timestamp(),
        }),
        Ok(None) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Symbol {} not found on {}", symbol, exchange)),
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Lookup failed: {}", e)),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get master contract metadata
#[tauri::command]
pub async fn dhan_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    use crate::database::symbol_master;

    match symbol_master::get_status("dhan") {
        Ok(info) => {
            let now = chrono::Utc::now().timestamp();
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "last_updated": info.last_updated,
                    "symbol_count": info.total_symbols,
                    "age_seconds": now - info.last_updated,
                    "status": info.status,
                    "is_ready": info.is_ready,
                })),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to get metadata: {}", e)),
            timestamp: get_timestamp(),
        }),
    }
}
