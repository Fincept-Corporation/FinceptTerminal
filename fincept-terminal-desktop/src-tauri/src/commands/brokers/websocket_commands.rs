// WebSocket Commands for All Indian Stock Brokers
//
// Provides unified Tauri commands for WebSocket operations across all brokers.
// Each broker has: {broker}_ws_connect, {broker}_ws_disconnect, {broker}_ws_subscribe, {broker}_ws_unsubscribe

use crate::websocket::adapters::*;
use crate::websocket::router::MessageRouter;
use crate::websocket::types::{MarketMessage, ProviderConfig};
use crate::WebSocketState;
use once_cell::sync::Lazy;
use serde_json::json;
use std::collections::HashMap;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::RwLock;

use super::common::ApiResponse;

// ============================================================================
// GLOBAL WEBSOCKET INSTANCES
// ============================================================================

static UPSTOX_WS: Lazy<Arc<RwLock<Option<UpstoxAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static DHAN_WS: Lazy<Arc<RwLock<Option<DhanAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static ANGELONE_WS: Lazy<Arc<RwLock<Option<AngelOneAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static KOTAK_WS: Lazy<Arc<RwLock<Option<KotakAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static GROWW_WS: Lazy<Arc<RwLock<Option<GrowwAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static ALICEBLUE_WS: Lazy<Arc<RwLock<Option<AliceBlueAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static FIVEPAISA_WS: Lazy<Arc<RwLock<Option<FivePaisaAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static IIFL_WS: Lazy<Arc<RwLock<Option<IiflAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static MOTILAL_WS: Lazy<Arc<RwLock<Option<MotilalAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

static SHOONYA_WS: Lazy<Arc<RwLock<Option<ShoonyaAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

// ============================================================================
// HELPER MACROS
// ============================================================================

/// Creates WebSocket event callback for a broker.
/// Emits broker-specific events to frontend AND routes through the shared MessageRouter
/// so backend services (monitoring, paper trading, etc.) also receive the ticks.
fn create_ws_callback(
    app: tauri::AppHandle,
    broker: &'static str,
    router: Option<Arc<tokio::sync::RwLock<MessageRouter>>>,
) -> Box<dyn Fn(MarketMessage) + Send + Sync> {
    Box::new(move |msg: MarketMessage| {
        // 1. Emit broker-specific events to frontend
        match &msg {
            MarketMessage::Ticker(data) => {
                let _ = app.emit(&format!("{}_ticker", broker), data);
            }
            MarketMessage::OrderBook(data) => {
                let _ = app.emit(&format!("{}_orderbook", broker), data);
            }
            MarketMessage::Trade(data) => {
                let _ = app.emit(&format!("{}_trade", broker), data);
            }
            MarketMessage::Status(data) => {
                let _ = app.emit(&format!("{}_status", broker), data);
            }
            MarketMessage::Candle(data) => {
                let _ = app.emit(&format!("{}_candle", broker), data);
            }
        }

        // 2. Route through MessageRouter for backend services (monitoring, paper trading, etc.)
        if let Some(ref router) = router {
            let router = router.clone();
            let msg = msg.clone();
            tokio::spawn(async move {
                router.read().await.route(msg).await;
            });
        }
    })
}

// ============================================================================
// UPSTOX WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn upstox_ws_connect(
    app: tauri::AppHandle,
    access_token: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "upstox".to_string(),
        url: "wss://api.upstox.com/v3/feed/market-data-feed".to_string(),
        api_key: Some(access_token),
        ..Default::default()
    };

    let mut adapter = UpstoxAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "upstox", None));

    match adapter.connect().await {
        Ok(_) => {
            *UPSTOX_WS.write().await = Some(adapter);
            let _ = app.emit("upstox_status", json!({
                "provider": "upstox",
                "status": "connected",
                "timestamp": timestamp
            }));
            Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
        }
        Err(e) => {
            let _ = app.emit("upstox_status", json!({
                "provider": "upstox",
                "status": "error",
                "message": e.to_string(),
                "timestamp": timestamp
            }));
            Ok(ApiResponse { success: false, data: Some(false), error: Some(e.to_string()), timestamp })
        }
    }
}

#[tauri::command]
pub async fn upstox_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = UPSTOX_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn upstox_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = UPSTOX_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let channel = match mode.as_str() {
            "full" | "depth" => "full",
            _ => "ltpc",
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
pub async fn upstox_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = UPSTOX_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

// ============================================================================
// DHAN WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn dhan_ws_connect(
    app: tauri::AppHandle,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "dhan", None));

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

// ============================================================================
// ANGEL ONE WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn angelone_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    auth_token: String,
    api_key: String,
    client_code: String,
    feed_token: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    eprintln!("[angelone_ws_connect] Called: client_code={}, auth_token_len={}, api_key_len={}, feed_token_len={}",
        client_code, auth_token.len(), api_key.len(), feed_token.len());

    let mut extra = HashMap::new();
    extra.insert("feed_token".to_string(), serde_json::json!(feed_token));

    let config = ProviderConfig {
        name: "angelone".to_string(),
        url: "wss://smartapisocket.angelone.in/smart-stream".to_string(),
        api_key: Some(auth_token),
        api_secret: Some(api_key),
        client_id: Some(client_code),
        extra: Some(extra),
        ..Default::default()
    };

    let router = state.router.clone();
    let mut adapter = AngelOneAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "angelone", Some(router)));

    eprintln!("[angelone_ws_connect] Attempting WebSocket connection...");

    match adapter.connect().await {
        Ok(_) => {
            eprintln!("[angelone_ws_connect] ✓ WebSocket connected successfully");
            *ANGELONE_WS.write().await = Some(adapter);
            let _ = app.emit("angelone_status", json!({
                "provider": "angelone",
                "status": "connected",
                "timestamp": timestamp
            }));
            Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
        }
        Err(e) => {
            eprintln!("[angelone_ws_connect] ✗ WebSocket connection failed: {}", e);
            Ok(ApiResponse { success: false, data: Some(false), error: Some(e.to_string()), timestamp })
        }
    }
}

