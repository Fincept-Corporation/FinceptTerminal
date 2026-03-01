// Zerodha Portfolio Commands — Positions, Holdings, Margins, Trade Book

use super::{ZERODHA_API_BASE, create_zerodha_headers};
use super::super::common::ApiResponse;
use serde_json::{json, Value};

/// Get positions
#[tauri::command]
pub async fn zerodha_get_positions(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/portfolio/positions", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").cloned().unwrap_or(json!({}));
        Ok(ApiResponse { success: true, data: Some(data), error: None, timestamp })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch positions")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get holdings
#[tauri::command]
pub async fn zerodha_get_holdings(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/portfolio/holdings", ZERODHA_API_BASE))
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
            .unwrap_or("Failed to fetch holdings")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get margins/funds
#[tauri::command]
pub async fn zerodha_get_margins(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_margins] Fetching margins");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/user/margins", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").cloned().unwrap_or(json!({}));
        Ok(ApiResponse { success: true, data: Some(data), error: None, timestamp })
    } else {
        let error_msg = body.get("message")
            .and_then(|m| m.as_str())
            .unwrap_or("Failed to fetch margins")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get trade book (executed trades)
#[tauri::command]
pub async fn zerodha_get_trade_book(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[zerodha_get_trade_book] Fetching trade book");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/trades", ZERODHA_API_BASE))
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
            .unwrap_or("Failed to fetch trade book")
            .to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Get open position for a specific symbol
#[tauri::command]
pub async fn zerodha_get_open_position(
    api_key: String,
    access_token: String,
    tradingsymbol: String,
    exchange: String,
    product: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[zerodha_get_open_position] Fetching position for {}:{}", exchange, tradingsymbol);

    let positions_response = zerodha_get_positions(api_key, access_token).await?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if !positions_response.success {
        return Ok(ApiResponse { success: false, data: None, error: positions_response.error, timestamp });
    }

    if let Some(positions_data) = positions_response.data {
        if let Some(net) = positions_data.get("net").and_then(|n| n.as_array()) {
            for position in net {
                let pos_symbol = position.get("tradingsymbol").and_then(|s| s.as_str()).unwrap_or("");
                let pos_exchange = position.get("exchange").and_then(|s| s.as_str()).unwrap_or("");
                let pos_product = position.get("product").and_then(|s| s.as_str()).unwrap_or("");

                if pos_symbol == tradingsymbol && pos_exchange == exchange && pos_product == product {
                    let quantity = position.get("quantity").and_then(|q| q.as_i64()).unwrap_or(0);
                    return Ok(ApiResponse {
                        success: true,
                        data: Some(json!({
                            "tradingsymbol": pos_symbol,
                            "exchange": pos_exchange,
                            "product": pos_product,
                            "quantity": quantity,
                            "position": position.clone()
                        })),
                        error: None,
                        timestamp,
                    });
                }
            }
        }
    }

    Ok(ApiResponse {
        success: true,
        data: Some(json!({
            "tradingsymbol": tradingsymbol,
            "exchange": exchange,
            "product": product,
            "quantity": 0,
            "position": null
        })),
        error: None,
        timestamp,
    })
}
