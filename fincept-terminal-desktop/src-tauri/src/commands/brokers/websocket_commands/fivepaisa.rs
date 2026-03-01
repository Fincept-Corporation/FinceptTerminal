// 5Paisa WebSocket Commands

use super::create_ws_callback;
use crate::websocket::adapters::{FivePaisaAdapter, WebSocketAdapter};
use crate::websocket::types::ProviderConfig;
use crate::WebSocketState;
use once_cell::sync::Lazy;
use serde_json::json;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::RwLock;

use super::super::common::ApiResponse;

static FIVEPAISA_WS: Lazy<Arc<RwLock<Option<FivePaisaAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

#[tauri::command]
pub async fn fivepaisa_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    client_code: String,
    jwt_token: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "fivepaisa".to_string(),
        url: "wss://openfeed.5paisa.com/Feeds/api/chat".to_string(),
        api_key: Some(jwt_token),
        client_id: Some(client_code),
        ..Default::default()
    };

    let mut adapter = FivePaisaAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "fivepaisa", Some(state.router.clone())));

    match adapter.connect().await {
        Ok(_) => {
            *FIVEPAISA_WS.write().await = Some(adapter);
            let _ = app.emit("fivepaisa_status", json!({
                "provider": "fivepaisa",
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
pub async fn fivepaisa_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = FIVEPAISA_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn fivepaisa_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = FIVEPAISA_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let channel = match mode.as_str() {
            "depth" => "depth",
            "index" => "index",
            "oi" => "oi",
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
pub async fn fivepaisa_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = FIVEPAISA_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}
