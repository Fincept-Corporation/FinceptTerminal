// Tradier Market Data Commands

use super::{get_api_base, create_tradier_headers, extract_tradier_error};
use super::super::common::ApiResponse;
use serde_json::Value;

/// Get quotes for symbols
#[tauri::command]
pub async fn tradier_get_quotes(
    token: String,
    symbols: String,
    is_paper: bool,
    greeks: Option<bool>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_quotes] Fetching quotes for: {}", symbols);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/markets/quotes?symbols={}", base_url, symbols);
    if let Some(g) = greeks { url = format!("{}&greeks={}", url, g); }

    let response = client.get(&url).headers(headers).send().await.map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Get historical price data
#[tauri::command]
pub async fn tradier_get_historical(
    token: String,
    symbol: String,
    interval: String,
    start: String,
    end: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_historical] Fetching history for: {} ({} to {})", symbol, start, end);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let url = format!("{}/markets/history?symbol={}&interval={}&start={}&end={}", base_url, symbol, interval, start, end);

    let response = client.get(&url).headers(headers).send().await.map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Get time and sales data
#[tauri::command]
pub async fn tradier_get_timesales(
    token: String,
    symbol: String,
    interval: String,
    start: String,
    end: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_timesales] Fetching timesales for: {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let url = format!("{}/markets/timesales?symbol={}&interval={}&start={}&end={}", base_url, symbol, interval, start, end);

    let response = client.get(&url).headers(headers).send().await.map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Get option chains
#[tauri::command]
pub async fn tradier_get_option_chains(
    token: String,
    symbol: String,
    expiration: String,
    is_paper: bool,
    greeks: Option<bool>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_option_chains] Fetching options for: {} exp: {}", symbol, expiration);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/markets/options/chains?symbol={}&expiration={}", base_url, symbol, expiration);
    if let Some(g) = greeks { url = format!("{}&greeks={}", url, g); }

    let response = client.get(&url).headers(headers).send().await.map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Get option expirations
#[tauri::command]
pub async fn tradier_get_option_expirations(
    token: String,
    symbol: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_option_expirations] Fetching expirations for: {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client.get(format!("{}/markets/options/expirations?symbol={}", base_url, symbol)).headers(headers).send().await
        .map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Get option strikes
#[tauri::command]
pub async fn tradier_get_option_strikes(
    token: String,
    symbol: String,
    expiration: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_option_strikes] Fetching strikes for: {} exp: {}", symbol, expiration);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client.get(format!("{}/markets/options/strikes?symbol={}&expiration={}", base_url, symbol, expiration)).headers(headers).send().await
        .map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Get market clock
#[tauri::command]
pub async fn tradier_get_clock(
    token: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_clock] Fetching market clock");

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client.get(format!("{}/markets/clock", base_url)).headers(headers).send().await
        .map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Get market calendar
#[tauri::command]
pub async fn tradier_get_calendar(
    token: String,
    is_paper: bool,
    month: Option<i32>,
    year: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_calendar] Fetching market calendar");

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/markets/calendar", base_url);
    let mut params = vec![];
    if let Some(m) = month { params.push(format!("month={}", m)); }
    if let Some(y) = year { params.push(format!("year={}", y)); }
    if !params.is_empty() { url = format!("{}?{}", url, params.join("&")); }

    let response = client.get(&url).headers(headers).send().await.map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Search for symbols
#[tauri::command]
pub async fn tradier_search_symbols(
    token: String,
    query: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_search_symbols] Searching for: {}", query);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client.get(format!("{}/markets/search?q={}", base_url, query)).headers(headers).send().await
        .map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}

/// Lookup symbol
#[tauri::command]
pub async fn tradier_lookup_symbol(
    token: String,
    symbol: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_lookup_symbol] Looking up: {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client.get(format!("{}/markets/lookup?q={}", base_url, symbol)).headers(headers).send().await
        .map_err(|e| format!("Request failed: {}", e))?;
    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some(extract_tradier_error(&body)), timestamp })
    }
}
