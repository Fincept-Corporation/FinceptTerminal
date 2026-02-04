#![allow(dead_code)]
/**
 * Kotak Neo Broker Commands
 *
 * Rust backend commands for Kotak Neo API integration.
 * Based on OpenAlgo Kotak implementation.
 *
 * Auth Flow: TOTP + MPIN (2-step)
 * 1. Login with TOTP → get view_token, view_sid
 * 2. Validate with MPIN → get trading_token, trading_sid, base_url
 *
 * Auth token format: trading_token:::trading_sid:::base_url:::access_token
 */

use reqwest::Client;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use tauri::command;
use crate::database::pool::get_db;
use rusqlite::params;

// ============================================================================
// URL ENCODING UTILITY
// ============================================================================

/// URL encode a string (percent encoding for form data)
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

// ============================================================================
// RESPONSE TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct ApiResponse<T> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct KotakAuthResponse {
    pub trading_token: String,
    pub trading_sid: String,
    pub base_url: String,
    pub access_token: String,
    pub auth_string: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct KotakOrderResponse {
    pub order_id: Option<String>,
    pub status: String,
    pub message: Option<String>,
}

// ============================================================================
// CONSTANTS
// ============================================================================

const KOTAK_AUTH_BASE: &str = "https://mis.kotaksecurities.com";

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/// Make authenticated request to Kotak API
async fn kotak_request(
    client: &Client,
    method: &str,
    base_url: &str,
    endpoint: &str,
    trading_token: &str,
    trading_sid: &str,
    payload: Option<String>,
) -> Result<Value, String> {
    let url = format!("{}{}", base_url, endpoint);

    let mut request = match method {
        "GET" => client.get(&url),
        "POST" => client.post(&url),
        "PUT" => client.put(&url),
        "DELETE" => client.delete(&url),
        _ => return Err("Invalid HTTP method".to_string()),
    };

    // Add headers
    request = request
        .header("accept", "application/json")
        .header("Sid", trading_sid)
        .header("Auth", trading_token)
        .header("neo-fin-key", "neotradeapi");

    // Add payload for POST requests
    if let Some(body) = payload {
        request = request
            .header("Content-Type", "application/x-www-form-urlencoded")
            .body(body);
    }

    let response = request
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let text = response
        .text()
        .await
        .map_err(|e| format!("Failed to read response: {}", e))?;

    serde_json::from_str(&text).map_err(|e| format!("Failed to parse JSON: {} - Response: {}", e, text))
}

/// Make quotes request (uses Authorization header instead of Auth/Sid)
async fn kotak_quotes_request(
    client: &Client,
    base_url: &str,
    endpoint: &str,
    access_token: &str,
) -> Result<Value, String> {
    let url = format!("{}{}", base_url, endpoint);

    let response = client
        .get(&url)
        .header("Authorization", access_token)
        .header("Content-Type", "application/json")
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let text = response
        .text()
        .await
        .map_err(|e| format!("Failed to read response: {}", e))?;

    serde_json::from_str(&text).map_err(|e| format!("Failed to parse JSON: {} - Response: {}", e, text))
}

/// Parse auth token string into components
fn parse_auth_token(auth_token: &str) -> Result<(String, String, String, String), String> {
    let parts: Vec<&str> = auth_token.split(":::").collect();
    if parts.len() != 4 {
        return Err("Invalid auth token format. Expected: trading_token:::trading_sid:::base_url:::access_token".to_string());
    }
    Ok((
        parts[0].to_string(),
        parts[1].to_string(),
        parts[2].to_string(),
        parts[3].to_string(),
    ))
}

/// URL encode JSON for Kotak API payload format: jData={encoded_json}
fn encode_jdata(data: &Value) -> String {
    let json_str = serde_json::to_string(data).unwrap_or_default();
    format!("jData={}", url_encode(&json_str))
}

/// Map internal exchange to Kotak exchange format
fn map_exchange_to_kotak(exchange: &str) -> &str {
    match exchange {
        "NSE" => "nse_cm",
        "BSE" => "bse_cm",
        "NFO" => "nse_fo",
        "BFO" => "bse_fo",
        "CDS" => "cde_fo",
        "BCD" => "bcs_fo",
        "MCX" => "mcx_fo",
        _ => exchange,
    }
}

