use super::{ALICEBLUE_API_BASE, create_aliceblue_headers};
use super::super::common::ApiResponse;
use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde_json::{json, Value};

/// Get quote for a symbol
#[tauri::command]
pub async fn aliceblue_get_quote(
    api_secret: String,
    session_id: String,
    exchange: String,
    token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[aliceblue_get_quote] Fetching quote for token {}", token);

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let payload = json!({
        "exch": exchange,
        "symbol": token,
    });

    let response = client
        .post(format!("{}/ScripDetails/getScripQuoteDetails", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quote")
            .to_string();
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        });
    }

    // Transform to standard quote format
    let ltp = body.get("ltp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let open = body.get("open").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let high = body.get("high").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let low = body.get("low").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let close = body.get("close").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let volume = body.get("volume").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0);
    let bid = body.get("bp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let ask = body.get("sp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let oi = body.get("oi").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0);

    let change = ltp - close;
    let change_percent = if close > 0.0 { (change / close) * 100.0 } else { 0.0 };

    let quote = json!({
        "symbol": body.get("symbol").and_then(|v| v.as_str()).unwrap_or(""),
        "exchange": exchange,
        "token": token,
        "ltp": ltp,
        "open": open,
        "high": high,
        "low": low,
        "close": close,
        "prev_close": close,
        "volume": volume,
        "change": change,
        "change_percent": change_percent,
        "bid": bid,
        "ask": ask,
        "oi": oi,
        "raw": body,
    });

    Ok(ApiResponse {
        success: true,
        data: Some(quote),
        error: None,
        timestamp,
    })
}

/// Get historical candle data
#[tauri::command]
pub async fn aliceblue_get_historical(
    _api_secret: String,
    session_id: String,
    user_id: String,
    token: String,
    exchange: String,
    interval: String,
    start_time: i64,
    end_time: i64,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_historical] Fetching historical data for token {}", token);

    let client = reqwest::Client::new();

    // Historical API uses different auth format: Bearer {user_id} {session_token}
    let mut headers = HeaderMap::new();
    let auth_value = format!("Bearer {} {}", user_id, session_id);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));

    // Map interval to Alice Blue format
    let ab_resolution = match interval.as_str() {
        "1m" | "1" => "1",
        "D" | "day" | "1d" => "D",
        _ => "D",
    };

    let payload = json!({
        "token": token,
        "exchange": exchange,
        "from": start_time.to_string(),
        "to": end_time.to_string(),
        "resolution": ab_resolution,
    });

    let response = client
        .post(format!("{}/chart/history", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch historical data")
            .to_string();
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        });
    }

    if let Some(result) = body.get("result").and_then(|r| r.as_array()) {
        // Transform candles to standard OHLCV format
        let formatted_candles: Vec<Value> = result.iter().map(|candle| {
            json!({
                "timestamp": candle.get("time").and_then(|v| v.as_str()).unwrap_or("0"),
                "open": candle.get("open").and_then(|v| v.as_f64()).unwrap_or(0.0),
                "high": candle.get("high").and_then(|v| v.as_f64()).unwrap_or(0.0),
                "low": candle.get("low").and_then(|v| v.as_f64()).unwrap_or(0.0),
                "close": candle.get("close").and_then(|v| v.as_f64()).unwrap_or(0.0),
                "volume": candle.get("volume").and_then(|v| v.as_i64()).unwrap_or(0),
            })
        }).collect();

        Ok(ApiResponse {
            success: true,
            data: Some(formatted_candles),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("No historical data available".to_string()),
            timestamp,
        })
    }
}
