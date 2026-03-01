// Kotak Order Management Commands

use super::{ApiResponse, KotakOrderResponse, kotak_request, parse_auth_token, encode_jdata, map_exchange_to_kotak, map_order_type_to_kotak, get_kotak_br_symbol, get_kotak_token};
use reqwest::Client;
use serde_json::json;
use tauri::command;

/// Place a new order
#[command]
pub async fn kotak_place_order(
    auth_token: String,
    symbol: String,
    exchange: String,
    side: String,
    quantity: i32,
    order_type: String,
    product_type: String,
    price: Option<f64>,
    trigger_price: Option<f64>,
) -> ApiResponse<KotakOrderResponse> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let br_symbol = get_kotak_br_symbol(&symbol, &exchange).unwrap_or(symbol.clone());

    let order_data = json!({
        "am": "NO", "dq": "0", "bc": "1",
        "es": map_exchange_to_kotak(&exchange),
        "mp": "0", "pc": product_type, "pf": "N",
        "pr": price.unwrap_or(0.0).to_string(),
        "pt": map_order_type_to_kotak(&order_type),
        "qt": quantity.to_string(), "rt": "DAY",
        "tp": trigger_price.unwrap_or(0.0).to_string(),
        "ts": br_symbol,
        "tt": if side == "BUY" { "B" } else { "S" }
    });

    let client = Client::new();
    let payload = encode_jdata(&order_data);

    match kotak_request(&client, "POST", &base_url, "/quick/order/rule/ms/place", &trading_token, &trading_sid, Some(payload)).await {
        Ok(data) => {
            if data.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
                ApiResponse {
                    success: true,
                    data: Some(KotakOrderResponse {
                        order_id: data.get("nOrdNo").and_then(|v| v.as_str()).map(|s| s.to_string()),
                        status: "success".to_string(),
                        message: None,
                    }),
                    error: None,
                }
            } else {
                let err_msg = data.get("emsg").and_then(|v| v.as_str()).unwrap_or("Order placement failed");
                ApiResponse {
                    success: false,
                    data: Some(KotakOrderResponse { order_id: None, status: "error".to_string(), message: Some(err_msg.to_string()) }),
                    error: Some(err_msg.to_string()),
                }
            }
        }
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}

/// Modify an existing order
#[command]
pub async fn kotak_modify_order(
    auth_token: String,
    order_id: String,
    symbol: String,
    exchange: String,
    side: String,
    quantity: i32,
    order_type: String,
    product_type: String,
    price: Option<f64>,
    trigger_price: Option<f64>,
) -> ApiResponse<KotakOrderResponse> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let br_symbol = get_kotak_br_symbol(&symbol, &exchange).unwrap_or(symbol.clone());
    let token = get_kotak_token(&symbol, &exchange).unwrap_or_default();

    let order_data = json!({
        "tk": token, "dq": "0",
        "es": map_exchange_to_kotak(&exchange),
        "mp": "0", "dd": "NA", "vd": "DAY",
        "pc": product_type,
        "pr": price.unwrap_or(0.0).to_string(),
        "pt": map_order_type_to_kotak(&order_type),
        "qt": quantity.to_string(),
        "tp": trigger_price.unwrap_or(0.0).to_string(),
        "ts": br_symbol, "no": order_id,
        "tt": if side == "BUY" { "B" } else { "S" }
    });

    let client = Client::new();
    let payload = encode_jdata(&order_data);

    match kotak_request(&client, "POST", &base_url, "/quick/order/vr/modify", &trading_token, &trading_sid, Some(payload)).await {
        Ok(data) => {
            if data.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
                ApiResponse {
                    success: true,
                    data: Some(KotakOrderResponse {
                        order_id: data.get("nOrdNo").and_then(|v| v.as_str()).map(|s| s.to_string()),
                        status: "success".to_string(),
                        message: None,
                    }),
                    error: None,
                }
            } else {
                let err_msg = data.get("emsg").and_then(|v| v.as_str()).unwrap_or("Order modification failed");
                ApiResponse {
                    success: false,
                    data: Some(KotakOrderResponse { order_id: None, status: "error".to_string(), message: Some(err_msg.to_string()) }),
                    error: Some(err_msg.to_string()),
                }
            }
        }
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}

/// Cancel an order
#[command]
pub async fn kotak_cancel_order(
    auth_token: String,
    order_id: String,
) -> ApiResponse<KotakOrderResponse> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let cancel_data = json!({ "on": order_id, "am": "NO" });
    let client = Client::new();
    let payload = encode_jdata(&cancel_data);

    match kotak_request(&client, "POST", &base_url, "/quick/order/cancel", &trading_token, &trading_sid, Some(payload)).await {
        Ok(data) => {
            if data.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
                ApiResponse {
                    success: true,
                    data: Some(KotakOrderResponse {
                        order_id: data.get("nOrdNo").and_then(|v| v.as_str()).map(|s| s.to_string()),
                        status: "success".to_string(),
                        message: None,
                    }),
                    error: None,
                }
            } else {
                let err_msg = data.get("emsg").and_then(|v| v.as_str()).unwrap_or("Order cancellation failed");
                ApiResponse {
                    success: false,
                    data: Some(KotakOrderResponse { order_id: None, status: "error".to_string(), message: Some(err_msg.to_string()) }),
                    error: Some(err_msg.to_string()),
                }
            }
        }
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}