#[tauri::command]
pub async fn angelone_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = ANGELONE_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn angelone_ws_subscribe(symbol: String, mode: String, symbol_name: Option<String>) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    eprintln!("[angelone_ws_subscribe] symbol={}, mode={}, name={:?}", symbol, mode, symbol_name);
    let mut ws_guard = ANGELONE_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let channel = match mode.as_str() {
            "depth" => "depth",
            "quote" => "quote",
            "snap" | "snap_quote" => "snap",
            _ => "ltp",
        };
        // Pass the human-readable name so the adapter can use it in tick messages
        let params = symbol_name.map(|n| serde_json::json!({ "name": n }));
        match adapter.subscribe(&symbol, channel, params).await {
            Ok(_) => {
                eprintln!("[angelone_ws_subscribe] ✓ Subscribed to {} ({})", symbol, channel);
                Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
            },
            Err(e) => {
                eprintln!("[angelone_ws_subscribe] ✗ Subscribe failed: {}", e);
                Ok(ApiResponse { success: false, data: Some(false), error: Some(e.to_string()), timestamp })
            },
        }
    } else {
        eprintln!("[angelone_ws_subscribe] ✗ Not connected");
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn angelone_ws_unsubscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = ANGELONE_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, &mode).await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

// ============================================================================
// KOTAK WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn kotak_ws_connect(
    app: tauri::AppHandle,
    access_token: String,
    consumer_key: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "kotak".to_string(),
        url: "wss://livefeeds.kotaksecurities.com".to_string(),
        api_key: Some(access_token),
        api_secret: Some(consumer_key),
        ..Default::default()
    };

    let mut adapter = KotakAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "kotak", None));

    match adapter.connect().await {
        Ok(_) => {
            *KOTAK_WS.write().await = Some(adapter);
            let _ = app.emit("kotak_status", json!({
                "provider": "kotak",
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
pub async fn kotak_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = KOTAK_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn kotak_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = KOTAK_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        match adapter.subscribe(&symbol, &mode, None).await {
            Ok(_) => Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp }),
            Err(e) => Ok(ApiResponse { success: false, data: Some(false), error: Some(e.to_string()), timestamp }),
        }
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn kotak_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = KOTAK_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

// ============================================================================
// GROWW WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn groww_ws_connect(
    app: tauri::AppHandle,
    auth_token: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "groww".to_string(),
        url: "wss://socket-api.groww.in".to_string(),
        api_key: Some(auth_token),
        ..Default::default()
    };

    let mut adapter = GrowwAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "groww", None));

    match adapter.connect().await {
        Ok(_) => {
            *GROWW_WS.write().await = Some(adapter);
            let _ = app.emit("groww_status", json!({
                "provider": "groww",
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
pub async fn groww_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = GROWW_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn groww_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = GROWW_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let channel = match mode.as_str() {
            "depth" => "depth",
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
pub async fn groww_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = GROWW_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

// ============================================================================
// ALICE BLUE WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn aliceblue_ws_connect(
    app: tauri::AppHandle,
    user_id: String,
    session_token: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "aliceblue".to_string(),
        url: "wss://ws1.aliceblueonline.com/NorenWS/".to_string(),
        api_key: Some(session_token),
        client_id: Some(user_id),
        ..Default::default()
    };

    let mut adapter = AliceBlueAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "aliceblue", None));

    match adapter.connect().await {
        Ok(_) => {
            *ALICEBLUE_WS.write().await = Some(adapter);
            let _ = app.emit("aliceblue_status", json!({
                "provider": "aliceblue",
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
pub async fn aliceblue_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = ALICEBLUE_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn aliceblue_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = ALICEBLUE_WS.write().await;

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
pub async fn aliceblue_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = ALICEBLUE_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

// ============================================================================
// 5PAISA WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn fivepaisa_ws_connect(
    app: tauri::AppHandle,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "fivepaisa", None));

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

// ============================================================================
// IIFL WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn iifl_ws_connect(
    app: tauri::AppHandle,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "iifl", None));

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

// ============================================================================
// MOTILAL OSWAL WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn motilal_ws_connect(
    app: tauri::AppHandle,
    access_token: String,
) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let config = ProviderConfig {
        name: "motilal".to_string(),
        url: "wss://wsapi.motilaloswal.com/".to_string(),
        api_key: Some(access_token),
        ..Default::default()
    };

    let mut adapter = MotilalAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "motilal", None));

    match adapter.connect().await {
        Ok(_) => {
            *MOTILAL_WS.write().await = Some(adapter);
            let _ = app.emit("motilal_status", json!({
                "provider": "motilal",
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
pub async fn motilal_ws_disconnect() -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = MOTILAL_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.disconnect().await;
        *ws_guard = None;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

#[tauri::command]
pub async fn motilal_ws_subscribe(symbol: String, mode: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = MOTILAL_WS.write().await;

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
pub async fn motilal_ws_unsubscribe(symbol: String) -> Result<ApiResponse<bool>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let mut ws_guard = MOTILAL_WS.write().await;

    if let Some(ref mut adapter) = *ws_guard {
        let _ = adapter.unsubscribe(&symbol, "").await;
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Not connected".to_string()), timestamp })
    }
}

// ============================================================================
// SHOONYA WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn shoonya_ws_connect(
    app: tauri::AppHandle,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "shoonya", None));

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
