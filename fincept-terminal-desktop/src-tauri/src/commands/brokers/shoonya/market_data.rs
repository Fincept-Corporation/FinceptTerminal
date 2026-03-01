// Shoonya Market Data Commands

use super::{SHOONYA_API_BASE, create_shoonya_headers, create_payload_with_auth};
use super::super::common::ApiResponse;
use serde_json::json;
use chrono::{NaiveDate, NaiveDateTime};
use crate::database::shoonya_symbols;

/// Get quotes for a symbol
#[tauri::command]
pub async fn shoonya_get_quotes(
    access_token: String,
    user_id: String,
    exchange: String,
    token: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    // Normalize index exchanges
    let api_exchange = match exchange.as_str() {
        "NSE_INDEX" => "NSE",
        "BSE_INDEX" => "BSE",
        _ => &exchange,
    };

    let payload = json!({
        "uid": user_id,
        "exch": api_exchange,
        "token": token
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/GetQuotes", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        // Transform to unified quote format
        let quote_data = json!({
            "bid": body.get("bp1").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "ask": body.get("sp1").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "open": body.get("o").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "high": body.get("h").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "low": body.get("l").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "ltp": body.get("lp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "prev_close": body.get("c").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "volume": body.get("v").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0),
            "oi": body.get("oi").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0),
            "raw": body
        });

        Ok(ApiResponse { success: true, data: Some(quote_data), error: None, timestamp })
    } else {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quotes")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get market depth (5-level bid/ask)
#[tauri::command]
pub async fn shoonya_get_depth(
    access_token: String,
    user_id: String,
    exchange: String,
    token: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let api_exchange = match exchange.as_str() {
        "NSE_INDEX" => "NSE",
        "BSE_INDEX" => "BSE",
        _ => &exchange,
    };

    let payload = json!({
        "uid": user_id,
        "exch": api_exchange,
        "token": token
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/GetQuotes", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        // Build depth arrays from bp1-bp5, bq1-bq5, sp1-sp5, sq1-sq5
        let mut bids = Vec::new();
        let mut asks = Vec::new();

        for i in 1..=5 {
            let bp_key = format!("bp{}", i);
            let bq_key = format!("bq{}", i);
            let sp_key = format!("sp{}", i);
            let sq_key = format!("sq{}", i);

            bids.push(json!({
                "price": body.get(&bp_key).and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
                "quantity": body.get(&bq_key).and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0)
            }));

            asks.push(json!({
                "price": body.get(&sp_key).and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
                "quantity": body.get(&sq_key).and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0)
            }));
        }

        let total_buy_qty: i64 = bids.iter().map(|b| b.get("quantity").and_then(|v| v.as_i64()).unwrap_or(0)).sum();
        let total_sell_qty: i64 = asks.iter().map(|a| a.get("quantity").and_then(|v| v.as_i64()).unwrap_or(0)).sum();

        let depth_data = json!({
            "bids": bids,
            "asks": asks,
            "totalbuyqty": total_buy_qty,
            "totalsellqty": total_sell_qty,
            "high": body.get("h").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "low": body.get("l").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "ltp": body.get("lp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "ltq": body.get("ltq").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0),
            "open": body.get("o").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "prev_close": body.get("c").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "volume": body.get("v").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0)
        });

        Ok(ApiResponse { success: true, data: Some(depth_data), error: None, timestamp })
    } else {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch market depth")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get historical OHLCV data
#[tauri::command]
pub async fn shoonya_get_history(
    access_token: String,
    user_id: String,
    exchange: String,
    token: String,
    symbol: String,
    resolution: String,  // 1, 3, 5, 10, 15, 30, 60, 120, 240, D
    from_date: String,   // YYYY-MM-DD
    to_date: String,     // YYYY-MM-DD
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let api_exchange = match exchange.as_str() {
        "NSE_INDEX" => "NSE",
        "BSE_INDEX" => "BSE",
        _ => &exchange,
    };

    // Parse dates to Unix timestamps
    let start_ts = NaiveDate::parse_from_str(&from_date, "%Y-%m-%d")
        .map_err(|e| format!("Invalid from_date: {}", e))?
        .and_hms_opt(0, 0, 0)
        .ok_or("Invalid time")?
        .and_utc()
        .timestamp();

    let end_ts = NaiveDate::parse_from_str(&to_date, "%Y-%m-%d")
        .map_err(|e| format!("Invalid to_date: {}", e))?
        .and_hms_opt(23, 59, 59)
        .ok_or("Invalid time")?
        .and_utc()
        .timestamp();

    // Choose endpoint based on resolution
    let (endpoint, payload) = if resolution == "D" {
        // Daily data uses EODChartData
        let sym = format!("{}:{}", api_exchange, symbol);
        (
            format!("{}/NorenWClientTP/EODChartData", SHOONYA_API_BASE),
            json!({
                "uid": user_id,
                "sym": sym,
                "from": start_ts.to_string(),
                "to": end_ts.to_string()
            })
        )
    } else {
        // Intraday uses TPSeries
        (
            format!("{}/NorenWClientTP/TPSeries", SHOONYA_API_BASE),
            json!({
                "uid": user_id,
                "exch": api_exchange,
                "token": token,
                "st": start_ts.to_string(),
                "et": end_ts.to_string(),
                "intrv": resolution
            })
        )
    };

    let payload_str = create_payload_with_auth(&payload, &access_token);

    eprintln!("[shoonya_get_history] Endpoint: {}, Payload: {}", endpoint, payload);

    let response = client
        .post(&endpoint)
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.is_array() {
        // Transform candle data to unified format
        let candles: Vec<serde_json::Value> = body.as_array()
            .unwrap_or(&vec![])
            .iter()
            .filter_map(|candle| {
                // Skip zero candles
                let into = candle.get("into").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
                let inth = candle.get("inth").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
                let intl = candle.get("intl").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
                let intc = candle.get("intc").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);

                if into == 0.0 && inth == 0.0 && intl == 0.0 && intc == 0.0 {
                    return None;
                }

                // Parse timestamp
                let ts = if resolution == "D" {
                    // EOD format: ssboe
                    candle.get("ssboe").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0)
                } else {
                    // Intraday format: time as "DD-MM-YYYY HH:MM:SS"
                    candle.get("time").and_then(|v| v.as_str()).and_then(|time_str| {
                        NaiveDateTime::parse_from_str(time_str, "%d-%m-%Y %H:%M:%S")
                            .ok()
                            .map(|dt| dt.and_utc().timestamp())
                    }).unwrap_or(0)
                };

                let intv = candle.get("intv").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
                let oi = candle.get("oi").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);

                Some(json!([ts, into, inth, intl, intc, intv, oi]))
            })
            .collect();

        Ok(ApiResponse { success: true, data: Some(json!(candles)), error: None, timestamp })
    } else {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch historical data")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Download and cache Shoonya master contract
#[tauri::command]
pub async fn shoonya_download_master_contract() -> Result<ApiResponse<serde_json::Value>, String> {
    eprintln!("[shoonya_download_master_contract] Downloading master contract via unified system");

    let timestamp = chrono::Utc::now().timestamp();

    // Use the new unified broker downloads system
    let result = crate::commands::broker_downloads::shoonya::shoonya_download_symbols().await;

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
        Ok(ApiResponse { success: false, data: None, error: Some(result.message), timestamp })
    }
}

/// Search for symbols in master contract database
#[tauri::command]
pub async fn shoonya_search_symbol(
    keyword: String,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<ApiResponse<serde_json::Value>, String> {
    eprintln!("[shoonya_search_symbol] Searching for: {} on exchange: {:?}", keyword, exchange);

    let timestamp = chrono::Utc::now().timestamp();
    let search_limit = limit.unwrap_or(20);

    match shoonya_symbols::search_symbols(&keyword, exchange.as_deref(), search_limit) {
        Ok(results) => {
            eprintln!("[shoonya_search_symbol] Found {} results", results.len());
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
            eprintln!("[shoonya_search_symbol] Error: {}", e);
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Search failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Get token for symbol (required for data subscription)
#[tauri::command]
pub async fn shoonya_get_token_for_symbol(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<String>, String> {
    eprintln!("[shoonya_get_token_for_symbol] Getting token for: {}:{}", exchange, symbol);

    let timestamp = chrono::Utc::now().timestamp();

    match shoonya_symbols::get_token_by_symbol(&symbol, &exchange) {
        Ok(Some(token)) => {
            eprintln!("[shoonya_get_token_for_symbol] Found token: {}", token);
            Ok(ApiResponse { success: true, data: Some(token), error: None, timestamp })
        }
        Ok(None) => {
            eprintln!("[shoonya_get_token_for_symbol] Symbol not found");
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Symbol {}:{} not found in master contract", exchange, symbol)),
                timestamp,
            })
        }
        Err(e) => {
            eprintln!("[shoonya_get_token_for_symbol] Error: {}", e);
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Lookup failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Get symbol info by token
#[tauri::command]
pub async fn shoonya_get_symbol_by_token(
    token: String,
    exchange: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    eprintln!("[shoonya_get_symbol_by_token] Getting symbol for token: {} on {}", token, exchange);

    let timestamp = chrono::Utc::now().timestamp();

    match shoonya_symbols::get_symbol_by_token(&token, &exchange) {
        Ok(Some(symbol_info)) => {
            Ok(ApiResponse { success: true, data: Some(json!(symbol_info)), error: None, timestamp })
        }
        Ok(None) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Token {} not found on {}", token, exchange)),
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Lookup failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Get master contract metadata (last updated, count)
#[tauri::command]
pub async fn shoonya_get_master_contract_metadata() -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match shoonya_symbols::get_metadata() {
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
                error: Some(format!("Failed to get metadata: {}", e)),
                timestamp,
            })
        }
    }
}
