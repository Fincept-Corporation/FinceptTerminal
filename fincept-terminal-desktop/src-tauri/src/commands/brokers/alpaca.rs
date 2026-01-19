//! Alpaca (US) Broker Integration
//!
//! Complete API implementation for Alpaca Trading API including:
//! - Authentication (API Key + Secret)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, account)
//! - Market Data (quotes, bars, snapshots)
//! - WebSocket (real-time data streaming)
//!
//! Supports both live and paper trading modes.

use reqwest::header::{HeaderMap, HeaderValue, CONTENT_TYPE};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::collections::HashMap;

use super::common::{ApiResponse, OrderPlaceResponse};

// ============================================================================
// Alpaca API Configuration
// ============================================================================

const ALPACA_LIVE_API_BASE: &str = "https://api.alpaca.markets";
const ALPACA_PAPER_API_BASE: &str = "https://paper-api.alpaca.markets";
const ALPACA_DATA_API_BASE: &str = "https://data.alpaca.markets";

fn get_api_base(is_paper: bool) -> &'static str {
    if is_paper {
        ALPACA_PAPER_API_BASE
    } else {
        ALPACA_LIVE_API_BASE
    }
}

fn create_alpaca_headers(api_key: &str, api_secret: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();

    if let Ok(key) = HeaderValue::from_str(api_key) {
        headers.insert("APCA-API-KEY-ID", key);
    }

    if let Ok(secret) = HeaderValue::from_str(api_secret) {
        headers.insert("APCA-API-SECRET-KEY", secret);
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers
}

// ============================================================================
// Alpaca Account Commands
// ============================================================================

/// Get Alpaca account information
#[tauri::command]
pub async fn alpaca_get_account(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_account] Fetching account (paper: {})", is_paper);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/account", base_url))
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch account")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get market clock (is market open, next open/close times)
#[tauri::command]
pub async fn alpaca_get_clock(
    api_key: String,
    api_secret: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_clock] Fetching market clock");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let response = client
        .get(format!("{}/v2/clock", ALPACA_LIVE_API_BASE))
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
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch clock".to_string()),
            timestamp,
        })
    }
}

/// Get market calendar
#[tauri::command]
pub async fn alpaca_get_calendar(
    api_key: String,
    api_secret: String,
    start: String,
    end: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_get_calendar] Fetching calendar from {} to {}", start, end);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);

    let response = client
        .get(format!("{}/v2/calendar?start={}&end={}", ALPACA_LIVE_API_BASE, start, end))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch calendar".to_string()),
            timestamp,
        })
    }
}

/// Get account configurations
#[tauri::command]
pub async fn alpaca_get_account_configurations(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_account_configurations] Fetching account configurations");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/account/configurations", base_url))
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch account configurations")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Update account configurations
#[tauri::command]
pub async fn alpaca_update_account_configurations(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    dtbp_check: Option<String>,           // "entry" or "exit" or "both"
    trade_confirm_email: Option<String>,   // "all" or "none"
    suspend_trade: Option<bool>,
    no_shorting: Option<bool>,
    fractional_trading: Option<bool>,
    max_margin_multiplier: Option<String>, // "1" or "2" or "4"
    pdt_check: Option<String>,             // "entry" or "exit" or "both"
    ptp_no_exception_entry: Option<bool>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_update_account_configurations] Updating account configurations");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let mut config = serde_json::Map::new();
    if let Some(v) = dtbp_check { config.insert("dtbp_check".to_string(), json!(v)); }
    if let Some(v) = trade_confirm_email { config.insert("trade_confirm_email".to_string(), json!(v)); }
    if let Some(v) = suspend_trade { config.insert("suspend_trade".to_string(), json!(v)); }
    if let Some(v) = no_shorting { config.insert("no_shorting".to_string(), json!(v)); }
    if let Some(v) = fractional_trading { config.insert("fractional_trading".to_string(), json!(v)); }
    if let Some(v) = max_margin_multiplier { config.insert("max_margin_multiplier".to_string(), json!(v)); }
    if let Some(v) = pdt_check { config.insert("pdt_check".to_string(), json!(v)); }
    if let Some(v) = ptp_no_exception_entry { config.insert("ptp_no_exception_entry".to_string(), json!(v)); }

    let response = client
        .patch(format!("{}/v2/account/configurations", base_url))
        .headers(headers)
        .json(&Value::Object(config))
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to update account configurations")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get account activities
#[tauri::command]
pub async fn alpaca_get_account_activities(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    activity_types: Option<String>,  // Comma-separated: FILL,TRANS,MISC,ACATC,ACATS,etc.
    date: Option<String>,            // YYYY-MM-DD
    until: Option<String>,           // RFC3339 timestamp
    after: Option<String>,           // RFC3339 timestamp
    direction: Option<String>,       // "asc" or "desc"
    page_size: Option<i32>,          // Max 100
    page_token: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_get_account_activities] Fetching account activities");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/v2/account/activities", base_url);
    let mut params: Vec<String> = Vec::new();

    if let Some(v) = activity_types { params.push(format!("activity_types={}", v)); }
    if let Some(v) = date { params.push(format!("date={}", v)); }
    if let Some(v) = until { params.push(format!("until={}", v)); }
    if let Some(v) = after { params.push(format!("after={}", v)); }
    if let Some(v) = direction { params.push(format!("direction={}", v)); }
    if let Some(v) = page_size { params.push(format!("page_size={}", v)); }
    if let Some(v) = page_token { params.push(format!("page_token={}", v)); }

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
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch account activities".to_string()),
            timestamp,
        })
    }
}

