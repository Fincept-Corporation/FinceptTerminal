//! Alice Blue Broker Integration
//!
//! Complete API implementation for Alice Blue broker including:
//! - Authentication (SHA256 checksum-based session)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, holdings, margins)
//! - Market Data (quotes, historical)

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde_json::{json, Value};
use sha2::{Sha256, Digest};

use super::common::{ApiResponse, TokenExchangeResponse, OrderPlaceResponse};

// Alice Blue API Configuration
// ============================================================================

const ALICEBLUE_API_BASE: &str = "https://ant.aliceblueonline.com/rest/AliceBlueAPIService/api";

fn create_aliceblue_headers(api_secret: &str, session_id: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();

    let auth_value = format!("Bearer {} {}", api_secret, session_id);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers.insert("Accept", HeaderValue::from_static("application/json"));
    headers
}

/// Generate SHA256 checksum for authentication
fn generate_checksum(user_id: &str, api_secret: &str, enc_key: &str) -> String {
    let input = format!("{}{}{}", user_id, api_secret, enc_key);
    let mut hasher = Sha256::new();
    hasher.update(input.as_bytes());
    let result = hasher.finalize();
    hex::encode(result)
}

// ============================================================================
// Alice Blue Authentication Commands
// ============================================================================

/// Get session ID using user ID, API secret, and encryption key
#[tauri::command]
pub async fn aliceblue_get_session(
    user_id: String,
    api_secret: String,
    enc_key: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[aliceblue_get_session] Getting session for user {}", user_id);

    let client = reqwest::Client::new();

    // Generate SHA256 checksum: userId + apiSecret + encKey
    let checksum = generate_checksum(&user_id, &api_secret, &enc_key);

    let payload = json!({
        "userId": user_id,
        "userData": checksum,
        "source": "WEB"
    });

    let mut headers = HeaderMap::new();
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers.insert("Accept", HeaderValue::from_static("application/json"));

    let response = client
        .post(format!("{}/customer/getUserSID", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[aliceblue_get_session] Response: {:?}", body);

    if status.is_success() {
        // Check for sessionID in response
        if let Some(session_id) = body.get("sessionID").and_then(|s| s.as_str()) {
            return Ok(TokenExchangeResponse {
                success: true,
                access_token: Some(session_id.to_string()),
                user_id: Some(user_id),
                error: None,
            });
        }

        // Check for error message
        if let Some(error_msg) = body.get("emsg").and_then(|e| e.as_str()) {
            return Ok(TokenExchangeResponse {
                success: false,
                access_token: None,
                user_id: None,
                error: Some(error_msg.to_string()),
            });
        }

        // Check for stat field
        if body.get("stat").and_then(|s| s.as_str()) == Some("Not ok") {
            let error_msg = body.get("emsg")
                .and_then(|m| m.as_str())
                .unwrap_or("Authentication failed")
                .to_string();
            return Ok(TokenExchangeResponse {
                success: false,
                access_token: None,
                user_id: None,
                error: Some(error_msg),
            });
        }
    }

    Ok(TokenExchangeResponse {
        success: false,
        access_token: None,
        user_id: None,
        error: Some("Failed to get session ID".to_string()),
    })
}

/// Validate session by checking margins endpoint
#[tauri::command]
pub async fn aliceblue_validate_session(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[aliceblue_validate_session] Validating session");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/limits/getRmsLimits", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        let body: Value = response.json().await.unwrap_or(json!([]));

        // Check if response is an array with valid data
        let is_valid = if let Some(arr) = body.as_array() {
            !arr.is_empty() && arr.get(0).and_then(|v| v.get("stat")).and_then(|s| s.as_str()) != Some("Not_Ok")
        } else {
            false
        };

        Ok(ApiResponse {
            success: is_valid,
            data: Some(is_valid),
            error: if is_valid { None } else { Some("Session validation failed".to_string()) },
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Session validation failed".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Alice Blue Order Commands
// ============================================================================

/// Place an order
#[tauri::command]
pub async fn aliceblue_place_order(
    api_secret: String,
    session_id: String,
    trading_symbol: String,
    exchange: String,
    transaction_type: String,
    order_type: String,
    quantity: i32,
    price: Option<f64>,
    trigger_price: Option<f64>,
    product: String,
    token: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[aliceblue_place_order] Placing order for {}", trading_symbol);

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    // Map order type to Alice Blue format
    let ab_order_type = match order_type.as_str() {
        "MARKET" => "MKT",
        "LIMIT" => "L",
        "SL" | "STOP_LOSS" => "SL",
        "SL-M" | "STOP_LOSS_MARKET" => "SL-M",
        _ => "MKT",
    };

    // Build order payload (Alice Blue expects array)
    let order = json!({
        "complexty": "regular",
        "discqty": "0",
        "exch": exchange,
        "pCode": product,
        "prctyp": ab_order_type,
        "price": price.unwrap_or(0.0).to_string(),
        "qty": quantity.to_string(),
        "ret": "DAY",
        "symbol_id": token,
        "trading_symbol": trading_symbol,
        "transtype": transaction_type.to_uppercase(),
        "trigPrice": trigger_price.unwrap_or(0.0).to_string(),
        "orderTag": "fincept",
    });

    let payload = json!([order]);

    let response = client
        .post(format!("{}/placeOrder/executePlaceOrder", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[aliceblue_place_order] Response: {:?}", body);

    // Response is an array
    if status.is_success() {
        if let Some(arr) = body.as_array() {
            if let Some(first) = arr.get(0) {
                if first.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
                    let order_id = first.get("NOrdNo")
                        .and_then(|v| v.as_str())
                        .map(String::from);

                    return Ok(OrderPlaceResponse {
                        success: true,
                        order_id,
                        error: None,
                    });
                } else {
                    let error_msg = first.get("emsg")
                        .and_then(|m| m.as_str())
                        .unwrap_or("Order placement failed")
                        .to_string();
                    return Ok(OrderPlaceResponse {
                        success: false,
                        order_id: None,
                        error: Some(error_msg),
                    });
                }
            }
        }
    }

    Ok(OrderPlaceResponse {
        success: false,
        order_id: None,
        error: Some("Order placement failed".to_string()),
    })
}

/// Modify an order
#[tauri::command]
pub async fn aliceblue_modify_order(
    api_secret: String,
    session_id: String,
    order_id: String,
    trading_symbol: String,
    exchange: String,
    transaction_type: String,
    order_type: String,
    quantity: i32,
    price: Option<f64>,
    trigger_price: Option<f64>,
    product: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[aliceblue_modify_order] Modifying order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    // Map order type to Alice Blue format
    let ab_order_type = match order_type.as_str() {
        "MARKET" => "MKT",
        "LIMIT" => "L",
        "SL" | "STOP_LOSS" => "SL",
        "SL-M" | "STOP_LOSS_MARKET" => "SL-M",
        _ => "MKT",
    };

    let payload = json!({
        "discqty": 0,
        "exch": exchange,
        "filledQuantity": 0,
        "nestOrderNumber": order_id,
        "prctyp": ab_order_type,
        "price": price.unwrap_or(0.0),
        "qty": quantity,
        "trading_symbol": trading_symbol,
        "trigPrice": trigger_price.unwrap_or(0.0).to_string(),
        "transtype": transaction_type.to_uppercase(),
        "pCode": product,
    });

    let response = client
        .post(format!("{}/placeOrder/modifyOrder", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        let modified_order_id = body.get("nestOrderNumber")
            .and_then(|v| v.as_str())
            .map(String::from)
            .unwrap_or(order_id);

        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(modified_order_id),
            error: None,
        })
    } else {
        let error_msg = body.get("emsg")
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
pub async fn aliceblue_cancel_order(
    api_secret: String,
    session_id: String,
    order_id: String,
    trading_symbol: String,
    exchange: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[aliceblue_cancel_order] Cancelling order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let payload = json!({
        "exch": exchange,
        "nestOrderNumber": order_id,
        "trading_symbol": trading_symbol,
    });

    let response = client
        .post(format!("{}/placeOrder/cancelOrder", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        let cancelled_order_id = body.get("nestOrderNumber")
            .and_then(|v| v.as_str())
            .map(String::from)
            .unwrap_or(order_id);

        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(cancelled_order_id),
            error: None,
        })
    } else {
        let error_msg = body.get("emsg")
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
pub async fn aliceblue_get_orders(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_orders] Fetching order book");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/placeOrder/fetchOrderBook", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Response can be array of orders or error object
    if let Some(orders) = body.as_array() {
        // Check if first item is error
        if let Some(first) = orders.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: true,
                    data: Some(vec![]),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(orders.clone()),
            error: None,
            timestamp,
        })
    } else if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch order book".to_string()),
            timestamp,
        })
    }
}

/// Get trade book
#[tauri::command]
pub async fn aliceblue_get_trades(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_trades] Fetching trade book");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/placeOrder/fetchTradeBook", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if let Some(trades) = body.as_array() {
        if let Some(first) = trades.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: true,
                    data: Some(vec![]),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(trades.clone()),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    }
}

// ============================================================================
// Alice Blue Portfolio Commands
// ============================================================================

/// Get positions
#[tauri::command]
pub async fn aliceblue_get_positions(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let payload = json!({
        "ret": "NET"
    });

    let response = client
        .post(format!("{}/positionAndHoldings/positionBook", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if let Some(positions) = body.as_array() {
        if let Some(first) = positions.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: true,
                    data: Some(vec![]),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(positions.clone()),
            error: None,
            timestamp,
        })
    } else if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    }
}

