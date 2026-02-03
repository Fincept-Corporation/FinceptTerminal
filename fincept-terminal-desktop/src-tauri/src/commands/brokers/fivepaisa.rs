//! 5Paisa Broker Commands
//!
//! REST API integration for 5Paisa Trading API
//! Based on OpenAlgo Python implementation

use reqwest::Client;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::collections::HashMap;

// ============================================================================
// CONSTANTS
// ============================================================================

const FIVEPAISA_BASE_URL: &str = "https://Openapi.5paisa.com";

// ============================================================================
// TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct FivePaisaResponse {
    pub success: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub data: Option<Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct FivePaisaApiResponse {
    pub head: FivePaisaHead,
    pub body: Value,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct FivePaisaHead {
    #[serde(rename = "statusDescription")]
    pub status_description: Option<String>,
    pub status: Option<String>,
    #[serde(rename = "Key")]
    pub key: Option<String>,
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

fn create_client() -> Result<Client, String> {
    Client::builder()
        .timeout(std::time::Duration::from_secs(30))
        .build()
        .map_err(|e| format!("Failed to create HTTP client: {}", e))
}

fn get_auth_headers(access_token: &str) -> HashMap<String, String> {
    let mut headers = HashMap::new();
    headers.insert("Content-Type".to_string(), "application/json".to_string());
    headers.insert("Authorization".to_string(), format!("bearer {}", access_token));
    headers
}

/// Map exchange to 5Paisa format
fn map_exchange(exchange: &str) -> &str {
    match exchange {
        "NSE" | "NSE_INDEX" => "N",
        "BSE" | "BSE_INDEX" => "B",
        "NFO" => "N",
        "BFO" => "B",
        "CDS" => "N",
        "BCD" => "B",
        "MCX" => "M",
        _ => "N",
    }
}

/// Map exchange type to 5Paisa format
fn map_exchange_type(exchange: &str) -> &str {
    match exchange {
        "NSE" | "BSE" | "NSE_INDEX" | "BSE_INDEX" => "C",  // Cash
        "NFO" | "BFO" | "MCX" => "D",  // Derivative
        "CDS" | "BCD" => "U",  // Currency
        _ => "C",
    }
}

/// Map order side to 5Paisa format
fn map_order_side(side: &str) -> &str {
    match side.to_uppercase().as_str() {
        "BUY" => "B",
        "SELL" => "S",
        _ => "B",
    }
}

/// Map product type to 5Paisa format (IsIntraday boolean)
fn is_intraday(product: &str) -> bool {
    match product.to_uppercase().as_str() {
        "MIS" | "INTRADAY" => true,
        "CNC" | "NRML" | "CASH" | "MARGIN" => false,
        _ => false,
    }
}

// ============================================================================
// AUTHENTICATION COMMANDS
// ============================================================================

/// Step 1: TOTP Login - Get request token
#[tauri::command]
pub async fn fivepaisa_totp_login(
    api_key: String,
    email: String,
    pin: String,
    totp: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "Key": api_key },
        "body": {
            "Email_ID": email,
            "TOTP": totp,
            "PIN": pin
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/TOTPLogin", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("TOTP login request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse TOTP response: {}", e))?;

    if let Some(request_token) = data.body.get("RequestToken").and_then(|v| v.as_str()) {
        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({ "request_token": request_token })),
            error: None,
        })
    } else {
        let error_msg = data.body.get("Message")
            .and_then(|v| v.as_str())
            .unwrap_or("Failed to obtain request token");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Step 2: Exchange request token for access token
#[tauri::command]
pub async fn fivepaisa_get_access_token(
    api_key: String,
    api_secret: String,
    user_id: String,
    request_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "Key": api_key },
        "body": {
            "RequestToken": request_token,
            "EncryKey": api_secret,
            "UserId": user_id
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/GetAccessToken", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Access token request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse access token response: {}", e))?;

    if let Some(access_token) = data.body.get("AccessToken").and_then(|v| v.as_str()) {
        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({
                "access_token": access_token,
                "user_id": user_id
            })),
            error: None,
        })
    } else {
        let error_msg = data.body.get("Message")
            .and_then(|v| v.as_str())
            .unwrap_or("Failed to obtain access token");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Validate access token by fetching margin
#[tauri::command]
pub async fn fivepaisa_validate_token(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V4/Margin", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Token validation failed: {}", e))?;

    let status = response.status();
    if status.is_success() {
        let data: FivePaisaApiResponse = response
            .json()
            .await
            .map_err(|e| format!("Failed to parse validation response: {}", e))?;

        let is_valid = data.head.status_description.as_deref() == Some("Success") ||
                       data.head.status.as_deref() == Some("0");

        Ok(FivePaisaResponse {
            success: is_valid,
            data: None,
            error: if is_valid { None } else { Some("Token validation failed".to_string()) },
        })
    } else {
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(format!("HTTP error: {}", status)),
        })
    }
}

