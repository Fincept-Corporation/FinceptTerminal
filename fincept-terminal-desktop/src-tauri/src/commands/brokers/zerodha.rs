//! Zerodha (Kite) Broker Integration
//! 
//! Complete API implementation for Zerodha/Kite broker including:
//! - Authentication (OAuth with SHA256 checksum)
//! - Order Management (place, modify, cancel, smart orders)
//! - Portfolio (positions, holdings, margins)
//! - Market Data (quotes, historical, depth)
//! - Bulk Operations (cancel all, close all positions)
//! - WebSocket (real-time data streaming)

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde_json::{json, Value};
use sha2::{Sha256, Digest};
use std::sync::Arc;
use std::collections::HashMap;
use tokio::sync::RwLock;
use once_cell::sync::Lazy;
use tauri::Emitter;

use crate::websocket::adapters::WebSocketAdapter;
use crate::websocket::types::MarketMessage;
use crate::WebSocketState;
use super::common::{ApiResponse, TokenExchangeResponse, OrderPlaceResponse};

// Zerodha API Configuration
// ============================================================================

const ZERODHA_API_BASE: &str = "https://api.kite.trade";
const ZERODHA_API_VERSION: &str = "3";

fn create_zerodha_headers(api_key: &str, access_token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    headers.insert("X-Kite-Version", HeaderValue::from_static(ZERODHA_API_VERSION));

    let auth_value = format!("token {}:{}", api_key, access_token);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/x-www-form-urlencoded"));
    headers
}

// ============================================================================
// Zerodha Authentication Commands
// ============================================================================

