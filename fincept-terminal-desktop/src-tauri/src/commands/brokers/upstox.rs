//! Upstox Broker Integration
//!
//! Complete API implementation for Upstox broker including:
//! - Authentication (OAuth)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, holdings, funds)
//! - Market Data (quotes, history, depth)
//! - Master Contract (symbol database)

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::collections::HashMap;
use flate2::read::GzDecoder;
use std::io::Read;

use super::common::{ApiResponse, TokenExchangeResponse, OrderPlaceResponse};
use crate::database::pool::get_db;

// Upstox API Configuration
const UPSTOX_API_BASE_V2: &str = "https://api.upstox.com/v2";
const UPSTOX_API_BASE_V3: &str = "https://api.upstox.com/v3";
const UPSTOX_MASTER_CONTRACT_URL: &str = "https://assets.upstox.com/market-quote/instruments/exchange/complete.json.gz";

fn create_upstox_headers(access_token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    let auth_value = format!("Bearer {}", access_token);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers.insert("Accept", HeaderValue::from_static("application/json"));
    headers
}

// ============================================================================
// Upstox Authentication Commands
// ============================================================================

/// Exchange authorization code for access token
#[tauri::command]
pub async fn upstox_exchange_token(
    api_key: String,
    api_secret: String,
    auth_code: String,
    redirect_uri: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[upstox_exchange_token] Exchanging authorization code");

    let client = reqwest::Client::new();

    let params = [
        ("code", auth_code.as_str()),
        ("client_id", api_key.as_str()),
        ("client_secret", api_secret.as_str()),
        ("redirect_uri", redirect_uri.as_str()),
        ("grant_type", "authorization_code"),
    ];

    let response = client
        .post(format!("{}/login/authorization/token", UPSTOX_API_BASE_V2))
        .form(&params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[upstox_exchange_token] Response status: {}, body: {:?}", status, body);

    if status.is_success() && body.get("access_token").is_some() {
        Ok(TokenExchangeResponse {
            success: true,
            access_token: body.get("access_token").and_then(|v| v.as_str()).map(String::from),
            user_id: body.get("user_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("errors")
            .and_then(|e| e.as_array())
            .and_then(|arr| arr.first())
            .and_then(|e| e.get("message"))
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

/// Validate existing token by fetching profile
#[tauri::command]
pub async fn upstox_validate_token(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[upstox_validate_token] Validating token");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);

    let response = client
        .get(format!("{}/user/profile", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        Ok(ApiResponse {
            success: true,
            data: body.get("data").cloned(),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Token validation failed".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Upstox Order Commands
// ============================================================================

/// Place a new order
#[tauri::command]
pub async fn upstox_place_order(
    access_token: String,
    instrument_token: String,
    quantity: i32,
    transaction_type: String, // BUY or SELL
    order_type: String,       // MARKET, LIMIT, SL, SL-M
    product: String,          // D (Delivery) or I (Intraday)
    price: Option<f64>,
    trigger_price: Option<f64>,
    disclosed_quantity: Option<i32>,
    is_amo: Option<bool>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[upstox_place_order] Placing order for {}", instrument_token);

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);

    let payload = json!({
        "instrument_token": instrument_token,
        "quantity": quantity,
        "transaction_type": transaction_type,
        "order_type": order_type,
        "product": product,
        "validity": "DAY",
        "price": price.unwrap_or(0.0),
        "trigger_price": trigger_price.unwrap_or(0.0),
        "disclosed_quantity": disclosed_quantity.unwrap_or(0),
        "is_amo": is_amo.unwrap_or(false),
        "tag": "fincept"
    });

    let response = client
        .post(format!("{}/order/place", UPSTOX_API_BASE_V2))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[upstox_place_order] Response: {:?}", body);

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let order_id = body.get("data")
            .and_then(|d| d.get("order_id"))
            .and_then(|o| o.as_str())
            .map(String::from);
        Ok(OrderPlaceResponse {
            success: true,
            order_id,
            error: None,
        })
    } else {
        let error_msg = body.get("errors")
            .and_then(|e| e.as_array())
            .and_then(|arr| arr.first())
            .and_then(|e| e.get("message"))
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
pub async fn upstox_modify_order(
    access_token: String,
    order_id: String,
    quantity: Option<i32>,
    order_type: Option<String>,
    price: Option<f64>,
    trigger_price: Option<f64>,
    disclosed_quantity: Option<i32>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[upstox_modify_order] Modifying order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);

    let mut payload = json!({
        "order_id": order_id,
        "validity": "DAY"
    });

    if let Some(qty) = quantity {
        payload["quantity"] = json!(qty);
    }
    if let Some(ot) = order_type {
        payload["order_type"] = json!(ot);
    }
    if let Some(p) = price {
        payload["price"] = json!(p);
    }
    if let Some(tp) = trigger_price {
        payload["trigger_price"] = json!(tp);
    }
    if let Some(dq) = disclosed_quantity {
        payload["disclosed_quantity"] = json!(dq);
    }

    let response = client
        .put(format!("{}/order/modify", UPSTOX_API_BASE_V2))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(order_id),
            error: None,
        })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Order modification failed").to_string();
        Ok(OrderPlaceResponse {
            success: false,
            order_id: None,
            error: Some(error_msg),
        })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn upstox_cancel_order(
    access_token: String,
    order_id: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[upstox_cancel_order] Cancelling order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);

    let response = client
        .delete(format!("{}/order/cancel?order_id={}", UPSTOX_API_BASE_V2, order_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(order_id),
            error: None,
        })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Order cancellation failed").to_string();
        Ok(OrderPlaceResponse {
            success: false,
            order_id: None,
            error: Some(error_msg),
        })
    }
}

/// Get all orders
#[tauri::command]
pub async fn upstox_get_orders(access_token: String) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[upstox_get_orders] Fetching order book");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let response = client
        .get(format!("{}/order/retrieve-all", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let orders = body.get("data").and_then(|d| d.as_array()).cloned().unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(orders),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch orders".to_string()),
            timestamp,
        })
    }
}

/// Get trade book
#[tauri::command]
pub async fn upstox_get_trade_book(access_token: String) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[upstox_get_trade_book] Fetching trades");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let response = client
        .get(format!("{}/order/trades/get-trades-for-day", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let trades = body.get("data").and_then(|d| d.as_array()).cloned().unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(trades),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch trades".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Upstox Portfolio Commands
// ============================================================================

/// Get positions
#[tauri::command]
pub async fn upstox_get_positions(access_token: String) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[upstox_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let response = client
        .get(format!("{}/portfolio/short-term-positions", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let positions = body.get("data").and_then(|d| d.as_array()).cloned().unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(positions),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch positions".to_string()),
            timestamp,
        })
    }
}

/// Get holdings
#[tauri::command]
pub async fn upstox_get_holdings(access_token: String) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[upstox_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let response = client
        .get(format!("{}/portfolio/long-term-holdings", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let holdings = body.get("data").and_then(|d| d.as_array()).cloned().unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(holdings),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch holdings".to_string()),
            timestamp,
        })
    }
}

/// Get funds and margins
#[tauri::command]
pub async fn upstox_get_funds(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[upstox_get_funds] Fetching funds");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let response = client
        .get(format!("{}/user/get-funds-and-margin", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        // Combine equity and commodity margins
        let data = body.get("data").cloned();
        Ok(ApiResponse {
            success: true,
            data,
            error: None,
            timestamp,
        })
    } else {
        // Handle service hours error (423)
        if status.as_u16() == 423 {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "equity": {"available_margin": 0.0, "used_margin": 0.0},
                    "commodity": {"available_margin": 0.0, "used_margin": 0.0}
                })),
                error: Some("Service outside operating hours".to_string()),
                timestamp,
            })
        } else {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some("Failed to fetch funds".to_string()),
                timestamp,
            })
        }
    }
}

