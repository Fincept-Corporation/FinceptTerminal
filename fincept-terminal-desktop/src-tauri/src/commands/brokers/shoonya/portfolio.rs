// Shoonya Portfolio Commands

use super::{SHOONYA_API_BASE, create_shoonya_headers, create_payload_with_auth};
use super::super::common::ApiResponse;
use serde_json::json;

/// Get positions
#[tauri::command]
pub async fn shoonya_get_positions(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({ "uid": user_id, "actid": user_id });
    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/PositionBook", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() {
        if body.is_array() || body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
            Ok(ApiResponse {
                success: true,
                data: Some(if body.is_array() { body } else { json!([]) }),
                error: None,
                timestamp,
            })
        } else {
            Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
        }
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch positions".to_string()), timestamp })
    }
}

/// Get holdings
#[tauri::command]
pub async fn shoonya_get_holdings(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "actid": user_id,
        "prd": "C"  // Holdings are always CNC product
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/Holdings", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() {
        if body.is_array() || body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
            Ok(ApiResponse {
                success: true,
                data: Some(if body.is_array() { body } else { json!([]) }),
                error: None,
                timestamp,
            })
        } else {
            Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
        }
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch holdings".to_string()), timestamp })
    }
}

/// Get funds/margins (Limits endpoint)
#[tauri::command]
pub async fn shoonya_get_funds(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({ "uid": user_id, "actid": user_id });
    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/Limits", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[shoonya_get_funds] Response: {:?}", body);

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        // Transform Shoonya funds response to unified format
        // Formula from OpenAlgo: total_available = cash + payin - marginused
        let cash = body.get("cash").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let payin = body.get("payin").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let margin_used = body.get("marginused").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let collateral = body.get("brkcollamt").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let rpnl = body.get("rpnl").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let urmtom = body.get("urmtom").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);

        let available_cash = cash + payin - margin_used;

        let funds_data = json!({
            "availablecash": format!("{:.2}", available_cash),
            "collateral": format!("{:.2}", collateral),
            "m2munrealized": format!("{:.2}", urmtom),
            "m2mrealized": format!("{:.2}", -rpnl),
            "utiliseddebits": format!("{:.2}", margin_used),
            "raw": body
        });

        Ok(ApiResponse { success: true, data: Some(funds_data), error: None, timestamp })
    } else {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch funds")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}
