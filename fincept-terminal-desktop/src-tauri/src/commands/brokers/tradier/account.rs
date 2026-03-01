// Tradier Account Commands

use super::{get_api_base, create_tradier_headers, extract_tradier_error};
use super::super::common::ApiResponse;
use serde_json::Value;

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

    let response = client.get(format!("{}/user/profile", base_url)).headers(headers).send().await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("fault").and_then(|f| f.get("faultstring")).and_then(|m| m.as_str()).unwrap_or("Failed to fetch profile").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
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

    let response = client.get(format!("{}/accounts/{}/balances", base_url, account_id)).headers(headers).send().await
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

    let response = client.get(format!("{}/accounts/{}/positions", base_url, account_id)).headers(headers).send().await
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
    if let Some(p) = page { params.push(format!("page={}", p)); }
    if let Some(l) = limit { params.push(format!("limit={}", l)); }
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
    if let Some(p) = page { params.push(format!("page={}", p)); }
    if let Some(l) = limit { params.push(format!("limit={}", l)); }
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
