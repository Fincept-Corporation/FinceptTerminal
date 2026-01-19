//! Groww Broker Integration
//!
//! Complete API implementation for Groww broker including:
//! - Authentication (TOTP-based with API key/secret)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, holdings, margins)
//! - Market Data (quotes, historical)

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde_json::{json, Value};

use super::common::{ApiResponse, TokenExchangeResponse, OrderPlaceResponse};

// Groww API Configuration
// ============================================================================

const GROWW_API_BASE: &str = "https://api.groww.in";

// Segment constants
const SEGMENT_CASH: &str = "CASH";
const SEGMENT_FNO: &str = "FNO";

// Exchange constants
const EXCHANGE_NSE: &str = "NSE";
const EXCHANGE_BSE: &str = "BSE";

fn create_groww_headers(access_token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();

    let auth_value = format!("Bearer {}", access_token);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers.insert("Accept", HeaderValue::from_static("application/json"));
    headers
}

/// Generate TOTP code from API secret using TOTP algorithm
fn generate_totp(api_secret: &str) -> Result<String, String> {
    use std::time::{SystemTime, UNIX_EPOCH};

    // Decode base32 secret
    let secret_bytes = base32_decode(api_secret)?;

    // Get current time step (30 second intervals)
    let time = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|e| format!("Time error: {}", e))?
        .as_secs() / 30;

    // Generate HOTP with time counter
    let hotp = generate_hotp(&secret_bytes, time)?;

    // Return 6 digit code with leading zeros
    Ok(format!("{:06}", hotp % 1_000_000))
}

/// Decode base32 string to bytes
fn base32_decode(input: &str) -> Result<Vec<u8>, String> {
    let alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    let input = input.to_uppercase().replace("=", "");

    let mut bits = String::new();
    for c in input.chars() {
        let index = alphabet.find(c)
            .ok_or_else(|| format!("Invalid base32 character: {}", c))?;
        bits.push_str(&format!("{:05b}", index));
    }

    let mut bytes = Vec::new();
    for chunk in bits.as_bytes().chunks(8) {
        if chunk.len() == 8 {
            let byte_str = std::str::from_utf8(chunk).unwrap();
            let byte = u8::from_str_radix(byte_str, 2).unwrap();
            bytes.push(byte);
        }
    }

    Ok(bytes)
}

/// Generate HOTP code
fn generate_hotp(secret: &[u8], counter: u64) -> Result<u32, String> {
    use hmac::{Hmac, Mac};
    use sha1::Sha1;

    type HmacSha1 = Hmac<Sha1>;

    let counter_bytes = counter.to_be_bytes();

    let mut mac = HmacSha1::new_from_slice(secret)
        .map_err(|e| format!("HMAC error: {}", e))?;
    mac.update(&counter_bytes);
    let result = mac.finalize().into_bytes();

    // Dynamic truncation
    let offset = (result[19] & 0x0f) as usize;
    let code = ((result[offset] & 0x7f) as u32) << 24
        | (result[offset + 1] as u32) << 16
        | (result[offset + 2] as u32) << 8
        | (result[offset + 3] as u32);

    Ok(code)
}

// ============================================================================
// Groww Authentication Commands
// ============================================================================

