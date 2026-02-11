//! Fyers Broker Integration
//! 
//! Complete API implementation for Fyers broker including:
//! - Authentication (OAuth with SHA256)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, holdings, funds)
//! - Market Data (quotes, history, depth, margin calculator)
//! - Master Contract (symbol database)
//! - WebSocket (real-time data streaming)

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde_json::{json, Value};
use sha2::{Sha256, Digest};
use std::sync::Arc;
use tokio::sync::RwLock;
use once_cell::sync::Lazy;
use tauri::Emitter;
// chrono::Datelike removed - not used

use crate::websocket::types::MarketMessage;
use crate::websocket::types::ProviderConfig;
use crate::websocket::adapters::WebSocketAdapter;
use crate::WebSocketState;
use super::common::{ApiResponse, TokenExchangeResponse, OrderPlaceResponse};

// Fyers API Configuration
// ============================================================================

const FYERS_API_BASE: &str = "https://api-t1.fyers.in";

fn create_fyers_headers(api_key: &str, access_token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    let auth_value = format!("{}:{}", api_key, access_token);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers
}

/// Validate that credentials are present and non-empty
/// Returns Err if credentials are missing or appear invalid
fn validate_credentials(api_key: &str, access_token: &str) -> Result<(), String> {
    if api_key.is_empty() {
        return Err("API key is empty. Please authenticate first.".to_string());
    }
    if access_token.is_empty() {
        return Err("Access token is empty. Please authenticate first.".to_string());
    }
    // Fyers access tokens should be at least 40 characters
    if access_token.len() < 40 {
        return Err("Access token appears invalid or expired. Please re-authenticate.".to_string());
    }
    Ok(())
}

// ============================================================================
// Fyers Authentication Commands
// ============================================================================