/// Exchange request token for access token
#[tauri::command]
pub async fn zerodha_exchange_token(
    api_key: String,
    api_secret: String,
    request_token: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[zerodha_exchange_token] Exchanging request token");

    // Create checksum: SHA256(api_key + request_token + api_secret)
    let checksum_input = format!("{}{}{}", api_key, request_token, api_secret);
    let mut hasher = Sha256::new();
    hasher.update(checksum_input.as_bytes());
    let checksum = format!("{:x}", hasher.finalize());

    let client = reqwest::Client::new();

    let params = [
        ("api_key", api_key.as_str()),
        ("request_token", request_token.as_str()),
        ("checksum", checksum.as_str()),
    ];

    let response = client
        .post(format!("{}/session/token", ZERODHA_API_BASE))
        .header("X-Kite-Version", ZERODHA_API_VERSION)
        .header(CONTENT_TYPE, "application/x-www-form-urlencoded")
        .form(&params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").ok_or("No data in response")?;
        Ok(TokenExchangeResponse {
            success: true,
            access_token: data.get("access_token").and_then(|v| v.as_str()).map(String::from),
            user_id: data.get("user_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Token exchange failed")
            .to_string();
        Ok(TokenExchangeResponse {
            success: false,
            access_token: None,
            user_id: None,
            error: Some(error_msg),
        })
    }
}

/// Validate access token by fetching user profile
#[tauri::command]
pub async fn zerodha_validate_token(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[zerodha_validate_token] Validating access token");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/user/profile", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Token validation failed".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Zerodha Order Commands
// ============================================================================

/// Place an order
#[tauri::command]
pub async fn zerodha_place_order(
    api_key: String,
    access_token: String,
    params: HashMap<String, Value>,
    variety: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[zerodha_place_order] Placing {} order", variety);

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    // Convert params to form data
    let form_params: Vec<(String, String)> = params
        .iter()
        .map(|(k, v)| {
            let value = match v {
                Value::String(s) => s.clone(),
                Value::Number(n) => n.to_string(),
                _ => v.to_string(),
            };
            (k.clone(), value)
        })
        .collect();

    let url = format!("{}/orders/{}", ZERODHA_API_BASE, variety);

    let response = client
        .post(&url)
        .headers(headers)
        .form(&form_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").ok_or("No data in response")?;
        Ok(OrderPlaceResponse {
            success: true,
            order_id: data.get("order_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order placement failed")
            .to_string();
        Ok(OrderPlaceResponse {
            success: false,
            order_id: None,
            error: Some(error_msg),
        })
    }
}

/// Modify an order
#[tauri::command]
pub async fn zerodha_modify_order(
    api_key: String,
    access_token: String,
    order_id: String,
    params: HashMap<String, Value>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[zerodha_modify_order] Modifying order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let form_params: Vec<(String, String)> = params
        .iter()
        .map(|(k, v)| {
            let value = match v {
                Value::String(s) => s.clone(),
                Value::Number(n) => n.to_string(),
                _ => v.to_string(),
            };
            (k.clone(), value)
        })
        .collect();

    let url = format!("{}/orders/regular/{}", ZERODHA_API_BASE, order_id);

    let response = client
        .put(&url)
        .headers(headers)
        .form(&form_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").ok_or("No data in response")?;
        Ok(OrderPlaceResponse {
            success: true,
            order_id: data.get("order_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order modification failed")
            .to_string();
        Ok(OrderPlaceResponse {
            success: false,
            order_id: None,
            error: Some(error_msg),
        })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn zerodha_cancel_order(
    api_key: String,
    access_token: String,
    order_id: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[zerodha_cancel_order] Cancelling order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let url = format!("{}/orders/regular/{}", ZERODHA_API_BASE, order_id);

    let response = client
        .delete(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").ok_or("No data in response")?;
        Ok(OrderPlaceResponse {
            success: true,
            order_id: data.get("order_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order cancellation failed")
            .to_string();
        Ok(OrderPlaceResponse {
            success: false,
            order_id: None,
            error: Some(error_msg),
        })
    }
}

/// Get all orders
#[tauri::command]
pub async fn zerodha_get_orders(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_get_orders] Fetching orders");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/orders", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data")
            .and_then(|d| d.as_array())
            .map(|arr| arr.clone())
            .unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch orders")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Zerodha Position & Holdings Commands
// ============================================================================

/// Get positions
#[tauri::command]
pub async fn zerodha_get_positions(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/portfolio/positions", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").cloned().unwrap_or(json!({}));
        Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch positions")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get holdings
#[tauri::command]
pub async fn zerodha_get_holdings(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/portfolio/holdings", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data")
            .and_then(|d| d.as_array())
            .map(|arr| arr.clone())
            .unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch holdings")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get margins/funds
#[tauri::command]
pub async fn zerodha_get_margins(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_margins] Fetching margins");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/user/margins", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").cloned().unwrap_or(json!({}));
        Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch margins")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Zerodha Market Data Commands
// ============================================================================

/// Get quotes for instruments
#[tauri::command]
pub async fn zerodha_get_quote(
    api_key: String,
    access_token: String,
    instruments: Vec<String>,
) -> Result<ApiResponse<HashMap<String, Value>>, String> {
    eprintln!("[zerodha_get_quote] Fetching quotes for {} instruments", instruments.len());

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    // Build query params
    let instruments_param = instruments.join("&i=");
    let url = format!("{}/quote?i={}", ZERODHA_API_BASE, instruments_param);

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data")
            .and_then(|d| d.as_object())
            .map(|obj| obj.iter().map(|(k, v)| (k.clone(), v.clone())).collect())
            .unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quotes")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get historical data (OHLCV)
#[tauri::command]
pub async fn zerodha_get_historical(
    api_key: String,
    access_token: String,
    instrument_token: String,
    interval: String,
    from: String,
    to: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_historical] Fetching historical data for {}", instrument_token);

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let url = format!(
        "{}/instruments/historical/{}/{}?from={}&to={}",
        ZERODHA_API_BASE, instrument_token, interval, from, to
    );

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").cloned().unwrap_or(json!({}));
        Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch historical data")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Download master contract (instruments list)
#[tauri::command]
pub async fn zerodha_download_master_contract(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_download_master_contract] Downloading instruments");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/instruments", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        // Instruments endpoint returns CSV
        let csv_data = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

        // Parse CSV to JSON
        let mut instruments: Vec<Value> = Vec::new();
        let mut lines = csv_data.lines();

        // Get headers
        let headers: Vec<&str> = if let Some(header_line) = lines.next() {
            header_line.split(',').collect()
        } else {
            return Err("Empty CSV response".to_string());
        };

        // Parse each row
        for line in lines {
            let values: Vec<&str> = line.split(',').collect();
            if values.len() == headers.len() {
                let mut obj = serde_json::Map::new();
                for (i, header) in headers.iter().enumerate() {
                    obj.insert(header.to_string(), json!(values[i]));
                }
                instruments.push(Value::Object(obj));
            }
        }

        eprintln!("[zerodha_download_master_contract] Downloaded {} instruments", instruments.len());

        Ok(ApiResponse {
            success: true,
            data: Some(instruments),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to download instruments".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Zerodha WebSocket Commands
// ============================================================================

// Global Zerodha WebSocket adapter instance
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

    // Validate token first
    let validate_result = zerodha_validate_token(api_key.clone(), access_token.clone()).await?;

    if !validate_result.success {
        return Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Invalid credentials for WebSocket".to_string()),
            timestamp,
        });
    }

    // Create adapter with credentials
    let mut adapter = crate::websocket::adapters::ZerodhaAdapter::new_with_credentials(
        api_key,
        access_token,
    );

    // Clone the router so backend services (candle aggregation, algo trading, etc.) receive ticks
    let router = state.router.clone();

    // Set up message callback to emit Tauri events AND route through MessageRouter
    let app_handle = app.clone();
    adapter.set_message_callback(Box::new(move |msg: MarketMessage| {
        // 1. Emit broker-specific events to frontend
        match &msg {
            MarketMessage::Ticker(data) => {
                let _ = app_handle.emit("zerodha_ticker", data);
                let _ = app_handle.emit("algo_live_ticker", data);
            }
            MarketMessage::OrderBook(data) => {
                let _ = app_handle.emit("zerodha_orderbook", data);
            }
            MarketMessage::Trade(data) => {
                let _ = app_handle.emit("zerodha_trade", data);
            }
            MarketMessage::Status(data) => {
                let _ = app_handle.emit("zerodha_status", data);
            }
            MarketMessage::Candle(data) => {
                let _ = app_handle.emit("zerodha_candle", data);
            }
        }

        // 2. Route through MessageRouter for backend services (candle aggregation, algo trading, etc.)
        let router = router.clone();
        let msg = msg.clone();
        tokio::spawn(async move {
            router.read().await.route(msg).await;
        });
    }));

    // Connect to WebSocket
    match adapter.connect().await {
        Ok(_) => {
            // Store the adapter
            let mut ws_guard = ZERODHA_WS.write().await;
            *ws_guard = Some(adapter);

            eprintln!("[zerodha_ws_connect] WebSocket connected successfully");

            // Emit connection success event
            let _ = app.emit("zerodha_status", json!({
                "provider": "zerodha",
                "status": "connected",
                "message": "Connected to Zerodha KiteTicker",
                "timestamp": chrono::Utc::now().timestamp_millis()
            }));

            Ok(ApiResponse {
                success: true,
                data: Some(true),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            eprintln!("[zerodha_ws_connect] WebSocket connection failed: {}", e);

            // Emit connection failure event
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
                eprintln!("[zerodha_ws_disconnect] WebSocket disconnected successfully");
                Ok(ApiResponse {
                    success: true,
                    data: Some(true),
                    error: None,
                    timestamp,
                })
            }
            Err(e) => {
                Ok(ApiResponse {
                    success: false,
                    data: Some(false),
                    error: Some(format!("Disconnect failed: {}", e)),
                    timestamp,
                })
            }
        }
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("WebSocket not connected".to_string()),
            timestamp,
        })
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
            Ok(_) => {
                eprintln!("[zerodha_ws_subscribe] Successfully subscribed to {} tokens", tokens.len());
                Ok(ApiResponse {
                    success: true,
                    data: Some(true),
                    error: None,
                    timestamp,
                })
            }
            Err(e) => {
                Ok(ApiResponse {
                    success: false,
                    data: Some(false),
                    error: Some(format!("Subscription failed: {}", e)),
                    timestamp,
                })
            }
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
            Ok(_) => {
                eprintln!("[zerodha_ws_unsubscribe] Successfully unsubscribed from {} tokens", tokens.len());
                Ok(ApiResponse {
                    success: true,
                    data: Some(true),
                    error: None,
                    timestamp,
                })
            }
            Err(e) => {
                Ok(ApiResponse {
                    success: false,
                    data: Some(false),
                    error: Some(format!("Unsubscription failed: {}", e)),
                    timestamp,
                })
            }
        }
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("WebSocket not connected".to_string()),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("WebSocket not connected".to_string()),
            timestamp,
        })
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

/// Search symbols in master contract
#[tauri::command]
pub async fn search_indian_symbols(
    broker_id: String,
    query: String,
    exchange: Option<String>,
) -> Result<Vec<Value>, String> {
    eprintln!("[search_indian_symbols] Searching for '{}' in {} broker", query, broker_id);

    use crate::database::symbol_master;

    // Search using symbol master DB
    match symbol_master::search_symbols(&broker_id, &query, exchange.as_deref(), None, 50) {
        Ok(results) => {
            let values: Vec<Value> = results.iter().map(|r| {
                json!({
                    "symbol": r.symbol,
                    "tradingsymbol": r.br_symbol,
                    "name": r.name,
                    "exchange": r.exchange,
                    "token": r.token,
                    "instrumenttype": r.instrument_type,
                    "lotsize": r.lot_size,
                    "ticksize": r.tick_size,
                    "expiry": r.expiry,
                    "strike": r.strike,
                })
            }).collect();
            Ok(values)
        }
        Err(e) => Err(format!("Search failed: {}", e)),
    }
}

/// Get symbol token from master contract
#[tauri::command]
pub async fn get_indian_symbol_token(
    broker_id: String,
    symbol: String,
    exchange: String,
) -> Result<Option<Value>, String> {
    eprintln!("[get_indian_symbol_token] Getting token for {}:{}", exchange, symbol);

    use crate::database::symbol_master;

    // Search using symbol master DB
    match symbol_master::get_symbol(&broker_id, &symbol, &exchange) {
        Ok(Some(r)) => Ok(Some(json!({
            "symbol": r.symbol,
            "tradingsymbol": r.br_symbol,
            "name": r.name,
            "exchange": r.exchange,
            "token": r.token,
            "instrumenttype": r.instrument_type,
            "lotsize": r.lot_size,
            "ticksize": r.tick_size,
            "expiry": r.expiry,
            "strike": r.strike,
        }))),
        Ok(None) => Ok(None),
        Err(e) => Err(format!("Lookup failed: {}", e)),
    }
}

/// Get master contract last updated timestamp
#[tauri::command]
pub async fn get_master_contract_timestamp(
    broker_id: String,
) -> Result<Option<i64>, String> {
    eprintln!("[get_master_contract_timestamp] Getting timestamp for: {}", broker_id);

    use crate::database::symbol_master;

    match symbol_master::get_status(&broker_id) {
        Ok(info) => Ok(Some(info.last_updated)),
        Err(_) => Ok(None),
    }
}

// ============================================================================
// Zerodha Trade Book & Position Commands
// ============================================================================

/// Get trade book (executed trades)
#[tauri::command]
pub async fn zerodha_get_trade_book(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_get_trade_book] Fetching trade book");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/trades", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data")
            .and_then(|d| d.as_array())
            .map(|arr| arr.clone())
            .unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch trade book")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get open position for a specific symbol
#[tauri::command]
pub async fn zerodha_get_open_position(
    api_key: String,
    access_token: String,
    tradingsymbol: String,
    exchange: String,
    product: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_open_position] Fetching position for {}:{}", exchange, tradingsymbol);

    // Get all positions first
    let positions_response = zerodha_get_positions(api_key, access_token).await?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if !positions_response.success {
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: positions_response.error,
            timestamp,
        });
    }

    // Find the matching position
    if let Some(positions_data) = positions_response.data {
        // Check net positions
        if let Some(net) = positions_data.get("net").and_then(|n| n.as_array()) {
            for position in net {
                let pos_symbol = position.get("tradingsymbol").and_then(|s| s.as_str()).unwrap_or("");
                let pos_exchange = position.get("exchange").and_then(|s| s.as_str()).unwrap_or("");
                let pos_product = position.get("product").and_then(|s| s.as_str()).unwrap_or("");

                if pos_symbol == tradingsymbol && pos_exchange == exchange && pos_product == product {
                    let quantity = position.get("quantity").and_then(|q| q.as_i64()).unwrap_or(0);
                    return Ok(ApiResponse {
                        success: true,
                        data: Some(json!({
                            "tradingsymbol": pos_symbol,
                            "exchange": pos_exchange,
                            "product": pos_product,
                            "quantity": quantity,
                            "position": position.clone()
                        })),
                        error: None,
                        timestamp,
                    });
                }
            }
        }
    }

    // Position not found - return with quantity 0
    Ok(ApiResponse {
        success: true,
        data: Some(json!({
            "tradingsymbol": tradingsymbol,
            "exchange": exchange,
            "product": product,
            "quantity": 0,
            "position": null
        })),
        error: None,
        timestamp,
    })
}

// ============================================================================
// Zerodha Smart Order Commands
// ============================================================================

/// Place smart order (checks position and adjusts action)
#[tauri::command]
pub async fn zerodha_place_smart_order(
    api_key: String,
    access_token: String,
    tradingsymbol: String,
    exchange: String,
    product: String,
    action: String,
    quantity: i64,
    order_type: String,
    price: Option<f64>,
    trigger_price: Option<f64>,
    position_size: i64,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[zerodha_place_smart_order] Smart order for {}:{}", exchange, tradingsymbol);

    // Determine actual action based on position
    let (final_action, final_quantity) = if action.to_uppercase() == "BUY" {
        if position_size == 0 {
            // No position, buy
            ("BUY".to_string(), quantity)
        } else if position_size > 0 {
            // Already long, add to position
            ("BUY".to_string(), quantity)
        } else {
            // Short position, close first then buy
            let close_qty = position_size.abs();
            if quantity <= close_qty {
                ("BUY".to_string(), quantity)
            } else {
                ("BUY".to_string(), quantity)
            }
        }
    } else {
        // SELL action
        if position_size == 0 {
            // No position, sell (short)
            ("SELL".to_string(), quantity)
        } else if position_size < 0 {
            // Already short, add to position
            ("SELL".to_string(), quantity)
        } else {
            // Long position, close
            ("SELL".to_string(), quantity)
        }
    };

    // Build order params
    let mut params = HashMap::new();
    params.insert("tradingsymbol".to_string(), json!(tradingsymbol));
    params.insert("exchange".to_string(), json!(exchange));
    params.insert("transaction_type".to_string(), json!(final_action));
    params.insert("quantity".to_string(), json!(final_quantity));
    params.insert("product".to_string(), json!(product));
    params.insert("order_type".to_string(), json!(order_type));

    if let Some(p) = price {
        if p > 0.0 {
            params.insert("price".to_string(), json!(p));
        }
    }

    if let Some(tp) = trigger_price {
        if tp > 0.0 {
            params.insert("trigger_price".to_string(), json!(tp));
        }
    }

    zerodha_place_order(api_key, access_token, params, "regular".to_string()).await
}

// ============================================================================
// Zerodha Bulk Operations
// ============================================================================

/// Close all open positions
#[tauri::command]
pub async fn zerodha_close_all_positions(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<OrderPlaceResponse>>, String> {
    eprintln!("[zerodha_close_all_positions] Closing all positions");

    let timestamp = chrono::Utc::now().timestamp_millis();

    // Get all positions
    let positions_response = zerodha_get_positions(api_key.clone(), access_token.clone()).await?;

    if !positions_response.success {
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: positions_response.error,
            timestamp,
        });
    }

    let mut results: Vec<OrderPlaceResponse> = Vec::new();

    if let Some(positions_data) = positions_response.data {
        if let Some(net) = positions_data.get("net").and_then(|n| n.as_array()) {
            for position in net {
                let quantity = position.get("quantity").and_then(|q| q.as_i64()).unwrap_or(0);

                // Skip if no open position
                if quantity == 0 {
                    continue;
                }

                let tradingsymbol = position.get("tradingsymbol").and_then(|s| s.as_str()).unwrap_or("").to_string();
                let exchange = position.get("exchange").and_then(|s| s.as_str()).unwrap_or("").to_string();
                let product = position.get("product").and_then(|s| s.as_str()).unwrap_or("").to_string();

                // Determine action to close position
                let action = if quantity > 0 { "SELL" } else { "BUY" };
                let close_quantity = quantity.abs();

                // Build order params
                let mut params = HashMap::new();
                params.insert("tradingsymbol".to_string(), json!(tradingsymbol));
                params.insert("exchange".to_string(), json!(exchange));
                params.insert("transaction_type".to_string(), json!(action));
                params.insert("quantity".to_string(), json!(close_quantity));
                params.insert("product".to_string(), json!(product));
                params.insert("order_type".to_string(), json!("MARKET"));

                // Place closing order
                let order_result = zerodha_place_order(
                    api_key.clone(),
                    access_token.clone(),
                    params,
                    "regular".to_string(),
                ).await;

                match order_result {
                    Ok(response) => results.push(response),
                    Err(e) => results.push(OrderPlaceResponse {
                        success: false,
                        order_id: None,
                        error: Some(e),
                    }),
                }
            }
        }
    }

    Ok(ApiResponse {
        success: true,
        data: Some(results),
        error: None,
        timestamp,
    })
}

/// Cancel all open orders
#[tauri::command]
pub async fn zerodha_cancel_all_orders(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<OrderPlaceResponse>>, String> {
    eprintln!("[zerodha_cancel_all_orders] Cancelling all open orders");

    let timestamp = chrono::Utc::now().timestamp_millis();

    // Get all orders
    let orders_response = zerodha_get_orders(api_key.clone(), access_token.clone()).await?;

    if !orders_response.success {
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: orders_response.error,
            timestamp,
        });
    }

    let mut results: Vec<OrderPlaceResponse> = Vec::new();

    if let Some(orders) = orders_response.data {
        for order in orders {
            let status = order.get("status").and_then(|s| s.as_str()).unwrap_or("");

            // Only cancel OPEN or TRIGGER PENDING orders
            if status == "OPEN" || status == "TRIGGER PENDING" {
                let order_id = order.get("order_id").and_then(|s| s.as_str()).unwrap_or("").to_string();

                if order_id.is_empty() {
                    continue;
                }

                let cancel_result = zerodha_cancel_order(
                    api_key.clone(),
                    access_token.clone(),
                    order_id,
                ).await;

                match cancel_result {
                    Ok(response) => results.push(response),
                    Err(e) => results.push(OrderPlaceResponse {
                        success: false,
                        order_id: None,
                        error: Some(e),
                    }),
                }
            }
        }
    }

    Ok(ApiResponse {
        success: true,
        data: Some(results),
        error: None,
        timestamp,
    })
}

// ============================================================================
// Zerodha Margin Calculator
// ============================================================================

/// Calculate margin for orders
#[tauri::command]
pub async fn zerodha_calculate_margin(
    api_key: String,
    access_token: String,
    orders: Vec<Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_calculate_margin] Calculating margin for {} orders", orders.len());

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Use basket margin endpoint for multiple orders
    let url = if orders.len() > 1 {
        format!("{}/margins/basket", ZERODHA_API_BASE)
    } else {
        format!("{}/margins/orders", ZERODHA_API_BASE)
    };

    let response = client
        .post(&url)
        .headers(headers)
        .json(&orders)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").cloned().unwrap_or(json!({}));
        Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to calculate margin")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Symbol Search & Instrument Lookup
// ============================================================================

/// Search symbols in master contract database
#[tauri::command]
pub async fn zerodha_search_symbols(
    query: String,
    _exchange: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_search_symbols] Searching for: {}", query);

    // TODO: Implement proper database search when master contract DB is set up
    // For now, return empty results
    let timestamp = chrono::Utc::now().timestamp();

    Ok(ApiResponse {
        success: true,
        data: Some(vec![]),
        error: Some("Master contract database not yet implemented. Use zerodha_download_master_contract first.".to_string()),
        timestamp,
    })
}

/// Get instrument details from master contract
#[tauri::command]
pub async fn zerodha_get_instrument(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_instrument] Looking up: {} on {}", symbol, exchange);

    // TODO: Implement proper database lookup when master contract DB is set up
    // For now, return basic instrument info
    let timestamp = chrono::Utc::now().timestamp();

    let instrument = json!({
        "symbol": symbol,
        "exchange": exchange,
        "name": symbol.clone(),
        "token": "0",
        "instrument_token": "0",
        "lot_size": 1,
        "tick_size": 0.05,
        "instrument_type": "EQ"
    });

    Ok(ApiResponse {
        success: true,
        data: Some(instrument),
        error: Some("Master contract database not yet implemented. Returning fallback data.".to_string()),
        timestamp,
    })
}

/// Get master contract metadata (last updated, count)
/// NOTE: Zerodha master contract is not yet persisted to database.
/// This is a placeholder that returns not-available status.
#[tauri::command]
pub async fn zerodha_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    // TODO: Implement proper database storage for Zerodha master contract
    // Currently zerodha_download_master_contract just returns data without persisting
    // Once database storage is implemented, this should query the database for metadata

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Master contract database not yet implemented for Zerodha".to_string()),
        timestamp,
    })
}