/// Map Kotak exchange format to internal
fn map_exchange_from_kotak(exchange: &str) -> &str {
    match exchange {
        "nse_cm" => "NSE",
        "bse_cm" => "BSE",
        "nse_fo" => "NFO",
        "bse_fo" => "BFO",
        "cde_fo" => "CDS",
        "bcs_fo" => "BCD",
        "mcx_fo" => "MCX",
        _ => exchange,
    }
}

/// Map internal order type to Kotak format
fn map_order_type_to_kotak(order_type: &str) -> &str {
    match order_type {
        "MARKET" => "MKT",
        "LIMIT" => "L",
        "SL" | "STOP" | "STOP_LOSS" => "SL",
        "SL-M" | "STOP_LOSS_MARKET" => "SL-M",
        _ => "MKT",
    }
}

// ============================================================================
// AUTHENTICATION COMMANDS
// ============================================================================

/// Step 1: Login with TOTP
#[command]
pub async fn kotak_totp_login(
    ucc: String,
    access_token: String,
    mobile_number: String,
    totp: String,
) -> ApiResponse<Value> {
    let client = Client::new();

    // Ensure mobile number has +91 prefix
    let mobile = if mobile_number.starts_with("+91") {
        mobile_number
    } else if mobile_number.starts_with("91") && mobile_number.len() == 12 {
        format!("+{}", mobile_number)
    } else {
        format!("+91{}", mobile_number.trim_start_matches("91"))
    };

    let payload = json!({
        "mobileNumber": mobile,
        "ucc": ucc,
        "totp": totp
    });

    let url = format!("{}/login/1.0/tradeApiLogin", KOTAK_AUTH_BASE);

    let response = client
        .post(&url)
        .header("Authorization", &access_token)
        .header("neo-fin-key", "neotradeapi")
        .header("Content-Type", "application/json")
        .json(&payload)
        .send()
        .await;

    match response {
        Ok(resp) => {
            let text = resp.text().await.unwrap_or_default();
            match serde_json::from_str::<Value>(&text) {
                Ok(data) => {
                    if data.get("data").and_then(|d| d.get("status")).and_then(|s| s.as_str()) == Some("success") {
                        ApiResponse {
                            success: true,
                            data: Some(json!({
                                "view_token": data["data"]["token"],
                                "view_sid": data["data"]["sid"]
                            })),
                            error: None,
                        }
                    } else {
                        let err_msg = data.get("errMsg")
                            .or(data.get("message"))
                            .and_then(|v| v.as_str())
                            .unwrap_or("TOTP login failed");
                        ApiResponse {
                            success: false,
                            data: None,
                            error: Some(err_msg.to_string()),
                        }
                    }
                }
                Err(e) => ApiResponse {
                    success: false,
                    data: None,
                    error: Some(format!("Failed to parse response: {}", e)),
                },
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Request failed: {}", e)),
        },
    }
}

/// Step 2: Validate with MPIN
#[command]
pub async fn kotak_mpin_validate(
    access_token: String,
    view_token: String,
    view_sid: String,
    mpin: String,
) -> ApiResponse<KotakAuthResponse> {
    let client = Client::new();

    let payload = json!({
        "mpin": mpin
    });

    let url = format!("{}/login/1.0/tradeApiValidate", KOTAK_AUTH_BASE);

    let response = client
        .post(&url)
        .header("Authorization", &access_token)
        .header("neo-fin-key", "neotradeapi")
        .header("sid", &view_sid)
        .header("Auth", &view_token)
        .header("Content-Type", "application/json")
        .json(&payload)
        .send()
        .await;

    match response {
        Ok(resp) => {
            let text = resp.text().await.unwrap_or_default();
            match serde_json::from_str::<Value>(&text) {
                Ok(data) => {
                    if data.get("data").and_then(|d| d.get("status")).and_then(|s| s.as_str()) == Some("success") {
                        let trading_token = data["data"]["token"].as_str().unwrap_or("").to_string();
                        let trading_sid = data["data"]["sid"].as_str().unwrap_or("").to_string();
                        let base_url = data["data"]["baseUrl"].as_str().unwrap_or("").to_string();

                        let auth_string = format!("{}:::{}:::{}:::{}", trading_token, trading_sid, base_url, access_token);

                        ApiResponse {
                            success: true,
                            data: Some(KotakAuthResponse {
                                trading_token,
                                trading_sid,
                                base_url,
                                access_token,
                                auth_string,
                            }),
                            error: None,
                        }
                    } else {
                        let err_msg = data.get("errMsg")
                            .or(data.get("message"))
                            .and_then(|v| v.as_str())
                            .unwrap_or("MPIN validation failed");
                        ApiResponse {
                            success: false,
                            data: None,
                            error: Some(err_msg.to_string()),
                        }
                    }
                }
                Err(e) => ApiResponse {
                    success: false,
                    data: None,
                    error: Some(format!("Failed to parse response: {}", e)),
                },
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Request failed: {}", e)),
        },
    }
}

/// Validate existing auth token
#[command]
pub async fn kotak_validate_token(auth_token: String) -> ApiResponse<bool> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: Some(false),
            error: Some(e),
        },
    };

    let client = Client::new();

    // Try to fetch orders to validate token
    match kotak_request(
        &client,
        "GET",
        &base_url,
        "/quick/user/orders",
        &trading_token,
        &trading_sid,
        None,
    ).await {
        Ok(data) => {
            let is_valid = data.get("stat").and_then(|s| s.as_str()) == Some("Ok")
                || data.get("data").is_some();
            ApiResponse {
                success: true,
                data: Some(is_valid),
                error: None,
            }
        }
        Err(_) => ApiResponse {
            success: true,
            data: Some(false),
            error: None,
        },
    }
}