/// Exchange authorization code for access token
#[tauri::command]
pub async fn fyers_exchange_token(
    api_key: String,
    api_secret: String,
    auth_code: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[fyers_exchange_token] Exchanging authorization code");

    // Create app_id_hash: SHA256(api_key:api_secret)
    let checksum_input = format!("{}:{}", api_key, api_secret);
    let mut hasher = Sha256::new();
    hasher.update(checksum_input.as_bytes());
    let app_id_hash = format!("{:x}", hasher.finalize());

    let client = reqwest::Client::new();

    let payload = json!({
        "grant_type": "authorization_code",
        "appIdHash": app_id_hash,
        "code": auth_code
    });

    let response = client
        .post(format!("{}/api/v3/validate-authcode", FYERS_API_BASE))
        .header(CONTENT_TYPE, "application/json")
        .header("Accept", "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(TokenExchangeResponse {
            success: true,
            access_token: body.get("access_token").and_then(|v| v.as_str()).map(String::from),
            user_id: body.get("user_id").and_then(|v| v.as_str()).map(String::from),
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

// ============================================================================
// Fyers Order Management Commands
// ============================================================================

/// Place a new order
#[tauri::command]
pub async fn fyers_place_order(
    api_key: String,
    access_token: String,
    symbol: String,
    qty: i32,
    r#type: i32, // 1=Limit, 2=Market, 3=Stop Loss Limit, 4=Stop Loss Market
    side: i32, // 1=Buy, -1=Sell
    product_type: String, // "CNC", "INTRADAY", "MARGIN", "CO", "BO"
    limit_price: Option<f64>,
    stop_price: Option<f64>,
    disclosed_qty: Option<i32>,
    validity: Option<String>, // "DAY", "IOC"
    offline_order: Option<bool>,
    stop_loss: Option<f64>,
    take_profit: Option<f64>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[fyers_place_order] Placing order for {}", symbol);

    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let mut payload = json!({
        "symbol": symbol,
        "qty": qty,
        "type": r#type,
        "side": side,
        "productType": product_type,
        "validity": validity.unwrap_or_else(|| "DAY".to_string()),
        "offlineOrder": offline_order.unwrap_or(false)
    });

    if let Some(price) = limit_price {
        payload["limitPrice"] = json!(price);
    }
    if let Some(price) = stop_price {
        payload["stopPrice"] = json!(price);
    }
    if let Some(qty) = disclosed_qty {
        payload["disclosedQty"] = json!(qty);
    }
    if let Some(sl) = stop_loss {
        payload["stopLoss"] = json!(sl);
    }
    if let Some(tp) = take_profit {
        payload["takeProfit"] = json!(tp);
    }

    let response = client
        .post(format!("{}/api/v3/orders", FYERS_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: body.get("id").and_then(|v| v.as_str()).map(String::from),
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

/// Modify an existing order
#[tauri::command]
pub async fn fyers_modify_order(
    api_key: String,
    access_token: String,
    order_id: String,
    qty: Option<i32>,
    r#type: Option<i32>,
    limit_price: Option<f64>,
    stop_price: Option<f64>,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    eprintln!("[fyers_modify_order] Modifying order {}", order_id);

    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let mut payload = json!({
        "id": order_id
    });

    if let Some(q) = qty {
        payload["qty"] = json!(q);
    }
    if let Some(t) = r#type {
        payload["type"] = json!(t);
    }
    if let Some(price) = limit_price {
        payload["limitPrice"] = json!(price);
    }
    if let Some(price) = stop_price {
        payload["stopPrice"] = json!(price);
    }

    let response = client
        .put(format!("{}/api/v3/orders", FYERS_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order modification failed")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn fyers_cancel_order(
    api_key: String,
    access_token: String,
    order_id: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    eprintln!("[fyers_cancel_order] Cancelling order {}", order_id);

    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .delete(format!("{}/api/v3/orders", FYERS_API_BASE))
        .headers(headers)
        .json(&json!({"id": order_id}))
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order cancellation failed")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get order book
#[tauri::command]
pub async fn fyers_get_orders(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/orders", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse {
            success: true,
            data: body.get("orderBook").cloned(),
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

/// Get trade book
#[tauri::command]
pub async fn fyers_get_trade_book(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/tradebook", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse {
            success: true,
            data: body.get("tradeBook").cloned(),
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

/// Get positions
#[tauri::command]
pub async fn fyers_get_positions(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/positions", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse {
            success: true,
            data: body.get("netPositions").cloned(),
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
pub async fn fyers_get_holdings(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/holdings", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse {
            success: true,
            data: body.get("holdings").cloned(),
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

/// Get margin/funds data
#[tauri::command]
pub async fn fyers_get_funds(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/funds", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse {
            success: true,
            data: body.get("fund_limit").cloned(),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch funds")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get quotes for symbols using Fyers v2 API depth endpoint
#[tauri::command]
pub async fn fyers_get_quotes(
    api_key: String,
    access_token: String,
    symbols: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    // Validate credentials early to avoid unnecessary API calls
    if let Err(e) = validate_credentials(&api_key, &access_token) {
        eprintln!("[fyers_get_quotes] Credential validation failed: {}", e);
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: chrono::Utc::now().timestamp(),
        });
    }

    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    // Use quotes endpoint to get OHLCV data
    // Symbols should be in broker format (e.g., NSE:SBIN-EQ)
    let symbols_param = symbols.join(",");
    let url = format!("{}/data/quotes?symbols={}", FYERS_API_BASE, symbols_param);

    eprintln!("[fyers_get_quotes] Requesting: {}", url);

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

    eprintln!("[fyers_get_quotes] Response status: {}", status);
    eprintln!("[fyers_get_quotes] Response body: {}", text);

    let body: Value = serde_json::from_str(&text).map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        // Fyers /data/quotes returns array format:
        // { "s": "ok", "d": [{ "n": "NSE:SBIN-EQ", "s": "ok", "v": {...} }] }
        let data_array = body.get("d").and_then(|d| d.as_array());

        if let Some(arr) = data_array {
            // Extract the "v" (value) objects from each quote
            let quotes: Vec<Value> = arr.iter().filter_map(|item| {
                // Each item has: "n" (name/symbol), "s" (status), "v" (values)
                if item.get("s").and_then(|s| s.as_str()) == Some("ok") {
                    item.get("v").cloned()
                } else {
                    None
                }
            }).collect();

            eprintln!("[fyers_get_quotes] Extracted {} quotes", quotes.len());

            Ok(ApiResponse {
                success: true,
                data: Some(json!(quotes)),
                error: None,
                timestamp,
            })
        } else {
            eprintln!("[fyers_get_quotes] No array found in response");
            Ok(ApiResponse {
                success: true,
                data: body.get("d").cloned(),
                error: None,
                timestamp,
            })
        }
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quotes")
            .to_string();
        eprintln!("[fyers_get_quotes] Error: {}", error_msg);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get historical OHLCV data for a symbol using Fyers v3 API
#[tauri::command]
pub async fn fyers_get_history(
    api_key: String,
    access_token: String,
    symbol: String,
    resolution: String,
    from_date: String,
    to_date: String,
) -> Result<ApiResponse<Value>, String> {
    // Validate credentials early to avoid unnecessary API calls
    if let Err(e) = validate_credentials(&api_key, &access_token) {
        eprintln!("[fyers_get_history] Credential validation failed: {}", e);
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: chrono::Utc::now().timestamp(),
        });
    }

    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    // Fyers API expects dates in YYYY-MM-DD format
    // Resolution: 1, 2, 3, 5, 10, 15, 20, 30, 60, 120, 240, 1D
    // Note: Fyers history API endpoint is /data/history (NOT /data-rest/v3/history)
    let url = format!(
        "{}/data/history",
        FYERS_API_BASE
    );

    // Log current system time for debugging
    let now = chrono::Utc::now();
    eprintln!("[fyers_get_history] Current system time: {}", now);

    // Parse date strings and convert to Unix timestamps
    // Fyers API requires epoch timestamps when date_format=0
    let from_timestamp = chrono::NaiveDate::parse_from_str(&from_date, "%Y-%m-%d")
        .map_err(|e| format!("Invalid from_date format: {}", e))?
        .and_hms_opt(0, 0, 0)
        .ok_or("Invalid time")?
        .and_utc()
        .timestamp();

    let to_timestamp = chrono::NaiveDate::parse_from_str(&to_date, "%Y-%m-%d")
        .map_err(|e| format!("Invalid to_date format: {}", e))?
        .and_hms_opt(23, 59, 59)
        .ok_or("Invalid time")?
        .and_utc()
        .timestamp();

    let params = serde_json::json!({
        "symbol": symbol,
        "resolution": resolution,
        "date_format": "0", // Unix timestamp format
        "range_from": from_timestamp.to_string(),
        "range_to": to_timestamp.to_string(),
        "cont_flag": "1"
    });

    eprintln!("[fyers_get_history] ========================================");
    eprintln!("[fyers_get_history] Symbol: {}", symbol);
    eprintln!("[fyers_get_history] Resolution: {}", resolution);
    eprintln!("[fyers_get_history] Date range: {} ({}) to {} ({})", from_date, from_timestamp, to_date, to_timestamp);
    eprintln!("[fyers_get_history] Full URL: {}", url);
    eprintln!("[fyers_get_history] Query params: {:?}", params);
    eprintln!("[fyers_get_history] ========================================");

    let response = client
        .get(&url)
        .headers(headers)
        .query(&params)
        .send()
        .await
        .map_err(|e| {
            eprintln!("[fyers_get_history] Request failed: {}", e);
            format!("Request failed: {}", e)
        })?;

    let status = response.status();
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

    eprintln!("[fyers_get_history] Response status: {}", status);
    eprintln!("[fyers_get_history] Response body length: {}", text.len());

    if text.is_empty() {
        eprintln!("[fyers_get_history] Empty response from Fyers API");
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Empty response from Fyers API. Check your API credentials and symbol format.".to_string()),
            timestamp,
        });
    }

    eprintln!("[fyers_get_history] Response body preview: {}...", &text[..text.len().min(200)]);

    let body: Value = serde_json::from_str(&text).map_err(|e| {
        eprintln!("[fyers_get_history] Parse error. Response was: {}", text);
        format!("Failed to parse response: {}. Response: {}", e, text)
    })?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        // Fyers history API returns:
        // { "s": "ok", "candles": [[timestamp, O, H, L, C, V], ...] }
        Ok(ApiResponse {
            success: true,
            data: Some(body.get("candles").unwrap_or(&serde_json::json!([])).clone()),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch historical data")
            .to_string();
        eprintln!("[fyers_get_history] Error: {}", error_msg);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get market depth (orderbook) for a symbol using Fyers v3 API
#[tauri::command]
pub async fn fyers_get_depth(
    api_key: String,
    access_token: String,
    symbol: String,
) -> Result<ApiResponse<Value>, String> {
    // Validate credentials early to avoid unnecessary API calls
    if let Err(e) = validate_credentials(&api_key, &access_token) {
        eprintln!("[fyers_get_depth] Credential validation failed: {}", e);
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: chrono::Utc::now().timestamp(),
        });
    }

    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    // Fyers depth endpoint provides 5-level bid/ask orderbook
    // Note: Fyers depth endpoint is /data/depth (NOT /data-rest/v3/depth)
    let url = format!("{}/data/depth?symbol={}&ohlcv_flag=1", FYERS_API_BASE, symbol);

    eprintln!("[fyers_get_depth] Requesting: {}", url);

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

    eprintln!("[fyers_get_depth] Response status: {}", status);
    eprintln!("[fyers_get_depth] Response body: {}", text);

    let body: Value = serde_json::from_str(&text).map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        // Fyers depth API returns:
        // { "s": "ok", "d": { "NSE:SYMBOL-EQ": { "totalbuyqty": N, "totalsellqty": N, "bids": [...], "ask": [...] } } }
        // We need to extract the data under the symbol key
        let depth_data = body.get("d")
            .and_then(|d| d.get(&symbol))
            .cloned()
            .or_else(|| body.get("d").cloned());

        Ok(ApiResponse {
            success: true,
            data: depth_data,
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch market depth")
            .to_string();
        eprintln!("[fyers_get_depth] Error: {}", error_msg);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Calculate margin required for orders using Fyers v3 API
#[tauri::command]
pub async fn fyers_calculate_margin(
    api_key: String,
    access_token: String,
    orders: Value,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    // Fyers multiorder margin endpoint
    let url = format!("{}/api/v3/multiorder/margin", FYERS_API_BASE);

    eprintln!("[fyers_calculate_margin] Requesting: {}", url);
    eprintln!("[fyers_calculate_margin] Orders: {}", orders);

    let response = client
        .post(&url)
        .headers(headers)
        .json(&orders)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;

    eprintln!("[fyers_calculate_margin] Response status: {}", status);
    eprintln!("[fyers_calculate_margin] Response body: {}", text);

    let body: Value = serde_json::from_str(&text).map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        // Fyers margin API returns:
        // { "s": "ok", "data": { "margin_avail": N, "margin_total": N, "margin_new_order": N } }
        Ok(ApiResponse {
            success: true,
            data: body.get("data").cloned(),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to calculate margin")
            .to_string();
        eprintln!("[fyers_calculate_margin] Error: {}", error_msg);
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// Fyers Master Contract Commands
// ============================================================================

use crate::database::symbol_master;
// Legacy: use crate::database::fyers_symbols::{self, FyersSymbol};

/// Download and cache Fyers master contract (symbol master)
/// This provides symbol-to-token mapping required for WebSocket subscriptions
#[tauri::command]
pub async fn fyers_download_master_contract(
    _api_key: String,
    _access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[fyers_download_master_contract] Downloading master contract via unified system");

    let timestamp = chrono::Utc::now().timestamp();

    // Use the new unified broker downloads system
    let result = crate::commands::broker_downloads::fyers::fyers_download_symbols().await;

    if result.success {
        Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "symbol_count": result.total_symbols,
                "message": result.message
            })),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(result.message),
            timestamp,
        })
    }
}

// Legacy parse_fyers_csv removed - now using broker_downloads::fyers module

/// Search for symbols in master contract
#[tauri::command]
pub async fn fyers_search_symbol(
    keyword: String,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[fyers_search_symbol] Searching for: {} on exchange: {:?}", keyword, exchange);

    let timestamp = chrono::Utc::now().timestamp();
    let search_limit = limit.unwrap_or(20);

    match symbol_master::search_symbols("fyers", &keyword, exchange.as_deref(), None, search_limit) {
        Ok(results) => {
            eprintln!("[fyers_search_symbol] Found {} results", results.len());
            let json_results: Vec<Value> = results.iter().map(|r| json!({
                "symbol": r.symbol,
                "br_symbol": r.br_symbol,
                "name": r.name,
                "exchange": r.exchange,
                "token": r.token,
                "instrument_type": r.instrument_type,
                "lot_size": r.lot_size,
                "tick_size": r.tick_size,
                "expiry": r.expiry,
                "strike": r.strike,
            })).collect();
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "results": json_results,
                    "count": results.len()
                })),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            eprintln!("[fyers_search_symbol] Error: {}", e);
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Search failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Get token for symbol (required for WebSocket subscription)
#[tauri::command]
pub async fn fyers_get_token_for_symbol(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<i32>, String> {
    eprintln!("[fyers_get_token_for_symbol] Getting token for: {}:{}", exchange, symbol);

    let timestamp = chrono::Utc::now().timestamp();

    match symbol_master::get_token_by_symbol("fyers", &symbol, &exchange) {
        Ok(Some(token_str)) => {
            let token: i32 = token_str.parse().unwrap_or(0);
            eprintln!("[fyers_get_token_for_symbol] Found token: {}", token);
            Ok(ApiResponse {
                success: true,
                data: Some(token),
                error: None,
                timestamp,
            })
        }
        Ok(None) => {
            eprintln!("[fyers_get_token_for_symbol] Symbol not found");
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Symbol {}:{} not found in master contract", exchange, symbol)),
                timestamp,
            })
        }
        Err(e) => {
            eprintln!("[fyers_get_token_for_symbol] Error: {}", e);
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Lookup failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Get symbol info by token
#[tauri::command]
pub async fn fyers_get_symbol_by_token(
    token: i32,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[fyers_get_symbol_by_token] Getting symbol for token: {}", token);

    let timestamp = chrono::Utc::now().timestamp();

    match symbol_master::get_symbol_by_token("fyers", &token.to_string()) {
        Ok(Some(r)) => {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "symbol": r.symbol,
                    "br_symbol": r.br_symbol,
                    "name": r.name,
                    "exchange": r.exchange,
                    "token": r.token,
                    "instrument_type": r.instrument_type,
                    "lot_size": r.lot_size,
                    "tick_size": r.tick_size,
                    "expiry": r.expiry,
                    "strike": r.strike,
                })),
                error: None,
                timestamp,
            })
        }
        Ok(None) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Token {} not found", token)),
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Lookup failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Get master contract metadata (last updated, count)
#[tauri::command]
pub async fn fyers_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match symbol_master::get_status("fyers") {
        Ok(info) => {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "last_updated": info.last_updated,
                    "symbol_count": info.total_symbols,
                    "age_seconds": timestamp - info.last_updated,
                    "status": info.status,
                    "is_ready": info.is_ready,
                })),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Failed to get metadata: {}", e)),
                timestamp,
            })
        }
    }
}

// ============================================================================
// Fyers WebSocket Commands
// ============================================================================

// Global Fyers WebSocket adapter instance
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

    // Create adapter with credentials
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

    // Clone the router so backend services (monitoring, candle aggregation, etc.) receive ticks
    let router = state.router.clone();

    // Set up message callback to emit Tauri events AND route through MessageRouter
    let app_handle = app.clone();
    adapter.set_message_callback(Box::new(move |msg: MarketMessage| {
        // 1. Emit broker-specific events to frontend
        match &msg {
            MarketMessage::Ticker(data) => {
                let _ = app_handle.emit("fyers_ticker", data);
            }
            MarketMessage::OrderBook(data) => {
                let _ = app_handle.emit("fyers_orderbook", data);
            }
            MarketMessage::Trade(data) => {
                let _ = app_handle.emit("fyers_trade", data);
            }
            MarketMessage::Status(data) => {
                let _ = app_handle.emit("fyers_status", data);
            }
            MarketMessage::Candle(data) => {
                let _ = app_handle.emit("fyers_candle", data);
            }
        }

        // 2. Route through MessageRouter for backend services (monitoring, algo trading, etc.)
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
            let mut ws_guard = FYERS_WS.write().await;
            *ws_guard = Some(adapter);

            eprintln!("[fyers_ws_connect] WebSocket connected successfully");

            // Emit connection success event
            let _ = app.emit("fyers_status", json!({
                "provider": "fyers",
                "status": "connected",
                "message": "Connected to Fyers WebSocket",
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
            eprintln!("[fyers_ws_connect] WebSocket connection failed: {}", e);

            // Emit connection failure event
            let _ = app.emit("fyers_status", json!({
                "provider": "fyers",
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
        // Map mode to channel
        let channel = match mode.as_str() {
            "ltp" => "ltp",
            "quotes" | "quote" => "quotes",
            "depth" | "market_depth" => "depth",
            _ => "ltp",
        };

        match adapter.subscribe(&symbol, channel, None).await {
            Ok(_) => {
                eprintln!("[fyers_ws_subscribe] Successfully subscribed to {}", symbol);
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

/// Batch subscribe to multiple symbols via Fyers WebSocket (FAST!)
/// This is much faster than subscribing one-by-one as it fetches all fytokens in one API call
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
        // Map mode to data type
        let data_type = match mode.as_str() {
            "depth" | "market_depth" | "full" => crate::websocket::adapters::fyers::FyersDataType::DepthUpdate,
            "index" => crate::websocket::adapters::fyers::FyersDataType::IndexUpdate,
            _ => crate::websocket::adapters::fyers::FyersDataType::SymbolUpdate,
        };

        match adapter.subscribe_batch(&symbols, data_type).await {
            Ok(_) => {
                let elapsed = start_time.elapsed();
                eprintln!("[fyers_ws_subscribe_batch] ✓ Subscribed to {} symbols in {:?}", symbols.len(), elapsed);
                Ok(ApiResponse {
                    success: true,
                    data: Some(true),
                    error: None,
                    timestamp,
                })
            }
            Err(e) => {
                eprintln!("[fyers_ws_subscribe_batch] ✗ Batch subscription failed: {}", e);
                Ok(ApiResponse {
                    success: false,
                    data: Some(false),
                    error: Some(format!("Batch subscription failed: {}", e)),
                    timestamp,
                })
            }
        }
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("WebSocket not connected. Call fyers_ws_connect first.".to_string()),
            timestamp,
        })
    }
}

/// Check Fyers WebSocket connection status
#[tauri::command]
pub async fn fyers_ws_is_connected() -> Result<bool, String> {
    let ws_guard = FYERS_WS.read().await;

    if let Some(ref adapter) = *ws_guard {
        Ok(adapter.is_connected())
    } else {
        Ok(false)
    }
}
