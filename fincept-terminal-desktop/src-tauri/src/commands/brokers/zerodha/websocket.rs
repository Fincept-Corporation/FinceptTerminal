// Zerodha WebSocket Commands

use super::super::common::ApiResponse;
use super::auth::zerodha_validate_token;
use crate::websocket::adapters::WebSocketAdapter;
use crate::websocket::types::MarketMessage;
use crate::WebSocketState;
use once_cell::sync::Lazy;
use serde_json::json;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::RwLock;

static ZERODHA_WS: Lazy<Arc<RwLock<Option<crate::websocket::adapters::ZerodhaAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

/// Connect to Zerodha WebSocket with Tauri event emission
#[tauri::command]
pub async fn zerodha_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[zerodha_ws_connect] WebSocket connection request");

    let timestamp = chrono::Utc::now().timestamp_millis();

    let validate_result = zerodha_validate_token(api_key.clone(), access_token.clone()).await?;
    if !validate_result.success {
        return Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Invalid credentials for WebSocket".to_string()),
            timestamp,
        });
    }

    let mut adapter = crate::websocket::adapters::ZerodhaAdapter::new_with_credentials(
        api_key,
        access_token,
    );

    let router = state.router.clone();
    let app_handle = app.clone();

    adapter.set_message_callback(Box::new(move |msg: MarketMessage| {
        match &msg {
            MarketMessage::Ticker(data) => {
                let _ = app_handle.emit("zerodha_ticker", data);
                let _ = app_handle.emit("algo_live_ticker", data);
            }
            MarketMessage::OrderBook(data) => { let _ = app_handle.emit("zerodha_orderbook", data); }
            MarketMessage::Trade(data) => { let _ = app_handle.emit("zerodha_trade", data); }
            MarketMessage::Status(data) => { let _ = app_handle.emit("zerodha_status", data); }
            MarketMessage::Candle(data) => { let _ = app_handle.emit("zerodha_candle", data); }
        }

        let router = router.clone();
        let msg = msg.clone();
        tokio::spawn(async move {
            router.read().await.route(msg).await;
        });
    }));

    match adapter.connect().await {
        Ok(_) => {
            let mut ws_guard = ZERODHA_WS.write().await;
            *ws_guard = Some(adapter);

            eprintln!("[zerodha_ws_connect] WebSocket connected successfully");
            let _ = app.emit("zerodha_status", json!({
                "provider": "zerodha",
                "status": "connected",
                "message": "Connected to Zerodha KiteTicker",
                "timestamp": chrono::Utc::now().timestamp_millis()
            }));

            Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
        }
        Err(e) => {
            eprintln!("[zerodha_ws_connect] WebSocket connection failed: {}", e);
            let _ = app.emit("zerodha_status", json!({
                "provider": "zerodha",
                "status": "error",
                "message": format!("Connection failed: {}", e),
                "timestamp": chrono::Utc::now().timestamp_millis()
            }));
            Ok(ApiResponse {
                success: false,
                data: Some(false),
                error: Some(format!("WebSocket connection failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Disconnect from Zerodha WebSocket
#[tauri::command]
pub async fn zerodha_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    eprintln!("[zerodha_ws_disconnect] WebSocket disconnect request");

    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = ZERODHA_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        match adapter.disconnect().await {
            Ok(_) => {
                *ws_guard = None;
                Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
            }
            Err(e) => Ok(ApiResponse {
                success: false,
                data: Some(false),
                error: Some(format!("Disconnect failed: {}", e)),
                timestamp,
            }),
        }
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("WebSocket not connected".to_string()), timestamp })
    }
}

/// Subscribe to symbols via WebSocket
#[tauri::command]
pub async fn zerodha_ws_subscribe(
    tokens: Vec<i64>,
    mode: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[zerodha_ws_subscribe] Subscribing to {} tokens in {} mode", tokens.len(), mode);

    let timestamp = chrono::Utc::now().timestamp_millis();
    let ws_guard = ZERODHA_WS.read().await;

    if let Some(ref adapter) = *ws_guard {
        match adapter.subscribe_tokens(tokens.clone(), &mode).await {
            Ok(_) => Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp }),
            Err(e) => Ok(ApiResponse {
                success: false,
                data: Some(false),
                error: Some(format!("Subscription failed: {}", e)),
                timestamp,
            }),
        }
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("WebSocket not connected. Call zerodha_ws_connect first.".to_string()),
            timestamp,
        })
    }
}

/// Unsubscribe from symbols via WebSocket
#[tauri::command]
pub async fn zerodha_ws_unsubscribe(
    tokens: Vec<i64>,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[zerodha_ws_unsubscribe] Unsubscribing from {} tokens", tokens.len());

    let timestamp = chrono::Utc::now().timestamp_millis();
    let ws_guard = ZERODHA_WS.read().await;

    if let Some(ref adapter) = *ws_guard {
        match adapter.unsubscribe_tokens(tokens.clone()).await {
            Ok(_) => Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp }),
            Err(e) => Ok(ApiResponse {
                success: false,
                data: Some(false),
                error: Some(format!("Unsubscription failed: {}", e)),
                timestamp,
            }),
        }
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("WebSocket not connected".to_string()), timestamp })
    }
}

/// Set token-to-symbol mapping for tick data resolution
#[tauri::command]
pub async fn zerodha_ws_set_token_map(
    token: i64,
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let ws_guard = ZERODHA_WS.read().await;

    if let Some(ref adapter) = *ws_guard {
        adapter.set_token_symbol_map(token, symbol.clone(), exchange.clone());
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("WebSocket not connected".to_string()), timestamp })
    }
}

/// Check WebSocket connection status
#[tauri::command]
pub async fn zerodha_ws_is_connected() -> Result<bool, String> {
    let ws_guard = ZERODHA_WS.read().await;
    if let Some(ref adapter) = *ws_guard {
        Ok(adapter.is_connected())
    } else {
        Ok(false)
    }
}
