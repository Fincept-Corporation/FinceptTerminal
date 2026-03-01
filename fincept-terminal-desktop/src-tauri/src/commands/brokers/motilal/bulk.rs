// Motilal Oswal Bulk Operation Commands

use serde_json::json;
use tauri::command;
use super::{ApiResponse, get_timestamp};
use super::orders::{motilal_get_orders, motilal_place_order, motilal_cancel_order};
use super::portfolio::motilal_get_positions;

/// Close all positions
#[command]
pub async fn motilal_close_all_positions(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();

    // Get positions first
    let positions_result = motilal_get_positions(
        auth_token.clone(),
        api_key.clone(),
        vendor_code.clone(),
    ).await?;

    if !positions_result.success {
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch positions".to_string()),
            timestamp,
        });
    }

    let positions = positions_result.data
        .and_then(|d| d.get("positions").cloned())
        .and_then(|p| p.as_array().cloned())
        .unwrap_or_default();

    if positions.is_empty() {
        return Ok(ApiResponse {
            success: true,
            data: Some(json!({ "message": "No open positions to close" })),
            error: None,
            timestamp,
        });
    }

    let mut closed_count = 0;
    let mut failed_count = 0;

    for position in positions {
        let buy_qty = position.get("buyquantity").and_then(|v| v.as_i64()).unwrap_or(0);
        let sell_qty = position.get("sellquantity").and_then(|v| v.as_i64()).unwrap_or(0);
        let net_qty = buy_qty - sell_qty;

        if net_qty == 0 {
            continue;
        }

        let symbol_token = position.get("symboltoken").and_then(|v| v.as_i64()).unwrap_or(0);
        let exchange = position.get("exchange").and_then(|v| v.as_str()).unwrap_or("NSE");
        let product_type = position.get("productname").and_then(|v| v.as_str()).unwrap_or("NORMAL");

        let (action, quantity) = if net_qty > 0 {
            ("SELL", net_qty)
        } else {
            ("BUY", -net_qty)
        };

        let result = motilal_place_order(
            auth_token.clone(),
            api_key.clone(),
            vendor_code.clone(),
            exchange.to_string(),
            symbol_token,
            action.to_string(),
            "MARKET".to_string(),
            product_type.to_string(),
            quantity,
            0.0,
            0.0,
            0,
            "DAY".to_string(),
            false,
        ).await;

        match result {
            Ok(response) if response.success => closed_count += 1,
            _ => failed_count += 1,
        }
    }

    Ok(ApiResponse {
        success: failed_count == 0,
        data: Some(json!({
            "closed": closed_count,
            "failed": failed_count
        })),
        error: if failed_count > 0 { Some(format!("{} positions failed to close", failed_count)) } else { None },
        timestamp,
    })
}

/// Cancel all open orders
#[command]
pub async fn motilal_cancel_all_orders(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();

    // Get orders first
    let orders_result = motilal_get_orders(
        auth_token.clone(),
        api_key.clone(),
        vendor_code.clone(),
    ).await?;

    if !orders_result.success {
        return Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch orders".to_string()),
            timestamp,
        });
    }

    let orders = orders_result.data
        .and_then(|d| d.get("orders").cloned())
        .and_then(|o| o.as_array().cloned())
        .unwrap_or_default();

    // Filter open orders (Motilal uses 'Confirm', 'Sent', 'Open' for open orders)
    let open_orders: Vec<_> = orders.iter()
        .filter(|o| {
            let status = o.get("orderstatus")
                .and_then(|s| s.as_str())
                .unwrap_or("")
                .to_lowercase();
            status == "confirm" || status == "sent" || status == "open"
        })
        .collect();

    if open_orders.is_empty() {
        return Ok(ApiResponse {
            success: true,
            data: Some(json!({ "message": "No open orders to cancel" })),
            error: None,
            timestamp,
        });
    }

    let mut cancelled_count = 0;
    let mut failed_count = 0;

    for order in open_orders {
        let order_id = order.get("uniqueorderid")
            .and_then(|v| v.as_str())
            .unwrap_or("");

        if order_id.is_empty() {
            failed_count += 1;
            continue;
        }

        let result = motilal_cancel_order(
            auth_token.clone(),
            api_key.clone(),
            vendor_code.clone(),
            order_id.to_string(),
        ).await;

        match result {
            Ok(response) if response.success => cancelled_count += 1,
            _ => failed_count += 1,
        }
    }

    Ok(ApiResponse {
        success: failed_count == 0,
        data: Some(json!({
            "cancelled": cancelled_count,
            "failed": failed_count
        })),
        error: if failed_count > 0 { Some(format!("{} orders failed to cancel", failed_count)) } else { None },
        timestamp,
    })
}
