// commands/brokers/dhan/portfolio.rs - Portfolio commands

use serde_json::Value;
use super::{ApiResponse, get_timestamp, dhan_request};

/// Get positions
#[tauri::command]
pub async fn dhan_get_positions(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    match dhan_request("/v2/positions", "GET", &access_token, &client_id, None).await {
        Ok(data) => {
            let positions = if let Some(arr) = data.as_array() {
                arr.clone()
            } else {
                vec![]
            };

            Ok(ApiResponse {
                success: true,
                data: Some(positions),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: Some(vec![]),
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get holdings
#[tauri::command]
pub async fn dhan_get_holdings(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    match dhan_request("/v2/holdings", "GET", &access_token, &client_id, None).await {
        Ok(data) => {
            let holdings = if let Some(arr) = data.as_array() {
                arr.clone()
            } else {
                vec![]
            };

            Ok(ApiResponse {
                success: true,
                data: Some(holdings),
                error: None,
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: Some(vec![]),
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}

/// Get funds/margin
#[tauri::command]
pub async fn dhan_get_funds(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<Value>, String> {
    match dhan_request("/v2/fundlimit", "GET", &access_token, &client_id, None).await {
        Ok(data) => Ok(ApiResponse {
            success: true,
            data: Some(data),
            error: None,
            timestamp: get_timestamp(),
        }),
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}
