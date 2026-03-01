// Fyers Portfolio Commands — Positions, Holdings, Funds

use super::{FYERS_API_BASE, create_fyers_headers};
use super::super::common::ApiResponse;
use serde_json::Value;

/// Get positions
#[tauri::command]
pub async fn fyers_get_positions(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/positions", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse { success: true, data: body.get("netPositions").cloned(), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to fetch positions").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get holdings
#[tauri::command]
pub async fn fyers_get_holdings(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/holdings", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse { success: true, data: body.get("holdings").cloned(), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to fetch holdings").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get margin/funds data
#[tauri::command]
pub async fn fyers_get_funds(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/funds", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse { success: true, data: body.get("fund_limit").cloned(), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to fetch funds").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}
