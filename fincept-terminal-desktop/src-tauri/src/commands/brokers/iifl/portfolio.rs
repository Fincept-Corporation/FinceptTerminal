// IIFL Portfolio Commands — Positions, Holdings, Funds

use super::{IIFL_INTERACTIVE_URL, create_iifl_headers};
use super::super::common::ApiResponse;
use serde_json::{json, Value};

/// Get positions
#[tauri::command]
pub async fn iifl_get_positions(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/portfolio/positions?dayOrNet=NetWise", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let positions = body.get("result").cloned().unwrap_or(json!({}));
        Ok(ApiResponse { success: true, data: Some(positions), error: None, timestamp })
    } else {
        let error_msg = body
            .get("description")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch positions")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get holdings
#[tauri::command]
pub async fn iifl_get_holdings(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/portfolio/holdings", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let holdings = body.get("result").cloned().unwrap_or(json!({}));
        Ok(ApiResponse { success: true, data: Some(holdings), error: None, timestamp })
    } else {
        let error_msg = body
            .get("description")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch holdings")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get funds/margins
#[tauri::command]
pub async fn iifl_get_funds(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_funds] Fetching funds");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/user/balance", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let result = body.get("result").cloned().unwrap_or(json!({}));

        let funds_data = if let Some(balance_list) = result.get("BalanceList").and_then(|b| b.as_array()) {
            if let Some(first_balance) = balance_list.first() {
                if let Some(rms_sublimits) = first_balance
                    .get("limitObject")
                    .and_then(|l| l.get("RMSSubLimits"))
                {
                    json!({
                        "availablecash": rms_sublimits.get("netMarginAvailable").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "collateral": rms_sublimits.get("collateral").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "m2munrealized": rms_sublimits.get("UnrealizedMTM").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "m2mrealized": rms_sublimits.get("RealizedMTM").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "utiliseddebits": rms_sublimits.get("marginUtilized").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "raw": result.clone()
                    })
                } else { result }
            } else { result }
        } else { result };

        Ok(ApiResponse { success: true, data: Some(funds_data), error: None, timestamp })
    } else {
        let error_msg = body
            .get("description")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch funds")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}
