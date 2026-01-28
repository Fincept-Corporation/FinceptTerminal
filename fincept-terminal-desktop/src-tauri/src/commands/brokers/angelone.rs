//! AngelOne (Angel Broking) API Commands
//!
//! Authentication, order management, market data, and portfolio for AngelOne
//! API Reference: https://smartapi.angelone.in/docs

use reqwest::Client;
use serde_json::{json, Value};
use super::common::ApiResponse;
use totp_rs::{Algorithm, TOTP, Secret};
use crate::database::master_contract;

/// Base URL for AngelOne API
const ANGELONE_BASE_URL: &str = "https://apiconnect.angelone.in";

// ============================================================================
// Helper Functions
// ============================================================================

/// Build a reqwest Client with standard AngelOne headers
fn build_angel_request(
    client: &Client,
    url: &str,
    method: &str,
    api_key: &str,
    access_token: Option<&str>,
) -> reqwest::RequestBuilder {
    let builder = match method {
        "POST" => client.post(url),
        "PUT" => client.put(url),
        "DELETE" => client.delete(url),
        _ => client.get(url),
    };

    let mut builder = builder
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("X-UserType", "USER")
        .header("X-SourceID", "WEB")
        .header("X-ClientLocalIP", "127.0.0.1")
        .header("X-ClientPublicIP", "127.0.0.1")
        .header("X-MACAddress", "00:00:00:00:00:00")
        .header("X-PrivateKey", api_key);

    if let Some(token) = access_token {
        builder = builder.header("Authorization", format!("Bearer {}", token));
    }

    builder
}

/// Make an authenticated API call and return parsed JSON
async fn angel_api_call(
    api_key: &str,
    access_token: &str,
    endpoint: &str,
    method: &str,
    payload: Option<Value>,
) -> Result<Value, String> {
    let client = Client::new();
    let url = format!("{}{}", ANGELONE_BASE_URL, endpoint);

    let mut builder = build_angel_request(&client, &url, method, api_key, Some(access_token));

    if let Some(body) = payload {
        builder = builder.json(&body);
    }

    let response = builder
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let text = response
        .text()
        .await
        .map_err(|e| format!("Failed to read response body: {}", e))?;

    if text.is_empty() {
        return Err(format!("Empty response from AngelOne API (HTTP {})", status.as_u16()));
    }

    let body: Value = serde_json::from_str(&text)
        .map_err(|e| format!("Failed to parse JSON (HTTP {}): {} — body: {}", status.as_u16(), e, &text[..text.len().min(200)]))?;

    if !status.is_success() {
        let msg = body.get("message")
            .and_then(|v| v.as_str())
            .unwrap_or("API request failed");
        return Err(format!("{} (HTTP {})", msg, status.as_u16()));
    }

    Ok(body)
}

/// Generate a 6-digit TOTP code from a base32-encoded secret string.
/// If the input is already a 6-digit code, returns it as-is (backward compat).
fn generate_totp_code(totp_input: &str) -> Result<String, String> {
    let trimmed = totp_input.trim();

    // If it's already a 6-digit numeric code, pass through directly
    if trimmed.len() == 6 && trimmed.chars().all(|c| c.is_ascii_digit()) {
        return Ok(trimmed.to_string());
    }

    // Otherwise, treat as a base32 TOTP secret and generate the code
    let secret_bytes = Secret::Encoded(trimmed.to_string())
        .to_bytes()
        .map_err(|e| format!("Invalid TOTP secret: {}", e))?;

    let totp = TOTP::new(
        Algorithm::SHA1,
        6,   // digits
        1,   // skew
        30,  // step (seconds)
        secret_bytes,
    ).map_err(|e| format!("TOTP init failed: {}", e))?;

    totp.generate_current()
        .map_err(|e| format!("TOTP generation failed: {}", e))
}

// ============================================================================
// Authentication
// ============================================================================