/// Get holdings
#[tauri::command]
pub async fn aliceblue_get_holdings(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/positionAndHoldings/holdings", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if let Some(holdings) = body.as_array() {
        if let Some(first) = holdings.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: true,
                    data: Some(vec![]),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(holdings.clone()),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    }
}

/// Get margin/funds data
#[tauri::command]
pub async fn aliceblue_get_margins(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[aliceblue_get_margins] Fetching margin data");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/limits/getRmsLimits", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if let Some(margins) = body.as_array() {
        if let Some(first) = margins.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: false,
                    data: None,
                    error: first.get("emsg").and_then(|m| m.as_str()).map(String::from),
                    timestamp,
                });
            }

            // Transform to standard format
            let available_cash = first.get("net")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let collateral = first.get("collateralvalue")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let margin_used = first.get("cncMarginUsed")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let unrealized_mtm = first.get("unrealizedMtomPrsnt")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let realized_mtm = first.get("realizedMtomPrsnt")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);

            let formatted_margins = json!({
                "availablecash": format!("{:.2}", available_cash),
                "collateral": format!("{:.2}", collateral),
                "utiliseddebits": format!("{:.2}", margin_used),
                "m2munrealized": format!("{:.2}", unrealized_mtm),
                "m2mrealized": format!("{:.2}", realized_mtm),
                "raw": first,
            });

            return Ok(ApiResponse {
                success: true,
                data: Some(formatted_margins),
                error: None,
                timestamp,
            });
        }
    }

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Failed to fetch margins".to_string()),
        timestamp,
    })
}