// ============================================================================
// ORDER COMMANDS
// ============================================================================

/// Place a new order
#[command]
pub async fn kotak_place_order(
    auth_token: String,
    symbol: String,
    exchange: String,
    side: String,
    quantity: i32,
    order_type: String,
    product_type: String,
    price: Option<f64>,
    trigger_price: Option<f64>,
) -> ApiResponse<KotakOrderResponse> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    // Get broker symbol from database
    let br_symbol = get_kotak_br_symbol(&symbol, &exchange).unwrap_or(symbol.clone());

    let order_data = json!({
        "am": "NO",
        "dq": "0",
        "bc": "1",
        "es": map_exchange_to_kotak(&exchange),
        "mp": "0",
        "pc": product_type,
        "pf": "N",
        "pr": price.unwrap_or(0.0).to_string(),
        "pt": map_order_type_to_kotak(&order_type),
        "qt": quantity.to_string(),
        "rt": "DAY",
        "tp": trigger_price.unwrap_or(0.0).to_string(),
        "ts": br_symbol,
        "tt": if side == "BUY" { "B" } else { "S" }
    });

    let client = Client::new();
    let payload = encode_jdata(&order_data);

    match kotak_request(
        &client,
        "POST",
        &base_url,
        "/quick/order/rule/ms/place",
        &trading_token,
        &trading_sid,
        Some(payload),
    ).await {
        Ok(data) => {
            if data.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
                ApiResponse {
                    success: true,
                    data: Some(KotakOrderResponse {
                        order_id: data.get("nOrdNo").and_then(|v| v.as_str()).map(|s| s.to_string()),
                        status: "success".to_string(),
                        message: None,
                    }),
                    error: None,
                }
            } else {
                let err_msg = data.get("emsg").and_then(|v| v.as_str()).unwrap_or("Order placement failed");
                ApiResponse {
                    success: false,
                    data: Some(KotakOrderResponse {
                        order_id: None,
                        status: "error".to_string(),
                        message: Some(err_msg.to_string()),
                    }),
                    error: Some(err_msg.to_string()),
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

/// Modify an existing order
#[command]
pub async fn kotak_modify_order(
    auth_token: String,
    order_id: String,
    symbol: String,
    exchange: String,
    side: String,
    quantity: i32,
    order_type: String,
    product_type: String,
    price: Option<f64>,
    trigger_price: Option<f64>,
) -> ApiResponse<KotakOrderResponse> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    // Get broker symbol and token from database
    let br_symbol = get_kotak_br_symbol(&symbol, &exchange).unwrap_or(symbol.clone());
    let token = get_kotak_token(&symbol, &exchange).unwrap_or_default();

    let order_data = json!({
        "tk": token,
        "dq": "0",
        "es": map_exchange_to_kotak(&exchange),
        "mp": "0",
        "dd": "NA",
        "vd": "DAY",
        "pc": product_type,
        "pr": price.unwrap_or(0.0).to_string(),
        "pt": map_order_type_to_kotak(&order_type),
        "qt": quantity.to_string(),
        "tp": trigger_price.unwrap_or(0.0).to_string(),
        "ts": br_symbol,
        "no": order_id,
        "tt": if side == "BUY" { "B" } else { "S" }
    });

    let client = Client::new();
    let payload = encode_jdata(&order_data);

    match kotak_request(
        &client,
        "POST",
        &base_url,
        "/quick/order/vr/modify",
        &trading_token,
        &trading_sid,
        Some(payload),
    ).await {
        Ok(data) => {
            if data.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
                ApiResponse {
                    success: true,
                    data: Some(KotakOrderResponse {
                        order_id: data.get("nOrdNo").and_then(|v| v.as_str()).map(|s| s.to_string()),
                        status: "success".to_string(),
                        message: None,
                    }),
                    error: None,
                }
            } else {
                let err_msg = data.get("emsg").and_then(|v| v.as_str()).unwrap_or("Order modification failed");
                ApiResponse {
                    success: false,
                    data: Some(KotakOrderResponse {
                        order_id: None,
                        status: "error".to_string(),
                        message: Some(err_msg.to_string()),
                    }),
                    error: Some(err_msg.to_string()),
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

/// Cancel an order
#[command]
pub async fn kotak_cancel_order(
    auth_token: String,
    order_id: String,
) -> ApiResponse<KotakOrderResponse> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let cancel_data = json!({
        "on": order_id,
        "am": "NO"
    });

    let client = Client::new();
    let payload = encode_jdata(&cancel_data);

    match kotak_request(
        &client,
        "POST",
        &base_url,
        "/quick/order/cancel",
        &trading_token,
        &trading_sid,
        Some(payload),
    ).await {
        Ok(data) => {
            if data.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
                ApiResponse {
                    success: true,
                    data: Some(KotakOrderResponse {
                        order_id: data.get("nOrdNo").and_then(|v| v.as_str()).map(|s| s.to_string()),
                        status: "success".to_string(),
                        message: None,
                    }),
                    error: None,
                }
            } else {
                let err_msg = data.get("emsg").and_then(|v| v.as_str()).unwrap_or("Order cancellation failed");
                ApiResponse {
                    success: false,
                    data: Some(KotakOrderResponse {
                        order_id: None,
                        status: "error".to_string(),
                        message: Some(err_msg.to_string()),
                    }),
                    error: Some(err_msg.to_string()),
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

// ============================================================================
// PORTFOLIO COMMANDS
// ============================================================================

/// Get order book
#[command]
pub async fn kotak_get_orders(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let client = Client::new();

    match kotak_request(
        &client,
        "GET",
        &base_url,
        "/quick/user/orders",
        &trading_token,
        &trading_sid,
        None,
    ).await {
        Ok(data) => ApiResponse {
            success: true,
            data: Some(data),
            error: None,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

/// Get trade book
#[command]
pub async fn kotak_get_trade_book(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let client = Client::new();

    match kotak_request(
        &client,
        "GET",
        &base_url,
        "/quick/user/trades",
        &trading_token,
        &trading_sid,
        None,
    ).await {
        Ok(data) => ApiResponse {
            success: true,
            data: Some(data),
            error: None,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

/// Get positions
#[command]
pub async fn kotak_get_positions(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let client = Client::new();

    match kotak_request(
        &client,
        "GET",
        &base_url,
        "/quick/user/positions",
        &trading_token,
        &trading_sid,
        None,
    ).await {
        Ok(data) => ApiResponse {
            success: true,
            data: Some(data),
            error: None,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

/// Get holdings
#[command]
pub async fn kotak_get_holdings(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let client = Client::new();

    match kotak_request(
        &client,
        "GET",
        &base_url,
        "/portfolio/v1/holdings",
        &trading_token,
        &trading_sid,
        None,
    ).await {
        Ok(data) => ApiResponse {
            success: true,
            data: Some(data),
            error: None,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

/// Get funds/limits
#[command]
pub async fn kotak_get_funds(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let client = Client::new();

    // Kotak funds API requires jData payload
    let funds_data = json!({
        "seg": "ALL",
        "exch": "ALL",
        "prod": "ALL"
    });
    let payload = encode_jdata(&funds_data);

    match kotak_request(
        &client,
        "POST",
        &base_url,
        "/quick/user/limits",
        &trading_token,
        &trading_sid,
        Some(payload),
    ).await {
        Ok(data) => ApiResponse {
            success: true,
            data: Some(data),
            error: None,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

// ============================================================================
// MARKET DATA COMMANDS
// ============================================================================

/// Get quotes for a symbol
#[command]
pub async fn kotak_get_quotes(
    auth_token: String,
    symbol: String,
    exchange: String,
) -> ApiResponse<Value> {
    let (_trading_token, _trading_sid, base_url, access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let client = Client::new();

    // Get pSymbol (token) from database
    let psymbol = get_kotak_token(&symbol, &exchange).unwrap_or(symbol.clone());
    let kotak_exchange = map_exchange_to_kotak(&exchange);

    // Build query: exchange|pSymbol
    let query = format!("{}|{}", kotak_exchange, psymbol);
    let encoded_query = url_encode(&query);
    let endpoint = format!("/script-details/1.0/quotes/neosymbol/{}/all", encoded_query);

    match kotak_quotes_request(&client, &base_url, &endpoint, &access_token).await {
        Ok(data) => ApiResponse {
            success: true,
            data: Some(data),
            error: None,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

/// Get market depth for a symbol
#[command]
pub async fn kotak_get_depth(
    auth_token: String,
    symbol: String,
    exchange: String,
) -> ApiResponse<Value> {
    let (_trading_token, _trading_sid, base_url, access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let client = Client::new();

    // Get pSymbol (token) from database
    let psymbol = get_kotak_token(&symbol, &exchange).unwrap_or(symbol.clone());
    let kotak_exchange = map_exchange_to_kotak(&exchange);

    // Build query: exchange|pSymbol
    let query = format!("{}|{}", kotak_exchange, psymbol);
    let encoded_query = url_encode(&query);
    let endpoint = format!("/script-details/1.0/quotes/neosymbol/{}/depth", encoded_query);

    match kotak_quotes_request(&client, &base_url, &endpoint, &access_token).await {
        Ok(data) => ApiResponse {
            success: true,
            data: Some(data),
            error: None,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    }
}

/// Get historical data (NOT SUPPORTED by Kotak Neo)
#[command]
pub async fn kotak_get_history(
    _auth_token: String,
    _symbol: String,
    _exchange: String,
    _interval: String,
    _from_date: String,
    _to_date: String,
) -> ApiResponse<Value> {
    ApiResponse {
        success: false,
        data: None,
        error: Some("Historical data is not supported by Kotak Neo API".to_string()),
    }
}

// ============================================================================
// MASTER CONTRACT COMMANDS
// ============================================================================

/// Download master contract files
#[command]
pub async fn kotak_download_master_contract(auth_token: String) -> ApiResponse<Value> {
    let (_trading_token, _trading_sid, base_url, access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(e),
        },
    };

    let client = Client::new();

    // Get file paths from scripmaster API
    let endpoint = "/script-details/1.0/masterscrip/file-paths";

    let response = client
        .get(format!("{}{}", base_url, endpoint))
        .header("Authorization", &access_token)
        .header("Content-Type", "application/json")
        .send()
        .await;

    match response {
        Ok(resp) => {
            let text = resp.text().await.unwrap_or_default();
            match serde_json::from_str::<Value>(&text) {
                Ok(data) => {
                    if let Some(file_paths) = data.get("data").and_then(|d| d.get("filesPaths")).and_then(|f| f.as_array()) {
                        // Download and process each CSV file
                        let mut total_records = 0;
                        let mut processed_files: Vec<String> = Vec::new();

                        for url_value in file_paths {
                            if let Some(url) = url_value.as_str() {
                                let filename = url.split('/').last().unwrap_or("unknown");

                                // Download CSV
                                if let Ok(csv_resp) = client.get(url).send().await {
                                    if let Ok(csv_text) = csv_resp.text().await {
                                        // Process and store in database
                                        let exchange = if filename.contains("nse_cm") {
                                            "NSE"
                                        } else if filename.contains("bse_cm") {
                                            "BSE"
                                        } else if filename.contains("nse_fo") {
                                            "NFO"
                                        } else if filename.contains("bse_fo") {
                                            "BFO"
                                        } else if filename.contains("cde_fo") {
                                            "CDS"
                                        } else if filename.contains("mcx_fo") {
                                            "MCX"
                                        } else {
                                            continue;
                                        };

                                        match store_kotak_master_contract(&csv_text, exchange) {
                                            Ok(count) => {
                                                total_records += count;
                                                processed_files.push(filename.to_string());
                                            }
                                            Err(e) => {
                                                eprintln!("Error processing {}: {}", filename, e);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ApiResponse {
                            success: true,
                            data: Some(json!({
                                "total_records": total_records,
                                "processed_files": processed_files
                            })),
                            error: None,
                        }
                    } else {
                        ApiResponse {
                            success: false,
                            data: None,
                            error: Some("Failed to get master contract file paths".to_string()),
                        }
                    }
                }
                Err(e) => ApiResponse {
                    success: false,
                    data: None,
                    error: Some(format!("Failed to parse response: {}", e)),
                },
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Request failed: {}", e)),
        },
    }
}

/// Store master contract data in SQLite
fn store_kotak_master_contract(csv_data: &str, exchange: &str) -> Result<usize, String> {
    let db = get_db().map_err(|e| format!("Failed to get database: {}", e))?;

    // Create table if not exists
    db.execute(
        "CREATE TABLE IF NOT EXISTS kotak_master_contract (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            brsymbol TEXT NOT NULL,
            token TEXT NOT NULL,
            exchange TEXT NOT NULL,
            brexchange TEXT NOT NULL,
            name TEXT,
            expiry TEXT,
            strike REAL,
            lotsize INTEGER,
            instrumenttype TEXT,
            tick_size REAL,
            UNIQUE(token, exchange)
        )",
        [],
    ).map_err(|e| format!("Failed to create table: {}", e))?;

    // Parse CSV and insert records
    let mut count = 0;
    let mut lines = csv_data.lines();
    let _header = lines.next(); // Skip header

    for line in lines {
        let fields: Vec<&str> = line.split(',').collect();
        if fields.len() < 8 {
            continue;
        }

        // Extract fields based on Kotak CSV format
        let token = fields.get(0).unwrap_or(&"").trim();
        let brsymbol = fields.get(1).unwrap_or(&"").trim();
        let symbol = fields.get(2).unwrap_or(&"").trim();
        let name = fields.get(3).unwrap_or(&"").trim();

        if token.is_empty() || symbol.is_empty() {
            continue;
        }

        let brexchange = map_exchange_to_kotak(exchange);

        let result = db.execute(
            "INSERT OR REPLACE INTO kotak_master_contract
             (symbol, brsymbol, token, exchange, brexchange, name, expiry, strike, lotsize, instrumenttype, tick_size)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)",
            params![
                symbol,
                brsymbol,
                token,
                exchange,
                brexchange,
                name,
                "", // expiry
                0.0, // strike
                1, // lotsize
                "EQ", // instrumenttype
                0.05 // tick_size
            ],
        );

        if result.is_ok() {
            count += 1;
        }
    }

    // Update metadata
    let now = chrono::Utc::now().to_rfc3339();
    db.execute(
        "INSERT OR REPLACE INTO broker_metadata (broker, key, value) VALUES ('kotak', 'master_contract_updated', ?1)",
        params![now],
    ).ok();

    Ok(count)
}

/// Search symbols in master contract
#[command]
pub async fn kotak_search_symbol(
    query: String,
    exchange: Option<String>,
) -> ApiResponse<Value> {
    let db = match get_db() {
        Ok(db) => db,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to get database: {}", e)),
        },
    };

    let sql = if let Some(exch) = exchange {
        format!(
            "SELECT symbol, brsymbol, token, exchange, brexchange, name, instrumenttype, lotsize
             FROM kotak_master_contract
             WHERE (symbol LIKE '%{}%' OR name LIKE '%{}%') AND exchange = '{}'
             LIMIT 50",
            query, query, exch
        )
    } else {
        format!(
            "SELECT symbol, brsymbol, token, exchange, brexchange, name, instrumenttype, lotsize
             FROM kotak_master_contract
             WHERE symbol LIKE '%{}%' OR name LIKE '%{}%'
             LIMIT 50",
            query, query
        )
    };

    let mut stmt = match db.prepare(&sql) {
        Ok(stmt) => stmt,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to prepare statement: {}", e)),
        },
    };

    let results: Vec<Value> = match stmt.query_map([], |row| {
        Ok(json!({
            "symbol": row.get::<_, String>(0).unwrap_or_default(),
            "brsymbol": row.get::<_, String>(1).unwrap_or_default(),
            "token": row.get::<_, String>(2).unwrap_or_default(),
            "exchange": row.get::<_, String>(3).unwrap_or_default(),
            "brexchange": row.get::<_, String>(4).unwrap_or_default(),
            "name": row.get::<_, String>(5).unwrap_or_default(),
            "instrumenttype": row.get::<_, String>(6).unwrap_or_default(),
            "lotsize": row.get::<_, i32>(7).unwrap_or(1)
        }))
    }) {
        Ok(rows) => rows.filter_map(|r| r.ok()).collect(),
        Err(e) => {
            eprintln!("[kotak_search_symbol] Query failed: {}", e);
            vec![]
        }
    };

    ApiResponse {
        success: true,
        data: Some(json!(results)),
        error: None,
    }
}

/// Get token (pSymbol) for a symbol
#[command]
pub async fn kotak_get_token_for_symbol(
    symbol: String,
    exchange: String,
) -> ApiResponse<String> {
    match get_kotak_token(&symbol, &exchange) {
        Some(token) => ApiResponse {
            success: true,
            data: Some(token),
            error: None,
        },
        None => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Token not found for {} on {}", symbol, exchange)),
        },
    }
}

/// Get symbol by token
#[command]
pub async fn kotak_get_symbol_by_token(
    token: String,
    exchange: String,
) -> ApiResponse<String> {
    let db = match get_db() {
        Ok(db) => db,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to get database: {}", e)),
        },
    };

    let result: Option<String> = db
        .query_row(
            "SELECT symbol FROM kotak_master_contract WHERE token = ?1 AND exchange = ?2",
            params![token, exchange],
            |row| row.get(0),
        )
        .ok();

    match result {
        Some(symbol) => ApiResponse {
            success: true,
            data: Some(symbol),
            error: None,
        },
        None => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Symbol not found for token {} on {}", token, exchange)),
        },
    }
}

/// Get master contract metadata
#[command]
pub async fn kotak_get_master_contract_metadata() -> ApiResponse<Value> {
    let db = match get_db() {
        Ok(db) => db,
        Err(e) => return ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to get database: {}", e)),
        },
    };

    // Get last updated timestamp
    let last_updated: Option<String> = db
        .query_row(
            "SELECT value FROM broker_metadata WHERE broker = 'kotak' AND key = 'master_contract_updated'",
            [],
            |row| row.get(0),
        )
        .ok();

    // Get record count
    let count: i64 = db
        .query_row(
            "SELECT COUNT(*) FROM kotak_master_contract",
            [],
            |row| row.get(0),
        )
        .unwrap_or(0);

    ApiResponse {
        success: true,
        data: Some(json!({
            "last_updated": last_updated,
            "total_records": count
        })),
        error: None,
    }
}

// ============================================================================
// DATABASE HELPER FUNCTIONS
// ============================================================================

/// Get broker symbol from database
fn get_kotak_br_symbol(symbol: &str, exchange: &str) -> Option<String> {
    let db = get_db().ok()?;
    db.query_row(
        "SELECT brsymbol FROM kotak_master_contract WHERE symbol = ?1 AND exchange = ?2",
        params![symbol, exchange],
        |row| row.get(0),
    )
    .ok()
}

/// Get token (pSymbol) from database
fn get_kotak_token(symbol: &str, exchange: &str) -> Option<String> {
    let db = get_db().ok()?;
    db.query_row(
        "SELECT token FROM kotak_master_contract WHERE symbol = ?1 AND exchange = ?2",
        params![symbol, exchange],
        |row| row.get(0),
    )
    .ok()
}
