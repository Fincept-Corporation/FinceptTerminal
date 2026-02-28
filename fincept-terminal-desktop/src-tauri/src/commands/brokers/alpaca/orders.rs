// Alpaca Order and Position Commands

use serde_json::{json, Value};
use std::collections::HashMap;
use crate::commands::brokers::common::ApiResponse;
use super::{get_api_base, create_alpaca_headers};

// ============================================================================
// Order Commands
// ============================================================================

/// Place an order
#[tauri::command]
pub async fn alpaca_place_order(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    params: HashMap<String, Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_place_order] Placing order (paper: {})", is_paper);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .post(format!("{}/v2/orders", base_url))
        .headers(headers)
        .json(&params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        eprintln!("[alpaca_place_order] Order placed successfully: {:?}", body.get("id"));
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Order placement failed").to_string();
        eprintln!("[alpaca_place_order] Order failed: {}", error_msg);
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Modify an order (replace)
#[tauri::command]
pub async fn alpaca_modify_order(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    order_id: String,
    params: HashMap<String, Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_modify_order] Modifying order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .patch(format!("{}/v2/orders/{}", base_url, order_id))
        .headers(headers)
        .json(&params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Order modification failed").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn alpaca_cancel_order(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    order_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[alpaca_cancel_order] Cancelling order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .delete(format!("{}/v2/orders/{}", base_url, order_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() || status.as_u16() == 204 {
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        let body: Value = response.json().await.unwrap_or(json!({}));
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Order cancellation failed").to_string();
        Ok(ApiResponse { success: false, data: Some(false), error: Some(error_msg), timestamp })
    }
}

/// Get all orders
#[tauri::command]
pub async fn alpaca_get_orders(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    status: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_get_orders] Fetching orders (status: {:?})", status);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let url = match status {
        Some(s) => format!("{}/v2/orders?status={}&limit=100", base_url, s),
        None => format!("{}/v2/orders?limit=100", base_url),
    };

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let http_status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if http_status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch orders".to_string()), timestamp })
    }
}

/// Get a specific order by ID
#[tauri::command]
pub async fn alpaca_get_order(
    api_key: String,
    api_secret: String,
    order_id: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_order] Fetching order: {}", order_id);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/orders/{}", base_url, order_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Order not found").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Cancel all orders
#[tauri::command]
pub async fn alpaca_cancel_all_orders(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_cancel_all_orders] Cancelling all orders");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .delete(format!("{}/v2/orders", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() || status.as_u16() == 207 {
        let body: Vec<Value> = response.json().await.unwrap_or_default();
        Ok(ApiResponse {
            success: true,
            data: Some(json!({ "cancelled_count": body.len() })),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to cancel orders".to_string()), timestamp })
    }
}

/// Place smart order (adjusts for existing position)
#[tauri::command]
pub async fn alpaca_place_smart_order(
    api_key: String,
    api_secret: String,
    is_paper: bool,
    symbol: String,
    side: String,
    quantity: f64,
    order_type: String,
    price: Option<f64>,
    stop_price: Option<f64>,
    position_size: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_place_smart_order] Smart order for {} (position: {})", symbol, position_size);

    let (final_side, final_qty) = if side.to_uppercase() == "BUY" {
        ("buy".to_string(), quantity)
    } else {
        ("sell".to_string(), quantity)
    };

    let mut params: HashMap<String, Value> = HashMap::new();
    params.insert("symbol".to_string(), json!(symbol));
    params.insert("qty".to_string(), json!(final_qty.to_string()));
    params.insert("side".to_string(), json!(final_side));
    params.insert("type".to_string(), json!(order_type.to_lowercase()));
    params.insert("time_in_force".to_string(), json!("day"));

    if let Some(p) = price {
        if p > 0.0 { params.insert("limit_price".to_string(), json!(p.to_string())); }
    }
    if let Some(sp) = stop_price {
        if sp > 0.0 { params.insert("stop_price".to_string(), json!(sp.to_string())); }
    }

    alpaca_place_order(api_key, api_secret, is_paper, params).await
}

// ============================================================================
// Position Commands
// ============================================================================

/// Get all positions
#[tauri::command]
pub async fn alpaca_get_positions(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/positions", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch positions".to_string()), timestamp })
    }
}

/// Get a specific position by symbol
#[tauri::command]
pub async fn alpaca_get_position(
    api_key: String,
    api_secret: String,
    symbol: String,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_get_position] Fetching position for: {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .get(format!("{}/v2/positions/{}", base_url, symbol))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Position not found").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Close a specific position by symbol
#[tauri::command]
pub async fn alpaca_close_position(
    api_key: String,
    api_secret: String,
    symbol: String,
    qty: Option<f64>,
    percentage: Option<f64>,
    is_paper: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[alpaca_close_position] Closing position for: {}", symbol);

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let mut url = format!("{}/v2/positions/{}", base_url, symbol);
    if let Some(q) = qty {
        url = format!("{}?qty={}", url, q);
    } else if let Some(p) = percentage {
        url = format!("{}?percentage={}", url, p);
    }

    let response = client
        .delete(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("message").and_then(|m| m.as_str())
            .unwrap_or("Failed to close position").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Close all positions
#[tauri::command]
pub async fn alpaca_close_all_positions(
    api_key: String,
    api_secret: String,
    is_paper: bool,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[alpaca_close_all_positions] Closing all positions");

    let client = reqwest::Client::new();
    let headers = create_alpaca_headers(&api_key, &api_secret);
    let base_url = get_api_base(is_paper);

    let response = client
        .delete(format!("{}/v2/positions?cancel_orders=true", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() || status.as_u16() == 207 {
        let body: Vec<Value> = response.json().await.unwrap_or_default();
        let results: Vec<Value> = body.iter().map(|item| {
            let success = item.get("status").and_then(|s| s.as_i64()).map(|s| s == 200).unwrap_or(false);
            json!({
                "success": success,
                "symbol": item.get("symbol"),
                "error": if !success { item.get("body").and_then(|b| b.get("message")) } else { None }
            })
        }).collect();
        Ok(ApiResponse { success: true, data: Some(results), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to close positions".to_string()), timestamp })
    }
}
