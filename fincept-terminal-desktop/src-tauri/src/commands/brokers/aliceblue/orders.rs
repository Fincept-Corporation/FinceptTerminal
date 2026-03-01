use super::{ALICEBLUE_API_BASE, create_aliceblue_headers};
use super::super::common::{ApiResponse, OrderPlaceResponse};
use serde_json::{json, Value};

/// Place an order
#[tauri::command]
pub async fn aliceblue_place_order(
    api_secret: String,
    session_id: String,
    trading_symbol: String,
    exchange: String,
    transaction_type: String,
    order_type: String,
    quantity: i32,
    price: Option<f64>,
    trigger_price: Option<f64>,
    product: String,
    token: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[aliceblue_place_order] Placing order for {}", trading_symbol);

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    // Map order type to Alice Blue format
    let ab_order_type = match order_type.as_str() {
        "MARKET" => "MKT",
        "LIMIT" => "L",
        "SL" | "STOP_LOSS" => "SL",
        "SL-M" | "STOP_LOSS_MARKET" => "SL-M",
        _ => "MKT",
    };

    // Build order payload (Alice Blue expects array)
    let order = json!({
        "complexty": "regular",
        "discqty": "0",
        "exch": exchange,
        "pCode": product,
        "prctyp": ab_order_type,
        "price": price.unwrap_or(0.0).to_string(),
        "qty": quantity.to_string(),
        "ret": "DAY",
        "symbol_id": token,
        "trading_symbol": trading_symbol,
        "transtype": transaction_type.to_uppercase(),
        "trigPrice": trigger_price.unwrap_or(0.0).to_string(),
        "orderTag": "fincept",
    });

    let payload = json!([order]);

    let response = client
        .post(format!("{}/placeOrder/executePlaceOrder", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[aliceblue_place_order] Response: {:?}", body);

    // Response is an array
    if status.is_success() {
        if let Some(arr) = body.as_array() {
            if let Some(first) = arr.get(0) {
                if first.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
                    let order_id = first.get("NOrdNo")
                        .and_then(|v| v.as_str())
                        .map(String::from);

                    return Ok(OrderPlaceResponse {
                        success: true,
                        order_id,
                        error: None,
                    });
                } else {
                    let error_msg = first.get("emsg")
                        .and_then(|m| m.as_str())
                        .unwrap_or("Order placement failed")
                        .to_string();
                    return Ok(OrderPlaceResponse {
                        success: false,
                        order_id: None,
                        error: Some(error_msg),
                    });
                }
            }
        }
    }

    Ok(OrderPlaceResponse {
        success: false,
        order_id: None,
        error: Some("Order placement failed".to_string()),
    })
}

/// Modify an order
#[tauri::command]
pub async fn aliceblue_modify_order(
    api_secret: String,
    session_id: String,
    order_id: String,
    trading_symbol: String,
    exchange: String,
    transaction_type: String,
    order_type: String,
    quantity: i32,
    price: Option<f64>,
    trigger_price: Option<f64>,
    product: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[aliceblue_modify_order] Modifying order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    // Map order type to Alice Blue format
    let ab_order_type = match order_type.as_str() {
        "MARKET" => "MKT",
        "LIMIT" => "L",
        "SL" | "STOP_LOSS" => "SL",
        "SL-M" | "STOP_LOSS_MARKET" => "SL-M",
        _ => "MKT",
    };

    let payload = json!({
        "discqty": 0,
        "exch": exchange,
        "filledQuantity": 0,
        "nestOrderNumber": order_id,
        "prctyp": ab_order_type,
        "price": price.unwrap_or(0.0),
        "qty": quantity,
        "trading_symbol": trading_symbol,
        "trigPrice": trigger_price.unwrap_or(0.0).to_string(),
        "transtype": transaction_type.to_uppercase(),
        "pCode": product,
    });

    let response = client
        .post(format!("{}/placeOrder/modifyOrder", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        let modified_order_id = body.get("nestOrderNumber")
            .and_then(|v| v.as_str())
            .map(String::from)
            .unwrap_or(order_id);

        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(modified_order_id),
            error: None,
        })
    } else {
        let error_msg = body.get("emsg")
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
pub async fn aliceblue_cancel_order(
    api_secret: String,
    session_id: String,
    order_id: String,
    trading_symbol: String,
    exchange: String,
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[aliceblue_cancel_order] Cancelling order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let payload = json!({
        "exch": exchange,
        "nestOrderNumber": order_id,
        "trading_symbol": trading_symbol,
    });

    let response = client
        .post(format!("{}/placeOrder/cancelOrder", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        let cancelled_order_id = body.get("nestOrderNumber")
            .and_then(|v| v.as_str())
            .map(String::from)
            .unwrap_or(order_id);

        Ok(OrderPlaceResponse {
            success: true,
            order_id: Some(cancelled_order_id),
            error: None,
        })
    } else {
        let error_msg = body.get("emsg")
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
pub async fn aliceblue_get_orders(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_orders] Fetching order book");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/placeOrder/fetchOrderBook", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Response can be array of orders or error object
    if let Some(orders) = body.as_array() {
        // Check if first item is error
        if let Some(first) = orders.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: true,
                    data: Some(vec![]),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(orders.clone()),
            error: None,
            timestamp,
        })
    } else if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch order book".to_string()),
            timestamp,
        })
    }
}

/// Get trade book
#[tauri::command]
pub async fn aliceblue_get_trades(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_trades] Fetching trade book");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/placeOrder/fetchTradeBook", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if let Some(trades) = body.as_array() {
        if let Some(first) = trades.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: true,
                    data: Some(vec![]),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(trades.clone()),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    }
}
