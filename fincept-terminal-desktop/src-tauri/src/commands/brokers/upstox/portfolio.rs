// commands/brokers/upstox/portfolio.rs - Portfolio commands

use serde_json::{json, Value};
use super::super::common::ApiResponse;
use super::{UPSTOX_API_BASE_V2, create_upstox_headers};

/// Get positions
#[tauri::command]
pub async fn upstox_get_positions(access_token: String) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[upstox_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let response = client
        .get(format!("{}/portfolio/short-term-positions", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let positions = body.get("data").and_then(|d| d.as_array()).cloned().unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(positions),
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

/// Get holdings
#[tauri::command]
pub async fn upstox_get_holdings(access_token: String) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[upstox_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let response = client
        .get(format!("{}/portfolio/long-term-holdings", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let holdings = body.get("data").and_then(|d| d.as_array()).cloned().unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(holdings),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch holdings".to_string()),
            timestamp,
        })
    }
}

/// Get funds and margins
#[tauri::command]
pub async fn upstox_get_funds(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[upstox_get_funds] Fetching funds");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let response = client
        .get(format!("{}/user/get-funds-and-margin", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        // Combine equity and commodity margins
        let data = body.get("data").cloned();
        Ok(ApiResponse {
            success: true,
            data,
            error: None,
            timestamp,
        })
    } else {
        // Handle service hours error (423)
        if status.as_u16() == 423 {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "equity": {"available_margin": 0.0, "used_margin": 0.0},
                    "commodity": {"available_margin": 0.0, "used_margin": 0.0}
                })),
                error: Some("Service outside operating hours".to_string()),
                timestamp,
            })
        } else {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some("Failed to fetch funds".to_string()),
                timestamp,
            })
        }
    }
}