// ============================================================================
// Upstox Market Data Commands
// ============================================================================

/// URL encode a string (simple implementation)
fn url_encode(s: &str) -> String {
    let mut result = String::new();
    for c in s.chars() {
        match c {
            'A'..='Z' | 'a'..='z' | '0'..='9' | '-' | '_' | '.' | '~' => result.push(c),
            _ => {
                for byte in c.to_string().as_bytes() {
                    result.push_str(&format!("%{:02X}", byte));
                }
            }
        }
    }
    result
}

/// Get quotes for symbols
#[tauri::command]
pub async fn upstox_get_quotes(
    access_token: String,
    instrument_keys: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[upstox_get_quotes] Fetching quotes for {} symbols", instrument_keys.len());

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    // URL encode instrument keys
    let encoded_keys: Vec<String> = instrument_keys.iter()
        .map(|k| url_encode(k))
        .collect();
    let keys_param = encoded_keys.join(",");

    let response = client
        .get(format!("{}/market-quote/ohlc?instrument_key={}&interval=1d", UPSTOX_API_BASE_V3, keys_param))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        Ok(ApiResponse {
            success: true,
            data: body.get("data").cloned(),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch quotes".to_string()),
            timestamp,
        })
    }
}

/// Get historical OHLCV data
#[tauri::command]
pub async fn upstox_get_history(
    access_token: String,
    instrument_key: String,
    interval: String, // 1m, 5m, 15m, 30m, 60m, 1h, D, W, M
    from_date: String, // YYYY-MM-DD
    to_date: String,   // YYYY-MM-DD
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[upstox_get_history] Fetching history for {} from {} to {}", instrument_key, from_date, to_date);

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    // Map interval to Upstox v3 format
    let (unit, interval_val) = match interval.as_str() {
        "1m" => ("minute", "1"),
        "5m" => ("minute", "5"),
        "15m" => ("minute", "15"),
        "30m" => ("minute", "30"),
        "60m" | "1h" => ("minute", "60"),
        "D" | "1D" => ("day", "1"),
        "W" | "1W" => ("week", "1"),
        "M" | "1M" => ("month", "1"),
        _ => ("day", "1"),
    };

    let encoded_key = url_encode(&instrument_key);
    let url = format!(
        "{}/historical-candle/{}/{}/{}?from_date={}&to_date={}",
        UPSTOX_API_BASE_V3, encoded_key, interval_val, unit, from_date, to_date
    );

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let candles = body.get("data")
            .and_then(|d| d.get("candles"))
            .and_then(|c| c.as_array())
            .cloned()
            .unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(candles),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch historical data".to_string()),
            timestamp,
        })
    }
}

