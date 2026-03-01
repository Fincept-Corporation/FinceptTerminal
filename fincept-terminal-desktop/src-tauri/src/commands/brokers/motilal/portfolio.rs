// Motilal Oswal Portfolio Commands

use reqwest::Client;
use serde_json::json;
use tauri::command;
use super::{ApiResponse, get_timestamp, get_motilal_headers, motilal_request};

/// Get positions from Motilal Oswal
#[command]
pub async fn motilal_get_positions(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/book/v1/getposition", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let positions = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "positions": positions,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "positions": [] })),
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

/// Get holdings from Motilal Oswal
#[command]
pub async fn motilal_get_holdings(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/report/v1/getdpholding", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                let holdings = data.get("data").cloned().unwrap_or(json!([]));
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "holdings": holdings,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({ "holdings": [] })),
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

/// Get funds/margin from Motilal Oswal
#[command]
pub async fn motilal_get_funds(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    match motilal_request(&client, "POST", "/rest/report/v1/getreportmargindetail", &headers, Some(json!({}))).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                // Parse margin data from Motilal response
                let data_items = data.get("data").and_then(|d| d.as_array());

                let mut available_cash = 0.0;
                let mut collateral = 0.0;
                let mut used_margin = 0.0;
                let mut unrealized_pnl = 0.0;
                let mut realized_pnl = 0.0;

                if let Some(items) = data_items {
                    for item in items {
                        let srno = item.get("srno").and_then(|s| s.as_i64()).unwrap_or(0);
                        let amount = item.get("amount").and_then(|a| a.as_f64()).unwrap_or(0.0);

                        match srno {
                            102 => available_cash = amount, // Available for Cash/SLBM
                            220 => collateral = amount,     // Non-Cash Balance
                            201 => {
                                if available_cash == 0.0 {
                                    available_cash = amount;
                                }
                            }
                            300 => used_margin = amount,    // Margin Usage Details
                            600 => unrealized_pnl = amount, // Total MTM P&L
                            700 => realized_pnl = amount,   // Booked P&L
                            _ => {}
                        }
                    }
                }

                Ok(ApiResponse {
                    success: true,
                    data: Some(json!({
                        "availablecash": available_cash,
                        "collateral": collateral,
                        "utiliseddebits": used_margin,
                        "m2munrealized": unrealized_pnl,
                        "m2mrealized": realized_pnl,
                        "raw": data
                    })),
                    error: None,
                    timestamp,
                })
            } else {
                Ok(ApiResponse {
                    success: false,
                    data: Some(json!({
                        "availablecash": 0.0,
                        "collateral": 0.0,
                        "utiliseddebits": 0.0,
                        "m2munrealized": 0.0,
                        "m2mrealized": 0.0
                    })),
                    error: data.get("message").and_then(|m| m.as_str()).map(|s| s.to_string()),
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
