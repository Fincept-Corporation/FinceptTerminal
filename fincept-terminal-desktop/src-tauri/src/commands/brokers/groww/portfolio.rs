// commands/brokers/groww/portfolio.rs - Portfolio commands

use serde_json::{json, Value};
use super::super::common::ApiResponse;
use super::{GROWW_API_BASE, SEGMENT_CASH, SEGMENT_FNO, create_groww_headers};

/// Get positions
#[tauri::command]
pub async fn groww_get_positions(
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[groww_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let mut all_positions = Vec::new();
    let segments = [SEGMENT_CASH, SEGMENT_FNO];

    for segment in segments {
        let response = client
            .get(format!("{}/v1/positions/user", GROWW_API_BASE))
            .headers(headers.clone())
            .query(&[("segment", segment)])
            .send()
            .await
            .map_err(|e| format!("Request failed: {}", e))?;

        let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

        if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
            if let Some(positions) = body.get("payload")
                .and_then(|p| p.get("positions"))
                .and_then(|pos| pos.as_array())
            {
                all_positions.extend(positions.clone());
            }
        }
    }

    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(all_positions),
        error: None,
        timestamp,
    })
}

/// Get holdings
#[tauri::command]
pub async fn groww_get_holdings(
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[groww_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let response = client
        .get(format!("{}/v1/holdings/detail/user", GROWW_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let holdings = body.get("payload").cloned().unwrap_or(json!({}));
        Ok(ApiResponse {
            success: true,
            data: Some(holdings),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch holdings")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get margin/funds data
#[tauri::command]
pub async fn groww_get_margins(
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[groww_get_margins] Fetching margin data");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let response = client
        .get(format!("{}/v1/margins/detail/user", GROWW_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let margins = body.get("payload").cloned().unwrap_or(json!({}));

        // Transform to standard format
        let available_cash = margins.get("clear_cash")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0);
        let collateral = margins.get("collateral_available")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0);
        let margin_used = margins.get("net_margin_used")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0);

        let formatted_margins = json!({
            "availablecash": format!("{:.2}", available_cash),
            "collateral": format!("{:.2}", collateral),
            "utiliseddebits": format!("{:.2}", margin_used),
            "m2munrealized": "0.00",
            "m2mrealized": "0.00",
            "raw": margins,
        });

        Ok(ApiResponse {
            success: true,
            data: Some(formatted_margins),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch margins")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}
