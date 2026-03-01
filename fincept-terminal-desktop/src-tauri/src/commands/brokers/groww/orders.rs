// commands/brokers/groww/orders.rs - Order management commands

use serde_json::{json, Value};
use super::super::common::{ApiResponse, OrderPlaceResponse};
use super::{GROWW_API_BASE, SEGMENT_CASH, SEGMENT_FNO, EXCHANGE_NSE, EXCHANGE_BSE, create_groww_headers};

/// Place an order
#[tauri::command]
pub async fn groww_place_order(
    access_token: String,
    trading_symbol: String,
    exchange: String,
    transaction_type: String,
    order_type: String,
    quantity: i32,
    price: Option<f64>,
    trigger_price: Option<f64>,
    product: String,
    validity: Option<String>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[groww_place_order] Placing order for {}", trading_symbol);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    // Map exchange to segment
    let segment = match exchange.as_str() {
        "NSE" | "BSE" => SEGMENT_CASH,
        "NFO" | "BFO" => SEGMENT_FNO,
        _ => SEGMENT_CASH,
    };

    // Map exchange for Groww API
    let groww_exchange = match exchange.as_str() {
        "NFO" => EXCHANGE_NSE,
        "BFO" => EXCHANGE_BSE,
        _ => exchange.as_str(),
    };

    // Map order type to Groww format
    let groww_order_type = match order_type.as_str() {
        "MARKET" => "MARKET",
        "LIMIT" => "LIMIT",
        "SL" | "STOP_LOSS" => "STOP_LOSS_LIMIT",
        "SL-M" | "STOP_LOSS_MARKET" => "STOP_LOSS_MARKET",
        _ => "MARKET",
    };

    // Build order payload
    let mut payload = json!({
        "trading_symbol": trading_symbol,
        "exchange": groww_exchange,
        "segment": segment,
        "transaction_type": transaction_type.to_uppercase(),
        "order_type": groww_order_type,
        "quantity": quantity,
        "product": product.to_uppercase(),
        "validity": validity.unwrap_or_else(|| "DAY".to_string()),
    });

    // Add price for limit orders
    if let Some(p) = price {
        if p > 0.0 {
            payload["price"] = json!(p);
        }
    }

    // Add trigger price for stop loss orders
    if let Some(tp) = trigger_price {
        if tp > 0.0 {
            payload["trigger_price"] = json!(tp);
        }
    }

    let response = client
        .post(format!("{}/v1/order/create", GROWW_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        let empty_obj = json!({});
        let payload_data = body.get("payload").unwrap_or(&empty_obj);
        let order_id = payload_data.get("groww_order_id")
            .or_else(|| payload_data.get("order_id"))
            .and_then(|v| v.as_str())
            .map(String::from);

        Ok(OrderPlaceResponse {
            success: true,
            order_id,
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
pub async fn groww_modify_order(
    access_token: String,
    order_id: String,
    segment: String,
    quantity: Option<i32>,
    price: Option<f64>,
    trigger_price: Option<f64>,
    order_type: Option<String>,
    validity: Option<String>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[groww_modify_order] Modifying order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let mut payload = json!({
        "groww_order_id": order_id,
        "segment": segment,
    });

    if let Some(q) = quantity {
        payload["quantity"] = json!(q);
    }
    if let Some(p) = price {
        payload["price"] = json!(p);
    }
    if let Some(tp) = trigger_price {
        payload["trigger_price"] = json!(tp);
    }
    if let Some(ot) = order_type {
        let groww_order_type = match ot.as_str() {
            "MARKET" => "MARKET",
            "LIMIT" => "LIMIT",
            "SL" => "STOP_LOSS_LIMIT",
            "SL-M" => "STOP_LOSS_MARKET",
            _ => &ot,
        };
        payload["order_type"] = json!(groww_order_type);
    }
    if let Some(v) = validity {
        payload["validity"] = json!(v);
    }

    let response = client
        .post(format!("{}/v1/order/modify", GROWW_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(order_id),
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
pub async fn groww_cancel_order(
    access_token: String,
    order_id: String,
    segment: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[groww_cancel_order] Cancelling order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let payload = json!({
        "groww_order_id": order_id,
        "segment": segment,
    });

    let response = client
        .post(format!("{}/v1/order/cancel", GROWW_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(order_id),
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

/// Get order book
#[tauri::command]
pub async fn groww_get_orders(
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[groww_get_orders] Fetching order book");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    let mut all_orders = Vec::new();
    let segments = [SEGMENT_CASH, SEGMENT_FNO];

    for segment in segments {
        let mut page = 0;
        let page_size = 25;

        loop {
            let response = client
                .get(format!("{}/v1/order/list", GROWW_API_BASE))
                .headers(headers.clone())
                .query(&[
                    ("segment", segment),
                    ("page", &page.to_string()),
                    ("page_size", &page_size.to_string()),
                ])
                .send()
                .await
                .map_err(|e| format!("Request failed: {}", e))?;

            let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

            if body.get("status").and_then(|s| s.as_str()) != Some("SUCCESS") {
                break;
            }

            if let Some(orders) = body.get("payload")
                .and_then(|p| p.get("order_list"))
                .and_then(|o| o.as_array())
            {
                if orders.is_empty() {
                    break;
                }
                all_orders.extend(orders.clone());

                if orders.len() < page_size as usize {
                    break;
                }
                page += 1;
            } else {
                break;
            }
        }
    }

    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: true,
        data: Some(all_orders),
        error: None,
        timestamp,
    })
}
