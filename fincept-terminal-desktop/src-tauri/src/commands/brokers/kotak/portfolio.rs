// Kotak Portfolio Commands — Orders, Positions, Holdings, Funds

use super::{ApiResponse, kotak_request, parse_auth_token, encode_jdata};
use reqwest::Client;
use serde_json::{json, Value};
use tauri::command;

/// Get order book
#[command]
pub async fn kotak_get_orders(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let client = Client::new();
    match kotak_request(&client, "GET", &base_url, "/quick/user/orders", &trading_token, &trading_sid, None).await {
        Ok(data) => ApiResponse { success: true, data: Some(data), error: None },
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}

/// Get trade book
#[command]
pub async fn kotak_get_trade_book(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let client = Client::new();
    match kotak_request(&client, "GET", &base_url, "/quick/user/trades", &trading_token, &trading_sid, None).await {
        Ok(data) => ApiResponse { success: true, data: Some(data), error: None },
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}

/// Get positions
#[command]
pub async fn kotak_get_positions(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let client = Client::new();
    match kotak_request(&client, "GET", &base_url, "/quick/user/positions", &trading_token, &trading_sid, None).await {
        Ok(data) => ApiResponse { success: true, data: Some(data), error: None },
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}

/// Get holdings
#[command]
pub async fn kotak_get_holdings(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let client = Client::new();
    match kotak_request(&client, "GET", &base_url, "/portfolio/v1/holdings", &trading_token, &trading_sid, None).await {
        Ok(data) => ApiResponse { success: true, data: Some(data), error: None },
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}

/// Get funds/limits
#[command]
pub async fn kotak_get_funds(auth_token: String) -> ApiResponse<Value> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let client = Client::new();
    let funds_data = json!({ "seg": "ALL", "exch": "ALL", "prod": "ALL" });
    let payload = encode_jdata(&funds_data);

    match kotak_request(&client, "POST", &base_url, "/quick/user/limits", &trading_token, &trading_sid, Some(payload)).await {
        Ok(data) => ApiResponse { success: true, data: Some(data), error: None },
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}
