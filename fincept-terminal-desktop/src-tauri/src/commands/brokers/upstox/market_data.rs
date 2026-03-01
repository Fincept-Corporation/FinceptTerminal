// commands/brokers/upstox/market_data.rs - Market data and master contract commands

use serde_json::{json, Value};
use super::super::common::ApiResponse;
use super::{UPSTOX_API_BASE_V2, UPSTOX_API_BASE_V3, create_upstox_headers, url_encode,
            search_upstox_symbols, get_upstox_instrument_key, get_upstox_metadata};

/// Get quotes for symbols
#[tauri::command]
pub async fn upstox_get_quotes(
    access_token: String,
    instrument_keys: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[upstox_get_quotes] Fetching quotes for {} symbols", instrument_keys.len());

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    // URL encode instrument keys
    let encoded_keys: Vec<String> = instrument_keys.iter()
        .map(|k| url_encode(k))
        .collect();
    let keys_param = encoded_keys.join(",");

    let response = client
        .get(format!("{}/market-quote/ohlc?instrument_key={}&interval=1d", UPSTOX_API_BASE_V3, keys_param))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        Ok(ApiResponse {
            success: true,
            data: body.get("data").cloned(),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch quotes".to_string()),
            timestamp,
        })
    }
}

/// Get historical OHLCV data
#[tauri::command]
pub async fn upstox_get_history(
    access_token: String,
    instrument_key: String,
    interval: String, // 1m, 5m, 15m, 30m, 60m, 1h, D, W, M
    from_date: String, // YYYY-MM-DD
    to_date: String,   // YYYY-MM-DD
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[upstox_get_history] Fetching history for {} from {} to {}", instrument_key, from_date, to_date);

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    // Map interval to Upstox v3 format
    let (unit, interval_val) = match interval.as_str() {
        "1m" => ("minute", "1"),
        "5m" => ("minute", "5"),
        "15m" => ("minute", "15"),
        "30m" => ("minute", "30"),
        "60m" | "1h" => ("minute", "60"),
        "D" | "1D" => ("day", "1"),
        "W" | "1W" => ("week", "1"),
        "M" | "1M" => ("month", "1"),
        _ => ("day", "1"),
    };

    let encoded_key = url_encode(&instrument_key);
    let url = format!(
        "{}/historical-candle/{}/{}/{}?from_date={}&to_date={}",
        UPSTOX_API_BASE_V3, encoded_key, interval_val, unit, from_date, to_date
    );

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let candles = body.get("data")
            .and_then(|d| d.get("candles"))
            .and_then(|c| c.as_array())
            .cloned()
            .unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(candles),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch historical data".to_string()),
            timestamp,
        })
    }
}

/// Get market depth
#[tauri::command]
pub async fn upstox_get_depth(
    access_token: String,
    instrument_key: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[upstox_get_depth] Fetching depth for {}", instrument_key);

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let encoded_key = url_encode(&instrument_key);

    let response = client
        .get(format!("{}/market-quote/quotes?instrument_key={}", UPSTOX_API_BASE_V2, encoded_key))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        Ok(ApiResponse {
            success: true,
            data: body.get("data").cloned(),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch market depth".to_string()),
            timestamp,
        })
    }
}

/// Download Upstox master contract
#[tauri::command]
pub async fn upstox_download_master_contract() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    // Use the new unified broker downloads system
    let result = crate::commands::broker_downloads::upstox::upstox_download_symbols().await;

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
pub async fn upstox_search_symbol(
    keyword: String,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let search_limit = limit.unwrap_or(20);

    match search_upstox_symbols(&keyword, exchange.as_deref(), search_limit) {
        Ok(results) => {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "results": results,
                    "count": results.len()
                })),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            })
        }
    }
}

/// Get instrument key for symbol
#[tauri::command]
pub async fn upstox_get_instrument_key(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<String>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match get_upstox_instrument_key(&symbol, &exchange) {
        Ok(Some(key)) => {
            Ok(ApiResponse {
                success: true,
                data: Some(key),
                error: None,
                timestamp,
            })
        }
        Ok(None) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Symbol {}:{} not found", exchange, symbol)),
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            })
        }
    }
}

/// Get master contract metadata
#[tauri::command]
pub async fn upstox_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match get_upstox_metadata() {
        Ok(Some((last_updated, count))) => {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "last_updated": last_updated,
                    "symbol_count": count,
                    "age_seconds": timestamp - last_updated
                })),
                error: None,
                timestamp,
            })
        }
        Ok(None) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some("Master contract not downloaded yet".to_string()),
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            })
        }
    }
}