/// Get account activities by type
#[tauri::command]
pub async fn alpaca_get_account_activities_by_type(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    activity_type: String,           // FILL, TRANS, MISC, etc.
    date: Option<String>,
    until: Option<String>,
    after: Option<String>,
    direction: Option<String>,
    page_size: Option<i32>,
    page_token: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_get_account_activities_by_type] Fetching activities of type: {}", activity_type);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/v2/account/activities/{}", base_url, activity_type);
    let mut params: Vec<String> = Vec::new();

    if let Some(v) = date { params.push(format!("date={}", v)); }
    if let Some(v) = until { params.push(format!("until={}", v)); }
    if let Some(v) = after { params.push(format!("after={}", v)); }
    if let Some(v) = direction { params.push(format!("direction={}", v)); }
    if let Some(v) = page_size { params.push(format!("page_size={}", v)); }
    if let Some(v) = page_token { params.push(format!("page_token={}", v)); }

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
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch account activities".to_string()),
            timestamp,
        })
    }
}

/// Get portfolio history
#[tauri::command]
pub async fn alpaca_get_portfolio_history(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    period: Option<String>,          // 1D, 1W, 1M, 3M, 6M, 1A, all, intraday
    timeframe: Option<String>,       // 1Min, 5Min, 15Min, 1H, 1D
    date_start: Option<String>,      // YYYY-MM-DD
    date_end: Option<String>,        // YYYY-MM-DD
    extended_hours: Option<bool>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_portfolio_history] Fetching portfolio history");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/v2/account/portfolio/history", base_url);
    let mut params: Vec<String> = Vec::new();

    if let Some(v) = period { params.push(format!("period={}", v)); }
    if let Some(v) = timeframe { params.push(format!("timeframe={}", v)); }
    if let Some(v) = date_start { params.push(format!("date_start={}", v)); }
    if let Some(v) = date_end { params.push(format!("date_end={}", v)); }
    if let Some(v) = extended_hours { params.push(format!("extended_hours={}", v)); }

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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch portfolio history")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Alpaca Watchlist Commands
// ============================================================================

/// Get all watchlists
#[tauri::command]
pub async fn alpaca_get_watchlists(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_get_watchlists] Fetching all watchlists");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/watchlists", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch watchlists".to_string()),
            timestamp,
        })
    }
}

/// Create a new watchlist
#[tauri::command]
pub async fn alpaca_create_watchlist(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    name: String,
    symbols: Option<Vec<String>>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_create_watchlist] Creating watchlist: {}", name);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let mut body = json!({ "name": name });
    if let Some(syms) = symbols {
        body["symbols"] = json!(syms);
    }

    let response = client
        .post(format!("{}/v2/watchlists", base_url))
        .headers(headers)
        .json(&body)
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to create watchlist")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get a specific watchlist by ID
#[tauri::command]
pub async fn alpaca_get_watchlist(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    watchlist_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_watchlist] Fetching watchlist: {}", watchlist_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/watchlists/{}", base_url, watchlist_id))
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Watchlist not found")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Update a watchlist
#[tauri::command]
pub async fn alpaca_update_watchlist(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    watchlist_id: String,
    name: Option<String>,
    symbols: Option<Vec<String>>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_update_watchlist] Updating watchlist: {}", watchlist_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let mut body = serde_json::Map::new();
    if let Some(n) = name { body.insert("name".to_string(), json!(n)); }
    if let Some(s) = symbols { body.insert("symbols".to_string(), json!(s)); }

    let response = client
        .put(format!("{}/v2/watchlists/{}", base_url, watchlist_id))
        .headers(headers)
        .json(&Value::Object(body))
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to update watchlist")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Delete a watchlist
#[tauri::command]
pub async fn alpaca_delete_watchlist(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    watchlist_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[alpaca_delete_watchlist] Deleting watchlist: {}", watchlist_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .delete(format!("{}/v2/watchlists/{}", base_url, watchlist_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() || status.as_u16() == 204 {
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Failed to delete watchlist".to_string()),
            timestamp,
        })
    }
}

