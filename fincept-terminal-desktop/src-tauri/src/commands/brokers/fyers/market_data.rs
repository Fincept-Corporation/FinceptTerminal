// Fyers Market Data & Master Contract Commands

use super::{FYERS_API_BASE, create_fyers_headers, validate_credentials};
use super::super::common::ApiResponse;
use crate::database::symbol_master;
use serde_json::{json, Value};

/// Get quotes for symbols
#[tauri::command]
pub async fn fyers_get_quotes(
    api_key: String,
    access_token: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    if let Err(e) = validate_credentials(&api_key, &access_token) {
        eprintln!("[fyers_get_quotes] Credential validation failed: {}", e);
        return Ok(ApiResponse { success: false, data: None, error: Some(e), timestamp: chrono::Utc::now().timestamp() });
    }

    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let symbols_param = symbols.join(",");
    let url = format!("{}/data/quotes?symbols={}", FYERS_API_BASE, symbols_param);
    eprintln!("[fyers_get_quotes] Requesting: {}", url);

    let response = client.get(&url).headers(headers).send().await.map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

    eprintln!("[fyers_get_quotes] Response status: {}", status);
    eprintln!("[fyers_get_quotes] Response body: {}", text);

    let body: Value = serde_json::from_str(&text).map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        let data_array = body.get("d").and_then(|d| d.as_array());
        if let Some(arr) = data_array {
            let quotes: Vec<Value> = arr.iter().filter_map(|item| {
                if item.get("s").and_then(|s| s.as_str()) == Some("ok") {
                    item.get("v").cloned()
                } else { None }
            }).collect();
            eprintln!("[fyers_get_quotes] Extracted {} quotes", quotes.len());
            Ok(ApiResponse { success: true, data: Some(json!(quotes)), error: None, timestamp })
        } else {
            Ok(ApiResponse { success: true, data: body.get("d").cloned(), error: None, timestamp })
        }
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to fetch quotes").to_string();
        eprintln!("[fyers_get_quotes] Error: {}", error_msg);
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get historical OHLCV data
#[tauri::command]
pub async fn fyers_get_history(
    api_key: String,
    access_token: String,
    symbol: String,
    resolution: String,
    from_date: String,
    to_date: String,
) -> Result<ApiResponse<Value>, String> {
    if let Err(e) = validate_credentials(&api_key, &access_token) {
        eprintln!("[fyers_get_history] Credential validation failed: {}", e);
        return Ok(ApiResponse { success: false, data: None, error: Some(e), timestamp: chrono::Utc::now().timestamp() });
    }

    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let url = format!("{}/data/history", FYERS_API_BASE);
    let now = chrono::Utc::now();
    eprintln!("[fyers_get_history] Current system time: {}", now);

    let from_timestamp = chrono::NaiveDate::parse_from_str(&from_date, "%Y-%m-%d")
        .map_err(|e| format!("Invalid from_date format: {}", e))?
        .and_hms_opt(0, 0, 0).ok_or("Invalid time")?.and_utc().timestamp();

    let to_timestamp = chrono::NaiveDate::parse_from_str(&to_date, "%Y-%m-%d")
        .map_err(|e| format!("Invalid to_date format: {}", e))?
        .and_hms_opt(23, 59, 59).ok_or("Invalid time")?.and_utc().timestamp();

    let params = json!({
        "symbol": symbol,
        "resolution": resolution,
        "date_format": "0",
        "range_from": from_timestamp.to_string(),
        "range_to": to_timestamp.to_string(),
        "cont_flag": "1"
    });

    eprintln!("[fyers_get_history] Symbol: {}, Resolution: {}, Date range: {} ({}) to {} ({})",
        symbol, resolution, from_date, from_timestamp, to_date, to_timestamp);

    let response = client.get(&url).headers(headers).query(&params).send().await
        .map_err(|e| { eprintln!("[fyers_get_history] Request failed: {}", e); format!("Request failed: {}", e) })?;

    let status = response.status();
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

    eprintln!("[fyers_get_history] Response status: {}, body length: {}", status, text.len());

    if text.is_empty() {
        return Ok(ApiResponse {
            success: false, data: None,
            error: Some("Empty response from Fyers API. Check your API credentials and symbol format.".to_string()),
            timestamp,
        });
    }

    let body: Value = serde_json::from_str(&text)
        .map_err(|e| format!("Failed to parse response: {}. Response: {}", e, text))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(body.get("candles").unwrap_or(&json!([])).clone()),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to fetch historical data").to_string();
        eprintln!("[fyers_get_history] Error: {}", error_msg);
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get market depth (orderbook) for a symbol
#[tauri::command]
pub async fn fyers_get_depth(
    api_key: String,
    access_token: String,
    symbol: String,
) -> Result<ApiResponse<Value>, String> {
    if let Err(e) = validate_credentials(&api_key, &access_token) {
        eprintln!("[fyers_get_depth] Credential validation failed: {}", e);
        return Ok(ApiResponse { success: false, data: None, error: Some(e), timestamp: chrono::Utc::now().timestamp() });
    }

    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let url = format!("{}/data/depth?symbol={}&ohlcv_flag=1", FYERS_API_BASE, symbol);
    eprintln!("[fyers_get_depth] Requesting: {}", url);

    let response = client.get(&url).headers(headers).send().await.map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

    eprintln!("[fyers_get_depth] Response status: {}, body: {}", status, text);

    let body: Value = serde_json::from_str(&text).map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        let depth_data = body.get("d").and_then(|d| d.get(&symbol)).cloned()
            .or_else(|| body.get("d").cloned());
        Ok(ApiResponse { success: true, data: depth_data, error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to fetch market depth").to_string();
        eprintln!("[fyers_get_depth] Error: {}", error_msg);
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Calculate margin required for orders
#[tauri::command]
pub async fn fyers_calculate_margin(
    api_key: String,
    access_token: String,
    orders: Value,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let url = format!("{}/api/v3/multiorder/margin", FYERS_API_BASE);
    eprintln!("[fyers_calculate_margin] Requesting: {}, Orders: {}", url, orders);

    let response = client.post(&url).headers(headers).json(&orders).send().await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;
    eprintln!("[fyers_calculate_margin] Response status: {}, body: {}", status, text);

    let body: Value = serde_json::from_str(&text).map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse { success: true, data: body.get("data").cloned(), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to calculate margin").to_string();
        eprintln!("[fyers_calculate_margin] Error: {}", error_msg);
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Download and cache Fyers master contract
#[tauri::command]
pub async fn fyers_download_master_contract(
    _api_key: String,
    _access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[fyers_download_master_contract] Downloading master contract via unified system");

    let timestamp = chrono::Utc::now().timestamp();
    let result = crate::commands::broker_downloads::fyers::fyers_download_symbols().await;

    if result.success {
        Ok(ApiResponse {
            success: true,
            data: Some(json!({ "symbol_count": result.total_symbols, "message": result.message })),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(result.message), timestamp })
    }
}

/// Search for symbols in master contract
#[tauri::command]
pub async fn fyers_search_symbol(
    keyword: String,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[fyers_search_symbol] Searching for: {} on exchange: {:?}", keyword, exchange);

    let timestamp = chrono::Utc::now().timestamp();
    let search_limit = limit.unwrap_or(20);

    match symbol_master::search_symbols("fyers", &keyword, exchange.as_deref(), None, search_limit) {
        Ok(results) => {
            eprintln!("[fyers_search_symbol] Found {} results", results.len());
            let json_results: Vec<Value> = results.iter().map(|r| json!({
                "symbol": r.symbol, "br_symbol": r.br_symbol, "name": r.name,
                "exchange": r.exchange, "token": r.token, "instrument_type": r.instrument_type,
                "lot_size": r.lot_size, "tick_size": r.tick_size, "expiry": r.expiry, "strike": r.strike,
            })).collect();
            Ok(ApiResponse {
                success: true,
                data: Some(json!({ "results": json_results, "count": results.len() })),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            eprintln!("[fyers_search_symbol] Error: {}", e);
            Ok(ApiResponse { success: false, data: None, error: Some(format!("Search failed: {}", e)), timestamp })
        }
    }
}

/// Get token for symbol (required for WebSocket subscription)
#[tauri::command]
pub async fn fyers_get_token_for_symbol(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<i32>, String> {
    eprintln!("[fyers_get_token_for_symbol] Getting token for: {}:{}", exchange, symbol);

    let timestamp = chrono::Utc::now().timestamp();

    match symbol_master::get_token_by_symbol("fyers", &symbol, &exchange) {
        Ok(Some(token_str)) => {
            let token: i32 = token_str.parse().unwrap_or(0);
            eprintln!("[fyers_get_token_for_symbol] Found token: {}", token);
            Ok(ApiResponse { success: true, data: Some(token), error: None, timestamp })
        }
        Ok(None) => Ok(ApiResponse {
            success: false, data: None,
            error: Some(format!("Symbol {}:{} not found in master contract", exchange, symbol)),
            timestamp,
        }),
        Err(e) => Ok(ApiResponse { success: false, data: None, error: Some(format!("Lookup failed: {}", e)), timestamp }),
    }
}

/// Get symbol info by token
#[tauri::command]
pub async fn fyers_get_symbol_by_token(
    token: i32,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[fyers_get_symbol_by_token] Getting symbol for token: {}", token);

    let timestamp = chrono::Utc::now().timestamp();

    match symbol_master::get_symbol_by_token("fyers", &token.to_string()) {
        Ok(Some(r)) => Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "symbol": r.symbol, "br_symbol": r.br_symbol, "name": r.name,
                "exchange": r.exchange, "token": r.token, "instrument_type": r.instrument_type,
                "lot_size": r.lot_size, "tick_size": r.tick_size, "expiry": r.expiry, "strike": r.strike,
            })),
            error: None,
            timestamp,
        }),
        Ok(None) => Ok(ApiResponse { success: false, data: None, error: Some(format!("Token {} not found", token)), timestamp }),
        Err(e) => Ok(ApiResponse { success: false, data: None, error: Some(format!("Lookup failed: {}", e)), timestamp }),
    }
}

/// Get master contract metadata (last updated, count)
#[tauri::command]
pub async fn fyers_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match symbol_master::get_status("fyers") {
        Ok(info) => Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "last_updated": info.last_updated, "symbol_count": info.total_symbols,
                "age_seconds": timestamp - info.last_updated, "status": info.status, "is_ready": info.is_ready,
            })),
            error: None,
            timestamp,
        }),
        Err(e) => Ok(ApiResponse { success: false, data: None, error: Some(format!("Failed to get metadata: {}", e)), timestamp }),
    }
}
