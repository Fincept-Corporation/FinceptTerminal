// Tradier WebSocket & Streaming Commands

use super::{get_api_base, create_tradier_headers, extract_tradier_error};
use super::super::common::ApiResponse;
use serde_json::Value;

/// Get streaming session ID
#[tauri::command]
pub async fn tradier_get_stream_session(
    token: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[tradier_get_stream_session] Getting stream session");

    let client = reqwest::Client::new();
    let headers = create_tradier_headers(&token);
    let base_url = get_api_base(is_paper);

    let response = client.post(format!("{}/markets/events/session", base_url)).headers(headers).send().await
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

/// Connect to WebSocket stream
#[tauri::command]
pub async fn tradier_ws_connect(
    session_id: String,
    _is_paper: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[tradier_ws_connect] Connecting to WebSocket with session: {}", session_id);

    // TODO: Implement actual WebSocket connection
    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
}

/// Subscribe to symbols on WebSocket
#[tauri::command]
pub async fn tradier_ws_subscribe(
    _session_id: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[tradier_ws_subscribe] Subscribing to: {:?}", symbols);

    // TODO: Implement WebSocket subscription
    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
}

/// Unsubscribe from symbols on WebSocket
#[tauri::command]
pub async fn tradier_ws_unsubscribe(
    _session_id: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[tradier_ws_unsubscribe] Unsubscribing from: {:?}", symbols);

    // TODO: Implement WebSocket unsubscription
    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
}

/// Disconnect from WebSocket
#[tauri::command]
pub async fn tradier_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    eprintln!("[tradier_ws_disconnect] Disconnecting from WebSocket");

    // TODO: Implement WebSocket disconnection
    let timestamp = chrono::Utc::now().timestamp_millis();
    Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
}
