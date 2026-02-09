#![allow(dead_code)]
//! Dhan Broker Integration
//!
//! Complete API integration for Dhan broker.
//! Based on OpenAlgo dhan broker implementation.

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::collections::HashMap;
use crate::database::pool::get_db;

// ============================================================================
// CONSTANTS
// ============================================================================

const DHAN_API_BASE: &str = "https://api.dhan.co";
const DHAN_AUTH_BASE: &str = "https://auth.dhan.co";
const DHAN_MASTER_CONTRACT_URL: &str = "https://images.dhan.co/api-data/api-scrip-master.csv";

// ============================================================================
// TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct ApiResponse<T> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
    pub timestamp: i64,
}

#[derive(Debug, Serialize, Deserialize)]
struct DhanOrderParams {
    #[serde(rename = "dhanClientId")]
    dhan_client_id: String,
    #[serde(rename = "transactionType")]
    transaction_type: String,
    #[serde(rename = "exchangeSegment")]
    exchange_segment: String,
    #[serde(rename = "productType")]
    product_type: String,
    #[serde(rename = "orderType")]
    order_type: String,
    validity: String,
    #[serde(rename = "securityId")]
    security_id: String,
    quantity: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    price: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "triggerPrice")]
    trigger_price: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "disclosedQuantity")]
    disclosed_quantity: Option<i32>,
}

#[derive(Debug, Serialize, Deserialize)]
struct DhanModifyParams {
    #[serde(rename = "dhanClientId")]
    dhan_client_id: String,
    #[serde(rename = "orderId")]
    order_id: String,
    #[serde(rename = "orderType")]
    order_type: String,
    #[serde(rename = "legName")]
    leg_name: String,
    quantity: i32,
    validity: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    price: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "triggerPrice")]
    trigger_price: Option<f64>,
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

fn get_timestamp() -> i64 {
    chrono::Utc::now().timestamp()
}

async fn dhan_request(
    endpoint: &str,
    method: &str,
    access_token: &str,
    client_id: &str,
    body: Option<Value>,
) -> Result<Value, String> {
    let client = reqwest::Client::new();
    let url = format!("{}{}", DHAN_API_BASE, endpoint);

    let mut request = match method {
        "GET" => client.get(&url),
        "POST" => client.post(&url),
        "PUT" => client.put(&url),
        "DELETE" => client.delete(&url),
        _ => return Err(format!("Unsupported HTTP method: {}", method)),
    };

    request = request
        .header("access-token", access_token)
        .header("client-id", client_id)
        .header("Content-Type", "application/json")
        .header("Accept", "application/json");

    if let Some(body_value) = body {
        request = request.json(&body_value);
    }

    let response = request.send().await.map_err(|e| e.to_string())?;
    let status = response.status();
    let text = response.text().await.map_err(|e| e.to_string())?;

    if !status.is_success() {
        return Err(format!("HTTP {}: {}", status, text));
    }

    serde_json::from_str(&text).map_err(|e| format!("JSON parse error: {} - {}", e, text))
}

fn map_exchange_to_dhan(exchange: &str) -> &str {
    match exchange {
        "NSE" => "NSE_EQ",
        "BSE" => "BSE_EQ",
        "NFO" => "NSE_FNO",
        "BFO" => "BSE_FNO",
        "MCX" => "MCX_COMM",
        "CDS" => "NSE_CURRENCY",
        "BCD" => "BSE_CURRENCY",
        "NSE_INDEX" => "IDX_I",
        "BSE_INDEX" => "IDX_I",
        _ => exchange,
    }
}

fn map_exchange_from_dhan(exchange: &str) -> &str {
    match exchange {
        "NSE_EQ" => "NSE",
        "BSE_EQ" => "BSE",
        "NSE_FNO" => "NFO",
        "BSE_FNO" => "BFO",
        "MCX_COMM" => "MCX",
        "NSE_CURRENCY" => "CDS",
        "BSE_CURRENCY" => "BCD",
        "IDX_I" => "NSE_INDEX",
        _ => exchange,
    }
}

