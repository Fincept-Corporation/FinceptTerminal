// Dhan WebSocket Commands

use super::create_ws_callback;
use crate::websocket::adapters::{DhanAdapter, WebSocketAdapter};
use crate::websocket::types::ProviderConfig;
use crate::WebSocketState;
use once_cell::sync::Lazy;
use serde_json::json;
use std::collections::HashMap;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::RwLock;

use super::super::common::ApiResponse;

static DHAN_WS: Lazy<Arc<RwLock<Option<DhanAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

#[tauri::command]
pub async fn dhan_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    client_id: String,
    access_token: String,
    is_20_depth: Option<bool>,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let mut extra = HashMap::new();
    if let Some(depth) = is_20_depth {
        extra.insert("is_20_depth".to_string(), serde_json::json!(depth));
    }

    let config = ProviderConfig {
        name: "dhan".to_string(),
        url: "wss://api-feed.dhan.co".to_string(),
        api_key: Some(access_token),
        client_id: Some(client_id),
        extra: Some(extra),
        ..Default::default()
    };

    let mut adapter = DhanAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "dhan", Some(state.router.clone())));

    match adapter.connect().await {
        Ok(_) => {
            *DHAN_WS.write().await = Some(adapter);
            let _ = app.emit("dhan_status", json!({
                "provider": "dhan",
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
pub async fn dhan_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = DHAN_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn dhan_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = DHAN_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let channel = match mode.as_str() {
            "full" | "depth" => "full",
            "quote" => "quote",
            "20depth" => "20depth",
            _ => "ticker",
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
pub async fn dhan_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = DHAN_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}
