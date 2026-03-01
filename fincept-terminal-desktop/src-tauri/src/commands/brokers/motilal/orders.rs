// Motilal Oswal Order Management Commands

use reqwest::Client;
use serde_json::json;
use tauri::command;
use super::{ApiResponse, get_timestamp, get_motilal_headers, motilal_request};

/// Place order with Motilal Oswal
#[command]
pub async fn motilal_place_order(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    exchange: String,
    symbol_token: i64,
    buy_or_sell: String,
    order_type: String,
    product_type: String,
    quantity: i64,
    price: f64,
    trigger_price: f64,
    disclosed_quantity: i64,
    validity: String,
    amo: bool,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    let payload = json!({
        "exchange": exchange,
        "symboltoken": symbol_token,
        "buyorsell": buy_or_sell.to_uppercase(),
        "ordertype": order_type.to_uppercase(),
        "producttype": product_type.to_uppercase(),
        "orderduration": validity.to_uppercase(),
        "price": price,
        "triggerprice": trigger_price,
        "quantityinlot": quantity,
        "disclosedquantity": disclosed_quantity,
        "amoorder": if amo { "Y" } else { "N" }
    });

    match motilal_request(&client, "POST", "/rest/trans/v1/placeorder", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let order_id = data.get("uniqueorderid")
                    .and_then(|o| o.as_str())
                    .unwrap_or("");
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "order_id": order_id,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Order placement failed")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Modify order with Motilal Oswal
#[command]
pub async fn motilal_modify_order(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    order_id: String,
    order_type: String,
    quantity: i64,
    price: f64,
    trigger_price: f64,
    disclosed_quantity: i64,
    last_modified_time: String,
    qty_traded_today: i64,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    let payload = json!({
        "uniqueorderid": order_id,
        "newordertype": order_type.to_uppercase(),
        "neworderduration": "DAY",
        "newprice": price,
        "newtriggerprice": trigger_price,
        "newquantityinlot": quantity,
        "newdisclosedquantity": disclosed_quantity,
        "newgoodtilldate": "",
        "lastmodifiedtime": last_modified_time,
        "qtytradedtoday": qty_traded_today
    });

    match motilal_request(&client, "POST", "/rest/trans/v2/modifyorder", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let order_id = data.get("uniqueorderid")
                    .and_then(|o| o.as_str())
                    .unwrap_or("");
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "order_id": order_id,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Order modification failed")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Cancel order with Motilal Oswal
#[command]
pub async fn motilal_cancel_order(
    auth_token: String,
    api_key: String,
    vendor_code: String,
    order_id: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    let payload = json!({
        "uniqueorderid": order_id
    });

    match motilal_request(&client, "POST", "/rest/trans/v1/cancelorder", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "order_id": order_id,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Order cancellation failed")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Get order book from Motilal Oswal
#[command]
pub async fn motilal_get_orders(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/book/v2/getorderbook", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let orders = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "orders": orders,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "orders": [] })),
                    error: None,
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Get trade book from Motilal Oswal
#[command]
pub async fn motilal_get_trade_book(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/book/v1/gettradebook", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let trades = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "trades": trades,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "trades": [] })),
                    error: None,
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}
