// Motilal Oswal Market Data & Symbol Commands

use reqwest::Client;
use serde_json::{json, Value};
use tauri::command;
use super::{ApiResponse, get_timestamp, get_motilal_headers, motilal_request};

/// Get LTP/quotes from Motilal Oswal
#[command]
pub async fn motilal_get_quotes(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    exchange: String,
    symbol_token: i64,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    let payload = json!({
        "exchange": exchange,
        "scripcode": symbol_token
    });

    match motilal_request(&client, "POST", "/rest/report/v1/getltpdata", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let quote_data = data.get("data").cloned().unwrap_or(json!({}));

                // Motilal returns values in paisa, convert to rupees
                let convert = |v: Option<&Value>| -> f64 {
                    v.and_then(|x| x.as_f64()).unwrap_or(0.0) / 100.0
                };

                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "bid": convert(quote_data.get("bid")),
                        "ask": convert(quote_data.get("ask")),
                        "open": convert(quote_data.get("open")),
                        "high": convert(quote_data.get("high")),
                        "low": convert(quote_data.get("low")),
                        "ltp": convert(quote_data.get("ltp")),
                        "prev_close": convert(quote_data.get("close")),
                        "volume": quote_data.get("volume").and_then(|v| v.as_i64()).unwrap_or(0),
                        "oi": 0,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Failed to fetch quotes")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Get historical data - Note: Motilal doesn't support historical data with date ranges
#[command]
pub async fn motilal_get_history(
    _auth_token: String,
    _api_key: String,
    _vendor_code: String,
    _exchange: String,
    _symbol_token: i64,
    _interval: String,
    _from_date: String,
    _to_date: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();

    // Motilal Oswal does not support historical data API
    Ok(ApiResponse {
        success: false,
        data: Some(json!({ "candles": [] })),
        error: Some("Historical data not supported by Motilal Oswal API".to_string()),
        timestamp,
    })
}

/// Get market depth - Note: Requires WebSocket for real-time depth
#[command]
pub async fn motilal_get_depth(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    exchange: String,
    symbol_token: i64,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();

    // Use LTP API as a fallback - full depth requires WebSocket
    let quotes_result = motilal_get_quotes(
        auth_token,
        api_key,
        vendor_code,
        exchange,
        symbol_token,
    ).await?;

    if quotes_result.success {
        if let Some(quote_data) = quotes_result.data {
            let bid = quote_data.get("bid").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let ask = quote_data.get("ask").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let ltp = quote_data.get("ltp").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let high = quote_data.get("high").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let low = quote_data.get("low").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let open = quote_data.get("open").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let prev_close = quote_data.get("prev_close").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let volume = quote_data.get("volume").and_then(|v| v.as_i64()).unwrap_or(0);

            return Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "bids": [{"price": bid, "quantity": 0}],
                    "asks": [{"price": ask, "quantity": 0}],
                    "high": high,
                    "low": low,
                    "ltp": ltp,
                    "ltq": 0,
                    "open": open,
                    "prev_close": prev_close,
                    "volume": volume,
                    "oi": 0,
                    "totalbuyqty": 0,
                    "totalsellqty": 0
                })),
                error: None,
                timestamp,
            });
        }
    }

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Failed to fetch market depth".to_string()),
        timestamp,
    })
}

/// Download master contract - Motilal uses contract download endpoint
#[command]
pub async fn motilal_download_master_contract(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    exchange: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    // Motilal master contract endpoint
    let payload = json!({
        "exchange": exchange
    });

    match motilal_request(&client, "POST", "/rest/report/v1/getcontractmaster", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                Ok(ApiResponse {
                    success: true,
                    data: Some(data),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Failed to download master contract")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Search symbols in master contract
#[command]
pub async fn motilal_search_symbols(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    search_text: String,
    _exchange: Option<String>,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    // Motilal symbol search endpoint
    let payload = json!({
        "symbol": search_text
    });

    match motilal_request(&client, "POST", "/rest/report/v1/getsearchscrip", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let symbols = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "symbols": symbols,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "symbols": [] })),
                    error: None,
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}
