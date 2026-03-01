// AngelOne Market Data Commands

use serde_json::{json, Value};
use super::super::common::ApiResponse;
use super::angel_api_call;

/// Get market quotes (supports single and multiple tokens)
/// AngelOne API: POST /rest/secure/angelbroking/market/v1/quote/
#[tauri::command]
pub async fn angelone_get_quote(
    api_key: String,
    access_token: String,
    exchange: String,
    tokens: Vec<String>,
    mode: Option<String>,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let quote_mode = mode.unwrap_or_else(|| "FULL".to_string());

    let payload = json!({
        "mode": quote_mode,
        "exchangeTokens": {
            exchange: tokens
        }
    });

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/market/v1/quote/",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            if body.get("status").and_then(|v| v.as_bool()) == Some(true) ||
               body.get("status").and_then(|v| v.as_str()) == Some("true") {
                ApiResponse {
                    success: true,
                    data: body.get("data").cloned(),
                    error: None,
                    timestamp,
                }
            } else {
                let msg = body.get("message")
                    .and_then(|v| v.as_str())
                    .unwrap_or("Quote fetch failed")
                    .to_string();
                ApiResponse {
                    success: false,
                    data: None,
                    error: Some(msg),
                    timestamp,
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Get historical OHLCV candle data
/// AngelOne API: POST /rest/secure/angelbroking/historical/v1/getCandleData
#[tauri::command]
pub async fn angelone_get_historical(
    api_key: String,
    access_token: String,
    exchange: String,
    symbol_token: String,
    interval: String,
    from_date: String,
    to_date: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let payload = json!({
        "exchange": exchange,
        "symboltoken": symbol_token,
        "interval": interval,
        "fromdate": from_date,
        "todate": to_date
    });

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/historical/v1/getCandleData",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            if body.get("status").and_then(|v| v.as_bool()) == Some(true) ||
               body.get("status").and_then(|v| v.as_str()) == Some("true") {
                // data is array of arrays: [[timestamp, open, high, low, close, volume], ...]
                let candle_data = body.get("data").cloned().unwrap_or(json!([]));
                ApiResponse {
                    success: true,
                    data: Some(candle_data),
                    error: None,
                    timestamp,
                }
            } else {
                let msg = body.get("message")
                    .and_then(|v| v.as_str())
                    .unwrap_or("No historical data available")
                    .to_string();
                ApiResponse {
                    success: false,
                    data: None,
                    error: Some(msg),
                    timestamp,
                }
            }
        }
        Err(e) => {
            ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            }
        },
    }
}
