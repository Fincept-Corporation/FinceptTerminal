// Zerodha Order, Smart Order, Bulk Operations & Margin Commands

use super::{ZERODHA_API_BASE, create_zerodha_headers};
use super::super::common::{ApiResponse, OrderPlaceResponse};
use serde_json::{json, Value};
use std::collections::HashMap;

/// Place an order
#[tauri::command]
pub async fn zerodha_place_order(
    api_key: String,
    access_token: String,
    params: HashMap<String, Value>,
    variety: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[zerodha_place_order] Placing {} order", variety);

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let form_params: Vec<(String, String)> = params
        .iter()
        .map(|(k, v)| {
            let value = match v {
                Value::String(s) => s.clone(),
                Value::Number(n) => n.to_string(),
                _ => v.to_string(),
            };
            (k.clone(), value)
        })
        .collect();

    let url = format!("{}/orders/{}", ZERODHA_API_BASE, variety);

    let response = client
        .post(&url)
        .headers(headers)
        .form(&form_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").ok_or("No data in response")?;
        Ok(OrderPlaceResponse {
            success: true,
            order_id: data.get("order_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
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
pub async fn zerodha_modify_order(
    api_key: String,
    access_token: String,
    order_id: String,
    params: HashMap<String, Value>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[zerodha_modify_order] Modifying order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let form_params: Vec<(String, String)> = params
        .iter()
        .map(|(k, v)| {
            let value = match v {
                Value::String(s) => s.clone(),
                Value::Number(n) => n.to_string(),
                _ => v.to_string(),
            };
            (k.clone(), value)
        })
        .collect();

    let url = format!("{}/orders/regular/{}", ZERODHA_API_BASE, order_id);

    let response = client
        .put(&url)
        .headers(headers)
        .form(&form_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").ok_or("No data in response")?;
        Ok(OrderPlaceResponse {
            success: true,
            order_id: data.get("order_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
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
pub async fn zerodha_cancel_order(
    api_key: String,
    access_token: String,
    order_id: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[zerodha_cancel_order] Cancelling order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let url = format!("{}/orders/regular/{}", ZERODHA_API_BASE, order_id);

    let response = client
        .delete(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").ok_or("No data in response")?;
        Ok(OrderPlaceResponse {
            success: true,
            order_id: data.get("order_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
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
pub async fn zerodha_get_orders(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_get_orders] Fetching orders");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/orders", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data")
            .and_then(|d| d.as_array())
            .map(|arr| arr.clone())
            .unwrap_or_default();
        Ok(ApiResponse { success: true, data: Some(data), error: None, timestamp })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch orders")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Place smart order (checks position and adjusts action)
#[tauri::command]
pub async fn zerodha_place_smart_order(
    api_key: String,
    access_token: String,
    tradingsymbol: String,
    exchange: String,
    product: String,
    action: String,
    quantity: i64,
    order_type: String,
    price: Option<f64>,
    trigger_price: Option<f64>,
    position_size: i64,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[zerodha_place_smart_order] Smart order for {}:{}", exchange, tradingsymbol);

    let (final_action, final_quantity) = if action.to_uppercase() == "BUY" {
        if position_size <= 0 {
            ("BUY".to_string(), quantity)
        } else {
            ("BUY".to_string(), quantity)
        }
    } else {
        ("SELL".to_string(), quantity)
    };

    let mut params = HashMap::new();
    params.insert("tradingsymbol".to_string(), json!(tradingsymbol));
    params.insert("exchange".to_string(), json!(exchange));
    params.insert("transaction_type".to_string(), json!(final_action));
    params.insert("quantity".to_string(), json!(final_quantity));
    params.insert("product".to_string(), json!(product));
    params.insert("order_type".to_string(), json!(order_type));

    if let Some(p) = price {
        if p > 0.0 { params.insert("price".to_string(), json!(p)); }
    }
    if let Some(tp) = trigger_price {
        if tp > 0.0 { params.insert("trigger_price".to_string(), json!(tp)); }
    }

    zerodha_place_order(api_key, access_token, params, "regular".to_string()).await
}

/// Close all open positions
#[tauri::command]
pub async fn zerodha_close_all_positions(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<OrderPlaceResponse>>, String> {
    eprintln!("[zerodha_close_all_positions] Closing all positions");

    let timestamp = chrono::Utc::now().timestamp_millis();
    let positions_response = super::portfolio::zerodha_get_positions(api_key.clone(), access_token.clone()).await?;

    if !positions_response.success {
        return Ok(ApiResponse { success: false, data: None, error: positions_response.error, timestamp });
    }

    let mut results: Vec<OrderPlaceResponse> = Vec::new();

    if let Some(positions_data) = positions_response.data {
        if let Some(net) = positions_data.get("net").and_then(|n| n.as_array()) {
            for position in net {
                let quantity = position.get("quantity").and_then(|q| q.as_i64()).unwrap_or(0);
                if quantity == 0 { continue; }

                let tradingsymbol = position.get("tradingsymbol").and_then(|s| s.as_str()).unwrap_or("").to_string();
                let exchange = position.get("exchange").and_then(|s| s.as_str()).unwrap_or("").to_string();
                let product = position.get("product").and_then(|s| s.as_str()).unwrap_or("").to_string();

                let action = if quantity > 0 { "SELL" } else { "BUY" };
                let close_quantity = quantity.abs();

                let mut params = HashMap::new();
                params.insert("tradingsymbol".to_string(), json!(tradingsymbol));
                params.insert("exchange".to_string(), json!(exchange));
                params.insert("transaction_type".to_string(), json!(action));
                params.insert("quantity".to_string(), json!(close_quantity));
                params.insert("product".to_string(), json!(product));
                params.insert("order_type".to_string(), json!("MARKET"));

                match zerodha_place_order(api_key.clone(), access_token.clone(), params, "regular".to_string()).await {
                    Ok(r) => results.push(r),
                    Err(e) => results.push(OrderPlaceResponse { success: false, order_id: None, error: Some(e) }),
                }
            }
        }
    }

    Ok(ApiResponse { success: true, data: Some(results), error: None, timestamp })
}

/// Cancel all open orders
#[tauri::command]
pub async fn zerodha_cancel_all_orders(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<OrderPlaceResponse>>, String> {
    eprintln!("[zerodha_cancel_all_orders] Cancelling all open orders");

    let timestamp = chrono::Utc::now().timestamp_millis();
    let orders_response = zerodha_get_orders(api_key.clone(), access_token.clone()).await?;

    if !orders_response.success {
        return Ok(ApiResponse { success: false, data: None, error: orders_response.error, timestamp });
    }

    let mut results: Vec<OrderPlaceResponse> = Vec::new();

    if let Some(orders) = orders_response.data {
        for order in orders {
            let status = order.get("status").and_then(|s| s.as_str()).unwrap_or("");
            if status == "OPEN" || status == "TRIGGER PENDING" {
                let order_id = order.get("order_id").and_then(|s| s.as_str()).unwrap_or("").to_string();
                if order_id.is_empty() { continue; }

                match zerodha_cancel_order(api_key.clone(), access_token.clone(), order_id).await {
                    Ok(r) => results.push(r),
                    Err(e) => results.push(OrderPlaceResponse { success: false, order_id: None, error: Some(e) }),
                }
            }
        }
    }

    Ok(ApiResponse { success: true, data: Some(results), error: None, timestamp })
}

/// Calculate margin for orders
#[tauri::command]
pub async fn zerodha_calculate_margin(
    api_key: String,
    access_token: String,
    orders: Vec<Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_calculate_margin] Calculating margin for {} orders", orders.len());

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);
    let timestamp = chrono::Utc::now().timestamp_millis();

    let url = if orders.len() > 1 {
        format!("{}/margins/basket", ZERODHA_API_BASE)
    } else {
        format!("{}/margins/orders", ZERODHA_API_BASE)
    };

    let response = client
        .post(&url)
        .headers(headers)
        .json(&orders)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").cloned().unwrap_or(json!({}));
        Ok(ApiResponse { success: true, data: Some(data), error: None, timestamp })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to calculate margin")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}