/// AngelOne login - authenticate with client code, password (PIN), and TOTP secret
#[tauri::command]
pub async fn angelone_login(
    api_key: String,
    client_code: String,
    password: String,
    totp: String,
) -> ApiResponse<Value> {
    let client = Client::new();
    let timestamp = chrono::Utc::now().timestamp_millis();

    let totp_code = match generate_totp_code(&totp) {
        Ok(code) => code,
        Err(e) => {
            return ApiResponse {
                success: false,
                data: None,
                error: Some(format!("TOTP error: {}", e)),
                timestamp,
            };
        }
    };

    let payload = json!({
        "clientcode": client_code,
        "password": password,
        "totp": totp_code
    });

    let url = format!("{}/rest/auth/angelbroking/user/v1/loginByPassword", ANGELONE_BASE_URL);

    match build_angel_request(&client, &url, "POST", &api_key, None)
        .json(&payload)
        .send()
        .await
    {
        Ok(response) => {
            let status = response.status();

            match response.json::<Value>().await {
                Ok(data) => {
                    if status.is_success() {
                        if let Some(data_obj) = data.get("data") {
                            let jwt_token = data_obj.get("jwtToken")
                                .and_then(|v| v.as_str())
                                .unwrap_or("");
                            let feed_token = data_obj.get("feedToken")
                                .and_then(|v| v.as_str());
                            let refresh_token = data_obj.get("refreshToken")
                                .and_then(|v| v.as_str());

                            if !jwt_token.is_empty() {
                                return ApiResponse {
                                    success: true,
                                    data: Some(json!({
                                        "access_token": jwt_token,
                                        "feed_token": feed_token,
                                        "refresh_token": refresh_token,
                                        "user_id": client_code
                                    })),
                                    error: None,
                                    timestamp,
                                };
                            }
                        }

                        let error_msg = data.get("message")
                            .and_then(|v| v.as_str())
                            .unwrap_or("Authentication failed");

                        ApiResponse {
                            success: false,
                            data: None,
                            error: Some(error_msg.to_string()),
                            timestamp,
                        }
                    } else {
                        let error_msg = data.get("message")
                            .and_then(|v| v.as_str())
                            .map(|s| s.to_string())
                            .unwrap_or_else(|| format!("HTTP error: {}", status));

                        ApiResponse {
                            success: false,
                            data: None,
                            error: Some(error_msg),
                            timestamp,
                        }
                    }
                }
                Err(e) => ApiResponse {
                    success: false,
                    data: None,
                    error: Some(format!("Failed to parse response: {}", e)),
                    timestamp,
                },
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Request failed: {}", e)),
            timestamp,
        },
    }
}

/// Validate AngelOne access token
#[tauri::command]
pub async fn angelone_validate_token(
    auth_token: String,
    api_key: String,
) -> ApiResponse<bool> {
    let client = Client::new();
    let timestamp = chrono::Utc::now().timestamp_millis();

    let url = format!("{}/rest/secure/angelbroking/user/v1/getProfile", ANGELONE_BASE_URL);

    match build_angel_request(&client, &url, "GET", &api_key, Some(&auth_token))
        .send()
        .await
    {
        Ok(response) => {
            let is_valid = response.status().is_success();
            ApiResponse {
                success: is_valid,
                data: Some(is_valid),
                error: if is_valid { None } else { Some("Token validation failed".to_string()) },
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: Some(false),
            error: Some(format!("Validation request failed: {}", e)),
            timestamp,
        },
    }
}

/// Refresh AngelOne session token
#[tauri::command]
pub async fn angelone_refresh_token(
    api_key: String,
    refresh_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let payload = json!({
        "refreshToken": refresh_token
    });

    match angel_api_call(
        &api_key,
        &refresh_token,
        "/rest/auth/angelbroking/jwt/v1/generateTokens",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            if let Some(data) = body.get("data") {
                let jwt = data.get("jwtToken")
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                let feed = data.get("feedToken")
                    .and_then(|v| v.as_str());
                let refresh = data.get("refreshToken")
                    .and_then(|v| v.as_str());

                if !jwt.is_empty() {
                    return ApiResponse {
                        success: true,
                        data: Some(json!({
                            "access_token": jwt,
                            "feed_token": feed,
                            "refresh_token": refresh,
                        })),
                        error: None,
                        timestamp,
                    };
                }
            }
            ApiResponse {
                success: false,
                data: None,
                error: Some("Token refresh failed".to_string()),
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

// ============================================================================
// Symbol Lookup (delegates to master contract DB)
// ============================================================================

/// Search AngelOne symbols from local master contract DB
#[tauri::command]
pub async fn angelone_search_symbols(
    _api_key: String,
    _access_token: String,
    query: String,
    exchange: Option<String>,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match master_contract::search_symbols(
        "angelone",
        &query,
        exchange.as_deref(),
        None,
        50,
    ) {
        Ok(results) => {
            let data: Vec<Value> = results.iter().map(|r| {
                json!({
                    "symbol": r.symbol,
                    "tradingsymbol": r.br_symbol,
                    "name": r.name,
                    "exchange": r.exchange,
                    "symboltoken": r.token,
                    "token": r.token,
                    "instrumenttype": r.instrument_type,
                    "lotsize": r.lot_size,
                    "ticksize": r.tick_size,
                    "expiry": r.expiry,
                    "strike": r.strike,
                })
            }).collect();

            ApiResponse {
                success: true,
                data: Some(json!(data)),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Search failed: {}", e)),
            timestamp,
        },
    }
}

/// Get a specific instrument from local master contract DB
#[tauri::command]
pub async fn angelone_get_instrument(
    _api_key: String,
    symbol: String,
    exchange: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match master_contract::get_symbol("angelone", &symbol, &exchange) {
        Ok(Some(result)) => {
            ApiResponse {
                success: true,
                data: Some(json!({
                    "symbol": result.symbol,
                    "tradingsymbol": result.br_symbol,
                    "name": result.name,
                    "exchange": result.exchange,
                    "symboltoken": result.token,
                    "token": result.token,
                    "instrumenttype": result.instrument_type,
                    "lotsize": result.lot_size,
                    "ticksize": result.tick_size,
                    "expiry": result.expiry,
                    "strike": result.strike,
                })),
                error: None,
                timestamp,
            }
        }
        Ok(None) => {
            // Try fuzzy search as fallback
            match master_contract::search_symbols("angelone", &symbol, Some(&exchange), None, 1) {
                Ok(results) if !results.is_empty() => {
                    let r = &results[0];
                    ApiResponse {
                        success: true,
                        data: Some(json!({
                            "symbol": r.symbol,
                            "tradingsymbol": r.br_symbol,
                            "name": r.name,
                            "exchange": r.exchange,
                            "symboltoken": r.token,
                            "token": r.token,
                            "instrumenttype": r.instrument_type,
                            "lotsize": r.lot_size,
                            "ticksize": r.tick_size,
                            "expiry": r.expiry,
                            "strike": r.strike,
                        })),
                        error: None,
                        timestamp,
                    }
                }
                _ => ApiResponse {
                    success: false,
                    data: None,
                    error: Some(format!("Instrument not found: {} on {}", symbol, exchange)),
                    timestamp,
                },
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Lookup failed: {}", e)),
            timestamp,
        },
    }
}

/// Download master contract (wrapper that matches the TS adapter's expected command name)
#[tauri::command]
pub async fn angelone_download_master_contract(
    _api_key: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Delegate to the existing master contract download
    match crate::commands::master_contract::download_angelone_master_contract().await {
        Ok(result) => ApiResponse {
            success: result.success,
            data: Some(json!({
                "symbol_count": result.symbol_count,
                "message": result.message,
            })),
            error: None,
            timestamp,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Get master contract info
#[tauri::command]
pub async fn angelone_master_contract_info() -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match master_contract::get_master_contract("angelone") {
        Ok(Some(info)) => {
            let now = chrono::Utc::now().timestamp();
            let cache_age = now - info.last_updated;
            ApiResponse {
                success: true,
                data: Some(json!({
                    "timestamp": info.last_updated,
                    "symbol_count": info.symbol_count,
                    "cache_age_seconds": cache_age,
                })),
                error: None,
                timestamp,
            }
        }
        Ok(None) => ApiResponse {
            success: false,
            data: None,
            error: Some("No master contract data found".to_string()),
            timestamp,
        },
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to get info: {}", e)),
            timestamp,
        },
    }
}

// ============================================================================
// Market Data
// ============================================================================

/// Get market quotes (supports single and multiple tokens)
/// AngelOne API: POST /rest/secure/angelbroking/market/v1/quote/
#[tauri::command]
pub async fn angelone_get_quote(
    api_key: String,
    access_token: String,
    exchange: String,
    tokens: Vec<String>,
    mode: Option<String>,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();
    let quote_mode = mode.unwrap_or_else(|| "FULL".to_string());

    eprintln!("[AngelOne Quote] exchange={}, tokens={:?}", exchange, tokens);

    let payload = json!({
        "mode": quote_mode,
        "exchangeTokens": {
            exchange: tokens
        }
    });

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/market/v1/quote/",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            if body.get("status").and_then(|v| v.as_bool()) == Some(true) ||
               body.get("status").and_then(|v| v.as_str()) == Some("true") {
                ApiResponse {
                    success: true,
                    data: body.get("data").cloned(),
                    error: None,
                    timestamp,
                }
            } else {
                let msg = body.get("message")
                    .and_then(|v| v.as_str())
                    .unwrap_or("Quote fetch failed")
                    .to_string();
                ApiResponse {
                    success: false,
                    data: None,
                    error: Some(msg),
                    timestamp,
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Get historical OHLCV candle data
/// AngelOne API: POST /rest/secure/angelbroking/historical/v1/getCandleData
#[tauri::command]
pub async fn angelone_get_historical(
    api_key: String,
    access_token: String,
    exchange: String,
    symbol_token: String,
    interval: String,
    from_date: String,
    to_date: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let payload = json!({
        "exchange": exchange,
        "symboltoken": symbol_token,
        "interval": interval,
        "fromdate": from_date,
        "todate": to_date
    });

    eprintln!("[AngelOne Historical] exchange={}, token={}, interval={}, from={}, to={}", exchange, symbol_token, interval, from_date, to_date);

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/historical/v1/getCandleData",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            if body.get("status").and_then(|v| v.as_bool()) == Some(true) ||
               body.get("status").and_then(|v| v.as_str()) == Some("true") {
                // data is array of arrays: [[timestamp, open, high, low, close, volume], ...]
                let candle_data = body.get("data").cloned().unwrap_or(json!([]));
                ApiResponse {
                    success: true,
                    data: Some(candle_data),
                    error: None,
                    timestamp,
                }
            } else {
                let msg = body.get("message")
                    .and_then(|v| v.as_str())
                    .unwrap_or("No historical data available")
                    .to_string();
                ApiResponse {
                    success: false,
                    data: None,
                    error: Some(msg),
                    timestamp,
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

// ============================================================================
// Orders
// ============================================================================

/// Place an order
/// AngelOne API: POST /rest/secure/angelbroking/order/v1/placeOrder
#[tauri::command]
pub async fn angelone_place_order(
    api_key: String,
    access_token: String,
    params: Value,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Extract variety from params, default to NORMAL
    let variety = params.get("variety")
        .and_then(|v| v.as_str())
        .unwrap_or("NORMAL")
        .to_string();

    let endpoint = "/rest/secure/angelbroking/order/v1/placeOrder";

    match angel_api_call(
        &api_key,
        &access_token,
        endpoint,
        "POST",
        Some(params),
    ).await {
        Ok(body) => {
            if body.get("status").and_then(|v| v.as_bool()) == Some(true) ||
               body.get("status").and_then(|v| v.as_str()) == Some("true") {
                let order_id = body.get("data")
                    .and_then(|d| d.get("orderid"))
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                ApiResponse {
                    success: true,
                    data: Some(json!({
                        "order_id": order_id,
                        "variety": variety,
                    })),
                    error: None,
                    timestamp,
                }
            } else {
                let msg = body.get("message")
                    .and_then(|v| v.as_str())
                    .unwrap_or("Order placement failed")
                    .to_string();
                ApiResponse {
                    success: false,
                    data: None,
                    error: Some(msg),
                    timestamp,
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Modify an existing order
/// AngelOne API: POST /rest/secure/angelbroking/order/v1/modifyOrder
#[tauri::command]
pub async fn angelone_modify_order(
    api_key: String,
    access_token: String,
    params: Value,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/order/v1/modifyOrder",
        "POST",
        Some(params),
    ).await {
        Ok(body) => {
            if body.get("status").and_then(|v| v.as_bool()) == Some(true) ||
               body.get("status").and_then(|v| v.as_str()) == Some("true") {
                let order_id = body.get("data")
                    .and_then(|d| d.get("orderid"))
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                ApiResponse {
                    success: true,
                    data: Some(json!({ "order_id": order_id })),
                    error: None,
                    timestamp,
                }
            } else {
                let msg = body.get("message")
                    .and_then(|v| v.as_str())
                    .unwrap_or("Order modification failed")
                    .to_string();
                ApiResponse {
                    success: false,
                    data: None,
                    error: Some(msg),
                    timestamp,
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Cancel an order
/// AngelOne API: POST /rest/secure/angelbroking/order/v1/cancelOrder
#[tauri::command]
pub async fn angelone_cancel_order(
    api_key: String,
    access_token: String,
    order_id: String,
    variety: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let payload = json!({
        "variety": variety,
        "orderid": order_id
    });

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/order/v1/cancelOrder",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            if body.get("status").and_then(|v| v.as_bool()) == Some(true) ||
               body.get("status").and_then(|v| v.as_str()) == Some("true") {
                let oid = body.get("data")
                    .and_then(|d| d.get("orderid"))
                    .and_then(|v| v.as_str())
                    .unwrap_or(&order_id);
                ApiResponse {
                    success: true,
                    data: Some(json!({ "order_id": oid })),
                    error: None,
                    timestamp,
                }
            } else {
                let msg = body.get("message")
                    .and_then(|v| v.as_str())
                    .unwrap_or("Order cancellation failed")
                    .to_string();
                ApiResponse {
                    success: false,
                    data: None,
                    error: Some(msg),
                    timestamp,
                }
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Get order book
/// AngelOne API: GET /rest/secure/angelbroking/order/v1/getOrderBook
#[tauri::command]
pub async fn angelone_get_order_book(
    api_key: String,
    access_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/order/v1/getOrderBook",
        "GET",
        None,
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!([]));
            ApiResponse {
                success: true,
                data: Some(data),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Get trade book
/// AngelOne API: GET /rest/secure/angelbroking/order/v1/getTradeBook
#[tauri::command]
pub async fn angelone_get_trade_book(
    api_key: String,
    access_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/order/v1/getTradeBook",
        "GET",
        None,
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!([]));
            ApiResponse {
                success: true,
                data: Some(data),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

// ============================================================================
// Portfolio
// ============================================================================

/// Get positions
/// AngelOne API: GET /rest/secure/angelbroking/order/v1/getPosition
#[tauri::command]
pub async fn angelone_get_positions(
    api_key: String,
    access_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/order/v1/getPosition",
        "GET",
        None,
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!([]));
            ApiResponse {
                success: true,
                data: Some(data),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Get holdings (portfolio)
/// AngelOne API: GET /rest/secure/angelbroking/portfolio/v1/getAllHolding
#[tauri::command]
pub async fn angelone_get_holdings(
    api_key: String,
    access_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/portfolio/v1/getAllHolding",
        "GET",
        None,
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!(null));
            ApiResponse {
                success: true,
                data: Some(data),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

/// Get funds / RMS (Risk Management System) limits
/// AngelOne API: GET /rest/secure/angelbroking/user/v1/getRMS
#[tauri::command]
pub async fn angelone_get_rms(
    api_key: String,
    access_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/user/v1/getRMS",
        "GET",
        None,
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!({}));
            ApiResponse {
                success: true,
                data: Some(data),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}

// ============================================================================
// Margin Calculator
// ============================================================================

/// Calculate margin for orders
/// AngelOne API: POST /rest/secure/angelbroking/margin/v1/batch
#[tauri::command]
pub async fn angelone_calculate_margin(
    api_key: String,
    access_token: String,
    orders: Value,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let payload = json!({
        "positions": orders
    });

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/margin/v1/batch",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!({}));
            ApiResponse {
                success: true,
                data: Some(data),
                error: None,
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}
