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
                // Also emit a unified event so algo monitoring can listen to a single channel
                let _ = app.emit("algo_live_ticker", data);
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
    state: tauri::State<'_, WebSocketState>,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "upstox", Some(state.router.clone())));

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
            eprintln!("[angelone_ws_connect] WebSocket connected successfully");
            *ANGELONE_WS.write().await = Some(adapter);
            let _ = app.emit("angelone_status", json!({
                "provider": "angelone",
                "status": "connected",
                "timestamp": timestamp
            }));
            Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
        }
        Err(e) => {
            eprintln!("[angelone_ws_connect] WebSocket connection failed: {}", e);
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
                eprintln!("[angelone_ws_subscribe] Subscribed to {} ({})", symbol, channel);
                Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
            },
            Err(e) => {
                eprintln!("[angelone_ws_subscribe] Subscribe failed: {}", e);
                Ok(ApiResponse { success: false, data: Some(false), error: Some(e.to_string()), timestamp })
            },
        }
    } else {
        eprintln!("[angelone_ws_subscribe] Not connected");
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
// ANGEL ONE AUTO-CONNECT HELPER (called from algo_trading deploy)
// ============================================================================

/// Check if Angel One WebSocket is connected
pub async fn is_angelone_ws_connected() -> bool {
    ANGELONE_WS.read().await.is_some()
}

/// Connect Angel One WebSocket using stored credentials (no frontend needed).
/// Called from deploy_algo_strategy when provider is "angelone".
/// Returns Ok(true) if connected (or already connected), Err on failure.
pub async fn ensure_angelone_ws_connected(
    app: &tauri::AppHandle,
    router: Arc<tokio::sync::RwLock<MessageRouter>>,
) -> Result<bool, String> {
    // Already connected? Nothing to do.
    if ANGELONE_WS.read().await.is_some() {
        eprintln!("[ensure_angelone_ws] Already connected");
        return Ok(true);
    }

    eprintln!("[ensure_angelone_ws] Not connected — fetching stored credentials...");

    // Fetch credentials from encrypted store
    use crate::commands::broker_credentials::get_broker_credentials;
    let creds = get_broker_credentials("angelone".to_string())
        .await
        .map_err(|e| format!("Failed to fetch angelone credentials: {}", e))?;

    let api_key = creds.api_key
        .ok_or_else(|| "No api_key stored for angelone".to_string())?;
    let access_token = creds.access_token
        .ok_or_else(|| "No access_token stored for angelone".to_string())?;

    // feedToken is stored as JSON inside api_secret: {"password":"...", "totpSecret":"...", "feedToken":"..."}
    let api_secret_str = creds.api_secret
        .ok_or_else(|| "No api_secret stored for angelone".to_string())?;
    let secret_json: serde_json::Value = serde_json::from_str(&api_secret_str)
        .map_err(|e| format!("Failed to parse api_secret JSON: {} — raw: {}", e, &api_secret_str[..api_secret_str.len().min(100)]))?;
    let feed_token = secret_json.get("feedToken")
        .and_then(|v| v.as_str())
        .ok_or_else(|| "feedToken not found in api_secret JSON".to_string())?
        .to_string();

    // Get client_code from additional_data
    let client_code = creds.additional_data
        .as_ref()
        .and_then(|data| serde_json::from_str::<serde_json::Value>(data).ok())
        .and_then(|v| v.get("userId").and_then(|u| u.as_str()).map(String::from))
        .unwrap_or_default();

    if client_code.is_empty() {
        return Err("No userId/clientCode found in angelone credentials".to_string());
    }

    eprintln!("[ensure_angelone_ws] Credentials found: api_key_len={}, token_len={}, feed_token_len={}, client={}",
        api_key.len(), access_token.len(), feed_token.len(), client_code);

    // Build config and connect
    let mut extra = HashMap::new();
    extra.insert("feed_token".to_string(), serde_json::json!(feed_token));

    let config = ProviderConfig {
        name: "angelone".to_string(),
        url: "wss://smartapisocket.angelone.in/smart-stream".to_string(),
        api_key: Some(access_token),
        api_secret: Some(api_key),
        client_id: Some(client_code),
        extra: Some(extra),
        ..Default::default()
    };

    let mut adapter = AngelOneAdapter::new(config);
    adapter.set_message_callback(create_ws_callback(app.clone(), "angelone", Some(router)));

    eprintln!("[ensure_angelone_ws] Attempting WebSocket connection...");

    match adapter.connect().await {
        Ok(_) => {
            eprintln!("[ensure_angelone_ws] WebSocket connected successfully");
            *ANGELONE_WS.write().await = Some(adapter);
            let _ = app.emit("angelone_status", json!({
                "provider": "angelone",
                "status": "connected",
                "timestamp": chrono::Utc::now().timestamp_millis()
            }));
            Ok(true)
        }
        Err(e) => {
            eprintln!("[ensure_angelone_ws] WebSocket connection failed: {}", e);
            Err(format!("Angel One WS connection failed: {}", e))
        }
    }
}

/// Subscribe Angel One WebSocket to a symbol.
/// If token is provided directly, uses it. Otherwise looks up from symbol master.
/// symbol_name: e.g. "AVANTIFEEDS" (user-facing name for tick display)
/// exchange: e.g. "NSE"
/// token: e.g. Some("7936") - if provided, skips symbol master lookup
pub async fn ensure_angelone_ws_subscribed(
    symbol_name: &str,
    exchange: &str,
    direct_token: Option<&str>,
) -> Result<bool, String> {
    let token = if let Some(t) = direct_token {
        eprintln!("[ensure_angelone_ws_subscribed] Using provided token: {}", t);
        t.to_string()
    } else {
        // Look up token from symbol master
        use crate::database::symbol_master;
        symbol_master::get_token_by_symbol("angelone", symbol_name, exchange)
            .map_err(|e| format!("Symbol master lookup failed: {}", e))?
            .ok_or_else(|| format!("No token found for {} on {} in angelone symbol master", symbol_name, exchange))?
    };

    let ws_symbol = format!("{}:{}", exchange, token);
    eprintln!("[ensure_angelone_ws_subscribed] {} => {} (token={})", symbol_name, ws_symbol, token);

    let mut ws_guard = ANGELONE_WS.write().await;
    if let Some(ref mut adapter) = *ws_guard {
        let params = Some(serde_json::json!({ "name": symbol_name }));
        match adapter.subscribe(&ws_symbol, "ltp", params).await {
            Ok(_) => {
                eprintln!("[ensure_angelone_ws_subscribed] Subscribed to {} ({})", ws_symbol, symbol_name);
                Ok(true)
            }
            Err(e) => {
                eprintln!("[ensure_angelone_ws_subscribed] Subscribe failed: {}", e);
                Err(format!("Subscribe failed: {}", e))
            }
        }
    } else {
        Err("Angel One WS not connected".to_string())
    }
}

// ============================================================================
// KOTAK WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn kotak_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "kotak", Some(state.router.clone())));

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
    state: tauri::State<'_, WebSocketState>,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "groww", Some(state.router.clone())));

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
    state: tauri::State<'_, WebSocketState>,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "aliceblue", Some(state.router.clone())));

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

// ============================================================================
// IIFL WEBSOCKET COMMANDS
// ============================================================================

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

// ============================================================================
// MOTILAL OSWAL WEBSOCKET COMMANDS
// ============================================================================

#[tauri::command]
pub async fn motilal_ws_connect(
    app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
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
    adapter.set_message_callback(create_ws_callback(app.clone(), "motilal", Some(state.router.clone())));

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
