/**
 * Motilal Oswal Broker Commands
 *
 * REST API integration for Motilal Oswal broker
 * Reference: https://openapi.motilaloswal.com
 */

use reqwest::Client;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use sha2::{Sha256, Digest};
use std::collections::HashMap;
use tauri::command;

// ============================================================================
// TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct ApiResponse {
    pub success: bool,
    pub data: Option<Value>,
    pub error: Option<String>,
    pub timestamp: u64,
}

fn get_timestamp() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64
}

// ============================================================================
// CONSTANTS
// ============================================================================

const MOTILAL_BASE_URL: &str = "https://openapi.motilaloswal.com";

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

fn get_motilal_headers(auth_token: &str, api_key: &str, vendor_code: &str) -> HashMap<String, String> {
    let mut headers = HashMap::new();
    headers.insert("Authorization".to_string(), auth_token.to_string());
    headers.insert("Content-Type".to_string(), "application/json".to_string());
    headers.insert("Accept".to_string(), "application/json".to_string());
    headers.insert("User-Agent".to_string(), "MOSL/V.1.1.0".to_string());
    headers.insert("ApiKey".to_string(), api_key.to_string());
    headers.insert("ClientLocalIp".to_string(), "127.0.0.1".to_string());
    headers.insert("ClientPublicIp".to_string(), "127.0.0.1".to_string());
    headers.insert("MacAddress".to_string(), "00:00:00:00:00:00".to_string());
    headers.insert("SourceId".to_string(), "WEB".to_string());
    headers.insert("vendorinfo".to_string(), vendor_code.to_string());
    headers.insert("osname".to_string(), "Windows".to_string());
    headers.insert("osversion".to_string(), "10.0".to_string());
    headers.insert("devicemodel".to_string(), "PC".to_string());
    headers.insert("manufacturer".to_string(), "Generic".to_string());
    headers.insert("productname".to_string(), "FinceptTerminal".to_string());
    headers.insert("productversion".to_string(), "1.0.0".to_string());
    headers.insert("browsername".to_string(), "Chrome".to_string());
    headers.insert("browserversion".to_string(), "120.0".to_string());
    headers
}

async fn motilal_request(
    client: &Client,
    method: &str,
    endpoint: &str,
    headers: &HashMap<String, String>,
    body: Option<Value>,
) -> Result<Value, String> {
    let url = format!("{}{}", MOTILAL_BASE_URL, endpoint);

    let mut request = match method {
        "GET" => client.get(&url),
        "POST" => client.post(&url),
        "PUT" => client.put(&url),
        "DELETE" => client.delete(&url),
        _ => return Err("Invalid HTTP method".to_string()),
    };

    // Add headers
    for (key, value) in headers {
        request = request.header(key.as_str(), value.as_str());
    }

    // Add body if present
    if let Some(body_data) = body {
        request = request.json(&body_data);
    }

    let response = request.send().await.map_err(|e| e.to_string())?;

    if response.status().is_success() {
        response.json::<Value>().await.map_err(|e| e.to_string())
    } else {
        let status = response.status();
        let error_text = response.text().await.unwrap_or_default();
        Err(format!("HTTP {}: {}", status, error_text))
    }
}

// ============================================================================
// AUTHENTICATION COMMANDS
// ============================================================================