/// Get market depth
#[tauri::command]
pub async fn upstox_get_depth(
    access_token: String,
    instrument_key: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[upstox_get_depth] Fetching depth for {}", instrument_key);

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);
    let timestamp = chrono::Utc::now().timestamp();

    let encoded_key = url_encode(&instrument_key);

    let response = client
        .get(format!("{}/market-quote/quotes?instrument_key={}", UPSTOX_API_BASE_V2, encoded_key))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        Ok(ApiResponse {
            success: true,
            data: body.get("data").cloned(),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch market depth".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Upstox Master Contract Commands
// ============================================================================

/// Upstox symbol data structure
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UpstoxSymbol {
    pub instrument_key: String,
    pub trading_symbol: String,
    pub name: String,
    pub exchange: String,
    pub segment: String,
    pub instrument_type: String,
    pub lot_size: i32,
    pub tick_size: f64,
    pub expiry: Option<String>,
    pub strike: Option<f64>,
}

/// Initialize Upstox symbols table in the shared database
fn init_upstox_symbols_table() -> Result<(), String> {
    let db = get_db().map_err(|e| e.to_string())?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS upstox_symbols (
            id INTEGER PRIMARY KEY,
            instrument_key TEXT NOT NULL UNIQUE,
            trading_symbol TEXT NOT NULL,
            name TEXT,
            exchange TEXT NOT NULL,
            segment TEXT NOT NULL,
            instrument_type TEXT,
            lot_size INTEGER DEFAULT 1,
            tick_size REAL DEFAULT 0.05,
            expiry TEXT,
            strike REAL
        )",
        [],
    ).map_err(|e| e.to_string())?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_upstox_trading_symbol ON upstox_symbols(trading_symbol)",
        [],
    ).map_err(|e| e.to_string())?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_upstox_exchange ON upstox_symbols(exchange)",
        [],
    ).map_err(|e| e.to_string())?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS upstox_metadata (
            key TEXT PRIMARY KEY,
            value TEXT
        )",
        [],
    ).map_err(|e| e.to_string())?;

    Ok(())
}

