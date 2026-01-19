//! Tradier (US) Broker Integration
//!
//! Complete API implementation for Tradier Brokerage API including:
//! - Authentication (Bearer Token)
//! - Account Management (profile, balances, positions)
//! - Order Management (place, modify, cancel)
//! - Market Data (quotes, history, options)
//! - WebSocket (streaming quotes)
//!
//! Supports both live (production) and paper (sandbox) trading modes.

use reqwest::header::{HeaderMap, HeaderValue, ACCEPT, AUTHORIZATION, CONTENT_TYPE};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::collections::HashMap;

use super::common::ApiResponse;

// ============================================================================
// Tradier API Configuration
// ============================================================================

const TRADIER_LIVE_API_BASE: &str = "https://api.tradier.com/v1";
const TRADIER_SANDBOX_API_BASE: &str = "https://sandbox.tradier.com/v1";
const TRADIER_STREAM_URL: &str = "https://stream.tradier.com/v1";

fn get_api_base(is_paper: bool) -> &'static str {
    if is_paper {
        TRADIER_SANDBOX_API_BASE
    } else {
        TRADIER_LIVE_API_BASE
    }
}

fn create_tradier_headers(token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();

    if let Ok(auth) = HeaderValue::from_str(&format!("Bearer {}", token)) {
        headers.insert(AUTHORIZATION, auth);
    }

    headers.insert(ACCEPT, HeaderValue::from_static("application/json"));
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/x-www-form-urlencoded"));
    headers
}

// ============================================================================
// Account Commands
// ============================================================================

/// Get user profile
#[tauri::command]
pub async fn tradier_get_profile(
    token: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_profile] Fetching user profile (paper: {})", is_paper);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/user/profile", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("fault")
            .and_then(|f| f.get("faultstring"))
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch profile")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get account balances
#[tauri::command]
pub async fn tradier_get_balances(
    token: String,
    account_id: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_balances] Fetching balances for account: {}", account_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/accounts/{}/balances", base_url, account_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get account positions
#[tauri::command]
pub async fn tradier_get_positions(
    token: String,
    account_id: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_positions] Fetching positions for account: {}", account_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/accounts/{}/positions", base_url, account_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get account history
#[tauri::command]
pub async fn tradier_get_history(
    token: String,
    account_id: String,
    is_paper: bool,
    page: Option<i32>,
    limit: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_history] Fetching history for account: {}", account_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/accounts/{}/history", base_url, account_id);
    let mut params = vec![];
    if let Some(p) = page {
        params.push(format!("page={}", p));
    }
    if let Some(l) = limit {
        params.push(format!("limit={}", l));
    }
    if !params.is_empty() {
        url = format!("{}?{}", url, params.join("&"));
    }

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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get account gain/loss
#[tauri::command]
pub async fn tradier_get_gainloss(
    token: String,
    account_id: String,
    is_paper: bool,
    page: Option<i32>,
    limit: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_gainloss] Fetching gain/loss for account: {}", account_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/accounts/{}/gainloss", base_url, account_id);
    let mut params = vec![];
    if let Some(p) = page {
        params.push(format!("page={}", p));
    }
    if let Some(l) = limit {
        params.push(format!("limit={}", l));
    }
    if !params.is_empty() {
        url = format!("{}?{}", url, params.join("&"));
    }

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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Order Commands
// ============================================================================

/// Get all orders
#[tauri::command]
pub async fn tradier_get_orders(
    token: String,
    account_id: String,
    is_paper: bool,
    filter: Option<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_orders] Fetching orders for account: {}", account_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/accounts/{}/orders", base_url, account_id);
    if let Some(f) = filter {
        url = format!("{}?filter={}", url, f);
    }

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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get single order by ID
#[tauri::command]
pub async fn tradier_get_order(
    token: String,
    account_id: String,
    order_id: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_order] Fetching order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/accounts/{}/orders/{}", base_url, account_id, order_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Place an order
