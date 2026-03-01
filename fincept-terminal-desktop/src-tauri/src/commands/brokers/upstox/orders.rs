// commands/brokers/upstox/orders.rs - Order management commands

use serde_json::{json, Value};
use super::super::common::{ApiResponse, OrderPlaceResponse};
use super::{UPSTOX_API_BASE_V2, create_upstox_headers};

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