/// Authenticate with Motilal Oswal and get auth token
#[command]
pub async fn motilal_authenticate(
    user_id: String,
    password: String,
    totp: Option<String>,
    dob: String,
    api_key: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();

    // SHA-256(password + apikey) as per Motilal Oswal API documentation
    let mut hasher = Sha256::new();
    hasher.update(format!("{}{}", password, api_key));
    let password_hash = format!("{:x}", hasher.finalize());

    let mut payload = json!({
        "userid": user_id,
        "password": password_hash,
        "2FA": dob
    });

    // Add TOTP if provided
    if let Some(totp_code) = totp {
        if !totp_code.is_empty() {
            payload["totp"] = json!(totp_code);
        }
    }

    let headers = get_motilal_headers("", &api_key, &user_id);

    match motilal_request(&client, "POST", "/rest/login/v3/authdirectapi", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                if let Some(auth_token) = data.get("AuthToken") {
                    Ok(ApiResponse {
                        success: true,
                        data: Some(json!({
                            "auth_token": auth_token,
                            "user_id": user_id,
                            "raw": data
                        })),
                        error: None,
                        timestamp,
                    })
                } else {
                    Ok(ApiResponse {
                        success: false,
                        data: None,
                        error: Some("Auth token not found in response".to_string()),
                        timestamp,
                    })
                }
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Authentication failed")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Validate auth token
#[command]
pub async fn motilal_validate_token(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    // Try to fetch order book to validate token
    match motilal_request(&client, "POST", "/rest/book/v2/getorderbook", &headers, Some(json!({}))).await {
        Ok(data) => {
            let is_valid = data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS");
            Ok(ApiResponse {
                success: is_valid,
                data: Some(json!({ "valid": is_valid })),
                error: if is_valid { None } else { Some("Token validation failed".to_string()) },
                timestamp,
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

// ============================================================================
// ORDER COMMANDS
// ============================================================================

/// Place order with Motilal Oswal
#[command]
pub async fn motilal_place_order(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    exchange: String,
    symbol_token: i64,
    buy_or_sell: String,
    order_type: String,
    product_type: String,
    quantity: i64,
    price: f64,
    trigger_price: f64,
    disclosed_quantity: i64,
    validity: String,
    amo: bool,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    let payload = json!({
        "exchange": exchange,
        "symboltoken": symbol_token,
        "buyorsell": buy_or_sell.to_uppercase(),
        "ordertype": order_type.to_uppercase(),
        "producttype": product_type.to_uppercase(),
        "orderduration": validity.to_uppercase(),
        "price": price,
        "triggerprice": trigger_price,
        "quantityinlot": quantity,
        "disclosedquantity": disclosed_quantity,
        "amoorder": if amo { "Y" } else { "N" }
    });

    match motilal_request(&client, "POST", "/rest/trans/v1/placeorder", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let order_id = data.get("uniqueorderid")
                    .and_then(|o| o.as_str())
                    .unwrap_or("");
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "order_id": order_id,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Order placement failed")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Modify order with Motilal Oswal
#[command]
pub async fn motilal_modify_order(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    order_id: String,
    order_type: String,
    quantity: i64,
    price: f64,
    trigger_price: f64,
    disclosed_quantity: i64,
    last_modified_time: String,
    qty_traded_today: i64,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    let payload = json!({
        "uniqueorderid": order_id,
        "newordertype": order_type.to_uppercase(),
        "neworderduration": "DAY",
        "newprice": price,
        "newtriggerprice": trigger_price,
        "newquantityinlot": quantity,
        "newdisclosedquantity": disclosed_quantity,
        "newgoodtilldate": "",
        "lastmodifiedtime": last_modified_time,
        "qtytradedtoday": qty_traded_today
    });

    match motilal_request(&client, "POST", "/rest/trans/v2/modifyorder", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let order_id = data.get("uniqueorderid")
                    .and_then(|o| o.as_str())
                    .unwrap_or("");
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "order_id": order_id,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Order modification failed")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Cancel order with Motilal Oswal
#[command]
pub async fn motilal_cancel_order(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    order_id: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    let payload = json!({
        "uniqueorderid": order_id
    });

    match motilal_request(&client, "POST", "/rest/trans/v1/cancelorder", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "order_id": order_id,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Order cancellation failed")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Get order book from Motilal Oswal
#[command]
pub async fn motilal_get_orders(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/book/v2/getorderbook", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let orders = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "orders": orders,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "orders": [] })),
                    error: None,
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Get trade book from Motilal Oswal
#[command]
pub async fn motilal_get_trade_book(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/book/v1/gettradebook", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let trades = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "trades": trades,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "trades": [] })),
                    error: None,
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

// ============================================================================
// PORTFOLIO COMMANDS
// ============================================================================

/// Get positions from Motilal Oswal
#[command]
pub async fn motilal_get_positions(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/book/v1/getposition", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let positions = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "positions": positions,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "positions": [] })),
                    error: None,
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Get holdings from Motilal Oswal
#[command]
pub async fn motilal_get_holdings(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/report/v1/getdpholding", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let holdings = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "holdings": holdings,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "holdings": [] })),
                    error: None,
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Get funds/margin from Motilal Oswal
#[command]
pub async fn motilal_get_funds(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/report/v1/getreportmargindetail", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                // Parse margin data from Motilal response
                let data_items = data.get("data").and_then(|d| d.as_array());

                let mut available_cash = 0.0;
                let mut collateral = 0.0;
                let mut used_margin = 0.0;
                let mut unrealized_pnl = 0.0;
                let mut realized_pnl = 0.0;

                if let Some(items) = data_items {
                    for item in items {
                        let srno = item.get("srno").and_then(|s| s.as_i64()).unwrap_or(0);
                        let amount = item.get("amount").and_then(|a| a.as_f64()).unwrap_or(0.0);

                        match srno {
                            102 => available_cash = amount, // Available for Cash/SLBM
                            220 => collateral = amount,     // Non-Cash Balance
                            201 => {
                                if available_cash == 0.0 {
                                    available_cash = amount;
                                }
                            }
                            300 => used_margin = amount,    // Margin Usage Details
                            600 => unrealized_pnl = amount, // Total MTM P&L
                            700 => realized_pnl = amount,   // Booked P&L
                            _ => {}
                        }
                    }
                }

                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "availablecash": available_cash,
                        "collateral": collateral,
                        "utiliseddebits": used_margin,
                        "m2munrealized": unrealized_pnl,
                        "m2mrealized": realized_pnl,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: false,
                    data: Some(json!({
                        "availablecash": 0.0,
                        "collateral": 0.0,
                        "utiliseddebits": 0.0,
                        "m2munrealized": 0.0,
                        "m2mrealized": 0.0
                    })),
                    error: data.get("message").and_then(|m| m.as_str()).map(|s| s.to_string()),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

// ============================================================================
// MARKET DATA COMMANDS
// ============================================================================

/// Get LTP/quotes from Motilal Oswal
#[command]
pub async fn motilal_get_quotes(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    exchange: String,
    symbol_token: i64,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    let payload = json!({
        "exchange": exchange,
        "scripcode": symbol_token
    });

    match motilal_request(&client, "POST", "/rest/report/v1/getltpdata", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let quote_data = data.get("data").cloned().unwrap_or(json!({}));

                // Motilal returns values in paisa, convert to rupees
                let convert = |v: Option<&Value>| -> f64 {
                    v.and_then(|x| x.as_f64()).unwrap_or(0.0) / 100.0
                };

                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "bid": convert(quote_data.get("bid")),
                        "ask": convert(quote_data.get("ask")),
                        "open": convert(quote_data.get("open")),
                        "high": convert(quote_data.get("high")),
                        "low": convert(quote_data.get("low")),
                        "ltp": convert(quote_data.get("ltp")),
                        "prev_close": convert(quote_data.get("close")),
                        "volume": quote_data.get("volume").and_then(|v| v.as_i64()).unwrap_or(0),
                        "oi": 0,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Failed to fetch quotes")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Get historical data - Note: Motilal doesn't support historical data with date ranges
#[command]
pub async fn motilal_get_history(
    _auth_token: String,
    _api_key: String,
    _vendor_code: String,
    _exchange: String,
    _symbol_token: i64,
    _interval: String,
    _from_date: String,
    _to_date: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();

    // Motilal Oswal does not support historical data API
    Ok(ApiResponse {
        success: false,
        data: Some(json!({ "candles": [] })),
        error: Some("Historical data not supported by Motilal Oswal API".to_string()),
        timestamp,
    })
}

/// Get market depth - Note: Requires WebSocket for real-time depth
#[command]
pub async fn motilal_get_depth(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    exchange: String,
    symbol_token: i64,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();

    // Use LTP API as a fallback - full depth requires WebSocket
    let quotes_result = motilal_get_quotes(
        auth_token,
        api_key,
        vendor_code,
        exchange,
        symbol_token,
    ).await?;

    if quotes_result.success {
        if let Some(quote_data) = quotes_result.data {
            let bid = quote_data.get("bid").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let ask = quote_data.get("ask").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let ltp = quote_data.get("ltp").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let high = quote_data.get("high").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let low = quote_data.get("low").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let open = quote_data.get("open").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let prev_close = quote_data.get("prev_close").and_then(|v| v.as_f64()).unwrap_or(0.0);
            let volume = quote_data.get("volume").and_then(|v| v.as_i64()).unwrap_or(0);

            return Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "bids": [{"price": bid, "quantity": 0}],
                    "asks": [{"price": ask, "quantity": 0}],
                    "high": high,
                    "low": low,
                    "ltp": ltp,
                    "ltq": 0,
                    "open": open,
                    "prev_close": prev_close,
                    "volume": volume,
                    "oi": 0,
                    "totalbuyqty": 0,
                    "totalsellqty": 0
                })),
                error: None,
                timestamp,
            });
        }
    }

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Failed to fetch market depth".to_string()),
        timestamp,
    })
}

// ============================================================================
// BULK OPERATIONS
// ============================================================================

/// Close all positions
#[command]
pub async fn motilal_close_all_positions(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();

    // Get positions first
    let positions_result = motilal_get_positions(
        auth_token.clone(),
        api_key.clone(),
        vendor_code.clone(),
    ).await?;

    if !positions_result.success {
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch positions".to_string()),
            timestamp,
        });
    }

    let positions = positions_result.data
        .and_then(|d| d.get("positions").cloned())
        .and_then(|p| p.as_array().cloned())
        .unwrap_or_default();

    if positions.is_empty() {
        return Ok(ApiResponse {
            success: true,
            data: Some(json!({ "message": "No open positions to close" })),
            error: None,
            timestamp,
        });
    }

    let mut closed_count = 0;
    let mut failed_count = 0;

    for position in positions {
        let buy_qty = position.get("buyquantity").and_then(|v| v.as_i64()).unwrap_or(0);
        let sell_qty = position.get("sellquantity").and_then(|v| v.as_i64()).unwrap_or(0);
        let net_qty = buy_qty - sell_qty;

        if net_qty == 0 {
            continue;
        }

        let symbol_token = position.get("symboltoken").and_then(|v| v.as_i64()).unwrap_or(0);
        let exchange = position.get("exchange").and_then(|v| v.as_str()).unwrap_or("NSE");
        let product_type = position.get("productname").and_then(|v| v.as_str()).unwrap_or("NORMAL");

        let (action, quantity) = if net_qty > 0 {
            ("SELL", net_qty)
        } else {
            ("BUY", -net_qty)
        };

        let result = motilal_place_order(
            auth_token.clone(),
            api_key.clone(),
            vendor_code.clone(),
            exchange.to_string(),
            symbol_token,
            action.to_string(),
            "MARKET".to_string(),
            product_type.to_string(),
            quantity,
            0.0,
            0.0,
            0,
            "DAY".to_string(),
            false,
        ).await;

        match result {
            Ok(response) if response.success => closed_count += 1,
            _ => failed_count += 1,
        }
    }

    Ok(ApiResponse {
        success: failed_count == 0,
        data: Some(json!({
            "closed": closed_count,
            "failed": failed_count
        })),
        error: if failed_count > 0 { Some(format!("{} positions failed to close", failed_count)) } else { None },
        timestamp,
    })
}

/// Cancel all open orders
#[command]
pub async fn motilal_cancel_all_orders(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();

    // Get orders first
    let orders_result = motilal_get_orders(
        auth_token.clone(),
        api_key.clone(),
        vendor_code.clone(),
    ).await?;

    if !orders_result.success {
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch orders".to_string()),
            timestamp,
        });
    }

    let orders = orders_result.data
        .and_then(|d| d.get("orders").cloned())
        .and_then(|o| o.as_array().cloned())
        .unwrap_or_default();

    // Filter open orders (Motilal uses 'Confirm', 'Sent', 'Open' for open orders)
    let open_orders: Vec<_> = orders.iter()
        .filter(|o| {
            let status = o.get("orderstatus")
                .and_then(|s| s.as_str())
                .unwrap_or("")
                .to_lowercase();
            status == "confirm" || status == "sent" || status == "open"
        })
        .collect();

    if open_orders.is_empty() {
        return Ok(ApiResponse {
            success: true,
            data: Some(json!({ "message": "No open orders to cancel" })),
            error: None,
            timestamp,
        });
    }

    let mut cancelled_count = 0;
    let mut failed_count = 0;

    for order in open_orders {
        let order_id = order.get("uniqueorderid")
            .and_then(|v| v.as_str())
            .unwrap_or("");

        if order_id.is_empty() {
            failed_count += 1;
            continue;
        }

        let result = motilal_cancel_order(
            auth_token.clone(),
            api_key.clone(),
            vendor_code.clone(),
            order_id.to_string(),
        ).await;

        match result {
            Ok(response) if response.success => cancelled_count += 1,
            _ => failed_count += 1,
        }
    }

    Ok(ApiResponse {
        success: failed_count == 0,
        data: Some(json!({
            "cancelled": cancelled_count,
            "failed": failed_count
        })),
        error: if failed_count > 0 { Some(format!("{} orders failed to cancel", failed_count)) } else { None },
        timestamp,
    })
}

// ============================================================================
// MASTER CONTRACT
// ============================================================================

/// Download master contract - Motilal uses contract download endpoint
#[command]
pub async fn motilal_download_master_contract(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    exchange: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    // Motilal master contract endpoint
    let payload = json!({
        "exchange": exchange
    });

    match motilal_request(&client, "POST", "/rest/report/v1/getcontractmaster", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                Ok(ApiResponse {
                    success: true,
                    data: Some(data),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Failed to download master contract")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Search symbols in master contract
#[command]
pub async fn motilal_search_symbols(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    search_text: String,
    _exchange: Option<String>,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    // Motilal symbol search endpoint
    let payload = json!({
        "symbol": search_text
    });

    match motilal_request(&client, "POST", "/rest/report/v1/getsearchscrip", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let symbols = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "symbols": symbols,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "symbols": [] })),
                    error: None,
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}