// ============================================================================
// ORDER COMMANDS
// ============================================================================

/// Place a new order
#[tauri::command]
pub async fn fivepaisa_place_order(
    api_key: String,
    client_id: String,
    access_token: String,
    exchange: String,
    _symbol: String,
    scrip_code: i64,
    side: String,
    quantity: i32,
    price: f64,
    trigger_price: f64,
    product: String,
    disclosed_quantity: Option<i32>,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": {
            "ClientCode": client_id,
            "OrderType": map_order_side(&side),
            "Exchange": map_exchange(&exchange),
            "ExchangeType": map_exchange_type(&exchange),
            "ScripCode": scrip_code,
            "Price": price,
            "Qty": quantity,
            "StopLossPrice": trigger_price,
            "DisQty": disclosed_quantity.unwrap_or(0),
            "IsIntraday": is_intraday(&product),
            "AHPlaced": "N",
            "RemoteOrderID": "FinceptTerminal"
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/PlaceOrderRequest", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Place order request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse order response: {}", e))?;

    if data.head.status_description.as_deref() == Some("Success") {
        let order_id = data.body.get("BrokerOrderID")
            .and_then(|v| v.as_i64())
            .map(|v| v.to_string())
            .or_else(|| data.body.get("BrokerOrderID").and_then(|v| v.as_str()).map(|s| s.to_string()));

        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({ "order_id": order_id })),
            error: None,
        })
    } else {
        let error_msg = data.body.get("Message")
            .and_then(|v| v.as_str())
            .or_else(|| data.head.status_description.as_deref())
            .unwrap_or("Order placement failed");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Modify an existing order
#[tauri::command]
pub async fn fivepaisa_modify_order(
    api_key: String,
    access_token: String,
    exchange_order_id: String,
    quantity: i32,
    price: f64,
    trigger_price: f64,
    disclosed_quantity: Option<i32>,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": {
            "ExchOrderID": exchange_order_id,
            "Price": price,
            "Qty": quantity,
            "StopLossPrice": trigger_price,
            "DisQty": disclosed_quantity.unwrap_or(0)
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/ModifyOrderRequest", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Modify order request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse modify response: {}", e))?;

    let is_success = data.head.status.as_deref() == Some("0") ||
                     data.head.status_description.as_deref() == Some("Success");

    if is_success {
        let order_id = data.body.get("BrokerOrderID")
            .and_then(|v| v.as_str())
            .unwrap_or(&exchange_order_id);
        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({ "order_id": order_id })),
            error: None,
        })
    } else {
        let error_msg = data.head.status_description.as_deref()
            .unwrap_or("Order modification failed");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn fivepaisa_cancel_order(
    api_key: String,
    access_token: String,
    exchange_order_id: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ExchOrderID": exchange_order_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/CancelOrderRequest", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Cancel order request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse cancel response: {}", e))?;

    if data.head.status_description.as_deref() == Some("Success") {
        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({ "order_id": exchange_order_id })),
            error: None,
        })
    } else {
        let error_msg = data.body.get("Message")
            .and_then(|v| v.as_str())
            .unwrap_or("Order cancellation failed");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Get order book
#[tauri::command]
pub async fn fivepaisa_get_orders(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V3/OrderBook", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Order book request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse order book: {}", e))?;

    let orders = data.body.get("OrderBookDetail")
        .cloned()
        .unwrap_or(json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(orders),
        error: None,
    })
}

