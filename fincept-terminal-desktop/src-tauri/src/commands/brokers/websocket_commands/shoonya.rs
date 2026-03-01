// Shoonya WebSocket Commands

use super::create_ws_callback;
use crate::websocket::adapters::{ShoonyaAdapter, WebSocketAdapter};
use crate::websocket::types::ProviderConfig;
use crate::WebSocketState;
use once_cell::sync::Lazy;
use serde_json::json;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::RwLock;

use super::super::common::ApiResponse;

static SHOONYA_WS: Lazy<Arc<RwLock<Option<ShoonyaAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

#[tauri::command]
pub async fn shoonya_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    user_id: String,
    session_token: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "shoonya".to_string(),
        url: "wss://api.shoonya.com/NorenWSTP/".to_string(),
        api_key: Some(session_token),
        client_id: Some(user_id),
        ..Default::default()
    };

    let mut adapter = ShoonyaAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "shoonya", Some(state.router.clone())));

    match adapter.connect().await {
        Ok(_) => {
            *SHOONYA_WS.write().await = Some(adapter);
            let _ = app.emit("shoonya_status", json!({
                "provider": "shoonya",
                "status": "connected",
                "timestamp": timestamp
            }));
            Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
        }
        Err(e) => {
            Ok(ApiResponse { success: false, data: Some(false), error: Some(e.to_string()), timestamp })
        }
    }
}

#[tauri::command]
pub async fn shoonya_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = SHOONYA_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn shoonya_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = SHOONYA_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let channel = match mode.as_str() {
            "depth" => "depth",
            _ => "touchline",
        };
        match adapter.subscribe(&symbol, channel, None).await {
            Ok(_) => Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp }),
            Err(e) => Ok(ApiResponse { success: false, data: Some(false), error: Some(e.to_string()), timestamp }),
        }
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn shoonya_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = SHOONYA_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}
