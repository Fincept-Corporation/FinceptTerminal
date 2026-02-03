//! Shoonya (Finvasia) Broker Integration
//!
//! Complete API implementation for Shoonya broker including:
//! - Authentication (TOTP-based with SHA256)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, holdings, funds)
//! - Market Data (quotes, history, depth)
//! - Master Contract (symbol database from ZIP files)
//!
//! API Reference: https://shoonya.com/api-documentation
//! Based on OpenAlgo implementation pattern

use reqwest::header::{HeaderMap, HeaderValue, CONTENT_TYPE};
use serde_json::{json, Value};
use sha2::{Sha256, Digest};
use chrono::{NaiveDate, NaiveDateTime};

use super::common::{ApiResponse, TokenExchangeResponse, OrderPlaceResponse};

// Shoonya API Configuration
// ============================================================================

const SHOONYA_API_BASE: &str = "https://api.shoonya.com";

/// Create Shoonya-specific headers (form-urlencoded)
fn create_shoonya_headers() -> HeaderMap {
    let mut headers = HeaderMap::new();
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/x-www-form-urlencoded"));
    headers
}

/// Generate SHA256 hash
fn sha256_hash(text: &str) -> String {
    let mut hasher = Sha256::new();
    hasher.update(text.as_bytes());
    format!("{:x}", hasher.finalize())
}

/// Create Shoonya payload string with jData and jKey
fn create_payload_with_auth(data: &Value, auth_token: &str) -> String {
    format!("jData={}&jKey={}", data.to_string(), auth_token)
}

/// Create Shoonya payload string (jData only, for auth)
fn create_payload(data: &Value) -> String {
    format!("jData={}", data.to_string())
}

// ============================================================================
// Shoonya Authentication Commands
// ============================================================================

