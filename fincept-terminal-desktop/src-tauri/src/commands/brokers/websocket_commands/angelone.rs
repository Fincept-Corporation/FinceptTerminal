// Angel One WebSocket Commands

use super::create_ws_callback;
use crate::websocket::adapters::{AngelOneAdapter, WebSocketAdapter};
use crate::websocket::router::MessageRouter;
use crate::websocket::types::ProviderConfig;
use crate::WebSocketState;
use once_cell::sync::Lazy;
use serde_json::json;
use std::collections::HashMap;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::RwLock;

use super::super::common::ApiResponse;

static ANGELONE_WS: Lazy<Arc<RwLock<Option<AngelOneAdapter>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

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