/// Download and store Upstox master contract (async version)
async fn download_and_store_upstox_symbols() -> Result<(i64, HashMap<String, i64>), String> {
    eprintln!("[upstox_symbols] Downloading master contract...");

    // Download gzipped JSON
    let client = reqwest::Client::new();
    let response = client
        .get(UPSTOX_MASTER_CONTRACT_URL)
        .send()
        .await
        .map_err(|e| format!("Download failed: {}", e))?;

    let bytes = response.bytes().await.map_err(|e| format!("Failed to read response: {}", e))?;

    // Decompress
    let mut decoder = GzDecoder::new(&bytes[..]);
    let mut json_str = String::new();
    decoder.read_to_string(&mut json_str).map_err(|e| format!("Decompression failed: {}", e))?;

    // Parse JSON
    let instruments: Vec<Value> = serde_json::from_str(&json_str)
        .map_err(|e| format!("JSON parse failed: {}", e))?;

    eprintln!("[upstox_symbols] Parsed {} instruments", instruments.len());

    // Initialize table
    init_upstox_symbols_table()?;

    let db = get_db().map_err(|e| e.to_string())?;

    // Clear existing data
    db.execute("DELETE FROM upstox_symbols", []).map_err(|e| e.to_string())?;

    let mut segment_counts: HashMap<String, i64> = HashMap::new();
    let mut total = 0i64;

    // Exchange mapping (Upstox segment → standard exchange)
    let exchange_map: HashMap<&str, &str> = [
        ("NSE_EQ", "NSE"),
        ("BSE_EQ", "BSE"),
        ("NSE_FO", "NFO"),
        ("BSE_FO", "BFO"),
        ("MCX_FO", "MCX"),
        ("NCD_FO", "CDS"),
        ("NSE_INDEX", "NSE_INDEX"),
        ("BSE_INDEX", "BSE_INDEX"),
    ].iter().cloned().collect();

    for inst in instruments {
        let segment = inst.get("segment").and_then(|s| s.as_str()).unwrap_or("");

        // Skip NSE_COM
        if segment == "NSE_COM" {
            continue;
        }

        let exchange = exchange_map.get(segment).copied().unwrap_or(segment);

        let instrument_key = inst.get("instrument_key").and_then(|v| v.as_str()).unwrap_or("");
        let trading_symbol = inst.get("trading_symbol").and_then(|v| v.as_str()).unwrap_or("");
        let name = inst.get("name").and_then(|v| v.as_str()).unwrap_or("");
        let instrument_type = inst.get("instrument_type").and_then(|v| v.as_str()).unwrap_or("");
        let lot_size = inst.get("lot_size").and_then(|v| v.as_i64()).unwrap_or(1) as i32;
        let tick_size = inst.get("tick_size").and_then(|v| v.as_f64()).unwrap_or(0.05);
        let strike = inst.get("strike_price").and_then(|v| v.as_f64());

        // Convert expiry from milliseconds to date string
        let expiry: Option<String> = inst.get("expiry")
            .and_then(|v| v.as_i64())
            .and_then(|ms| {
                let secs = ms / 1000;
                chrono::DateTime::from_timestamp(secs, 0)
                    .map(|dt| dt.format("%d-%b-%y").to_string().to_uppercase())
            });

        if db.execute(
            "INSERT OR REPLACE INTO upstox_symbols
             (instrument_key, trading_symbol, name, exchange, segment, instrument_type, lot_size, tick_size, expiry, strike)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
            rusqlite::params![instrument_key, trading_symbol, name, exchange, segment, instrument_type, lot_size, tick_size, expiry, strike],
        ).is_ok() {
            total += 1;
            *segment_counts.entry(segment.to_string()).or_insert(0) += 1;
        }
    }

    // Update metadata
    let now = chrono::Utc::now().timestamp();
    db.execute(
        "INSERT OR REPLACE INTO upstox_metadata (key, value) VALUES ('last_updated', ?1)",
        rusqlite::params![now.to_string()],
    ).map_err(|e| e.to_string())?;

    db.execute(
        "INSERT OR REPLACE INTO upstox_metadata (key, value) VALUES ('symbol_count', ?1)",
        rusqlite::params![total.to_string()],
    ).map_err(|e| e.to_string())?;

    eprintln!("[upstox_symbols] Stored {} symbols", total);

    Ok((total, segment_counts))
}

