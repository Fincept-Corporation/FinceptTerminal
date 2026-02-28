// IBKR Market Data and Scanner Commands

use serde_json::Value;
use crate::commands::brokers::common::ApiResponse;
use super::{get_api_base, create_ibkr_headers, create_client};

// ============================================================================
// Market Data Commands
// ============================================================================

/// Get market data snapshot for conids
#[tauri::command]
pub async fn ibkr_get_snapshot(
    access_token: Option<String>,
    use_gateway: bool,
    conids: Vec<i64>,
    fields: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_snapshot] Fetching snapshot for {} conids", conids.len());

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let conids_param = conids.iter().map(|c| c.to_string()).collect::<Vec<_>>().join(",");
    let fields_param = fields.join(",");

    let response = client
        .get(format!("{}/iserver/marketdata/snapshot?conids={}&fields={}", base_url, conids_param, fields_param))
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
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch snapshot".to_string()), timestamp })
    }
}

/// Unsubscribe from market data
#[tauri::command]
pub async fn ibkr_unsubscribe_market_data(
    access_token: Option<String>,
    use_gateway: bool,
    conid: i64,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_unsubscribe_market_data] Unsubscribing from conid: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/marketdata/{}/unsubscribe", base_url, conid))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Failed to unsubscribe".to_string()), timestamp })
    }
}

/// Unsubscribe from all market data
#[tauri::command]
pub async fn ibkr_unsubscribe_all_market_data(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_unsubscribe_all_market_data] Unsubscribing from all market data");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/marketdata/unsubscribeall", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Failed to unsubscribe all".to_string()), timestamp })
    }
}

/// Get historical data (bars)
#[tauri::command]
pub async fn ibkr_get_historical_data(
    access_token: Option<String>,
    use_gateway: bool,
    conid: i64,
    period: String,
    bar_size: String,
    outside_rth: Option<bool>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_historical_data] Fetching history for conid: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut url = format!(
        "{}/iserver/marketdata/history?conid={}&period={}&bar={}",
        base_url, conid, period, bar_size
    );

    if let Some(orh) = outside_rth {
        url = format!("{}&outsideRth={}", url, orh);
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
        let error_msg = body.get("error").and_then(|e| e.as_str())
            .unwrap_or("Failed to fetch historical data").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

// ============================================================================
// Scanner Commands
// ============================================================================

/// Get available scanner parameters
#[tauri::command]
pub async fn ibkr_get_scanner_params(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_scanner_params] Fetching scanner parameters");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/scanner/params", base_url))
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
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch scanner params".to_string()), timestamp })
    }
}

/// Run market scanner
#[tauri::command]
pub async fn ibkr_run_scanner(
    access_token: Option<String>,
    use_gateway: bool,
    scanner_params: Value,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_run_scanner] Running scanner");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/iserver/scanner/run", base_url))
        .headers(headers)
        .json(&scanner_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Scanner run failed".to_string()), timestamp })
    }
}