/// Add symbol to watchlist
#[tauri::command]
pub async fn alpaca_add_to_watchlist(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    watchlist_id: String,
    symbol: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_add_to_watchlist] Adding {} to watchlist: {}", symbol, watchlist_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let body = json!({ "symbol": symbol });

    let response = client
        .post(format!("{}/v2/watchlists/{}", base_url, watchlist_id))
        .headers(headers)
        .json(&body)
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to add symbol to watchlist")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Remove symbol from watchlist
#[tauri::command]
pub async fn alpaca_remove_from_watchlist(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    watchlist_id: String,
    symbol: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_remove_from_watchlist] Removing {} from watchlist: {}", symbol, watchlist_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .delete(format!("{}/v2/watchlists/{}/{}", base_url, watchlist_id, symbol))
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to remove symbol from watchlist")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Alpaca Order Commands
// ============================================================================

/// Place an order
#[tauri::command]
pub async fn alpaca_place_order(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    params: HashMap<String, Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_place_order] Placing order (paper: {})", is_paper);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .post(format!("{}/v2/orders", base_url))
        .headers(headers)
        .json(&params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        eprintln!("[alpaca_place_order] Order placed successfully: {:?}", body.get("id"));
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order placement failed")
            .to_string();
        eprintln!("[alpaca_place_order] Order failed: {}", error_msg);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Modify an order (replace)
#[tauri::command]
pub async fn alpaca_modify_order(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    order_id: String,
    params: HashMap<String, Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_modify_order] Modifying order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .patch(format!("{}/v2/orders/{}", base_url, order_id))
        .headers(headers)
        .json(&params)
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order modification failed")
            .to_string();
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
pub async fn alpaca_cancel_order(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    order_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[alpaca_cancel_order] Cancelling order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .delete(format!("{}/v2/orders/{}", base_url, order_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() || status.as_u16() == 204 {
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        let body: Value = response.json().await.unwrap_or(json!({}));
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order cancellation failed")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get all orders
#[tauri::command]
pub async fn alpaca_get_orders(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    status: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_get_orders] Fetching orders (status: {:?})", status);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let url = match status {
        Some(s) => format!("{}/v2/orders?status={}&limit=100", base_url, s),
        None => format!("{}/v2/orders?limit=100", base_url),
    };

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let http_status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if http_status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch orders".to_string()),
            timestamp,
        })
    }
}

/// Get a specific order by ID
#[tauri::command]
pub async fn alpaca_get_order(
    api_key: String,
    api_secret: String,
    order_id: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_order] Fetching order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/orders/{}", base_url, order_id))
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order not found")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Cancel all orders
#[tauri::command]
pub async fn alpaca_cancel_all_orders(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_cancel_all_orders] Cancelling all orders");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .delete(format!("{}/v2/orders", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() || status.as_u16() == 207 {
        let body: Vec<Value> = response.json().await.unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(json!({ "cancelled_count": body.len() })),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to cancel orders".to_string()),
            timestamp,
        })
    }
}

/// Place smart order (adjusts for existing position)
#[tauri::command]
pub async fn alpaca_place_smart_order(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    symbol: String,
    side: String,
    quantity: f64,
    order_type: String,
    price: Option<f64>,
    stop_price: Option<f64>,
    position_size: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_place_smart_order] Smart order for {} (position: {})", symbol, position_size);

    // Determine final action based on current position
    let (final_side, final_qty) = if side.to_uppercase() == "BUY" {
        if position_size < 0 {
            // Short position exists, need to close it first
            ("buy".to_string(), quantity)
        } else {
            ("buy".to_string(), quantity)
        }
    } else {
        if position_size > 0 {
            // Long position exists
            ("sell".to_string(), quantity)
        } else {
            ("sell".to_string(), quantity)
        }
    };

    // Build order params
    let mut params: HashMap<String, Value> = HashMap::new();
    params.insert("symbol".to_string(), json!(symbol));
    params.insert("qty".to_string(), json!(final_qty.to_string()));
    params.insert("side".to_string(), json!(final_side));
    params.insert("type".to_string(), json!(order_type.to_lowercase()));
    params.insert("time_in_force".to_string(), json!("day"));

    if let Some(p) = price {
        if p > 0.0 {
            params.insert("limit_price".to_string(), json!(p.to_string()));
        }
    }

    if let Some(sp) = stop_price {
        if sp > 0.0 {
            params.insert("stop_price".to_string(), json!(sp.to_string()));
        }
    }

    alpaca_place_order(api_key, api_secret, is_paper, params).await
}

// ============================================================================
// Alpaca Position Commands
// ============================================================================

/// Get all positions
#[tauri::command]
pub async fn alpaca_get_positions(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/positions", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch positions".to_string()),
            timestamp,
        })
    }
}