fn map_product_to_dhan(product: &str) -> &str {
    match product {
        "CNC" | "CASH" => "CNC",
        "MIS" | "INTRADAY" => "INTRADAY",
        "NRML" | "MARGIN" => "MARGIN",
        _ => "INTRADAY",
    }
}

fn map_order_type_to_dhan(order_type: &str) -> &str {
    match order_type {
        "MARKET" => "MARKET",
        "LIMIT" => "LIMIT",
        "SL" | "STOP" | "STOP_LOSS" => "STOP_LOSS",
        "SL-M" | "STOP_LOSS_MARKET" => "STOP_LOSS_MARKET",
        _ => "MARKET",
    }
}

// ============================================================================
// AUTHENTICATION COMMANDS
// ============================================================================

/// Generate consent URL for Dhan OAuth login
#[tauri::command]
pub async fn dhan_generate_consent(
    api_key: String,
    api_secret: String,
    client_id: String,
) -> Result<ApiResponse<Value>, String> {
    let client = reqwest::Client::new();
    let url = format!(
        "{}/app/generate-consent?client_id={}",
        DHAN_AUTH_BASE, client_id
    );

    let response = client
        .post(&url)
        .header("app_id", &api_key)
        .header("app_secret", &api_secret)
        .send()
        .await
        .map_err(|e| e.to_string())?;

    let data: Value = response.json().await.map_err(|e| e.to_string())?;

    if data.get("status").and_then(|s| s.as_str()) == Some("success") {
        let consent_app_id = data.get("consentAppId").cloned();
        let login_url = consent_app_id.as_ref().and_then(|id| id.as_str()).map(|id| {
            format!(
                "{}/login/consentApp-login?consentAppId={}",
                DHAN_AUTH_BASE, id
            )
        });

        Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "consent_app_id": consent_app_id,
                "login_url": login_url
            })),
            error: None,
            timestamp: get_timestamp(),
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to generate consent: {:?}", data)),
            timestamp: get_timestamp(),
        })
    }
}

/// Exchange token ID for access token
#[tauri::command]
pub async fn dhan_exchange_token(
    api_key: String,
    api_secret: String,
    token_id: String,
) -> Result<ApiResponse<Value>, String> {
    let client = reqwest::Client::new();
    let url = format!(
        "{}/app/consumeApp-consent?tokenId={}",
        DHAN_AUTH_BASE, token_id
    );

    let response = client
        .post(&url)
        .header("app_id", &api_key)
        .header("app_secret", &api_secret)
        .header("Content-Type", "application/json")
        .send()
        .await
        .map_err(|e| e.to_string())?;

    let data: Value = response.json().await.map_err(|e| e.to_string())?;

    if let Some(access_token) = data.get("accessToken").and_then(|t| t.as_str()) {
        Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "access_token": access_token,
                "client_id": data.get("dhanClientId"),
                "client_name": data.get("dhanClientName"),
                "expiry": data.get("expiryTime")
            })),
            error: None,
            timestamp: get_timestamp(),
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to get access token".to_string()),
            timestamp: get_timestamp(),
        })
    }
}

