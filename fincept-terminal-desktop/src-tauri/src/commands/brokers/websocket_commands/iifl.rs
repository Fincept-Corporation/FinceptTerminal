// IIFL WebSocket Commands

use super::create_ws_callback;
use crate::websocket::adapters::{IiflAdapter, WebSocketAdapter};
use crate::websocket::types::ProviderConfig;
use crate::WebSocketState;
use once_cell::sync::Lazy;
use serde_json::json;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::RwLock;

use super::super::common::ApiResponse;

static IIFL_WS: Lazy<Arc<RwLock<Option<IiflAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

#[tauri::command]
pub async fn iifl_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    app_key: String,
    secret_key: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "iifl".to_string(),
        url: "wss://ttblaze.iifl.com".to_string(),
        api_key: Some(app_key),
        api_secret: Some(secret_key),
        ..Default::default()
    };

    let mut adapter = IiflAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "iifl", Some(state.router.clone())));

    match adapter.connect().await {
        Ok(_) => {
            *IIFL_WS.write().await = Some(adapter);
            let _ = app.emit("iifl_status", json!({
                "provider": "iifl",
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
pub async fn iifl_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = IIFL_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn iifl_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = IIFL_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let channel = match mode.as_str() {
            "depth" => "depth",
            "quote" => "quote",
            _ => "ltp",
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
pub async fn iifl_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = IIFL_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}
