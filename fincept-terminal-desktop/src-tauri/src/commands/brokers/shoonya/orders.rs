// Shoonya Order Management Commands

use super::{SHOONYA_API_BASE, create_shoonya_headers, create_payload_with_auth};
use super::super::common::{ApiResponse, OrderPlaceResponse};
use serde_json::json;

/// Place a new order
#[tauri::command]
pub async fn shoonya_place_order(
    access_token: String,
    user_id: String,
    symbol: String,         // Trading symbol (broker format)
    exchange: String,       // NSE, BSE, NFO, MCX, CDS, BFO
    quantity: i32,
    price: f64,
    trigger_price: Option<f64>,
    disclosed_qty: Option<i32>,
    product_type: String,   // C (CNC), M (NRML), I (MIS)
    transaction_type: String, // B (Buy), S (Sell)
    order_type: String,     // MKT, LMT, SL-LMT, SL-MKT
) -> Result<OrderPlaceResponse, String> {
    eprintln!("[shoonya_place_order] Placing order for {} on {}", symbol, exchange);

    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "actid": user_id,
        "exch": exchange,
        "tsym": symbol,
        "qty": quantity.to_string(),
        "prc": price.to_string(),
        "trgprc": trigger_price.unwrap_or(0.0).to_string(),
        "dscqty": disclosed_qty.unwrap_or(0).to_string(),
        "prd": product_type,
        "trantype": transaction_type,
        "prctyp": order_type,
        "mkt_protection": "0",
        "ret": "DAY",
        "ordersource": "API"
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/PlaceOrder", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[shoonya_place_order] Response: {:?}", body);

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        Ok(OrderPlaceResponse {
            success: true,
            order_id: body.get("norenordno").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("emsg")
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
pub async fn shoonya_modify_order(
    access_token: String,
    user_id: String,
    order_id: String,
    exchange: String,
    symbol: String,
    quantity: i32,
    price: f64,
    trigger_price: Option<f64>,
    disclosed_qty: Option<i32>,
    order_type: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    eprintln!("[shoonya_modify_order] Modifying order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "exch": exchange,
        "norenordno": order_id,
        "prctyp": order_type,
        "prc": price.to_string(),
        "qty": quantity.to_string(),
        "tsym": symbol,
        "ret": "DAY",
        "mkt_protection": "0",
        "trgprc": trigger_price.unwrap_or(0.0).to_string(),
        "dscqty": disclosed_qty.unwrap_or(0).to_string()
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/ModifyOrder", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Order modification failed")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn shoonya_cancel_order(
    access_token: String,
    user_id: String,
    order_id: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    eprintln!("[shoonya_cancel_order] Cancelling order {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({
        "uid": user_id,
        "norenordno": order_id
    });

    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/CancelOrder", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        Ok(ApiResponse { success: true, data: Some(json!({"order_id": order_id})), error: None, timestamp })
    } else {
        let error_msg = body.get("emsg")
            .and_then(|m| m.as_str())
            .unwrap_or("Order cancellation failed")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get order book
#[tauri::command]
pub async fn shoonya_get_orders(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({ "uid": user_id, "actid": user_id });
    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/OrderBook", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    // Shoonya returns array on success, dict with stat=Not_Ok on error
    if status.is_success() {
        if body.is_array() {
            Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
        } else if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
            // No orders case
            Ok(ApiResponse { success: true, data: Some(json!([])), error: None, timestamp })
        } else {
            Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
        }
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch orders".to_string()), timestamp })
    }
}

/// Get trade book
#[tauri::command]
pub async fn shoonya_get_trade_book(
    access_token: String,
    user_id: String,
) -> Result<ApiResponse<serde_json::Value>, String> {
    let timestamp = chrono::Utc::now().timestamp();
    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    let payload = json!({ "uid": user_id, "actid": user_id });
    let payload_str = create_payload_with_auth(&payload, &access_token);

    let response = client
        .post(format!("{}/NorenWClientTP/TradeBook", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() {
        if body.is_array() || body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
            Ok(ApiResponse {
                success: true,
                data: Some(if body.is_array() { body } else { json!([]) }),
                error: None,
                timestamp,
            })
        } else {
            Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
        }
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch trade book".to_string()), timestamp })
    }
}
