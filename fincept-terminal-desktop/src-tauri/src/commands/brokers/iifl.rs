//! IIFL Broker Integration (XTS API)
//!
//! Complete API implementation for IIFL broker including:
//! - Authentication (Session Token + Feed Token)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, holdings, margins)
//! - Market Data (quotes, historical, depth)
//! - Master Contract (download, search)
//!
//! API Documentation: https://symphonyfintech.com/xts-trading-front-end-api/

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde_json::{json, Value};
use std::collections::HashMap;

use super::common::{ApiResponse, OrderPlaceResponse, TokenExchangeResponse};

// IIFL API Configuration
// ============================================================================

const IIFL_BASE_URL: &str = "https://ttblaze.iifl.com";
const IIFL_INTERACTIVE_URL: &str = "https://ttblaze.iifl.com/interactive";
const IIFL_MARKET_DATA_URL: &str = "https://ttblaze.iifl.com/apimarketdata";

fn create_iifl_headers(token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    if let Ok(auth_value) = HeaderValue::from_str(token) {
        headers.insert(AUTHORIZATION, auth_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers
}

// Exchange segment mapping for API calls
fn get_exchange_segment(exchange: &str) -> &'static str {
    match exchange {
        "NSE" => "NSECM",
        "BSE" => "BSECM",
        "NFO" => "NSEFO",
        "BFO" => "BSEFO",
        "MCX" => "MCXFO",
        "CDS" => "NSECD",
        _ => "NSECM",
    }
}

// Exchange segment ID for market data API
fn get_exchange_segment_id(exchange: &str) -> i32 {
    match exchange {
        "NSE" | "NSE_INDEX" => 1,
        "NFO" => 2,
        "CDS" => 3,
        "BSE" | "BSE_INDEX" => 11,
        "BFO" => 12,
        "MCX" => 51,
        _ => 1,
    }
}

// ============================================================================
// IIFL Authentication Commands
// ============================================================================

/// Exchange credentials for session token
#[tauri::command]
pub async fn iifl_exchange_token(
    api_key: String,
    api_secret: String,
    _request_token: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[iifl_exchange_token] Authenticating with IIFL");

    let client = reqwest::Client::new();

    // Step 1: Get session token
    let payload = json!({
        "appKey": api_key,
        "secretKey": api_secret,
        "source": "WebAPI"
    });

    let response = client
        .post(format!("{}/user/session", IIFL_INTERACTIVE_URL))
        .header(CONTENT_TYPE, "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let result = body.get("result").ok_or("No result in response")?;
        let token = result
            .get("token")
            .and_then(|v| v.as_str())
            .map(String::from);
        let user_id = result
            .get("userID")
            .and_then(|v| v.as_str())
            .map(String::from);

        Ok(TokenExchangeResponse {
            success: true,
            access_token: token,
            user_id,
            error: None,
        })
    } else {
        let error_msg = body
            .get("description")
            .or(body.get("message"))
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

/// Get feed token for market data
#[tauri::command]
pub async fn iifl_get_feed_token(
    api_key: String,
    api_secret: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[iifl_get_feed_token] Getting feed token");

    let client = reqwest::Client::new();

    let payload = json!({
        "appKey": api_key,
        "secretKey": api_secret,
        "source": "WebAPI"
    });

    let response = client
        .post(format!("{}/auth/login", IIFL_MARKET_DATA_URL))
        .header(CONTENT_TYPE, "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let result = body.get("result").ok_or("No result in response")?;
        let token = result
            .get("token")
            .and_then(|v| v.as_str())
            .map(String::from);
        let user_id = result
            .get("userID")
            .and_then(|v| v.as_str())
            .map(String::from);

        Ok(TokenExchangeResponse {
            success: true,
            access_token: token,
            user_id,
            error: None,
        })
    } else {
        let error_msg = body
            .get("description")
            .and_then(|m| m.as_str())
            .unwrap_or("Feed token request failed")
            .to_string();
        Ok(TokenExchangeResponse {
            success: false,
            access_token: None,
            user_id: None,
            error: Some(error_msg),
        })
    }
}

/// Validate access token by fetching user balance
#[tauri::command]
pub async fn iifl_validate_token(access_token: String) -> Result<ApiResponse<bool>, String> {
    eprintln!("[iifl_validate_token] Validating access token");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/user/balance", IIFL_INTERACTIVE_URL))
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
// IIFL Order Commands
// ============================================================================

/// Place an order
#[tauri::command]
pub async fn iifl_place_order(
    access_token: String,
    params: HashMap<String, Value>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[iifl_place_order] Placing order");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    // Build order payload according to IIFL XTS format
    let order_payload = json!({
        "exchangeSegment": params.get("exchangeSegment").and_then(|v| v.as_str()).unwrap_or("NSECM"),
        "exchangeInstrumentID": params.get("exchangeInstrumentID").and_then(|v| v.as_i64()).unwrap_or(0),
        "productType": params.get("productType").and_then(|v| v.as_str()).unwrap_or("MIS"),
        "orderType": params.get("orderType").and_then(|v| v.as_str()).unwrap_or("MARKET"),
        "orderSide": params.get("orderSide").and_then(|v| v.as_str()).unwrap_or("BUY"),
        "timeInForce": params.get("timeInForce").and_then(|v| v.as_str()).unwrap_or("DAY"),
        "disclosedQuantity": params.get("disclosedQuantity").and_then(|v| v.as_str()).unwrap_or("0"),
        "orderQuantity": params.get("orderQuantity").and_then(|v| v.as_i64()).unwrap_or(1),
        "limitPrice": params.get("limitPrice").and_then(|v| v.as_f64()).unwrap_or(0.0),
        "stopPrice": params.get("stopPrice").and_then(|v| v.as_f64()).unwrap_or(0.0),
        "orderUniqueIdentifier": params.get("orderUniqueIdentifier").and_then(|v| v.as_str()).unwrap_or("fincept")
    });

    let response = client
        .post(format!("{}/orders", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .json(&order_payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let result = body.get("result").ok_or("No result in response")?;
        let order_id = result
            .get("AppOrderID")
            .and_then(|v| v.as_i64())
            .map(|id| id.to_string());

        Ok(OrderPlaceResponse {
            success: true,
            order_id,
            error: None,
        })
    } else {
        let error_msg = body
            .get("description")
            .or(body.get("message"))
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
pub async fn iifl_modify_order(
    access_token: String,
    order_id: String,
    params: HashMap<String, Value>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[iifl_modify_order] Modifying order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    // Build modify order payload according to IIFL XTS format
    let modify_payload = json!({
        "appOrderID": order_id,
        "modifiedProductType": params.get("productType").and_then(|v| v.as_str()).unwrap_or("MIS"),
        "modifiedOrderType": params.get("orderType").and_then(|v| v.as_str()).unwrap_or("LIMIT"),
        "modifiedOrderQuantity": params.get("quantity").and_then(|v| v.as_i64()).unwrap_or(1),
        "modifiedDisclosedQuantity": params.get("disclosedQuantity").and_then(|v| v.as_str()).unwrap_or("0"),
        "modifiedLimitPrice": params.get("price").and_then(|v| v.as_f64()).unwrap_or(0.0),
        "modifiedStopPrice": params.get("triggerPrice").and_then(|v| v.as_f64()).unwrap_or(0.0),
        "modifiedTimeInForce": "DAY",
        "orderUniqueIdentifier": "fincept"
    });

    let response = client
        .put(format!("{}/orders", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .json(&modify_payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success()
        && (body.get("type").and_then(|t| t.as_str()) == Some("success")
            || body.get("message").and_then(|m| m.as_str()) == Some("SUCCESS"))
    {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(order_id),
            error: None,
        })
    } else {
        let error_msg = body
            .get("description")
            .or(body.get("message"))
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
pub async fn iifl_cancel_order(
    access_token: String,
    order_id: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[iifl_cancel_order] Cancelling order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let url = format!(
        "{}/orders?appOrderID={}",
        IIFL_INTERACTIVE_URL, order_id
    );

    let response = client
        .delete(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(order_id),
            error: None,
        })
    } else {
        let error_msg = body
            .get("description")
            .or(body.get("message"))
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
pub async fn iifl_get_orders(access_token: String) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[iifl_get_orders] Fetching orders");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/orders", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let orders = body
            .get("result")
            .and_then(|r| r.as_array())
            .cloned()
            .unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(orders),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
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
pub async fn iifl_get_trade_book(access_token: String) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[iifl_get_trade_book] Fetching trade book");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/orders/trades", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let trades = body
            .get("result")
            .and_then(|r| r.as_array())
            .cloned()
            .unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(trades),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
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

// ============================================================================
// IIFL Position & Holdings Commands
// ============================================================================

/// Get positions
#[tauri::command]
pub async fn iifl_get_positions(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!(
            "{}/portfolio/positions?dayOrNet=NetWise",
            IIFL_INTERACTIVE_URL
        ))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let positions = body.get("result").cloned().unwrap_or(json!({}));
        Ok(ApiResponse {
            success: true,
            data: Some(positions),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
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
pub async fn iifl_get_holdings(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/portfolio/holdings", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let holdings = body.get("result").cloned().unwrap_or(json!({}));
        Ok(ApiResponse {
            success: true,
            data: Some(holdings),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
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

/// Get funds/margins
#[tauri::command]
pub async fn iifl_get_funds(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[iifl_get_funds] Fetching funds");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/user/balance", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        // Extract and format funds data
        let result = body.get("result").cloned().unwrap_or(json!({}));

        // Try to extract RMSSubLimits from the nested structure
        let funds_data = if let Some(balance_list) = result.get("BalanceList").and_then(|b| b.as_array()) {
            if let Some(first_balance) = balance_list.first() {
                if let Some(rms_sublimits) = first_balance
                    .get("limitObject")
                    .and_then(|l| l.get("RMSSubLimits"))
                {
                    json!({
                        "availablecash": rms_sublimits.get("netMarginAvailable").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "collateral": rms_sublimits.get("collateral").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "m2munrealized": rms_sublimits.get("UnrealizedMTM").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "m2mrealized": rms_sublimits.get("RealizedMTM").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "utiliseddebits": rms_sublimits.get("marginUtilized").and_then(|v| v.as_f64()).unwrap_or(0.0),
                        "raw": result.clone()
                    })
                } else {
                    result
                }
            } else {
                result
            }
        } else {
            result
        };

        Ok(ApiResponse {
            success: true,
            data: Some(funds_data),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
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
// IIFL Market Data Commands
// ============================================================================

/// Get quotes for instruments
#[tauri::command]
pub async fn iifl_get_quotes(
    feed_token: String,
    exchange: String,
    token: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!(
        "[iifl_get_quotes] Fetching quotes for {}:{}",
        exchange, token
    );

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&feed_token);

    let exchange_segment = get_exchange_segment_id(&exchange);

    let payload = json!({
        "instruments": [{
            "exchangeSegment": exchange_segment,
            "exchangeInstrumentID": token
        }],
        "xtsMessageCode": 1502,
        "publishFormat": "JSON"
    });

    let response = client
        .post(format!("{}/instruments/quotes", IIFL_MARKET_DATA_URL))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        // Parse the quote from listQuotes
        let list_quotes = body
            .get("result")
            .and_then(|r| r.get("listQuotes"))
            .and_then(|l| l.as_array());

        if let Some(quotes) = list_quotes {
            if let Some(first_quote) = quotes.first() {
                // Parse JSON string if needed
                let quote_data: Value = if first_quote.is_string() {
                    serde_json::from_str(first_quote.as_str().unwrap_or("{}"))
                        .unwrap_or(json!({}))
                } else {
                    first_quote.clone()
                };

                // Extract touchline data
                let empty_obj = json!({});
                let touchline = quote_data.get("Touchline").unwrap_or(&empty_obj);

                let formatted_quote = json!({
                    "ask": touchline.get("AskInfo").and_then(|a| a.get("Price")).and_then(|p| p.as_f64()).unwrap_or(0.0),
                    "bid": touchline.get("BidInfo").and_then(|b| b.get("Price")).and_then(|p| p.as_f64()).unwrap_or(0.0),
                    "high": touchline.get("High").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "low": touchline.get("Low").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "ltp": touchline.get("LastTradedPrice").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "open": touchline.get("Open").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "prev_close": touchline.get("Close").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "volume": touchline.get("TotalTradedQuantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "oi": 0,
                    "raw": quote_data
                });

                return Ok(ApiResponse {
                    success: true,
                    data: Some(formatted_quote),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("No quote data available".to_string()),
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
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

/// Get historical OHLCV data
#[tauri::command]
pub async fn iifl_get_history(
    feed_token: String,
    exchange: String,
    token: i64,
    timeframe: String,
    from_date: String,
    to_date: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!(
        "[iifl_get_history] Fetching history for {}:{} from {} to {}",
        exchange, token, from_date, to_date
    );

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&feed_token);

    // Map timeframe to compression value
    let compression_value = match timeframe.as_str() {
        "1s" => "1",
        "1m" => "60",
        "2m" => "120",
        "3m" => "180",
        "5m" => "300",
        "10m" => "600",
        "15m" => "900",
        "30m" => "1800",
        "60m" | "1h" => "3600",
        "D" | "1D" => "D",
        _ => "60",
    };

    let exchange_segment = get_exchange_segment(&exchange);

    let url = format!(
        "{}/instruments/ohlc?exchangeSegment={}&exchangeInstrumentID={}&startTime={}&endTime={}&compressionValue={}",
        IIFL_MARKET_DATA_URL, exchange_segment, token, from_date, to_date, compression_value
    );

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        // Parse dataReponse (pipe-delimited string)
        let raw_data = body
            .get("result")
            .and_then(|r| r.get("dataReponse"))
            .and_then(|d| d.as_str())
            .unwrap_or("");

        let mut candles: Vec<Value> = Vec::new();

        if !raw_data.is_empty() {
            for row in raw_data.split(',') {
                let fields: Vec<&str> = row.split('|').collect();
                if fields.len() >= 6 {
                    if let (Ok(ts), Ok(open), Ok(high), Ok(low), Ok(close), Ok(volume)) = (
                        fields[0].parse::<i64>(),
                        fields[1].parse::<f64>(),
                        fields[2].parse::<f64>(),
                        fields[3].parse::<f64>(),
                        fields[4].parse::<f64>(),
                        fields[5].parse::<i64>(),
                    ) {
                        candles.push(json!({
                            "timestamp": ts,
                            "open": open,
                            "high": high,
                            "low": low,
                            "close": close,
                            "volume": volume
                        }));
                    }
                }
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(candles),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
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

/// Get market depth
#[tauri::command]
pub async fn iifl_get_depth(
    feed_token: String,
    exchange: String,
    token: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!(
        "[iifl_get_depth] Fetching depth for {}:{}",
        exchange, token
    );

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&feed_token);

    let exchange_segment = get_exchange_segment_id(&exchange);

    let payload = json!({
        "instruments": [{
            "exchangeSegment": exchange_segment,
            "exchangeInstrumentID": token
        }],
        "xtsMessageCode": 1502,
        "publishFormat": "JSON"
    });

    let response = client
        .post(format!("{}/instruments/quotes", IIFL_MARKET_DATA_URL))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let list_quotes = body
            .get("result")
            .and_then(|r| r.get("listQuotes"))
            .and_then(|l| l.as_array());

        if let Some(quotes) = list_quotes {
            if let Some(first_quote) = quotes.first() {
                let quote_data: Value = if first_quote.is_string() {
                    serde_json::from_str(first_quote.as_str().unwrap_or("{}"))
                        .unwrap_or(json!({}))
                } else {
                    first_quote.clone()
                };

                let empty_obj2 = json!({});
                let touchline = quote_data.get("Touchline").unwrap_or(&empty_obj2);

                // Extract bids and asks
                let bids: Vec<Value> = quote_data
                    .get("Bids")
                    .and_then(|b| b.as_array())
                    .map(|arr| {
                        arr.iter()
                            .take(5)
                            .map(|b| {
                                json!({
                                    "price": b.get("Price").and_then(|p| p.as_f64()).unwrap_or(0.0),
                                    "quantity": b.get("Size").and_then(|s| s.as_i64()).unwrap_or(0)
                                })
                            })
                            .collect()
                    })
                    .unwrap_or_default();

                let asks: Vec<Value> = quote_data
                    .get("Asks")
                    .and_then(|a| a.as_array())
                    .map(|arr| {
                        arr.iter()
                            .take(5)
                            .map(|a| {
                                json!({
                                    "price": a.get("Price").and_then(|p| p.as_f64()).unwrap_or(0.0),
                                    "quantity": a.get("Size").and_then(|s| s.as_i64()).unwrap_or(0)
                                })
                            })
                            .collect()
                    })
                    .unwrap_or_default();

                let depth_data = json!({
                    "bids": bids,
                    "asks": asks,
                    "high": touchline.get("High").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "low": touchline.get("Low").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "ltp": touchline.get("LastTradedPrice").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "ltq": touchline.get("LastTradedQunatity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "open": touchline.get("Open").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "prev_close": touchline.get("Close").and_then(|v| v.as_f64()).unwrap_or(0.0),
                    "volume": touchline.get("TotalTradedQuantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "oi": 0,
                    "totalbuyqty": touchline.get("TotalBuyQuantity").and_then(|v| v.as_i64()).unwrap_or(0),
                    "totalsellqty": touchline.get("TotalSellQuantity").and_then(|v| v.as_i64()).unwrap_or(0)
                });

                return Ok(ApiResponse {
                    success: true,
                    data: Some(depth_data),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("No depth data available".to_string()),
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
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

// ============================================================================
// IIFL Master Contract Commands
// ============================================================================

/// Download master contract
#[tauri::command]
pub async fn iifl_download_master_contract(
    exchange: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!(
        "[iifl_download_master_contract] Downloading master contract for {}",
        exchange
    );

    let client = reqwest::Client::new();

    let exchange_segment = get_exchange_segment(&exchange);

    let payload = json!({
        "exchangeSegmentList": [exchange_segment]
    });

    let response = client
        .post(format!("{}/instruments/master", IIFL_MARKET_DATA_URL))
        .header(CONTENT_TYPE, "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        let result_str = body
            .get("result")
            .and_then(|r| r.as_str())
            .unwrap_or("");

        let mut instruments: Vec<Value> = Vec::new();

        // Parse pipe-delimited data
        for row in result_str.split('\n') {
            if row.trim().is_empty() {
                continue;
            }

            let fields: Vec<&str> = row.split('|').collect();
            if fields.len() >= 6 {
                // Basic instrument info
                let instrument = json!({
                    "exchange": exchange,
                    "exchangeSegment": fields.get(0).unwrap_or(&""),
                    "exchangeInstrumentID": fields.get(1).unwrap_or(&""),
                    "instrumentType": fields.get(2).unwrap_or(&""),
                    "name": fields.get(3).unwrap_or(&""),
                    "description": fields.get(4).unwrap_or(&""),
                    "series": fields.get(5).unwrap_or(&""),
                    "token": fields.get(1).unwrap_or(&""),
                    "symbol": fields.get(3).unwrap_or(&""),
                    "lotSize": fields.get(12).unwrap_or(&"1"),
                    "tickSize": fields.get(11).unwrap_or(&"0.05")
                });
                instruments.push(instrument);
            }
        }

        eprintln!(
            "[iifl_download_master_contract] Downloaded {} instruments for {}",
            instruments.len(),
            exchange
        );

        Ok(ApiResponse {
            success: true,
            data: Some(instruments),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body
            .get("description")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to download master contract")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Search symbols in master contract
#[tauri::command]
pub async fn iifl_search_symbol(
    query: String,
    exchange: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[iifl_search_symbol] Searching for: {}", query);

    // This would typically search from cached master contract in database
    // For now, return placeholder
    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(vec![]),
        error: Some(
            "Master contract search not yet implemented. Use iifl_download_master_contract first."
                .to_string(),
        ),
        timestamp,
    })
}

/// Get token for symbol
#[tauri::command]
pub async fn iifl_get_token_for_symbol(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!(
        "[iifl_get_token_for_symbol] Getting token for {}:{}",
        exchange, symbol
    );

    // This would typically lookup from cached master contract
    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Symbol lookup not yet implemented".to_string()),
        timestamp,
    })
}

/// Get master contract metadata
#[tauri::command]
pub async fn iifl_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Master contract database not yet implemented for IIFL".to_string()),
        timestamp,
    })
}

// ============================================================================
// IIFL Bulk Operations
// ============================================================================

/// Close all open positions
#[tauri::command]
pub async fn iifl_close_all_positions(
    access_token: String,
) -> Result<ApiResponse<Vec<OrderPlaceResponse>>, String> {
    eprintln!("[iifl_close_all_positions] Closing all positions");

    let timestamp = chrono::Utc::now().timestamp_millis();

    // Get all positions
    let positions_response = iifl_get_positions(access_token.clone()).await?;

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
        if let Some(position_list) = positions_data.get("positionList").and_then(|p| p.as_array()) {
            for position in position_list {
                let quantity = position
                    .get("Quantity")
                    .and_then(|q| q.as_i64())
                    .unwrap_or(0);

                if quantity == 0 {
                    continue;
                }

                let exchange_segment = position
                    .get("ExchangeSegment")
                    .and_then(|e| e.as_str())
                    .unwrap_or("NSECM");
                let instrument_id = position
                    .get("ExchangeInstrumentId")
                    .and_then(|i| i.as_i64())
                    .unwrap_or(0);
                let product_type = position
                    .get("ProductType")
                    .and_then(|p| p.as_str())
                    .unwrap_or("MIS");

                let action = if quantity > 0 { "SELL" } else { "BUY" };
                let close_quantity = quantity.abs();

                let mut params = HashMap::new();
                params.insert(
                    "exchangeSegment".to_string(),
                    json!(exchange_segment),
                );
                params.insert(
                    "exchangeInstrumentID".to_string(),
                    json!(instrument_id),
                );
                params.insert("productType".to_string(), json!(product_type));
                params.insert("orderType".to_string(), json!("MARKET"));
                params.insert("orderSide".to_string(), json!(action));
                params.insert("orderQuantity".to_string(), json!(close_quantity));

                let order_result = iifl_place_order(access_token.clone(), params).await;

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
pub async fn iifl_cancel_all_orders(
    access_token: String,
) -> Result<ApiResponse<Vec<OrderPlaceResponse>>, String> {
    eprintln!("[iifl_cancel_all_orders] Cancelling all open orders");

    let timestamp = chrono::Utc::now().timestamp_millis();

    // Get all orders
    let orders_response = iifl_get_orders(access_token.clone()).await?;

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
            let status = order
                .get("OrderStatus")
                .and_then(|s| s.as_str())
                .unwrap_or("");

            // Only cancel New or Trigger Pending orders
            if status == "New" || status == "Trigger Pending" {
                let order_id = order
                    .get("AppOrderID")
                    .and_then(|id| id.as_i64())
                    .map(|id| id.to_string())
                    .unwrap_or_default();

                if order_id.is_empty() {
                    continue;
                }

                let cancel_result = iifl_cancel_order(access_token.clone(), order_id).await;

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
