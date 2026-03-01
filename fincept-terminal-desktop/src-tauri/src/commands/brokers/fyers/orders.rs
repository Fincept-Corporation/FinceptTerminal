// Fyers Order Management Commands

use super::{FYERS_API_BASE, create_fyers_headers};
use super::super::common::{ApiResponse, OrderPlaceResponse};
use serde_json::{json, Value};

/// Place a new order
#[tauri::command]
pub async fn fyers_place_order(
    api_key: String,
    access_token: String,
    symbol: String,
    qty: i32,
    r#type: i32,
    side: i32,
    product_type: String,
    limit_price: Option<f64>,
    stop_price: Option<f64>,
    disclosed_qty: Option<i32>,
    validity: Option<String>,
    offline_order: Option<bool>,
    stop_loss: Option<f64>,
    take_profit: Option<f64>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[fyers_place_order] Placing order for {}", symbol);

    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let mut payload = json!({
        "symbol": symbol,
        "qty": qty,
        "type": r#type,
        "side": side,
        "productType": product_type,
        "validity": validity.unwrap_or_else(|| "DAY".to_string()),
        "offlineOrder": offline_order.unwrap_or(false)
    });

    if let Some(price) = limit_price { payload["limitPrice"] = json!(price); }
    if let Some(price) = stop_price { payload["stopPrice"] = json!(price); }
    if let Some(qty) = disclosed_qty { payload["disclosedQty"] = json!(qty); }
    if let Some(sl) = stop_loss { payload["stopLoss"] = json!(sl); }
    if let Some(tp) = take_profit { payload["takeProfit"] = json!(tp); }

    let response = client
        .post(format!("{}/api/v3/orders", FYERS_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: body.get("id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Order placement failed")
            .to_string();
        Ok(OrderPlaceResponse { success: false, order_id: None, error: Some(error_msg) })
    }
}

/// Modify an existing order
#[tauri::command]
pub async fn fyers_modify_order(
    api_key: String,
    access_token: String,
    order_id: String,
    qty: Option<i32>,
    r#type: Option<i32>,
    limit_price: Option<f64>,
    stop_price: Option<f64>,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    eprintln!("[fyers_modify_order] Modifying order {}", order_id);

    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let mut payload = json!({ "id": order_id });
    if let Some(q) = qty { payload["qty"] = json!(q); }
    if let Some(t) = r#type { payload["type"] = json!(t); }
    if let Some(price) = limit_price { payload["limitPrice"] = json!(price); }
    if let Some(price) = stop_price { payload["stopPrice"] = json!(price); }

    let response = client
        .put(format!("{}/api/v3/orders", FYERS_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Order modification failed").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn fyers_cancel_order(
    api_key: String,
    access_token: String,
    order_id: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    eprintln!("[fyers_cancel_order] Cancelling order {}", order_id);

    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .delete(format!("{}/api/v3/orders", FYERS_API_BASE))
        .headers(headers)
        .json(&json!({"id": order_id}))
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Order cancellation failed").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get order book
#[tauri::command]
pub async fn fyers_get_orders(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/orders", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse { success: true, data: body.get("orderBook").cloned(), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to fetch orders").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get trade book
#[tauri::command]
pub async fn fyers_get_trade_book(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let headers = create_fyers_headers(&api_key, &access_token);
    let client = reqwest::Client::new();

    let response = client
        .get(format!("{}/api/v3/tradebook", FYERS_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(ApiResponse { success: true, data: body.get("tradeBook").cloned(), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str()).unwrap_or("Failed to fetch trade book").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}