// ============================================================================
// Alice Blue Market Data Commands
// ============================================================================

/// Get quote for a symbol
#[tauri::command]
pub async fn aliceblue_get_quote(
    api_secret: String,
    session_id: String,
    exchange: String,
    token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[aliceblue_get_quote] Fetching quote for token {}", token);

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let payload = json!({
        "exch": exchange,
        "symbol": token,
    });

    let response = client
        .post(format!("{}/ScripDetails/getScripQuoteDetails", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch quote")
            .to_string();
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        });
    }

    // Transform to standard quote format
    let ltp = body.get("ltp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let open = body.get("open").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let high = body.get("high").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let low = body.get("low").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let close = body.get("close").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let volume = body.get("volume").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0);
    let bid = body.get("bp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let ask = body.get("sp").and_then(|v| v.as_str()).and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
    let oi = body.get("oi").and_then(|v| v.as_str()).and_then(|s| s.parse::<i64>().ok()).unwrap_or(0);

    let change = ltp - close;
    let change_percent = if close > 0.0 { (change / close) * 100.0 } else { 0.0 };

    let quote = json!({
        "symbol": body.get("symbol").and_then(|v| v.as_str()).unwrap_or(""),
        "exchange": exchange,
        "token": token,
        "ltp": ltp,
        "open": open,
        "high": high,
        "low": low,
        "close": close,
        "prev_close": close,
        "volume": volume,
        "change": change,
        "change_percent": change_percent,
        "bid": bid,
        "ask": ask,
        "oi": oi,
        "raw": body,
    });

    Ok(ApiResponse {
        success: true,
        data: Some(quote),
        error: None,
        timestamp,
    })
}

/// Get historical candle data
#[tauri::command]
pub async fn aliceblue_get_historical(
    api_secret: String,
    session_id: String,
    user_id: String,
    token: String,
    exchange: String,
    interval: String,
    start_time: i64,
    end_time: i64,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_historical] Fetching historical data for token {}", token);

    let client = reqwest::Client::new();

    // Historical API uses different auth format: Bearer {user_id} {session_token}
    let mut headers = HeaderMap::new();
    let auth_value = format!("Bearer {} {}", user_id, session_id);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));

    // Map interval to Alice Blue format
    let ab_resolution = match interval.as_str() {
        "1m" | "1" => "1",
        "D" | "day" | "1d" => "D",
        _ => "D",
    };

    let payload = json!({
        "token": token,
        "exchange": exchange,
        "from": start_time.to_string(),
        "to": end_time.to_string(),
        "resolution": ab_resolution,
    });

    let response = client
        .post(format!("{}/chart/history", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch historical data")
            .to_string();
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        });
    }

    if let Some(result) = body.get("result").and_then(|r| r.as_array()) {
        // Transform candles to standard OHLCV format
        let formatted_candles: Vec<Value> = result.iter().map(|candle| {
            json!({
                "timestamp": candle.get("time").and_then(|v| v.as_str()).unwrap_or("0"),
                "open": candle.get("open").and_then(|v| v.as_f64()).unwrap_or(0.0),
                "high": candle.get("high").and_then(|v| v.as_f64()).unwrap_or(0.0),
                "low": candle.get("low").and_then(|v| v.as_f64()).unwrap_or(0.0),
                "close": candle.get("close").and_then(|v| v.as_f64()).unwrap_or(0.0),
                "volume": candle.get("volume").and_then(|v| v.as_i64()).unwrap_or(0),
            })
        }).collect();

        Ok(ApiResponse {
            success: true,
            data: Some(formatted_candles),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("No historical data available".to_string()),
            timestamp,
        })
    }
}