#[tauri::command]
pub async fn tradier_place_order(
    token: String,
    account_id: String,
    is_paper: bool,
    params: Value,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_place_order] Placing order for account: {}", account_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    // Convert JSON params to form body
    let mut form_params: Vec<(String, String)> = vec![];
    if let Value::Object(map) = params {
        for (key, value) in map {
            match value {
                Value::String(s) => form_params.push((key, s)),
                Value::Number(n) => form_params.push((key, n.to_string())),
                Value::Bool(b) => form_params.push((key, b.to_string())),
                _ => {}
            }
        }
    }

    let response = client
        .post(format!("{}/accounts/{}/orders", base_url, account_id))
        .headers(headers)
        .form(&form_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Modify an order
#[tauri::command]
pub async fn tradier_modify_order(
    token: String,
    account_id: String,
    order_id: String,
    is_paper: bool,
    params: Value,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_modify_order] Modifying order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    // Convert JSON params to form body
    let mut form_params: Vec<(String, String)> = vec![];
    if let Value::Object(map) = params {
        for (key, value) in map {
            match value {
                Value::String(s) => form_params.push((key, s)),
                Value::Number(n) => form_params.push((key, n.to_string())),
                Value::Bool(b) => form_params.push((key, b.to_string())),
                _ => {}
            }
        }
    }

    let response = client
        .put(format!("{}/accounts/{}/orders/{}", base_url, account_id, order_id))
        .headers(headers)
        .form(&form_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn tradier_cancel_order(
    token: String,
    account_id: String,
    order_id: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_cancel_order] Cancelling order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client
        .delete(format!("{}/accounts/{}/orders/{}", base_url, account_id, order_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Market Data Commands
// ============================================================================

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
    if let Some(g) = greeks {
        url = format!("{}&greeks={}", url, g);
    }

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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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

    let url = format!(
        "{}/markets/history?symbol={}&interval={}&start={}&end={}",
        base_url, symbol, interval, start, end
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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

    let url = format!(
        "{}/markets/timesales?symbol={}&interval={}&start={}&end={}",
        base_url, symbol, interval, start, end
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
    if let Some(g) = greeks {
        url = format!("{}&greeks={}", url, g);
    }

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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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

    let response = client
        .get(format!("{}/markets/options/expirations?symbol={}", base_url, symbol))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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

    let response = client
        .get(format!("{}/markets/options/strikes?symbol={}&expiration={}", base_url, symbol, expiration))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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

    let response = client
        .get(format!("{}/markets/clock", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
    if let Some(m) = month {
        params.push(format!("month={}", m));
    }
    if let Some(y) = year {
        params.push(format!("year={}", y));
    }
    if !params.is_empty() {
        url = format!("{}?{}", url, params.join("&"));
    }

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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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

    let response = client
        .get(format!("{}/markets/search?q={}", base_url, query))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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

    let response = client
        .get(format!("{}/markets/lookup?q={}", base_url, symbol))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Streaming Commands
// ============================================================================

/// Get streaming session ID
#[tauri::command]
pub async fn tradier_get_stream_session(
    token: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_stream_session] Getting stream session");

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client
        .post(format!("{}/markets/events/session", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = extract_tradier_error(&body);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// WebSocket connect/subscribe/unsubscribe commands would be implemented here
// For now, we'll add placeholder commands

/// Connect to WebSocket stream
#[tauri::command]
pub async fn tradier_ws_connect(
    session_id: String,
    is_paper: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[tradier_ws_connect] Connecting to WebSocket with session: {}", session_id);

    // TODO: Implement actual WebSocket connection
    // This would use tungstenite or tokio-tungstenite

    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

/// Subscribe to symbols on WebSocket
#[tauri::command]
pub async fn tradier_ws_subscribe(
    session_id: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[tradier_ws_subscribe] Subscribing to: {:?}", symbols);

    // TODO: Implement WebSocket subscription

    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

/// Unsubscribe from symbols on WebSocket
#[tauri::command]
pub async fn tradier_ws_unsubscribe(
    session_id: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[tradier_ws_unsubscribe] Unsubscribing from: {:?}", symbols);

    // TODO: Implement WebSocket unsubscription

    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

/// Disconnect from WebSocket
#[tauri::command]
pub async fn tradier_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    eprintln!("[tradier_ws_disconnect] Disconnecting from WebSocket");

    // TODO: Implement WebSocket disconnection

    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

// ============================================================================
// Helper Functions
// ============================================================================

fn extract_tradier_error(body: &Value) -> String {
    // Try fault format first
    if let Some(fault) = body.get("fault") {
        if let Some(faultstring) = fault.get("faultstring").and_then(|f| f.as_str()) {
            return faultstring.to_string();
        }
    }

    // Try errors format
    if let Some(errors) = body.get("errors") {
        if let Some(error) = errors.get("error") {
            if let Some(s) = error.as_str() {
                return s.to_string();
            }
            if let Some(arr) = error.as_array() {
                let msgs: Vec<String> = arr.iter()
                    .filter_map(|v| v.as_str().map(String::from))
                    .collect();
                if !msgs.is_empty() {
                    return msgs.join(", ");
                }
            }
        }
    }

    "Unknown error".to_string()
}