/// Get trade book
#[tauri::command]
pub async fn fivepaisa_get_trades(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/TradeBook", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Trade book request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse trade book: {}", e))?;

    let trades = data.body.get("TradeBookDetail")
        .cloned()
        .unwrap_or(json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(trades),
        error: None,
    })
}

// ============================================================================
// PORTFOLIO COMMANDS
// ============================================================================

/// Get positions
#[tauri::command]
pub async fn fivepaisa_get_positions(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V2/NetPositionNetWise", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .timeout(std::time::Duration::from_secs(60))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Positions request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse positions: {}", e))?;

    let positions = data.body.get("NetPositionDetail")
        .cloned()
        .unwrap_or(json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(positions),
        error: None,
    })
}

/// Get holdings
#[tauri::command]
pub async fn fivepaisa_get_holdings(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V3/Holding", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Holdings request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse holdings: {}", e))?;

    let holdings = data.body.get("Data")
        .cloned()
        .unwrap_or(json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(holdings),
        error: None,
    })
}

/// Get margin/funds
#[tauri::command]
pub async fn fivepaisa_get_margins(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V4/Margin", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Margin request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse margins: {}", e))?;

    // Extract equity margin from array
    let equity_margin = data.body.get("EquityMargin")
        .and_then(|v| v.as_array())
        .and_then(|arr| arr.first())
        .cloned()
        .unwrap_or(json!({}));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(equity_margin),
        error: None,
    })
}

// ============================================================================
// MARKET DATA COMMANDS
// ============================================================================

/// Get market depth/quote
#[tauri::command]
pub async fn fivepaisa_get_quote(
    api_key: String,
    client_id: String,
    access_token: String,
    exchange: String,
    scrip_code: i64,
    scrip_data: Option<String>,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": {
            "ClientCode": client_id,
            "Exchange": map_exchange(&exchange),
            "ExchangeType": map_exchange_type(&exchange),
            "ScripCode": scrip_code,
            "ScripData": scrip_data.unwrap_or_default()
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V2/MarketDepth", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Quote request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse quote: {}", e))?;

    if data.head.status_description.as_deref() == Some("Success") {
        Ok(FivePaisaResponse {
            success: true,
            data: Some(data.body),
            error: None,
        })
    } else {
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch quote".to_string()),
        })
    }
}

/// Get historical data (OHLCV)
#[tauri::command]
pub async fn fivepaisa_get_historical(
    api_key: String,
    client_id: String,
    access_token: String,
    exchange: String,
    scrip_code: i64,
    resolution: String,
    from_timestamp: i64,
    to_timestamp: i64,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    // Convert timestamps to date strings
    let from_date = chrono::DateTime::from_timestamp(from_timestamp, 0)
        .map(|dt| dt.format("%Y-%m-%d").to_string())
        .unwrap_or_default();
    let to_date = chrono::DateTime::from_timestamp(to_timestamp, 0)
        .map(|dt| dt.format("%Y-%m-%d").to_string())
        .unwrap_or_default();

    let payload = json!({
        "head": { "key": api_key },
        "body": {
            "ClientCode": client_id,
            "Exch": map_exchange(&exchange),
            "ExchType": map_exchange_type(&exchange),
            "ScripCode": scrip_code,
            "Interval": resolution,
            "FromDate": from_date,
            "ToDate": to_date
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/HistoricalData", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Historical data request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse historical data: {}", e))?;

    let candles = data.body.get("Data")
        .cloned()
        .unwrap_or(json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(candles),
        error: None,
    })
}
