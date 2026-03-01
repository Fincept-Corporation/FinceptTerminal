// Tradier Order Management Commands

use super::{get_api_base, create_tradier_headers, extract_tradier_error};
use super::super::common::ApiResponse;
use serde_json::Value;

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
    if let Some(f) = filter { url = format!("{}?filter={}", url, f); }

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

    let response = client.get(format!("{}/accounts/{}/orders/{}", base_url, account_id, order_id)).headers(headers).send().await
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

    let response = client.post(format!("{}/accounts/{}/orders", base_url, account_id)).headers(headers).form(&form_params).send().await
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

    let response = client.put(format!("{}/accounts/{}/orders/{}", base_url, account_id, order_id)).headers(headers).form(&form_params).send().await
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

    let response = client.delete(format!("{}/accounts/{}/orders/{}", base_url, account_id, order_id)).headers(headers).send().await
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