/// Get access token using API key and secret with TOTP flow
#[tauri::command]
pub async fn groww_get_access_token(
    api_key: String,
    api_secret: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[groww_get_access_token] Getting access token via TOTP");

    // Generate TOTP from API secret
    let totp = generate_totp(&api_secret)?;

    let client = reqwest::Client::new();

    let payload = json!({
        "totp": totp
    });

    let mut headers = HeaderMap::new();
    let auth_value = format!("Bearer {}", api_key);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));

    let response = client
        .post(format!("{}/v1/token/api/access", GROWW_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() {
        // Based on Groww SDK, expect 'token' field in response
        if let Some(token) = body.get("token").and_then(|t| t.as_str()) {
            Ok(TokenExchangeResponse {
                success: true,
                access_token: Some(token.to_string()),
                user_id: None,
                error: None,
            })
        } else {
            Ok(TokenExchangeResponse {
                success: false,
                access_token: None,
                user_id: None,
                error: Some("No token in response".to_string()),
            })
        }
    } else {
        let error_msg = body.get("message")
            .or_else(|| body.get("error"))
            .and_then(|m| m.as_str())
            .unwrap_or("Authentication failed")
            .to_string();
        Ok(TokenExchangeResponse {
            success: false,
            access_token: None,
            user_id: None,
            error: Some(error_msg),
        })
    }
}

/// Validate access token
#[tauri::command]
pub async fn groww_validate_token(
    access_token: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[groww_validate_token] Validating access token");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    // Use margins endpoint to validate token (lightweight check)
    let response = client
        .get(format!("{}/v1/margins/detail/user", GROWW_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        let body: Value = response.json().await.unwrap_or(json!({}));
        let is_valid = body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS");

        Ok(ApiResponse {
            success: is_valid,
            data: Some(is_valid),
            error: if is_valid { None } else { Some("Token validation failed".to_string()) },
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
// Groww Order Commands
// ============================================================================

/// Place an order
#[tauri::command]
pub async fn groww_place_order(
    access_token: String,
    trading_symbol: String,
    exchange: String,
    transaction_type: String,
    order_type: String,
    quantity: i32,
    price: Option<f64>,
    trigger_price: Option<f64>,
    product: String,
    validity: Option<String>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[groww_place_order] Placing order for {}", trading_symbol);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    // Map exchange to segment
    let segment = match exchange.as_str() {
        "NSE" | "BSE" => SEGMENT_CASH,
        "NFO" | "BFO" => SEGMENT_FNO,
        _ => SEGMENT_CASH,
    };

    // Map exchange for Groww API
    let groww_exchange = match exchange.as_str() {
        "NFO" => EXCHANGE_NSE,
        "BFO" => EXCHANGE_BSE,
        _ => exchange.as_str(),
    };

    // Map order type to Groww format
    let groww_order_type = match order_type.as_str() {
        "MARKET" => "MARKET",
        "LIMIT" => "LIMIT",
        "SL" | "STOP_LOSS" => "STOP_LOSS_LIMIT",
        "SL-M" | "STOP_LOSS_MARKET" => "STOP_LOSS_MARKET",
        _ => "MARKET",
    };

    // Build order payload
    let mut payload = json!({
        "trading_symbol": trading_symbol,
        "exchange": groww_exchange,
        "segment": segment,
        "transaction_type": transaction_type.to_uppercase(),
        "order_type": groww_order_type,
        "quantity": quantity,
        "product": product.to_uppercase(),
        "validity": validity.unwrap_or_else(|| "DAY".to_string()),
    });

    // Add price for limit orders
    if let Some(p) = price {
        if p > 0.0 {
            payload["price"] = json!(p);
        }
    }

    // Add trigger price for stop loss orders
    if let Some(tp) = trigger_price {
        if tp > 0.0 {
            payload["trigger_price"] = json!(tp);
        }
    }

    let response = client
        .post(format!("{}/v1/order/create", GROWW_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let empty_obj = json!({});
        let payload_data = body.get("payload").unwrap_or(&empty_obj);
        let order_id = payload_data.get("groww_order_id")
            .or_else(|| payload_data.get("order_id"))
            .and_then(|v| v.as_str())
            .map(String::from);

        Ok(OrderPlaceResponse {
            success: true,
            order_id,
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
pub async fn groww_modify_order(
    access_token: String,
    order_id: String,
    segment: String,
    quantity: Option<i32>,
    price: Option<f64>,
    trigger_price: Option<f64>,
    order_type: Option<String>,
    validity: Option<String>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[groww_modify_order] Modifying order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let mut payload = json!({
        "groww_order_id": order_id,
        "segment": segment,
    });

    if let Some(q) = quantity {
        payload["quantity"] = json!(q);
    }
    if let Some(p) = price {
        payload["price"] = json!(p);
    }
    if let Some(tp) = trigger_price {
        payload["trigger_price"] = json!(tp);
    }
    if let Some(ot) = order_type {
        let groww_order_type = match ot.as_str() {
            "MARKET" => "MARKET",
            "LIMIT" => "LIMIT",
            "SL" => "STOP_LOSS_LIMIT",
            "SL-M" => "STOP_LOSS_MARKET",
            _ => &ot,
        };
        payload["order_type"] = json!(groww_order_type);
    }
    if let Some(v) = validity {
        payload["validity"] = json!(v);
    }

    let response = client
        .post(format!("{}/v1/order/modify", GROWW_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(order_id),
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
pub async fn groww_cancel_order(
    access_token: String,
    order_id: String,
    segment: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[groww_cancel_order] Cancelling order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let payload = json!({
        "groww_order_id": order_id,
        "segment": segment,
    });

    let response = client
        .post(format!("{}/v1/order/cancel", GROWW_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(order_id),
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

/// Get order book
#[tauri::command]
pub async fn groww_get_orders(
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[groww_get_orders] Fetching order book");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let mut all_orders = Vec::new();
    let segments = [SEGMENT_CASH, SEGMENT_FNO];

    for segment in segments {
        let mut page = 0;
        let page_size = 25;

        loop {
            let response = client
                .get(format!("{}/v1/order/list", GROWW_API_BASE))
                .headers(headers.clone())
                .query(&[
                    ("segment", segment),
                    ("page", &page.to_string()),
                    ("page_size", &page_size.to_string()),
                ])
                .send()
                .await
                .map_err(|e| format!("Request failed: {}", e))?;

            let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

            if body.get("status").and_then(|s| s.as_str()) != Some("SUCCESS") {
                break;
            }

            if let Some(orders) = body.get("payload")
                .and_then(|p| p.get("order_list"))
                .and_then(|o| o.as_array())
            {
                if orders.is_empty() {
                    break;
                }
                all_orders.extend(orders.clone());

                if orders.len() < page_size as usize {
                    break;
                }
                page += 1;
            } else {
                break;
            }
        }
    }

    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(all_orders),
        error: None,
        timestamp,
    })
}

// ============================================================================
// Groww Portfolio Commands
// ============================================================================

/// Get positions
#[tauri::command]
pub async fn groww_get_positions(
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[groww_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let mut all_positions = Vec::new();
    let segments = [SEGMENT_CASH, SEGMENT_FNO];

    for segment in segments {
        let response = client
            .get(format!("{}/v1/positions/user", GROWW_API_BASE))
            .headers(headers.clone())
            .query(&[("segment", segment)])
            .send()
            .await
            .map_err(|e| format!("Request failed: {}", e))?;

        let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

        if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
            if let Some(positions) = body.get("payload")
                .and_then(|p| p.get("positions"))
                .and_then(|pos| pos.as_array())
            {
                all_positions.extend(positions.clone());
            }
        }
    }

    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(all_positions),
        error: None,
        timestamp,
    })
}

/// Get holdings
#[tauri::command]
pub async fn groww_get_holdings(
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[groww_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let response = client
        .get(format!("{}/v1/holdings/detail/user", GROWW_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let holdings = body.get("payload").cloned().unwrap_or(json!({}));
        Ok(ApiResponse {
            success: true,
            data: Some(holdings),
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
pub async fn groww_get_margins(
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[groww_get_margins] Fetching margin data");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let response = client
        .get(format!("{}/v1/margins/detail/user", GROWW_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let margins = body.get("payload").cloned().unwrap_or(json!({}));

        // Transform to standard format
        let available_cash = margins.get("clear_cash")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0);
        let collateral = margins.get("collateral_available")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0);
        let margin_used = margins.get("net_margin_used")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0);

        let formatted_margins = json!({
            "availablecash": format!("{:.2}", available_cash),
            "collateral": format!("{:.2}", collateral),
            "utiliseddebits": format!("{:.2}", margin_used),
            "m2munrealized": "0.00",
            "m2mrealized": "0.00",
            "raw": margins,
        });

        Ok(ApiResponse {
            success: true,
            data: Some(formatted_margins),
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
// Groww Market Data Commands
// ============================================================================

/// Get quote for a symbol
#[tauri::command]
pub async fn groww_get_quote(
    access_token: String,
    trading_symbol: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[groww_get_quote] Fetching quote for {}", trading_symbol);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    // Map exchange to segment
    let segment = match exchange.as_str() {
        "NSE" | "BSE" => SEGMENT_CASH,
        "NFO" | "BFO" => SEGMENT_FNO,
        _ => SEGMENT_CASH,
    };

    // Map exchange for Groww API
    let groww_exchange = match exchange.as_str() {
        "NFO" => EXCHANGE_NSE,
        "BFO" => EXCHANGE_BSE,
        _ => &exchange,
    };

    let response = client
        .get(format!("{}/v1/live-data/quote", GROWW_API_BASE))
        .headers(headers)
        .query(&[
            ("exchange", groww_exchange),
            ("segment", segment),
            ("trading_symbol", &trading_symbol),
        ])
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let payload = body.get("payload").cloned().unwrap_or(json!({}));

        // Transform to standard quote format
        let ltp = payload.get("last_price").and_then(|v| v.as_f64()).unwrap_or(0.0);
        let open = payload.get("ohlc").and_then(|o| o.get("open")).and_then(|v| v.as_f64()).unwrap_or(0.0);
        let high = payload.get("ohlc").and_then(|o| o.get("high")).and_then(|v| v.as_f64()).unwrap_or(0.0);
        let low = payload.get("ohlc").and_then(|o| o.get("low")).and_then(|v| v.as_f64()).unwrap_or(0.0);
        let close = payload.get("ohlc").and_then(|o| o.get("close")).and_then(|v| v.as_f64()).unwrap_or(0.0);
        let volume = payload.get("volume").and_then(|v| v.as_i64()).unwrap_or(0);
        let change = payload.get("day_change").and_then(|v| v.as_f64()).unwrap_or(0.0);
        let change_percent = payload.get("day_change_perc").and_then(|v| v.as_f64()).unwrap_or(0.0);

        let quote = json!({
            "symbol": trading_symbol,
            "exchange": exchange,
            "ltp": ltp,
            "open": open,
            "high": high,
            "low": low,
            "close": close,
            "prev_close": close,
            "volume": volume,
            "change": change,
            "change_percent": change_percent,
            "bid": payload.get("bid_price").and_then(|v| v.as_f64()).unwrap_or(0.0),
            "ask": payload.get("offer_price").and_then(|v| v.as_f64()).unwrap_or(0.0),
            "bid_qty": payload.get("bid_quantity").and_then(|v| v.as_i64()).unwrap_or(0),
            "ask_qty": payload.get("offer_quantity").and_then(|v| v.as_i64()).unwrap_or(0),
            "oi": payload.get("open_interest").and_then(|v| v.as_i64()).unwrap_or(0),
            "raw": payload,
        });

        Ok(ApiResponse {
            success: true,
            data: Some(quote),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quote")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get historical candle data
#[tauri::command]
pub async fn groww_get_historical(
    access_token: String,
    trading_symbol: String,
    exchange: String,
    interval: String,
    start_date: String,
    end_date: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[groww_get_historical] Fetching historical data for {}", trading_symbol);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    // Map exchange to segment
    let segment = match exchange.as_str() {
        "NSE" | "BSE" => SEGMENT_CASH,
        "NFO" | "BFO" => SEGMENT_FNO,
        _ => SEGMENT_CASH,
    };

    // Map exchange for Groww API
    let groww_exchange = match exchange.as_str() {
        "NFO" => EXCHANGE_NSE,
        "BFO" => EXCHANGE_BSE,
        _ => &exchange,
    };

    // Map interval to Groww format (minutes)
    let interval_minutes = match interval.as_str() {
        "1m" | "1" => "1",
        "5m" | "5" => "5",
        "10m" | "10" => "10",
        "15m" | "15" => "15",
        "30m" | "30" => "30",
        "1h" | "60" => "60",
        "4h" | "240" => "240",
        "D" | "day" | "1d" | "1440" => "1440",
        "W" | "week" | "1w" | "10080" => "10080",
        _ => "1440", // Default to daily
    };

    let response = client
        .get(format!("{}/v1/historical/candle/range", GROWW_API_BASE))
        .headers(headers)
        .query(&[
            ("exchange", groww_exchange),
            ("segment", segment),
            ("trading_symbol", trading_symbol.as_str()),
            ("start_time", &format!("{} 09:15:00", start_date)),
            ("end_time", &format!("{} 15:30:00", end_date)),
            ("interval_in_minutes", interval_minutes),
        ])
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let candles = body.get("payload")
            .and_then(|p| p.get("candles"))
            .and_then(|c| c.as_array())
            .cloned()
            .unwrap_or_default();

        // Transform candles to standard OHLCV format
        let formatted_candles: Vec<Value> = candles.iter().map(|candle| {
            if let Some(arr) = candle.as_array() {
                let ts = arr.get(0).and_then(|v| v.as_i64()).unwrap_or(0);
                let ts_seconds = if ts > 4102444800 { ts / 1000 } else { ts };

                json!({
                    "timestamp": ts_seconds,
                    "open": arr.get(1).and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "high": arr.get(2).and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "low": arr.get(3).and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "close": arr.get(4).and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "volume": arr.get(5).and_then(|v| v.as_i64()).unwrap_or(0),
                })
            } else {
                candle.clone()
            }
        }).collect();

        Ok(ApiResponse {
            success: true,
            data: Some(formatted_candles),
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

/// Get market depth for a symbol
#[tauri::command]
pub async fn groww_get_depth(
    access_token: String,
    trading_symbol: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[groww_get_depth] Fetching market depth for {}", trading_symbol);

    // Depth data is included in the quote response for Groww
    let quote_result = groww_get_quote(access_token, trading_symbol.clone(), exchange.clone()).await?;

    let timestamp = chrono::Utc::now().timestamp_millis();

    if quote_result.success {
        if let Some(quote_data) = quote_result.data {
            if let Some(raw) = quote_data.get("raw") {
                // Extract depth from quote response
                let depth = raw.get("depth").cloned().unwrap_or(json!({}));

                let buy_levels = depth.get("buy")
                    .and_then(|b| b.as_array())
                    .cloned()
                    .unwrap_or_default();
                let sell_levels = depth.get("sell")
                    .and_then(|s| s.as_array())
                    .cloned()
                    .unwrap_or_default();

                // Format bids/asks
                let bids: Vec<Value> = buy_levels.iter().take(5).map(|level| {
                    json!({
                        "price": level.get("price").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "quantity": level.get("quantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    })
                }).collect();

                let asks: Vec<Value> = sell_levels.iter().take(5).map(|level| {
                    json!({
                        "price": level.get("price").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "quantity": level.get("quantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    })
                }).collect();

                let depth_data = json!({
                    "symbol": trading_symbol,
                    "exchange": exchange,
                    "bids": bids,
                    "asks": asks,
                    "ltp": quote_data.get("ltp").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "totalbuyqty": raw.get("total_buy_quantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "totalsellqty": raw.get("total_sell_quantity").and_then(|v| v.as_i64()).unwrap_or(0),
                });

                return Ok(ApiResponse {
                    success: true,
                    data: Some(depth_data),
                    error: None,
                    timestamp,
                });
            }
        }
    }

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Failed to fetch market depth".to_string()),
        timestamp,
    })
}
