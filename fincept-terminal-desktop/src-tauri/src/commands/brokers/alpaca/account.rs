// Alpaca Account and Watchlist Commands

use serde_json::{json, Value};
use crate::commands::brokers::common::ApiResponse;
use super::{get_api_base, create_alpaca_headers, ALPACA_LIVE_API_BASE};

// ============================================================================
// Account Commands
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch account").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch clock".to_string()), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch calendar".to_string()), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch account configurations").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Update account configurations
#[tauri::command]
pub async fn alpaca_update_account_configurations(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    dtbp_check: Option<String>,
    trade_confirm_email: Option<String>,
    suspend_trade: Option<bool>,
    no_shorting: Option<bool>,
    fractional_trading: Option<bool>,
    max_margin_multiplier: Option<String>,
    pdt_check: Option<String>,
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to update account configurations").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get account activities
#[tauri::command]
pub async fn alpaca_get_account_activities(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    activity_types: Option<String>,
    date: Option<String>,
    until: Option<String>,
    after: Option<String>,
    direction: Option<String>,
    page_size: Option<i32>,
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch account activities".to_string()), timestamp })
    }
}

/// Get account activities by type
#[tauri::command]
pub async fn alpaca_get_account_activities_by_type(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    activity_type: String,
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch account activities".to_string()), timestamp })
    }
}

/// Get portfolio history
#[tauri::command]
pub async fn alpaca_get_portfolio_history(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    period: Option<String>,
    timeframe: Option<String>,
    date_start: Option<String>,
    date_end: Option<String>,
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch portfolio history").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

// ============================================================================
// Watchlist Commands
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch watchlists".to_string()), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to create watchlist").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Watchlist not found").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to update watchlist").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Failed to delete watchlist".to_string()), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to add symbol to watchlist").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to remove symbol from watchlist").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

// ============================================================================
// Credential Storage
// ============================================================================

/// Store Alpaca credentials (validates by fetching account)
#[tauri::command]
pub async fn store_alpaca_credentials(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[store_alpaca_credentials] Storing credentials (paper: {})", is_paper);

    let validate = alpaca_get_account(api_key, api_secret, is_paper).await?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: validate.success,
        data: Some(validate.success),
        error: validate.error,
        timestamp,
    })
}