fn search_upstox_symbols(keyword: &str, exchange: Option<&str>, limit: i32) -> Result<Vec<UpstoxSymbol>, String> {
    init_upstox_symbols_table()?;
    let db = get_db().map_err(|e| e.to_string())?;

    let query = if let Some(exch) = exchange {
        format!(
            "SELECT instrument_key, trading_symbol, name, exchange, segment, instrument_type, lot_size, tick_size, expiry, strike
             FROM upstox_symbols WHERE trading_symbol LIKE '%{}%' AND exchange = '{}' LIMIT {}",
            keyword, exch, limit
        )
    } else {
        format!(
            "SELECT instrument_key, trading_symbol, name, exchange, segment, instrument_type, lot_size, tick_size, expiry, strike
             FROM upstox_symbols WHERE trading_symbol LIKE '%{}%' LIMIT {}",
            keyword, limit
        )
    };

    let mut stmt = db.prepare(&query).map_err(|e| e.to_string())?;
    let rows = stmt.query_map([], |row| {
        Ok(UpstoxSymbol {
            instrument_key: row.get(0)?,
            trading_symbol: row.get(1)?,
            name: row.get(2)?,
            exchange: row.get(3)?,
            segment: row.get(4)?,
            instrument_type: row.get(5)?,
            lot_size: row.get(6)?,
            tick_size: row.get(7)?,
            expiry: row.get(8)?,
            strike: row.get(9)?,
        })
    }).map_err(|e| e.to_string())?;

    let mut results = Vec::new();
    for row in rows {
        if let Ok(symbol) = row {
            results.push(symbol);
        }
    }

    Ok(results)
}

fn get_upstox_instrument_key(symbol: &str, exchange: &str) -> Result<Option<String>, String> {
    init_upstox_symbols_table()?;
    let db = get_db().map_err(|e| e.to_string())?;

    let result: Result<String, _> = db.query_row(
        "SELECT instrument_key FROM upstox_symbols WHERE trading_symbol = ?1 AND exchange = ?2",
        rusqlite::params![symbol, exchange],
        |row| row.get(0),
    );

    match result {
        Ok(key) => Ok(Some(key)),
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
        Err(e) => Err(e.to_string()),
    }
}

fn get_upstox_metadata() -> Result<Option<(i64, i64)>, String> {
    init_upstox_symbols_table()?;
    let db = get_db().map_err(|e| e.to_string())?;

    let last_updated: Result<String, _> = db.query_row(
        "SELECT value FROM upstox_metadata WHERE key = 'last_updated'",
        [],
        |row| row.get(0),
    );

    let count: Result<String, _> = db.query_row(
        "SELECT value FROM upstox_metadata WHERE key = 'symbol_count'",
        [],
        |row| row.get(0),
    );

    match (last_updated, count) {
        (Ok(ts), Ok(cnt)) => {
            let timestamp = ts.parse::<i64>().unwrap_or(0);
            let symbol_count = cnt.parse::<i64>().unwrap_or(0);
            Ok(Some((timestamp, symbol_count)))
        }
        _ => Ok(None),
    }
}

/// Download Upstox master contract
#[tauri::command]
pub async fn upstox_download_master_contract() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match download_and_store_upstox_symbols().await {
        Ok((total, segments)) => {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "total_symbols": total,
                    "segments": segments
                })),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            })
        }
    }
}

/// Search symbols in master contract
#[tauri::command]
pub async fn upstox_search_symbol(
    keyword: String,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let search_limit = limit.unwrap_or(20);

    match search_upstox_symbols(&keyword, exchange.as_deref(), search_limit) {
        Ok(results) => {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "results": results,
                    "count": results.len()
                })),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            })
        }
    }
}

/// Get instrument key for symbol
#[tauri::command]
pub async fn upstox_get_instrument_key(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<String>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match get_upstox_instrument_key(&symbol, &exchange) {
        Ok(Some(key)) => {
            Ok(ApiResponse {
                success: true,
                data: Some(key),
                error: None,
                timestamp,
            })
        }
        Ok(None) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Symbol {}:{} not found", exchange, symbol)),
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            })
        }
    }
}

/// Get master contract metadata
#[tauri::command]
pub async fn upstox_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match get_upstox_metadata() {
        Ok(Some((last_updated, count))) => {
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "last_updated": last_updated,
                    "symbol_count": count,
                    "age_seconds": timestamp - last_updated
                })),
                error: None,
                timestamp,
            })
        }
        Ok(None) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some("Master contract not downloaded yet".to_string()),
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            })
        }
    }
}
