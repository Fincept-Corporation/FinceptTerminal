// Fyers WebSocket Commands

use super::super::common::ApiResponse;
use crate::websocket::types::{MarketMessage, ProviderConfig};
use crate::websocket::adapters::WebSocketAdapter;
use crate::WebSocketState;
use once_cell::sync::Lazy;
use serde_json::json;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::RwLock;

static FYERS_WS: Lazy<Arc<RwLock<Option<crate::websocket::adapters::FyersAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

/// Connect to Fyers WebSocket with Tauri event emission
#[tauri::command]
pub async fn fyers_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    _api_key: String,
    access_token: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[fyers_ws_connect] WebSocket connection request");

    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "fyers".to_string(),
        url: "wss://socket.fyers.in/hsm/v1-5/prod".to_string(),
        api_key: Some(access_token.clone()),
        api_secret: None,
        client_id: None,
        enabled: true,
        reconnect_delay_ms: 5000,
        max_reconnect_attempts: 10,
        heartbeat_interval_ms: 30000,
        extra: None,
    };

    let mut adapter = crate::websocket::adapters::FyersAdapter::new(config);
    let router = state.router.clone();
    let app_handle = app.clone();

    adapter.set_message_callback(Box::new(move |msg: MarketMessage| {
        match &msg {
            MarketMessage::Ticker(data) => {
                let _ = app_handle.emit("fyers_ticker", data);
                let _ = app_handle.emit("algo_live_ticker", data);
            }
            MarketMessage::OrderBook(data) => { let _ = app_handle.emit("fyers_orderbook", data); }
            MarketMessage::Trade(data) => { let _ = app_handle.emit("fyers_trade", data); }
            MarketMessage::Status(data) => { let _ = app_handle.emit("fyers_status", data); }
            MarketMessage::Candle(data) => { let _ = app_handle.emit("fyers_candle", data); }
        }

        let router = router.clone();
        let msg = msg.clone();
        tokio::spawn(async move {
            router.read().await.route(msg).await;
        });
    }));

    match adapter.connect().await {
        Ok(_) => {
            let mut ws_guard = FYERS_WS.write().await;
            *ws_guard = Some(adapter);
            eprintln!("[fyers_ws_connect] WebSocket connected successfully");
            let _ = app.emit("fyers_status", json!({
                "provider": "fyers", "status": "connected",
                "message": "Connected to Fyers WebSocket",
                "timestamp": chrono::Utc::now().timestamp_millis()
            }));
            Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
        }
        Err(e) => {
            eprintln!("[fyers_ws_connect] WebSocket connection failed: {}", e);
            let _ = app.emit("fyers_status", json!({
                "provider": "fyers", "status": "error",
                "message": format!("Connection failed: {}", e),
                "timestamp": chrono::Utc::now().timestamp_millis()
            }));
            Ok(ApiResponse {
                success: false, data: Some(false),
                error: Some(format!("WebSocket connection failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Disconnect from Fyers WebSocket
#[tauri::command]
pub async fn fyers_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    eprintln!("[fyers_ws_disconnect] WebSocket disconnect request");

    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = FYERS_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        match adapter.disconnect().await {
            Ok(_) => {
                *ws_guard = None;
                eprintln!("[fyers_ws_disconnect] WebSocket disconnected successfully");
                Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
            }
            Err(e) => Ok(ApiResponse {
                success: false, data: Some(false),
                error: Some(format!("Disconnect failed: {}", e)),
                timestamp,
            }),
        }
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("WebSocket not connected".to_string()), timestamp })
    }
}

/// Subscribe to symbol via Fyers WebSocket
#[tauri::command]
pub async fn fyers_ws_subscribe(
    symbol: String,
    mode: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[fyers_ws_subscribe] Subscribing to {} in {} mode", symbol, mode);

    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = FYERS_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let channel = match mode.as_str() {
            "ltp" => "ltp",
            "quotes" | "quote" => "quotes",
            "depth" | "market_depth" => "depth",
            _ => "ltp",
        };

        match adapter.subscribe(&symbol, channel, None).await {
            Ok(_) => {
                eprintln!("[fyers_ws_subscribe] Successfully subscribed to {}", symbol);
                Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
            }
            Err(e) => Ok(ApiResponse {
                success: false, data: Some(false),
                error: Some(format!("Subscription failed: {}", e)),
                timestamp,
            }),
        }
    } else {
        Ok(ApiResponse {
            success: false, data: Some(false),
            error: Some("WebSocket not connected. Call fyers_ws_connect first.".to_string()),
            timestamp,
        })
    }
}

/// Unsubscribe from symbol via Fyers WebSocket
#[tauri::command]
pub async fn fyers_ws_unsubscribe(
    symbol: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[fyers_ws_unsubscribe] Unsubscribing from {}", symbol);

    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = FYERS_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        match adapter.unsubscribe(&symbol, "").await {
            Ok(_) => {
                eprintln!("[fyers_ws_unsubscribe] Successfully unsubscribed from {}", symbol);
                Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
            }
            Err(e) => Ok(ApiResponse {
                success: false, data: Some(false),
                error: Some(format!("Unsubscription failed: {}", e)),
                timestamp,
            }),
        }
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("WebSocket not connected".to_string()), timestamp })
    }
}

/// Batch subscribe to multiple symbols via Fyers WebSocket
#[tauri::command]
pub async fn fyers_ws_subscribe_batch(
    symbols: Vec<String>,
    mode: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[fyers_ws_subscribe_batch] Subscribing to {} symbols in {} mode", symbols.len(), mode);

    let timestamp = chrono::Utc::now().timestamp_millis();
    let start_time = std::time::Instant::now();
    let mut ws_guard = FYERS_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let data_type = match mode.as_str() {
            "depth" | "market_depth" | "full" => crate::websocket::adapters::fyers::FyersDataType::DepthUpdate,
            "index" => crate::websocket::adapters::fyers::FyersDataType::IndexUpdate,
            _ => crate::websocket::adapters::fyers::FyersDataType::SymbolUpdate,
        };

        match adapter.subscribe_batch(&symbols, data_type).await {
            Ok(_) => {
                let elapsed = start_time.elapsed();
                eprintln!("[fyers_ws_subscribe_batch] ✓ Subscribed to {} symbols in {:?}", symbols.len(), elapsed);
                Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
            }
            Err(e) => {
                eprintln!("[fyers_ws_subscribe_batch] ✗ Batch subscription failed: {}", e);
                Ok(ApiResponse {
                    success: false, data: Some(false),
                    error: Some(format!("Batch subscription failed: {}", e)),
                    timestamp,
                })
            }
        }
    } else {
        Ok(ApiResponse {
            success: false, data: Some(false),
            error: Some("WebSocket not connected. Call fyers_ws_connect first.".to_string()),
            timestamp,
        })
    }
}

/// Check Fyers WebSocket connection status
#[tauri::command]
pub async fn fyers_ws_is_connected() -> Result<bool, String> {
    let ws_guard = FYERS_WS.read().await;
    if let Some(ref adapter) = *ws_guard { Ok(adapter.is_connected()) } else { Ok(false) }
}