/// Get a specific position by symbol
#[tauri::command]
pub async fn alpaca_get_position(
    api_key: String,
    api_secret: String,
    symbol: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_position] Fetching position for: {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/positions/{}", base_url, symbol))
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Position not found")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Close a specific position by symbol
#[tauri::command]
pub async fn alpaca_close_position(
    api_key: String,
    api_secret: String,
    symbol: String,
    qty: Option<f64>,
    percentage: Option<f64>,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_close_position] Closing position for: {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    // Build query params for partial close
    let mut url = format!("{}/v2/positions/{}", base_url, symbol);
    if let Some(q) = qty {
        url = format!("{}?qty={}", url, q);
    } else if let Some(p) = percentage {
        url = format!("{}?percentage={}", url, p);
    }

    let response = client
        .delete(&url)
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
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to close position")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Close all positions
#[tauri::command]
pub async fn alpaca_close_all_positions(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_close_all_positions] Closing all positions");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    // This endpoint closes all positions and cancels orders
    let response = client
        .delete(format!("{}/v2/positions?cancel_orders=true", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() || status.as_u16() == 207 {
        let body: Vec<Value> = response.json().await.unwrap_or_default();
        let results: Vec<Value> = body.iter().map(|item| {
            let success = item.get("status").and_then(|s| s.as_i64()).map(|s| s == 200).unwrap_or(false);
            json!({
                "success": success,
                "symbol": item.get("symbol"),
                "error": if !success { item.get("body").and_then(|b| b.get("message")) } else { None }
            })
        }).collect();

        Ok(ApiResponse {
            success: true,
            data: Some(results),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to close positions".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Alpaca Market Data Commands
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch snapshot")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch snapshots".to_string()),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch bars")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch trades")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quotes")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch latest trade")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch latest quote")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch latest trades")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch latest quotes")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch multi-bars")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Alpaca Asset Commands
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

    // Get all tradable assets
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
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch assets".to_string()),
            timestamp,
        });
    }

    let assets: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let query_lower = query.to_lowercase();

    // Filter assets by query
    let filtered: Vec<Value> = assets
        .into_iter()
        .filter(|asset| {
            let symbol = asset.get("symbol")
                .and_then(|s| s.as_str())
                .unwrap_or("")
                .to_lowercase();
            let name = asset.get("name")
                .and_then(|n| n.as_str())
                .unwrap_or("")
                .to_lowercase();
            let asset_exchange = asset.get("exchange")
                .and_then(|e| e.as_str())
                .unwrap_or("");

            let matches_query = symbol.contains(&query_lower) || name.contains(&query_lower);
            let matches_exchange = exchange.as_ref()
                .map(|e| asset_exchange.eq_ignore_ascii_case(e))
                .unwrap_or(true);

            matches_query && matches_exchange
        })
        .take(50)
        .collect();

    Ok(ApiResponse {
        success: true,
        data: Some(filtered),
        error: None,
        timestamp,
    })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Asset not found")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Alpaca WebSocket Commands (placeholders - actual WS handled differently)
// ============================================================================

/// Connect to Alpaca WebSocket
#[tauri::command]
pub async fn alpaca_ws_connect(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[alpaca_ws_connect] WebSocket connection request (paper: {})", is_paper);

    // TODO: Implement actual WebSocket connection
    // For now, return success placeholder
    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

/// Disconnect from Alpaca WebSocket
#[tauri::command]
pub async fn alpaca_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    eprintln!("[alpaca_ws_disconnect] WebSocket disconnect request");

    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

/// Subscribe to symbols via WebSocket
#[tauri::command]
pub async fn alpaca_ws_subscribe(
    trades: Vec<String>,
    quotes: Vec<String>,
    bars: Vec<String>,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[alpaca_ws_subscribe] Subscribing to trades: {:?}, quotes: {:?}, bars: {:?}",
        trades.len(), quotes.len(), bars.len());

    // TODO: Implement actual WebSocket subscription
    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

/// Unsubscribe from symbols via WebSocket
#[tauri::command]
pub async fn alpaca_ws_unsubscribe(
    trades: Vec<String>,
    quotes: Vec<String>,
    bars: Vec<String>,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[alpaca_ws_unsubscribe] Unsubscribing from trades: {:?}, quotes: {:?}, bars: {:?}",
        trades.len(), quotes.len(), bars.len());

    // TODO: Implement actual WebSocket unsubscription
    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

// ============================================================================
// Credential Storage Commands
// ============================================================================

/// Store Alpaca credentials
#[tauri::command]
pub async fn store_alpaca_credentials(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[store_alpaca_credentials] Storing credentials (paper: {})", is_paper);

    // TODO: Implement secure credential storage
    // For now, just validate they work
    let validate = alpaca_get_account(api_key, api_secret, is_paper).await?;

    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: validate.success,
        data: Some(validate.success),
        error: validate.error,
        timestamp,
    })
}
