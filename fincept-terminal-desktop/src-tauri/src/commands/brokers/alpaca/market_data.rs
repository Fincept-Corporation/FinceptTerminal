// Alpaca Market Data and Asset Commands

use serde_json::Value;
use std::collections::HashMap;
use crate::commands::brokers::common::ApiResponse;
use super::{create_alpaca_headers, ALPACA_DATA_API_BASE, ALPACA_LIVE_API_BASE};

// ============================================================================
// Market Data Commands
// ============================================================================

/// Get stock snapshot (latest trade, quote, bars)
#[tauri::command]
pub async fn alpaca_get_snapshot(
    api_key: String,
    api_secret: String,
    symbol: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_snapshot] Fetching snapshot for {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let response = client
        .get(format!("{}/v2/stocks/{}/snapshot", ALPACA_DATA_API_BASE, symbol))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch snapshot").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get multiple stock snapshots
#[tauri::command]
pub async fn alpaca_get_snapshots(
    api_key: String,
    api_secret: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<HashMap<String, Value>>, String> {
    eprintln!("[alpaca_get_snapshots] Fetching snapshots for {} symbols", symbols.len());

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let symbols_param = symbols.join(",");
    let response = client
        .get(format!("{}/v2/stocks/snapshots?symbols={}", ALPACA_DATA_API_BASE, symbols_param))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: HashMap<String, Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch snapshots".to_string()), timestamp })
    }
}

/// Get historical bars (OHLCV)
#[tauri::command]
pub async fn alpaca_get_bars(
    api_key: String,
    api_secret: String,
    symbol: String,
    timeframe: String,
    start: String,
    end: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_bars] Fetching bars for {} ({} to {})", symbol, start, end);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let url = format!(
        "{}/v2/stocks/{}/bars?timeframe={}&start={}&end={}&limit=10000",
        ALPACA_DATA_API_BASE, symbol, timeframe, start, end
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

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch bars").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get historical trades for a symbol
#[tauri::command]
pub async fn alpaca_get_trades(
    api_key: String,
    api_secret: String,
    symbol: String,
    start: String,
    end: String,
    limit: Option<i32>,
    page_token: Option<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_trades] Fetching trades for {} ({} to {})", symbol, start, end);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let mut url = format!(
        "{}/v2/stocks/{}/trades?start={}&end={}",
        ALPACA_DATA_API_BASE, symbol, start, end
    );
    if let Some(l) = limit { url = format!("{}&limit={}", url, l); }
    if let Some(pt) = page_token { url = format!("{}&page_token={}", url, pt); }

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch trades").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get historical quotes for a symbol
#[tauri::command]
pub async fn alpaca_get_quotes(
    api_key: String,
    api_secret: String,
    symbol: String,
    start: String,
    end: String,
    limit: Option<i32>,
    page_token: Option<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_quotes] Fetching quotes for {} ({} to {})", symbol, start, end);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let mut url = format!(
        "{}/v2/stocks/{}/quotes?start={}&end={}",
        ALPACA_DATA_API_BASE, symbol, start, end
    );
    if let Some(l) = limit { url = format!("{}&limit={}", url, l); }
    if let Some(pt) = page_token { url = format!("{}&page_token={}", url, pt); }

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quotes").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get latest trade for a symbol
#[tauri::command]
pub async fn alpaca_get_latest_trade(
    api_key: String,
    api_secret: String,
    symbol: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_latest_trade] Fetching latest trade for {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let response = client
        .get(format!("{}/v2/stocks/{}/trades/latest", ALPACA_DATA_API_BASE, symbol))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch latest trade").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get latest quote for a symbol
#[tauri::command]
pub async fn alpaca_get_latest_quote(
    api_key: String,
    api_secret: String,
    symbol: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_latest_quote] Fetching latest quote for {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let response = client
        .get(format!("{}/v2/stocks/{}/quotes/latest", ALPACA_DATA_API_BASE, symbol))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch latest quote").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get latest trades for multiple symbols
#[tauri::command]
pub async fn alpaca_get_latest_trades(
    api_key: String,
    api_secret: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_latest_trades] Fetching latest trades for {} symbols", symbols.len());

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let symbols_param = symbols.join(",");
    let response = client
        .get(format!("{}/v2/stocks/trades/latest?symbols={}", ALPACA_DATA_API_BASE, symbols_param))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch latest trades").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get latest quotes for multiple symbols
#[tauri::command]
pub async fn alpaca_get_latest_quotes(
    api_key: String,
    api_secret: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_latest_quotes] Fetching latest quotes for {} symbols", symbols.len());

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let symbols_param = symbols.join(",");
    let response = client
        .get(format!("{}/v2/stocks/quotes/latest?symbols={}", ALPACA_DATA_API_BASE, symbols_param))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch latest quotes").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get multi-bar data for multiple symbols
#[tauri::command]
pub async fn alpaca_get_multi_bars(
    api_key: String,
    api_secret: String,
    symbols: Vec<String>,
    timeframe: String,
    start: String,
    end: String,
    limit: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_multi_bars] Fetching bars for {} symbols", symbols.len());

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let symbols_param = symbols.join(",");
    let mut url = format!(
        "{}/v2/stocks/bars?symbols={}&timeframe={}&start={}&end={}",
        ALPACA_DATA_API_BASE, symbols_param, timeframe, start, end
    );
    if let Some(l) = limit { url = format!("{}&limit={}", url, l); }

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch multi-bars").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

// ============================================================================
// Asset Commands
// ============================================================================

/// Search assets
#[tauri::command]
pub async fn alpaca_search_assets(
    api_key: String,
    api_secret: String,
    query: String,
    exchange: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_search_assets] Searching for: {}", query);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let url = format!("{}/v2/assets?status=active&asset_class=us_equity", ALPACA_LIVE_API_BASE);

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if !status.is_success() {
        return Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch assets".to_string()), timestamp });
    }

    let assets: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let query_lower = query.to_lowercase();

    let filtered: Vec<Value> = assets
        .into_iter()
        .filter(|asset| {
            let symbol = asset.get("symbol").and_then(|s| s.as_str()).unwrap_or("").to_lowercase();
            let name = asset.get("name").and_then(|n| n.as_str()).unwrap_or("").to_lowercase();
            let asset_exchange = asset.get("exchange").and_then(|e| e.as_str()).unwrap_or("");

            let matches_query = symbol.contains(&query_lower) || name.contains(&query_lower);
            let matches_exchange = exchange.as_ref()
                .map(|e| asset_exchange.eq_ignore_ascii_case(e))
                .unwrap_or(true);

            matches_query && matches_exchange
        })
        .take(50)
        .collect();

    Ok(ApiResponse { success: true, data: Some(filtered), error: None, timestamp })
}

/// Get single asset
#[tauri::command]
pub async fn alpaca_get_asset(
    api_key: String,
    api_secret: String,
    symbol: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_asset] Fetching asset: {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let response = client
        .get(format!("{}/v2/assets/{}", ALPACA_LIVE_API_BASE, symbol))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Asset not found").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}