/// Validate access token by fetching funds
#[tauri::command]
pub async fn dhan_validate_token(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<bool>, String> {
    match dhan_request("/v2/fundlimit", "GET", &access_token, &client_id, None).await {
        Ok(data) => {
            let is_error = data.get("errorType").is_some()
                || data.get("status").and_then(|s| s.as_str()) == Some("error");

            Ok(ApiResponse {
                success: !is_error,
                data: Some(!is_error),
                error: if is_error {
                    Some("Invalid or expired token".to_string())
                } else {
                    None
                },
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

// ============================================================================
// ORDER COMMANDS
// ============================================================================

/// Place a new order
#[tauri::command]
pub async fn dhan_place_order(
    access_token: String,
    client_id: String,
    params: Value,
) -> Result<ApiResponse<Value>, String> {
    let order_params = json!({
        "dhanClientId": client_id,
        "transactionType": params.get("side").and_then(|s| s.as_str()).unwrap_or("BUY"),
        "exchangeSegment": map_exchange_to_dhan(
            params.get("exchange").and_then(|e| e.as_str()).unwrap_or("NSE")
        ),
        "productType": map_product_to_dhan(
            params.get("product_type").and_then(|p| p.as_str()).unwrap_or("INTRADAY")
        ),
        "orderType": map_order_type_to_dhan(
            params.get("order_type").and_then(|o| o.as_str()).unwrap_or("MARKET")
        ),
        "validity": "DAY",
        "securityId": params.get("security_id").and_then(|s| s.as_str()).unwrap_or(""),
        "quantity": params.get("quantity").and_then(|q| q.as_i64()).unwrap_or(0),
        "price": params.get("price").and_then(|p| p.as_f64()),
        "triggerPrice": params.get("trigger_price").and_then(|t| t.as_f64()),
    });

    match dhan_request("/v2/orders", "POST", &access_token, &client_id, Some(order_params)).await {
        Ok(data) => {
            let order_id = data.get("orderId").cloned();
            Ok(ApiResponse {
                success: order_id.is_some(),
                data: Some(json!({ "order_id": order_id })),
                error: if order_id.is_none() {
                    Some(format!("Order placement failed: {:?}", data))
                } else {
                    None
                },
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Modify an existing order
#[tauri::command]
pub async fn dhan_modify_order(
    access_token: String,
    client_id: String,
    order_id: String,
    params: Value,
) -> Result<ApiResponse<Value>, String> {
    let modify_params = json!({
        "dhanClientId": client_id,
        "orderId": order_id,
        "orderType": map_order_type_to_dhan(
            params.get("order_type").and_then(|o| o.as_str()).unwrap_or("MARKET")
        ),
        "legName": "ENTRY_LEG",
        "quantity": params.get("quantity").and_then(|q| q.as_i64()).unwrap_or(0),
        "validity": "DAY",
        "price": params.get("price").and_then(|p| p.as_f64()),
        "triggerPrice": params.get("trigger_price").and_then(|t| t.as_f64()),
    });

    let endpoint = format!("/v2/orders/{}", order_id);

    match dhan_request(&endpoint, "PUT", &access_token, &client_id, Some(modify_params)).await {
        Ok(data) => {
            let result_order_id = data.get("orderId").cloned();
            Ok(ApiResponse {
                success: result_order_id.is_some(),
                data: Some(json!({ "order_id": result_order_id })),
                error: if result_order_id.is_none() {
                    Some(format!("Order modification failed: {:?}", data))
                } else {
                    None
                },
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Cancel an order
#[tauri::command]
pub async fn dhan_cancel_order(
    access_token: String,
    client_id: String,
    order_id: String,
) -> Result<ApiResponse<Value>, String> {
    let endpoint = format!("/v2/orders/{}", order_id);

    match dhan_request(&endpoint, "DELETE", &access_token, &client_id, None).await {
        Ok(_) => Ok(ApiResponse {
            success: true,
            data: Some(json!({ "order_id": order_id })),
            error: None,
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get order book
#[tauri::command]
pub async fn dhan_get_orders(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    match dhan_request("/v2/orders", "GET", &access_token, &client_id, None).await {
        Ok(data) => {
            let orders = if let Some(arr) = data.as_array() {
                arr.clone()
            } else {
                vec![]
            };

            Ok(ApiResponse {
                success: true,
                data: Some(orders),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: Some(vec![]),
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get trade book
#[tauri::command]
pub async fn dhan_get_trade_book(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    match dhan_request("/v2/trades", "GET", &access_token, &client_id, None).await {
        Ok(data) => {
            let trades = if let Some(arr) = data.as_array() {
                arr.clone()
            } else {
                vec![]
            };

            Ok(ApiResponse {
                success: true,
                data: Some(trades),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: Some(vec![]),
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

// ============================================================================
// PORTFOLIO COMMANDS
// ============================================================================

/// Get positions
#[tauri::command]
pub async fn dhan_get_positions(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    match dhan_request("/v2/positions", "GET", &access_token, &client_id, None).await {
        Ok(data) => {
            let positions = if let Some(arr) = data.as_array() {
                arr.clone()
            } else {
                vec![]
            };

            Ok(ApiResponse {
                success: true,
                data: Some(positions),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: Some(vec![]),
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get holdings
#[tauri::command]
pub async fn dhan_get_holdings(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    match dhan_request("/v2/holdings", "GET", &access_token, &client_id, None).await {
        Ok(data) => {
            let holdings = if let Some(arr) = data.as_array() {
                arr.clone()
            } else {
                vec![]
            };

            Ok(ApiResponse {
                success: true,
                data: Some(holdings),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: Some(vec![]),
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get funds/margin
#[tauri::command]
pub async fn dhan_get_funds(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<Value>, String> {
    match dhan_request("/v2/fundlimit", "GET", &access_token, &client_id, None).await {
        Ok(data) => Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

// ============================================================================
// MARKET DATA COMMANDS
// ============================================================================

/// Get quotes for instruments
#[tauri::command]
pub async fn dhan_get_quotes(
    access_token: String,
    client_id: String,
    instrument_keys: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    // Group instruments by exchange segment
    let mut exchange_groups: HashMap<String, Vec<i64>> = HashMap::new();

    for key in &instrument_keys {
        let parts: Vec<&str> = key.split('|').collect();
        if parts.len() == 2 {
            let exchange = parts[0].to_string();
            if let Ok(security_id) = parts[1].parse::<i64>() {
                exchange_groups
                    .entry(exchange)
                    .or_insert_with(Vec::new)
                    .push(security_id);
            }
        }
    }

    let payload = serde_json::to_value(&exchange_groups).map_err(|e| e.to_string())?;

    match dhan_request(
        "/v2/marketfeed/quote",
        "POST",
        &access_token,
        &client_id,
        Some(payload),
    )
    .await
    {
        Ok(data) => Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get historical OHLCV data
#[tauri::command]
pub async fn dhan_get_history(
    access_token: String,
    client_id: String,
    security_id: String,
    exchange_segment: String,
    instrument: String,
    interval: String,
    from_date: String,
    to_date: String,
) -> Result<ApiResponse<Value>, String> {
    let endpoint = if interval == "D" || interval == "1D" {
        "/v2/charts/historical"
    } else {
        "/v2/charts/intraday"
    };

    let mut payload = json!({
        "securityId": security_id,
        "exchangeSegment": exchange_segment,
        "instrument": instrument,
        "fromDate": from_date,
        "toDate": to_date,
        "oi": true
    });

    // Add interval for intraday
    if endpoint == "/v2/charts/intraday" {
        payload["interval"] = json!(interval.replace("m", "").replace("h", "60"));
    }

    // Add expiryCode for equity
    if instrument == "EQUITY" {
        payload["expiryCode"] = json!(0);
    }

    match dhan_request(endpoint, "POST", &access_token, &client_id, Some(payload)).await {
        Ok(data) => Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get market depth
#[tauri::command]
pub async fn dhan_get_depth(
    access_token: String,
    client_id: String,
    security_id: String,
    exchange_segment: String,
) -> Result<ApiResponse<Value>, String> {
    let mut payload: HashMap<String, Vec<i64>> = HashMap::new();

    if let Ok(sec_id) = security_id.parse::<i64>() {
        payload.insert(exchange_segment.clone(), vec![sec_id]);
    }

    let payload_value = serde_json::to_value(&payload).map_err(|e| e.to_string())?;

    match dhan_request(
        "/v2/marketfeed/quote",
        "POST",
        &access_token,
        &client_id,
        Some(payload_value),
    )
    .await
    {
        Ok(data) => {
            // Extract depth from response
            let depth = data
                .get("data")
                .and_then(|d| d.get(&exchange_segment))
                .and_then(|e| e.get(&security_id))
                .cloned()
                .unwrap_or(json!({}));

            Ok(ApiResponse {
                success: true,
                data: Some(depth),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

// ============================================================================
// MASTER CONTRACT COMMANDS
// ============================================================================

/// Initialize Dhan symbols table
fn init_dhan_symbols_table() -> Result<(), String> {
    let db = get_db().map_err(|e| e.to_string())?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS dhan_symbols (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            name TEXT,
            exchange TEXT NOT NULL,
            br_exchange TEXT,
            security_id TEXT NOT NULL,
            instrument_type TEXT,
            lot_size INTEGER DEFAULT 1,
            tick_size REAL DEFAULT 0.05,
            expiry TEXT,
            strike REAL
        )",
        [],
    )
    .map_err(|e| e.to_string())?;

    // Create metadata table
    db.execute(
        "CREATE TABLE IF NOT EXISTS dhan_metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )",
        [],
    )
    .map_err(|e| e.to_string())?;

    // Create indexes for faster search
    let _ = db.execute(
        "CREATE INDEX IF NOT EXISTS idx_dhan_symbol ON dhan_symbols(symbol)",
        [],
    );
    let _ = db.execute(
        "CREATE INDEX IF NOT EXISTS idx_dhan_exchange ON dhan_symbols(exchange)",
        [],
    );

    Ok(())
}

/// Download and store master contract
#[tauri::command]
pub async fn dhan_download_master_contract() -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();

    // Use the new unified broker downloads system
    let result = crate::commands::broker_downloads::dhan::dhan_download_symbols().await;

    if result.success {
        Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "total_symbols": result.total_symbols,
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

// Legacy download code removed - now using broker_downloads::dhan module

/// Search symbols in master contract
#[tauri::command]
pub async fn dhan_search_symbol(
    keyword: String,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    use crate::database::symbol_master;

    let search_limit = limit.unwrap_or(20);

    match symbol_master::search_symbols("dhan", &keyword, exchange.as_deref(), None, search_limit) {
        Ok(results) => {
            let symbols: Vec<Value> = results.iter().map(|r| {
                json!({
                    "symbol": r.symbol,
                    "name": r.name,
                    "exchange": r.exchange,
                    "br_symbol": r.br_symbol,
                    "token": r.token,
                    "lot_size": r.lot_size,
                    "tick_size": r.tick_size,
                    "instrument_type": r.instrument_type,
                    "strike": r.strike,
                    "expiry": r.expiry,
                })
            }).collect();

            Ok(ApiResponse {
                success: true,
                data: Some(symbols),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: Some(vec![]),
                error: Some(format!("Search failed: {}", e)),
                timestamp: get_timestamp(),
            })
        }
    }
}

/// Get security ID for a symbol
#[tauri::command]
pub async fn dhan_get_security_id(
    symbol: String,
    exchange: String,
) -> Result<ApiResponse<String>, String> {
    use crate::database::symbol_master;

    match symbol_master::get_token_by_symbol("dhan", &symbol, &exchange) {
        Ok(Some(token)) => Ok(ApiResponse {
            success: true,
            data: Some(token),
            error: None,
            timestamp: get_timestamp(),
        }),
        Ok(None) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Symbol {} not found on {}", symbol, exchange)),
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Lookup failed: {}", e)),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get master contract metadata
#[tauri::command]
pub async fn dhan_get_master_contract_metadata() -> Result<ApiResponse<Value>, String> {
    use crate::database::symbol_master;

    match symbol_master::get_status("dhan") {
        Ok(info) => {
            let now = chrono::Utc::now().timestamp();
            Ok(ApiResponse {
                success: true,
                data: Some(json!({
                    "last_updated": info.last_updated,
                    "symbol_count": info.total_symbols,
                    "age_seconds": now - info.last_updated,
                    "status": info.status,
                    "is_ready": info.is_ready,
                })),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to get metadata: {}", e)),
            timestamp: get_timestamp(),
        }),
    }
}
