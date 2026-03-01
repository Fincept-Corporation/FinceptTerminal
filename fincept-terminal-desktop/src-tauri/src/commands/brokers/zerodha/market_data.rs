// Zerodha Market Data & Symbol Lookup Commands

use super::{ZERODHA_API_BASE, create_zerodha_headers};
use super::super::common::ApiResponse;
use serde_json::{json, Value};
use std::collections::HashMap;

/// Get quotes for instruments
#[tauri::command]
pub async fn zerodha_get_quote(
    api_key: String,
    access_token: String,
    instruments: Vec<String>,
) -> Result<ApiResponse<HashMap<String, Value>>, String> {
    eprintln!("[zerodha_get_quote] Fetching quotes for {} instruments", instruments.len());

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let instruments_param = instruments.join("&i=");
    let url = format!("{}/quote?i={}", ZERODHA_API_BASE, instruments_param);

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data")
            .and_then(|d| d.as_object())
            .map(|obj| obj.iter().map(|(k, v)| (k.clone(), v.clone())).collect())
            .unwrap_or_default();
        Ok(ApiResponse { success: true, data: Some(data), error: None, timestamp })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quotes")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get historical data (OHLCV)
#[tauri::command]
pub async fn zerodha_get_historical(
    api_key: String,
    access_token: String,
    instrument_token: String,
    interval: String,
    from: String,
    to: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_historical] Fetching historical data for {}", instrument_token);

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let url = format!(
        "{}/instruments/historical/{}/{}?from={}&to={}",
        ZERODHA_API_BASE, instrument_token, interval, from, to
    );

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").cloned().unwrap_or(json!({}));
        Ok(ApiResponse { success: true, data: Some(data), error: None, timestamp })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch historical data")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Download master contract (instruments list)
#[tauri::command]
pub async fn zerodha_download_master_contract(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_download_master_contract] Downloading instruments");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/instruments", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        let csv_data = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;
        let mut instruments: Vec<Value> = Vec::new();
        let mut lines = csv_data.lines();

        let headers: Vec<&str> = if let Some(header_line) = lines.next() {
            header_line.split(',').collect()
        } else {
            return Err("Empty CSV response".to_string());
        };

        for line in lines {
            let values: Vec<&str> = line.split(',').collect();
            if values.len() == headers.len() {
                let mut obj = serde_json::Map::new();
                for (i, header) in headers.iter().enumerate() {
                    obj.insert(header.to_string(), json!(values[i]));
                }
                instruments.push(Value::Object(obj));
            }
        }

        eprintln!("[zerodha_download_master_contract] Downloaded {} instruments", instruments.len());
        Ok(ApiResponse { success: true, data: Some(instruments), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to download instruments".to_string()), timestamp })
    }
}

/// Search symbols in master contract database
#[tauri::command]
pub async fn zerodha_search_symbols(
    query: String,
    _exchange: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_search_symbols] Searching for: {}", query);

    let timestamp = chrono::Utc::now().timestamp();
    Ok(ApiResponse {
        success: true,
        data: Some(vec![]),
        error: Some("Master contract database not yet implemented. Use zerodha_download_master_contract first.".to_string()),
        timestamp,
    })
}

/// Get instrument details from master contract
#[tauri::command]
pub async fn zerodha_get_instrument(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_instrument] Looking up: {} on {}", symbol, exchange);

    let timestamp = chrono::Utc::now().timestamp();
    let instrument = json!({
        "symbol": symbol,
        "exchange": exchange,
        "name": symbol.clone(),
        "token": "0",
        "instrument_token": "0",
        "lot_size": 1,
        "tick_size": 0.05,
        "instrument_type": "EQ"
    });

    Ok(ApiResponse {
        success: true,
        data: Some(instrument),
        error: Some("Master contract database not yet implemented. Returning fallback data.".to_string()),
        timestamp,
    })
}

/// Get master contract metadata (last updated, count)
#[tauri::command]
pub async fn zerodha_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Master contract database not yet implemented for Zerodha".to_string()),
        timestamp,
    })
}

/// Search symbols using the symbol master DB (shared across Indian brokers)
#[tauri::command]
pub async fn search_indian_symbols(
    broker_id: String,
    query: String,
    exchange: Option<String>,
) -> Result<Vec<Value>, String> {
    eprintln!("[search_indian_symbols] Searching for '{}' in {} broker", query, broker_id);

    use crate::database::symbol_master;

    match symbol_master::search_symbols(&broker_id, &query, exchange.as_deref(), None, 50) {
        Ok(results) => {
            let values: Vec<Value> = results.iter().map(|r| {
                json!({
                    "symbol": r.symbol,
                    "tradingsymbol": r.br_symbol,
                    "name": r.name,
                    "exchange": r.exchange,
                    "token": r.token,
                    "instrumenttype": r.instrument_type,
                    "lotsize": r.lot_size,
                    "ticksize": r.tick_size,
                    "expiry": r.expiry,
                    "strike": r.strike,
                })
            }).collect();
            Ok(values)
        }
        Err(e) => Err(format!("Search failed: {}", e)),
    }
}

/// Get symbol token from master contract
#[tauri::command]
pub async fn get_indian_symbol_token(
    broker_id: String,
    symbol: String,
    exchange: String,
) -> Result<Option<Value>, String> {
    eprintln!("[get_indian_symbol_token] Getting token for {}:{}", exchange, symbol);

    use crate::database::symbol_master;

    match symbol_master::get_symbol(&broker_id, &symbol, &exchange) {
        Ok(Some(r)) => Ok(Some(json!({
            "symbol": r.symbol,
            "tradingsymbol": r.br_symbol,
            "name": r.name,
            "exchange": r.exchange,
            "token": r.token,
            "instrumenttype": r.instrument_type,
            "lotsize": r.lot_size,
            "ticksize": r.tick_size,
            "expiry": r.expiry,
            "strike": r.strike,
        }))),
        Ok(None) => Ok(None),
        Err(e) => Err(format!("Lookup failed: {}", e)),
    }
}

/// Get master contract last updated timestamp
#[tauri::command]
pub async fn get_master_contract_timestamp(
    broker_id: String,
) -> Result<Option<i64>, String> {
    eprintln!("[get_master_contract_timestamp] Getting timestamp for: {}", broker_id);

    use crate::database::symbol_master;

    match symbol_master::get_status(&broker_id) {
        Ok(info) => Ok(Some(info.last_updated)),
        Err(_) => Ok(None),
    }
}
