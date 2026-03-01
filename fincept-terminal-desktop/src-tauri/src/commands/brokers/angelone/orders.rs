// AngelOne Order Management Commands

use serde_json::{json, Value};
use super::super::common::ApiResponse;
use super::angel_api_call;

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
