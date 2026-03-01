// AngelOne Portfolio & Margin Commands

use serde_json::{json, Value};
use super::super::common::ApiResponse;
use super::angel_api_call;

/// Get positions
/// AngelOne API: GET /rest/secure/angelbroking/order/v1/getPosition
#[tauri::command]
pub async fn angelone_get_positions(
    api_key: String,
    access_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/order/v1/getPosition",
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

/// Get holdings (portfolio)
/// AngelOne API: GET /rest/secure/angelbroking/portfolio/v1/getAllHolding
#[tauri::command]
pub async fn angelone_get_holdings(
    api_key: String,
    access_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/portfolio/v1/getAllHolding",
        "GET",
        None,
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!(null));
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

/// Get funds / RMS (Risk Management System) limits
/// AngelOne API: GET /rest/secure/angelbroking/user/v1/getRMS
#[tauri::command]
pub async fn angelone_get_rms(
    api_key: String,
    access_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/user/v1/getRMS",
        "GET",
        None,
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!({}));
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

/// Calculate margin for orders
/// AngelOne API: POST /rest/secure/angelbroking/margin/v1/batch
#[tauri::command]
pub async fn angelone_calculate_margin(
    api_key: String,
    access_token: String,
    orders: Value,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let payload = json!({
        "positions": orders
    });

    match angel_api_call(
        &api_key,
        &access_token,
        "/rest/secure/angelbroking/margin/v1/batch",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            let data = body.get("data").cloned().unwrap_or(json!({}));
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
