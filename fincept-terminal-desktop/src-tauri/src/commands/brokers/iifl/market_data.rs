// IIFL Market Data & Master Contract Commands

use super::{IIFL_MARKET_DATA_URL, create_iifl_headers, get_exchange_segment, get_exchange_segment_id};
use super::super::common::ApiResponse;
use reqwest::header::CONTENT_TYPE;
use serde_json::{json, Value};

/// Get quotes for instruments
#[tauri::command]
pub async fn iifl_get_quotes(
    feed_token: String,
    exchange: String,
    token: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_quotes] Fetching quotes for {}:{}", exchange, token);

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&feed_token);
    let exchange_segment = get_exchange_segment_id(&exchange);

    let payload = json!({
        "instruments": [{"exchangeSegment": exchange_segment, "exchangeInstrumentID": token}],
        "xtsMessageCode": 1502,
        "publishFormat": "JSON"
    });

    let response = client
        .post(format!("{}/instruments/quotes", IIFL_MARKET_DATA_URL))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let list_quotes = body
            .get("result")
            .and_then(|r| r.get("listQuotes"))
            .and_then(|l| l.as_array());

        if let Some(quotes) = list_quotes {
            if let Some(first_quote) = quotes.first() {
                let quote_data: Value = if first_quote.is_string() {
                    serde_json::from_str(first_quote.as_str().unwrap_or("{}")).unwrap_or(json!({}))
                } else {
                    first_quote.clone()
                };

                let empty_obj = json!({});
                let touchline = quote_data.get("Touchline").unwrap_or(&empty_obj);

                let formatted_quote = json!({
                    "ask": touchline.get("AskInfo").and_then(|a| a.get("Price")).and_then(|p| p.as_f64()).unwrap_or(0.0),
                    "bid": touchline.get("BidInfo").and_then(|b| b.get("Price")).and_then(|p| p.as_f64()).unwrap_or(0.0),
                    "high": touchline.get("High").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "low": touchline.get("Low").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "ltp": touchline.get("LastTradedPrice").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "open": touchline.get("Open").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "prev_close": touchline.get("Close").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "volume": touchline.get("TotalTradedQuantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "oi": 0,
                    "raw": quote_data
                });

                return Ok(ApiResponse { success: true, data: Some(formatted_quote), error: None, timestamp });
            }
        }

        Ok(ApiResponse { success: false, data: None, error: Some("No quote data available".to_string()), timestamp })
    } else {
        let error_msg = body.get("description").and_then(|m| m.as_str()).unwrap_or("Failed to fetch quotes").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get historical OHLCV data
#[tauri::command]
pub async fn iifl_get_history(
    feed_token: String,
    exchange: String,
    token: i64,
    timeframe: String,
    from_date: String,
    to_date: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[iifl_get_history] Fetching history for {}:{} from {} to {}", exchange, token, from_date, to_date);

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&feed_token);

    let compression_value = match timeframe.as_str() {
        "1s" => "1",
        "1m" => "60",
        "2m" => "120",
        "3m" => "180",
        "5m" => "300",
        "10m" => "600",
        "15m" => "900",
        "30m" => "1800",
        "60m" | "1h" => "3600",
        "D" | "1D" => "D",
        _ => "60",
    };

    let exchange_segment = get_exchange_segment(&exchange);

    let url = format!(
        "{}/instruments/ohlc?exchangeSegment={}&exchangeInstrumentID={}&startTime={}&endTime={}&compressionValue={}",
        IIFL_MARKET_DATA_URL, exchange_segment, token, from_date, to_date, compression_value
    );

    let response = client.get(&url).headers(headers).send().await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let raw_data = body
            .get("result")
            .and_then(|r| r.get("dataReponse"))
            .and_then(|d| d.as_str())
            .unwrap_or("");

        let mut candles: Vec<Value> = Vec::new();

        if !raw_data.is_empty() {
            for row in raw_data.split(',') {
                let fields: Vec<&str> = row.split('|').collect();
                if fields.len() >= 6 {
                    if let (Ok(ts), Ok(open), Ok(high), Ok(low), Ok(close), Ok(volume)) = (
                        fields[0].parse::<i64>(),
                        fields[1].parse::<f64>(),
                        fields[2].parse::<f64>(),
                        fields[3].parse::<f64>(),
                        fields[4].parse::<f64>(),
                        fields[5].parse::<i64>(),
                    ) {
                        candles.push(json!({"timestamp": ts, "open": open, "high": high, "low": low, "close": close, "volume": volume}));
                    }
                }
            }
        }

        Ok(ApiResponse { success: true, data: Some(candles), error: None, timestamp })
    } else {
        let error_msg = body.get("description").and_then(|m| m.as_str()).unwrap_or("Failed to fetch historical data").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get market depth
#[tauri::command]
pub async fn iifl_get_depth(
    feed_token: String,
    exchange: String,
    token: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_depth] Fetching depth for {}:{}", exchange, token);

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&feed_token);
    let exchange_segment = get_exchange_segment_id(&exchange);

    let payload = json!({
        "instruments": [{"exchangeSegment": exchange_segment, "exchangeInstrumentID": token}],
        "xtsMessageCode": 1502,
        "publishFormat": "JSON"
    });

    let response = client
        .post(format!("{}/instruments/quotes", IIFL_MARKET_DATA_URL))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let list_quotes = body.get("result").and_then(|r| r.get("listQuotes")).and_then(|l| l.as_array());

        if let Some(quotes) = list_quotes {
            if let Some(first_quote) = quotes.first() {
                let quote_data: Value = if first_quote.is_string() {
                    serde_json::from_str(first_quote.as_str().unwrap_or("{}")).unwrap_or(json!({}))
                } else {
                    first_quote.clone()
                };

                let empty_obj = json!({});
                let touchline = quote_data.get("Touchline").unwrap_or(&empty_obj);

                let bids: Vec<Value> = quote_data
                    .get("Bids")
                    .and_then(|b| b.as_array())
                    .map(|arr| arr.iter().take(5).map(|b| json!({
                        "price": b.get("Price").and_then(|p| p.as_f64()).unwrap_or(0.0),
                        "quantity": b.get("Size").and_then(|s| s.as_i64()).unwrap_or(0)
                    })).collect())
                    .unwrap_or_default();

                let asks: Vec<Value> = quote_data
                    .get("Asks")
                    .and_then(|a| a.as_array())
                    .map(|arr| arr.iter().take(5).map(|a| json!({
                        "price": a.get("Price").and_then(|p| p.as_f64()).unwrap_or(0.0),
                        "quantity": a.get("Size").and_then(|s| s.as_i64()).unwrap_or(0)
                    })).collect())
                    .unwrap_or_default();

                let depth_data = json!({
                    "bids": bids,
                    "asks": asks,
                    "high": touchline.get("High").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "low": touchline.get("Low").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "ltp": touchline.get("LastTradedPrice").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "ltq": touchline.get("LastTradedQunatity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "open": touchline.get("Open").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "prev_close": touchline.get("Close").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "volume": touchline.get("TotalTradedQuantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "oi": 0,
                    "totalbuyqty": touchline.get("TotalBuyQuantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "totalsellqty": touchline.get("TotalSellQuantity").and_then(|v| v.as_i64()).unwrap_or(0)
                });

                return Ok(ApiResponse { success: true, data: Some(depth_data), error: None, timestamp });
            }
        }

        Ok(ApiResponse { success: false, data: None, error: Some("No depth data available".to_string()), timestamp })
    } else {
        let error_msg = body.get("description").and_then(|m| m.as_str()).unwrap_or("Failed to fetch market depth").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Download master contract
#[tauri::command]
pub async fn iifl_download_master_contract(
    exchange: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[iifl_download_master_contract] Downloading master contract for {}", exchange);

    let client = reqwest::Client::new();
    let exchange_segment = get_exchange_segment(&exchange);

    let payload = json!({"exchangeSegmentList": [exchange_segment]});

    let response = client
        .post(format!("{}/instruments/master", IIFL_MARKET_DATA_URL))
        .header(CONTENT_TYPE, "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        let result_str = body.get("result").and_then(|r| r.as_str()).unwrap_or("");
        let mut instruments: Vec<Value> = Vec::new();

        for row in result_str.split('\n') {
            if row.trim().is_empty() { continue; }
            let fields: Vec<&str> = row.split('|').collect();
            if fields.len() >= 6 {
                instruments.push(json!({
                    "exchange": exchange,
                    "exchangeSegment": fields.get(0).unwrap_or(&""),
                    "exchangeInstrumentID": fields.get(1).unwrap_or(&""),
                    "instrumentType": fields.get(2).unwrap_or(&""),
                    "name": fields.get(3).unwrap_or(&""),
                    "description": fields.get(4).unwrap_or(&""),
                    "series": fields.get(5).unwrap_or(&""),
                    "token": fields.get(1).unwrap_or(&""),
                    "symbol": fields.get(3).unwrap_or(&""),
                    "lotSize": fields.get(12).unwrap_or(&"1"),
                    "tickSize": fields.get(11).unwrap_or(&"0.05")
                }));
            }
        }

        eprintln!("[iifl_download_master_contract] Downloaded {} instruments for {}", instruments.len(), exchange);
        Ok(ApiResponse { success: true, data: Some(instruments), error: None, timestamp })
    } else {
        let error_msg = body.get("description").and_then(|m| m.as_str()).unwrap_or("Failed to download master contract").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Search symbols in master contract
#[tauri::command]
pub async fn iifl_search_symbol(
    query: String,
    _exchange: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[iifl_search_symbol] Searching for: {}", query);

    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse {
        success: true,
        data: Some(vec![]),
        error: Some("Master contract search not yet implemented. Use iifl_download_master_contract first.".to_string()),
        timestamp,
    })
}

/// Get token for symbol
#[tauri::command]
pub async fn iifl_get_token_for_symbol(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_token_for_symbol] Getting token for {}:{}", exchange, symbol);

    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Symbol lookup not yet implemented".to_string()),
        timestamp,
    })
}

/// Get master contract metadata
#[tauri::command]
pub async fn iifl_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Master contract database not yet implemented for IIFL".to_string()),
        timestamp,
    })
}
