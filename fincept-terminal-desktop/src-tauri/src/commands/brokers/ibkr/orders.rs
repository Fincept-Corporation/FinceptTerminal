// IBKR Order, Contract, and Alert Commands

use serde_json::{json, Value};
use crate::commands::brokers::common::ApiResponse;
use super::{get_api_base, create_ibkr_headers, create_client};

// ============================================================================
// Order Commands
// ============================================================================

/// Get live orders
#[tauri::command]
pub async fn ibkr_get_orders(
    access_token: Option<String>,
    use_gateway: bool,
    filters: Option<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_orders] Fetching live orders");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut url = format!("{}/iserver/account/orders", base_url);
    if let Some(f) = filters {
        url = format!("{}?filters={}", url, f);
    }

    let response = client
        .get(&url)
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
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch orders".to_string()), timestamp })
    }
}

/// Get specific order by order ID
#[tauri::command]
pub async fn ibkr_get_order(
    access_token: Option<String>,
    use_gateway: bool,
    order_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_order] Fetching order: {}", order_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/account/order/status/{}", base_url, order_id))
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
        Ok(ApiResponse { success: false, data: None, error: Some("Order not found".to_string()), timestamp })
    }
}

/// Place order
#[tauri::command]
pub async fn ibkr_place_order(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    orders: Vec<Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_place_order] Placing order for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "orders": orders });

    let response = client
        .post(format!("{}/iserver/account/{}/orders", base_url, account_id))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        eprintln!("[ibkr_place_order] Order response: {:?}", body);
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("error").and_then(|e| e.as_str())
            .unwrap_or("Order placement failed").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Reply to order confirmation message
#[tauri::command]
pub async fn ibkr_reply_to_order(
    access_token: Option<String>,
    use_gateway: bool,
    reply_id: String,
    confirmed: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_reply_to_order] Replying to order message: {} (confirmed: {})", reply_id, confirmed);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "confirmed": confirmed });

    let response = client
        .post(format!("{}/iserver/reply/{}", base_url, reply_id))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to confirm order".to_string()), timestamp })
    }
}

/// Modify order
#[tauri::command]
pub async fn ibkr_modify_order(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    order_id: String,
    order_params: Value,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_modify_order] Modifying order: {} for account: {}", order_id, account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/iserver/account/{}/order/{}", base_url, account_id, order_id))
        .headers(headers)
        .json(&order_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("error").and_then(|e| e.as_str())
            .unwrap_or("Order modification failed").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Cancel order
#[tauri::command]
pub async fn ibkr_cancel_order(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    order_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_cancel_order] Cancelling order: {} for account: {}", order_id, account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .delete(format!("{}/iserver/account/{}/order/{}", base_url, account_id, order_id))
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
        let error_msg = body.get("error").and_then(|e| e.as_str())
            .unwrap_or("Order cancellation failed").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Preview order (what-if)
#[tauri::command]
pub async fn ibkr_preview_order(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    orders: Vec<Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_preview_order] Previewing order for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "orders": orders });

    let response = client
        .post(format!("{}/iserver/account/{}/orders/whatif", base_url, account_id))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Order preview failed".to_string()), timestamp })
    }
}

// ============================================================================
// Contract/Symbol Commands
// ============================================================================

/// Search for contracts by symbol
#[tauri::command]
pub async fn ibkr_search_contracts(
    access_token: Option<String>,
    use_gateway: bool,
    symbol: String,
    sec_type: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[ibkr_search_contracts] Searching for: {}", symbol);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut body = json!({ "symbol": symbol });
    if let Some(st) = sec_type {
        body["secType"] = json!(st);
    }

    let response = client
        .post(format!("{}/iserver/secdef/search", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Contract search failed".to_string()), timestamp })
    }
}

/// Get contract details by conid
#[tauri::command]
pub async fn ibkr_get_contract(
    access_token: Option<String>,
    use_gateway: bool,
    conid: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_contract] Fetching contract: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/contract/{}/info", base_url, conid))
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
        Ok(ApiResponse { success: false, data: None, error: Some("Contract not found".to_string()), timestamp })
    }
}

/// Get contract rules (trading rules, hours)
#[tauri::command]
pub async fn ibkr_get_contract_rules(
    access_token: Option<String>,
    use_gateway: bool,
    conid: i64,
    is_buy: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_contract_rules] Fetching rules for conid: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "conid": conid, "isBuy": is_buy });

    let response = client
        .post(format!("{}/iserver/contract/rules", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch contract rules".to_string()), timestamp })
    }
}

/// Get security definition info for multiple conids
#[tauri::command]
pub async fn ibkr_get_secdef_info(
    access_token: Option<String>,
    use_gateway: bool,
    conids: Vec<i64>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_secdef_info] Fetching secdef for {} conids", conids.len());

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "conids": conids });

    let response = client
        .post(format!("{}/trsrv/secdef", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch security definitions".to_string()), timestamp })
    }
}

// ============================================================================
// Alert Commands
// ============================================================================

/// Get alerts
#[tauri::command]
pub async fn ibkr_get_alerts(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[ibkr_get_alerts] Fetching alerts for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/account/{}/alerts", base_url, account_id))
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
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch alerts".to_string()), timestamp })
    }
}

/// Create alert
#[tauri::command]
pub async fn ibkr_create_alert(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    alert_params: Value,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_create_alert] Creating alert for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/iserver/account/{}/alert", base_url, account_id))
        .headers(headers)
        .json(&alert_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to create alert".to_string()), timestamp })
    }
}

/// Delete alert
#[tauri::command]
pub async fn ibkr_delete_alert(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    alert_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_delete_alert] Deleting alert: {} for account: {}", alert_id, account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .delete(format!("{}/iserver/account/{}/alert/{}", base_url, account_id, alert_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Failed to delete alert".to_string()), timestamp })
    }
}