/// Authenticate with Shoonya using TOTP
/// Shoonya uses QuickAuth with SHA256 hashed password and appkey
#[tauri::command]
pub async fn shoonya_authenticate(
    user_id: String,
    password: String,
    totp_code: String,
    api_key: String,      // vendor_code
    api_secret: String,   // api_secretkey
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[shoonya_authenticate] Authenticating user: {}", user_id);

    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    // Hash password with SHA256
    let pwd_hash = sha256_hash(&password);

    // Hash appkey: SHA256(userid|api_secret)
    let appkey_input = format!("{}|{}", user_id, api_secret);
    let appkey_hash = sha256_hash(&appkey_input);

    let payload = json!({
        "uid": user_id,
        "pwd": pwd_hash,
        "factor2": totp_code,
        "apkversion": "1.0.0",
        "appkey": appkey_hash,
        "imei": "abc1234",
        "vc": api_key,
        "source": "API"
    });

    let payload_str = create_payload(&payload);

    let response = client
        .post(format!("{}/NorenWClientTP/QuickAuth", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[shoonya_authenticate] Response: {:?}", body);

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        Ok(TokenExchangeResponse {
            success: true,
            access_token: body.get("susertoken").and_then(|v| v.as_str()).map(String::from),
            user_id: Some(user_id),
            error: None,
        })
    } else {
        let error_msg = body.get("emsg")
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

// ============================================================================
// Shoonya Order Management Commands
// ============================================================================

/// Place a new order
#[tauri::command]
pub async fn shoonya_place_order(
    access_token: String,
    user_id: String,
    symbol: String,         // Trading symbol (broker format)
    exchange: String,       // NSE, BSE, NFO, MCX, CDS, BFO
    quantity: i32,
    price: f64,
    trigger_price: Option<f64>,
    disclosed_qty: Option<i32>,
    product_type: String,   // C (CNC), M (NRML), I (MIS)
    transaction_type: String, // B (Buy), S (Sell)
    order_type: String,     // MKT, LMT, SL-LMT, SL-MKT
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[shoonya_place_order] Placing order for {} on {}", symbol, exchange);

    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "actid": user_id,
        "exch": exchange,
        "tsym": symbol,
        "qty": quantity.to_string(),
        "prc": price.to_string(),
        "trgprc": trigger_price.unwrap_or(0.0).to_string(),
        "dscqty": disclosed_qty.unwrap_or(0).to_string(),
        "prd": product_type,
        "trantype": transaction_type,
        "prctyp": order_type,
        "mkt_protection": "0",
        "ret": "DAY",
        "ordersource": "API"
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/PlaceOrder", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[shoonya_place_order] Response: {:?}", body);

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: body.get("norenordno").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("emsg")
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
pub async fn shoonya_modify_order(
    access_token: String,
    user_id: String,
    order_id: String,
    exchange: String,
    symbol: String,
    quantity: i32,
    price: f64,
    trigger_price: Option<f64>,
    disclosed_qty: Option<i32>,
    order_type: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    eprintln!("[shoonya_modify_order] Modifying order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "exch": exchange,
        "norenordno": order_id,
        "prctyp": order_type,
        "prc": price.to_string(),
        "qty": quantity.to_string(),
        "tsym": symbol,
        "ret": "DAY",
        "mkt_protection": "0",
        "trgprc": trigger_price.unwrap_or(0.0).to_string(),
        "dscqty": disclosed_qty.unwrap_or(0).to_string()
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/ModifyOrder", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("emsg")
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
pub async fn shoonya_cancel_order(
    access_token: String,
    user_id: String,
    order_id: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    eprintln!("[shoonya_cancel_order] Cancelling order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "norenordno": order_id
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/CancelOrder", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(json!({"order_id": order_id})),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("emsg")
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
pub async fn shoonya_get_orders(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "actid": user_id
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/OrderBook", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    // Shoonya returns array on success, dict with stat=Not_Ok on error
    if status.is_success() {
        if body.is_array() {
            Ok(ApiResponse {
                success: true,
                data: Some(body),
                error: None,
                timestamp,
            })
        } else if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
            // No orders case
            Ok(ApiResponse {
                success: true,
                data: Some(json!([])),
                error: None,
                timestamp,
            })
        } else {
            Ok(ApiResponse {
                success: true,
                data: Some(body),
                error: None,
                timestamp,
            })
        }
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
pub async fn shoonya_get_trade_book(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "actid": user_id
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/TradeBook", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() {
        if body.is_array() || body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
            Ok(ApiResponse {
                success: true,
                data: Some(if body.is_array() { body } else { json!([]) }),
                error: None,
                timestamp,
            })
        } else {
            Ok(ApiResponse {
                success: true,
                data: Some(body),
                error: None,
                timestamp,
            })
        }
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch trade book".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Shoonya Portfolio Commands
// ============================================================================

/// Get positions
#[tauri::command]
pub async fn shoonya_get_positions(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "actid": user_id
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/PositionBook", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() {
        if body.is_array() || body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
            Ok(ApiResponse {
                success: true,
                data: Some(if body.is_array() { body } else { json!([]) }),
                error: None,
                timestamp,
            })
        } else {
            Ok(ApiResponse {
                success: true,
                data: Some(body),
                error: None,
                timestamp,
            })
        }
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
pub async fn shoonya_get_holdings(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "actid": user_id,
        "prd": "C"  // Holdings are always CNC product
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/Holdings", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() {
        if body.is_array() || body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
            Ok(ApiResponse {
                success: true,
                data: Some(if body.is_array() { body } else { json!([]) }),
                error: None,
                timestamp,
            })
        } else {
            Ok(ApiResponse {
                success: true,
                data: Some(body),
                error: None,
                timestamp,
            })
        }
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch holdings".to_string()),
            timestamp,
        })
    }
}

/// Get funds/margins (Limits endpoint)
#[tauri::command]
pub async fn shoonya_get_funds(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "actid": user_id
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/Limits", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[shoonya_get_funds] Response: {:?}", body);

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        // Transform Shoonya funds response to unified format
        // Formula from OpenAlgo: total_available = cash + payin - marginused
        let cash = body.get("cash").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let payin = body.get("payin").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let margin_used = body.get("marginused").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let collateral = body.get("brkcollamt").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let rpnl = body.get("rpnl").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
        let urmtom = body.get("urmtom").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);

        let available_cash = cash + payin - margin_used;

        let funds_data = json!({
            "availablecash": format!("{:.2}", available_cash),
            "collateral": format!("{:.2}", collateral),
            "m2munrealized": format!("{:.2}", urmtom),
            "m2mrealized": format!("{:.2}", -rpnl),
            "utiliseddebits": format!("{:.2}", margin_used),
            "raw": body
        });

        Ok(ApiResponse {
            success: true,
            data: Some(funds_data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("emsg")
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

// ============================================================================
// Shoonya Market Data Commands
// ============================================================================

/// Get quotes for a symbol
#[tauri::command]
pub async fn shoonya_get_quotes(
    access_token: String,
    user_id: String,
    exchange: String,
    token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    // Normalize index exchanges
    let api_exchange = match exchange.as_str() {
        "NSE_INDEX" => "NSE",
        "BSE_INDEX" => "BSE",
        _ => &exchange,
    };

    let payload = json!({
        "uid": user_id,
        "exch": api_exchange,
        "token": token
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/GetQuotes", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        // Transform to unified quote format
        let quote_data = json!({
            "bid": body.get("bp1").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "ask": body.get("sp1").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "open": body.get("o").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "high": body.get("h").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "low": body.get("l").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "ltp": body.get("lp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "prev_close": body.get("c").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "volume": body.get("v").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0),
            "oi": body.get("oi").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0),
            "raw": body
        });

        Ok(ApiResponse {
            success: true,
            data: Some(quote_data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("emsg")
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

/// Get market depth (5-level bid/ask)
#[tauri::command]
pub async fn shoonya_get_depth(
    access_token: String,
    user_id: String,
    exchange: String,
    token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let api_exchange = match exchange.as_str() {
        "NSE_INDEX" => "NSE",
        "BSE_INDEX" => "BSE",
        _ => &exchange,
    };

    let payload = json!({
        "uid": user_id,
        "exch": api_exchange,
        "token": token
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/GetQuotes", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        // Build depth arrays from bp1-bp5, bq1-bq5, sp1-sp5, sq1-sq5
        let mut bids = Vec::new();
        let mut asks = Vec::new();

        for i in 1..=5 {
            let bp_key = format!("bp{}", i);
            let bq_key = format!("bq{}", i);
            let sp_key = format!("sp{}", i);
            let sq_key = format!("sq{}", i);

            bids.push(json!({
                "price": body.get(&bp_key).and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
                "quantity": body.get(&bq_key).and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0)
            }));

            asks.push(json!({
                "price": body.get(&sp_key).and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
                "quantity": body.get(&sq_key).and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0)
            }));
        }

        let total_buy_qty: i64 = bids.iter().map(|b| b.get("quantity").and_then(|v| v.as_i64()).unwrap_or(0)).sum();
        let total_sell_qty: i64 = asks.iter().map(|a| a.get("quantity").and_then(|v| v.as_i64()).unwrap_or(0)).sum();

        let depth_data = json!({
            "bids": bids,
            "asks": asks,
            "totalbuyqty": total_buy_qty,
            "totalsellqty": total_sell_qty,
            "high": body.get("h").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "low": body.get("l").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "ltp": body.get("lp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "ltq": body.get("ltq").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0),
            "open": body.get("o").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "prev_close": body.get("c").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0),
            "volume": body.get("v").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0)
        });

        Ok(ApiResponse {
            success: true,
            data: Some(depth_data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch market depth")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Get historical OHLCV data
#[tauri::command]
pub async fn shoonya_get_history(
    access_token: String,
    user_id: String,
    exchange: String,
    token: String,
    symbol: String,
    resolution: String,  // 1, 3, 5, 10, 15, 30, 60, 120, 240, D
    from_date: String,   // YYYY-MM-DD
    to_date: String,     // YYYY-MM-DD
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let api_exchange = match exchange.as_str() {
        "NSE_INDEX" => "NSE",
        "BSE_INDEX" => "BSE",
        _ => &exchange,
    };

    // Parse dates to Unix timestamps
    let start_ts = NaiveDate::parse_from_str(&from_date, "%Y-%m-%d")
        .map_err(|e| format!("Invalid from_date: {}", e))?
        .and_hms_opt(0, 0, 0)
        .ok_or("Invalid time")?
        .and_utc()
        .timestamp();

    let end_ts = NaiveDate::parse_from_str(&to_date, "%Y-%m-%d")
        .map_err(|e| format!("Invalid to_date: {}", e))?
        .and_hms_opt(23, 59, 59)
        .ok_or("Invalid time")?
        .and_utc()
        .timestamp();

    // Choose endpoint based on resolution
    let (endpoint, payload) = if resolution == "D" {
        // Daily data uses EODChartData
        let sym = format!("{}:{}", api_exchange, symbol);
        (
            format!("{}/NorenWClientTP/EODChartData", SHOONYA_API_BASE),
            json!({
                "uid": user_id,
                "sym": sym,
                "from": start_ts.to_string(),
                "to": end_ts.to_string()
            })
        )
    } else {
        // Intraday uses TPSeries
        (
            format!("{}/NorenWClientTP/TPSeries", SHOONYA_API_BASE),
            json!({
                "uid": user_id,
                "exch": api_exchange,
                "token": token,
                "st": start_ts.to_string(),
                "et": end_ts.to_string(),
                "intrv": resolution
            })
        )
    };

    let payload_str = create_payload_with_auth(&payload, &access_token);

    eprintln!("[shoonya_get_history] Endpoint: {}, Payload: {}", endpoint, payload);

    let response = client
        .post(&endpoint)
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.is_array() {
        // Transform candle data to unified format
        let candles: Vec<Value> = body.as_array()
            .unwrap_or(&vec![])
            .iter()
            .filter_map(|candle| {
                // Skip zero candles
                let into = candle.get("into").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
                let inth = candle.get("inth").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
                let intl = candle.get("intl").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
                let intc = candle.get("intc").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);

                if into == 0.0 && inth == 0.0 && intl == 0.0 && intc == 0.0 {
                    return None;
                }

                // Parse timestamp
                let ts = if resolution == "D" {
                    // EOD format: ssboe
                    candle.get("ssboe").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0)
                } else {
                    // Intraday format: time as "DD-MM-YYYY HH:MM:SS"
                    candle.get("time").and_then(|v| v.as_str()).and_then(|time_str| {
                        NaiveDateTime::parse_from_str(time_str, "%d-%m-%Y %H:%M:%S")
                            .ok()
                            .map(|dt| dt.and_utc().timestamp())
                    }).unwrap_or(0)
                };

                let intv = candle.get("intv").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
                let oi = candle.get("oi").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);

                Some(json!([ts, into, inth, intl, intc, intv, oi]))
            })
            .collect();

        Ok(ApiResponse {
            success: true,
            data: Some(json!(candles)),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("emsg")
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

// ============================================================================
// Shoonya Master Contract Commands
// ============================================================================

use crate::database::shoonya_symbols::{self, ShoonyaSymbol};

/// Master contract URLs for each exchange
const SHOONYA_MASTER_URLS: &[(&str, &str)] = &[
    ("NSE", "https://api.shoonya.com/NSE_symbols.txt.zip"),
    ("NFO", "https://api.shoonya.com/NFO_symbols.txt.zip"),
    ("CDS", "https://api.shoonya.com/CDS_symbols.txt.zip"),
    ("MCX", "https://api.shoonya.com/MCX_symbols.txt.zip"),
    ("BSE", "https://api.shoonya.com/BSE_symbols.txt.zip"),
    ("BFO", "https://api.shoonya.com/BFO_symbols.txt.zip"),
];

/// Download and cache Shoonya master contract
#[tauri::command]
pub async fn shoonya_download_master_contract() -> Result<ApiResponse<Value>, String> {
    eprintln!("[shoonya_download_master_contract] Downloading master contract");

    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let mut total_symbols = 0;
    let mut segment_counts: std::collections::HashMap<String, i32> = std::collections::HashMap::new();

    for (exchange, url) in SHOONYA_MASTER_URLS {
        eprintln!("[shoonya_download_master_contract] Downloading {}", exchange);

        match client.get(*url).send().await {
            Ok(response) => {
                if response.status().is_success() {
                    let bytes = response.bytes().await.map_err(|e| format!("Failed to read response: {}", e))?;

                    // Unzip and parse CSV
                    match parse_shoonya_zip(&bytes, exchange) {
                        Ok(symbols) => {
                            let count = symbols.len() as i32;
                            eprintln!("[shoonya_download_master_contract] Parsed {} symbols for {}", count, exchange);

                            // Save to database
                            if let Err(e) = shoonya_symbols::save_symbols_bulk(&symbols) {
                                eprintln!("[shoonya_download_master_contract] Failed to save {} symbols: {}", exchange, e);
                            } else {
                                segment_counts.insert(exchange.to_string(), count);
                                total_symbols += count;
                            }
                        }
                        Err(e) => {
                            eprintln!("[shoonya_download_master_contract] Failed to parse {}: {}", exchange, e);
                        }
                    }
                } else {
                    eprintln!("[shoonya_download_master_contract] Failed to download {}: {}", exchange, response.status());
                }
            }
            Err(e) => {
                eprintln!("[shoonya_download_master_contract] Request failed for {}: {}", exchange, e);
            }
        }
    }

    Ok(ApiResponse {
        success: total_symbols > 0,
        data: Some(json!({
            "total_symbols": total_symbols,
            "segments": segment_counts,
            "message": format!("Downloaded {} symbols", total_symbols)
        })),
        error: if total_symbols == 0 { Some("Failed to download any symbols".to_string()) } else { None },
        timestamp,
    })
}

/// Parse Shoonya ZIP file containing symbol CSV
fn parse_shoonya_zip(zip_bytes: &[u8], exchange: &str) -> Result<Vec<ShoonyaSymbol>, String> {
    use std::io::{Cursor, Read};
    use zip::ZipArchive;

    let cursor = Cursor::new(zip_bytes);
    let mut archive = ZipArchive::new(cursor).map_err(|e| format!("Failed to open ZIP: {}", e))?;

    let mut symbols = Vec::new();

    for i in 0..archive.len() {
        let mut file = archive.by_index(i).map_err(|e| format!("Failed to read ZIP entry: {}", e))?;

        if file.name().ends_with(".txt") {
            let mut contents = String::new();
            file.read_to_string(&mut contents).map_err(|e| format!("Failed to read file: {}", e))?;

            // Parse CSV based on exchange
            let parsed = parse_shoonya_csv(&contents, exchange)?;
            symbols.extend(parsed);
        }
    }

    Ok(symbols)
}

/// Parse Shoonya CSV content
fn parse_shoonya_csv(csv_content: &str, exchange: &str) -> Result<Vec<ShoonyaSymbol>, String> {
    let mut symbols = Vec::new();
    let mut lines = csv_content.lines();

    // Skip header
    lines.next();

    for line in lines {
        if line.trim().is_empty() {
            continue;
        }

        let fields: Vec<&str> = line.split(',').collect();

        // Minimum fields check
        if fields.len() < 6 {
            continue;
        }

        // Parse based on exchange type
        let symbol = match exchange {
            "NSE" | "BSE" => parse_equity_symbol(&fields, exchange),
            "NFO" | "BFO" => parse_fo_symbol(&fields, exchange),
            "MCX" => parse_mcx_symbol(&fields),
            "CDS" => parse_cds_symbol(&fields),
            _ => continue,
        };

        if let Some(sym) = symbol {
            symbols.push(sym);
        }
    }

    Ok(symbols)
}

fn parse_equity_symbol(fields: &[&str], exchange: &str) -> Option<ShoonyaSymbol> {
    // Columns: Exchange, Token, LotSize, Symbol, TradingSymbol, Instrument, TickSize
    let token = fields.get(1)?.trim().to_string();
    let lot_size = fields.get(2)?.trim().parse::<i32>().unwrap_or(1);
    let name = fields.get(3)?.trim().to_string();
    let br_symbol = fields.get(4)?.trim().to_string();
    let instrument_type = fields.get(5)?.trim().to_string();
    let tick_size = fields.get(6)?.trim().parse::<f64>().unwrap_or(0.05);

    // Generate OpenAlgo-style symbol
    let symbol = if br_symbol.ends_with("-EQ") {
        br_symbol.replace("-EQ", "")
    } else if br_symbol.ends_with("-BE") {
        br_symbol.replace("-BE", "")
    } else {
        br_symbol.clone()
    };

    // Determine actual exchange (handle INDEX)
    let actual_exchange = if instrument_type == "INDEX" {
        format!("{}_INDEX", exchange)
    } else {
        exchange.to_string()
    };

    Some(ShoonyaSymbol {
        symbol,
        br_symbol,
        name,
        exchange: actual_exchange.clone(),
        br_exchange: actual_exchange,
        token,
        expiry: None,
        strike: None,
        lot_size,
        instrument_type: if instrument_type == "EQ" || instrument_type == "BE" { "EQ".to_string() } else { instrument_type },
        tick_size,
    })
}

fn parse_fo_symbol(fields: &[&str], exchange: &str) -> Option<ShoonyaSymbol> {
    // Columns: Exchange, Token, LotSize, Symbol, TradingSymbol, Expiry, Instrument, OptionType, StrikePrice, TickSize
    if fields.len() < 10 {
        return None;
    }

    let token = fields.get(1)?.trim().to_string();
    let lot_size = fields.get(2)?.trim().parse::<i32>().unwrap_or(1);
    let name = fields.get(3)?.trim().to_string();
    let br_symbol = fields.get(4)?.trim().to_string();
    let expiry_str = fields.get(5)?.trim();
    let _instrument = fields.get(6)?.trim();
    let option_type = fields.get(7)?.trim();
    let strike_str = fields.get(8)?.trim();
    let tick_size = fields.get(9)?.trim().parse::<f64>().unwrap_or(0.05);

    // Parse expiry (DD-MMM-YYYY to DD-MMM-YY)
    let expiry = chrono::NaiveDate::parse_from_str(expiry_str, "%d-%b-%Y")
        .ok()
        .map(|d| d.format("%d-%b-%y").to_string().to_uppercase());

    // Determine instrument type
    let instrument_type = if option_type == "XX" {
        "FUT".to_string()
    } else {
        option_type.to_string()
    };

    // Parse strike price
    let strike: Option<f64> = strike_str.parse().ok();

    // Generate OpenAlgo symbol
    let compact_expiry = expiry.as_ref().map(|e| e.replace("-", "")).unwrap_or_default();
    let symbol = if instrument_type == "FUT" {
        format!("{}{}FUT", name, compact_expiry)
    } else {
        let strike_fmt = if let Some(s) = strike {
            if s.fract() == 0.0 { format!("{}", s as i64) } else { format!("{}", s) }
        } else {
            "0".to_string()
        };
        format!("{}{}{}{}", name, compact_expiry, strike_fmt, instrument_type)
    };

    Some(ShoonyaSymbol {
        symbol,
        br_symbol,
        name,
        exchange: exchange.to_string(),
        br_exchange: exchange.to_string(),
        token,
        expiry,
        strike,
        lot_size,
        instrument_type,
        tick_size,
    })
}

fn parse_mcx_symbol(fields: &[&str]) -> Option<ShoonyaSymbol> {
    // Similar to NFO but for commodities
    parse_fo_symbol(fields, "MCX")
}

fn parse_cds_symbol(fields: &[&str]) -> Option<ShoonyaSymbol> {
    // Currency derivatives
    if fields.len() < 10 {
        return None;
    }

    // Filter out dummy entries
    let token: i32 = fields.get(1)?.trim().parse().ok()?;
    if token < 100 {
        return None;
    }

    parse_fo_symbol(fields, "CDS")
}

/// Search for symbols in master contract database
#[tauri::command]
pub async fn shoonya_search_symbol(
    keyword: String,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[shoonya_search_symbol] Searching for: {} on exchange: {:?}", keyword, exchange);

    let timestamp = chrono::Utc::now().timestamp();
    let search_limit = limit.unwrap_or(20);

    match shoonya_symbols::search_symbols(&keyword, exchange.as_deref(), search_limit) {
        Ok(results) => {
            eprintln!("[shoonya_search_symbol] Found {} results", results.len());
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
            eprintln!("[shoonya_search_symbol] Error: {}", e);
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Search failed: {}", e)),
                timestamp,
            })
        }
    }
}

/// Get token for symbol (required for data subscription)
#[tauri::command]
pub async fn shoonya_get_token_for_symbol(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<String>, String> {
    eprintln!("[shoonya_get_token_for_symbol] Getting token for: {}:{}", exchange, symbol);

    let timestamp = chrono::Utc::now().timestamp();

    match shoonya_symbols::get_token_by_symbol(&symbol, &exchange) {
        Ok(Some(token)) => {
            eprintln!("[shoonya_get_token_for_symbol] Found token: {}", token);
            Ok(ApiResponse {
                success: true,
                data: Some(token),
                error: None,
                timestamp,
            })
        }
        Ok(None) => {
            eprintln!("[shoonya_get_token_for_symbol] Symbol not found");
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Symbol {}:{} not found in master contract", exchange, symbol)),
                timestamp,
            })
        }
        Err(e) => {
            eprintln!("[shoonya_get_token_for_symbol] Error: {}", e);
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
pub async fn shoonya_get_symbol_by_token(
    token: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[shoonya_get_symbol_by_token] Getting symbol for token: {} on {}", token, exchange);

    let timestamp = chrono::Utc::now().timestamp();

    match shoonya_symbols::get_symbol_by_token(&token, &exchange) {
        Ok(Some(symbol_info)) => {
            Ok(ApiResponse {
                success: true,
                data: Some(json!(symbol_info)),
                error: None,
                timestamp,
            })
        }
        Ok(None) => {
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Token {} not found on {}", token, exchange)),
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
pub async fn shoonya_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    match shoonya_symbols::get_metadata() {
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
                error: Some(format!("Failed to get metadata: {}", e)),
                timestamp,
            })
        }
    }
}
