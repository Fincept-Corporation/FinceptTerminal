// commands/brokers/dhan/orders.rs - Order management commands

use serde_json::{json, Value};
use super::{ApiResponse, get_timestamp, dhan_request, map_exchange_to_dhan, map_product_to_dhan, map_order_type_to_dhan};

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
