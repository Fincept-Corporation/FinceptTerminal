// commands/brokers/groww/market_data.rs - Market data commands

use serde_json::{json, Value};
use super::super::common::ApiResponse;
use super::{GROWW_API_BASE, SEGMENT_CASH, SEGMENT_FNO, EXCHANGE_NSE, EXCHANGE_BSE, create_groww_headers};

/// Get quote for a symbol
#[tauri::command]
pub async fn groww_get_quote(
    access_token: String,
    trading_symbol: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[groww_get_quote] Fetching quote for {}", trading_symbol);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    // Map exchange to segment
    let segment = match exchange.as_str() {
        "NSE" | "BSE" => SEGMENT_CASH,
        "NFO" | "BFO" => SEGMENT_FNO,
        _ => SEGMENT_CASH,
    };

    // Map exchange for Groww API
    let groww_exchange = match exchange.as_str() {
        "NFO" => EXCHANGE_NSE,
        "BFO" => EXCHANGE_BSE,
        _ => &exchange,
    };

    let response = client
        .get(format!("{}/v1/live-data/quote", GROWW_API_BASE))
        .headers(headers)
        .query(&[
            ("exchange", groww_exchange),
            ("segment", segment),
            ("trading_symbol", &trading_symbol),
        ])
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let payload = body.get("payload").cloned().unwrap_or(json!({}));

        // Transform to standard quote format
        let ltp = payload.get("last_price").and_then(|v| v.as_f64()).unwrap_or(0.0);
        let open = payload.get("ohlc").and_then(|o| o.get("open")).and_then(|v| v.as_f64()).unwrap_or(0.0);
        let high = payload.get("ohlc").and_then(|o| o.get("high")).and_then(|v| v.as_f64()).unwrap_or(0.0);
        let low = payload.get("ohlc").and_then(|o| o.get("low")).and_then(|v| v.as_f64()).unwrap_or(0.0);
        let close = payload.get("ohlc").and_then(|o| o.get("close")).and_then(|v| v.as_f64()).unwrap_or(0.0);
        let volume = payload.get("volume").and_then(|v| v.as_i64()).unwrap_or(0);
        let change = payload.get("day_change").and_then(|v| v.as_f64()).unwrap_or(0.0);
        let change_percent = payload.get("day_change_perc").and_then(|v| v.as_f64()).unwrap_or(0.0);

        let quote = json!({
            "symbol": trading_symbol,
            "exchange": exchange,
            "ltp": ltp,
            "open": open,
            "high": high,
            "low": low,
            "close": close,
            "prev_close": close,
            "volume": volume,
            "change": change,
            "change_percent": change_percent,
            "bid": payload.get("bid_price").and_then(|v| v.as_f64()).unwrap_or(0.0),
            "ask": payload.get("offer_price").and_then(|v| v.as_f64()).unwrap_or(0.0),
            "bid_qty": payload.get("bid_quantity").and_then(|v| v.as_i64()).unwrap_or(0),
            "ask_qty": payload.get("offer_quantity").and_then(|v| v.as_i64()).unwrap_or(0),
            "oi": payload.get("open_interest").and_then(|v| v.as_i64()).unwrap_or(0),
            "raw": payload,
        });

        Ok(ApiResponse {
            success: true,
            data: Some(quote),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quote")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get historical candle data
#[tauri::command]
pub async fn groww_get_historical(
    access_token: String,
    trading_symbol: String,
    exchange: String,
    interval: String,
    start_date: String,
    end_date: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[groww_get_historical] Fetching historical data for {}", trading_symbol);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    // Map exchange to segment
    let segment = match exchange.as_str() {
        "NSE" | "BSE" => SEGMENT_CASH,
        "NFO" | "BFO" => SEGMENT_FNO,
        _ => SEGMENT_CASH,
    };

    // Map exchange for Groww API
    let groww_exchange = match exchange.as_str() {
        "NFO" => EXCHANGE_NSE,
        "BFO" => EXCHANGE_BSE,
        _ => &exchange,
    };

    // Map interval to Groww format (minutes)
    let interval_minutes = match interval.as_str() {
        "1m" | "1" => "1",
        "5m" | "5" => "5",
        "10m" | "10" => "10",
        "15m" | "15" => "15",
        "30m" | "30" => "30",
        "1h" | "60" => "60",
        "4h" | "240" => "240",
        "D" | "day" | "1d" | "1440" => "1440",
        "W" | "week" | "1w" | "10080" => "10080",
        _ => "1440", // Default to daily
    };

    let response = client
        .get(format!("{}/v1/historical/candle/range", GROWW_API_BASE))
        .headers(headers)
        .query(&[
            ("exchange", groww_exchange),
            ("segment", segment),
            ("trading_symbol", trading_symbol.as_str()),
            ("start_time", &format!("{} 09:15:00", start_date)),
            ("end_time", &format!("{} 15:30:00", end_date)),
            ("interval_in_minutes", interval_minutes),
        ])
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let candles = body.get("payload")
            .and_then(|p| p.get("candles"))
            .and_then(|c| c.as_array())
            .cloned()
            .unwrap_or_default();

        // Transform candles to standard OHLCV format
        let formatted_candles: Vec<Value> = candles.iter().map(|candle| {
            if let Some(arr) = candle.as_array() {
                let ts = arr.get(0).and_then(|v| v.as_i64()).unwrap_or(0);
                let ts_seconds = if ts > 4102444800 { ts / 1000 } else { ts };

                json!({
                    "timestamp": ts_seconds,
                    "open": arr.get(1).and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "high": arr.get(2).and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "low": arr.get(3).and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "close": arr.get(4).and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "volume": arr.get(5).and_then(|v| v.as_i64()).unwrap_or(0),
                })
            } else {
                candle.clone()
            }
        }).collect();

        Ok(ApiResponse {
            success: true,
            data: Some(formatted_candles),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch historical data")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get market depth for a symbol
#[tauri::command]
pub async fn groww_get_depth(
    access_token: String,
    trading_symbol: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[groww_get_depth] Fetching market depth for {}", trading_symbol);

    // Depth data is included in the quote response for Groww
    let quote_result = groww_get_quote(access_token, trading_symbol.clone(), exchange.clone()).await?;

    let timestamp = chrono::Utc::now().timestamp_millis();

    if quote_result.success {
        if let Some(quote_data) = quote_result.data {
            if let Some(raw) = quote_data.get("raw") {
                // Extract depth from quote response
                let depth = raw.get("depth").cloned().unwrap_or(json!({}));

                let buy_levels = depth.get("buy")
                    .and_then(|b| b.as_array())
                    .cloned()
                    .unwrap_or_default();
                let sell_levels = depth.get("sell")
                    .and_then(|s| s.as_array())
                    .cloned()
                    .unwrap_or_default();

                // Format bids/asks
                let bids: Vec<Value> = buy_levels.iter().take(5).map(|level| {
                    json!({
                        "price": level.get("price").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "quantity": level.get("quantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    })
                }).collect();

                let asks: Vec<Value> = sell_levels.iter().take(5).map(|level| {
                    json!({
                        "price": level.get("price").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "quantity": level.get("quantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    })
                }).collect();

                let depth_data = json!({
                    "symbol": trading_symbol,
                    "exchange": exchange,
                    "bids": bids,
                    "asks": asks,
                    "ltp": quote_data.get("ltp").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "totalbuyqty": raw.get("total_buy_quantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "totalsellqty": raw.get("total_sell_quantity").and_then(|v| v.as_i64()).unwrap_or(0),
                });

                return Ok(ApiResponse {
                    success: true,
                    data: Some(depth_data),
                    error: None,
                    timestamp,
                });
            }
        }
    }

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Failed to fetch market depth".to_string()),
        timestamp,
    })
}
