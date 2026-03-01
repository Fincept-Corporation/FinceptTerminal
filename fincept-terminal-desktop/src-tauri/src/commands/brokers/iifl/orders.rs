// IIFL Order Management Commands

use super::{IIFL_INTERACTIVE_URL, create_iifl_headers};
use super::super::common::{ApiResponse, OrderPlaceResponse};
use serde_json::{json, Value};
use std::collections::HashMap;

/// Place an order
#[tauri::command]
pub async fn iifl_place_order(
    access_token: String,
    params: HashMap<String, Value>,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[iifl_place_order] Placing order");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

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

        Ok(OrderPlaceResponse { success: true, order_id, error: None })
    } else {
        let error_msg = body
            .get("description")
            .or(body.get("message"))
            .and_then(|m| m.as_str())
            .unwrap_or("Order placement failed")
            .to_string();
        Ok(OrderPlaceResponse { success: false, order_id: None, error: Some(error_msg) })
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
        Ok(OrderPlaceResponse { success: true, order_id: Some(order_id), error: None })
    } else {
        let error_msg = body
            .get("description")
            .or(body.get("message"))
            .and_then(|m| m.as_str())
            .unwrap_or("Order modification failed")
            .to_string();
        Ok(OrderPlaceResponse { success: false, order_id: None, error: Some(error_msg) })
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

    let url = format!("{}/orders?appOrderID={}", IIFL_INTERACTIVE_URL, order_id);

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
        Ok(OrderPlaceResponse { success: true, order_id: Some(order_id), error: None })
    } else {
        let error_msg = body
            .get("description")
            .or(body.get("message"))
            .and_then(|m| m.as_str())
            .unwrap_or("Order cancellation failed")
            .to_string();
        Ok(OrderPlaceResponse { success: false, order_id: None, error: Some(error_msg) })
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
        Ok(ApiResponse { success: true, data: Some(orders), error: None, timestamp })
    } else {
        let error_msg = body
            .get("description")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch orders")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
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
        Ok(ApiResponse { success: true, data: Some(trades), error: None, timestamp })
    } else {
        let error_msg = body
            .get("description")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch trade book")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Close all open positions
#[tauri::command]
pub async fn iifl_close_all_positions(
    access_token: String,
) -> Result<ApiResponse<Vec<OrderPlaceResponse>>, String> {
    eprintln!("[iifl_close_all_positions] Closing all positions");

    let timestamp = chrono::Utc::now().timestamp_millis();
    let positions_response = super::portfolio::iifl_get_positions(access_token.clone()).await?;

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
                let quantity = position.get("Quantity").and_then(|q| q.as_i64()).unwrap_or(0);
                if quantity == 0 { continue; }

                let exchange_segment = position.get("ExchangeSegment").and_then(|e| e.as_str()).unwrap_or("NSECM");
                let instrument_id = position.get("ExchangeInstrumentId").and_then(|i| i.as_i64()).unwrap_or(0);
                let product_type = position.get("ProductType").and_then(|p| p.as_str()).unwrap_or("MIS");
                let action = if quantity > 0 { "SELL" } else { "BUY" };
                let close_quantity = quantity.abs();

                let mut params = HashMap::new();
                params.insert("exchangeSegment".to_string(), json!(exchange_segment));
                params.insert("exchangeInstrumentID".to_string(), json!(instrument_id));
                params.insert("productType".to_string(), json!(product_type));
                params.insert("orderType".to_string(), json!("MARKET"));
                params.insert("orderSide".to_string(), json!(action));
                params.insert("orderQuantity".to_string(), json!(close_quantity));

                let order_result = iifl_place_order(access_token.clone(), params).await;
                match order_result {
                    Ok(response) => results.push(response),
                    Err(e) => results.push(OrderPlaceResponse { success: false, order_id: None, error: Some(e) }),
                }
            }
        }
    }

    Ok(ApiResponse { success: true, data: Some(results), error: None, timestamp })
}

/// Cancel all open orders
#[tauri::command]
pub async fn iifl_cancel_all_orders(
    access_token: String,
) -> Result<ApiResponse<Vec<OrderPlaceResponse>>, String> {
    eprintln!("[iifl_cancel_all_orders] Cancelling all open orders");

    let timestamp = chrono::Utc::now().timestamp_millis();
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
            let status = order.get("OrderStatus").and_then(|s| s.as_str()).unwrap_or("");
            if status == "New" || status == "Trigger Pending" {
                let order_id = order
                    .get("AppOrderID")
                    .and_then(|id| id.as_i64())
                    .map(|id| id.to_string())
                    .unwrap_or_default();

                if order_id.is_empty() { continue; }

                let cancel_result = iifl_cancel_order(access_token.clone(), order_id).await;
                match cancel_result {
                    Ok(response) => results.push(response),
                    Err(e) => results.push(OrderPlaceResponse { success: false, order_id: None, error: Some(e) }),
                }
            }
        }
    }

    Ok(ApiResponse { success: true, data: Some(results), error: None, timestamp })
}
